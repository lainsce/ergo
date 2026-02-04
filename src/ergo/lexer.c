#include "lexer.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

typedef struct {
    const char *path;
    const char *src;
    size_t len;
    size_t i;
    int line;
    int col;
    int nest;
    int ret_depth;
    TokKind last_real;
    TokKind last_sig;
    Arena *arena;
} Lexer;

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} CharVec;

typedef struct {
    StrPart *data;
    size_t len;
    size_t cap;
} StrPartVec;

static const char *tok_name_default(TokKind kind) {
    switch (kind) {
        case TOK_EOF: return "EOF";
        case TOK_IDENT: return "IDENT";
        case TOK_INT: return "INT";
        case TOK_FLOAT: return "FLOAT";
        case TOK_STR: return "STR";
        case TOK_SEMI: return "SEMI";
        case TOK_LPAR: return "LPAR";
        case TOK_RPAR: return "RPAR";
        case TOK_LBRACK: return "LBRACK";
        case TOK_RBRACK: return "RBRACK";
        case TOK_LBRACE: return "LBRACE";
        case TOK_RBRACE: return "RBRACE";
        case TOK_COMMA: return "COMMA";
        case TOK_DOT: return "DOT";
        case TOK_COLON: return "COLON";
        case TOK_PLUS: return "+";
        case TOK_MINUS: return "-";
        case TOK_STAR: return "*";
        case TOK_SLASH: return "/";
        case TOK_PERCENT: return "%";
        case TOK_BANG: return "!";
        case TOK_EQ: return "=";
        case TOK_LT: return "<";
        case TOK_GT: return ">";
        case TOK_BAR: return "BAR";
        case TOK_EQEQ: return "==";
        case TOK_NEQ: return "!=";
        case TOK_LTE: return "<=";
        case TOK_GTE: return ">=";
        case TOK_ANDAND: return "&&";
        case TOK_OROR: return "||";
        case TOK_ARROW: return "=>";
        case TOK_PLUSEQ: return "+=";
        case TOK_MINUSEQ: return "-=";
        case TOK_STAREQ: return "*=";
        case TOK_SLASHEQ: return "/=";
        case TOK_QMARK: return "QMARK";
        case TOK_HASH: return "#";
        case TOK_RET_L: return "((";
        case TOK_RET_R: return "))";
        case TOK_RET_VOID: return "--";
        case TOK_KW_module: return "KW_module";
        case TOK_KW_bring: return "KW_bring";
        case TOK_KW_fun: return "KW_fun";
        case TOK_KW_entry: return "KW_entry";
        case TOK_KW_class: return "KW_class";
        case TOK_KW_pub: return "KW_pub";
        case TOK_KW_lock: return "KW_lock";
        case TOK_KW_seal: return "KW_seal";
        case TOK_KW_def: return "KW_def";
        case TOK_KW_let: return "KW_let";
        case TOK_KW_const: return "KW_const";
        case TOK_KW_if: return "KW_if";
        case TOK_KW_else: return "KW_else";
        case TOK_KW_elif: return "KW_elif";
        case TOK_KW_return: return "KW_return";
        case TOK_KW_true: return "KW_true";
        case TOK_KW_false: return "KW_false";
        case TOK_KW_null: return "KW_null";
        case TOK_KW_for: return "KW_for";
        case TOK_KW_match: return "KW_match";
        case TOK_KW_new: return "KW_new";
        case TOK_KW_in: return "KW_in";
        default: return "<invalid>";
    }
}

const char *tok_kind_name(TokKind kind) {
    return tok_name_default(kind);
}

static char peek(Lexer *lx, size_t k) {
    size_t idx = lx->i + k;
    if (idx >= lx->len) {
        return '\0';
    }
    return lx->src[idx];
}

static void adv(Lexer *lx, size_t n) {
    for (size_t k = 0; k < n; k++) {
        if (lx->i >= lx->len) {
            return;
        }
        char ch = lx->src[lx->i++];
        if (ch == '\n') {
            lx->line += 1;
            lx->col = 1;
        } else {
            lx->col += 1;
        }
    }
}

