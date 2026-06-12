# AGENTS.md -- Alloc-N-Buffer

## Project overview

C library providing two buffer types:
- **`ANB_Slab`** — slab allocator / buffer queue with item tracking, alignment, and deletion.
- **`ANB_Blob`** — simple contiguous byte buffer, externally managed (like a managed `uint8_t*`).

## Build

```sh
cmake -B build -DBUILD_TESTS=ON && cmake --build build && ctest --test-dir build --output-on-failure
```

Fuzzer (clang only):
```sh
cmake -B build -DBUILD_FUZZ=ON && cmake --build build --target fuzz_slab
```

## Architecture

- **Headers**: `include/slab.h`, `include/blob.h` -- public APIs, opaque types
- **Implementation**: `src/slab.c`, `src/blob.c` -- struct definitions, all functions
- **Tests**: `tests/test_slab.c`, `tests/test_blob.c` -- Unity framework tests
- **Fuzzer**: `fuzz/fuzz_slab.c` -- libFuzzer harness

CMake produces both static (`allocnbuffer_static`) and shared (`allocnbuffer_shared`) libraries from a single object library. Source files are collected via `GLOB_RECURSE` on `src/*.c`, so new `.c` files are auto-included.

## ANB_Slab

### Critical alignment behavior

All pushed data is padded to `max_align_t` alignment (typically 16 bytes on 64-bit). Data pointers returned by peek are always aligned. However, the original `data_len` is preserved:

- `peek_item_iter(q, &iter, &size)` writes the **original** `data_len` to `*size`
- `size()` returns total bytes written (`write_pos`) — this includes aligned padding and deleted items

Use `ALIGN_UP(x) = ((x + _Alignof(max_align_t) - 1) & ~(_Alignof(max_align_t) - 1))` to compute aligned sizes from original data lengths if needed.

Internally, a parallel `uint8_t *metadata` array tracks per-item state. The low nibble (`& 0x0F`) stores the padding offset added to each item. The high nibble (`& 0xF0`) stores flags (e.g., deleted). This shares `index_write`/`index_cap` with the main `size_t *index`.

### API at a glance

| Function | Purpose |
|---|---|
| `ANB_slab_create(size)` | Allocate queue with initial capacity (aborts on failure) |
| `ANB_slab_destroy(q)` | Free queue (NULL-safe) |
| `ANB_slab_push_item(q, data, len)` | Append data as a discrete item (auto-grows, pads to alignment) |
| `ANB_slab_size(q)` | Total bytes written (includes padding and deleted items) |
| `ANB_slab_item_count(q)` | Number of non-deleted items |
| `ANB_slab_peek_item_iter(q, &iter, &sz)` | O(1) per-step item iterator, skips deleted items, returns NULL when done; `sz` receives original data_len |
| `ANB_slab_pop_item(q, &iter)` | Mark current iterator item as deleted. Pass NULL to pop first non-deleted item. Returns 0 on success, -1 on failure |

### Iterator usage

Use `ANB_SlabIter_t` (zero-initialize) to iterate items. The iterator tracks current (`_idx`/`_ptr`) and next (`_n_idx`/`_n_ptr`) positions internally.

- **Popping while iterating is safe.** Deleted items are skipped automatically.
- **Pushing while iterating is undefined behavior** — realloc may invalidate iterator pointers.
- When all items are deleted, `write_pos` and `index_write` reset to 0 for buffer reuse. A subsequent `peek_item_iter` call returns NULL.
- The iterator can be saved and reused as a read cursor for queue-like consumption patterns.

### Performance notes

Data is stored in a contiguous buffer with a contiguous `size_t[]` index, so cache locality is excellent compared to a linked list. No per-item heap allocations.

- `peek_item_iter`: O(1) per non-deleted item
- `pop_item` with iter: O(1)
- `pop_item` with NULL: O(n) in the worst case (scans for first non-deleted item)

## ANB_Blob

### API at a glance

| Function | Purpose |
|---|---|
| `ANB_blob_create(size)` | Allocate blob with initial capacity (aborts on failure) |
| `ANB_blob_destroy(b)` | Free memory (NULL-safe) |
| `ANB_blob_data(b)` | Return `uint8_t*` to internal buffer |
| `ANB_blob_capacity(b)` | Return total allocated bytes |
| `ANB_blob_alloc(b, bytes)` | Grow buffer; `bytes == 0` doubles capacity |
| `ANB_blob_realloc(b, size)` | Set exact capacity (shrink or grow) |
| `ANB_blob_clear(b)` | `memset` entire buffer to 0 |

### Key behaviors

- No item tracking, no alignment padding, no queue semantics.
- `ANB_blob_alloc(b, bytes)` adds `bytes` to current capacity. Passing `0` doubles.
- `ANB_blob_realloc(b, size)` sets capacity to exactly `size`.
- Data pointers are invalidated by `ANB_blob_alloc` and `ANB_blob_realloc`.

## Conventions

- **Prefix**: `ANB_slab_` for Slab functions, `ANB_blob_` for Blob functions
- **Internal macros**: `ANB_S_` prefix (Slab internals)
- **Error handling**: `abort()` on precondition violations (NULL pointers, zero initial sizes, overflow). `pop_item` returns -1 on failure (empty, already deleted).
- **Doxygen**: All public API functions and types have Doxygen comments with `@file`, `@defgroup`, `@ingroup`, `@brief`, `@param`, `@return`, `@note`, `@warning`.
- **Types**: Opaque types use `typedef struct ANB_Xxx ANB_Xxx_t;` pattern.
- **NULL safety**: Destroy functions are NULL-safe. All other functions assert non-NULL.

## Using as a dependency

```cmake
add_subdirectory(path/to/Alloc-N-Buffer)
target_link_libraries(your_target allocnbuffer_static)
```

The `include/` directory is exported via `BUILD_INTERFACE`, so `#include "slab.h"` and `#include "blob.h"` work automatically.

## Development workflow

- **Add new source**: Place `.c` in `src/` (auto-included by GLOB). Add header to `include/`.
- **Add tests**: Place in `tests/test_*.c`, add to `anb_tests` executable in `CMakeLists.txt`.
- **Run tests**: `cmake -B build -DBUILD_TESTS=ON && cmake --build build && ctest --test-dir build --output-on-failure`
- **Regenerate docs**: `doxygen Doxyfile` (output goes to `docs/`)
