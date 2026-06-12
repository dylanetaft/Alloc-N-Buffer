#pragma once
/**
 * @file blob.h
 * @brief ANB_Blob public API — simple contiguous byte buffer.
 */

/**
 * @defgroup ANB_Blob ANB_Blob
 * @brief Simple contiguous byte buffer, externally managed.
 */
#include <stdint.h>
#include <stdlib.h>

/**
 * @ingroup ANB_Blob
 * @brief Opaque contiguous byte buffer with externally managed contents.
 *
 * Unlike ANB_Slab, this is a raw byte buffer with no item tracking,
 * alignment padding, or queue semantics. The caller manages what goes
 * into the buffer and at what offsets.
 */
typedef struct ANB_Blob ANB_Blob_t;

/**
 * @ingroup ANB_Blob
 * @brief Create a new blob buffer.
 * @param initial_size Initial capacity in bytes. Must be > 0.
 * @return Pointer to the new blob. Aborts on allocation failure.
 */
ANB_Blob_t* ANB_blob_create(size_t initial_size);

/**
 * @ingroup ANB_Blob
 * @brief Destroy a blob buffer and free its memory.
 * @param blob The blob to destroy. Safe to pass NULL.
 */
void ANB_blob_destroy(ANB_Blob_t* blob);

/**
 * @ingroup ANB_Blob
 * @brief Get a pointer to the internal data buffer.
 * @param blob The blob. Must not be NULL.
 * @return Pointer to the raw byte buffer.
 * @warning This pointer may be invalidated by ANB_blob_alloc or ANB_blob_realloc.
 */
uint8_t* ANB_blob_data(ANB_Blob_t* blob);

/**
 * @ingroup ANB_Blob
 * @brief Get the total capacity of the blob buffer.
 * @param blob The blob. Must not be NULL.
 * @return Total allocated bytes in the buffer.
 */
size_t ANB_blob_capacity(ANB_Blob_t* blob);

/**
 * @ingroup ANB_Blob
 * @brief Grow the blob buffer by adding bytes to its capacity.
 * @param blob The blob. Must not be NULL.
 * @param bytes Number of bytes to add. If 0, doubles the current capacity.
 * @note Aborts on allocation failure or if the new capacity would overflow.
 * @warning Any pointer previously returned by ANB_blob_data may be invalidated.
 */
void ANB_blob_alloc(ANB_Blob_t* blob, size_t bytes);

/**
 * @ingroup ANB_Blob
 * @brief Set the blob buffer to an exact capacity, shrinking or growing as needed.
 * @param blob The blob. Must not be NULL.
 * @param new_capacity The desired capacity in bytes. Must be > 0.
 * @note Aborts on allocation failure. Shrinking may lose data beyond the new size.
 * @warning Any pointer previously returned by ANB_blob_data may be invalidated.
 */
void ANB_blob_realloc(ANB_Blob_t* blob, size_t new_capacity);

/**
 * @ingroup ANB_Blob
 * @brief Zero the entire blob buffer.
 * @param blob The blob. Must not be NULL.
 */
void ANB_blob_clear(ANB_Blob_t* blob);
