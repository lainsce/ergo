#include "str.h"

#include <string.h>

Str str_from_c(const char *s) {
    Str out;
    out.data = s ? s : "";
    out.len = s ? strlen(s) : 0;
    return out;
}

int str_eq(Str a, Str b) {
    if (a.len != b.len) {
        return 0;
    }
    if (a.len == 0) {
        return 1;
    }
    return memcmp(a.data, b.data, a.len) == 0;
}

int str_eq_c(Str a, const char *b) {
    return str_eq(a, str_from_c(b));
}
