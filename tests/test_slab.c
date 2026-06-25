#include "unity.h"
#include "slab.h"
#include <string.h>
#include <stddef.h>

/* Mirrors the alignment macro used internally by ANB_Slab. */
#define ALIGN_UP(x) (((x) + _Alignof(max_align_t) - 1) & ~(_Alignof(max_align_t) - 1))

void setUp(void) {}
void tearDown(void) {}

/* ------------------------------------------------------------------ */
/* 1. Push / iterate / pop with C strings                             */
/* ------------------------------------------------------------------ */
void test_push_iterate_pop_strings(void) {
    const char *strings[] = {"alpha", "bravo", "charlie", "delta", "echo"};
    const size_t count = sizeof(strings) / sizeof(strings[0]);

    ANB_Slab_t *q = ANB_slab_create(256);
    TEST_ASSERT_NOT_NULL(q);

    for (size_t i = 0; i < count; i++) {
        size_t len = strlen(strings[i]) + 1;
        ANB_slab_push_item(q, (const uint8_t *)strings[i], len);
    }

    TEST_ASSERT_EQUAL_size_t(count, ANB_slab_item_count(q));

    /* Iterate all items, verify content */
    ANB_SlabIter_t iter = {0};
    size_t item_size;
    uint8_t *data;
    size_t i = 0;

    while ((data = ANB_slab_peek_item_iter(q, &iter, &item_size)) != NULL) {
        TEST_ASSERT_LESS_THAN(count, i);
        TEST_ASSERT_EQUAL_size_t(strlen(strings[i]) + 1, item_size);
        TEST_ASSERT_EQUAL_INT(0, memcmp(data, strings[i], strlen(strings[i]) + 1));
        i++;
    }
    TEST_ASSERT_EQUAL_size_t(count, i);

    /* Pop items one by one using NULL iter (pops first non-deleted) */
    for (i = 0; i < count; i++) {
        TEST_ASSERT_EQUAL_INT(0, ANB_slab_pop_item(q, NULL));
        TEST_ASSERT_EQUAL_size_t(count - i - 1, ANB_slab_item_count(q));
    }

    /* Pop on empty returns -1 */
    TEST_ASSERT_EQUAL_INT(-1, ANB_slab_pop_item(q, NULL));

    ANB_slab_destroy(q);
}

/* ------------------------------------------------------------------ */
/* 2. Struct + trailing string composite blobs                        */
/* ------------------------------------------------------------------ */
typedef struct {
    uint32_t id;
    float    value;
    uint16_t flags;
} TestRecord;

#define MAX_LABEL 32

void test_aligned_struct_with_trailing_string(void) {
    TestRecord records[] = {
        {1, 3.14f, 0x00FF},
        {2, 2.72f, 0x0F0F},
        {3, 1.41f, 0xAAAA},
    };
    const char *labels[] = {"sensor-a", "sensor-b", "sensor-c"};
    const size_t count = sizeof(records) / sizeof(records[0]);

    ANB_Slab_t *q = ANB_slab_create(512);
    TEST_ASSERT_NOT_NULL(q);

    for (size_t i = 0; i < count; i++) {
        size_t label_len = strlen(labels[i]) + 1;
        size_t blob_size = sizeof(TestRecord) + label_len;
        uint8_t buf[sizeof(TestRecord) + MAX_LABEL];

        memcpy(buf, &records[i], sizeof(TestRecord));
        memcpy(buf + sizeof(TestRecord), labels[i], label_len);

        ANB_slab_push_item(q, buf, blob_size);
    }

    TEST_ASSERT_EQUAL_size_t(count, ANB_slab_item_count(q));

    /* Iterate and verify struct fields and trailing label */
    ANB_SlabIter_t iter = {0};
    size_t item_size;
    uint8_t *data;
    size_t i = 0;

    while ((data = ANB_slab_peek_item_iter(q, &iter, &item_size)) != NULL) {
        TestRecord r;
        memcpy(&r, data, sizeof(TestRecord));
        TEST_ASSERT_EQUAL_UINT32(records[i].id, r.id);
        TEST_ASSERT_EQUAL_FLOAT(records[i].value, r.value);
        TEST_ASSERT_EQUAL_UINT16(records[i].flags, r.flags);

        const char *label = (const char *)(data + sizeof(TestRecord));
        TEST_ASSERT_EQUAL_STRING(labels[i], label);
        i++;
    }
    TEST_ASSERT_EQUAL_size_t(count, i);

    /* Pop all via NULL iter */
    for (i = 0; i < count; i++) {
        TEST_ASSERT_EQUAL_INT(0, ANB_slab_pop_item(q, NULL));
    }
    TEST_ASSERT_EQUAL_size_t(0, ANB_slab_item_count(q));

    ANB_slab_destroy(q);
}

