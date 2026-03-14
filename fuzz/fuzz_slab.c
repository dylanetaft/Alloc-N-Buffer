#include "slab.h"
#include <stdint.h>
#include <stddef.h>

/*
 * libFuzzer harness for ANB_Slab.
 *
 * Interprets fuzz input as a stream of commands:
 *   0 = push_item   (next 2 bytes = length LE, then that many bytes = data)
 *   1 = pop_item with NULL iter (pops first non-deleted)
 *   2 = peek_item_iter (iterate all items from the start)
 *   3 = item_count
 *   4 = pop_item via iterator (iterate to Nth item, pop it)
 *   5 = size
 *
 * Goal: no crashes, no ASAN/UBSAN violations under any input.
 */

static uint16_t read_u16(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    ANB_Slab_t *q = ANB_slab_create(64);

    size_t i = 0;
    while (i < size) {
        uint8_t cmd = data[i++] % 6;

        switch (cmd) {
        case 0: { /* push_item */
            if (i + 2 > size) goto done;
            uint16_t len = read_u16(data + i);
            i += 2;
            /* Cap push size to avoid OOM killing the fuzzer */
            if (len > 4096) len = 4096;
            if (len == 0) break;
            if (i + len > size) len = (uint16_t)(size - i);
            ANB_slab_push_item(q, data + i, len);
            i += len;
            break;
        }
        case 1: { /* pop_item with NULL iter */
            ANB_slab_pop_item(q, NULL);
            break;
        }
        case 2: { /* peek_item_iter — iterate all items */
            ANB_SlabIter_t iter = {0};
            size_t item_size = 0;
            volatile uint8_t *p;
            while ((p = ANB_slab_peek_item_iter(q, &iter, &item_size)) != NULL) {
                (void)*p;
            }
            break;
        }
        case 3: { /* item_count */
            ANB_slab_item_count(q);
            break;
        }
        case 4: { /* pop via iterator — iterate to Nth item and pop */
            if (i + 1 > size) goto done;
            uint8_t n = data[i++];
            ANB_SlabIter_t iter = {0};
            size_t item_size = 0;
            uint8_t *p;
            uint8_t seen = 0;
            while ((p = ANB_slab_peek_item_iter(q, &iter, &item_size)) != NULL) {
                if (seen == n) {
                    ANB_slab_pop_item(q, &iter);
                    break;
                }
                seen++;
            }
            break;
        }
        case 5: { /* size */
            ANB_slab_size(q);
            break;
        }
        }
    }

done:
    ANB_slab_destroy(q);
    return 0;
}
