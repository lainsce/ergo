#include "codegen.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "file.h"
#include "str.h"
#include "typecheck.h"
#include "vec.h"

// -----------------
// Small string buf
// -----------------

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} StrBuf;

static void sb_init(StrBuf *b) {
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

static void sb_free(StrBuf *b) {
    free(b->data);
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

static bool sb_reserve(StrBuf *b, size_t add) {
    size_t need = b->len + add + 1;
    if (need <= b->cap) {
        return true;
    }
    size_t next = b->cap ? b->cap : 256;
    while (next < need) {
        next *= 2;
    }
    char *p = (char *)realloc(b->data, next);
    if (!p) {
        return false;
    }
    b->data = p;
    b->cap = next;
    return true;
}

static bool sb_append_n(StrBuf *b, const char *s, size_t n) {
    if (!s || n == 0) {
        return true;
    }
    if (!sb_reserve(b, n)) {
        return false;
    }
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
    return true;
}

static bool sb_append(StrBuf *b, const char *s) {
    if (!s) {
        return true;
    }
    return sb_append_n(b, s, strlen(s));
}

static bool sb_append_char(StrBuf *b, char c) {
    if (!sb_reserve(b, 1)) {
        return false;
    }
    b->data[b->len++] = c;
    b->data[b->len] = '\0';
    return true;
}

static bool sb_appendf(StrBuf *b, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    int needed = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (needed < 0) {
        va_end(ap2);
        return false;
    }
    if (!sb_reserve(b, (size_t)needed)) {
        va_end(ap2);
        return false;
    }
    vsnprintf(b->data + b->len, (size_t)needed + 1, fmt, ap2);
    va_end(ap2);
    b->len += (size_t)needed;
    b->data[b->len] = '\0';
    return true;
}

// -----------------
// Writer helpers
// -----------------

typedef struct {
    StrBuf *buf;
    int indent;
} Writer;

static bool w_line(Writer *w, const char *fmt, ...) {
    if (!w || !w->buf) {
        return false;
    }
    for (int i = 0; i < w->indent; i++) {
        if (!sb_append(w->buf, "  ")) {
            return false;
        }
    }
    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    int needed = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (needed < 0) {
        va_end(ap2);
        return false;
    }
    if (!sb_reserve(w->buf, (size_t)needed + 1)) {
        va_end(ap2);
        return false;
    }
    vsnprintf(w->buf->data + w->buf->len, (size_t)needed + 1, fmt, ap2);
    va_end(ap2);
    w->buf->len += (size_t)needed;
    w->buf->data[w->buf->len] = '\0';
    if (!sb_append_char(w->buf, '\n')) {
        return false;
    }
    return true;
}

static bool w_raw(Writer *w, const char *s) {
    if (!w || !w->buf) {
        return false;
    }
    return sb_append(w->buf, s);
}

// -----------------
// Error handling
// -----------------

static bool cg_set_errf(Diag *err, Str path, int line, int col, const char *fmt, ...) {
    if (!err) {
        return false;
    }
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    char *msg = (char *)malloc(strlen(buf) + 1);
    if (!msg) {
        err->message = "codegen error";
    } else {
        strcpy(msg, buf);
        err->message = msg;
    }
    err->path = path.data;
    err->line = line;
    err->col = col;
    return false;
}

static bool cg_set_err(Diag *err, Str path, const char *msg) {
    if (!err) {
        return false;
    }
    err->path = path.data;
    err->line = 0;
    err->col = 0;
    err->message = msg;
    return false;
}

// -----------------
// String helpers
// -----------------

static char *arena_strndup(Arena *arena, const char *s, size_t len) {
    if (!arena) return NULL;
    char *buf = (char *)arena_alloc(arena, len + 1);
    if (!buf) return NULL;
    memcpy(buf, s, len);
    buf[len] = '\0';
    return buf;
}

static char *dup_cstr(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len + 1);
    return out;
}

static char *arena_printf(Arena *arena, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    int needed = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (needed < 0) {
        va_end(ap2);
        return NULL;
    }
    char *buf = (char *)arena_alloc(arena, (size_t)needed + 1);
    if (!buf) {
        va_end(ap2);
        return NULL;
    }
    vsnprintf(buf, (size_t)needed + 1, fmt, ap2);
    va_end(ap2);
    return buf;
}

static char *c_escape(Arena *arena, Str s) {
    StrBuf b;
    sb_init(&b);
    for (size_t i = 0; i < s.len; i++) {
        unsigned char c = (unsigned char)s.data[i];
        switch (c) {
            case '\\': sb_append(&b, "\\\\"); break;
            case '"': sb_append(&b, "\\\""); break;
            case '\n': sb_append(&b, "\\n"); break;
            case '\t': sb_append(&b, "\\t"); break;
            case '\r': sb_append(&b, "\\r"); break;
            default:
                sb_append_char(&b, (char)c);
                break;
        }
    }
    char *out = arena_strndup(arena, b.data ? b.data : "", b.len);
    sb_free(&b);
    return out;
}

static char *mangle_mod(Arena *arena, Str name) {
    if (!arena) return NULL;
    char *buf = (char *)arena_alloc(arena, name.len + 1);
    if (!buf) return NULL;
    for (size_t i = 0; i < name.len; i++) {
        unsigned char c = (unsigned char)name.data[i];
        if (isalnum(c) || c == '_') {
            buf[i] = (char)c;
        } else {
            buf[i] = '_';
        }
    }
    buf[name.len] = '\0';
    return buf;
}

static char *mangle_global(Arena *arena, Str mod, Str name) {
    char *mm = mangle_mod(arena, mod);
    if (!mm) return NULL;
    return arena_printf(arena, "ergo_%s_%.*s", mm, (int)name.len, name.data);
}

static char *mangle_global_var(Arena *arena, Str mod, Str name) {
    char *mm = mangle_mod(arena, mod);
    if (!mm) return NULL;
    return arena_printf(arena, "ergo_g_%s_%.*s", mm, (int)name.len, name.data);
}

static char *mangle_global_init(Arena *arena, Str mod) {
    char *mm = mangle_mod(arena, mod);
    if (!mm) return NULL;
    return arena_printf(arena, "ergo_init_%s", mm);
}

static char *mangle_method(Arena *arena, Str mod, Str cls, Str name) {
    char *mm = mangle_mod(arena, mod);
    if (!mm) return NULL;
    return arena_printf(arena, "ergo_m_%s_%.*s_%.*s", mm, (int)cls.len, cls.data, (int)name.len, name.data);
}

static void split_qname(Str qname, Str *mod, Str *name) {
    size_t dot = 0;
    while (dot < qname.len && qname.data[dot] != '.') {
        dot++;
    }
    if (dot >= qname.len) {
        if (mod) { mod->data = ""; mod->len = 0; }
        if (name) { *name = qname; }
        return;
    }
    if (mod) {
        mod->data = qname.data;
        mod->len = dot;
    }
    if (name) {
        name->data = qname.data + dot + 1;
        name->len = qname.len - dot - 1;
    }
}

// -----------------
// Codegen structs
// -----------------

typedef struct {
    Str name;
    char *cname;
} NameBinding;

typedef struct {
    NameBinding *items;
    size_t len;
    size_t cap;
} NameScope;

typedef struct {
    char **items;
    size_t len;
    size_t cap;
} LocalList;

typedef struct {
    Expr *lam;
    Str path;
    char *name;
} LambdaInfo;

typedef struct {
    Str module;
    Str name;
    char *wrapper;
} FunValInfo;

typedef struct {
    Str qname;
    ClassDecl *decl;
} ClassDeclEntry;

typedef struct {
    Program *prog;
    GlobalEnv *env;
    Arena *arena;
    StrBuf out;
    Writer w;

    int tmp_id;
    int var_id;
    int arr_id;
    int sym_id;
    int lambda_id;

    NameScope *scopes;
    size_t scopes_len;
    size_t scopes_cap;

    LocalList *scope_locals;
    size_t scope_locals_len;
    size_t scope_locals_cap;

    Locals ty_loc;

    Str current_module;
    Str *current_imports;
    size_t current_imports_len;
    Str current_class;
    bool has_current_class;

    LambdaInfo *lambdas;
    size_t lambdas_len;
    size_t lambdas_cap;

    FunValInfo *funvals;
    size_t funvals_len;
    size_t funvals_cap;

    ClassDeclEntry *class_decls;
    size_t class_decls_len;
    size_t class_decls_cap;
} Codegen;

static char *codegen_c_class_name(Codegen *cg, Str qname) {
    Str mod, name;
    split_qname(qname, &mod, &name);
    if (mod.len == 0) {
        return arena_printf(cg->arena, "ErgoObj_%.*s", (int)name.len, name.data);
    }
    return arena_printf(cg->arena, "ErgoObj_%s_%.*s", mangle_mod(cg->arena, mod), (int)name.len, name.data);
}

static char *codegen_c_field_name(Codegen *cg, Str name) {
    return arena_printf(cg->arena, "f_%.*s", (int)name.len, name.data);
}

// -----------------
// Scope helpers
// -----------------

static bool scope_reserve_names(NameScope *scope, size_t need) {
    if (scope->cap >= need) return true;
    size_t next = scope->cap ? scope->cap * 2 : 8;
    while (next < need) next *= 2;
    NameBinding *items = (NameBinding *)realloc(scope->items, next * sizeof(NameBinding));
    if (!items) return false;
    scope->items = items;
    scope->cap = next;
    return true;
}

static bool scope_reserve_locals(LocalList *list, size_t need) {
    if (list->cap >= need) return true;
    size_t next = list->cap ? list->cap * 2 : 8;
    while (next < need) next *= 2;
    char **items = (char **)realloc(list->items, next * sizeof(char *));
    if (!items) return false;
    list->items = items;
    list->cap = next;
    return true;
}

static bool codegen_push_scope(Codegen *cg) {
    if (cg->scopes_len + 1 > cg->scopes_cap) {
        size_t next = cg->scopes_cap ? cg->scopes_cap * 2 : 8;
        NameScope *sc = (NameScope *)realloc(cg->scopes, next * sizeof(NameScope));
        if (!sc) return false;
        cg->scopes = sc;
        cg->scopes_cap = next;
    }
    if (cg->scope_locals_len + 1 > cg->scope_locals_cap) {
        size_t next = cg->scope_locals_cap ? cg->scope_locals_cap * 2 : 8;
        LocalList *ls = (LocalList *)realloc(cg->scope_locals, next * sizeof(LocalList));
        if (!ls) return false;
        cg->scope_locals = ls;
        cg->scope_locals_cap = next;
    }
    NameScope *ns = &cg->scopes[cg->scopes_len++];
    ns->items = NULL;
    ns->len = 0;
    ns->cap = 0;
    LocalList *ll = &cg->scope_locals[cg->scope_locals_len++];
    ll->items = NULL;
    ll->len = 0;
    ll->cap = 0;
    locals_push(&cg->ty_loc);
    return true;
}

static LocalList codegen_pop_scope(Codegen *cg) {
    LocalList out = {NULL, 0, 0};
    if (cg->scopes_len == 0 || cg->scope_locals_len == 0) {
        return out;
    }
    NameScope *ns = &cg->scopes[cg->scopes_len - 1];
    free(ns->items);
    cg->scopes_len--;
    out = cg->scope_locals[cg->scope_locals_len - 1];
    cg->scope_locals_len--;
    locals_pop(&cg->ty_loc);
    return out;
}

static bool codegen_add_name(Codegen *cg, Str name, char *cname) {
    if (cg->scopes_len == 0) return false;
    NameScope *ns = &cg->scopes[cg->scopes_len - 1];
    if (!scope_reserve_names(ns, ns->len + 1)) return false;
    ns->items[ns->len].name = name;
    ns->items[ns->len].cname = cname;
    ns->len++;
    return true;
}

static bool codegen_add_local(Codegen *cg, char *cname) {
    if (cg->scope_locals_len == 0) return false;
    LocalList *ll = &cg->scope_locals[cg->scope_locals_len - 1];
    if (!scope_reserve_locals(ll, ll->len + 1)) return false;
    ll->items[ll->len++] = cname;
    return true;
}

static ModuleGlobals *codegen_module_globals(Codegen *cg, Str module);
static GlobalVar *codegen_find_global(ModuleGlobals *mg, Str name);
static bool gen_stmt(Codegen *cg, Str path, Stmt *s, bool ret_void, Diag *err);

static char *codegen_cname_of(Codegen *cg, Str name) {
    for (size_t i = cg->scopes_len; i-- > 0;) {
        NameScope *ns = &cg->scopes[i];
        for (size_t j = 0; j < ns->len; j++) {
            if (str_eq(ns->items[j].name, name)) {
                return ns->items[j].cname;
            }
        }
    }
    if (cg->current_module.len) {
        ModuleGlobals *mg = codegen_module_globals(cg, cg->current_module);
        if (codegen_find_global(mg, name)) {
            return mangle_global_var(cg->arena, cg->current_module, name);
        }
    }
    return NULL;
}

static char *codegen_new_tmp(Codegen *cg) {
    cg->tmp_id++;
    return arena_printf(cg->arena, "__t%d", cg->tmp_id);
}

static char *codegen_new_sym(Codegen *cg, const char *base) {
    cg->sym_id++;
    return arena_printf(cg->arena, "__%s%d", base, cg->sym_id);
}

static char *codegen_new_lambda(Codegen *cg) {
    cg->lambda_id++;
    return arena_printf(cg->arena, "ergo_lambda_%d", cg->lambda_id);
}

static char *codegen_define_local(Codegen *cg, Str name, Ty *ty, bool is_mut, bool is_const) {
    cg->var_id++;
    size_t n_digits = 1;
    int tmp = cg->var_id;
    while (tmp >= 10) {
        n_digits++;
        tmp /= 10;
    }
    size_t total = name.len + 2 + n_digits;
    char *buf = (char *)arena_alloc(cg->arena, total + 1);
    if (!buf) return NULL;
    memcpy(buf, name.data, name.len);
    buf[name.len] = '_';
    buf[name.len + 1] = '_';
    snprintf(buf + name.len + 2, n_digits + 1, "%d", cg->var_id);

    Binding b = { ty, is_mut, is_const };
    locals_define(&cg->ty_loc, name, b);
    if (!codegen_add_name(cg, name, buf)) return NULL;
    if (!codegen_add_local(cg, buf)) return NULL;
    return buf;
}

static bool codegen_bind_temp(Codegen *cg, Str name, char *cname, Ty *ty) {
    Binding b = { ty, false, false };
    locals_define(&cg->ty_loc, name, b);
    return codegen_add_name(cg, name, cname);
}

static void codegen_release_scope(Codegen *cg, LocalList locals) {
    if (!cg) return;
    for (size_t i = locals.len; i-- > 0;) {
        w_line(&cg->w, "ergo_release_val(%s);", locals.items[i]);
    }
    free(locals.items);
}

// -----------------
// Env helpers
// -----------------

static Str module_name_for_path(Arena *arena, Str path) {
    const char *p = path.data;
    size_t len = path.len;
    size_t start = 0;
    for (size_t i = 0; i < len; i++) {
        char c = p[i];
        if (c == '/' || c == '\\') {
            start = i + 1;
        }
    }
    size_t name_len = len - start;
    if (name_len >= 2 && p[start + name_len - 2] == '.' && p[start + name_len - 1] == 'e') {
        name_len -= 2;
    }
    char *buf = arena_strndup(arena, p + start, name_len);
    Str out = { buf ? buf : "", name_len };
    
    return out;
}

static Str codegen_module_name(Codegen *cg, Str path) {
    for (size_t i = 0; i < cg->env->module_names_len; i++) {
        if (str_eq(cg->env->module_names[i].path, path)) {
            return cg->env->module_names[i].name;
        }
    }
    return module_name_for_path(cg->arena, path);
}

static ModuleImport *codegen_module_imports(Codegen *cg, Str module_name) {
    for (size_t i = 0; i < cg->env->module_imports_len; i++) {
        if (str_eq(cg->env->module_imports[i].module, module_name)) {
            return &cg->env->module_imports[i];
        }
    }
    return NULL;
}

static ClassDecl *codegen_class_decl(Codegen *cg, Str qname) {
    for (size_t i = 0; i < cg->class_decls_len; i++) {
        if (str_eq(cg->class_decls[i].qname, qname)) {
            return cg->class_decls[i].decl;
        }
    }
    return NULL;
}

static ClassInfo *codegen_class_info(Codegen *cg, Str qname) {
    for (size_t i = 0; i < cg->env->classes_len; i++) {
        if (str_eq(cg->env->classes[i].qname, qname)) {
            return &cg->env->classes[i];
        }
    }
    return NULL;
}

static FunSig *codegen_fun_sig(Codegen *cg, Str module, Str name) {
    for (size_t i = 0; i < cg->env->funs_len; i++) {
        if (str_eq(cg->env->funs[i].module, module) && str_eq(cg->env->funs[i].name, name)) {
            return &cg->env->funs[i];
        }
    }
    return NULL;
}

static ModuleConsts *codegen_module_consts(Codegen *cg, Str module) {
    for (size_t i = 0; i < cg->env->module_consts_len; i++) {
        if (str_eq(cg->env->module_consts[i].module, module)) {
            return &cg->env->module_consts[i];
        }
    }
    return NULL;
}

static ModuleGlobals *codegen_module_globals(Codegen *cg, Str module) {
    for (size_t i = 0; i < cg->env->module_globals_len; i++) {
        if (str_eq(cg->env->module_globals[i].module, module)) {
            return &cg->env->module_globals[i];
        }
    }
    return NULL;
}

static GlobalVar *codegen_find_global(ModuleGlobals *mg, Str name) {
    if (!mg) return NULL;
    for (size_t i = 0; i < mg->len; i++) {
        if (str_eq(mg->vars[i].name, name)) {
            return &mg->vars[i];
        }
    }
    return NULL;
}

static ConstEntry *codegen_find_const(ModuleConsts *mc, Str name) {
    if (!mc) return NULL;
    for (size_t i = 0; i < mc->len; i++) {
        if (str_eq(mc->entries[i].name, name)) {
            return &mc->entries[i];
        }
    }
    return NULL;
}

static bool is_stdr_prelude(Str name) {
    return str_eq_c(name, "write") || str_eq_c(name, "writef") || str_eq_c(name, "readf") ||
           str_eq_c(name, "len") || str_eq_c(name, "is_null") || str_eq_c(name, "str");
}

static Ctx codegen_ctx_for(Codegen *cg, Str path) {
    Ctx ctx;
    ctx.module_path = path;
    ctx.module_name = cg->current_module.len ? cg->current_module : codegen_module_name(cg, path);
    ModuleImport *mi = codegen_module_imports(cg, ctx.module_name);
    ctx.imports = mi ? mi->imports : NULL;
    ctx.imports_len = mi ? mi->imports_len : 0;
    ctx.has_current_class = cg->has_current_class;
    ctx.current_class = cg->current_class;
    return ctx;
}

static bool codegen_module_in_scope(Codegen *cg, Str name) {
    if (locals_lookup(&cg->ty_loc, name)) return false;
    if (str_eq(name, cg->current_module)) return true;
    for (size_t i = 0; i < cg->current_imports_len; i++) {
        if (str_eq(cg->current_imports[i], name)) return true;
    }
    return false;
}

// -----------------
// Type helpers (for codegen)
// -----------------

static Ty *cg_ty_new(Codegen *cg, TyTag tag) {
    Ty *t = (Ty *)arena_alloc(cg->arena, sizeof(Ty));
    if (!t) return NULL;
    memset(t, 0, sizeof(Ty));
    t->tag = tag;
    return t;
}

static Ty *cg_ty_prim(Codegen *cg, const char *name) {
    Ty *t = cg_ty_new(cg, TY_PRIM);
    if (!t) return NULL;
    t->name = str_from_c(name);
    return t;
}

static Ty *cg_ty_class(Codegen *cg, Str name) {
    Ty *t = cg_ty_new(cg, TY_CLASS);
    if (!t) return NULL;
    t->name = name;
    return t;
}

static Ty *cg_ty_array(Codegen *cg, Ty *elem) {
    Ty *t = cg_ty_new(cg, TY_ARRAY);
    if (!t) return NULL;
    t->elem = elem;
    return t;
}

static Ty *cg_ty_void(Codegen *cg) {
    return cg_ty_new(cg, TY_VOID);
}

static Ty *cg_ty_gen(Codegen *cg, Str name) {
    Ty *t = cg_ty_new(cg, TY_GEN);
    if (!t) return NULL;
    t->name = name;
    return t;
}

static bool str_is_ident_like(Str s) {
    if (s.len == 0) return false;
    for (size_t i = 0; i < s.len; i++) {
        char c = s.data[i];
        if (!(c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) {
            return false;
        }
    }
    return true;
}

static Ty *cg_ty_from_type_ref(Codegen *cg, TypeRef *tref, Str ctx_mod, Str *imports, size_t imports_len, Diag *err) {
    if (!tref) return NULL;
    if (tref->kind == TYPE_ARRAY) {
        Ty *elem = cg_ty_from_type_ref(cg, tref->as.elem, ctx_mod, imports, imports_len, err);
        return cg_ty_array(cg, elem);
    }
    Str n = tref->as.name;
    if (str_eq_c(n, "str")) n = str_from_c("string");
    if (str_eq_c(n, "bool") || str_eq_c(n, "string") || str_eq_c(n, "num") || str_eq_c(n, "any")) {
        return cg_ty_prim(cg, n.data);
    }
    if (str_eq_c(n, "void")) {
        return cg_ty_void(cg);
    }
    if (memchr(n.data, '.', n.len)) {
        size_t dot = 0;
        while (dot < n.len && n.data[dot] != '.') dot++;
        Str mod = { n.data, dot };
        bool in_scope = str_eq(mod, ctx_mod);
        for (size_t i = 0; i < imports_len && !in_scope; i++) {
            if (str_eq(imports[i], mod)) in_scope = true;
        }
        if (!in_scope) {
            cg_set_errf(err, ctx_mod, tref->line, tref->col, "unknown type '%.*s'", (int)n.len, n.data);
            return NULL;
        }
        if (codegen_class_info(cg, n)) {
            return cg_ty_class(cg, n);
        }
        cg_set_errf(err, ctx_mod, tref->line, tref->col, "unknown type '%.*s'", (int)n.len, n.data);
        return NULL;
    }
    // unqualified class name
    Str qname;
    size_t len = ctx_mod.len + 1 + n.len;
    char *buf = (char *)arena_alloc(cg->arena, len + 1);
    if (!buf) return NULL;
    memcpy(buf, ctx_mod.data, ctx_mod.len);
    buf[ctx_mod.len] = '.';
    memcpy(buf + ctx_mod.len + 1, n.data, n.len);
    buf[len] = '\0';
    qname.data = buf;
    qname.len = len;
    if (codegen_class_info(cg, qname)) {
        return cg_ty_class(cg, qname);
    }
    if (str_is_ident_like(n)) {
        return cg_ty_gen(cg, n);
    }
    cg_set_errf(err, ctx_mod, tref->line, tref->col, "unknown type '%.*s'", (int)n.len, n.data);
    return NULL;
}

// -----------------
// Lambda collection
// -----------------

static LambdaInfo *codegen_lambda_info(Codegen *cg, Expr *lam) {
    for (size_t i = 0; i < cg->lambdas_len; i++) {
        if (cg->lambdas[i].lam == lam) {
            return &cg->lambdas[i];
        }
    }
    return NULL;
}

static bool codegen_add_lambda(Codegen *cg, Expr *lam, Str path) {
    if (cg->lambdas_len + 1 > cg->lambdas_cap) {
        size_t next = cg->lambdas_cap ? cg->lambdas_cap * 2 : 8;
        LambdaInfo *arr = (LambdaInfo *)realloc(cg->lambdas, next * sizeof(LambdaInfo));
        if (!arr) return false;
        cg->lambdas = arr;
        cg->lambdas_cap = next;
    }
    LambdaInfo *li = &cg->lambdas[cg->lambdas_len++];
    li->lam = lam;
    li->path = path;
    li->name = codegen_new_lambda(cg);
    return li->name != NULL;
}

static FunValInfo *codegen_funval_info(Codegen *cg, Str module, Str name) {
    for (size_t i = 0; i < cg->funvals_len; i++) {
        if (str_eq(cg->funvals[i].module, module) && str_eq(cg->funvals[i].name, name)) {
            return &cg->funvals[i];
        }
    }
    return NULL;
}

static bool codegen_add_funval(Codegen *cg, Str module, Str name) {
    if (codegen_funval_info(cg, module, name)) return true;
    if (cg->funvals_len + 1 > cg->funvals_cap) {
        size_t next = cg->funvals_cap ? cg->funvals_cap * 2 : 8;
        FunValInfo *arr = (FunValInfo *)realloc(cg->funvals, next * sizeof(FunValInfo));
        if (!arr) return false;
        cg->funvals = arr;
        cg->funvals_cap = next;
    }
    FunValInfo *fi = &cg->funvals[cg->funvals_len++];
    fi->module = module;
    fi->name = name;
    fi->wrapper = arena_printf(cg->arena, "__fnwrap_%s_%.*s", mangle_mod(cg->arena, module), (int)name.len, name.data);
    return fi->wrapper != NULL;
}

static void collect_expr(Codegen *cg, Expr *e, Str path, bool allow_funval);
static void collect_stmt(Codegen *cg, Stmt *s, Str path);

static void collect_expr(Codegen *cg, Expr *e, Str path, bool allow_funval) {
    if (!e) return;
    if (e->kind == EXPR_LAMBDA) {
        if (!codegen_lambda_info(cg, e)) {
            codegen_add_lambda(cg, e, path);
        }
        collect_expr(cg, e->as.lambda.body, path, true);
        return;
    }
    switch (e->kind) {
        case EXPR_IDENT:
            if (allow_funval) {
                Str mod = codegen_module_name(cg, path);
                FunSig *sig = codegen_fun_sig(cg, mod, e->as.ident.name);
                if (sig) {
                    codegen_add_funval(cg, sig->module, sig->name);
                } else if (is_stdr_prelude(e->as.ident.name)) {
                    codegen_add_funval(cg, str_from_c("stdr"), e->as.ident.name);
                }
            }
            break;
        case EXPR_UNARY:
            collect_expr(cg, e->as.unary.x, path, true);
            break;
        case EXPR_BINARY:
            collect_expr(cg, e->as.binary.a, path, true);
            collect_expr(cg, e->as.binary.b, path, true);
            break;
        case EXPR_ASSIGN:
            collect_expr(cg, e->as.assign.target, path, true);
            collect_expr(cg, e->as.assign.value, path, true);
            break;
        case EXPR_CALL:
            collect_expr(cg, e->as.call.fn, path, false);
            for (size_t i = 0; i < e->as.call.args_len; i++) {
                collect_expr(cg, e->as.call.args[i], path, true);
            }
            break;
        case EXPR_INDEX:
            collect_expr(cg, e->as.index.a, path, true);
            collect_expr(cg, e->as.index.i, path, true);
            break;
        case EXPR_MEMBER:
            collect_expr(cg, e->as.member.a, path, true);
            break;
        case EXPR_PAREN:
            collect_expr(cg, e->as.paren.x, path, true);
            break;
        case EXPR_TERNARY:
            collect_expr(cg, e->as.ternary.cond, path, true);
            collect_expr(cg, e->as.ternary.then_expr, path, true);
            collect_expr(cg, e->as.ternary.else_expr, path, true);
            break;
        case EXPR_MOVE:
            collect_expr(cg, e->as.move.x, path, true);
            break;
        case EXPR_ARRAY:
            for (size_t i = 0; i < e->as.array_lit.items_len; i++) {
                collect_expr(cg, e->as.array_lit.items[i], path, true);
            }
            break;
        case EXPR_TUPLE:
            for (size_t i = 0; i < e->as.tuple_lit.items_len; i++) {
                collect_expr(cg, e->as.tuple_lit.items[i], path, true);
            }
            break;
        case EXPR_MATCH:
            collect_expr(cg, e->as.match_expr.scrut, path, true);
            for (size_t i = 0; i < e->as.match_expr.arms_len; i++) {
                collect_expr(cg, e->as.match_expr.arms[i]->expr, path, true);
            }
            break;
        case EXPR_BLOCK:
            collect_stmt(cg, e->as.block_expr.block, path);
            break;
        case EXPR_NEW:
            for (size_t i = 0; i < e->as.new_expr.args_len; i++) {
                collect_expr(cg, e->as.new_expr.args[i], path, true);
            }
            break;
        default:
            break;
    }
}

static void collect_stmt(Codegen *cg, Stmt *s, Str path) {
    if (!s) return;
    switch (s->kind) {
        case STMT_LET:
            collect_expr(cg, s->as.let_s.expr, path, true);
            break;
        case STMT_CONST:
            collect_expr(cg, s->as.const_s.expr, path, true);
            break;
        case STMT_EXPR:
            collect_expr(cg, s->as.expr_s.expr, path, true);
            break;
        case STMT_RETURN:
            if (s->as.ret_s.expr) collect_expr(cg, s->as.ret_s.expr, path, true);
            break;
        case STMT_IF:
            for (size_t i = 0; i < s->as.if_s.arms_len; i++) {
                IfArm *arm = s->as.if_s.arms[i];
                if (arm->cond) collect_expr(cg, arm->cond, path, true);
                collect_stmt(cg, arm->body, path);
            }
            break;
        case STMT_FOR:
            if (s->as.for_s.init) collect_stmt(cg, s->as.for_s.init, path);
            if (s->as.for_s.cond) collect_expr(cg, s->as.for_s.cond, path, true);
            if (s->as.for_s.step) collect_expr(cg, s->as.for_s.step, path, true);
            collect_stmt(cg, s->as.for_s.body, path);
            break;
        case STMT_FOREACH:
            collect_expr(cg, s->as.foreach_s.expr, path, true);
            collect_stmt(cg, s->as.foreach_s.body, path);
            break;
        case STMT_BLOCK:
            for (size_t i = 0; i < s->as.block_s.stmts_len; i++) {
                collect_stmt(cg, s->as.block_s.stmts[i], path);
            }
            break;
        default:
            break;
    }
}

static void codegen_collect_lambdas(Codegen *cg) {
    for (size_t i = 0; i < cg->prog->mods_len; i++) {
        Module *m = cg->prog->mods[i];
        for (size_t j = 0; j < m->decls_len; j++) {
            Decl *d = m->decls[j];
            if (d->kind == DECL_FUN) {
                collect_stmt(cg, d->as.fun.body, m->path);
            } else if (d->kind == DECL_CLASS) {
                for (size_t k = 0; k < d->as.class_decl.methods_len; k++) {
                    collect_stmt(cg, d->as.class_decl.methods[k]->body, m->path);
                }
            } else if (d->kind == DECL_ENTRY) {
                collect_stmt(cg, d->as.entry.body, m->path);
            } else if (d->kind == DECL_DEF) {
                collect_expr(cg, d->as.def_decl.expr, m->path, true);
            }
        }
    }
}

// -----------------
// Expr generation
// -----------------

typedef struct {
    char *tmp;
    VEC(char *) cleanup;
} GenExpr;

static void gen_expr_init(GenExpr *ge) {
    ge->tmp = NULL;
    ge->cleanup.data = NULL;
    ge->cleanup.len = 0;
    ge->cleanup.cap = 0;
}

static void gen_expr_free(GenExpr *ge) {
    VEC_FREE(ge->cleanup);
    ge->tmp = NULL;
}

static void gen_expr_add(GenExpr *ge, char *tmp) {
    VEC_PUSH(ge->cleanup, tmp);
}

static void gen_expr_release_except(Codegen *cg, GenExpr *ge, char *keep) {
    for (size_t i = 0; i < ge->cleanup.len; i++) {
        char *v = ge->cleanup.data[i];
        if (v != keep) {
            w_line(&cg->w, "ergo_release_val(%s);", v);
        }
    }
}

static Ty *cg_tc_expr(Codegen *cg, Str path, Expr *e, Diag *err) {
    Ctx ctx = codegen_ctx_for(cg, path);
    Ty *t = tc_expr_ctx(e, &ctx, &cg->ty_loc, cg->env, err);
    return t;
}

static bool gen_expr(Codegen *cg, Str path, Expr *e, GenExpr *out, Diag *err);

static bool gen_expr(Codegen *cg, Str path, Expr *e, GenExpr *out, Diag *err) {
    gen_expr_init(out);
    if (!e) {
        return cg_set_err(err, path, "codegen: null expr");
    }

    switch (e->kind) {
        case EXPR_INT: {
            char *t = codegen_new_tmp(cg);
            if (!t) return cg_set_err(err, path, "out of memory");
            w_line(&cg->w, "ErgoVal %s = EV_INT(%lld);", t, e->as.int_lit.v);
            gen_expr_add(out, t);
            out->tmp = t;
            return true;
        }
        case EXPR_FLOAT: {
            char *t = codegen_new_tmp(cg);
            if (!t) return cg_set_err(err, path, "out of memory");
            w_line(&cg->w, "ErgoVal %s = EV_FLOAT(%.17g);", t, e->as.float_lit.v);
            gen_expr_add(out, t);
            out->tmp = t;
            return true;
        }
        case EXPR_BOOL: {
            char *t = codegen_new_tmp(cg);
            if (!t) return cg_set_err(err, path, "out of memory");
            w_line(&cg->w, "ErgoVal %s = EV_BOOL(%s);", t, e->as.bool_lit.v ? "true" : "false");
            gen_expr_add(out, t);
            out->tmp = t;
            return true;
        }
        case EXPR_NULL: {
            char *t = codegen_new_tmp(cg);
            if (!t) return cg_set_err(err, path, "out of memory");
            w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
            gen_expr_add(out, t);
            out->tmp = t;
            return true;
        }
        case EXPR_STR: {
            StrParts *parts = e->as.str_lit.parts;
            if (!parts || parts->len == 0) {
                char *t = codegen_new_tmp(cg);
                if (!t) return cg_set_err(err, path, "out of memory");
                w_line(&cg->w, "ErgoVal %s = EV_STR(stdr_str_lit(\"\"));", t);
                gen_expr_add(out, t);
                out->tmp = t;
                return true;
            }
            char **part_tmps = (char **)malloc(parts->len * sizeof(char *));
            if (!part_tmps) return cg_set_err(err, path, "out of memory");
            for (size_t i = 0; i < parts->len; i++) {
                StrPart *p = &parts->parts[i];
                char *pt = codegen_new_tmp(cg);
                if (!pt) { free(part_tmps); return cg_set_err(err, path, "out of memory"); }
                if (p->kind == STR_PART_TEXT) {
                    char *esc = c_escape(cg->arena, p->text);
                    w_line(&cg->w, "ErgoVal %s = EV_STR(stdr_str_lit(\"%s\"));", pt, esc ? esc : "");
                } else {
                    char *cname = codegen_cname_of(cg, p->text);
                    if (!cname) { free(part_tmps); return cg_set_errf(err, path, e->line, e->col, "unknown local '%.*s'", (int)p->text.len, p->text.data); }
                    w_line(&cg->w, "ErgoVal %s = %s; ergo_retain_val(%s);", pt, cname, pt);
                }
                part_tmps[i] = pt;
            }
            char *parts_name = codegen_new_sym(cg, "parts");
            char *s_name = codegen_new_sym(cg, "s");
            char *arr = codegen_new_tmp(cg);
            w_line(&cg->w, "ErgoVal %s = EV_NULLV;", arr);
            w_line(&cg->w, "{");
            cg->w.indent++;
            {
                StrBuf line;
                sb_init(&line);
                sb_appendf(&line, "ErgoVal %s[%zu] = { ", parts_name, parts->len);
                for (size_t i = 0; i < parts->len; i++) {
                    if (i) sb_append(&line, ", ");
                    sb_append(&line, part_tmps[i]);
                }
                sb_append(&line, " };");
                w_line(&cg->w, "%s", line.data ? line.data : "");
                sb_free(&line);
            }
            w_line(&cg->w, "ErgoStr* %s = stdr_str_from_parts(%zu, %s);", s_name, parts->len, parts_name);
            w_line(&cg->w, "%s = EV_STR(%s);", arr, s_name);
            cg->w.indent--;
            w_line(&cg->w, "}");
            for (size_t i = 0; i < parts->len; i++) {
                w_line(&cg->w, "ergo_release_val(%s);", part_tmps[i]);
            }
            free(part_tmps);
            gen_expr_add(out, arr);
            out->tmp = arr;
            return true;
        }
        case EXPR_TUPLE: {
            cg->arr_id++;
            char *arrsym = arena_printf(cg->arena, "__tup%d", cg->arr_id);
            char *t = codegen_new_tmp(cg);
            w_line(&cg->w, "ErgoArr* %s = stdr_arr_new(%zu);", arrsym, e->as.tuple_lit.items_len);
            w_line(&cg->w, "ErgoVal %s = EV_ARR(%s);", t, arrsym);
            for (size_t i = 0; i < e->as.tuple_lit.items_len; i++) {
                GenExpr ge;
                if (!gen_expr(cg, path, e->as.tuple_lit.items[i], &ge, err)) return false;
                w_line(&cg->w, "ergo_arr_add(%s, %s);", arrsym, ge.tmp);
                gen_expr_release_except(cg, &ge, ge.tmp);
                gen_expr_free(&ge);
            }
            gen_expr_add(out, t);
            out->tmp = t;
            return true;
        }
        case EXPR_ARRAY: {
            cg->arr_id++;
            char *arrsym = arena_printf(cg->arena, "__a%d", cg->arr_id);
            char *t = codegen_new_tmp(cg);
            w_line(&cg->w, "ErgoArr* %s = stdr_arr_new(%zu);", arrsym, e->as.array_lit.items_len);
            w_line(&cg->w, "ErgoVal %s = EV_ARR(%s);", t, arrsym);
            for (size_t i = 0; i < e->as.array_lit.items_len; i++) {
                GenExpr ge;
                if (!gen_expr(cg, path, e->as.array_lit.items[i], &ge, err)) return false;
                w_line(&cg->w, "ergo_arr_add(%s, %s);", arrsym, ge.tmp);
                gen_expr_release_except(cg, &ge, ge.tmp);
                gen_expr_free(&ge);
            }
            gen_expr_add(out, t);
            out->tmp = t;
            return true;
        }
        case EXPR_IDENT: {
            char *t = codegen_new_tmp(cg);
            char *cname = codegen_cname_of(cg, e->as.ident.name);
            if (!cname) {
                FunSig *sig = codegen_fun_sig(cg, cg->current_module, e->as.ident.name);
                if (!sig && is_stdr_prelude(e->as.ident.name)) {
                    bool allow = str_eq_c(cg->current_module, "stdr");
                    for (size_t i = 0; i < cg->current_imports_len && !allow; i++) {
                        if (str_eq_c(cg->current_imports[i], "stdr")) allow = true;
                    }
                    if (allow) {
                        sig = codegen_fun_sig(cg, str_from_c("stdr"), e->as.ident.name);
                    }
                }
                if (!sig) {
                    return cg_set_errf(err, path, e->line, e->col, "unknown local '%.*s'", (int)e->as.ident.name.len, e->as.ident.name.data);
                }
                FunValInfo *fi = codegen_funval_info(cg, sig->module, sig->name);
                if (!fi || !fi->wrapper) {
                    return cg_set_err(err, path, "missing function wrapper (internal error)");
                }
                w_line(&cg->w, "ErgoVal %s = EV_FN(ergo_fn_new(%s, %zu));", t, fi->wrapper, sig->params_len);
                gen_expr_add(out, t);
                out->tmp = t;
                return true;
            }
            w_line(&cg->w, "ErgoVal %s = %s; ergo_retain_val(%s);", t, cname, t);
            gen_expr_add(out, t);
            out->tmp = t;
            return true;
        }
        case EXPR_MEMBER: {
            Ty *base_ty = cg_tc_expr(cg, path, e->as.member.a, err);
            if (!base_ty) return false;
            if (base_ty->tag == TY_MOD) {
                ModuleConsts *mc = codegen_module_consts(cg, base_ty->name);
                ConstEntry *ce = mc ? codegen_find_const(mc, e->as.member.name) : NULL;
                if (ce) {
                    char *t = codegen_new_tmp(cg);
                    if (ce->val.ty && ce->val.ty->tag == TY_PRIM && str_eq_c(ce->val.ty->name, "num")) {
                        if (ce->val.is_float) {
                            w_line(&cg->w, "ErgoVal %s = EV_FLOAT(%.17g);", t, ce->val.f);
                        } else {
                            w_line(&cg->w, "ErgoVal %s = EV_INT(%lld);", t, ce->val.i);
                        }
                    } else if (ce->val.ty && ce->val.ty->tag == TY_PRIM && str_eq_c(ce->val.ty->name, "bool")) {
                        w_line(&cg->w, "ErgoVal %s = EV_BOOL(%s);", t, ce->val.b ? "true" : "false");
                    } else if (ce->val.ty && ce->val.ty->tag == TY_PRIM && str_eq_c(ce->val.ty->name, "string")) {
                        char *esc = c_escape(cg->arena, ce->val.s);
                        w_line(&cg->w, "ErgoVal %s = EV_STR(stdr_str_lit(\"%s\"));", t, esc ? esc : "");
                    } else if (ce->val.ty && ce->val.ty->tag == TY_NULL) {
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                    } else {
                        return cg_set_err(err, path, "unsupported const type");
                    }
                    gen_expr_add(out, t);
                    out->tmp = t;
                    return true;
                }
                ModuleGlobals *mg = codegen_module_globals(cg, base_ty->name);
                if (codegen_find_global(mg, e->as.member.name)) {
                    char *t = codegen_new_tmp(cg);
                    char *gname = mangle_global_var(cg->arena, base_ty->name, e->as.member.name);
                    w_line(&cg->w, "ErgoVal %s = %s; ergo_retain_val(%s);", t, gname, t);
                    gen_expr_add(out, t);
                    out->tmp = t;
                    return true;
                }
                return cg_set_errf(err, path, e->line, e->col, "unknown module member '%.*s.%.*s'", (int)base_ty->name.len, base_ty->name.data, (int)e->as.member.name.len, e->as.member.name.data);
            }
            if (base_ty->tag == TY_CLASS) {
                GenExpr ge;
                if (!gen_expr(cg, path, e->as.member.a, &ge, err)) return false;
                char *t = codegen_new_tmp(cg);
                char *cname = codegen_c_class_name(cg, base_ty->name);
                char *field = codegen_c_field_name(cg, e->as.member.name);
                w_line(&cg->w, "ErgoVal %s = ((%s*)%s.as.p)->%s; ergo_retain_val(%s);", t, cname, ge.tmp, field, t);
                w_line(&cg->w, "ergo_release_val(%s);", ge.tmp);
                gen_expr_release_except(cg, &ge, ge.tmp);
                gen_expr_free(&ge);
                gen_expr_add(out, t);
                out->tmp = t;
                return true;
            }
            return cg_set_err(err, path, "member access not supported on this type");
        }
        case EXPR_MOVE: {
            if (!e->as.move.x || e->as.move.x->kind != EXPR_IDENT) {
                return cg_set_err(err, path, "move(...) must be an identifier");
            }
            char *slot = codegen_cname_of(cg, e->as.move.x->as.ident.name);
            if (!slot) return cg_set_err(err, path, "unknown move target");
            char *t = codegen_new_tmp(cg);
            w_line(&cg->w, "ErgoVal %s = ergo_move(&%s);", t, slot);
            gen_expr_add(out, t);
            out->tmp = t;
            return true;
        }
        case EXPR_MATCH: {
            Ty *scrut_ty = cg_tc_expr(cg, path, e->as.match_expr.scrut, err);
            if (!scrut_ty) return false;
            GenExpr scrut;
            if (!gen_expr(cg, path, e->as.match_expr.scrut, &scrut, err)) return false;
            char *t = codegen_new_tmp(cg);
            char *matched = codegen_new_sym(cg, "matched");
            w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
            w_line(&cg->w, "bool %s = false;", matched);
            for (size_t i = 0; i < e->as.match_expr.arms_len; i++) {
                MatchArm *arm = e->as.match_expr.arms[i];
                char *cond_name = codegen_new_sym(cg, "mc");
                char *bind_tmp = NULL;
                Str bind_name = {NULL, 0};
                if (arm->pat->kind == PAT_WILD) {
                    w_line(&cg->w, "bool %s = true;", cond_name);
                } else if (arm->pat->kind == PAT_IDENT) {
                    bind_name = arm->pat->as.name;
                    w_line(&cg->w, "bool %s = true;", cond_name);
                } else {
                    char *pv = NULL;
                    if (arm->pat->kind == PAT_INT) {
                        pv = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_INT(%lld);", pv, arm->pat->as.i);
                    } else if (arm->pat->kind == PAT_BOOL) {
                        pv = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_BOOL(%s);", pv, arm->pat->as.b ? "true" : "false");
                    } else if (arm->pat->kind == PAT_NULL) {
                        pv = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", pv);
                    } else if (arm->pat->kind == PAT_STR) {
                        Expr tmp_expr;
                        memset(&tmp_expr, 0, sizeof(tmp_expr));
                        tmp_expr.kind = EXPR_STR;
                        tmp_expr.as.str_lit.parts = arm->pat->as.str;
                        GenExpr ge;
                        if (!gen_expr(cg, path, &tmp_expr, &ge, err)) return false;
                        pv = ge.tmp;
                        gen_expr_release_except(cg, &ge, pv);
                        gen_expr_free(&ge);
                    } else {
                        return cg_set_err(err, path, "unsupported match pattern in codegen");
                    }
                    char *eqt = codegen_new_tmp(cg);
                    w_line(&cg->w, "ErgoVal %s = ergo_eq(%s, %s);", eqt, scrut.tmp, pv);
                    w_line(&cg->w, "bool %s = ergo_as_bool(%s);", cond_name, eqt);
                    w_line(&cg->w, "ergo_release_val(%s);", eqt);
                    w_line(&cg->w, "ergo_release_val(%s);", pv);
                }
                w_line(&cg->w, "if (!%s && %s) {", matched, cond_name);
                cg->w.indent++;
                w_line(&cg->w, "%s = true;", matched);
                bool pushed = false;
                if (bind_name.data) {
                    bind_tmp = codegen_new_tmp(cg);
                    w_line(&cg->w, "ErgoVal %s = %s; ergo_retain_val(%s);", bind_tmp, scrut.tmp, bind_tmp);
                    codegen_push_scope(cg);
                    pushed = true;
                    codegen_bind_temp(cg, bind_name, bind_tmp, scrut_ty);
                }
                GenExpr arm_expr;
                if (!gen_expr(cg, path, arm->expr, &arm_expr, err)) return false;
                w_line(&cg->w, "ergo_move_into(&%s, %s);", t, arm_expr.tmp);
                gen_expr_release_except(cg, &arm_expr, arm_expr.tmp);
                gen_expr_free(&arm_expr);
                if (bind_tmp && pushed) {
                    LocalList locals = codegen_pop_scope(cg);
                    codegen_release_scope(cg, locals);
                    w_line(&cg->w, "ergo_release_val(%s);", bind_tmp);
                }
                cg->w.indent--;
                w_line(&cg->w, "}");
            }
            w_line(&cg->w, "ergo_release_val(%s);", scrut.tmp);
            gen_expr_release_except(cg, &scrut, scrut.tmp);
            gen_expr_free(&scrut);
            gen_expr_add(out, t);
            out->tmp = t;
            return true;
        }
        case EXPR_LAMBDA: {
            LambdaInfo *li = codegen_lambda_info(cg, e);
            if (!li) {
                codegen_add_lambda(cg, e, path);
                li = codegen_lambda_info(cg, e);
            }
            char *t = codegen_new_tmp(cg);
            w_line(&cg->w, "ErgoVal %s = EV_FN(ergo_fn_new(%s, %zu));", t, li->name, e->as.lambda.params_len);
            gen_expr_add(out, t);
            out->tmp = t;
            return true;
        }
        case EXPR_NEW: {
            Str name = e->as.new_expr.name;
            Str qname = name;
            if (!memchr(name.data, '.', name.len)) {
                size_t len = cg->current_module.len + 1 + name.len;
                char *buf = (char *)arena_alloc(cg->arena, len + 1);
                if (!buf) return cg_set_err(err, path, "out of memory");
                memcpy(buf, cg->current_module.data, cg->current_module.len);
                buf[cg->current_module.len] = '.';
                memcpy(buf + cg->current_module.len + 1, name.data, name.len);
                buf[len] = '\0';
                qname.data = buf;
                qname.len = len;
            } else {
                size_t dot = 0;
                while (dot < name.len && name.data[dot] != '.') dot++;
                Str mod = { name.data, dot };
                if (!codegen_module_in_scope(cg, mod)) {
                    return cg_set_errf(err, path, e->line, e->col, "unknown class '%.*s'", (int)name.len, name.data);
                }
            }
            ClassDecl *decl = codegen_class_decl(cg, qname);
            if (!decl) {
                return cg_set_errf(err, path, e->line, e->col, "unknown class '%.*s'", (int)name.len, name.data);
            }
            Str mod, cls_short;
            split_qname(qname, &mod, &cls_short);
            char *cname = codegen_c_class_name(cg, qname);
            char *drop_sym = mod.len ? arena_printf(cg->arena, "ergo_drop_%s_%.*s", mangle_mod(cg->arena, mod), (int)cls_short.len, cls_short.data)
                                      : arena_printf(cg->arena, "ergo_drop_%.*s", (int)cls_short.len, cls_short.data);
            char *obj_name = codegen_new_sym(cg, "obj");
            w_line(&cg->w, "%s* %s = (%s*)ergo_obj_new(sizeof(%s), %s);", cname, obj_name, cname, cname, drop_sym);
            for (size_t i = 0; i < decl->fields_len; i++) {
                FieldDecl *fd = decl->fields[i];
                w_line(&cg->w, "%s->%s = EV_NULLV;", obj_name, codegen_c_field_name(cg, fd->name));
            }
            char *t = codegen_new_tmp(cg);
            w_line(&cg->w, "ErgoVal %s = EV_OBJ(%s);", t, obj_name);

            ClassInfo *ci = codegen_class_info(cg, qname);
            if (ci) {
                MethodEntry *init = NULL;
                for (size_t i = 0; i < ci->methods_len; i++) {
                    if (str_eq_c(ci->methods[i].name, "init")) {
                        init = &ci->methods[i];
                        break;
                    }
                }
                if (init) {
                    VEC(char *) arg_ts = VEC_INIT;
                    for (size_t i = 0; i < e->as.new_expr.args_len; i++) {
                        GenExpr ge;
                        if (!gen_expr(cg, path, e->as.new_expr.args[i], &ge, err)) { VEC_FREE(arg_ts); return false; }
                        VEC_PUSH(arg_ts, ge.tmp);
                        gen_expr_release_except(cg, &ge, ge.tmp);
                        gen_expr_free(&ge);
                    }
                    // call init
                    Str mname = str_from_c("init");
                    char *mangled = mangle_method(cg->arena, mod, cls_short, mname);
                    StrBuf line; sb_init(&line);
                    sb_appendf(&line, "%s(%s", mangled, t);
                    for (size_t i = 0; i < arg_ts.len; i++) {
                        sb_append(&line, ", ");
                        sb_append(&line, arg_ts.data[i]);
                    }
                    sb_append(&line, ");");
                    w_line(&cg->w, "%s", line.data ? line.data : "");
                    sb_free(&line);
                    for (size_t i = 0; i < arg_ts.len; i++) {
                        w_line(&cg->w, "ergo_release_val(%s);", arg_ts.data[i]);
                    }
                    VEC_FREE(arg_ts);
                }
            }
            gen_expr_add(out, t);
            out->tmp = t;
            return true;
        }
        case EXPR_UNARY: {
            GenExpr ge;
            if (!gen_expr(cg, path, e->as.unary.x, &ge, err)) return false;
            char *t = codegen_new_tmp(cg);
            if (e->as.unary.op == TOK_BANG) {
                w_line(&cg->w, "ErgoVal %s = EV_BOOL(!ergo_as_bool(%s));", t, ge.tmp);
            } else if (e->as.unary.op == TOK_MINUS) {
                Ty *xty = cg_tc_expr(cg, path, e->as.unary.x, err);
                if (!xty) return false;
                if (xty->tag == TY_PRIM && str_eq_c(xty->name, "num")) {
                    w_line(&cg->w, "ErgoVal %s = ergo_neg(%s);", t, ge.tmp);
                } else {
                    w_line(&cg->w, "ErgoVal %s = EV_INT(-ergo_as_int(%s));", t, ge.tmp);
                }
            } else {
                gen_expr_free(&ge);
                return cg_set_err(err, path, "unsupported unary op");
            }
            w_line(&cg->w, "ergo_release_val(%s);", ge.tmp);
            gen_expr_release_except(cg, &ge, ge.tmp);
            gen_expr_free(&ge);
            gen_expr_add(out, t);
            out->tmp = t;
            return true;
        }
        case EXPR_BINARY: {
            TokKind op = e->as.binary.op;
            if (op != TOK_ANDAND && op != TOK_OROR) {
                GenExpr a;
                GenExpr b;
                if (!gen_expr(cg, path, e->as.binary.a, &a, err)) return false;
                if (!gen_expr(cg, path, e->as.binary.b, &b, err)) { gen_expr_free(&a); return false; }
                char *t = codegen_new_tmp(cg);
                const char *opfn = NULL;
                switch (op) {
                    case TOK_PLUS: opfn = "ergo_add"; break;
                    case TOK_MINUS: opfn = "ergo_sub"; break;
                    case TOK_STAR: opfn = "ergo_mul"; break;
                    case TOK_SLASH: opfn = "ergo_div"; break;
                    case TOK_PERCENT: opfn = "ergo_mod"; break;
                    case TOK_EQEQ: opfn = "ergo_eq"; break;
                    case TOK_NEQ: opfn = "ergo_ne"; break;
                    case TOK_LT: opfn = "ergo_lt"; break;
                    case TOK_LTE: opfn = "ergo_le"; break;
                    case TOK_GT: opfn = "ergo_gt"; break;
                    case TOK_GTE: opfn = "ergo_ge"; break;
                    default: break;
                }
                if (!opfn) {
                    gen_expr_free(&a);
                    gen_expr_free(&b);
                    return cg_set_err(err, path, "unsupported binary op");
                }
                w_line(&cg->w, "ErgoVal %s = %s(%s, %s);", t, opfn, a.tmp, b.tmp);
                w_line(&cg->w, "ergo_release_val(%s);", a.tmp);
                w_line(&cg->w, "ergo_release_val(%s);", b.tmp);
                gen_expr_release_except(cg, &a, a.tmp);
                gen_expr_release_except(cg, &b, b.tmp);
                gen_expr_free(&a);
                gen_expr_free(&b);
                gen_expr_add(out, t);
                out->tmp = t;
                return true;
            }
            // logical ops
            GenExpr left;
            if (!gen_expr(cg, path, e->as.binary.a, &left, err)) return false;
            char *t = codegen_new_tmp(cg);
            w_line(&cg->w, "ErgoVal %s = EV_BOOL(false);", t);
            if (op == TOK_ANDAND) {
                w_line(&cg->w, "if (ergo_as_bool(%s)) {", left.tmp);
                cg->w.indent++;
                GenExpr right;
                if (!gen_expr(cg, path, e->as.binary.b, &right, err)) { gen_expr_free(&left); return false; }
                w_line(&cg->w, "ergo_move_into(&%s, EV_BOOL(ergo_as_bool(%s)));", t, right.tmp);
                w_line(&cg->w, "ergo_release_val(%s);", right.tmp);
                gen_expr_release_except(cg, &right, right.tmp);
                gen_expr_free(&right);
                cg->w.indent--;
                w_line(&cg->w, "} else {");
                cg->w.indent++;
                w_line(&cg->w, "ergo_move_into(&%s, EV_BOOL(false));", t);
                cg->w.indent--;
                w_line(&cg->w, "}");
            } else {
                w_line(&cg->w, "if (ergo_as_bool(%s)) {", left.tmp);
                cg->w.indent++;
                w_line(&cg->w, "ergo_move_into(&%s, EV_BOOL(true));", t);
                cg->w.indent--;
                w_line(&cg->w, "} else {");
                cg->w.indent++;
                GenExpr right;
                if (!gen_expr(cg, path, e->as.binary.b, &right, err)) { gen_expr_free(&left); return false; }
                w_line(&cg->w, "ergo_move_into(&%s, EV_BOOL(ergo_as_bool(%s)));", t, right.tmp);
                w_line(&cg->w, "ergo_release_val(%s);", right.tmp);
                gen_expr_release_except(cg, &right, right.tmp);
                gen_expr_free(&right);
                cg->w.indent--;
                w_line(&cg->w, "}");
            }
            w_line(&cg->w, "ergo_release_val(%s);", left.tmp);
            gen_expr_release_except(cg, &left, left.tmp);
            gen_expr_free(&left);
            gen_expr_add(out, t);
            out->tmp = t;
            return true;
        }
        case EXPR_INDEX: {
            Ty *base_ty = cg_tc_expr(cg, path, e->as.index.a, err);
            if (!base_ty) return false;
            GenExpr at;
            GenExpr it;
            if (!gen_expr(cg, path, e->as.index.a, &at, err)) return false;
            if (!gen_expr(cg, path, e->as.index.i, &it, err)) { gen_expr_free(&at); return false; }
            char *t = codegen_new_tmp(cg);
            if (base_ty->tag == TY_PRIM && str_eq_c(base_ty->name, "string")) {
                w_line(&cg->w, "ErgoVal %s = stdr_str_at(%s, ergo_as_int(%s));", t, at.tmp, it.tmp);
            } else {
                w_line(&cg->w, "ErgoVal %s = ergo_arr_get((ErgoArr*)%s.as.p, ergo_as_int(%s));", t, at.tmp, it.tmp);
            }
            w_line(&cg->w, "ergo_release_val(%s);", at.tmp);
            w_line(&cg->w, "ergo_release_val(%s);", it.tmp);
            gen_expr_release_except(cg, &at, at.tmp);
            gen_expr_release_except(cg, &it, it.tmp);
            gen_expr_free(&at);
            gen_expr_free(&it);
            gen_expr_add(out, t);
            out->tmp = t;
            return true;
        }
        case EXPR_TERNARY: {
            GenExpr ct;
            if (!gen_expr(cg, path, e->as.ternary.cond, &ct, err)) return false;
            char *t = codegen_new_tmp(cg);
            w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
            w_line(&cg->w, "if (ergo_as_bool(%s)) {", ct.tmp);
            cg->w.indent++;
            GenExpr at;
            if (!gen_expr(cg, path, e->as.ternary.then_expr, &at, err)) { gen_expr_free(&ct); return false; }
            w_line(&cg->w, "ergo_move_into(&%s, %s);", t, at.tmp);
            gen_expr_release_except(cg, &at, at.tmp);
            gen_expr_free(&at);
            cg->w.indent--;
            w_line(&cg->w, "} else {");
            cg->w.indent++;
            GenExpr bt;
            if (!gen_expr(cg, path, e->as.ternary.else_expr, &bt, err)) { gen_expr_free(&ct); return false; }
            w_line(&cg->w, "ergo_move_into(&%s, %s);", t, bt.tmp);
            gen_expr_release_except(cg, &bt, bt.tmp);
            gen_expr_free(&bt);
            cg->w.indent--;
            w_line(&cg->w, "}");
            w_line(&cg->w, "ergo_release_val(%s);", ct.tmp);
            gen_expr_release_except(cg, &ct, ct.tmp);
            gen_expr_free(&ct);
            gen_expr_add(out, t);
            out->tmp = t;
            return true;
        }
        case EXPR_ASSIGN: {
            GenExpr vt;
            if (!gen_expr(cg, path, e->as.assign.value, &vt, err)) return false;
            char *tret = codegen_new_tmp(cg);
            w_line(&cg->w, "ErgoVal %s = %s; ergo_retain_val(%s);", tret, vt.tmp, tret);
            if (e->as.assign.target->kind == EXPR_IDENT) {
                char *slot = codegen_cname_of(cg, e->as.assign.target->as.ident.name);
                if (!slot) return cg_set_err(err, path, "unknown assignment target");
                w_line(&cg->w, "ergo_move_into(&%s, %s);", slot, vt.tmp);
            } else if (e->as.assign.target->kind == EXPR_INDEX) {
                GenExpr at, it;
                if (!gen_expr(cg, path, e->as.assign.target->as.index.a, &at, err)) return false;
                if (!gen_expr(cg, path, e->as.assign.target->as.index.i, &it, err)) { gen_expr_free(&at); return false; }
                w_line(&cg->w, "ergo_arr_set((ErgoArr*)%s.as.p, ergo_as_int(%s), %s);", at.tmp, it.tmp, vt.tmp);
                w_line(&cg->w, "ergo_release_val(%s);", at.tmp);
                w_line(&cg->w, "ergo_release_val(%s);", it.tmp);
                gen_expr_release_except(cg, &at, at.tmp);
                gen_expr_release_except(cg, &it, it.tmp);
                gen_expr_free(&at);
                gen_expr_free(&it);
            } else if (e->as.assign.target->kind == EXPR_MEMBER) {
                Ty *base_ty = cg_tc_expr(cg, path, e->as.assign.target->as.member.a, err);
                if (!base_ty || base_ty->tag != TY_CLASS) return cg_set_err(err, path, "unsupported member assignment");
                GenExpr at;
                if (!gen_expr(cg, path, e->as.assign.target->as.member.a, &at, err)) return false;
                char *cname = codegen_c_class_name(cg, base_ty->name);
                char *field = codegen_c_field_name(cg, e->as.assign.target->as.member.name);
                w_line(&cg->w, "ergo_move_into(&((%s*)%s.as.p)->%s, %s);", cname, at.tmp, field, vt.tmp);
                w_line(&cg->w, "ergo_release_val(%s);", at.tmp);
                gen_expr_release_except(cg, &at, at.tmp);
                gen_expr_free(&at);
            } else {
                return cg_set_err(err, path, "unsupported assignment target");
            }
            gen_expr_release_except(cg, &vt, vt.tmp);
            gen_expr_free(&vt);
            gen_expr_add(out, tret);
            out->tmp = tret;
            return true;
        }
        case EXPR_CALL: {
            Expr *fn = e->as.call.fn;
            // module-qualified calls
            if (fn && fn->kind == EXPR_MEMBER && fn->as.member.a && fn->as.member.a->kind == EXPR_IDENT) {
                Str mod = fn->as.member.a->as.ident.name;
                if (codegen_module_in_scope(cg, mod)) {
                    Str name = fn->as.member.name;
                    FunSig *sig = codegen_fun_sig(cg, mod, name);
                    if (!sig) {
                        return cg_set_errf(err, path, e->line, e->col, "unknown %.*s.%.*s", (int)mod.len, mod.data, (int)name.len, name.data);
                    }
                    VEC(char *) arg_ts = VEC_INIT;
                    for (size_t i = 0; i < e->as.call.args_len; i++) {
                        GenExpr ge;
                        if (!gen_expr(cg, path, e->as.call.args[i], &ge, err)) { VEC_FREE(arg_ts); return false; }
                        VEC_PUSH(arg_ts, ge.tmp);
                        gen_expr_release_except(cg, &ge, ge.tmp);
                        gen_expr_free(&ge);
                    }
                    bool ret_void = sig->ret && sig->ret->tag == TY_VOID;
                    if (ret_void) {
                        StrBuf line; sb_init(&line);
                        char *mangled = mangle_global(cg->arena, mod, name);
                        sb_appendf(&line, "%s(", mangled);
                        for (size_t i = 0; i < arg_ts.len; i++) {
                            if (i) sb_append(&line, ", ");
                            sb_append(&line, arg_ts.data[i]);
                        }
                        sb_append(&line, ");");
                        w_line(&cg->w, "%s", line.data ? line.data : "");
                        sb_free(&line);
                        for (size_t i = 0; i < arg_ts.len; i++) {
                            w_line(&cg->w, "ergo_release_val(%s);", arg_ts.data[i]);
                        }
                        VEC_FREE(arg_ts);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    char *t = codegen_new_tmp(cg);
                    StrBuf line; sb_init(&line);
                    char *mangled = mangle_global(cg->arena, mod, name);
                    sb_appendf(&line, "ErgoVal %s = %s(", t, mangled);
                    for (size_t i = 0; i < arg_ts.len; i++) {
                        if (i) sb_append(&line, ", ");
                        sb_append(&line, arg_ts.data[i]);
                    }
                    sb_append(&line, ");");
                    w_line(&cg->w, "%s", line.data ? line.data : "");
                    sb_free(&line);
                    for (size_t i = 0; i < arg_ts.len; i++) {
                        w_line(&cg->w, "ergo_release_val(%s);", arg_ts.data[i]);
                    }
                    VEC_FREE(arg_ts);
                    gen_expr_add(out, t);
                    out->tmp = t;
                    return true;
                }
            }

            // method calls
            if (fn && fn->kind == EXPR_MEMBER) {
                Expr *base = fn->as.member.a;
                Str mname = fn->as.member.name;
                Ty *base_ty = cg_tc_expr(cg, path, base, err);
                if (!base_ty) return false;

                if (str_eq_c(mname, "to_string") && e->as.call.args_len == 0) {
                    GenExpr bt;
                    if (!gen_expr(cg, path, base, &bt, err)) return false;
                    char *t = codegen_new_tmp(cg);
                    w_line(&cg->w, "ErgoVal %s = EV_STR(stdr_to_string(%s));", t, bt.tmp);
                    w_line(&cg->w, "ergo_release_val(%s);", bt.tmp);
                    gen_expr_release_except(cg, &bt, bt.tmp);
                    gen_expr_free(&bt);
                    gen_expr_add(out, t);
                    out->tmp = t;
                    return true;
                }

                if (base_ty->tag == TY_ARRAY && str_eq_c(mname, "add") && e->as.call.args_len == 1) {
                    GenExpr at, vt;
                    if (!gen_expr(cg, path, base, &at, err)) return false;
                    if (!gen_expr(cg, path, e->as.call.args[0], &vt, err)) { gen_expr_free(&at); return false; }
                    w_line(&cg->w, "ergo_arr_add((ErgoArr*)%s.as.p, %s);", at.tmp, vt.tmp);
                    w_line(&cg->w, "ergo_release_val(%s);", at.tmp);
                    gen_expr_release_except(cg, &at, at.tmp);
                    gen_expr_release_except(cg, &vt, vt.tmp);
                    gen_expr_free(&at);
                    gen_expr_free(&vt);
                    char *t = codegen_new_tmp(cg);
                    w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                    gen_expr_add(out, t);
                    out->tmp = t;
                    return true;
                }

                if (base_ty->tag == TY_ARRAY && str_eq_c(mname, "remove") && e->as.call.args_len == 1) {
                    GenExpr at, it;
                    if (!gen_expr(cg, path, base, &at, err)) return false;
                    if (!gen_expr(cg, path, e->as.call.args[0], &it, err)) { gen_expr_free(&at); return false; }
                    char *t = codegen_new_tmp(cg);
                    w_line(&cg->w, "ErgoVal %s = ergo_arr_remove((ErgoArr*)%s.as.p, ergo_as_int(%s));", t, at.tmp, it.tmp);
                    w_line(&cg->w, "ergo_release_val(%s);", at.tmp);
                    w_line(&cg->w, "ergo_release_val(%s);", it.tmp);
                    gen_expr_release_except(cg, &at, at.tmp);
                    gen_expr_release_except(cg, &it, it.tmp);
                    gen_expr_free(&at);
                    gen_expr_free(&it);
                    gen_expr_add(out, t);
                    out->tmp = t;
                    return true;
                }
                if (base_ty->tag == TY_CLASS) {
                    ClassInfo *ci = codegen_class_info(cg, base_ty->name);
                    if (!ci) return cg_set_err(err, path, "unknown class method");
                    FunSig *sig = NULL;
                    for (size_t i = 0; i < ci->methods_len; i++) {
                        if (str_eq(ci->methods[i].name, mname)) {
                            sig = ci->methods[i].sig;
                            break;
                        }
                    }
                    if (!sig) {
                        return cg_set_errf(err, path, e->line, e->col, "unknown method '%.*s'", (int)mname.len, mname.data);
                    }
                    Str mod, cls_short;
                    split_qname(ci->qname, &mod, &cls_short);
                    GenExpr bt;
                    if (!gen_expr(cg, path, base, &bt, err)) return false;
                    VEC(char *) arg_ts = VEC_INIT;
                    for (size_t i = 0; i < e->as.call.args_len; i++) {
                        GenExpr ge;
                        if (!gen_expr(cg, path, e->as.call.args[i], &ge, err)) { gen_expr_free(&bt); VEC_FREE(arg_ts); return false; }
                        VEC_PUSH(arg_ts, ge.tmp);
                        gen_expr_release_except(cg, &ge, ge.tmp);
                        gen_expr_free(&ge);
                    }
                    bool ret_void = sig->ret && sig->ret->tag == TY_VOID;
                    char *mangled = mangle_method(cg->arena, mod, cls_short, mname);
                    if (ret_void) {
                        StrBuf line; sb_init(&line);
                        sb_appendf(&line, "%s(%s", mangled, bt.tmp);
                        for (size_t i = 0; i < arg_ts.len; i++) {
                            sb_append(&line, ", ");
                            sb_append(&line, arg_ts.data[i]);
                        }
                        sb_append(&line, ");");
                        w_line(&cg->w, "%s", line.data ? line.data : "");
                        sb_free(&line);
                        w_line(&cg->w, "ergo_release_val(%s);", bt.tmp);
                        gen_expr_release_except(cg, &bt, bt.tmp);
                        gen_expr_free(&bt);
                        for (size_t i = 0; i < arg_ts.len; i++) {
                            w_line(&cg->w, "ergo_release_val(%s);", arg_ts.data[i]);
                        }
                        VEC_FREE(arg_ts);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    char *t = codegen_new_tmp(cg);
                    StrBuf line; sb_init(&line);
                    sb_appendf(&line, "ErgoVal %s = %s(%s", t, mangled, bt.tmp);
                    for (size_t i = 0; i < arg_ts.len; i++) {
                        sb_append(&line, ", ");
                        sb_append(&line, arg_ts.data[i]);
                    }
                    sb_append(&line, ");");
                    w_line(&cg->w, "%s", line.data ? line.data : "");
                    sb_free(&line);
                    w_line(&cg->w, "ergo_release_val(%s);", bt.tmp);
                    gen_expr_release_except(cg, &bt, bt.tmp);
                    gen_expr_free(&bt);
                    for (size_t i = 0; i < arg_ts.len; i++) {
                        w_line(&cg->w, "ergo_release_val(%s);", arg_ts.data[i]);
                    }
                    VEC_FREE(arg_ts);
                    gen_expr_add(out, t);
                    out->tmp = t;
                    return true;
                }

                return cg_set_err(err, path, "unknown member call");
            }

            // global prelude calls
            if (fn && fn->kind == EXPR_IDENT) {
                Str fname = fn->as.ident.name;
                if (!locals_lookup(&cg->ty_loc, fname)) {
                    if (str_eq_c(fname, "str")) {
                        if (e->as.call.args_len != 1) return cg_set_err(err, path, "str expects 1 arg");
                        GenExpr at;
                        if (!gen_expr(cg, path, e->as.call.args[0], &at, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_STR(stdr_to_string(%s));", t, at.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", at.tmp);
                        gen_expr_release_except(cg, &at, at.tmp);
                        gen_expr_free(&at);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__len")) {
                        GenExpr at;
                        if (!gen_expr(cg, path, e->as.call.args[0], &at, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_INT(stdr_len(%s));", t, at.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", at.tmp);
                        gen_expr_release_except(cg, &at, at.tmp);
                        gen_expr_free(&at);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__writef")) {
                        GenExpr fmt;
                        GenExpr args;
                        if (!gen_expr(cg, path, e->as.call.args[0], &fmt, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &args, err)) { gen_expr_free(&fmt); return false; }
                        w_line(&cg->w, "stdr_writef_args(%s, %s);", fmt.tmp, args.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", fmt.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", args.tmp);
                        gen_expr_release_except(cg, &fmt, fmt.tmp);
                        gen_expr_release_except(cg, &args, args.tmp);
                        gen_expr_free(&fmt);
                        gen_expr_free(&args);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__read_line")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_STR(stdr_read_line());", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__readf_parse")) {
                        GenExpr fmt, line, args;
                        if (!gen_expr(cg, path, e->as.call.args[0], &fmt, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &line, err)) { gen_expr_free(&fmt); return false; }
                        if (!gen_expr(cg, path, e->as.call.args[2], &args, err)) { gen_expr_free(&fmt); gen_expr_free(&line); return false; }
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = stdr_readf_parse(%s, %s, %s);", t, fmt.tmp, line.tmp, args.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", fmt.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", line.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", args.tmp);
                        gen_expr_release_except(cg, &fmt, fmt.tmp);
                        gen_expr_release_except(cg, &line, line.tmp);
                        gen_expr_release_except(cg, &args, args.tmp);
                        gen_expr_free(&fmt);
                        gen_expr_free(&line);
                        gen_expr_free(&args);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_app")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_app_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_window")) {
                        GenExpr title, w, h;
                        if (!gen_expr(cg, path, e->as.call.args[0], &title, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &w, err)) { gen_expr_free(&title); return false; }
                        if (!gen_expr(cg, path, e->as.call.args[2], &h, err)) { gen_expr_free(&title); gen_expr_free(&w); return false; }
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_window_new(%s, %s, %s);", t, title.tmp, w.tmp, h.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", title.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", w.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", h.tmp);
                        gen_expr_release_except(cg, &title, title.tmp);
                        gen_expr_release_except(cg, &w, w.tmp);
                        gen_expr_release_except(cg, &h, h.tmp);
                        gen_expr_free(&title);
                        gen_expr_free(&w);
                        gen_expr_free(&h);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_button")) {
                        GenExpr text;
                        if (!gen_expr(cg, path, e->as.call.args[0], &text, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_button_new(%s);", t, text.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", text.tmp);
                        gen_expr_release_except(cg, &text, text.tmp);
                        gen_expr_free(&text);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_iconbtn")) {
                        GenExpr icon;
                        if (!gen_expr(cg, path, e->as.call.args[0], &icon, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_iconbtn_new(%s);", t, icon.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", icon.tmp);
                        gen_expr_release_except(cg, &icon, icon.tmp);
                        gen_expr_free(&icon);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_label")) {
                        GenExpr text;
                        if (!gen_expr(cg, path, e->as.call.args[0], &text, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_label_new(%s);", t, text.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", text.tmp);
                        gen_expr_release_except(cg, &text, text.tmp);
                        gen_expr_free(&text);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_dialog")) {
                        GenExpr title;
                        if (!gen_expr(cg, path, e->as.call.args[0], &title, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_dialog_new(%s);", t, title.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", title.tmp);
                        gen_expr_release_except(cg, &title, title.tmp);
                        gen_expr_free(&title);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_dialog_slot")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_dialog_slot_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_image")) {
                        GenExpr icon;
                        if (!gen_expr(cg, path, e->as.call.args[0], &icon, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_image_new(%s);", t, icon.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", icon.tmp);
                        gen_expr_release_except(cg, &icon, icon.tmp);
                        gen_expr_free(&icon);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_checkbox")) {
                        GenExpr text, group;
                        if (!gen_expr(cg, path, e->as.call.args[0], &text, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &group, err)) { gen_expr_free(&text); return false; }
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_checkbox_new(%s, %s);", t, text.tmp, group.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", text.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", group.tmp);
                        gen_expr_release_except(cg, &text, text.tmp);
                        gen_expr_release_except(cg, &group, group.tmp);
                        gen_expr_free(&text);
                        gen_expr_free(&group);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_switch")) {
                        GenExpr text;
                        if (!gen_expr(cg, path, e->as.call.args[0], &text, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_switch_new(%s);", t, text.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", text.tmp);
                        gen_expr_release_except(cg, &text, text.tmp);
                        gen_expr_free(&text);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_textfield")) {
                        GenExpr text;
                        if (!gen_expr(cg, path, e->as.call.args[0], &text, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_textfield_new(%s);", t, text.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", text.tmp);
                        gen_expr_release_except(cg, &text, text.tmp);
                        gen_expr_free(&text);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_searchfield")) {
                        GenExpr text;
                        if (!gen_expr(cg, path, e->as.call.args[0], &text, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_searchfield_new(%s);", t, text.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", text.tmp);
                        gen_expr_release_except(cg, &text, text.tmp);
                        gen_expr_free(&text);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_textview")) {
                        GenExpr text;
                        if (!gen_expr(cg, path, e->as.call.args[0], &text, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_textview_new(%s);", t, text.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", text.tmp);
                        gen_expr_release_except(cg, &text, text.tmp);
                        gen_expr_free(&text);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_dropdown")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_dropdown_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_datepicker")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_datepicker_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_stepper")) {
                        GenExpr minv, maxv, valv, stepv;
                        if (!gen_expr(cg, path, e->as.call.args[0], &minv, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &maxv, err)) { gen_expr_free(&minv); return false; }
                        if (!gen_expr(cg, path, e->as.call.args[2], &valv, err)) { gen_expr_free(&minv); gen_expr_free(&maxv); return false; }
                        if (!gen_expr(cg, path, e->as.call.args[3], &stepv, err)) { gen_expr_free(&minv); gen_expr_free(&maxv); gen_expr_free(&valv); return false; }
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_stepper_new(%s, %s, %s, %s);", t, minv.tmp, maxv.tmp, valv.tmp, stepv.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", minv.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", maxv.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", valv.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", stepv.tmp);
                        gen_expr_release_except(cg, &minv, minv.tmp);
                        gen_expr_release_except(cg, &maxv, maxv.tmp);
                        gen_expr_release_except(cg, &valv, valv.tmp);
                        gen_expr_release_except(cg, &stepv, stepv.tmp);
                        gen_expr_free(&minv);
                        gen_expr_free(&maxv);
                        gen_expr_free(&valv);
                        gen_expr_free(&stepv);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_slider")) {
                        GenExpr minv, maxv, valv;
                        if (!gen_expr(cg, path, e->as.call.args[0], &minv, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &maxv, err)) { gen_expr_free(&minv); return false; }
                        if (!gen_expr(cg, path, e->as.call.args[2], &valv, err)) { gen_expr_free(&minv); gen_expr_free(&maxv); return false; }
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_slider_new(%s, %s, %s);", t, minv.tmp, maxv.tmp, valv.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", minv.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", maxv.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", valv.tmp);
                        gen_expr_release_except(cg, &minv, minv.tmp);
                        gen_expr_release_except(cg, &maxv, maxv.tmp);
                        gen_expr_release_except(cg, &valv, valv.tmp);
                        gen_expr_free(&minv);
                        gen_expr_free(&maxv);
                        gen_expr_free(&valv);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_tabs")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_tabs_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_segmented")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_segmented_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_view_switcher")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_view_switcher_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_progress")) {
                        GenExpr val;
                        if (!gen_expr(cg, path, e->as.call.args[0], &val, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_progress_new(%s);", t, val.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", val.tmp);
                        gen_expr_release_except(cg, &val, val.tmp);
                        gen_expr_free(&val);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_treeview")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_treeview_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_colorpicker")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_colorpicker_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_toasts")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_toasts_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_toast")) {
                        GenExpr text;
                        if (!gen_expr(cg, path, e->as.call.args[0], &text, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_toast_new(%s);", t, text.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", text.tmp);
                        gen_expr_release_except(cg, &text, text.tmp);
                        gen_expr_free(&text);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_appbar")) {
                        GenExpr title, subtitle;
                        if (!gen_expr(cg, path, e->as.call.args[0], &title, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &subtitle, err)) { gen_expr_free(&title); return false; }
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_appbar_new(%s, %s);", t, title.tmp, subtitle.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", title.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", subtitle.tmp);
                        gen_expr_release_except(cg, &title, title.tmp);
                        gen_expr_release_except(cg, &subtitle, subtitle.tmp);
                        gen_expr_free(&title);
                        gen_expr_free(&subtitle);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_toolbar")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_toolbar_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_vstack")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_vstack_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_hstack")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_hstack_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_zstack")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_zstack_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_fixed")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_fixed_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_scroller")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_scroller_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_list")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_list_new();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_grid")) {
                        GenExpr cols;
                        if (!gen_expr(cg, path, e->as.call.args[0], &cols, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_grid_new(%s);", t, cols.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", cols.tmp);
                        gen_expr_release_except(cg, &cols, cols.tmp);
                        gen_expr_free(&cols);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_container_add")) {
                        GenExpr parent, child;
                        if (!gen_expr(cg, path, e->as.call.args[0], &parent, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &child, err)) { gen_expr_free(&parent); return false; }
                        w_line(&cg->w, "cogito_container_add(%s, %s);", parent.tmp, child.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", parent.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", child.tmp);
                        gen_expr_release_except(cg, &parent, parent.tmp);
                        gen_expr_release_except(cg, &child, child.tmp);
                        gen_expr_free(&parent);
                        gen_expr_free(&child);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_container_set_margins")) {
                        GenExpr node, left, top, right, bottom;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &left, err)) { gen_expr_free(&node); return false; }
                        if (!gen_expr(cg, path, e->as.call.args[2], &top, err)) { gen_expr_free(&node); gen_expr_free(&left); return false; }
                        if (!gen_expr(cg, path, e->as.call.args[3], &right, err)) { gen_expr_free(&node); gen_expr_free(&left); gen_expr_free(&top); return false; }
                        if (!gen_expr(cg, path, e->as.call.args[4], &bottom, err)) { gen_expr_free(&node); gen_expr_free(&left); gen_expr_free(&top); gen_expr_free(&right); return false; }
                        w_line(&cg->w, "cogito_container_set_margins(%s, %s, %s, %s, %s);", node.tmp, left.tmp, top.tmp, right.tmp, bottom.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", left.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", top.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", right.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", bottom.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &left, left.tmp);
                        gen_expr_release_except(cg, &top, top.tmp);
                        gen_expr_release_except(cg, &right, right.tmp);
                        gen_expr_release_except(cg, &bottom, bottom.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&left);
                        gen_expr_free(&top);
                        gen_expr_free(&right);
                        gen_expr_free(&bottom);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_container_set_align")) {
                        GenExpr node, align;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &align, err)) { gen_expr_free(&node); return false; }
                        w_line(&cg->w, "cogito_container_set_align(%s, %s);", node.tmp, align.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", align.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &align, align.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&align);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_container_set_halign")) {
                        GenExpr node, align;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &align, err)) { gen_expr_free(&node); return false; }
                        w_line(&cg->w, "cogito_container_set_halign(%s, %s);", node.tmp, align.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", align.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &align, align.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&align);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_container_set_valign")) {
                        GenExpr node, align;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &align, err)) { gen_expr_free(&node); return false; }
                        w_line(&cg->w, "cogito_container_set_valign(%s, %s);", node.tmp, align.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", align.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &align, align.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&align);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_container_set_hexpand")) {
                        GenExpr node, expand;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &expand, err)) { gen_expr_free(&node); return false; }
                        w_line(&cg->w, "cogito_container_set_hexpand(%s, %s);", node.tmp, expand.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", expand.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &expand, expand.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&expand);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_container_set_vexpand")) {
                        GenExpr node, expand;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &expand, err)) { gen_expr_free(&node); return false; }
                        w_line(&cg->w, "cogito_container_set_vexpand(%s, %s);", node.tmp, expand.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", expand.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &expand, expand.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&expand);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_dialog_slot_show")) {
                        GenExpr slot, dialog;
                        if (!gen_expr(cg, path, e->as.call.args[0], &slot, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &dialog, err)) { gen_expr_free(&slot); return false; }
                        w_line(&cg->w, "cogito_dialog_slot_show(%s, %s);", slot.tmp, dialog.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", slot.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", dialog.tmp);
                        gen_expr_release_except(cg, &slot, slot.tmp);
                        gen_expr_release_except(cg, &dialog, dialog.tmp);
                        gen_expr_free(&slot);
                        gen_expr_free(&dialog);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_dialog_slot_clear")) {
                        GenExpr slot;
                        if (!gen_expr(cg, path, e->as.call.args[0], &slot, err)) return false;
                        w_line(&cg->w, "cogito_dialog_slot_clear(%s);", slot.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", slot.tmp);
                        gen_expr_release_except(cg, &slot, slot.tmp);
                        gen_expr_free(&slot);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_container_set_padding")) {
                        GenExpr node, left, top, right, bottom;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &left, err)) { gen_expr_free(&node); return false; }
                        if (!gen_expr(cg, path, e->as.call.args[2], &top, err)) { gen_expr_free(&node); gen_expr_free(&left); return false; }
                        if (!gen_expr(cg, path, e->as.call.args[3], &right, err)) { gen_expr_free(&node); gen_expr_free(&left); gen_expr_free(&top); return false; }
                        if (!gen_expr(cg, path, e->as.call.args[4], &bottom, err)) { gen_expr_free(&node); gen_expr_free(&left); gen_expr_free(&top); gen_expr_free(&right); return false; }
                        w_line(&cg->w, "cogito_container_set_padding(%s, %s, %s, %s, %s);", node.tmp, left.tmp, top.tmp, right.tmp, bottom.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", left.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", top.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", right.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", bottom.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &left, left.tmp);
                        gen_expr_release_except(cg, &top, top.tmp);
                        gen_expr_release_except(cg, &right, right.tmp);
                        gen_expr_release_except(cg, &bottom, bottom.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&left);
                        gen_expr_free(&top);
                        gen_expr_free(&right);
                        gen_expr_free(&bottom);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_fixed_set_pos")) {
                        GenExpr fixedv, child, x, y;
                        if (!gen_expr(cg, path, e->as.call.args[0], &fixedv, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &child, err)) { gen_expr_free(&fixedv); return false; }
                        if (!gen_expr(cg, path, e->as.call.args[2], &x, err)) { gen_expr_free(&fixedv); gen_expr_free(&child); return false; }
                        if (!gen_expr(cg, path, e->as.call.args[3], &y, err)) { gen_expr_free(&fixedv); gen_expr_free(&child); gen_expr_free(&x); return false; }
                        w_line(&cg->w, "cogito_fixed_set_pos(%s, %s, %s, %s);", fixedv.tmp, child.tmp, x.tmp, y.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", fixedv.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", child.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", x.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", y.tmp);
                        gen_expr_release_except(cg, &fixedv, fixedv.tmp);
                        gen_expr_release_except(cg, &child, child.tmp);
                        gen_expr_release_except(cg, &x, x.tmp);
                        gen_expr_release_except(cg, &y, y.tmp);
                        gen_expr_free(&fixedv);
                        gen_expr_free(&child);
                        gen_expr_free(&x);
                        gen_expr_free(&y);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_scroller_set_axes")) {
                        GenExpr sc, horz, vert;
                        if (!gen_expr(cg, path, e->as.call.args[0], &sc, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &horz, err)) { gen_expr_free(&sc); return false; }
                        if (!gen_expr(cg, path, e->as.call.args[2], &vert, err)) { gen_expr_free(&sc); gen_expr_free(&horz); return false; }
                        w_line(&cg->w, "cogito_scroller_set_axes(%s, %s, %s);", sc.tmp, horz.tmp, vert.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", sc.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", horz.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", vert.tmp);
                        gen_expr_release_except(cg, &sc, sc.tmp);
                        gen_expr_release_except(cg, &horz, horz.tmp);
                        gen_expr_release_except(cg, &vert, vert.tmp);
                        gen_expr_free(&sc);
                        gen_expr_free(&horz);
                        gen_expr_free(&vert);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_grid_set_gap")) {
                        GenExpr grid, x, y;
                        if (!gen_expr(cg, path, e->as.call.args[0], &grid, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &x, err)) { gen_expr_free(&grid); return false; }
                        if (!gen_expr(cg, path, e->as.call.args[2], &y, err)) { gen_expr_free(&grid); gen_expr_free(&x); return false; }
                        w_line(&cg->w, "cogito_grid_set_gap(%s, %s, %s);", grid.tmp, x.tmp, y.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", grid.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", x.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", y.tmp);
                        gen_expr_release_except(cg, &grid, grid.tmp);
                        gen_expr_release_except(cg, &x, x.tmp);
                        gen_expr_release_except(cg, &y, y.tmp);
                        gen_expr_free(&grid);
                        gen_expr_free(&x);
                        gen_expr_free(&y);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_grid_set_span")) {
                        GenExpr child, cols, rows;
                        if (!gen_expr(cg, path, e->as.call.args[0], &child, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &cols, err)) { gen_expr_free(&child); return false; }
                        if (!gen_expr(cg, path, e->as.call.args[2], &rows, err)) { gen_expr_free(&child); gen_expr_free(&cols); return false; }
                        w_line(&cg->w, "cogito_grid_set_span(%s, %s, %s);", child.tmp, cols.tmp, rows.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", child.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", cols.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", rows.tmp);
                        gen_expr_release_except(cg, &child, child.tmp);
                        gen_expr_release_except(cg, &cols, cols.tmp);
                        gen_expr_release_except(cg, &rows, rows.tmp);
                        gen_expr_free(&child);
                        gen_expr_free(&cols);
                        gen_expr_free(&rows);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_grid_set_align")) {
                        GenExpr child, hx, hy;
                        if (!gen_expr(cg, path, e->as.call.args[0], &child, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &hx, err)) { gen_expr_free(&child); return false; }
                        if (!gen_expr(cg, path, e->as.call.args[2], &hy, err)) { gen_expr_free(&child); gen_expr_free(&hx); return false; }
                        w_line(&cg->w, "cogito_grid_set_align(%s, %s, %s);", child.tmp, hx.tmp, hy.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", child.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", hx.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", hy.tmp);
                        gen_expr_release_except(cg, &child, child.tmp);
                        gen_expr_release_except(cg, &hx, hx.tmp);
                        gen_expr_release_except(cg, &hy, hy.tmp);
                        gen_expr_free(&child);
                        gen_expr_free(&hx);
                        gen_expr_free(&hy);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_label_set_class")) {
                        GenExpr label, classv;
                        if (!gen_expr(cg, path, e->as.call.args[0], &label, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &classv, err)) { gen_expr_free(&label); return false; }
                        w_line(&cg->w, "cogito_label_set_class(%s, %s);", label.tmp, classv.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", label.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", classv.tmp);
                        gen_expr_release_except(cg, &label, label.tmp);
                        gen_expr_release_except(cg, &classv, classv.tmp);
                        gen_expr_free(&label);
                        gen_expr_free(&classv);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_label_set_wrap")) {
                        GenExpr label, onv;
                        if (!gen_expr(cg, path, e->as.call.args[0], &label, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &onv, err)) { gen_expr_free(&label); return false; }
                        w_line(&cg->w, "cogito_label_set_wrap(%s, %s);", label.tmp, onv.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", label.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", onv.tmp);
                        gen_expr_release_except(cg, &label, label.tmp);
                        gen_expr_release_except(cg, &onv, onv.tmp);
                        gen_expr_free(&label);
                        gen_expr_free(&onv);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_label_set_ellipsis")) {
                        GenExpr label, onv;
                        if (!gen_expr(cg, path, e->as.call.args[0], &label, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &onv, err)) { gen_expr_free(&label); return false; }
                        w_line(&cg->w, "cogito_label_set_ellipsis(%s, %s);", label.tmp, onv.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", label.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", onv.tmp);
                        gen_expr_release_except(cg, &label, label.tmp);
                        gen_expr_release_except(cg, &onv, onv.tmp);
                        gen_expr_free(&label);
                        gen_expr_free(&onv);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_label_set_align")) {
                        GenExpr label, alignv;
                        if (!gen_expr(cg, path, e->as.call.args[0], &label, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &alignv, err)) { gen_expr_free(&label); return false; }
                        w_line(&cg->w, "cogito_label_set_align(%s, %s);", label.tmp, alignv.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", label.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", alignv.tmp);
                        gen_expr_release_except(cg, &label, label.tmp);
                        gen_expr_release_except(cg, &alignv, alignv.tmp);
                        gen_expr_free(&label);
                        gen_expr_free(&alignv);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_node_set_disabled")) {
                        GenExpr node, onv;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &onv, err)) { gen_expr_free(&node); return false; }
                        w_line(&cg->w, "cogito_node_set_disabled(%s, %s);", node.tmp, onv.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", onv.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &onv, onv.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&onv);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_node_set_id")) {
                        GenExpr node, idv;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &idv, err)) { gen_expr_free(&node); return false; }
                        w_line(&cg->w, "cogito_node_set_id(%s, %s);", node.tmp, idv.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", idv.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &idv, idv.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&idv);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_node_set_class")) {
                        GenExpr node, classv;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &classv, err)) { gen_expr_free(&node); return false; }
                        w_line(&cg->w, "cogito_node_set_class(%s, %s);", node.tmp, classv.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", classv.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &classv, classv.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&classv);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_node_set_a11y_label")) {
                        GenExpr node, labelv;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &labelv, err)) { gen_expr_free(&node); return false; }
                        w_line(&cg->w, "cogito_node_set_a11y_label(%s, %s);", node.tmp, labelv.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", labelv.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &labelv, labelv.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&labelv);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_node_set_a11y_role")) {
                        GenExpr node, rolev;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &rolev, err)) { gen_expr_free(&node); return false; }
                        w_line(&cg->w, "cogito_node_set_a11y_role(%s, %s);", node.tmp, rolev.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", rolev.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &rolev, rolev.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&rolev);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_node_set_tooltip")) {
                        GenExpr node, textv;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &textv, err)) { gen_expr_free(&node); return false; }
                        w_line(&cg->w, "cogito_node_set_tooltip_val(%s, %s);", node.tmp, textv.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", textv.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &textv, textv.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&textv);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_app_set_appid")) {
                        GenExpr app, idv;
                        if (!gen_expr(cg, path, e->as.call.args[0], &app, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &idv, err)) { gen_expr_free(&app); return false; }
                        w_line(&cg->w, "cogito_app_set_appid(%s, %s);", app.tmp, idv.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", app.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", idv.tmp);
                        gen_expr_release_except(cg, &app, app.tmp);
                        gen_expr_release_except(cg, &idv, idv.tmp);
                        gen_expr_free(&app);
                        gen_expr_free(&idv);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_app_set_accent_color")) {
                        GenExpr app, colorv, ov;
                        if (!gen_expr(cg, path, e->as.call.args[0], &app, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &colorv, err)) { gen_expr_free(&app); return false; }
                        if (!gen_expr(cg, path, e->as.call.args[2], &ov, err)) { gen_expr_free(&app); gen_expr_free(&colorv); return false; }
                        w_line(&cg->w, "cogito_app_set_accent_color(%s, %s, %s);", app.tmp, colorv.tmp, ov.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", app.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", colorv.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", ov.tmp);
                        gen_expr_release_except(cg, &app, app.tmp);
                        gen_expr_release_except(cg, &colorv, colorv.tmp);
                        gen_expr_release_except(cg, &ov, ov.tmp);
                        gen_expr_free(&app);
                        gen_expr_free(&colorv);
                        gen_expr_free(&ov);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_pointer_capture")) {
                        GenExpr node;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        w_line(&cg->w, "cogito_pointer_capture_set(%s);", node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_free(&node);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_pointer_release")) {
                        w_line(&cg->w, "cogito_pointer_capture_clear();");
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_view_switcher_set_active")) {
                        GenExpr node, idv;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &idv, err)) { gen_expr_free(&node); return false; }
                        w_line(&cg->w, "cogito_view_switcher_set_active(%s, %s);", node.tmp, idv.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", idv.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &idv, idv.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&idv);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_textfield_set_text")) {
                        GenExpr node, text;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &text, err)) { gen_expr_free(&node); return false; }
                        w_line(&cg->w, "cogito_textfield_set_text(%s, %s);", node.tmp, text.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", text.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &text, text.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&text);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_textfield_get_text")) {
                        GenExpr node;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_textfield_get_text(%s);", t, node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_free(&node);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_searchfield_set_text")) {
                        GenExpr node, text;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &text, err)) { gen_expr_free(&node); return false; }
                        w_line(&cg->w, "cogito_searchfield_set_text(%s, %s);", node.tmp, text.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", text.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &text, text.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&text);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_searchfield_get_text")) {
                        GenExpr node;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_searchfield_get_text(%s);", t, node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_free(&node);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_searchfield_on_change")) {
                        GenExpr node, fn;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &fn, err)) { gen_expr_free(&node); return false; }
                        w_line(&cg->w, "cogito_searchfield_on_change(%s, %s);", node.tmp, fn.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", fn.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &fn, fn.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&fn);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_textfield_on_change")) {
                        GenExpr node, fn;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &fn, err)) { gen_expr_free(&node); return false; }
                        w_line(&cg->w, "cogito_textfield_on_change(%s, %s);", node.tmp, fn.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", fn.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &fn, fn.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&fn);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_textview_set_text")) {
                        GenExpr node, text;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &text, err)) { gen_expr_free(&node); return false; }
                        w_line(&cg->w, "cogito_textview_set_text(%s, %s);", node.tmp, text.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", text.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &text, text.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&text);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_textview_get_text")) {
                        GenExpr node;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_textview_get_text(%s);", t, node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_free(&node);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_textview_on_change")) {
                        GenExpr node, fn;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &fn, err)) { gen_expr_free(&node); return false; }
                        w_line(&cg->w, "cogito_textview_on_change(%s, %s);", node.tmp, fn.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", fn.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &fn, fn.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&fn);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_datepicker_on_change")) {
                        GenExpr node, fn;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &fn, err)) { gen_expr_free(&node); return false; }
                        w_line(&cg->w, "cogito_datepicker_on_change(%s, %s);", node.tmp, fn.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", fn.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &fn, fn.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&fn);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_dropdown_set_items")) {
                        GenExpr node, arr;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &arr, err)) { gen_expr_free(&node); return false; }
                        w_line(&cg->w, "cogito_dropdown_set_items(%s, %s);", node.tmp, arr.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", arr.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &arr, arr.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&arr);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_dropdown_set_selected")) {
                        GenExpr node, idx;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &idx, err)) { gen_expr_free(&node); return false; }
                        w_line(&cg->w, "cogito_dropdown_set_selected(%s, %s);", node.tmp, idx.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", idx.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &idx, idx.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&idx);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_dropdown_get_selected")) {
                        GenExpr node;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_dropdown_get_selected(%s);", t, node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_free(&node);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_dropdown_on_change")) {
                        GenExpr node, fn;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &fn, err)) { gen_expr_free(&node); return false; }
                        w_line(&cg->w, "cogito_dropdown_on_change(%s, %s);", node.tmp, fn.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", fn.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &fn, fn.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&fn);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_slider_set_value")) {
                        GenExpr node, val;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &val, err)) { gen_expr_free(&node); return false; }
                        w_line(&cg->w, "cogito_slider_set_value(%s, %s);", node.tmp, val.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", val.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &val, val.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&val);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_slider_get_value")) {
                        GenExpr node;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_slider_get_value(%s);", t, node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_free(&node);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_slider_on_change")) {
                        GenExpr node, fn;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &fn, err)) { gen_expr_free(&node); return false; }
                        w_line(&cg->w, "cogito_slider_on_change(%s, %s);", node.tmp, fn.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", fn.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &fn, fn.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&fn);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_colorpicker_on_change")) {
                        GenExpr node, fn;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &fn, err)) { gen_expr_free(&node); return false; }
                        w_line(&cg->w, "cogito_colorpicker_on_change(%s, %s);", node.tmp, fn.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", fn.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &fn, fn.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&fn);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_tabs_set_items")) {
                        GenExpr node, arr;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &arr, err)) { gen_expr_free(&node); return false; }
                        w_line(&cg->w, "cogito_tabs_set_items(%s, %s);", node.tmp, arr.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", arr.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &arr, arr.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&arr);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_tabs_set_ids")) {
                        GenExpr node, arr;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &arr, err)) { gen_expr_free(&node); return false; }
                        w_line(&cg->w, "cogito_tabs_set_ids(%s, %s);", node.tmp, arr.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", arr.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &arr, arr.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&arr);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_tabs_set_selected")) {
                        GenExpr node, idx;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &idx, err)) { gen_expr_free(&node); return false; }
                        w_line(&cg->w, "cogito_tabs_set_selected(%s, %s);", node.tmp, idx.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", idx.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &idx, idx.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&idx);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_tabs_get_selected")) {
                        GenExpr node;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_tabs_get_selected(%s);", t, node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_free(&node);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_tabs_on_change")) {
                        GenExpr node, fn;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &fn, err)) { gen_expr_free(&node); return false; }
                        w_line(&cg->w, "cogito_tabs_on_change(%s, %s);", node.tmp, fn.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", fn.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &fn, fn.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&fn);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_tabs_bind")) {
                        GenExpr tabs, vs;
                        if (!gen_expr(cg, path, e->as.call.args[0], &tabs, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &vs, err)) { gen_expr_free(&tabs); return false; }
                        w_line(&cg->w, "cogito_tabs_bind(%s, %s);", tabs.tmp, vs.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", tabs.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", vs.tmp);
                        gen_expr_release_except(cg, &tabs, tabs.tmp);
                        gen_expr_release_except(cg, &vs, vs.tmp);
                        gen_expr_free(&tabs);
                        gen_expr_free(&vs);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_progress_set_value")) {
                        GenExpr node, val;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &val, err)) { gen_expr_free(&node); return false; }
                        w_line(&cg->w, "cogito_progress_set_value(%s, %s);", node.tmp, val.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", val.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &val, val.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&val);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_progress_get_value")) {
                        GenExpr node;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_progress_get_value(%s);", t, node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_free(&node);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_toast_set_text")) {
                        GenExpr node, text;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &text, err)) { gen_expr_free(&node); return false; }
                        w_line(&cg->w, "cogito_toast_set_text(%s, %s);", node.tmp, text.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", text.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &text, text.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&text);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_toast_on_click")) {
                        GenExpr node, fn;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &fn, err)) { gen_expr_free(&node); return false; }
                        w_line(&cg->w, "cogito_toast_on_click(%s, %s);", node.tmp, fn.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", fn.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &fn, fn.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&fn);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_window_set_autosize")) {
                        GenExpr win, on;
                        if (!gen_expr(cg, path, e->as.call.args[0], &win, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &on, err)) { gen_expr_free(&win); return false; }
                        w_line(&cg->w, "cogito_window_set_autosize(%s, %s);", win.tmp, on.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", win.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", on.tmp);
                        gen_expr_release_except(cg, &win, win.tmp);
                        gen_expr_release_except(cg, &on, on.tmp);
                        gen_expr_free(&win);
                        gen_expr_free(&on);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_window_set_resizable")) {
                        GenExpr win, on;
                        if (!gen_expr(cg, path, e->as.call.args[0], &win, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &on, err)) { gen_expr_free(&win); return false; }
                        w_line(&cg->w, "cogito_window_set_resizable(%s, %s);", win.tmp, on.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", win.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", on.tmp);
                        gen_expr_release_except(cg, &win, win.tmp);
                        gen_expr_release_except(cg, &on, on.tmp);
                        gen_expr_free(&win);
                        gen_expr_free(&on);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_window_set_dialog")) {
                        GenExpr win, dialog;
                        if (!gen_expr(cg, path, e->as.call.args[0], &win, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &dialog, err)) { gen_expr_free(&win); return false; }
                        w_line(&cg->w, "cogito_window_set_dialog(%s, %s);", win.tmp, dialog.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", win.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", dialog.tmp);
                        gen_expr_release_except(cg, &win, win.tmp);
                        gen_expr_release_except(cg, &dialog, dialog.tmp);
                        gen_expr_free(&win);
                        gen_expr_free(&dialog);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_window_clear_dialog")) {
                        GenExpr win;
                        if (!gen_expr(cg, path, e->as.call.args[0], &win, err)) return false;
                        w_line(&cg->w, "cogito_window_clear_dialog(%s);", win.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", win.tmp);
                        gen_expr_release_except(cg, &win, win.tmp);
                        gen_expr_free(&win);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_node_window")) {
                        GenExpr node;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_node_window_val(%s);", t, node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_free(&node);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_build")) {
                        GenExpr node, builder;
                        if (!gen_expr(cg, path, e->as.call.args[0], &node, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &builder, err)) { gen_expr_free(&node); return false; }
                        w_line(&cg->w, "cogito_build(%s, %s);", node.tmp, builder.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", node.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", builder.tmp);
                        gen_expr_release_except(cg, &node, node.tmp);
                        gen_expr_release_except(cg, &builder, builder.tmp);
                        gen_expr_free(&node);
                        gen_expr_free(&builder);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_window_set_builder")) {
                        GenExpr win, builder;
                        if (!gen_expr(cg, path, e->as.call.args[0], &win, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &builder, err)) { gen_expr_free(&win); return false; }
                        w_line(&cg->w, "cogito_window_set_builder(%s, %s);", win.tmp, builder.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", win.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", builder.tmp);
                        gen_expr_release_except(cg, &win, win.tmp);
                        gen_expr_release_except(cg, &builder, builder.tmp);
                        gen_expr_free(&win);
                        gen_expr_free(&builder);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_state_new")) {
                        GenExpr val;
                        if (!gen_expr(cg, path, e->as.call.args[0], &val, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_state_new(%s);", t, val.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", val.tmp);
                        gen_expr_release_except(cg, &val, val.tmp);
                        gen_expr_free(&val);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_state_get")) {
                        GenExpr st;
                        if (!gen_expr(cg, path, e->as.call.args[0], &st, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_state_get(%s);", t, st.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", st.tmp);
                        gen_expr_release_except(cg, &st, st.tmp);
                        gen_expr_free(&st);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_state_set")) {
                        GenExpr st, val;
                        if (!gen_expr(cg, path, e->as.call.args[0], &st, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &val, err)) { gen_expr_free(&st); return false; }
                        w_line(&cg->w, "cogito_state_set(%s, %s);", st.tmp, val.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", st.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", val.tmp);
                        gen_expr_release_except(cg, &st, st.tmp);
                        gen_expr_release_except(cg, &val, val.tmp);
                        gen_expr_free(&st);
                        gen_expr_free(&val);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_button_set_text")) {
                        GenExpr btn, text;
                        if (!gen_expr(cg, path, e->as.call.args[0], &btn, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &text, err)) { gen_expr_free(&btn); return false; }
                        w_line(&cg->w, "cogito_button_set_text(%s, %s);", btn.tmp, text.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", btn.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", text.tmp);
                        gen_expr_release_except(cg, &btn, btn.tmp);
                        gen_expr_release_except(cg, &text, text.tmp);
                        gen_expr_free(&btn);
                        gen_expr_free(&text);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_image_set_icon")) {
                        GenExpr img, icon;
                        if (!gen_expr(cg, path, e->as.call.args[0], &img, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &icon, err)) { gen_expr_free(&img); return false; }
                        w_line(&cg->w, "cogito_image_set_icon(%s, %s);", img.tmp, icon.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", img.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", icon.tmp);
                        gen_expr_release_except(cg, &img, img.tmp);
                        gen_expr_release_except(cg, &icon, icon.tmp);
                        gen_expr_free(&img);
                        gen_expr_free(&icon);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_checkbox_set_checked")) {
                        GenExpr cb, checked;
                        if (!gen_expr(cg, path, e->as.call.args[0], &cb, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &checked, err)) { gen_expr_free(&cb); return false; }
                        w_line(&cg->w, "cogito_checkbox_set_checked(%s, %s);", cb.tmp, checked.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", cb.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", checked.tmp);
                        gen_expr_release_except(cg, &cb, cb.tmp);
                        gen_expr_release_except(cg, &checked, checked.tmp);
                        gen_expr_free(&cb);
                        gen_expr_free(&checked);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_checkbox_get_checked")) {
                        GenExpr cb;
                        if (!gen_expr(cg, path, e->as.call.args[0], &cb, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_checkbox_get_checked(%s);", t, cb.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", cb.tmp);
                        gen_expr_release_except(cg, &cb, cb.tmp);
                        gen_expr_free(&cb);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_switch_set_checked")) {
                        GenExpr sw, checked;
                        if (!gen_expr(cg, path, e->as.call.args[0], &sw, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &checked, err)) { gen_expr_free(&sw); return false; }
                        w_line(&cg->w, "cogito_switch_set_checked(%s, %s);", sw.tmp, checked.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", sw.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", checked.tmp);
                        gen_expr_release_except(cg, &sw, sw.tmp);
                        gen_expr_release_except(cg, &checked, checked.tmp);
                        gen_expr_free(&sw);
                        gen_expr_free(&checked);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_switch_get_checked")) {
                        GenExpr sw;
                        if (!gen_expr(cg, path, e->as.call.args[0], &sw, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_switch_get_checked(%s);", t, sw.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", sw.tmp);
                        gen_expr_release_except(cg, &sw, sw.tmp);
                        gen_expr_free(&sw);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_checkbox_on_change")) {
                        GenExpr cb, handler;
                        if (!gen_expr(cg, path, e->as.call.args[0], &cb, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &handler, err)) { gen_expr_free(&cb); return false; }
                        w_line(&cg->w, "cogito_checkbox_on_change(%s, %s);", cb.tmp, handler.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", cb.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", handler.tmp);
                        gen_expr_release_except(cg, &cb, cb.tmp);
                        gen_expr_release_except(cg, &handler, handler.tmp);
                        gen_expr_free(&cb);
                        gen_expr_free(&handler);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_switch_on_change")) {
                        GenExpr sw, handler;
                        if (!gen_expr(cg, path, e->as.call.args[0], &sw, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &handler, err)) { gen_expr_free(&sw); return false; }
                        w_line(&cg->w, "cogito_switch_on_change(%s, %s);", sw.tmp, handler.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", sw.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", handler.tmp);
                        gen_expr_release_except(cg, &sw, sw.tmp);
                        gen_expr_release_except(cg, &handler, handler.tmp);
                        gen_expr_free(&sw);
                        gen_expr_free(&handler);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_list_on_select")) {
                        GenExpr list, handler;
                        if (!gen_expr(cg, path, e->as.call.args[0], &list, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &handler, err)) { gen_expr_free(&list); return false; }
                        w_line(&cg->w, "cogito_list_on_select(%s, %s);", list.tmp, handler.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", list.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", handler.tmp);
                        gen_expr_release_except(cg, &list, list.tmp);
                        gen_expr_release_except(cg, &handler, handler.tmp);
                        gen_expr_free(&list);
                        gen_expr_free(&handler);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_list_on_activate")) {
                        GenExpr list, handler;
                        if (!gen_expr(cg, path, e->as.call.args[0], &list, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &handler, err)) { gen_expr_free(&list); return false; }
                        w_line(&cg->w, "cogito_list_on_activate(%s, %s);", list.tmp, handler.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", list.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", handler.tmp);
                        gen_expr_release_except(cg, &list, list.tmp);
                        gen_expr_release_except(cg, &handler, handler.tmp);
                        gen_expr_free(&list);
                        gen_expr_free(&handler);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_grid_on_select")) {
                        GenExpr grid, handler;
                        if (!gen_expr(cg, path, e->as.call.args[0], &grid, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &handler, err)) { gen_expr_free(&grid); return false; }
                        w_line(&cg->w, "cogito_grid_on_select(%s, %s);", grid.tmp, handler.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", grid.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", handler.tmp);
                        gen_expr_release_except(cg, &grid, grid.tmp);
                        gen_expr_release_except(cg, &handler, handler.tmp);
                        gen_expr_free(&grid);
                        gen_expr_free(&handler);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_grid_on_activate")) {
                        GenExpr grid, handler;
                        if (!gen_expr(cg, path, e->as.call.args[0], &grid, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &handler, err)) { gen_expr_free(&grid); return false; }
                        w_line(&cg->w, "cogito_grid_on_activate(%s, %s);", grid.tmp, handler.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", grid.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", handler.tmp);
                        gen_expr_release_except(cg, &grid, grid.tmp);
                        gen_expr_release_except(cg, &handler, handler.tmp);
                        gen_expr_free(&grid);
                        gen_expr_free(&handler);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_button_on_click")) {
                        GenExpr btn, handler;
                        if (!gen_expr(cg, path, e->as.call.args[0], &btn, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &handler, err)) { gen_expr_free(&btn); return false; }
                        w_line(&cg->w, "cogito_button_on_click(%s, %s);", btn.tmp, handler.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", btn.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", handler.tmp);
                        gen_expr_release_except(cg, &btn, btn.tmp);
                        gen_expr_release_except(cg, &handler, handler.tmp);
                        gen_expr_free(&btn);
                        gen_expr_free(&handler);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_button_add_menu")) {
                        GenExpr btn, label, handler;
                        if (!gen_expr(cg, path, e->as.call.args[0], &btn, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &label, err)) { gen_expr_free(&btn); return false; }
                        if (!gen_expr(cg, path, e->as.call.args[2], &handler, err)) { gen_expr_free(&btn); gen_expr_free(&label); return false; }
                        w_line(&cg->w, "cogito_button_add_menu(%s, %s, %s);", btn.tmp, label.tmp, handler.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", btn.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", label.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", handler.tmp);
                        gen_expr_release_except(cg, &btn, btn.tmp);
                        gen_expr_release_except(cg, &label, label.tmp);
                        gen_expr_release_except(cg, &handler, handler.tmp);
                        gen_expr_free(&btn);
                        gen_expr_free(&label);
                        gen_expr_free(&handler);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_appbar_add_button")) {
                        GenExpr app, text, handler;
                        if (!gen_expr(cg, path, e->as.call.args[0], &app, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &text, err)) { gen_expr_free(&app); return false; }
                        if (!gen_expr(cg, path, e->as.call.args[2], &handler, err)) { gen_expr_free(&app); gen_expr_free(&text); return false; }
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = cogito_appbar_add_button(%s, %s, %s);", t, app.tmp, text.tmp, handler.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", app.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", text.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", handler.tmp);
                        gen_expr_release_except(cg, &app, app.tmp);
                        gen_expr_release_except(cg, &text, text.tmp);
                        gen_expr_release_except(cg, &handler, handler.tmp);
                        gen_expr_free(&app);
                        gen_expr_free(&text);
                        gen_expr_free(&handler);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_appbar_set_controls")) {
                        GenExpr app, layout;
                        if (!gen_expr(cg, path, e->as.call.args[0], &app, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &layout, err)) { gen_expr_free(&app); return false; }
                        w_line(&cg->w, "cogito_appbar_set_controls(%s, %s);", app.tmp, layout.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", app.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", layout.tmp);
                        gen_expr_release_except(cg, &app, app.tmp);
                        gen_expr_release_except(cg, &layout, layout.tmp);
                        gen_expr_free(&app);
                        gen_expr_free(&layout);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_iconbtn_add_menu")) {
                        GenExpr btn, label, handler;
                        if (!gen_expr(cg, path, e->as.call.args[0], &btn, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &label, err)) { gen_expr_free(&btn); return false; }
                        if (!gen_expr(cg, path, e->as.call.args[2], &handler, err)) { gen_expr_free(&btn); gen_expr_free(&label); return false; }
                        w_line(&cg->w, "cogito_iconbtn_add_menu(%s, %s, %s);", btn.tmp, label.tmp, handler.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", btn.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", label.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", handler.tmp);
                        gen_expr_release_except(cg, &btn, btn.tmp);
                        gen_expr_release_except(cg, &label, label.tmp);
                        gen_expr_release_except(cg, &handler, handler.tmp);
                        gen_expr_free(&btn);
                        gen_expr_free(&label);
                        gen_expr_free(&handler);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_run")) {
                        GenExpr app, win;
                        if (!gen_expr(cg, path, e->as.call.args[0], &app, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &win, err)) { gen_expr_free(&app); return false; }
                        w_line(&cg->w, "cogito_run(%s, %s);", app.tmp, win.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", app.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", win.tmp);
                        gen_expr_release_except(cg, &app, app.tmp);
                        gen_expr_release_except(cg, &win, win.tmp);
                        gen_expr_free(&app);
                        gen_expr_free(&win);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__cogito_load_sum")) {
                        GenExpr pathv;
                        if (!gen_expr(cg, path, e->as.call.args[0], &pathv, err)) return false;
                        w_line(&cg->w, "cogito_load_sum(%s);", pathv.tmp);
                        w_line(&cg->w, "ergo_release_val(%s);", pathv.tmp);
                        gen_expr_release_except(cg, &pathv, pathv.tmp);
                        gen_expr_free(&pathv);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    FunSig *sig = codegen_fun_sig(cg, cg->current_module, fname);
                    if (!sig && is_stdr_prelude(fname)) {
                        bool allow = str_eq_c(cg->current_module, "stdr");
                        for (size_t i = 0; i < cg->current_imports_len && !allow; i++) {
                            if (str_eq_c(cg->current_imports[i], "stdr")) allow = true;
                        }
                        if (allow) sig = codegen_fun_sig(cg, str_from_c("stdr"), fname);
                    }
                    if (sig) {
                        VEC(char *) arg_ts = VEC_INIT;
                        for (size_t i = 0; i < e->as.call.args_len; i++) {
                            GenExpr ge;
                            if (!gen_expr(cg, path, e->as.call.args[i], &ge, err)) { VEC_FREE(arg_ts); return false; }
                            VEC_PUSH(arg_ts, ge.tmp);
                            gen_expr_release_except(cg, &ge, ge.tmp);
                            gen_expr_free(&ge);
                        }
                        bool ret_void = sig->ret && sig->ret->tag == TY_VOID;
                        char *mangled = mangle_global(cg->arena, sig->module, fname);
                        if (ret_void) {
                            StrBuf line; sb_init(&line);
                            sb_appendf(&line, "%s(", mangled);
                            for (size_t i = 0; i < arg_ts.len; i++) {
                                if (i) sb_append(&line, ", ");
                                sb_append(&line, arg_ts.data[i]);
                            }
                            sb_append(&line, ");");
                            w_line(&cg->w, "%s", line.data ? line.data : "");
                            sb_free(&line);
                            for (size_t i = 0; i < arg_ts.len; i++) {
                                w_line(&cg->w, "ergo_release_val(%s);", arg_ts.data[i]);
                            }
                            VEC_FREE(arg_ts);
                            char *t = codegen_new_tmp(cg);
                            w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
                            gen_expr_add(out, t);
                            out->tmp = t;
                            return true;
                        }
                        char *t = codegen_new_tmp(cg);
                        StrBuf line; sb_init(&line);
                        sb_appendf(&line, "ErgoVal %s = %s(", t, mangled);
                        for (size_t i = 0; i < arg_ts.len; i++) {
                            if (i) sb_append(&line, ", ");
                            sb_append(&line, arg_ts.data[i]);
                        }
                        sb_append(&line, ");");
                        w_line(&cg->w, "%s", line.data ? line.data : "");
                        sb_free(&line);
                        for (size_t i = 0; i < arg_ts.len; i++) {
                            w_line(&cg->w, "ergo_release_val(%s);", arg_ts.data[i]);
                        }
                        VEC_FREE(arg_ts);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                }
            }

            // function value call
            GenExpr ft;
            if (!gen_expr(cg, path, fn, &ft, err)) return false;
            VEC(char *) arg_ts = VEC_INIT;
            for (size_t i = 0; i < e->as.call.args_len; i++) {
                GenExpr ge;
                if (!gen_expr(cg, path, e->as.call.args[i], &ge, err)) { gen_expr_free(&ft); VEC_FREE(arg_ts); return false; }
                VEC_PUSH(arg_ts, ge.tmp);
                gen_expr_release_except(cg, &ge, ge.tmp);
                gen_expr_free(&ge);
            }
            char *t = codegen_new_tmp(cg);
            w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
            if (arg_ts.len > 0) {
                char *argv_name = codegen_new_sym(cg, "argv");
                StrBuf line; sb_init(&line);
                sb_appendf(&line, "ErgoVal %s[%zu] = { ", argv_name, arg_ts.len);
                for (size_t i = 0; i < arg_ts.len; i++) {
                    if (i) sb_append(&line, ", ");
                    sb_append(&line, arg_ts.data[i]);
                }
                sb_append(&line, " };");
                w_line(&cg->w, "{");
                cg->w.indent++;
                w_line(&cg->w, "%s", line.data ? line.data : "");
                w_line(&cg->w, "%s = ergo_call(%s, %zu, %s);", t, ft.tmp, arg_ts.len, argv_name);
                cg->w.indent--;
                w_line(&cg->w, "}");
                sb_free(&line);
            } else {
                w_line(&cg->w, "%s = ergo_call(%s, 0, NULL);", t, ft.tmp);
            }
            w_line(&cg->w, "ergo_release_val(%s);", ft.tmp);
            gen_expr_release_except(cg, &ft, ft.tmp);
            gen_expr_free(&ft);
            for (size_t i = 0; i < arg_ts.len; i++) {
                w_line(&cg->w, "ergo_release_val(%s);", arg_ts.data[i]);
            }
            VEC_FREE(arg_ts);
            gen_expr_add(out, t);
            out->tmp = t;
            return true;
        }
        case EXPR_BLOCK: {
            char *t = codegen_new_tmp(cg);
            if (!t) return cg_set_err(err, path, "out of memory");
            w_line(&cg->w, "ErgoVal %s = EV_NULLV;", t);
            if (!gen_stmt(cg, path, e->as.block_expr.block, false, err)) return false;
            gen_expr_add(out, t);
            out->tmp = t;
            return true;
        }
        case EXPR_PAREN:
            return gen_expr(cg, path, e->as.paren.x, out, err);
        default:
            break;
    }
    return cg_set_err(err, path, "unhandled expr in codegen");
}

// -----------------
// Statement generation
// -----------------

static bool gen_block(Codegen *cg, Str path, Stmt *b, bool ret_void, Diag *err);

static bool gen_if_chain(Codegen *cg, Str path, IfArm **arms, size_t idx, size_t arms_len, bool ret_void, Diag *err) {
    if (idx >= arms_len) return true;
    IfArm *arm = arms[idx];
    if (!arm->cond) {
        if (arm->body->kind == STMT_BLOCK) {
            return gen_block(cg, path, arm->body, ret_void, err);
        }
        return gen_block(cg, path, arm->body, ret_void, err);
    }
    GenExpr cond;
    if (!gen_expr(cg, path, arm->cond, &cond, err)) return false;
    cg->var_id++;
    char *bname = arena_printf(cg->arena, "__b%d", cg->var_id);
    w_line(&cg->w, "bool %s = ergo_as_bool(%s);", bname, cond.tmp);
    w_line(&cg->w, "ergo_release_val(%s);", cond.tmp);
    gen_expr_release_except(cg, &cond, cond.tmp);
    gen_expr_free(&cond);

    w_line(&cg->w, "if (%s) {", bname);
    cg->w.indent++;
    codegen_push_scope(cg);
    if (arm->body->kind == STMT_BLOCK) {
        if (!gen_block(cg, path, arm->body, ret_void, err)) return false;
    } else {
        if (!gen_block(cg, path, arm->body, ret_void, err)) return false;
    }
    LocalList locals = codegen_pop_scope(cg);
    codegen_release_scope(cg, locals);
    cg->w.indent--;

    if (idx + 1 < arms_len) {
        w_line(&cg->w, "} else {");
        cg->w.indent++;
        if (!gen_if_chain(cg, path, arms, idx + 1, arms_len, ret_void, err)) return false;
        cg->w.indent--;
        w_line(&cg->w, "}");
    } else {
        w_line(&cg->w, "}");
    }
    return true;
}

static bool gen_block(Codegen *cg, Str path, Stmt *b, bool ret_void, Diag *err) {
    if (!b) return true;
    if (b->kind != STMT_BLOCK) {
        return gen_stmt(cg, path, b, ret_void, err);
    }
    for (size_t i = 0; i < b->as.block_s.stmts_len; i++) {
        if (!gen_stmt(cg, path, b->as.block_s.stmts[i], ret_void, err)) return false;
    }
    return true;
}

static bool gen_stmt(Codegen *cg, Str path, Stmt *s, bool ret_void, Diag *err) {
    if (!s) return true;
    switch (s->kind) {
        case STMT_LET: {
            Ty *ty = cg_tc_expr(cg, path, s->as.let_s.expr, err);
            if (!ty) return false;
            char *cvar = codegen_define_local(cg, s->as.let_s.name, ty, s->as.let_s.is_mut, false);
            if (!cvar) return cg_set_err(err, path, "out of memory");
            w_line(&cg->w, "ErgoVal %s = EV_NULLV;", cvar);
            GenExpr ge;
            if (!gen_expr(cg, path, s->as.let_s.expr, &ge, err)) return false;
            w_line(&cg->w, "ergo_move_into(&%s, %s);", cvar, ge.tmp);
            gen_expr_release_except(cg, &ge, ge.tmp);
            gen_expr_free(&ge);
            return true;
        }
        case STMT_CONST: {
            Ty *ty = cg_tc_expr(cg, path, s->as.const_s.expr, err);
            if (!ty) return false;
            char *cvar = codegen_define_local(cg, s->as.const_s.name, ty, false, true);
            if (!cvar) return cg_set_err(err, path, "out of memory");
            w_line(&cg->w, "ErgoVal %s = EV_NULLV;", cvar);
            GenExpr ge;
            if (!gen_expr(cg, path, s->as.const_s.expr, &ge, err)) return false;
            w_line(&cg->w, "ergo_move_into(&%s, %s);", cvar, ge.tmp);
            gen_expr_release_except(cg, &ge, ge.tmp);
            gen_expr_free(&ge);
            return true;
        }
        case STMT_EXPR: {
            GenExpr ge;
            if (!gen_expr(cg, path, s->as.expr_s.expr, &ge, err)) return false;
            w_line(&cg->w, "ergo_release_val(%s);", ge.tmp);
            gen_expr_release_except(cg, &ge, ge.tmp);
            gen_expr_free(&ge);
            return true;
        }
        case STMT_RETURN: {
            if (ret_void) {
                if (s->as.ret_s.expr) {
                    GenExpr ge;
                    if (!gen_expr(cg, path, s->as.ret_s.expr, &ge, err)) return false;
                    w_line(&cg->w, "ergo_release_val(%s);", ge.tmp);
                    gen_expr_release_except(cg, &ge, ge.tmp);
                    gen_expr_free(&ge);
                }
                w_line(&cg->w, "return;");
            } else {
                if (!s->as.ret_s.expr) {
                    w_line(&cg->w, "__ret = EV_NULLV;");
                } else {
                    GenExpr ge;
                    if (!gen_expr(cg, path, s->as.ret_s.expr, &ge, err)) return false;
                    w_line(&cg->w, "ergo_move_into(&__ret, %s);", ge.tmp);
                    gen_expr_release_except(cg, &ge, ge.tmp);
                    gen_expr_free(&ge);
                }
                w_line(&cg->w, "return __ret;");
            }
            return true;
        }
        case STMT_IF: {
            return gen_if_chain(cg, path, s->as.if_s.arms, 0, s->as.if_s.arms_len, ret_void, err);
        }
        case STMT_FOR: {
            if (s->as.for_s.init) {
                if (!gen_stmt(cg, path, s->as.for_s.init, ret_void, err)) return false;
            }
            w_line(&cg->w, "for (;;) {");
            cg->w.indent++;
            if (s->as.for_s.cond) {
                GenExpr ct;
                if (!gen_expr(cg, path, s->as.for_s.cond, &ct, err)) return false;
                cg->var_id++;
                char *bname = arena_printf(cg->arena, "__b%d", cg->var_id);
                w_line(&cg->w, "bool %s = ergo_as_bool(%s);", bname, ct.tmp);
                w_line(&cg->w, "ergo_release_val(%s);", ct.tmp);
                gen_expr_release_except(cg, &ct, ct.tmp);
                gen_expr_free(&ct);
                w_line(&cg->w, "if (!%s) {", bname);
                cg->w.indent++;
                w_line(&cg->w, "break;");
                cg->w.indent--;
                w_line(&cg->w, "}");
            }
            codegen_push_scope(cg);
            if (s->as.for_s.body->kind == STMT_BLOCK) {
                if (!gen_block(cg, path, s->as.for_s.body, ret_void, err)) return false;
            } else {
                if (!gen_stmt(cg, path, s->as.for_s.body, ret_void, err)) return false;
            }
            LocalList locals = codegen_pop_scope(cg);
            codegen_release_scope(cg, locals);
            if (s->as.for_s.step) {
                GenExpr st;
                if (!gen_expr(cg, path, s->as.for_s.step, &st, err)) return false;
                w_line(&cg->w, "ergo_release_val(%s);", st.tmp);
                gen_expr_release_except(cg, &st, st.tmp);
                gen_expr_free(&st);
            }
            cg->w.indent--;
            w_line(&cg->w, "}");
            return true;
        }
        case STMT_FOREACH: {
            GenExpr it;
            if (!gen_expr(cg, path, s->as.foreach_s.expr, &it, err)) return false;
            char *idx_name = codegen_new_sym(cg, "i");
            char *len_name = codegen_new_sym(cg, "len");

            Ty *elem_ty = cg_tc_expr(cg, path, s->as.foreach_s.expr, err);
            if (!elem_ty) return false;
            Ty *ety = NULL;
            if (elem_ty->tag == TY_ARRAY && elem_ty->elem) {
                ety = elem_ty->elem;
            } else if (elem_ty->tag == TY_PRIM && str_eq_c(elem_ty->name, "string")) {
                ety = elem_ty; // string
            } else {
                return cg_set_err(err, path, "foreach expects array or string");
            }

            codegen_push_scope(cg);
            char *cvar = codegen_define_local(cg, s->as.foreach_s.name, ety, false, false);
            w_line(&cg->w, "ErgoVal %s = EV_NULLV;", cvar);

            w_line(&cg->w, "int %s = stdr_len(%s);", len_name, it.tmp);
            w_line(&cg->w, "for (int %s = 0; %s < %s; %s++) {", idx_name, idx_name, len_name, idx_name);
            cg->w.indent++;
            codegen_push_scope(cg);

            if (elem_ty->tag == TY_ARRAY) {
                w_line(&cg->w, "ErgoVal __e = ergo_arr_get((ErgoArr*)%s.as.p, %s);", it.tmp, idx_name);
            } else {
                w_line(&cg->w, "ErgoVal __e = stdr_str_at(%s, %s);", it.tmp, idx_name);
            }
            w_line(&cg->w, "ergo_move_into(&%s, __e);", cvar);

            if (s->as.foreach_s.body->kind == STMT_BLOCK) {
                if (!gen_block(cg, path, s->as.foreach_s.body, ret_void, err)) return false;
            } else {
                if (!gen_stmt(cg, path, s->as.foreach_s.body, ret_void, err)) return false;
            }

            LocalList inner_locals = codegen_pop_scope(cg);
            codegen_release_scope(cg, inner_locals);
            cg->w.indent--;
            w_line(&cg->w, "}");

            LocalList outer_locals = codegen_pop_scope(cg);
            codegen_release_scope(cg, outer_locals);
            w_line(&cg->w, "ergo_release_val(%s);", it.tmp);
            gen_expr_release_except(cg, &it, it.tmp);
            gen_expr_free(&it);
            return true;
        }
        case STMT_BLOCK: {
            w_line(&cg->w, "{");
            cg->w.indent++;
            codegen_push_scope(cg);
            if (!gen_block(cg, path, s, ret_void, err)) return false;
            LocalList locals = codegen_pop_scope(cg);
            codegen_release_scope(cg, locals);
            cg->w.indent--;
            w_line(&cg->w, "}");
            return true;
        }
        default:
            return cg_set_err(err, path, "unhandled stmt in codegen");
    }
}

// -----------------
// Function generation
// -----------------

static bool gen_class_defs(Codegen *cg, Diag *err) {
    (void)err;
    for (size_t i = 0; i < cg->class_decls_len; i++) {
        ClassDeclEntry *ce = &cg->class_decls[i];
        Str mod, name;
        split_qname(ce->qname, &mod, &name);
        char *cname = codegen_c_class_name(cg, ce->qname);
        char *drop_sym = mod.len ? arena_printf(cg->arena, "ergo_drop_%s_%.*s", mangle_mod(cg->arena, mod), (int)name.len, name.data)
                                  : arena_printf(cg->arena, "ergo_drop_%.*s", (int)name.len, name.data);
        w_line(&cg->w, "typedef struct %s {", cname);
        cg->w.indent++;
        w_line(&cg->w, "ErgoObj base;");
        for (size_t f = 0; f < ce->decl->fields_len; f++) {
            FieldDecl *fd = ce->decl->fields[f];
            w_line(&cg->w, "ErgoVal %s;", codegen_c_field_name(cg, fd->name));
        }
        cg->w.indent--;
        w_line(&cg->w, "} %s;", cname);
        w_line(&cg->w, "static void %s(ErgoObj* o);", drop_sym);
        w_line(&cg->w, "");
    }
    for (size_t i = 0; i < cg->class_decls_len; i++) {
        ClassDeclEntry *ce = &cg->class_decls[i];
        Str mod, name;
        split_qname(ce->qname, &mod, &name);
        char *cname = codegen_c_class_name(cg, ce->qname);
        char *drop_sym = mod.len ? arena_printf(cg->arena, "ergo_drop_%s_%.*s", mangle_mod(cg->arena, mod), (int)name.len, name.data)
                                  : arena_printf(cg->arena, "ergo_drop_%.*s", (int)name.len, name.data);
        w_line(&cg->w, "static void %s(ErgoObj* o) {", drop_sym);
        cg->w.indent++;
        w_line(&cg->w, "%s* self = (%s*)o;", cname, cname);
        for (size_t f = 0; f < ce->decl->fields_len; f++) {
            FieldDecl *fd = ce->decl->fields[f];
            w_line(&cg->w, "ergo_release_val(self->%s);", codegen_c_field_name(cg, fd->name));
        }
        cg->w.indent--;
        w_line(&cg->w, "}");
        w_line(&cg->w, "");
    }
    return true;
}

static char *c_params(size_t count, bool leading_comma) {
    if (count == 0) return dup_cstr(leading_comma ? "" : "void");
    StrBuf b; sb_init(&b);
    for (size_t i = 0; i < count; i++) {
        if (i) sb_append(&b, ", ");
        sb_appendf(&b, "ErgoVal a%zu", i);
    }
    char *out = b.data ? dup_cstr(b.data) : NULL;
    sb_free(&b);
    if (!out) return dup_cstr(leading_comma ? "" : "void");
    if (leading_comma) {
        StrBuf b2; sb_init(&b2);
        sb_appendf(&b2, ", %s", out);
        free(out);
        out = b2.data ? dup_cstr(b2.data) : NULL;
        sb_free(&b2);
        return out ? out : dup_cstr("");
    }
    return out;
}

static bool gen_method(Codegen *cg, Str path, ClassDecl *cls, FunDecl *fn, Diag *err) {
    cg->scopes_len = 0;
    cg->scope_locals_len = 0;
    locals_free(&cg->ty_loc);
    locals_init(&cg->ty_loc);
    codegen_push_scope(cg);

    Str qname = cls->name;
    if (cg->current_module.len) {
        size_t len = cg->current_module.len + 1 + cls->name.len;
        char *buf = (char *)arena_alloc(cg->arena, len + 1);
        if (!buf) return cg_set_err(err, path, "out of memory");
        memcpy(buf, cg->current_module.data, cg->current_module.len);
        buf[cg->current_module.len] = '.';
        memcpy(buf + cg->current_module.len + 1, cls->name.data, cls->name.len);
        buf[len] = '\0';
        qname.data = buf;
        qname.len = len;
    }
    cg->current_class = qname;
    cg->has_current_class = true;

    // receiver
    if (fn->params_len > 0) {
        codegen_add_name(cg, fn->params[0]->name, "self");
        Ty *self_ty = (Ty *)arena_alloc(cg->arena, sizeof(Ty));
        if (!self_ty) return cg_set_err(err, path, "out of memory");
        memset(self_ty, 0, sizeof(Ty));
        self_ty->tag = TY_CLASS;
        self_ty->name = qname;
        Binding b = { self_ty, fn->params[0]->is_mut, false };
        locals_define(&cg->ty_loc, fn->params[0]->name, b);
    }

    ClassInfo *ci = codegen_class_info(cg, qname);
    FunSig *sig = NULL;
    if (ci) {
        for (size_t i = 0; i < ci->methods_len; i++) {
            if (str_eq(ci->methods[i].name, fn->name)) {
                sig = ci->methods[i].sig;
                break;
            }
        }
    }

    // params after this
    for (size_t i = 1; i < fn->params_len; i++) {
        Param *p = fn->params[i];
        char *cname = arena_printf(cg->arena, "a%zu", i - 1);
        codegen_add_name(cg, p->name, cname);
        Ty *pty = sig ? sig->params[i - 1] : NULL;
        Binding b = { pty, p->is_mut, false };
        locals_define(&cg->ty_loc, p->name, b);
    }

    bool ret_void = fn->ret.is_void;
    const char *ret_ty = ret_void ? "void" : "ErgoVal";
    char *params = c_params(fn->params_len > 0 ? fn->params_len - 1 : 0, true);
    char *mangled = mangle_method(cg->arena, cg->current_module, cls->name, fn->name);
    w_line(&cg->w, "static %s %s(ErgoVal self%s) {", ret_ty, mangled, params ? params : "");
    cg->w.indent++;
    if (!ret_void) {
        w_line(&cg->w, "ErgoVal __ret = EV_NULLV;");
    }

    if (!gen_block(cg, path, fn->body, ret_void, err)) return false;
    {
        LocalList locals = codegen_pop_scope(cg);
        codegen_release_scope(cg, locals);
    }

    if (!ret_void) {
        w_line(&cg->w, "return __ret;");
    }
    cg->w.indent--;
    w_line(&cg->w, "}");
    w_line(&cg->w, "");
    if (params) free(params);
    cg->has_current_class = false;
    return true;
}

static bool gen_fun(Codegen *cg, Str path, FunDecl *fn, Diag *err) {
    cg->scopes_len = 0;
    cg->scope_locals_len = 0;
    locals_free(&cg->ty_loc);
    locals_init(&cg->ty_loc);
    codegen_push_scope(cg);
    cg->has_current_class = false;

    FunSig *sig = codegen_fun_sig(cg, cg->current_module, fn->name);

    for (size_t i = 0; i < fn->params_len; i++) {
        Param *p = fn->params[i];
        char *cname = arena_printf(cg->arena, "a%zu", i);
        codegen_add_name(cg, p->name, cname);
        Ty *pty = sig ? sig->params[i] : NULL;
        Binding b = { pty, p->is_mut, false };
        locals_define(&cg->ty_loc, p->name, b);
    }

    bool ret_void = fn->ret.is_void;
    const char *ret_ty = ret_void ? "void" : "ErgoVal";
    char *params = c_params(fn->params_len, false);
    char *mangled = mangle_global(cg->arena, cg->current_module, fn->name);
    w_line(&cg->w, "static %s %s(%s) {", ret_ty, mangled, params ? params : "void");
    cg->w.indent++;
    if (!ret_void) {
        w_line(&cg->w, "ErgoVal __ret = EV_NULLV;");
    }

    if (!gen_block(cg, path, fn->body, ret_void, err)) return false;
    {
        LocalList locals = codegen_pop_scope(cg);
        codegen_release_scope(cg, locals);
    }

    if (!ret_void) {
        w_line(&cg->w, "return __ret;");
    }
    cg->w.indent--;
    w_line(&cg->w, "}");
    w_line(&cg->w, "");
    if (params) free(params);
    return true;
}

static bool gen_entry(Codegen *cg, Diag *err) {
    EntryDecl *entry_decl = NULL;
    Str entry_path = {NULL, 0};
    for (size_t i = 0; i < cg->prog->mods_len; i++) {
        Module *m = cg->prog->mods[i];
        for (size_t j = 0; j < m->decls_len; j++) {
            if (m->decls[j]->kind == DECL_ENTRY) {
                entry_decl = &m->decls[j]->as.entry;
                entry_path = m->path;
            }
        }
    }
    if (!entry_decl) {
        return cg_set_err(err, (Str){NULL, 0}, "missing entry()");
    }

    cg->scopes_len = 0;
    cg->scope_locals_len = 0;
    locals_free(&cg->ty_loc);
    locals_init(&cg->ty_loc);
    codegen_push_scope(cg);

    if (entry_path.data) {
        Str mod_name = codegen_module_name(cg, entry_path);
        cg->current_module = mod_name;
        ModuleImport *mi = codegen_module_imports(cg, mod_name);
        cg->current_imports = mi ? mi->imports : NULL;
        cg->current_imports_len = mi ? mi->imports_len : 0;
    }

    w_line(&cg->w, "static void ergo_entry(void) {");
    cg->w.indent++;
    for (size_t i = 0; i < cg->prog->mods_len; i++) {
        Module *m = cg->prog->mods[i];
        Str mod_name = codegen_module_name(cg, m->path);
        ModuleGlobals *mg = codegen_module_globals(cg, mod_name);
        if (mg && mg->len > 0) {
            char *init_name = mangle_global_init(cg->arena, mod_name);
            w_line(&cg->w, "%s();", init_name);
        }
    }
    if (!gen_block(cg, entry_path, entry_decl->body, true, err)) return false;
    {
        LocalList locals = codegen_pop_scope(cg);
        codegen_release_scope(cg, locals);
    }
    cg->w.indent--;
    w_line(&cg->w, "}");
    w_line(&cg->w, "");
    return true;
}

// -----------------
// Codegen top-level
// -----------------

static bool codegen_init(Codegen *cg, Program *prog, Arena *arena, Diag *err) {
    memset(cg, 0, sizeof(*cg));
    cg->prog = prog;
    cg->arena = arena;
    sb_init(&cg->out);
    cg->w.buf = &cg->out;
    cg->w.indent = 0;

    cg->env = build_global_env(prog, arena, err);
    if (!cg->env) {
        return false;
    }

    // build class decl map
    for (size_t i = 0; i < prog->mods_len; i++) {
        Module *m = prog->mods[i];
        Str mod_name = codegen_module_name(cg, m->path);
        for (size_t j = 0; j < m->decls_len; j++) {
            Decl *d = m->decls[j];
            if (d->kind == DECL_CLASS) {
                Str qname = {NULL, 0};
                char *buf = arena_printf(arena, "%.*s.%.*s", (int)mod_name.len, mod_name.data, (int)d->as.class_decl.name.len, d->as.class_decl.name.data);
                if (buf) {
                    qname.data = buf;
                    qname.len = mod_name.len + 1 + d->as.class_decl.name.len;
                }
                if (cg->class_decls_len + 1 > cg->class_decls_cap) {
                    size_t next = cg->class_decls_cap ? cg->class_decls_cap * 2 : 8;
                    ClassDeclEntry *arr = (ClassDeclEntry *)realloc(cg->class_decls, next * sizeof(ClassDeclEntry));
                    if (!arr) return cg_set_err(err, m->path, "out of memory");
                    cg->class_decls = arr;
                    cg->class_decls_cap = next;
                }
                cg->class_decls[cg->class_decls_len].qname = qname;
                cg->class_decls[cg->class_decls_len].decl = &d->as.class_decl;
                cg->class_decls_len++;
            }
        }
    }

    locals_init(&cg->ty_loc);
    codegen_push_scope(cg);

    return true;
}

static void codegen_free(Codegen *cg) {
    for (size_t i = 0; i < cg->scopes_len; i++) {
        free(cg->scopes[i].items);
    }
    free(cg->scopes);
    for (size_t i = 0; i < cg->scope_locals_len; i++) {
        free(cg->scope_locals[i].items);
    }
    free(cg->scope_locals);
    free(cg->lambdas);
    free(cg->funvals);
    free(cg->class_decls);
    locals_free(&cg->ty_loc);
    sb_free(&cg->out);
}

static bool codegen_gen(Codegen *cg, Diag *err) {
    codegen_collect_lambdas(cg);

    const char *runtime_path = getenv("ERGO_RUNTIME");
    if (!runtime_path || !runtime_path[0]) {
        if (path_is_file("src/ergo/runtime.inc")) {
            runtime_path = "src/ergo/runtime.inc";
        } else if (path_is_file("../src/ergo/runtime.inc")) {
            runtime_path = "../src/ergo/runtime.inc";
        } else {
            runtime_path = "src/ergo/runtime.inc";
        }
    }
    Arena tmp_arena;
    arena_init(&tmp_arena);
    size_t runtime_len = 0;
    Diag rerr = {0};
    char *runtime_src = read_file_with_includes(runtime_path, "// @include", &tmp_arena, &runtime_len, &rerr);
    if (!runtime_src) {
        arena_free(&tmp_arena);
        return cg_set_err(err, (Str){runtime_path, runtime_path ? strlen(runtime_path) : 0}, "failed to read runtime.inc");
    }
    w_raw(&cg->w, runtime_src);
    if (runtime_len == 0 || runtime_src[runtime_len - 1] != '\n') {
        sb_append_char(&cg->out, '\n');
    }
    arena_free(&tmp_arena);

    w_line(&cg->w, "// ---- module globals ----");
    for (size_t i = 0; i < cg->prog->mods_len; i++) {
        Module *m = cg->prog->mods[i];
        Str mod_name = codegen_module_name(cg, m->path);
        ModuleGlobals *mg = codegen_module_globals(cg, mod_name);
        if (!mg || mg->len == 0) continue;
        for (size_t j = 0; j < m->decls_len; j++) {
            Decl *d = m->decls[j];
            if (d->kind != DECL_DEF) continue;
            char *gname = mangle_global_var(cg->arena, mod_name, d->as.def_decl.name);
            w_line(&cg->w, "static ErgoVal %s = EV_NULLV;", gname);
        }
    }
    w_line(&cg->w, "");

    w_line(&cg->w, "// ---- class definitions ----");
    if (!gen_class_defs(cg, err)) return false;
    w_line(&cg->w, "");

    if (cg->lambdas_len > 0) {
        w_line(&cg->w, "// ---- lambda forward decls ----");
        for (size_t i = 0; i < cg->lambdas_len; i++) {
            w_line(&cg->w, "static ErgoVal %s(void* env, int argc, ErgoVal* argv);", cg->lambdas[i].name);
        }
        w_line(&cg->w, "");
    }

    if (cg->funvals_len > 0) {
        w_line(&cg->w, "// ---- function value forward decls ----");
        for (size_t i = 0; i < cg->funvals_len; i++) {
            w_line(&cg->w, "static ErgoVal %s(void* env, int argc, ErgoVal* argv);", cg->funvals[i].wrapper);
        }
        w_line(&cg->w, "");
    }

    w_line(&cg->w, "// ---- forward decls ----");
    for (size_t i = 0; i < cg->prog->mods_len; i++) {
        Module *m = cg->prog->mods[i];
        Str mod_name = codegen_module_name(cg, m->path);
        for (size_t j = 0; j < m->decls_len; j++) {
            Decl *d = m->decls[j];
            if (d->kind == DECL_CLASS) {
                for (size_t k = 0; k < d->as.class_decl.methods_len; k++) {
                    FunDecl *md = d->as.class_decl.methods[k];
                    const char *ret_ty = md->ret.is_void ? "void" : "ErgoVal";
                    char *params = c_params(md->params_len > 0 ? md->params_len - 1 : 0, true);
                    char *mangled = mangle_method(cg->arena, mod_name, d->as.class_decl.name, md->name);
                    w_line(&cg->w, "static %s %s(ErgoVal self%s);", ret_ty, mangled, params ? params : "");
                    if (params) free(params);
                }
            }
            if (d->kind == DECL_FUN) {
                const char *ret_ty = d->as.fun.ret.is_void ? "void" : "ErgoVal";
                char *params = c_params(d->as.fun.params_len, false);
                char *mangled = mangle_global(cg->arena, mod_name, d->as.fun.name);
                w_line(&cg->w, "static %s %s(%s);", ret_ty, mangled, params ? params : "void");
                if (params) free(params);
            }
        }
        ModuleGlobals *mg = codegen_module_globals(cg, mod_name);
        if (mg && mg->len > 0) {
            char *init_name = mangle_global_init(cg->arena, mod_name);
            w_line(&cg->w, "static void %s(void);", init_name);
        }
    }
    w_line(&cg->w, "static void ergo_entry(void);");
    w_line(&cg->w, "");

    if (cg->lambdas_len > 0) {
        w_line(&cg->w, "// ---- lambda defs ----");
        // generate lambda definitions
        for (size_t i = 0; i < cg->lambdas_len; i++) {
            LambdaInfo *li = &cg->lambdas[i];
            // save state
            NameScope *saved_scopes = cg->scopes;
            size_t saved_scopes_len = cg->scopes_len;
            size_t saved_scopes_cap = cg->scopes_cap;
            LocalList *saved_locals = cg->scope_locals;
            size_t saved_locals_len = cg->scope_locals_len;
            size_t saved_locals_cap = cg->scope_locals_cap;
            Locals saved_ty = cg->ty_loc;
            Str saved_mod = cg->current_module;
            Str *saved_imports = cg->current_imports;
            size_t saved_imports_len = cg->current_imports_len;
            Str saved_class = cg->current_class;
            bool saved_has_class = cg->has_current_class;
            int saved_indent = cg->w.indent;

            cg->scopes = NULL;
            cg->scopes_len = 0;
            cg->scopes_cap = 0;
            cg->scope_locals = NULL;
            cg->scope_locals_len = 0;
            cg->scope_locals_cap = 0;
            locals_init(&cg->ty_loc);
            codegen_push_scope(cg);
            cg->w.indent = 0;

            // set module context
            Str mod_name = codegen_module_name(cg, li->path);
            cg->current_module = mod_name;
            ModuleImport *mi = codegen_module_imports(cg, mod_name);
            cg->current_imports = mi ? mi->imports : NULL;
            cg->current_imports_len = mi ? mi->imports_len : 0;
            cg->has_current_class = false;

            // emit lambda
            w_line(&cg->w, "static ErgoVal %s(void* env, int argc, ErgoVal* argv) {", li->name);
            cg->w.indent++;
            w_line(&cg->w, "(void)env;");
            w_line(&cg->w, "if (argc != %zu) ergo_trap(\"lambda arity mismatch\");", li->lam->as.lambda.params_len);
            for (size_t p = 0; p < li->lam->as.lambda.params_len; p++) {
                Param *param = li->lam->as.lambda.params[p];
                char *cname = arena_printf(cg->arena, "arg%zu", p);
                w_line(&cg->w, "ErgoVal %s = argv[%zu];", cname, p);
                codegen_add_name(cg, param->name, cname);
                Ty *pty = NULL;
                if (param->typ) {
                    pty = cg_ty_from_type_ref(cg, param->typ, cg->current_module, cg->current_imports, cg->current_imports_len, err);
                } else {
                    pty = cg_ty_gen(cg, param->name);
                }
                Binding b = { pty, param->is_mut, false };
                locals_define(&cg->ty_loc, param->name, b);
            }
            w_line(&cg->w, "ErgoVal __ret = EV_NULLV;");
            GenExpr ge;
            if (!gen_expr(cg, li->path, li->lam->as.lambda.body, &ge, err)) return false;
            w_line(&cg->w, "ergo_move_into(&__ret, %s);", ge.tmp);
            gen_expr_release_except(cg, &ge, ge.tmp);
            gen_expr_free(&ge);
            {
                LocalList locals = codegen_pop_scope(cg);
                codegen_release_scope(cg, locals);
            }
            w_line(&cg->w, "return __ret;");
            cg->w.indent--;
            w_line(&cg->w, "}");
            w_line(&cg->w, "");

            // cleanup lambda state
            for (size_t si = 0; si < cg->scopes_len; si++) {
                free(cg->scopes[si].items);
            }
            free(cg->scopes);
            for (size_t lii = 0; lii < cg->scope_locals_len; lii++) {
                free(cg->scope_locals[lii].items);
            }
            free(cg->scope_locals);
            locals_free(&cg->ty_loc);

            // restore state
            cg->scopes = saved_scopes;
            cg->scopes_len = saved_scopes_len;
            cg->scopes_cap = saved_scopes_cap;
            cg->scope_locals = saved_locals;
            cg->scope_locals_len = saved_locals_len;
            cg->scope_locals_cap = saved_locals_cap;
            cg->ty_loc = saved_ty;
            cg->current_module = saved_mod;
            cg->current_imports = saved_imports;
            cg->current_imports_len = saved_imports_len;
            cg->current_class = saved_class;
            cg->has_current_class = saved_has_class;
            cg->w.indent = saved_indent;
        }
        w_line(&cg->w, "");
    }

    if (cg->funvals_len > 0) {
        w_line(&cg->w, "// ---- function value defs ----");
        for (size_t i = 0; i < cg->funvals_len; i++) {
            FunValInfo *fi = &cg->funvals[i];
            FunSig *sig = codegen_fun_sig(cg, fi->module, fi->name);
            if (!sig) continue;
            w_line(&cg->w, "static ErgoVal %s(void* env, int argc, ErgoVal* argv) {", fi->wrapper);
            cg->w.indent++;
            w_line(&cg->w, "(void)env;");
            w_line(&cg->w, "if (argc != %zu) ergo_trap(\"fn arity mismatch\");", sig->params_len);
            for (size_t p = 0; p < sig->params_len; p++) {
                w_line(&cg->w, "ErgoVal arg%zu = argv[%zu];", p, p);
            }
            bool ret_void = sig->ret && sig->ret->tag == TY_VOID;
            char *mangled = mangle_global(cg->arena, sig->module, sig->name);
            if (ret_void) {
                StrBuf line; sb_init(&line);
                sb_appendf(&line, "%s(", mangled);
                for (size_t p = 0; p < sig->params_len; p++) {
                    if (p) sb_append(&line, ", ");
                    sb_appendf(&line, "arg%zu", p);
                }
                sb_append(&line, ");");
                w_line(&cg->w, "%s", line.data ? line.data : "");
                sb_free(&line);
                w_line(&cg->w, "return EV_NULLV;");
            } else {
                StrBuf line; sb_init(&line);
                sb_appendf(&line, "return %s(", mangled);
                for (size_t p = 0; p < sig->params_len; p++) {
                    if (p) sb_append(&line, ", ");
                    sb_appendf(&line, "arg%zu", p);
                }
                sb_append(&line, ");");
                w_line(&cg->w, "%s", line.data ? line.data : "");
                sb_free(&line);
            }
            cg->w.indent--;
            w_line(&cg->w, "}");
        }
        w_line(&cg->w, "");
    }

    w_line(&cg->w, "// ---- module global init ----");
    for (size_t i = 0; i < cg->prog->mods_len; i++) {
        Module *m = cg->prog->mods[i];
        Str mod_name = codegen_module_name(cg, m->path);
        ModuleGlobals *mg = codegen_module_globals(cg, mod_name);
        if (!mg || mg->len == 0) continue;
        char *init_name = mangle_global_init(cg->arena, mod_name);
        w_line(&cg->w, "static void %s(void) {", init_name);
        cg->w.indent++;
        Str saved_mod = cg->current_module;
        Str *saved_imports = cg->current_imports;
        size_t saved_imports_len = cg->current_imports_len;
        cg->current_module = mod_name;
        ModuleImport *mi = codegen_module_imports(cg, mod_name);
        cg->current_imports = mi ? mi->imports : NULL;
        cg->current_imports_len = mi ? mi->imports_len : 0;

        for (size_t j = 0; j < m->decls_len; j++) {
            Decl *d = m->decls[j];
            if (d->kind != DECL_DEF) continue;
            GenExpr ge;
            if (!gen_expr(cg, m->path, d->as.def_decl.expr, &ge, err)) return false;
            char *gname = mangle_global_var(cg->arena, mod_name, d->as.def_decl.name);
            w_line(&cg->w, "ergo_move_into(&%s, %s);", gname, ge.tmp);
            gen_expr_release_except(cg, &ge, ge.tmp);
            gen_expr_free(&ge);
        }

        cg->current_module = saved_mod;
        cg->current_imports = saved_imports;
        cg->current_imports_len = saved_imports_len;
        cg->w.indent--;
        w_line(&cg->w, "}");
        w_line(&cg->w, "");
    }

    w_line(&cg->w, "// ---- compiled functions ----");
    for (size_t i = 0; i < cg->prog->mods_len; i++) {
        Module *m = cg->prog->mods[i];
        Str mod_name = codegen_module_name(cg, m->path);
        cg->current_module = mod_name;
        ModuleImport *mi = codegen_module_imports(cg, mod_name);
        cg->current_imports = mi ? mi->imports : NULL;
        cg->current_imports_len = mi ? mi->imports_len : 0;
        cg->has_current_class = false;
        for (size_t j = 0; j < m->decls_len; j++) {
            Decl *d = m->decls[j];
            if (d->kind == DECL_CLASS) {
                for (size_t k = 0; k < d->as.class_decl.methods_len; k++) {
                    if (!gen_method(cg, m->path, &d->as.class_decl, d->as.class_decl.methods[k], err)) return false;
                }
            }
            if (d->kind == DECL_FUN) {
                if (!gen_fun(cg, m->path, &d->as.fun, err)) return false;
            }
        }
    }

    w_line(&cg->w, "// ---- entry ----");
    if (!gen_entry(cg, err)) return false;

    w_line(&cg->w, "int main(void) {");
    cg->w.indent++;
    w_line(&cg->w, "#ifdef __OBJC__");
    w_line(&cg->w, "@autoreleasepool {");
    cg->w.indent++;
    w_line(&cg->w, "ergo_runtime_init();");
    w_line(&cg->w, "ergo_entry();");
    cg->w.indent--;
    w_line(&cg->w, "}");
    w_line(&cg->w, "#else");
    w_line(&cg->w, "ergo_runtime_init();");
    w_line(&cg->w, "ergo_entry();");
    w_line(&cg->w, "#endif");
    w_line(&cg->w, "return 0;");
    cg->w.indent--;
    w_line(&cg->w, "}");
    return true;
}

bool emit_c(Program *prog, const char *out_path, Diag *err) {
    if (!prog || !out_path) {
        return cg_set_err(err, (Str){NULL, 0}, "emit_c: missing program or output path");
    }
    Arena arena;
    arena_init(&arena);
    Codegen cg;
    if (!codegen_init(&cg, prog, &arena, err)) {
        arena_free(&arena);
        return false;
    }
    if (!codegen_gen(&cg, err)) {
        codegen_free(&cg);
        arena_free(&arena);
        return false;
    }
    FILE *f = fopen(out_path, "wb");
    if (!f) {
        codegen_free(&cg);
        arena_free(&arena);
        return cg_set_err(err, (Str){out_path, strlen(out_path)}, "failed to open output file");
    }
    fwrite(cg.out.data, 1, cg.out.len, f);
    fclose(f);
    codegen_free(&cg);
    arena_free(&arena);
    return true;
}
