#ifndef YIS_VEC_H
#define YIS_VEC_H

#include <stdlib.h>

static inline void vec_reserve_impl(void **data, size_t *cap, size_t elem_size, size_t need) {
    if (*cap >= need) {
        return;
    }
    size_t next = *cap ? *cap : 8;
    while (next < need) {
        next *= 2;
    }
    void *new_data = realloc(*data, next * elem_size);
    if (!new_data) {
        return;
    }
    *data = new_data;
    *cap = next;
}

#define VEC(T) struct { T *data; size_t len; size_t cap; }
#define VEC_INIT { NULL, 0, 0 }

#define VEC_PUSH(vec, value) do { \
    vec_reserve_impl((void **)&(vec).data, &(vec).cap, sizeof(*(vec).data), (vec).len + 1); \
    (vec).data[(vec).len++] = (value); \
} while (0)

#define VEC_FREE(vec) do { \
    free((vec).data); \
    (vec).data = NULL; \
    (vec).len = 0; \
    (vec).cap = 0; \
} while (0)

#endif
