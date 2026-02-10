#include "fifoslab.h"
#include <stdint.h>
#include <stddef.h>

/*
 * libFuzzer harness for ANB_FifoSlab.
 *
 * Interprets fuzz input as a stream of commands:
 *   0 = push        (next 2 bytes = length LE, then that many bytes = data)
 *   1 = pop         (next 2 bytes = length LE)
 *   2 = pop_item
 *   3 = peek        (next 2 bytes = length LE)
 *   4 = peek_item   (next 1 byte  = index)
 *   5 = peek_size
 *   6 = item_count
 *   7 = peek_item_iter (iterate all items from the start)
 *
 * Goal: no crashes, no ASAN/UBSAN violations under any input.
 */

static uint16_t read_u16(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    ANB_FifoSlab_t *q = ANB_fifoslab_create(64);

    size_t i = 0;
    while (i < size) {
        uint8_t cmd = data[i++] % 8;

        switch (cmd) {
        case 0: { /* push */
            if (i + 2 > size) goto done;
            uint16_t len = read_u16(data + i);
            i += 2;
            /* Cap push size to avoid OOM killing the fuzzer */
            if (len > 4096) len = 4096;
            if (len == 0) break;
            if (i + len > size) len = (uint16_t)(size - i);
            ANB_fifoslab_push(q, data + i, len);
            i += len;
            break;
        }
        case 1: { /* pop */
            if (i + 2 > size) goto done;
            uint16_t len = read_u16(data + i);
            i += 2;
            ANB_fifoslab_pop(q, len);
            break;
        }
        case 2: { /* pop_item */
            ANB_fifoslab_pop_item(q);
            break;
        }
        case 3: { /* peek */
            if (i + 2 > size) goto done;
            uint16_t len = read_u16(data + i);
            i += 2;
            volatile uint8_t *p = ANB_fifoslab_peek(q, len);
            /* Touch first byte if non-NULL so ASAN catches bad pointers */
            if (p) { (void)*p; }
            break;
        }
        case 4: { /* peek_item */
            if (i + 1 > size) goto done;
            uint8_t idx = data[i++];
            size_t item_size = 0;
            volatile uint8_t *p = ANB_fifoslab_peek_item(q, idx, &item_size);
            if (p) { (void)*p; }
            break;
        }
        case 5: { /* peek_size */
            ANB_fifoslab_peek_size(q);
            break;
        }
        case 6: { /* item_count */
            ANB_fifoslab_item_count(q);
            break;
        }
        case 7: { /* peek_item_iter — iterate all items */
            ANB_FifoSlabIter_t iter = {0};
            size_t item_size = 0;
            volatile uint8_t *p;
            while ((p = ANB_fifoslab_peek_item_iter(q, &iter, &item_size)) != NULL) {
                (void)*p;
            }
            break;
        }
        }
    }

done:
    ANB_fifoslab_destroy(q);
    return 0;
}
