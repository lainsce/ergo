#ifndef ERGO_STR_H
#define ERGO_STR_H

#include <stddef.h>

typedef struct {
    const char *data;
    size_t len;
} Str;

#define STR_LIT(s) ((Str){ (s), sizeof(s) - 1 })

static inline Str str_from_slice(const char *s, size_t len) {
    return (Str){ s, len };
}

Str str_from_c(const char *s);
int str_eq(Str a, Str b);
int str_eq_c(Str a, const char *b);

#endif
