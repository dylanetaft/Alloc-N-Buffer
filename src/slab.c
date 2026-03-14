#include <stdint.h>
#include "slab.h"
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define ANB_S_ALIGN_UP(x) (((x) + _Alignof(max_align_t) - 1) & ~(_Alignof(max_align_t) - 1))

#define ANB_S_META_MASK  0xF0
#define ANB_S_PAD_MASK   0x0F

#define ANB_S_INITIAL_INDEX_CAP 64

struct ANB_Slab {
  uint8_t *data;        // Contiguous block of buffer data
  size_t write_pos; // Current write position
  size_t size; // Total size of the buffer
  size_t count; // Number of items currently in the buffer 

  size_t *index;       // Parallel buffer: aligned size of each pushed item
  uint8_t *metadata;   // Parallel buffer: high nibble = flags, low nibble = padding
  size_t index_write;  // Number of entries written
  size_t index_cap;    // Capacity (number of slots)
};


ANB_Slab_t* ANB_slab_create(size_t initial_size) {
    assert(initial_size > 0);
    ANB_Slab_t* queue = (ANB_Slab_t*)calloc(1, sizeof(ANB_Slab_t));
    assert(queue != NULL);

    queue->data = (uint8_t *)malloc(initial_size);
    assert(queue->data != NULL);

    queue->size = initial_size;

    queue->index = (size_t *)calloc(ANB_S_INITIAL_INDEX_CAP, sizeof(size_t));
    assert(queue->index != NULL);
    queue->metadata = (uint8_t *)calloc(ANB_S_INITIAL_INDEX_CAP, sizeof(uint8_t));
    assert(queue->metadata != NULL);
    queue->index_cap = ANB_S_INITIAL_INDEX_CAP;

    return queue;
}

void ANB_slab_destroy(ANB_Slab_t* queue) {
    if (queue) {
        free(queue->data);
        free(queue->index);
        free(queue->metadata);
        free(queue);
    }
}

void ANB_slab_push_item(ANB_Slab_t* queue, const uint8_t* data, size_t data_len) {
    assert(queue != NULL);
    assert(data != NULL);

    size_t aligned_len = ANB_S_ALIGN_UP(data_len);

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

    // Expand index buffers if needed
    if (queue->index_write >= queue->index_cap) {
        size_t new_cap = queue->index_cap * 2;
        queue->index = (size_t *)realloc(queue->index, new_cap * sizeof(size_t));
        assert(queue->index != NULL);
        queue->metadata = (uint8_t *)realloc(queue->metadata, new_cap * sizeof(uint8_t));
        assert(queue->metadata != NULL);
        memset(queue->metadata + queue->index_cap, 0, (new_cap - queue->index_cap) * sizeof(uint8_t));
        queue->index_cap = new_cap;
    }

    // Record this entry's aligned size and padding (low nibble), flags zeroed
    queue->metadata[queue->index_write] = (uint8_t)(aligned_len - data_len);
    queue->index[queue->index_write++] = aligned_len;
    queue->count++;
}

size_t ANB_slab_size(ANB_Slab_t* queue) {
    assert(queue != NULL);
    return queue->write_pos;
}

size_t ANB_slab_item_count(ANB_Slab_t* queue) {
    assert(queue != NULL);
    return queue->count;
}

uint8_t *ANB_slab_peek_item_iter(ANB_Slab_t* queue, ANB_SlabIter_t *iter, size_t *out_size) {
    assert(queue != NULL);
    assert(iter != NULL);

    for (;;) {
      iter->_idx = iter->_n_idx; //advance to next item if set
      iter->_ptr = iter->_n_ptr;
      if (iter->_ptr == NULL) {
          iter->_idx = 0; //start of iteration
          iter->_ptr = queue->data;
      }
      if (iter->_idx >= queue->index_write) {
          return NULL; //end of iteration
      }
      //could be out of bounds
      //but we check idx first on next use
      iter->_n_ptr = iter->_ptr + queue->index[iter->_idx];
      iter->_n_idx = iter->_idx + 1;

      size_t idx = iter->_idx;
      uint8_t *ptr = iter->_ptr;

       
      if (queue->metadata[idx] & ANB_S_META_MASK) {
          continue; //item is deleted, skip
      }

      size_t aligned_size = queue->index[idx];
      if (out_size) {
          *out_size = aligned_size - (queue->metadata[idx] & ANB_S_PAD_MASK);
      }
      return ptr;
    }
}

int ANB_slab_pop_item(ANB_Slab_t* queue, ANB_SlabIter_t *iter) {
    assert(queue != NULL);
    if (queue->count == 0) {
        return -1;
    }
    size_t idx;
    if (iter) idx = iter->_idx;
    else {
      ANB_SlabIter_t temp_iter = {0};
      ANB_slab_peek_item_iter(queue, &temp_iter, NULL);
      idx = temp_iter._idx;
    }

    if (queue->metadata[idx] & ANB_S_META_MASK) {
        return -1; // already deleted
    }
    queue->metadata[idx] = 0xF0; // Set high nibble - deleted
    queue->count--;
    // If all data consumed, reset everything
    if (queue->count == 0) {
        queue->write_pos = 0;
        queue->index_write = 0;
    }

    return 0;
}
