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
ANB_fifoslab_pop_item(q);  // removes "hello", returns aligned size

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

## Alignment and size recovery

Every push pads the data up to the next `max_align_t` boundary (typically 16 bytes on 64-bit systems). This means the buffer may store **more bytes than you pushed**, and the sizes returned by the API (`peek_item`, `pop_item`, `peek_size`, `pop`) are always the padded (aligned) size -- not the original length.

This has a practical consequence: **you cannot recover the original size of arbitrary byte arrays from the buffer alone.**

For example, pushing a 7-byte `uint8_t[]` consumes 16 bytes in the buffer. When you `peek_item` it back, `out_size` will be 16, not 7. The original length is lost.

If you need to recover exact sizes, pack your data into a fixed-size struct (or a struct with a length field):

```c
// Original size is lost -- out_size will be 16, not 7
uint8_t blob[7] = { ... };
ANB_fifoslab_push_item(q, blob, 7);

// Original size is recoverable -- sizeof(MyPacket) is known at compile time
typedef struct { uint32_t id; float value; } MyPacket;
MyPacket pkt = {1, 3.14f};
ANB_fifoslab_push_item(q, (const uint8_t *)&pkt, sizeof(MyPacket));

// For variable-length data, encode the length in a header struct
typedef struct { uint32_t len; } Header;
Header hdr = { .len = 7 };
// push header, then push payload, or push together as one item with offset by sizeof(Header)
```

The padding bytes are always zeroed, so reading `sizeof(YourStruct)` number of bytes from a peeked pointer is safe.

## Key behaviors

- Buffer and item index grow automatically (doubling strategy).
- When all data is consumed, internal positions reset to offset 0, reusing the buffer without reallocation.
- All sizes returned by `peek_item`, `pop_item`, `peek_size` are **aligned** sizes, not the original `data_len`.
- Allocation failures abort via `assert()`.
