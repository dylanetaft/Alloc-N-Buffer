# CLAUDE.md -- Alloc-N-Buffer

## Project overview

C library providing `ANB_FifoSlab` -- a FIFO slab allocator / buffer queue. Single header (`include/fifoslab.h`), single source (`src/fifoslab.c`).

## Build

```sh
cmake -B build -DBUILD_TESTS=ON && cmake --build build && ctest --test-dir build --output-on-failure
```

Fuzzer (clang only):
```sh
cmake -B build -DBUILD_FUZZ=ON && cmake --build build --target fuzz_fifoslab
```

## Architecture

- **Header**: `include/fifoslab.h` -- public API, opaque `ANB_FifoSlab_t` type
- **Implementation**: `src/fifoslab.c` -- struct definition, all functions
- **Tests**: `tests/test_fifoslab.c` -- Unity framework tests
- **Fuzzer**: `fuzz/fuzz_fifoslab.c` -- libFuzzer harness

CMake produces both static (`allocnbuffer_static`) and shared (`allocnbuffer_shared`) libraries from a single object library.

## Critical alignment behavior

All pushed data is padded to `max_align_t` alignment (typically 16 bytes on 64-bit). Data pointers returned by peek are always aligned. However, the original `data_len` is preserved:

- `peek_item(q, n, &size)` writes the **original** `data_len` to `*size`
- `pop_item()` returns the **original** `data_len`
- `size()` returns the total bytes in use (sum of **aligned** item sizes, since that reflects actual buffer consumption)

Use `ALIGN_UP(x) = ((x + _Alignof(max_align_t) - 1) & ~(_Alignof(max_align_t) - 1))` to compute aligned sizes from original data lengths if needed.

Internally, a parallel `uint8_t *pad_index` tracks the padding offset added to each item. This shares `index_read`/`index_write`/`index_cap` with the main `size_t *index`.

## API at a glance

| Function | Purpose |
|---|---|
| `ANB_fifoslab_create(size)` | Allocate queue with initial capacity (aborts on failure) |
| `ANB_fifoslab_destroy(q)` | Free queue (NULL-safe) |
| `ANB_fifoslab_push_item(q, data, len)` | Append data as a discrete item (auto-grows, pads to alignment) |
| `ANB_fifoslab_size(q)` | Total bytes in use (sum of aligned item sizes) |
| `ANB_fifoslab_item_count(q)` | Number of discrete items |
| `ANB_fifoslab_peek_item(q, n, &sz)` | Pointer to item n, or NULL (O(n) offset walk); `sz` receives original data_len |
| `ANB_fifoslab_peek_item_iter(q, &iter, &sz)` | O(1) per-step item iterator, returns NULL when done; `sz` receives original data_len |
| `ANB_fifoslab_pop_item(q)` | Remove first item, returns original data_len or 0 |

## Performance notes

This is a FIFO structure. For front-to-back consumption, use `pop_item` (O(1) per call). To inspect items without consuming, use the `peek_item_iter` API with an `ANB_FifoSlabIter_t` (zero-initialize, O(1) per step). Do **not** iterate with `peek_item(q, 0..N-1)` in a loop — `peek_item` walks the index from the start each call, making that pattern O(N²). Pushing or popping while an iterator is live is undefined behavior.

Data is stored in a contiguous buffer with a contiguous `size_t[]` index, so cache locality is excellent compared to a linked list. No per-item heap allocations.

## Conventions

- Prefix: `ANB_fifoslab_` for all public symbols
- Internal macros: `ANB_FS_` prefix
- Error handling: `assert()` for preconditions (NULL queue, NULL data, zero initial_size). No graceful error returns except where documented (peek_item returns NULL, pop_item returns 0).

## Using as a dependency

```cmake
add_subdirectory(path/to/Alloc-N-Buffer)
target_link_libraries(your_target allocnbuffer_static)
```

The `include/` directory is exported via `BUILD_INTERFACE`, so `#include "fifoslab.h"` works automatically.
