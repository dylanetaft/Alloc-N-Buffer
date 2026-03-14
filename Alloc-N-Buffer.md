# Alloc-N-Buffer

A C library providing `ANB_Slab` -- a slab allocator that doubles as a buffer queue with item tracking.

## What it does

`ANB_Slab` is a growable, contiguous byte buffer with internal tracking of each push as indexed offsets to the item data.
It supports pushing arbitrary byte arrays, and iterating/deleting them. The internal buffer grows as needed,
and when all items are deleted, it resets to offset 0 for reuse.

All data is stored with `max_align_t` alignment padding, so structs can be pushed and read back without alignment concerns.
You can also store structs as headers next to variable-length binary data within a single item.


## Quick start

```c
#include "slab.h"

ANB_Slab_t *q = ANB_slab_create(1024);

// Push some items
ANB_slab_push_item(q, (const uint8_t *)"hello", 6);
ANB_slab_push_item(q, (const uint8_t *)"world", 6);

// Iterate items
ANB_SlabIter_t iter = {0};
size_t item_size;
uint8_t *data;
while ((data = ANB_slab_peek_item_iter(q, &iter, &item_size)) != NULL) {
    printf("%.*s\n", (int)item_size, data);
}

// Pop first non-deleted item
ANB_slab_pop_item(q, NULL);

// Or pop via iterator
ANB_SlabIter_t iter2 = {0};
ANB_slab_peek_item_iter(q, &iter2, &item_size);  // advance to first item
ANB_slab_pop_item(q, &iter2);                     // delete it

ANB_slab_destroy(q);
```

## Building

```sh
cmake -B build
cmake --build build
```

With tests:
```sh
cmake -B build -DBUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

With fuzzing (requires clang):
```sh
cmake -B build -DBUILD_FUZZ=ON
cmake --build build --target fuzz_slab
./build/fuzz_slab -max_total_time=60
```

## Alignment and size tracking

Every push pads the data up to the next `max_align_t` boundary (typically 16 bytes on 64-bit systems). This means the buffer may store **more bytes than you pushed** internally. However, the original `data_len` is preserved and returned by the iterator:

- `peek_item_iter` writes the original `data_len` to `*out_size`
- `size()` returns total bytes written (includes alignment padding and deleted items)

For example, pushing a 7-byte `uint8_t[]` consumes 16 bytes in the buffer, but `peek_item_iter` will report `out_size = 7`.

```c
uint8_t blob[7] = { ... };
ANB_slab_push_item(q, blob, 7);

ANB_SlabIter_t iter = {0};
size_t sz;
uint8_t *data = ANB_slab_peek_item_iter(q, &iter, &sz);  // sz == 7, data is 16-byte aligned
```

As bytes are aligned, you may directly cast the returned pointer to a struct type if you know the layout, or use it as a header for variable-length data.

## Key behaviors

- Buffer and item index grow automatically (doubling strategy).
- When all items are deleted, internal positions reset to offset 0, reusing the buffer without reallocation.
- Items are deleted by marking them in per-item metadata; the iterator skips deleted items automatically.
- Popping while iterating is safe. Pushing while iterating is undefined behavior (realloc may invalidate pointers).
- Allocation failures abort via `assert()`.
