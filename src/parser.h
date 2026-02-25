#ifndef YIS_PARSER_H
#define YIS_PARSER_H

#include <stdbool.h>

#include "arena.h"
#include "ast.h"
#include "diag.h"
#include "lexer.h"

Module *parse_cask(Tok *toks, size_t len, const char *path, Arena *arena, Diag *err);

#endif
