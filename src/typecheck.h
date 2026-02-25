#ifndef YIS_TYPECHECK_H
#define YIS_TYPECHECK_H

#include <stdbool.h>
#include <stddef.h>

#include "arena.h"
#include "ast.h"
#include "diag.h"
#include "str.h"

typedef enum {
    TY_PRIM,
    TY_CLASS,
    TY_ARRAY,
    TY_TUPLE,
    TY_VOID,
    TY_NULL,
    TY_MOD,
    TY_FN,
    TY_NULLABLE,
    TY_GEN
} TyTag;

typedef struct Ty Ty;

struct Ty {
    TyTag tag;
    Str name;
    Ty *elem;
    Ty **items;
    size_t items_len;
    Ty **params;
    size_t params_len;
    Ty *ret;
};

typedef struct {
    Ty *ty;
    bool is_mut;
    bool is_const;
    bool is_moved;
} Binding;

typedef struct {
    Str name;
    Binding binding;
} LocalEntry;

typedef struct {
    LocalEntry *entries;
    size_t len;
    size_t cap;
} LocalScope;

typedef struct {
    LocalScope *scopes;
    size_t len;
    size_t cap;
} Locals;

typedef struct {
    Str cask_path;
    Str cask_name;
    Str *imports;
    size_t imports_len;
    Str current_class;
    bool has_current_class;
    int loop_depth;
} Ctx;

typedef struct {
    Ty *ty;
    bool is_float;
    long long i;
    double f;
    bool b;
    Str s;
} ConstVal;

typedef struct {
    Str name;
    ConstVal val;
    bool is_pub;
} ConstEntry;

typedef struct {
    Str cask;
    ConstEntry *entries;
    size_t len;
} ModuleConsts;

typedef struct {
    Str name;
    Ty *ty;
    bool is_mut;
    bool is_pub;
} GlobalVar;

typedef struct {
    Str cask;
    GlobalVar *vars;
    size_t len;
} ModuleGlobals;

typedef struct {
    Str name;
    Ty *ty;
    bool is_pub;
} FieldEntry;

typedef struct FunSig FunSig;

typedef struct {
    Str name;
    FunSig *sig;
    bool is_pub;
} MethodEntry;

struct FunSig {
    Str name;
    Str cask;
    Ty **params;
    size_t params_len;
    Str *param_names;
    size_t param_names_len;
    Ty *ret;
    bool is_method;
    bool recv_mut;
    Str owner_class;
    Str cask_path;
    bool extern_stub;
    bool is_pub;
};

typedef struct {
    Str name;
    Str cask;
    Str qname;
    Str vis;
    bool is_seal;
    Str base_qname;
    ClassKind kind;
    Str cask_path;
    FieldEntry *fields;
    size_t fields_len;
    MethodEntry *methods;
    size_t methods_len;
} ClassInfo;

typedef struct {
    Str path;
    Str name;
} ModuleName;

typedef struct {
    Str cask;
    Str *imports;
    size_t imports_len;
} ModuleImport;

typedef struct {
    ClassInfo *classes;
    size_t classes_len;
    FunSig *funs;
    size_t funs_len;
    EntryDecl *entry;
    ModuleName *cask_names;
    size_t cask_names_len;
    ModuleImport *cask_imports;
    size_t cask_imports_len;
    ModuleConsts *cask_consts;
    size_t cask_consts_len;
    ModuleGlobals *cask_globals;
    size_t cask_globals_len;
    Arena *arena;
} GlobalEnv;

typedef enum {
    YIS_LINT_WARN = 0,
    YIS_LINT_STRICT = 1
} YisLintMode;

Program *lower_program(Program *prog, Arena *arena, Diag *err);
bool typecheck_program(Program *prog, Arena *arena, Diag *err);
bool lint_program(Program *prog, Arena *arena, YisLintMode mode, int *warning_count, int *error_count);

GlobalEnv *build_global_env(Program *prog, Arena *arena, Diag *err);
Ty *tc_expr(Expr *e, GlobalEnv *env, Str cask_path, Str cask_name, Str *imports, size_t imports_len, Diag *err);
Ty *tc_expr_ctx(Expr *e, Ctx *ctx, Locals *loc, GlobalEnv *env, Diag *err);

void locals_init(Locals *loc);
void locals_free(Locals *loc);
void locals_push(Locals *loc);
void locals_pop(Locals *loc);
void locals_define(Locals *loc, Str name, Binding b);
Binding *locals_lookup(Locals *loc, Str name);

#endif
