#include "unity.h"
#include "blob.h"
#include <string.h>
#include <stddef.h>

void setUp_blob(void) {}
void tearDown_blob(void) {}

/* ------------------------------------------------------------------ */
/* 1. Create / destroy (NULL-safe)                                    */
/* ------------------------------------------------------------------ */
void test_create_destroy(void) {
    ANB_Blob_t *b = ANB_blob_create(256);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_EQUAL_size_t(256, ANB_blob_capacity(b));
    TEST_ASSERT_NOT_NULL(ANB_blob_data(b));

    ANB_blob_destroy(b);
    ANB_blob_destroy(NULL);
}

/* ------------------------------------------------------------------ */
/* 2. Data pointer is usable                                          */
/* ------------------------------------------------------------------ */
void test_data_usable(void) {
    ANB_Blob_t *b = ANB_blob_create(64);
    uint8_t *d = ANB_blob_data(b);

    memcpy(d, "hello world", 12);
    TEST_ASSERT_EQUAL_STRING("hello world", (const char *)d);

    ANB_blob_destroy(b);
}

/* ------------------------------------------------------------------ */
/* 3. ANB_blob_alloc with explicit bytes adds to capacity             */
/* ------------------------------------------------------------------ */
void test_alloc_explicit(void) {
    ANB_Blob_t *b = ANB_blob_create(100);
    TEST_ASSERT_EQUAL_size_t(100, ANB_blob_capacity(b));

    ANB_blob_alloc(b, 50);
    TEST_ASSERT_EQUAL_size_t(150, ANB_blob_capacity(b));

    ANB_blob_alloc(b, 200);
    TEST_ASSERT_EQUAL_size_t(350, ANB_blob_capacity(b));

    ANB_blob_destroy(b);
}

/* ------------------------------------------------------------------ */
/* 4. ANB_blob_alloc with 0 doubles capacity                          */
/* ------------------------------------------------------------------ */
void test_alloc_double(void) {
    ANB_Blob_t *b = ANB_blob_create(64);
    TEST_ASSERT_EQUAL_size_t(64, ANB_blob_capacity(b));

    ANB_blob_alloc(b, 0);
    TEST_ASSERT_EQUAL_size_t(128, ANB_blob_capacity(b));

    ANB_blob_alloc(b, 0);
    TEST_ASSERT_EQUAL_size_t(256, ANB_blob_capacity(b));

    ANB_blob_alloc(b, 0);
    TEST_ASSERT_EQUAL_size_t(512, ANB_blob_capacity(b));

    ANB_blob_destroy(b);
}

/* ------------------------------------------------------------------ */
/* 5. ANB_blob_realloc sets exact capacity                            */
/* ------------------------------------------------------------------ */
void test_realloc_exact(void) {
    ANB_Blob_t *b = ANB_blob_create(256);

    ANB_blob_realloc(b, 512);
    TEST_ASSERT_EQUAL_size_t(512, ANB_blob_capacity(b));

    ANB_blob_realloc(b, 128);
    TEST_ASSERT_EQUAL_size_t(128, ANB_blob_capacity(b));

    ANB_blob_realloc(b, 1024);
    TEST_ASSERT_EQUAL_size_t(1024, ANB_blob_capacity(b));

    ANB_blob_destroy(b);
}

/* ------------------------------------------------------------------ */
/* 6. Data pointer survives alloc (may move)                          */
/* ------------------------------------------------------------------ */
void test_data_after_alloc(void) {
    ANB_Blob_t *b = ANB_blob_create(16);
    uint8_t *d = ANB_blob_data(b);
    memcpy(d, "before", 7);

    ANB_blob_alloc(b, 64);
    d = ANB_blob_data(b);
    TEST_ASSERT_NOT_NULL(d);

    memcpy(d + 64, "after", 6);
    TEST_ASSERT_EQUAL_STRING("after", (const char *)(d + 64));

    ANB_blob_destroy(b);
}

/* ------------------------------------------------------------------ */
/* 7. Clear zeros entire buffer                                       */
/* ------------------------------------------------------------------ */
void test_clear(void) {
    ANB_Blob_t *b = ANB_blob_create(128);
    uint8_t *d = ANB_blob_data(b);

    memset(d, 0xFF, 128);
    ANB_blob_clear(b);

    for (size_t i = 0; i < 128; i++) {
        TEST_ASSERT_EQUAL_UINT8(0, d[i]);
    }

    ANB_blob_destroy(b);
}

