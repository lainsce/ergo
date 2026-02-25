#ifndef ERGO_PROJECT_H
#define ERGO_PROJECT_H

#include <stdbool.h>
#include <stdint.h>

#include "arena.h"
#include "ast.h"
#include "diag.h"

bool load_project(const char *entry_path, Arena *arena, Program **out_prog, uint64_t *out_hash, Diag *err);

#endif