static bool is_ident_start(char ch) {
    return isalpha((unsigned char)ch) || ch == '_';
}

static bool is_ident_mid(char ch) {
    return isalnum((unsigned char)ch) || ch == '_';
}

static Str str_from_slice(const char *data, size_t len) {
    Str out;
    out.data = data;
    out.len = len;
    return out;
}

static char *arena_strndup(Arena *arena, const char *s, size_t len) {
    if (!arena) {
        return NULL;
    }
    char *out = (char *)arena_alloc(arena, len + 1);
    if (!out) {
        return NULL;
    }
    if (len) {
        memcpy(out, s, len);
    }
    out[len] = '\0';
    return out;
}

static Str arena_str(Arena *arena, const char *s, size_t len) {
    char *buf = arena_strndup(arena, s, len);
    Str out;
    out.data = buf ? buf : "";
    out.len = buf ? len : 0;
    return out;
}

static bool tokvec_push(TokVec *vec, Tok tok, Diag *err, Lexer *lx) {
    if (vec->len + 1 > vec->cap) {
        size_t next = vec->cap ? vec->cap * 2 : 64;
        while (next < vec->len + 1) {
            next *= 2;
        }
        Tok *new_data = (Tok *)realloc(vec->data, next * sizeof(Tok));
        if (!new_data) {
            if (err) {
                err->path = lx->path;
                err->line = lx->line;
                err->col = lx->col;
                err->message = "out of memory";
            }
            return false;
        }
        vec->data = new_data;
        vec->cap = next;
    }
    vec->data[vec->len++] = tok;
    return true;
}

static bool charvec_push(CharVec *vec, char c, Diag *err, Lexer *lx) {
    if (vec->len + 1 > vec->cap) {
        size_t next = vec->cap ? vec->cap * 2 : 64;
        while (next < vec->len + 1) {
            next *= 2;
        }
        char *new_data = (char *)realloc(vec->data, next);
        if (!new_data) {
            if (err) {
                err->path = lx->path;
                err->line = lx->line;
                err->col = lx->col;
                err->message = "out of memory";
            }
            return false;
        }
        vec->data = new_data;
        vec->cap = next;
    }
    vec->data[vec->len++] = c;
    return true;
}

static void charvec_free(CharVec *vec) {
    free(vec->data);
    vec->data = NULL;
    vec->len = 0;
    vec->cap = 0;
}

static bool strpartvec_push(StrPartVec *vec, StrPart part, Diag *err, Lexer *lx) {
    if (vec->len + 1 > vec->cap) {
        size_t next = vec->cap ? vec->cap * 2 : 16;
        while (next < vec->len + 1) {
            next *= 2;
        }
        StrPart *new_data = (StrPart *)realloc(vec->data, next * sizeof(StrPart));
        if (!new_data) {
            if (err) {
                err->path = lx->path;
                err->line = lx->line;
                err->col = lx->col;
                err->message = "out of memory";
            }
            return false;
        }
        vec->data = new_data;
        vec->cap = next;
    }
    vec->data[vec->len++] = part;
    return true;
}

static void strpartvec_free(StrPartVec *vec) {
    free(vec->data);
    vec->data = NULL;
    vec->len = 0;
    vec->cap = 0;
}

static bool set_error(Lexer *lx, Diag *err, int line, int col, const char *fmt, ...) {
    if (!err) {
        return false;
    }
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    err->path = lx->path;
    err->line = line;
    err->col = col;
    err->message = arena_strndup(lx->arena, buf, strlen(buf));
    if (!err->message) {
        err->message = "lex error";
    }
    return false;
}

