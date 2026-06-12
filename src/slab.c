#include <stdint.h>
#include "slab.h"
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

  uint64_t version;    // Incremented on buffer reset (all items consumed)
};


ANB_Slab_t* ANB_slab_create(size_t initial_size) {
    if (initial_size == 0) abort();
    ANB_Slab_t* queue = (ANB_Slab_t*)calloc(1, sizeof(ANB_Slab_t));
    if (!queue) abort();

    queue->data = (uint8_t *)malloc(initial_size);
    if (!queue->data) abort();

    queue->size = initial_size;

    queue->index = (size_t *)calloc(ANB_S_INITIAL_INDEX_CAP, sizeof(size_t));
    if (!queue->index) abort();
    queue->metadata = (uint8_t *)calloc(ANB_S_INITIAL_INDEX_CAP, sizeof(uint8_t));
    if (!queue->metadata) abort();
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

uint8_t *ANB_slab_alloc_item(ANB_Slab_t* queue, size_t data_len) {
    if (!queue) abort();

    size_t aligned_len = ANB_S_ALIGN_UP(data_len);

    // Expand data buffer if needed
    if (queue->write_pos + aligned_len > queue->size) {
        size_t new_size = queue->size;
        while (new_size < queue->write_pos + aligned_len) {
          if (new_size >= SIZE_MAX / 2) abort();
          new_size *= 2;
        }
        queue->data = (uint8_t *)realloc(queue->data, new_size);
        if (!queue->data) abort();
        queue->size = new_size;
    }

    uint8_t *ptr = queue->data + queue->write_pos;
    queue->write_pos += aligned_len;

    // Expand index buffers if needed
    if (queue->index_write >= queue->index_cap) {
        size_t new_cap = queue->index_cap * 2;
        queue->index = (size_t *)realloc(queue->index, new_cap * sizeof(size_t));
        if (!queue->index) abort();
        queue->metadata = (uint8_t *)realloc(queue->metadata, new_cap * sizeof(uint8_t));
        if (!queue->metadata) abort();
        memset(queue->metadata + queue->index_cap, 0, (new_cap - queue->index_cap) * sizeof(uint8_t));
        queue->index_cap = new_cap;
    }

    // Record this entry's aligned size and padding (low nibble), flags zeroed
    queue->metadata[queue->index_write] = (uint8_t)(aligned_len - data_len);
    queue->index[queue->index_write++] = aligned_len;
    queue->count++;

    return ptr;
}

void ANB_slab_push_item(ANB_Slab_t* queue, const uint8_t* data, size_t data_len) {
    if (!data) abort();
    uint8_t *ptr = ANB_slab_alloc_item(queue, data_len);
    memcpy(ptr, data, data_len);
}

size_t ANB_slab_size(ANB_Slab_t* queue) {
    if (!queue) abort();
    return queue->write_pos;
}

size_t ANB_slab_item_count(ANB_Slab_t* queue) {
    if (!queue) abort();
    return queue->count;
}

uint8_t *ANB_slab_peek_item_iter(ANB_Slab_t* queue, ANB_SlabIter_t *iter, size_t *out_size) {
    if (!queue) abort();
    if (!iter) abort();

    if (iter->_idx == 0 && iter->_n_off == 0) {
        iter->_version = queue->version;
    }

    for (;;) {
      iter->_idx = iter->_n_idx; //advance to next item if set
      iter->_off = iter->_n_off;
      if (iter->_idx >= queue->index_write) {
          return NULL; //end of iteration
      }
      //could be out of bounds
      //but we check idx first on next use
      iter->_n_off = iter->_off + queue->index[iter->_idx];
      iter->_n_idx = iter->_idx + 1;

      size_t idx = iter->_idx;

      if (queue->metadata[idx] & ANB_S_META_MASK) {
          continue; //item is deleted, skip
      }

      size_t aligned_size = queue->index[idx];
      if (out_size) {
          *out_size = aligned_size - (queue->metadata[idx] & ANB_S_PAD_MASK);
      }
      return queue->data + iter->_off;
    }
}

int ANB_slab_pop_item(ANB_Slab_t* queue, ANB_SlabIter_t *iter) {
    if (!queue) abort();
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

    queue->metadata[idx] = 0xF0; // Set high nibble - deleted
    queue->count--;
    // If all data consumed, reset everything
    if (queue->count == 0) {
        queue->write_pos = 0;
        queue->index_write = 0;
        queue->version++;
    }

    return 0;
}

int ANB_slab_securepop_item(ANB_Slab_t* queue, ANB_SlabIter_t *iter) {
    if (!queue) abort();
    if (queue->count == 0) {
        return -1;
    }

    size_t idx;
    uint8_t *ptr;
    if (iter) {
        idx = iter->_idx;
        ptr = queue->data + iter->_off;
    } else {
        ANB_SlabIter_t temp_iter = {0};
        ptr = ANB_slab_peek_item_iter(queue, &temp_iter, NULL);
        if (!ptr) return -1;
        idx = temp_iter._idx;
    }

    volatile uint8_t *p = (volatile uint8_t *)ptr;
    size_t len = queue->index[idx];
    while (len--) *p++ = 0;

    return ANB_slab_pop_item(queue, iter);
}

int ANB_slab_item_valid(ANB_Slab_t* queue, ANB_SlabIter_t *iter) {
    if (!queue) abort();
    if (!iter) abort();
    if (iter->_version != queue->version) return 0;
    if (iter->_idx >= queue->index_write) return 0;
    if (queue->metadata[iter->_idx] & ANB_S_META_MASK) return 0;
    return 1;
}

uint8_t *ANB_slab_peek_item(ANB_Slab_t* queue, ANB_SlabIter_t *iter, size_t *out_size) {
    if (!queue) abort();
    if (!iter) abort();
    if (!ANB_slab_item_valid(queue, iter)) return NULL;

    if (out_size) {
        size_t aligned_size = queue->index[iter->_idx];
        *out_size = aligned_size - (queue->metadata[iter->_idx] & ANB_S_PAD_MASK);
    }
    return queue->data + iter->_off;
}
