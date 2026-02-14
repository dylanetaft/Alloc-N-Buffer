#include "unity.h"
#include "fifoslab.h"
#include <string.h>
#include <stddef.h>

/* Mirrors the alignment macro used internally by ANB_FifoSlab. */
#define ALIGN_UP(x) (((x) + _Alignof(max_align_t) - 1) & ~(_Alignof(max_align_t) - 1))

void setUp(void) {}
void tearDown(void) {}

/* ------------------------------------------------------------------ */
/* 1. Item-level push / peek_item / pop_item with C strings           */
/* ------------------------------------------------------------------ */
void test_push_peek_item_pop_item_strings(void) {
    const char *strings[] = {"alpha", "bravo", "charlie", "delta", "echo"};
    const size_t count = sizeof(strings) / sizeof(strings[0]);

    ANB_FifoSlab_t *q = ANB_fifoslab_create(256);
    TEST_ASSERT_NOT_NULL(q);

    for (size_t i = 0; i < count; i++) {
        size_t len = strlen(strings[i]) + 1;
        ANB_fifoslab_push_item(q, (const uint8_t *)strings[i], len);
    }

    TEST_ASSERT_EQUAL_size_t(count, ANB_fifoslab_item_count(q));

    /* peek_item for each index — verify content via memcmp */
    for (size_t i = 0; i < count; i++) {
        size_t item_size = 0;
        uint8_t *data = ANB_fifoslab_peek_item(q, i, &item_size);
        TEST_ASSERT_NOT_NULL(data);
        TEST_ASSERT_EQUAL_size_t(strlen(strings[i]) + 1, item_size);
        TEST_ASSERT_EQUAL_INT(0, memcmp(data, strings[i], strlen(strings[i]) + 1));
    }

    /* pop_item one by one, verify item_count decrements */
    for (size_t i = 0; i < count; i++) {
        size_t sz = ANB_fifoslab_pop_item(q);
        TEST_ASSERT_EQUAL_size_t(strlen(strings[i]) + 1, sz);
        TEST_ASSERT_EQUAL_size_t(count - i - 1, ANB_fifoslab_item_count(q));
    }

    ANB_fifoslab_destroy(q);
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

    ANB_FifoSlab_t *q = ANB_fifoslab_create(512);
    TEST_ASSERT_NOT_NULL(q);

    for (size_t i = 0; i < count; i++) {
        size_t label_len = strlen(labels[i]) + 1;
        size_t blob_size = sizeof(TestRecord) + label_len;
        uint8_t buf[sizeof(TestRecord) + MAX_LABEL];

        memcpy(buf, &records[i], sizeof(TestRecord));
        memcpy(buf + sizeof(TestRecord), labels[i], label_len);

        ANB_fifoslab_push_item(q, buf, blob_size);
    }

    TEST_ASSERT_EQUAL_size_t(count, ANB_fifoslab_item_count(q));

    /* peek_item each — verify struct fields and trailing label */
    for (size_t i = 0; i < count; i++) {
        size_t item_size = 0;
        uint8_t *data = ANB_fifoslab_peek_item(q, i, &item_size);
        TEST_ASSERT_NOT_NULL(data);

        TestRecord r;
        memcpy(&r, data, sizeof(TestRecord));
        TEST_ASSERT_EQUAL_UINT32(records[i].id, r.id);
        TEST_ASSERT_EQUAL_FLOAT(records[i].value, r.value);
        TEST_ASSERT_EQUAL_UINT16(records[i].flags, r.flags);

        const char *label = (const char *)(data + sizeof(TestRecord));
        TEST_ASSERT_EQUAL_STRING(labels[i], label);
    }

    /* pop_item each, verify returned sizes */
    for (size_t i = 0; i < count; i++) {
        size_t label_len = strlen(labels[i]) + 1;
        size_t expected = sizeof(TestRecord) + label_len;
        size_t sz = ANB_fifoslab_pop_item(q);
        TEST_ASSERT_EQUAL_size_t(expected, sz);
        TEST_ASSERT_EQUAL_size_t(count - i - 1, ANB_fifoslab_item_count(q));
    }

    ANB_fifoslab_destroy(q);
}