static bool append_utf8(CharVec *buf, uint32_t code, Diag *err, Lexer *lx) {
    if (code <= 0x7F) {
        return charvec_push(buf, (char)code, err, lx);
    }
    if (code <= 0x7FF) {
        if (!charvec_push(buf, (char)(0xC0 | ((code >> 6) & 0x1F)), err, lx)) return false;
        return charvec_push(buf, (char)(0x80 | (code & 0x3F)), err, lx);
    }
    if (code <= 0xFFFF) {
        if (!charvec_push(buf, (char)(0xE0 | ((code >> 12) & 0x0F)), err, lx)) return false;
        if (!charvec_push(buf, (char)(0x80 | ((code >> 6) & 0x3F)), err, lx)) return false;
        return charvec_push(buf, (char)(0x80 | (code & 0x3F)), err, lx);
    }
    if (code <= 0x10FFFF) {
        if (!charvec_push(buf, (char)(0xF0 | ((code >> 18) & 0x07)), err, lx)) return false;
        if (!charvec_push(buf, (char)(0x80 | ((code >> 12) & 0x3F)), err, lx)) return false;
        if (!charvec_push(buf, (char)(0x80 | ((code >> 6) & 0x3F)), err, lx)) return false;
        return charvec_push(buf, (char)(0x80 | (code & 0x3F)), err, lx);
    }
    return true;
}

static bool is_stmt_end(TokKind kind) {
    switch (kind) {
        case TOK_RBRACE:
        case TOK_SEMI:
        case TOK_RPAR:
        case TOK_RBRACK:
        case TOK_INT:
        case TOK_FLOAT:
        case TOK_IDENT:
        case TOK_STR:
        case TOK_KW_true:
        case TOK_KW_false:
        case TOK_KW_null:
            return true;
        default:
            return false;
    }
}

static void set_last(Lexer *lx, TokKind kind) {
    lx->last_real = kind;
    if (kind != TOK_SEMI) {
        lx->last_sig = kind;
    }
}

static bool emit_tok(Lexer *lx, TokVec *out, Diag *err, TokKind kind, Str text, int line, int col, Tok *tok) {
    Tok t;
    memset(&t, 0, sizeof(t));
    t.kind = kind;
    t.text = text;
    t.line = line;
    t.col = col;
    if (tok) {
        *tok = t;
    }
    return tokvec_push(out, t, err, lx);
}

static bool emit_simple(Lexer *lx, TokVec *out, Diag *err, TokKind kind, Str text, int line, int col) {
    return emit_tok(lx, out, err, kind, text, line, col, NULL);
}

static bool emit_ident(Lexer *lx, TokVec *out, Diag *err, Str text, int line, int col) {
    Tok t;
    if (!emit_tok(lx, out, err, TOK_IDENT, text, line, col, &t)) {
        return false;
    }
    out->data[out->len - 1].val.ident = text;
    return true;
}

static bool emit_int(Lexer *lx, TokVec *out, Diag *err, Str text, long long value, int line, int col) {
    Tok t;
    if (!emit_tok(lx, out, err, TOK_INT, text, line, col, &t)) {
        return false;
    }
    out->data[out->len - 1].val.i = value;
    return true;
}

static bool emit_float(Lexer *lx, TokVec *out, Diag *err, Str text, double value, int line, int col) {
    Tok t;
    if (!emit_tok(lx, out, err, TOK_FLOAT, text, line, col, &t)) {
        return false;
    }
    out->data[out->len - 1].val.f = value;
    return true;
}

static bool emit_str(Lexer *lx, TokVec *out, Diag *err, Str text, StrParts *parts, int line, int col) {
    Tok t;
    if (!emit_tok(lx, out, err, TOK_STR, text, line, col, &t)) {
        return false;
    }
    out->data[out->len - 1].val.str = parts;
    return true;
}

static bool make_str_parts(Lexer *lx, StrPartVec *parts, StrParts **out_parts) {
    StrParts *sp = (StrParts *)arena_alloc(lx->arena, sizeof(StrParts));
    if (!sp) {
        return false;
    }
    sp->len = parts->len;
    if (parts->len == 0) {
        sp->parts = NULL;
    } else {
        sp->parts = (StrPart *)arena_alloc(lx->arena, sizeof(StrPart) * parts->len);
        if (!sp->parts) {
            return false;
        }
        memcpy(sp->parts, parts->data, sizeof(StrPart) * parts->len);
    }
    *out_parts = sp;
    return true;
}

