#pragma once
/**
 * @file slab.h
 * @brief ANB_Slab public API — slab allocator with item tracking.
 */

/**
 * @defgroup ANB_Slab ANB_Slab
 * @brief Slab allocator / buffer queue with item tracking.
 */
#include <stdint.h>
#include <stdlib.h>

/**
 * @ingroup ANB_Slab
 * @brief Opaque slab allocator / buffer queue with item tracking.
 *
 * All pushed data is padded to max_align_t alignment. Sizes reported by
 * peek_item and pop_item reflect the original data_len passed to
 * push_item, not the aligned size.
 */
typedef struct ANB_Slab ANB_Slab_t;

/**
 * @ingroup ANB_Slab
 * @brief Create a new buffer queue.
 * @param initial_size Initial capacity in bytes. Must be > 0.
 * @return Pointer to the new queue. Aborts on allocation failure.
 */
ANB_Slab_t* ANB_slab_create(size_t initial_size);

/**
 * @ingroup ANB_Slab
 * @brief Destroy a buffer queue and free its memory.
 * @param queue The queue to destroy. Safe to pass NULL.
 */
void ANB_slab_destroy(ANB_Slab_t* queue);

/**
 * @ingroup ANB_Slab
 * @brief Push data onto the end of the queue as a discrete item.
 * @param queue The queue. Must not be NULL.
 * @param data Pointer to data to copy. Must not be NULL.
 * @param data_len Number of bytes to copy.
 * @note Data is stored with max_align_t alignment padding. The buffer
 *       consumes ALIGN_UP(data_len) bytes internally. Padding bytes are
 *       zeroed. Buffer and item index grow automatically if needed.
 */
void ANB_slab_push_item(ANB_Slab_t* queue, const uint8_t* data, size_t data_len);

/**
 * @ingroup ANB_Slab
 * @brief Get the total number of bytes currently in use (including alignment padding).
 * @param queue The queue. Must not be NULL.
 * @return Total bytes occupied by all items in the queue.
 */
size_t ANB_slab_size(ANB_Slab_t* queue);

/**
 * @ingroup ANB_Slab
 * @brief Get the number of discrete items in the queue.
 * @param queue The queue. Must not be NULL.
 * @return Number of items available (each push counts as one item).
 */
size_t ANB_slab_item_count(ANB_Slab_t* queue);

/**
 * @ingroup ANB_Slab
 * @brief Iterator state for O(1)-per-step item traversal. Use peek_item_iter if you need to iterate through all items in order without random access.
 *
 * Initialize to zero before first use:
 *   ANB_SlabIter_t iter = {0};
 *
 * Pushing while an iterator is live is undefined behavior (realloc may
 * invalidate pointers). Popping is safe.
 */
typedef struct ANB_SlabIter {
    size_t _idx;     /* item index */
    uint8_t *_ptr;   /* pointer to current item data, or NULL if iter is exhausted */
    size_t _n_idx;
    uint8_t *_n_ptr;
} ANB_SlabIter_t;

/**
 * @ingroup ANB_Slab
 * @brief Iterate items in FIFO order, O(1) per call.
 * @param queue The queue. Must not be NULL.
 * @param iter  Iterator state. Zero-initialize before first call.  If null,
 * return the first item
 * @param out_size If non-NULL, receives the item's original size in bytes.
 * @return Pointer to the current item's data, or NULL if no more items.
 * @note Advances iter to the next item on each call. Do not push or pop
 *       while iterating.  Restart iteration if you need to do this
 */
uint8_t *ANB_slab_peek_item_iter(ANB_Slab_t* queue, ANB_SlabIter_t *iter, size_t *out_size);

/**
 * @ingroup ANB_Slab
 * @brief Pop the first item from the queue.
 * @param queue The queue. Must not be NULL.
 * @param iter Optional iterator state to advance in sync with the popped item. If non-NULL, iter must be currently at the popped item (i.e. the next peek_item_iter call would return the popped item). If iter is NULL, no iterator is advanced.
 * @return 0 if success, -1 if fail.
 * @note When all items are consumed, internal positions reset to reuse buffer space.
 */
int ANB_slab_pop_item(ANB_Slab_t* queue, ANB_SlabIter_t *iter);