/* ------------------------------------------------------------------ */
/* 3. Iterator skips deleted items                                    */
/* ------------------------------------------------------------------ */
void test_iter_skips_deleted(void) {
    const char *strings[] = {"alpha", "bravo", "charlie", "delta", "echo"};
    const size_t count = sizeof(strings) / sizeof(strings[0]);

    ANB_Slab_t *q = ANB_slab_create(256);
    TEST_ASSERT_NOT_NULL(q);

    for (size_t i = 0; i < count; i++) {
        size_t len = strlen(strings[i]) + 1;
        ANB_slab_push_item(q, (const uint8_t *)strings[i], len);
    }

    /* Delete "bravo" (index 1) and "delta" (index 3) via iterator */
    ANB_SlabIter_t iter = {0};
    size_t item_size;
    size_t i = 0;
    while (ANB_slab_peek_item_iter(q, &iter, &item_size) != NULL) {
        if (i == 1 || i == 3) {
            TEST_ASSERT_EQUAL_INT(0, ANB_slab_pop_item(q, &iter));
        }
        i++;
    }

    TEST_ASSERT_EQUAL_size_t(3, ANB_slab_item_count(q));

    /* Iterate again — should only see alpha, charlie, echo */
    const char *expected[] = {"alpha", "charlie", "echo"};
    ANB_SlabIter_t iter2 = {0};
    uint8_t *data;
    i = 0;
    while ((data = ANB_slab_peek_item_iter(q, &iter2, &item_size)) != NULL) {
        TEST_ASSERT_EQUAL_STRING(expected[i], (const char *)data);
        i++;
    }
    TEST_ASSERT_EQUAL_size_t(3, i);

    ANB_slab_destroy(q);
}

/* ------------------------------------------------------------------ */
/* 4. Pop with NULL iter pops first non-deleted                       */
/* ------------------------------------------------------------------ */
void test_pop_null_iter(void) {
    ANB_Slab_t *q = ANB_slab_create(256);

    ANB_slab_push_item(q, (const uint8_t *)"aaa", 4);
    ANB_slab_push_item(q, (const uint8_t *)"bbb", 4);
    ANB_slab_push_item(q, (const uint8_t *)"ccc", 4);

    /* Pop first (aaa) */
    TEST_ASSERT_EQUAL_INT(0, ANB_slab_pop_item(q, NULL));
    TEST_ASSERT_EQUAL_size_t(2, ANB_slab_item_count(q));

    /* Iterate — should see bbb, ccc */
    ANB_SlabIter_t iter = {0};
    size_t sz;
    uint8_t *data = ANB_slab_peek_item_iter(q, &iter, &sz);
    TEST_ASSERT_NOT_NULL(data);
    TEST_ASSERT_EQUAL_STRING("bbb", (const char *)data);

    data = ANB_slab_peek_item_iter(q, &iter, &sz);
    TEST_ASSERT_NOT_NULL(data);
    TEST_ASSERT_EQUAL_STRING("ccc", (const char *)data);

    TEST_ASSERT_NULL(ANB_slab_peek_item_iter(q, &iter, &sz));

    ANB_slab_destroy(q);
}

/* ------------------------------------------------------------------ */
/* 5. Double-delete returns -1                                        */
/* ------------------------------------------------------------------ */
void test_double_delete(void) {
    ANB_Slab_t *q = ANB_slab_create(256);

    ANB_slab_push_item(q, (const uint8_t *)"test", 5);

    ANB_SlabIter_t iter = {0};
    size_t sz;
    ANB_slab_peek_item_iter(q, &iter, &sz);

    TEST_ASSERT_EQUAL_INT(0, ANB_slab_pop_item(q, &iter));
    TEST_ASSERT_EQUAL_INT(-1, ANB_slab_pop_item(q, &iter));
    TEST_ASSERT_EQUAL_size_t(0, ANB_slab_item_count(q));

    ANB_slab_destroy(q);
}