/* ------------------------------------------------------------------ */
/* 8. Clear after alloc                                               */
/* ------------------------------------------------------------------ */
void test_clear_after_alloc(void) {
    ANB_Blob_t *b = ANB_blob_create(64);
    uint8_t *d = ANB_blob_data(b);
    memset(d, 0xAA, 64);

    ANB_blob_alloc(b, 64);
    d = ANB_blob_data(b);
    ANB_blob_clear(b);

    size_t total = ANB_blob_capacity(b);
    for (size_t i = 0; i < total; i++) {
        TEST_ASSERT_EQUAL_UINT8(0, d[i]);
    }

    ANB_blob_destroy(b);
}

/* ------------------------------------------------------------------ */
/* 9. Push basic                                                      */
/* ------------------------------------------------------------------ */
void test_push_basic(void) {
    ANB_Blob_t *b = ANB_blob_create(64);
    uint8_t data[] = {1, 2, 3, 4, 5};

    ANB_blob_push(b, data, 5);
    TEST_ASSERT_EQUAL_size_t(5, ANB_blob_data_len(b));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(data, ANB_blob_data(b), 5);

    ANB_blob_destroy(b);
}

/* ------------------------------------------------------------------ */
/* 10. Push auto-grow                                                 */
/* ------------------------------------------------------------------ */
void test_push_auto_grow(void) {
    ANB_Blob_t *b = ANB_blob_create(8);
    TEST_ASSERT_EQUAL_size_t(8, ANB_blob_capacity(b));

    uint8_t data[20];
    memset(data, 0xAA, 20);

    ANB_blob_push(b, data, 20);
    TEST_ASSERT(ANB_blob_capacity(b) >= 20);
    TEST_ASSERT_EQUAL_size_t(20, ANB_blob_data_len(b));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(data, ANB_blob_data(b), 20);

    ANB_blob_destroy(b);
}

/* ------------------------------------------------------------------ */
/* 11. Reset preserves data but zeros position                        */
/* ------------------------------------------------------------------ */
void test_reset(void) {
    ANB_Blob_t *b = ANB_blob_create(64);
    uint8_t data[] = {10, 20, 30};

    ANB_blob_push(b, data, 3);
    TEST_ASSERT_EQUAL_size_t(3, ANB_blob_data_len(b));

    ANB_blob_reset(b);
    TEST_ASSERT_EQUAL_size_t(0, ANB_blob_data_len(b));

    uint8_t *d = ANB_blob_data(b);
    TEST_ASSERT_EQUAL_UINT8(10, d[0]);
    TEST_ASSERT_EQUAL_UINT8(20, d[1]);
    TEST_ASSERT_EQUAL_UINT8(30, d[2]);

    ANB_blob_destroy(b);
}

/* ------------------------------------------------------------------ */
/* 12. Clear resets position                                          */
/* ------------------------------------------------------------------ */
void test_clear_resets_pos(void) {
    ANB_Blob_t *b = ANB_blob_create(64);
    uint8_t data[] = {1, 2, 3, 4};

    ANB_blob_push(b, data, 4);
    TEST_ASSERT_EQUAL_size_t(4, ANB_blob_data_len(b));

    ANB_blob_clear(b);
    TEST_ASSERT_EQUAL_size_t(0, ANB_blob_data_len(b));

    uint8_t *d = ANB_blob_data(b);
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL_UINT8(0, d[i]);
    }

    ANB_blob_destroy(b);
}

/* ------------------------------------------------------------------ */
/* 13. Push multiple sequential                                       */
/* ------------------------------------------------------------------ */
void test_push_multiple(void) {
    ANB_Blob_t *b = ANB_blob_create(64);
    uint8_t d1[] = {1, 2, 3};
    uint8_t d2[] = {4, 5};
    uint8_t d3[] = {6, 7, 8, 9};

    ANB_blob_push(b, d1, 3);
    ANB_blob_push(b, d2, 2);
    ANB_blob_push(b, d3, 4);

    TEST_ASSERT_EQUAL_size_t(9, ANB_blob_data_len(b));

    uint8_t expected[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, ANB_blob_data(b), 9);

    ANB_blob_destroy(b);
}
