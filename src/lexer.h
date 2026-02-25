#ifndef YIS_LEXER_H
#define YIS_LEXER_H

#include <stddef.h>
#include <stdbool.h>

#include "arena.h"
#include "diag.h"
#include "str.h"
#include "vec.h"

typedef struct Expr Expr;

typedef enum {
    TOK_INVALID = 0,
    TOK_EOF,
    TOK_IDENT,
    TOK_INT,
    TOK_FLOAT,
    TOK_STR,
    TOK_SEMI,
    TOK_LPAR,
    TOK_RPAR,
    TOK_LBRACK,
    TOK_RBRACK,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_COMMA,
    TOK_DOT,
    TOK_COLON,
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,
    TOK_PERCENT,
    TOK_BANG,
    TOK_EQ,
    TOK_LT,
    TOK_GT,
    TOK_BAR,
    TOK_EQEQ,
    TOK_NEQ,
    TOK_LTE,
    TOK_GTE,
    TOK_ANDAND,
    TOK_OROR,
    TOK_ARROW,
    TOK_PLUSEQ,
    TOK_MINUSEQ,
    TOK_STAREQ,
    TOK_SLASHEQ,
    TOK_QMARK,
    TOK_QQ,
    TOK_HASH,
    TOK_RET_L,
    TOK_RET_R,
    TOK_RET_VOID,
    TOK_KW_cask,
    TOK_KW_bring,
    TOK_KW_fun,
    TOK_KW_macro,
    TOK_KW_entry,
    TOK_KW_class,
    TOK_KW_struct,
    TOK_KW_enum,
    TOK_KW_pub,
    TOK_KW_lock,
    TOK_KW_seal,
    TOK_KW_def,
    TOK_KW_let,
    TOK_KW_const,
    TOK_KW_if,
    TOK_KW_else,
    TOK_KW_elif,
    TOK_KW_return,
    TOK_KW_true,
    TOK_KW_false,
    TOK_KW_null,
    TOK_KW_for,
    TOK_KW_match,
    TOK_KW_new,
    TOK_KW_in,
    TOK_KW_break,
    TOK_KW_continue
} TokKind;

const char *tok_kind_name(TokKind kind);
const char *tok_kind_desc(TokKind kind);

typedef enum {
    STR_PART_TEXT,
    STR_PART_EXPR_RAW,
    STR_PART_EXPR
} StrPartKind;

typedef struct {
    StrPartKind kind;
    union {
        Str text;
        Expr *expr;
    } as;
} StrPart;

typedef struct {
    StrPart *parts;
    size_t len;
} StrParts;

typedef struct {
    TokKind kind;
    Str text;
    int line;
    int col;
    union {
        long long i;
        double f;
        Str ident;
        StrParts *str;
    } val;
} Tok;

typedef VEC(Tok) TokVec;

bool lex_source(const char *path, const char *src, size_t len, Arena *arena, TokVec *out, Diag *err);

#endif
