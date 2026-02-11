#ifndef ERGO_AST_H
#define ERGO_AST_H

#include <stdbool.h>
#include <stddef.h>

#include "arena.h"
#include "lexer.h"
#include "str.h"

// --- Types ---

typedef enum {
    TYPE_NAME,
    TYPE_ARRAY
} TypeKind;

typedef struct TypeRef TypeRef;

struct TypeRef {
    TypeKind kind;
    int line;
    int col;
    union {
        Str name;
        TypeRef *elem;
    } as;
};

typedef struct {
    bool is_void;
    TypeRef **types;
    size_t types_len;
} RetSpec;

typedef struct {
    Str name;
    TypeRef *typ;
    bool is_mut;
    bool is_this;
} Param;

typedef struct Stmt Stmt;

// --- Expressions ---

typedef enum {
    EXPR_INT,
    EXPR_FLOAT,
    EXPR_STR,
    EXPR_TUPLE,
    EXPR_IDENT,
    EXPR_NULL,
    EXPR_BOOL,
    EXPR_ARRAY,
    EXPR_UNARY,
    EXPR_BINARY,
    EXPR_ASSIGN,
    EXPR_CALL,
    EXPR_INDEX,
    EXPR_MEMBER,
    EXPR_PAREN,
    EXPR_MATCH,
    EXPR_LAMBDA,
    EXPR_BLOCK,
    EXPR_NEW,
    EXPR_TERNARY,
    EXPR_MOVE
} ExprKind;

typedef struct Expr Expr;

typedef struct {
    long long v;
} ExprInt;

typedef struct {
    double v;
} ExprFloat;

typedef struct {
    StrParts *parts;
} ExprStr;

typedef struct {
    Expr **items;
    size_t items_len;
} ExprTuple;

typedef struct {
    Str name;
} ExprIdent;

typedef struct {
    bool v;
} ExprBool;

typedef struct {
    Expr **items;
    size_t items_len;
} ExprArray;

typedef struct {
    TokKind op;
    Expr *x;
} ExprUnary;

typedef struct {
    TokKind op;
    Expr *a;
    Expr *b;
} ExprBinary;

typedef struct {
    TokKind op;
    Expr *target;
    Expr *value;
} ExprAssign;

typedef struct {
    Expr *fn;
    Expr **args;
    size_t args_len;
} ExprCall;

typedef struct {
    Expr *a;
    Expr *i;
} ExprIndex;

typedef struct {
    Expr *a;
    Str name;
} ExprMember;

typedef struct {
    Expr *x;
} ExprParen;

typedef struct Pat Pat;

typedef struct {
    Pat *pat;
    Expr *expr;
} MatchArm;

typedef struct {
    Expr *scrut;
    MatchArm **arms;
    size_t arms_len;
} ExprMatch;

typedef struct {
    Str name;
    char *cname;
    void *ty;
} Capture;

typedef struct {
    Param **params;
    size_t params_len;
    Expr *body;
    Capture **captures;
    size_t captures_len;
} ExprLambda;

typedef struct {
    Stmt *block;
} ExprBlock;

typedef struct {
    Str name;
    Expr **args;
    size_t args_len;
} ExprNew;

typedef struct {
    Expr *cond;
    Expr *then_expr;
    Expr *else_expr;
} ExprTernary;

typedef struct {
    Expr *x;
} ExprMove;

struct Expr {
    ExprKind kind;
    int line;
    int col;
    union {
        ExprInt int_lit;
        ExprFloat float_lit;
        ExprStr str_lit;
        ExprTuple tuple_lit;
        ExprIdent ident;
        ExprBool bool_lit;
        ExprArray array_lit;
        ExprUnary unary;
        ExprBinary binary;
        ExprAssign assign;
        ExprCall call;
        ExprIndex index;
        ExprMember member;
        ExprParen paren;
        ExprMatch match_expr;
        ExprLambda lambda;
        ExprBlock block_expr;
        ExprNew new_expr;
        ExprTernary ternary;
        ExprMove move;
    } as;
};

// --- Patterns ---

typedef enum {
    PAT_WILD,
    PAT_IDENT,
    PAT_INT,
    PAT_STR,
    PAT_BOOL,
    PAT_NULL
} PatKind;

struct Pat {
    PatKind kind;
    int line;
    int col;
    union {
        Str name;
        long long i;
        StrParts *str;
        bool b;
    } as;
};

// --- Statements ---

typedef enum {
    STMT_LET,
    STMT_CONST,
    STMT_IF,
    STMT_FOR,
    STMT_FOREACH,
    STMT_RETURN,
    STMT_EXPR,
    STMT_BLOCK
} StmtKind;

typedef struct {
    Str name;
    bool is_mut;
    Expr *expr;
} StmtLet;

typedef struct {
    Str name;
    Expr *expr;
} StmtConst;

typedef struct {
    Expr *cond;
    Stmt *body;
} IfArm;

typedef struct {
    IfArm **arms;
    size_t arms_len;
} StmtIf;

typedef struct {
    Stmt *init;
    Expr *cond;
    Expr *step;
    Stmt *body;
} StmtFor;

typedef struct {
    Str name;
    Expr *expr;
    Stmt *body;
} StmtForEach;

typedef struct {
    Expr *expr;
} StmtReturn;

typedef struct {
    Expr *expr;
} StmtExpr;

typedef struct {
    Stmt **stmts;
    size_t stmts_len;
} StmtBlock;

struct Stmt {
    StmtKind kind;
    int line;
    int col;
    union {
        StmtLet let_s;
        StmtConst const_s;
        StmtIf if_s;
        StmtFor for_s;
        StmtForEach foreach_s;
        StmtReturn ret_s;
        StmtExpr expr_s;
        StmtBlock block_s;
    } as;
};

// --- Declarations ---

typedef enum {
    DECL_FUN,
    DECL_ENTRY,
    DECL_CLASS,
    DECL_CONST,
    DECL_DEF
} DeclKind;

typedef struct Decl Decl;

typedef struct {
    Str name;
    Param **params;
    size_t params_len;
    RetSpec ret;
    Stmt *body;
} FunDecl;

typedef struct {
    RetSpec ret;
    Stmt *body;
} EntryDecl;

typedef struct {
    Str name;
    Expr *expr;
} ConstDecl;

typedef struct {
    Str name;
    Expr *expr;
    bool is_mut;
} DefDecl;

typedef struct {
    Str name;
    TypeRef *typ;
} FieldDecl;

typedef struct {
    Str name;
    Str vis;
    bool is_seal;
    FieldDecl **fields;
    size_t fields_len;
    FunDecl **methods;
    size_t methods_len;
} ClassDecl;

struct Decl {
    DeclKind kind;
    int line;
    int col;
    union {
        FunDecl fun;
        EntryDecl entry;
        ClassDecl class_decl;
        ConstDecl const_decl;
        DefDecl def_decl;
    } as;
};

// --- Program Structure ---

typedef struct {
    Str name;
} Import;

typedef struct {
    Str path;
    Import **imports;
    size_t imports_len;
    Decl **decls;
    size_t decls_len;
} Module;

typedef struct {
    Module **mods;
    size_t mods_len;
} Program;

static inline void *ast_alloc(Arena *arena, size_t size) {
    return arena_alloc_zero(arena, size);
}

#endif
