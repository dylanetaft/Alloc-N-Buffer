#include "unity.h"
#include "fifoslab.h"
#include <string.h>
#include <stddef.h>

/* Mirrors the alignment macro used internally by ANB_FifoSlab. */
#define ALIGN_UP(x) (((x) + _Alignof(max_align_t) - 1) & ~(_Alignof(max_align_t) - 1))

void setUp(void) {}
void tearDown(void) {}

/* ------------------------------------------------------------------ */
/* 1. Raw byte-level push / peek / pop with C strings                 */
/* ------------------------------------------------------------------ */
void test_push_peek_pop_strings(void) {
    const char *strings[] = {"hello", "world", "foo", "barbaz", "x"};
    const size_t count = sizeof(strings) / sizeof(strings[0]);

    ANB_FifoSlab_t *q = ANB_fifoslab_create(256);
    TEST_ASSERT_NOT_NULL(q);

    size_t expected_total = 0;
    for (size_t i = 0; i < count; i++) {
        size_t len = strlen(strings[i]) + 1;            /* include '\0' */
        ANB_fifoslab_push(q, (const uint8_t *)strings[i], len);
        expected_total += ALIGN_UP(len);
    }

    /* peek_size should equal the sum of all aligned lengths */
    TEST_ASSERT_EQUAL_size_t(expected_total, ANB_fifoslab_peek_size(q));

    /* peek the full size — first bytes must be the first string */
    uint8_t *ptr = ANB_fifoslab_peek(q, expected_total);
    TEST_ASSERT_NOT_NULL(ptr);
    TEST_ASSERT_EQUAL_STRING("hello", (const char *)ptr);

    /* pop each string one at a time and verify the next one via peek */
    for (size_t i = 0; i < count; i++) {
        size_t aligned = ALIGN_UP(strlen(strings[i]) + 1);
        size_t remaining = ANB_fifoslab_peek_size(q);

        ptr = ANB_fifoslab_peek(q, aligned);
        TEST_ASSERT_NOT_NULL(ptr);
        TEST_ASSERT_EQUAL_STRING(strings[i], (const char *)ptr);

        size_t popped = ANB_fifoslab_pop(q, aligned);
        TEST_ASSERT_EQUAL_size_t(aligned, popped);
    }

    TEST_ASSERT_EQUAL_size_t(0, ANB_fifoslab_peek_size(q));
    ANB_fifoslab_destroy(q);
}

/* ------------------------------------------------------------------ */
/* 2. Item-level push / peek_item / pop_item with C strings           */
/* ------------------------------------------------------------------ */
void test_push_peek_item_pop_item_strings(void) {
    const char *strings[] = {"alpha", "bravo", "charlie", "delta", "echo"};
    const size_t count = sizeof(strings) / sizeof(strings[0]);

    ANB_FifoSlab_t *q = ANB_fifoslab_create(256);
    TEST_ASSERT_NOT_NULL(q);

    for (size_t i = 0; i < count; i++) {
        size_t len = strlen(strings[i]) + 1;
        ANB_fifoslab_push(q, (const uint8_t *)strings[i], len);
    }

    TEST_ASSERT_EQUAL_size_t(count, ANB_fifoslab_item_count(q));

    /* peek_item for each index — verify content via memcmp */
    for (size_t i = 0; i < count; i++) {
        size_t item_size = 0;
        uint8_t *data = ANB_fifoslab_peek_item(q, i, &item_size);
        TEST_ASSERT_NOT_NULL(data);
        TEST_ASSERT_GREATER_OR_EQUAL(strlen(strings[i]) + 1, item_size);
        TEST_ASSERT_EQUAL_INT(0, memcmp(data, strings[i], strlen(strings[i]) + 1));
    }

    /* pop_item one by one, verify item_count decrements */
    for (size_t i = 0; i < count; i++) {
        size_t sz = ANB_fifoslab_pop_item(q);
        TEST_ASSERT_EQUAL_size_t(ALIGN_UP(strlen(strings[i]) + 1), sz);
        TEST_ASSERT_EQUAL_size_t(count - i - 1, ANB_fifoslab_item_count(q));
    }

    ANB_fifoslab_destroy(q);
}