static bool flush_text_part(Lexer *lx, CharVec *buf, StrPartVec *parts, Diag *err) {
    if (buf->len == 0) {
        return true;
    }
    StrPart part;
    part.kind = STR_PART_TEXT;
    part.text = arena_str(lx->arena, buf->data, buf->len);
    if (!strpartvec_push(parts, part, err, lx)) {
        return false;
    }
    buf->len = 0;
    return true;
}

static bool append_hex_code(Lexer *lx, CharVec *buf, const char *hex, size_t hex_len, Diag *err, int line, int col) {
    if (hex_len == 0) {
        return set_error(lx, err, line, col, "bad \\u{...} escape");
    }
    uint32_t code = 0;
    for (size_t i = 0; i < hex_len; i++) {
        char c = hex[i];
        int val = 0;
        if (c >= '0' && c <= '9') {
            val = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            val = 10 + (c - 'a');
        } else if (c >= 'A' && c <= 'F') {
            val = 10 + (c - 'A');
        } else {
            return set_error(lx, err, line, col, "bad \\u{...} escape");
        }
        code = (code << 4) | (uint32_t)val;
        if (code > 0x10FFFF) {
            return set_error(lx, err, line, col, "bad \\u{...} escape");
        }
    }
    return append_utf8(buf, code, err, lx);
}