/* ------------------------------------------------------------------ */
/* 6. Size reports total write_pos                                    */
/* ------------------------------------------------------------------ */
void test_size(void) {
    ANB_Slab_t *q = ANB_slab_create(256);
    TEST_ASSERT_NOT_NULL(q);

    TEST_ASSERT_EQUAL_size_t(0, ANB_slab_size(q));

    const char *a = "hello";
    const char *b = "world!!";
    size_t a_len = strlen(a) + 1;
    size_t b_len = strlen(b) + 1;

    ANB_slab_push_item(q, (const uint8_t *)a, a_len);
    TEST_ASSERT_EQUAL_size_t(ALIGN_UP(a_len), ANB_slab_size(q));

    ANB_slab_push_item(q, (const uint8_t *)b, b_len);
    TEST_ASSERT_EQUAL_size_t(ALIGN_UP(a_len) + ALIGN_UP(b_len), ANB_slab_size(q));

    ANB_slab_destroy(q);
}

/* ------------------------------------------------------------------ */
/* 7. Reset when all items consumed                                   */
/* ------------------------------------------------------------------ */
void test_reset_on_empty(void) {
    ANB_Slab_t *q = ANB_slab_create(256);

    ANB_slab_push_item(q, (const uint8_t *)"a", 2);
    ANB_slab_push_item(q, (const uint8_t *)"b", 2);

    ANB_slab_pop_item(q, NULL);
    ANB_slab_pop_item(q, NULL);

    /* After all consumed, size and count should be 0 */
    TEST_ASSERT_EQUAL_size_t(0, ANB_slab_item_count(q));
    TEST_ASSERT_EQUAL_size_t(0, ANB_slab_size(q));

    /* Can push again after reset */
    ANB_slab_push_item(q, (const uint8_t *)"c", 2);
    TEST_ASSERT_EQUAL_size_t(1, ANB_slab_item_count(q));

    ANB_SlabIter_t iter = {0};
    size_t sz;
    uint8_t *data = ANB_slab_peek_item_iter(q, &iter, &sz);
    TEST_ASSERT_NOT_NULL(data);
    TEST_ASSERT_EQUAL_size_t(2, sz);
    TEST_ASSERT_EQUAL_STRING("c", (const char *)data);

    ANB_slab_destroy(q);
}

/* ------------------------------------------------------------------ */
/* 8. Iterator version validity                                       */
/* ------------------------------------------------------------------ */
void test_iter_valid(void) {
    ANB_Slab_t *q = ANB_slab_create(256);

    ANB_slab_push_item(q, (const uint8_t *)"aaa", 4);
    ANB_slab_push_item(q, (const uint8_t *)"bbb", 4);

    ANB_SlabIter_t iter = {0};
    size_t sz;
    ANB_slab_peek_item_iter(q, &iter, &sz);

    /* Iterator is valid while items exist */
    TEST_ASSERT_EQUAL_INT(1, ANB_slab_item_valid(q, &iter));

    /* Pop all items — triggers reset and version increment */
    ANB_slab_pop_item(q, NULL);
    ANB_slab_pop_item(q, NULL);

    /* Old iterator is now stale */
    TEST_ASSERT_EQUAL_INT(0, ANB_slab_item_valid(q, &iter));

    /* Push new data, create fresh iterator — valid again */
    ANB_slab_push_item(q, (const uint8_t *)"ccc", 4);
    ANB_SlabIter_t iter2 = {0};
    ANB_slab_peek_item_iter(q, &iter2, &sz);
    TEST_ASSERT_EQUAL_INT(1, ANB_slab_item_valid(q, &iter2));

    /* Original iterator is still stale */
    TEST_ASSERT_EQUAL_INT(0, ANB_slab_item_valid(q, &iter));

    /* Zero-init iterator on empty queue is not valid */
    ANB_Slab_t *q2 = ANB_slab_create(256);
    ANB_SlabIter_t iter3 = {0};
    TEST_ASSERT_EQUAL_INT(0, ANB_slab_item_valid(q2, &iter3));
    ANB_slab_destroy(q2);

    /* Iterator pointing at deleted item is not valid */
    ANB_Slab_t *q3 = ANB_slab_create(256);
    ANB_slab_push_item(q3, (const uint8_t *)"xxx", 4);
    ANB_slab_push_item(q3, (const uint8_t *)"yyy", 4);
    ANB_SlabIter_t iter4 = {0};
    ANB_slab_peek_item_iter(q3, &iter4, &sz);
    TEST_ASSERT_EQUAL_INT(1, ANB_slab_item_valid(q3, &iter4));
    ANB_slab_pop_item(q3, &iter4);
    TEST_ASSERT_EQUAL_INT(0, ANB_slab_item_valid(q3, &iter4));
    ANB_slab_destroy(q3);

    ANB_slab_destroy(q);
}

