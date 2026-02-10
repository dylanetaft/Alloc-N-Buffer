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

CMake produces both static (`alloc-n-buffer_static`) and shared (`alloc-n-buffer_shared`) libraries from a single object library.

## Critical alignment behavior

All pushed data is padded to `max_align_t` alignment (typically 16 bytes on 64-bit). This affects everything:

- `peek_size()` returns the sum of **aligned** sizes, not original data lengths
- `peek_item(q, n, &size)` writes the **aligned** size to `*size`
- `pop_item()` returns the **aligned** size
- `pop(q, len)` consumes `len` raw bytes **and** advances the item index proportionally

Use `ALIGN_UP(x) = ((x + _Alignof(max_align_t) - 1) & ~(_Alignof(max_align_t) - 1))` to compute aligned sizes from original data lengths.

**The original `data_len` is not stored and cannot be recovered from the buffer.** To recover exact sizes, push fixed-size structs (where `sizeof` is known) or encode a length field in a header struct before the variable-length payload.

## API at a glance

| Function | Purpose |
|---|---|
| `ANB_fifoslab_create(size)` | Allocate queue with initial capacity (aborts on failure) |
| `ANB_fifoslab_destroy(q)` | Free queue (NULL-safe) |
| `ANB_fifoslab_push(q, data, len)` | Append data (auto-grows, pads to alignment) |
| `ANB_fifoslab_peek_size(q)` | Total readable bytes (aligned) |
| `ANB_fifoslab_peek(q, len)` | Pointer to read position, or NULL |
| `ANB_fifoslab_pop(q, len)` | Consume raw bytes, returns count or 0 |
| `ANB_fifoslab_item_count(q)` | Number of discrete items |
| `ANB_fifoslab_peek_item(q, n, &sz)` | Pointer to item n, or NULL (O(n) offset walk) |
| `ANB_fifoslab_peek_item_iter(q, &iter, &sz)` | O(1) per-step item iterator, returns NULL when done |
| `ANB_fifoslab_pop_item(q)` | Remove first item, returns aligned size or 0 |

## Performance notes

This is a FIFO structure. For front-to-back consumption, use `pop_item` (O(1) per call). To inspect items without consuming, use the `peek_item_iter` API with an `ANB_FifoSlabIter_t` (zero-initialize, O(1) per step). Do **not** iterate with `peek_item(q, 0..N-1)` in a loop — `peek_item` walks the index from the start each call, making that pattern O(N²). Pushing or popping while an iterator is live is undefined behavior.

Data is stored in a contiguous buffer with a contiguous `size_t[]` index, so cache locality is excellent compared to a linked list. No per-item heap allocations.

## Conventions

- Prefix: `ANB_fifoslab_` for all public symbols
- Internal macros: `ANB_FS_` prefix
- Error handling: `assert()` for preconditions (NULL queue, NULL data, zero initial_size). No graceful error returns except where documented (peek returns NULL, pop returns 0).
- Byte-level and item-level APIs share state. Mixing them is valid but the caller must pop aligned boundaries to keep item tracking consistent.

## Using as a dependency

```cmake
add_subdirectory(path/to/Alloc-N-Buffer)
target_link_libraries(your_target alloc-n-buffer_static)
```

The `include/` directory is exported via `BUILD_INTERFACE`, so `#include "fifoslab.h"` works automatically.