/* ------------------------------------------------------------------ */
/* 3. Struct + trailing string composite blobs                        */
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

        ANB_fifoslab_push(q, buf, blob_size);
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
        size_t expected = ALIGN_UP(sizeof(TestRecord) + label_len);
        size_t sz = ANB_fifoslab_pop_item(q);
        TEST_ASSERT_EQUAL_size_t(expected, sz);
        TEST_ASSERT_EQUAL_size_t(count - i - 1, ANB_fifoslab_item_count(q));
    }

    ANB_fifoslab_destroy(q);
}

/* ------------------------------------------------------------------ */
/* 4. Push struct then string; raw-pop the struct, item-pop the string */
/* ------------------------------------------------------------------ */
void test_raw_pop_struct_then_item_pop_string(void) {
    TestRecord rec = {42, 9.81f, 0xBEEF};
    const char *msg = "hello-mixed";
    size_t msg_len = strlen(msg) + 1;

    ANB_FifoSlab_t *q = ANB_fifoslab_create(256);
    TEST_ASSERT_NOT_NULL(q);

    /* Push struct as item 0, string as item 1 */
    ANB_fifoslab_push(q, (const uint8_t *)&rec, sizeof(TestRecord));
    ANB_fifoslab_push(q, (const uint8_t *)msg, msg_len);
    TEST_ASSERT_EQUAL_size_t(2, ANB_fifoslab_item_count(q));

    /* Raw-pop exactly the struct's aligned size */
    size_t struct_aligned = ALIGN_UP(sizeof(TestRecord));
    size_t popped = ANB_fifoslab_pop(q, struct_aligned);
    TEST_ASSERT_EQUAL_size_t(struct_aligned, popped);

    /* Item 0 in the index was consumed by raw pop; item_count should be 1 */
    TEST_ASSERT_EQUAL_size_t(1, ANB_fifoslab_item_count(q));

    /* peek_item(0) should now be the string */
    size_t item_size = 0;
    uint8_t *data = ANB_fifoslab_peek_item(q, 0, &item_size);
    TEST_ASSERT_NOT_NULL(data);
    TEST_ASSERT_EQUAL_size_t(ALIGN_UP(msg_len), item_size);
    TEST_ASSERT_EQUAL_STRING(msg, (const char *)data);

    /* pop_item should return the string's aligned size */
    size_t sz = ANB_fifoslab_pop_item(q);
    TEST_ASSERT_EQUAL_size_t(ALIGN_UP(msg_len), sz);
    TEST_ASSERT_EQUAL_size_t(0, ANB_fifoslab_item_count(q));

    ANB_fifoslab_destroy(q);
}

/* ------------------------------------------------------------------ */
/* 5. O(1) iterator over items                                        */
/* ------------------------------------------------------------------ */
void test_peek_item_iter(void) {
    const char *strings[] = {"alpha", "bravo", "charlie", "delta", "echo"};
    const size_t count = sizeof(strings) / sizeof(strings[0]);

    ANB_FifoSlab_t *q = ANB_fifoslab_create(256);
    TEST_ASSERT_NOT_NULL(q);

    for (size_t i = 0; i < count; i++) {
        size_t len = strlen(strings[i]) + 1;
        ANB_fifoslab_push(q, (const uint8_t *)strings[i], len);
    }

    /* Iterate all items via the O(1) iterator */
    ANB_FifoSlabIter_t iter = {0};
    size_t item_size;
    uint8_t *data;
    size_t i = 0;

    while ((data = ANB_fifoslab_peek_item_iter(q, &iter, &item_size)) != NULL) {
        TEST_ASSERT_LESS_THAN(count, i);
        TEST_ASSERT_EQUAL_size_t(ALIGN_UP(strlen(strings[i]) + 1), item_size);
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
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_push_peek_pop_strings);
    RUN_TEST(test_push_peek_item_pop_item_strings);
    RUN_TEST(test_aligned_struct_with_trailing_string);
    RUN_TEST(test_raw_pop_struct_then_item_pop_string);
    RUN_TEST(test_peek_item_iter);
    return UNITY_END();
}
