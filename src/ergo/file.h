#ifndef ERGO_FILE_H
#define ERGO_FILE_H

#include <stddef.h>
#include <stdbool.h>

#include "arena.h"
#include "diag.h"

char *read_file_arena(const char *path, Arena *arena, size_t *out_len, Diag *err);
char *read_file_with_includes(const char *path, const char *directive, Arena *arena, size_t *out_len, Diag *err);
char *path_abs(const char *path);
char *path_dirname(const char *path);
char *path_join(const char *a, const char *b);
bool path_is_file(const char *path);
bool path_has_prefix(const char *path, const char *dir);

#endif
