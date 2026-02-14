#pragma once
/**
 * @file fifoslab.h
 * @brief ANB_FifoSlab public API — FIFO slab allocator with item tracking.
 */

/**
 * @defgroup ANB_FifoSlab ANB_FifoSlab
 * @brief FIFO slab allocator / buffer queue with item tracking.
 */
#include <stdint.h>
#include <stdlib.h>

/**
 * @ingroup ANB_FifoSlab
 * @brief Opaque FIFO slab allocator / buffer queue with item tracking.
 *
 * All pushed data is padded to max_align_t alignment. Sizes reported by
 * peek_item and pop_item reflect the original data_len passed to
 * push_item, not the aligned size.
 */
typedef struct ANB_FifoSlab ANB_FifoSlab_t;

/**
 * @ingroup ANB_FifoSlab
 * @brief Create a new buffer queue.
 * @param initial_size Initial capacity in bytes. Must be > 0.
 * @return Pointer to the new queue. Aborts on allocation failure.
 */
ANB_FifoSlab_t* ANB_fifoslab_create(size_t initial_size);

/**
 * @ingroup ANB_FifoSlab
 * @brief Destroy a buffer queue and free its memory.
 * @param queue The queue to destroy. Safe to pass NULL.
 */
void ANB_fifoslab_destroy(ANB_FifoSlab_t* queue);

/**
 * @ingroup ANB_FifoSlab
 * @brief Push data onto the end of the queue as a discrete item.
 * @param queue The queue. Must not be NULL.
 * @param data Pointer to data to copy. Must not be NULL.
 * @param data_len Number of bytes to copy.
 * @note Data is stored with max_align_t alignment padding. The buffer
 *       consumes ALIGN_UP(data_len) bytes internally. Padding bytes are
 *       zeroed. Buffer and item index grow automatically if needed.
 */
void ANB_fifoslab_push_item(ANB_FifoSlab_t* queue, const uint8_t* data, size_t data_len);

/**
 * @ingroup ANB_FifoSlab
 * @brief Get the total number of bytes currently in use (including alignment padding).
 * @param queue The queue. Must not be NULL.
 * @return Total bytes occupied by all items in the queue.
 */
size_t ANB_fifoslab_size(ANB_FifoSlab_t* queue);

/**
 * @ingroup ANB_FifoSlab
 * @brief Get the number of discrete items in the queue.
 * @param queue The queue. Must not be NULL.
 * @return Number of items available (each push counts as one item).
 */
size_t ANB_fifoslab_item_count(ANB_FifoSlab_t* queue);

/**
 * @ingroup ANB_FifoSlab
 * @brief Peek at a specific item by index without consuming.
 * @param queue The queue. Must not be NULL.
 * @param n Zero-based index (0 = first unconsumed item).
 * @param out_size If non-NULL, receives the item's original size in bytes
 *                 (i.e. the data_len passed to push_item).
 * @return Pointer to the item's data, or NULL if n is out of range.
 * @note The returned pointer is valid until the next push or pop.
 */
uint8_t *ANB_fifoslab_peek_item(ANB_FifoSlab_t* queue, size_t n, size_t *out_size);

/**
 * @ingroup ANB_FifoSlab
 * @brief Iterator state for O(1)-per-step item traversal. Use peek_item_iter if you need to iterate through all items in order without random access.
 *
 * Initialize to zero before first use:
 *   ANB_FifoSlabIter_t iter = {0};
 *
 * Pushing or popping while an iterator is live is undefined behavior.
 */
typedef struct ANB_FifoSlabIter {
    size_t _item_idx;     /* index entries advanced past index_read */
    size_t _byte_offset;  /* bytes advanced past read_pos */
} ANB_FifoSlabIter_t;

/**
 * @ingroup ANB_FifoSlab
 * @brief Iterate items in FIFO order, O(1) per call.
 * @param queue The queue. Must not be NULL.
 * @param iter  Iterator state. Zero-initialize before first call.
 * @param out_size If non-NULL, receives the item's original size in bytes.
 * @return Pointer to the current item's data, or NULL if no more items.
 * @note Advances iter to the next item on each call. Do not push or pop
 *       while iterating.  Restart iteration if you need to do this
 */
uint8_t *ANB_fifoslab_peek_item_iter(ANB_FifoSlab_t* queue, ANB_FifoSlabIter_t *iter, size_t *out_size);

/**
 * @ingroup ANB_FifoSlab
 * @brief Pop the first item from the queue.
 * @param queue The queue. Must not be NULL.
 * @return Original size of the popped item in bytes, or 0 if empty.
 * @note When all items are consumed, internal positions reset to reuse buffer space.
 */
size_t ANB_fifoslab_pop_item(ANB_FifoSlab_t* queue);
