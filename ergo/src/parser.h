#ifndef ERGO_PARSER_H
#define ERGO_PARSER_H

#include <stdbool.h>

#include "arena.h"
#include "ast.h"
#include "diag.h"
#include "lexer.h"

Module *parse_module(Tok *toks, size_t len, const char *path, Arena *arena, Diag *err);

#endif
