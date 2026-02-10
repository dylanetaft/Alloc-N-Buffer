#include <stdint.h>
#include "fifoslab.h"
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define ANB_FS_ALIGN_UP(x) (((x) + _Alignof(max_align_t) - 1) & ~(_Alignof(max_align_t) - 1))

#define ANB_FS_INITIAL_INDEX_CAP 64 

struct ANB_FifoSlab {
  uint8_t *data;        // Contiguous block of buffer data
  size_t write_pos; // Current write position
  size_t read_pos;  // Current read position
  size_t size; // Total size of the buffer

  size_t *index;       // Parallel buffer: size of each pushed item
  size_t index_write;  // Number of entries written
  size_t index_read;   // Number of entries consumed
  size_t index_cap;    // Capacity (number of size_t slots)
};


ANB_FifoSlab_t* ANB_fifoslab_create(size_t initial_size) {
    assert(initial_size > 0);
    ANB_FifoSlab_t* queue = (ANB_FifoSlab_t*)malloc(sizeof(ANB_FifoSlab_t));
    assert(queue != NULL);

    queue->data = (uint8_t *)malloc(initial_size);
    assert(queue->data != NULL);

    queue->write_pos = 0;
    queue->read_pos = 0;
    queue->size = initial_size;

    queue->index = (size_t *)malloc(ANB_FS_INITIAL_INDEX_CAP * sizeof(size_t));
    assert(queue->index != NULL);
    queue->index_write = 0;
    queue->index_read = 0;
    queue->index_cap = ANB_FS_INITIAL_INDEX_CAP;

    return queue;
}

void ANB_fifoslab_destroy(ANB_FifoSlab_t* queue) {
    if (queue) {
        free(queue->data);
        free(queue->index);
        free(queue);
    }
}

void ANB_fifoslab_push(ANB_FifoSlab_t* queue, const uint8_t* data, size_t data_len) {
    assert(queue != NULL);
    assert(data != NULL);

    size_t aligned_len = ANB_FS_ALIGN_UP(data_len);

    // Expand data buffer if needed
    if (queue->write_pos + aligned_len > queue->size) {
        size_t new_size = queue->size;
        while (new_size < queue->write_pos + aligned_len) {
          assert(new_size < SIZE_MAX / 2); // Prevent overflow
          new_size *= 2;
        }
        queue->data = (uint8_t *)realloc(queue->data, new_size);
        assert(queue->data != NULL);
        queue->size = new_size;
    }

    // Copy data into buffer and zero padding
    memcpy(queue->data + queue->write_pos, data, data_len);
    if (aligned_len > data_len)
        memset(queue->data + queue->write_pos + data_len, 0, aligned_len - data_len);
    queue->write_pos += aligned_len;

    // Expand index buffer if needed
    if (queue->index_write >= queue->index_cap) {
        size_t new_cap = queue->index_cap * 2;
        queue->index = (size_t *)realloc(queue->index, new_cap * sizeof(size_t));
        assert(queue->index != NULL);
        queue->index_cap = new_cap;
    }

    // Record this entry's aligned size
    queue->index[queue->index_write++] = aligned_len;
}

size_t ANB_fifoslab_peek_size(ANB_FifoSlab_t* queue) {
    assert(queue != NULL);
    return queue->write_pos - queue->read_pos;
}

uint8_t *ANB_fifoslab_peek(ANB_FifoSlab_t* queue, size_t requested_len) {
    if (queue == NULL || requested_len == 0 || requested_len > (queue->write_pos - queue->read_pos)) {
        return NULL;
    }
    return queue->data + queue->read_pos;
}

size_t ANB_fifoslab_pop(ANB_FifoSlab_t* queue, size_t requested_len) {
  uint8_t *data = ANB_fifoslab_peek(queue, requested_len);
  if (data == NULL) {
      return 0;
  }
  queue->read_pos += requested_len;

  // Advance index to match: consume whole items, reduce partial
  size_t remaining = requested_len;
  while (remaining > 0 && queue->index_read < queue->index_write) {
      size_t item_size = queue->index[queue->index_read];
      if (remaining >= item_size) {
          remaining -= item_size;
          queue->index_read++;
      } else {
          queue->index[queue->index_read] -= remaining;
          remaining = 0;
      }
  }

  // If all data has been read, reset positions to avoid overflow
  // This also helps in reusing the buffer space
  if (queue->read_pos == queue->write_pos) {
      queue->read_pos = 0;
      queue->write_pos = 0;
      queue->index_read = 0;
      queue->index_write = 0;
  }
  return requested_len;
}

size_t ANB_fifoslab_item_count(ANB_FifoSlab_t* queue) {
    assert(queue != NULL);
    return queue->index_write - queue->index_read;
}

uint8_t *ANB_fifoslab_peek_item(ANB_FifoSlab_t* queue, size_t n, size_t *out_size) {
    assert(queue != NULL);
    if (n >= queue->index_write - queue->index_read) {
        return NULL;
    }

    // Walk the index to compute data offset for item n
    size_t offset = queue->read_pos;
    for (size_t i = queue->index_read; i < queue->index_read + n; i++) {
        offset += queue->index[i];
    }

    size_t item_size = queue->index[queue->index_read + n];
    if (out_size) {
        *out_size = item_size;
    }
    return queue->data + offset;
}

uint8_t *ANB_fifoslab_peek_item_iter(ANB_FifoSlab_t* queue, ANB_FifoSlabIter_t *iter, size_t *out_size) {
    assert(queue != NULL);
    assert(iter != NULL);

    size_t abs_idx = queue->index_read + iter->_item_idx;
    if (abs_idx >= queue->index_write) {
        return NULL;
    }

    size_t item_size = queue->index[abs_idx];
    if (out_size) {
        *out_size = item_size;
    }

    uint8_t *ptr = queue->data + queue->read_pos + iter->_byte_offset;

    iter->_item_idx++;
    iter->_byte_offset += item_size;

    return ptr;
}

size_t ANB_fifoslab_pop_item(ANB_FifoSlab_t* queue) {
    assert(queue != NULL);
    if (queue->index_read >= queue->index_write) {
        return 0;
    }

    size_t item_size = queue->index[queue->index_read++];
    queue->read_pos += item_size;

    // If all data consumed, reset everything
    if (queue->read_pos == queue->write_pos) {
        queue->read_pos = 0;
        queue->write_pos = 0;
        queue->index_read = 0;
        queue->index_write = 0;
    }

    return item_size;
}
