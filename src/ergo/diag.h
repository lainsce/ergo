#ifndef ERGO_DIAG_H
#define ERGO_DIAG_H

#include <stdio.h>

typedef struct {
    const char *path;
    int line;
    int col;
    const char *message;
} Diag;

static inline void diag_print(const Diag *d) {
    if (!d) {
        return;
    }
    if (d->path) {
        fprintf(stderr, "%s:%d:%d: %s\n", d->path, d->line, d->col, d->message);
    } else {
        fprintf(stderr, "%d:%d: %s\n", d->line, d->col, d->message);
    }
}

#endif
