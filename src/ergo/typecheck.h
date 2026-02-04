#ifndef ERGO_TYPECHECK_H
#define ERGO_TYPECHECK_H

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
    Str module_path;
    Str module_name;
    Str *imports;
    size_t imports_len;
    Str current_class;
    bool has_current_class;
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
} ConstEntry;

typedef struct {
    Str module;
    ConstEntry *entries;
    size_t len;
} ModuleConsts;

typedef struct {
    Str name;
    Ty *ty;
    bool is_mut;
} GlobalVar;

typedef struct {
    Str module;
    GlobalVar *vars;
    size_t len;
} ModuleGlobals;

typedef struct {
    Str name;
    Ty *ty;
} FieldEntry;

typedef struct FunSig FunSig;

typedef struct {
    Str name;
    FunSig *sig;
} MethodEntry;

struct FunSig {
    Str name;
    Str module;
    Ty **params;
    size_t params_len;
    Str *param_names;
    size_t param_names_len;
    Ty *ret;
    bool is_method;
    bool recv_mut;
    Str owner_class;
    Str module_path;
};

typedef struct {
    Str name;
    Str module;
    Str qname;
    Str vis;
    bool is_seal;
    Str module_path;
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
    Str module;
    Str *imports;
    size_t imports_len;
} ModuleImport;

typedef struct {
    ClassInfo *classes;
    size_t classes_len;
    FunSig *funs;
    size_t funs_len;
    EntryDecl *entry;
    ModuleName *module_names;
    size_t module_names_len;
    ModuleImport *module_imports;
    size_t module_imports_len;
    ModuleConsts *module_consts;
    size_t module_consts_len;
    ModuleGlobals *module_globals;
    size_t module_globals_len;
    Arena *arena;
} GlobalEnv;

Program *lower_program(Program *prog, Arena *arena, Diag *err);
bool typecheck_program(Program *prog, Arena *arena, Diag *err);

GlobalEnv *build_global_env(Program *prog, Arena *arena, Diag *err);
Ty *tc_expr(Expr *e, GlobalEnv *env, Str module_path, Str module_name, Str *imports, size_t imports_len, Diag *err);
Ty *tc_expr_ctx(Expr *e, Ctx *ctx, Locals *loc, GlobalEnv *env, Diag *err);

void locals_init(Locals *loc);
void locals_free(Locals *loc);
void locals_push(Locals *loc);
void locals_pop(Locals *loc);
void locals_define(Locals *loc, Str name, Binding b);
Binding *locals_lookup(Locals *loc, Str name);

#endif
