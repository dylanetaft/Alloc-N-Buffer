# Alloc-N-Buffer

A C library providing `ANB_FifoSlab` -- a FIFO slab allocator that doubles as a buffer queue with item tracking.

## What it does

`ANB_FifoSlab` is a growable, contiguous byte buffer with internal tracking of each push as indexed offsets to the item data. 
It supports pushing arbitrary byte arrays, and peeking/popping them back in FIFO order. The internal buffer grows as needed, 
and when all data is consumed, it resets to offset 0 for reuse.

All data is stored with `max_align_t` alignment padding, so structs can be pushed and read back without alignment concerns.
You can also store structs as headers next to variable-length binary data within a single item.


## Quick start

```c
#include "fifoslab.h"

ANB_FifoSlab_t *q = ANB_fifoslab_create(1024);

// Push some items
ANB_fifoslab_push(q, (const uint8_t *)"hello", 6);
ANB_fifoslab_push(q, (const uint8_t *)"world", 6);

// Item-level access
size_t count = ANB_fifoslab_item_count(q);  // 2
size_t item_size;
uint8_t *data = ANB_fifoslab_peek_item(q, 0, &item_size);  // "hello"
ANB_fifoslab_pop_item(q);  // removes "hello", returns original size (6)

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
cmake --build build --target fuzz_fifoslab
./build/fuzz_fifoslab -max_total_time=60
```

## Alignment and size tracking

Every push pads the data up to the next `max_align_t` boundary (typically 16 bytes on 64-bit systems). This means the buffer may store **more bytes than you pushed** internally. However, the original `data_len` is preserved and returned by the API:

- `peek_item` writes the original `data_len` to `*out_size`
- `pop_item` returns the original `data_len`
- `size()` returns total **aligned** bytes in use (reflecting actual buffer consumption)

For example, pushing a 7-byte `uint8_t[]` consumes 16 bytes in the buffer, but `peek_item` will report `out_size = 7`.

```c
uint8_t blob[7] = { ... };
ANB_fifoslab_push_item(q, blob, 7);

size_t sz;
uint8_t *data = ANB_fifoslab_peek_item(q, 0, &sz);  // sz == 7, data is 16-byte aligned
```

As bytes are aligned, you may directly cast the returned pointer to a struct type if you know the layout, or use it as a header for variable-length data.

## Key behaviors

- Buffer and item index grow automatically (doubling strategy).
- When all data is consumed, internal positions reset to offset 0, reusing the buffer without reallocation.
- All sizes returned by `peek_item`, `pop_item` are the **original** `data_len`. Use `ANB_fifoslab_size()` for total aligned buffer consumption.
- Allocation failures abort via `assert()`.
