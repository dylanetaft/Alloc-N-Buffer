#include <stdint.h>
#include "blob.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

struct ANB_Blob {
    uint8_t *data;
    size_t capacity;
    size_t pos;
};

ANB_Blob_t* ANB_blob_create(size_t initial_size) {
    if (initial_size == 0) abort();
    ANB_Blob_t* blob = (ANB_Blob_t*)calloc(1, sizeof(ANB_Blob_t));
    if (!blob) abort();

    blob->data = (uint8_t*)malloc(initial_size);
    if (!blob->data) abort();
    blob->capacity = initial_size;

    return blob;
}

void ANB_blob_destroy(ANB_Blob_t* blob) {
    if (blob) {
        free(blob->data);
        free(blob);
    }
}

uint8_t* ANB_blob_data(ANB_Blob_t* blob) {
    if (!blob) abort();
    return blob->data;
}

size_t ANB_blob_capacity(ANB_Blob_t* blob) {
    if (!blob) abort();
    return blob->capacity;
}

void ANB_blob_alloc(ANB_Blob_t* blob, size_t bytes) {
    if (!blob) abort();
    size_t new_cap;
    if (bytes == 0) {
        new_cap = blob->capacity * 2;
    } else {
        new_cap = blob->capacity + bytes;
    }
    if (new_cap <= blob->capacity) abort();
    blob->data = (uint8_t*)realloc(blob->data, new_cap);
    if (!blob->data) abort();
    blob->capacity = new_cap;
}

void ANB_blob_realloc(ANB_Blob_t* blob, size_t new_capacity) {
    if (!blob) abort();
    if (new_capacity == 0) abort();
    blob->data = (uint8_t*)realloc(blob->data, new_capacity);
    if (!blob->data) abort();
    blob->capacity = new_capacity;
}

void ANB_blob_clear(ANB_Blob_t* blob) {
    if (!blob) abort();
    memset(blob->data, 0, blob->capacity);
    blob->pos = 0;
}

void ANB_blob_push(ANB_Blob_t* blob, const uint8_t* bytes, size_t len) {
    if (!blob) abort();
    if (len == 0) return;
    if (blob->pos + len > blob->capacity) {
        size_t new_cap = blob->capacity;
        while (new_cap < blob->pos + len) {
            new_cap *= 2;
            if (new_cap <= blob->capacity) abort();
        }
        blob->data = (uint8_t*)realloc(blob->data, new_cap);
        if (!blob->data) abort();
        blob->capacity = new_cap;
    }
    memcpy(blob->data + blob->pos, bytes, len);
    blob->pos += len;
}

size_t ANB_blob_data_len(ANB_Blob_t* blob) {
    if (!blob) abort();
    return blob->pos;
}

void ANB_blob_reset(ANB_Blob_t* blob) {
    if (!blob) abort();
    blob->pos = 0;
}