/* ------------------------------------------------------------------ */
/* 3. O(1) iterator over items                                        */
/* ------------------------------------------------------------------ */
void test_peek_item_iter(void) {
    const char *strings[] = {"alpha", "bravo", "charlie", "delta", "echo"};
    const size_t count = sizeof(strings) / sizeof(strings[0]);

    ANB_FifoSlab_t *q = ANB_fifoslab_create(256);
    TEST_ASSERT_NOT_NULL(q);

    for (size_t i = 0; i < count; i++) {
        size_t len = strlen(strings[i]) + 1;
        ANB_fifoslab_push_item(q, (const uint8_t *)strings[i], len);
    }

    /* Iterate all items via the O(1) iterator */
    ANB_FifoSlabIter_t iter = {0};
    size_t item_size;
    uint8_t *data;
    size_t i = 0;

    while ((data = ANB_fifoslab_peek_item_iter(q, &iter, &item_size)) != NULL) {
        TEST_ASSERT_LESS_THAN(count, i);
        TEST_ASSERT_EQUAL_size_t(strlen(strings[i]) + 1, item_size);
        TEST_ASSERT_EQUAL_STRING(strings[i], (const char *)data);
        i++;
    }
    TEST_ASSERT_EQUAL_size_t(count, i);

    /* Iterator past the end keeps returning NULL */
    TEST_ASSERT_NULL(ANB_fifoslab_peek_item_iter(q, &iter, &item_size));

    /* Pop first two items, iterate remainder with a fresh iterator */
    ANB_fifoslab_pop_item(q);
    ANB_fifoslab_pop_item(q);

    ANB_FifoSlabIter_t iter2 = {0};
    i = 2;
    while ((data = ANB_fifoslab_peek_item_iter(q, &iter2, &item_size)) != NULL) {
        TEST_ASSERT_EQUAL_STRING(strings[i], (const char *)data);
        i++;
    }
    TEST_ASSERT_EQUAL_size_t(count, i);

    ANB_fifoslab_destroy(q);
}

/* ------------------------------------------------------------------ */
/* 4. ANB_fifoslab_size reports total bytes in use                     */
/* ------------------------------------------------------------------ */
void test_size(void) {
    ANB_FifoSlab_t *q = ANB_fifoslab_create(256);
    TEST_ASSERT_NOT_NULL(q);

    TEST_ASSERT_EQUAL_size_t(0, ANB_fifoslab_size(q));

    const char *a = "hello";
    const char *b = "world!!";
    size_t a_len = strlen(a) + 1;
    size_t b_len = strlen(b) + 1;

    ANB_fifoslab_push_item(q, (const uint8_t *)a, a_len);
    TEST_ASSERT_EQUAL_size_t(ALIGN_UP(a_len), ANB_fifoslab_size(q));

    ANB_fifoslab_push_item(q, (const uint8_t *)b, b_len);
    TEST_ASSERT_EQUAL_size_t(ALIGN_UP(a_len) + ALIGN_UP(b_len), ANB_fifoslab_size(q));

    ANB_fifoslab_pop_item(q);
    TEST_ASSERT_EQUAL_size_t(ALIGN_UP(b_len), ANB_fifoslab_size(q));

    ANB_fifoslab_pop_item(q);
    TEST_ASSERT_EQUAL_size_t(0, ANB_fifoslab_size(q));

    ANB_fifoslab_destroy(q);
}

/* ------------------------------------------------------------------ */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_push_peek_item_pop_item_strings);
    RUN_TEST(test_aligned_struct_with_trailing_string);
    RUN_TEST(test_peek_item_iter);
    RUN_TEST(test_size);
    return UNITY_END();
}
