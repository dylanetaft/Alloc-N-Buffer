# CLAUDE.md -- Alloc-N-Buffer

## Project overview

C library providing `ANB_Slab` -- a slab allocator / buffer queue. Single header (`include/slab.h`), single source (`src/slab.c`).

## Build

```sh
cmake -B build -DBUILD_TESTS=ON && cmake --build build && ctest --test-dir build --output-on-failure
```

Fuzzer (clang only):
```sh
cmake -B build -DBUILD_FUZZ=ON && cmake --build build --target fuzz_slab
```

## Architecture

- **Header**: `include/slab.h` -- public API, opaque `ANB_Slab_t` type
- **Implementation**: `src/slab.c` -- struct definition, all functions
- **Tests**: `tests/test_slab.c` -- Unity framework tests
- **Fuzzer**: `fuzz/fuzz_slab.c` -- libFuzzer harness

CMake produces both static (`allocnbuffer_static`) and shared (`allocnbuffer_shared`) libraries from a single object library.

## Critical alignment behavior

All pushed data is padded to `max_align_t` alignment (typically 16 bytes on 64-bit). Data pointers returned by peek are always aligned. However, the original `data_len` is preserved:

- `peek_item_iter(q, &iter, &size)` writes the **original** `data_len` to `*size`
- `size()` returns total bytes written (`write_pos`) — this includes aligned padding and deleted items

Use `ALIGN_UP(x) = ((x + _Alignof(max_align_t) - 1) & ~(_Alignof(max_align_t) - 1))` to compute aligned sizes from original data lengths if needed.

Internally, a parallel `uint8_t *metadata` array tracks per-item state. The low nibble (`& 0x0F`) stores the padding offset added to each item. The high nibble (`& 0xF0`) stores flags (e.g., deleted). This shares `index_write`/`index_cap` with the main `size_t *index`.

## API at a glance

| Function | Purpose |
|---|---|
| `ANB_slab_create(size)` | Allocate queue with initial capacity (aborts on failure) |
| `ANB_slab_destroy(q)` | Free queue (NULL-safe) |
| `ANB_slab_push_item(q, data, len)` | Append data as a discrete item (auto-grows, pads to alignment) |
| `ANB_slab_size(q)` | Total bytes written (includes padding and deleted items) |
| `ANB_slab_item_count(q)` | Number of non-deleted items |
| `ANB_slab_peek_item_iter(q, &iter, &sz)` | O(1) per-step item iterator, skips deleted items, returns NULL when done; `sz` receives original data_len |
| `ANB_slab_pop_item(q, &iter)` | Mark current iterator item as deleted. Pass NULL to pop first non-deleted item. Returns 0 on success, -1 on failure |

## Iterator usage

Use `ANB_SlabIter_t` (zero-initialize) to iterate items. The iterator tracks current (`_idx`/`_ptr`) and next (`_n_idx`/`_n_ptr`) positions internally.

- **Popping while iterating is safe.** Deleted items are skipped automatically.
- **Pushing while iterating is undefined behavior** — realloc may invalidate iterator pointers.
- When all items are deleted, `write_pos` and `index_write` reset to 0 for buffer reuse. A subsequent `peek_item_iter` call returns NULL.
- The iterator can be saved and reused as a read cursor for queue-like consumption patterns.

## Performance notes

Data is stored in a contiguous buffer with a contiguous `size_t[]` index, so cache locality is excellent compared to a linked list. No per-item heap allocations.

- `peek_item_iter`: O(1) per non-deleted item
- `pop_item` with iter: O(1)
- `pop_item` with NULL: O(n) in the worst case (scans for first non-deleted item)

## Conventions

- Prefix: `ANB_slab_` for all public symbols
- Internal macros: `ANB_S_` prefix
- Error handling: `assert()` for preconditions (NULL queue, NULL data, zero initial_size). `pop_item` returns -1 on failure (empty, already deleted).

## Using as a dependency

```cmake
add_subdirectory(path/to/Alloc-N-Buffer)
target_link_libraries(your_target allocnbuffer_static)
```

The `include/` directory is exported via `BUILD_INTERFACE`, so `#include "slab.h"` works automatically.