bool lex_source(const char *path, const char *src, size_t len, Arena *arena, TokVec *out, Diag *err) {
    if (!out) {
        return false;
    }
    out->data = NULL;
    out->len = 0;
    out->cap = 0;

    Lexer lx;
    lx.path = path ? path : "";
    lx.src = src ? src : "";
    lx.len = len;
    lx.i = 0;
    lx.line = 1;
    lx.col = 1;
    lx.nest = 0;
    lx.ret_depth = 0;
    lx.last_real = TOK_INVALID;
    lx.last_sig = TOK_INVALID;
    lx.arena = arena;

    while (lx.i < lx.len) {
        char ch = peek(&lx, 0);
        char two0 = ch;
        char two1 = peek(&lx, 1);

        if (ch == ' ' || ch == '\t' || ch == '\r') {
            adv(&lx, 1);
            continue;
        }

        if (ch == '\n') {
            adv(&lx, 1);
            if (lx.nest == 0 && is_stmt_end(lx.last_sig)) {
                if (!emit_simple(&lx, out, err, TOK_SEMI, str_from_c(";"), lx.line - 1, 0)) {
                    return false;
                }
                lx.last_real = TOK_SEMI;
            }
            continue;
        }

        if (two0 == '(' && two1 == '(' && lx.ret_depth == 0 && lx.last_sig == TOK_RPAR) {
            if (!emit_simple(&lx, out, err, TOK_RET_L, str_from_c("(("), lx.line, lx.col)) {
                return false;
            }
            adv(&lx, 2);
            lx.ret_depth += 1;
            set_last(&lx, TOK_RET_L);
            continue;
        }

        if (two0 == ')' && two1 == ')' && lx.ret_depth > 0) {
            if (!emit_simple(&lx, out, err, TOK_RET_R, str_from_c("))"), lx.line, lx.col)) {
                return false;
            }
            adv(&lx, 2);
            if (lx.ret_depth > 0) {
                lx.ret_depth -= 1;
            }
            set_last(&lx, TOK_RET_R);
            continue;
        }

        if (two0 == '-' && two1 == '-' && lx.ret_depth > 0) {
            if (!emit_simple(&lx, out, err, TOK_RET_VOID, str_from_c("--"), lx.line, lx.col)) {
                return false;
            }
            adv(&lx, 2);
            set_last(&lx, TOK_RET_VOID);
            continue;
        }

        if (two0 == '-' && two1 == '-' && lx.ret_depth == 0) {
            adv(&lx, 2);
            while (lx.i < lx.len && peek(&lx, 0) != '\n') {
                adv(&lx, 1);
            }
            continue;
        }

        if (two0 == '=' && two1 == '=') {
            if (!emit_simple(&lx, out, err, TOK_EQEQ, str_from_c("=="), lx.line, lx.col)) {
                return false;
            }
            adv(&lx, 2);
            set_last(&lx, TOK_EQEQ);
            continue;
        }
        if (two0 == '!' && two1 == '=') {
            if (!emit_simple(&lx, out, err, TOK_NEQ, str_from_c("!="), lx.line, lx.col)) {
                return false;
            }
            adv(&lx, 2);
            set_last(&lx, TOK_NEQ);
            continue;
        }
        if (two0 == '<' && two1 == '=') {
            if (!emit_simple(&lx, out, err, TOK_LTE, str_from_c("<="), lx.line, lx.col)) {
                return false;
            }
            adv(&lx, 2);
            set_last(&lx, TOK_LTE);
            continue;
        }
        if (two0 == '>' && two1 == '=') {
            if (!emit_simple(&lx, out, err, TOK_GTE, str_from_c(">="), lx.line, lx.col)) {
                return false;
            }
            adv(&lx, 2);
            set_last(&lx, TOK_GTE);
            continue;
        }
        if (two0 == '&' && two1 == '&') {
            if (!emit_simple(&lx, out, err, TOK_ANDAND, str_from_c("&&"), lx.line, lx.col)) {
                return false;
            }
            adv(&lx, 2);
            set_last(&lx, TOK_ANDAND);
            continue;
        }
        if (two0 == '|' && two1 == '|') {
            if (!emit_simple(&lx, out, err, TOK_OROR, str_from_c("||"), lx.line, lx.col)) {
                return false;
            }
            adv(&lx, 2);
            set_last(&lx, TOK_OROR);
            continue;
        }
        if (two0 == '=' && two1 == '>') {
            if (!emit_simple(&lx, out, err, TOK_ARROW, str_from_c("=>"), lx.line, lx.col)) {
                return false;
            }
            adv(&lx, 2);
            set_last(&lx, TOK_ARROW);
            continue;
        }
        if (two0 == '+' && two1 == '=') {
            if (!emit_simple(&lx, out, err, TOK_PLUSEQ, str_from_c("+="), lx.line, lx.col)) {
                return false;
            }
            adv(&lx, 2);
            set_last(&lx, TOK_PLUSEQ);
            continue;
        }
        if (two0 == '-' && two1 == '=') {
            if (!emit_simple(&lx, out, err, TOK_MINUSEQ, str_from_c("-="), lx.line, lx.col)) {
                return false;
            }
            adv(&lx, 2);
            set_last(&lx, TOK_MINUSEQ);
            continue;
        }
        if (two0 == '*' && two1 == '=') {
            if (!emit_simple(&lx, out, err, TOK_STAREQ, str_from_c("*="), lx.line, lx.col)) {
                return false;
            }
            adv(&lx, 2);
            set_last(&lx, TOK_STAREQ);
            continue;
        }
        if (two0 == '/' && two1 == '=') {
            if (!emit_simple(&lx, out, err, TOK_SLASHEQ, str_from_c("/="), lx.line, lx.col)) {
                return false;
            }
            adv(&lx, 2);
            set_last(&lx, TOK_SLASHEQ);
            continue;
        }

        if (ch == ';') {
            if (!emit_simple(&lx, out, err, TOK_SEMI, str_from_c(";"), lx.line, lx.col)) {
                return false;
            }
            adv(&lx, 1);
            lx.last_real = TOK_SEMI;
            continue;
        }

        if (ch == '(' || ch == ')' || ch == '[' || ch == ']' || ch == '{' || ch == '}' ||
            ch == ',' || ch == '.' || ch == ':' || ch == '+' || ch == '-' || ch == '*' ||
            ch == '/' || ch == '%' || ch == '!' || ch == '=' || ch == '<' || ch == '>' ||
            ch == '|') {
            TokKind kind = TOK_INVALID;
            switch (ch) {
                case '(': kind = TOK_LPAR; break;
                case ')': kind = TOK_RPAR; break;
                case '[': kind = TOK_LBRACK; break;
                case ']': kind = TOK_RBRACK; break;
                case '{': kind = TOK_LBRACE; break;
                case '}': kind = TOK_RBRACE; break;
                case ',': kind = TOK_COMMA; break;
                case '.': kind = TOK_DOT; break;
                case ':': kind = TOK_COLON; break;
                case '+': kind = TOK_PLUS; break;
                case '-': kind = TOK_MINUS; break;
                case '*': kind = TOK_STAR; break;
                case '/': kind = TOK_SLASH; break;
                case '%': kind = TOK_PERCENT; break;
                case '!': kind = TOK_BANG; break;
                case '=': kind = TOK_EQ; break;
                case '<': kind = TOK_LT; break;
                case '>': kind = TOK_GT; break;
                case '|': kind = TOK_BAR; break;
                default: kind = TOK_INVALID; break;
            }
            if (!emit_simple(&lx, out, err, kind, str_from_slice(&lx.src[lx.i], 1), lx.line, lx.col)) {
                return false;
            }
            adv(&lx, 1);

            if (ch == '(' || ch == '[' || ch == '{') {
                lx.nest += 1;
            } else if (ch == ')' || ch == ']' || ch == '}') {
                if (lx.nest > 0) {
                    lx.nest -= 1;
                }
            }

            set_last(&lx, kind);
            continue;
        }

        if (ch == '?') {
            if (!emit_simple(&lx, out, err, TOK_QMARK, str_from_c("?"), lx.line, lx.col)) {
                return false;
            }
            adv(&lx, 1);
            set_last(&lx, TOK_QMARK);
            continue;
        }

        if (ch == '#') {
            if (!emit_simple(&lx, out, err, TOK_HASH, str_from_c("#"), lx.line, lx.col)) {
                return false;
            }
            adv(&lx, 1);
            set_last(&lx, TOK_HASH);
            continue;
        }

        if (ch == '@' && peek(&lx, 1) == '"') {
            int start_line = lx.line;
            int start_col = lx.col;
            adv(&lx, 2);

            StrPartVec parts = {0};
            CharVec buf = {0};

            while (lx.i < lx.len) {
                char c = peek(&lx, 0);
                if (c == '"') {
                    adv(&lx, 1);
                    if (!flush_text_part(&lx, &buf, &parts, err)) {
                        strpartvec_free(&parts);
                        charvec_free(&buf);
                        return false;
                    }
                    StrParts *sp = NULL;
                    if (!make_str_parts(&lx, &parts, &sp)) {
                        strpartvec_free(&parts);
                        charvec_free(&buf);
                        return set_error(&lx, err, start_line, start_col, "out of memory");
                    }
                    if (!emit_str(&lx, out, err, str_from_c("@\"...\""), sp, start_line, start_col)) {
                        strpartvec_free(&parts);
                        charvec_free(&buf);
                        return false;
                    }
                    set_last(&lx, TOK_STR);
                    break;
                }
                if (c == '\n') {
                    strpartvec_free(&parts);
                    charvec_free(&buf);
                    return set_error(&lx, err, start_line, start_col, "unterminated string");
                }
                if (c == '\\') {
                    adv(&lx, 1);
                    char e = peek(&lx, 0);
                    if (e == 'n') {
                        if (!charvec_push(&buf, '\n', err, &lx)) { return false; }
                        adv(&lx, 1);
                    } else if (e == 't') {
                        if (!charvec_push(&buf, '\t', err, &lx)) { return false; }
                        adv(&lx, 1);
                    } else if (e == 'r') {
                        if (!charvec_push(&buf, '\r', err, &lx)) { return false; }
                        adv(&lx, 1);
                    } else if (e == '\\') {
                        if (!charvec_push(&buf, '\\', err, &lx)) { return false; }
                        adv(&lx, 1);
                    } else if (e == '"') {
                        if (!charvec_push(&buf, '"', err, &lx)) { return false; }
                        adv(&lx, 1);
                    } else if (e == '$') {
                        if (!charvec_push(&buf, '$', err, &lx)) { return false; }
                        adv(&lx, 1);
                    } else if (e == 'u' && peek(&lx, 1) == '{') {
                        adv(&lx, 2);
                        CharVec hexbuf = {0};
                        while (lx.i < lx.len && peek(&lx, 0) != '}') {
                            if (!charvec_push(&hexbuf, peek(&lx, 0), err, &lx)) { return false; }
                            adv(&lx, 1);
                        }
                        if (peek(&lx, 0) != '}') {
                            strpartvec_free(&parts);
                            charvec_free(&buf);
                            charvec_free(&hexbuf);
                            return set_error(&lx, err, lx.line, lx.col, "bad \\u{...} escape");
                        }
                        adv(&lx, 1);
                        if (!append_hex_code(&lx, &buf, hexbuf.data, hexbuf.len, err, lx.line, lx.col)) {
                            strpartvec_free(&parts);
                            charvec_free(&buf);
                            charvec_free(&hexbuf);
                            return false;
                        }
                        charvec_free(&hexbuf);
                    } else {
                        strpartvec_free(&parts);
                        charvec_free(&buf);
                        return set_error(&lx, err, lx.line, lx.col, "unknown escape");
                    }
                    continue;
                }
                if (c == '$') {
                    if (!is_ident_start(peek(&lx, 1))) {
                        if (!charvec_push(&buf, '$', err, &lx)) { return false; }
                        adv(&lx, 1);
                        continue;
                    }
                    if (!flush_text_part(&lx, &buf, &parts, err)) {
                        strpartvec_free(&parts);
                        charvec_free(&buf);
                        return false;
                    }
                    adv(&lx, 1);
                    CharVec namebuf = {0};
                    while (lx.i < lx.len && is_ident_mid(peek(&lx, 0))) {
                        if (!charvec_push(&namebuf, peek(&lx, 0), err, &lx)) { return false; }
                        adv(&lx, 1);
                    }
                    StrPart part;
                    part.kind = STR_PART_VAR;
                    part.text = arena_str(lx.arena, namebuf.data, namebuf.len);
                    if (!strpartvec_push(&parts, part, err, &lx)) { return false; }
                    charvec_free(&namebuf);
                    continue;
                }
                if (!charvec_push(&buf, c, err, &lx)) { return false; }
                adv(&lx, 1);
            }
            strpartvec_free(&parts);
            charvec_free(&buf);
            continue;
        }

        if (ch == '"') {
            int start_line = lx.line;
            int start_col = lx.col;
            adv(&lx, 1);
            CharVec buf_raw = {0};
            while (lx.i < lx.len) {
                char c = peek(&lx, 0);
                if (c == '"') {
                    adv(&lx, 1);
                    StrPart part;
                    part.kind = STR_PART_TEXT;
                    part.text = arena_str(lx.arena, buf_raw.data, buf_raw.len);
                    StrPartVec parts = {0};
                    if (!strpartvec_push(&parts, part, err, &lx)) { return false; }
                    StrParts *sp = NULL;
                    if (!make_str_parts(&lx, &parts, &sp)) {
                        charvec_free(&buf_raw);
                        strpartvec_free(&parts);
                        return set_error(&lx, err, start_line, start_col, "out of memory");
                    }
                    if (!emit_str(&lx, out, err, str_from_c("\"...\""), sp, start_line, start_col)) {
                        charvec_free(&buf_raw);
                        strpartvec_free(&parts);
                        return false;
                    }
                    charvec_free(&buf_raw);
                    strpartvec_free(&parts);
                    set_last(&lx, TOK_STR);
                    break;
                }
                if (c == '\n') {
                    charvec_free(&buf_raw);
                    return set_error(&lx, err, start_line, start_col, "unterminated string");
                }
                if (!charvec_push(&buf_raw, c, err, &lx)) { return false; }
                adv(&lx, 1);
            }
            continue;
        }

        if (isdigit((unsigned char)ch)) {
            int start_line = lx.line;
            int start_col = lx.col;
            size_t start = lx.i;
            while (lx.i < lx.len && isdigit((unsigned char)peek(&lx, 0))) {
                adv(&lx, 1);
            }
            bool is_float = false;
            if (peek(&lx, 0) == '.' && isdigit((unsigned char)peek(&lx, 1))) {
                is_float = true;
                adv(&lx, 1);
                while (lx.i < lx.len && isdigit((unsigned char)peek(&lx, 0))) {
                    adv(&lx, 1);
                }
            }
            size_t end = lx.i;
            Str text = str_from_slice(&lx.src[start], end - start);
            if (is_float) {
                char *tmp = (char *)malloc(text.len + 1);
                if (!tmp) {
                    return set_error(&lx, err, start_line, start_col, "out of memory");
                }
                memcpy(tmp, text.data, text.len);
                tmp[text.len] = '\0';
                double val = strtod(tmp, NULL);
                free(tmp);
                if (!emit_float(&lx, out, err, text, val, start_line, start_col)) {
                    return false;
                }
                set_last(&lx, TOK_FLOAT);
            } else {
                long long val = 0;
                for (size_t k = 0; k < text.len; k++) {
                    val = val * 10 + (long long)(text.data[k] - '0');
                }
                if (!emit_int(&lx, out, err, text, val, start_line, start_col)) {
                    return false;
                }
                set_last(&lx, TOK_INT);
            }
            continue;
        }

        if (is_ident_start(ch)) {
            int start_line = lx.line;
            int start_col = lx.col;
            size_t start = lx.i;
            while (lx.i < lx.len && is_ident_mid(peek(&lx, 0))) {
                adv(&lx, 1);
            }
            size_t end = lx.i;
            Str word = str_from_slice(&lx.src[start], end - start);

            TokKind kw = TOK_INVALID;
            if (str_eq_c(word, "module")) kw = TOK_KW_module;
            else if (str_eq_c(word, "bring")) kw = TOK_KW_bring;
            else if (str_eq_c(word, "fun")) kw = TOK_KW_fun;
            else if (str_eq_c(word, "entry")) kw = TOK_KW_entry;
            else if (str_eq_c(word, "class")) kw = TOK_KW_class;
            else if (str_eq_c(word, "pub")) kw = TOK_KW_pub;
            else if (str_eq_c(word, "lock")) kw = TOK_KW_lock;
            else if (str_eq_c(word, "seal")) kw = TOK_KW_seal;
            else if (str_eq_c(word, "def")) kw = TOK_KW_def;
            else if (str_eq_c(word, "let")) kw = TOK_KW_let;
            else if (str_eq_c(word, "const")) kw = TOK_KW_const;
            else if (str_eq_c(word, "if")) kw = TOK_KW_if;
            else if (str_eq_c(word, "else")) kw = TOK_KW_else;
            else if (str_eq_c(word, "elif")) kw = TOK_KW_elif;
            else if (str_eq_c(word, "return")) kw = TOK_KW_return;
            else if (str_eq_c(word, "true")) kw = TOK_KW_true;
            else if (str_eq_c(word, "false")) kw = TOK_KW_false;
            else if (str_eq_c(word, "null")) kw = TOK_KW_null;
            else if (str_eq_c(word, "for")) kw = TOK_KW_for;
            else if (str_eq_c(word, "match")) kw = TOK_KW_match;
            else if (str_eq_c(word, "new")) kw = TOK_KW_new;
            else if (str_eq_c(word, "in")) kw = TOK_KW_in;

            if (kw != TOK_INVALID) {
                if (!emit_simple(&lx, out, err, kw, word, start_line, start_col)) {
                    return false;
                }
                set_last(&lx, kw);
            } else {
                if (!emit_ident(&lx, out, err, word, start_line, start_col)) {
                    return false;
                }
                set_last(&lx, TOK_IDENT);
            }
            continue;
        }

        return set_error(&lx, err, lx.line, lx.col, "unexpected character");
    }

    if (lx.nest == 0 && is_stmt_end(lx.last_sig)) {
        if (!emit_simple(&lx, out, err, TOK_SEMI, str_from_c(";"), lx.line, lx.col)) {
            return false;
        }
        lx.last_real = TOK_SEMI;
    }

    if (out->len > 1) {
        size_t w = 0;
        for (size_t r = 0; r < out->len; r++) {
            if (out->data[r].kind == TOK_SEMI && w > 0 && out->data[w - 1].kind == TOK_SEMI) {
                continue;
            }
            out->data[w++] = out->data[r];
        }
        out->len = w;
    }

    return true;
}