/* ------------------------------------------------------------------ */
/* 9. Peek item without advancing                                     */
/* ------------------------------------------------------------------ */
void test_peek_item(void) {
    ANB_Slab_t *q = ANB_slab_create(256);

    ANB_slab_push_item(q, (const uint8_t *)"hello", 6);
    ANB_slab_push_item(q, (const uint8_t *)"world", 6);

    ANB_SlabIter_t iter = {0};
    size_t sz;
    uint8_t *data = ANB_slab_peek_item_iter(q, &iter, &sz);
    TEST_ASSERT_EQUAL_STRING("hello", (const char *)data);

    /* peek_item returns same item without advancing */
    data = ANB_slab_peek_item(q, &iter, &sz);
    TEST_ASSERT_NOT_NULL(data);
    TEST_ASSERT_EQUAL_STRING("hello", (const char *)data);
    TEST_ASSERT_EQUAL_size_t(6, sz);

    /* calling peek_item again still returns the same item */
    data = ANB_slab_peek_item(q, &iter, &sz);
    TEST_ASSERT_NOT_NULL(data);
    TEST_ASSERT_EQUAL_STRING("hello", (const char *)data);

    /* advance, then peek_item returns the next one */
    ANB_slab_peek_item_iter(q, &iter, &sz);
    data = ANB_slab_peek_item(q, &iter, &sz);
    TEST_ASSERT_NOT_NULL(data);
    TEST_ASSERT_EQUAL_STRING("world", (const char *)data);

    /* pop the item, peek_item returns NULL (deleted) */
    ANB_slab_pop_item(q, &iter);
    TEST_ASSERT_NULL(ANB_slab_peek_item(q, &iter, NULL));

    ANB_slab_destroy(q);
}

/* ------------------------------------------------------------------ */
/* Blob test declarations                                             */
/* ------------------------------------------------------------------ */
void test_create_destroy(void);
void test_data_usable(void);
void test_alloc_explicit(void);
void test_alloc_double(void);
void test_realloc_exact(void);
void test_data_after_alloc(void);
void test_clear(void);
void test_clear_after_alloc(void);
void test_push_basic(void);
void test_push_auto_grow(void);
void test_reset(void);
void test_clear_resets_pos(void);
void test_push_multiple(void);

/* ------------------------------------------------------------------ */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_push_iterate_pop_strings);
    RUN_TEST(test_aligned_struct_with_trailing_string);
    RUN_TEST(test_iter_skips_deleted);
    RUN_TEST(test_pop_null_iter);
    RUN_TEST(test_double_delete);
    RUN_TEST(test_size);
    RUN_TEST(test_reset_on_empty);
    RUN_TEST(test_iter_valid);
    RUN_TEST(test_peek_item);
    RUN_TEST(test_create_destroy);
    RUN_TEST(test_data_usable);
    RUN_TEST(test_alloc_explicit);
    RUN_TEST(test_alloc_double);
    RUN_TEST(test_realloc_exact);
    RUN_TEST(test_data_after_alloc);
    RUN_TEST(test_clear);
    RUN_TEST(test_clear_after_alloc);
    RUN_TEST(test_push_basic);
    RUN_TEST(test_push_auto_grow);
    RUN_TEST(test_reset);
    RUN_TEST(test_clear_resets_pos);
    RUN_TEST(test_push_multiple);
    return UNITY_END();
}
