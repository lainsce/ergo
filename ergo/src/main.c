#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <ctype.h>

#if defined(_WIN32)
#include <direct.h>
#define ergo_getcwd _getcwd
#define ergo_mkdir(path) _mkdir(path)
#else
#include <unistd.h>
#include <sys/stat.h>
#define ergo_getcwd getcwd
#define ergo_mkdir(path) mkdir((path), 0755)
#endif

#include "arena.h"
#include "codegen.h"
#include "diag.h"
#include "file.h"
#include "platform.h"
#include "project.h"
#include "str.h"
#include "typecheck.h"

#define ERGO_CACHE_VERSION __DATE__ " " __TIME__

static uint64_t hash_update(uint64_t h, const void *data, size_t len) {
    const unsigned char *p = (const unsigned char *)data;
    for (size_t i = 0; i < len; i++) {
        h ^= (uint64_t)p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

static uint64_t hash_cstr(uint64_t h, const char *s) {
    if (!s) {
        return hash_update(h, "", 0);
    }
    return hash_update(h, s, strlen(s));
}

static char *dup_cstr(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len + 1);
    return out;
}

static bool ensure_dir(const char *path) {
    if (!path || !path[0]) {
        return false;
    }
    if (ergo_mkdir(path) == 0) {
        return true;
    }
    return errno == EEXIST;
}

static char *cache_base_dir(void) {
    const char *env = getenv("ERGO_CACHE_DIR");
    if (env && env[0]) {
        return dup_cstr(env);
    }
    char buf[4096];
    if (!ergo_getcwd(buf, sizeof(buf))) {
        return NULL;
    }
    return path_join(buf, ".ergo-cache");
}

static int run_binary(const char *path) {
    if (!path) return 1;
    char cmd[4096];
    int n = snprintf(cmd, sizeof(cmd), "\"%s\"", path);
    if (n < 0 || (size_t)n >= sizeof(cmd)) {
        return 1;
    }
    return system(cmd);
}

static bool program_uses_cogito(Program *prog) {
    if (!prog) return false;
    for (size_t i = 0; i < prog->mods_len; i++) {
        Module *m = prog->mods[i];
        for (size_t j = 0; j < m->imports_len; j++) {
            if (str_eq_c(m->imports[j]->name, "cogito")) {
                return true;
            }
        }
    }
    return false;
}

static bool sanitize_filename_component(const char *src, size_t src_len, char *out, size_t out_cap) {
    if (!src || !out || out_cap == 0) return false;
    size_t n = 0;
    for (size_t i = 0; i < src_len; i++) {
        unsigned char c = (unsigned char)src[i];
        char mapped;
        if (isalnum(c) || c == '.' || c == '_' || c == '-') {
            mapped = (char)c;
        } else if (c == ' ' || c == '\t') {
            mapped = '-';
        } else {
            mapped = '_';
        }
        if (n + 1 < out_cap) {
            out[n++] = mapped;
        }
    }
    while (n > 0 && (out[n - 1] == '.' || out[n - 1] == ' ')) {
        n--;
    }
    if (n == 0) {
        out[0] = '\0';
        return false;
    }
    out[n] = '\0';
    return true;
}

static bool expr_string_literal_as_filename(Expr *e, char *out, size_t out_cap) {
    if (!e || e->kind != EXPR_STR || !out || out_cap == 0) return false;
    StrParts *parts = e->as.str_lit.parts;
    if (!parts || parts->len == 0) return false;

    size_t n = 0;
    for (size_t i = 0; i < parts->len; i++) {
        StrPart *part = &parts->parts[i];
        if (part->kind != STR_PART_TEXT) return false;
        for (size_t j = 0; j < part->text.len; j++) {
            unsigned char c = (unsigned char)part->text.data[j];
            char mapped;
            if (isalnum(c) || c == '.' || c == '_' || c == '-') {
                mapped = (char)c;
            } else if (c == ' ' || c == '\t') {
                mapped = '-';
            } else {
                mapped = '_';
            }
            if (n + 1 < out_cap) {
                out[n++] = mapped;
            }
        }
    }
    while (n > 0 && (out[n - 1] == '.' || out[n - 1] == ' ')) {
        n--;
    }
    if (n == 0) {
        out[0] = '\0';
        return false;
    }
    out[n] = '\0';
    return true;
}

static void program_find_cogito_appid_stmt(Stmt *s, char *out, size_t out_cap, bool *found);

static void program_find_cogito_appid_expr(Expr *e, char *out, size_t out_cap, bool *found) {
    if (!e) return;
    switch (e->kind) {
        case EXPR_CALL: {
            Expr *fn = e->as.call.fn;
            bool matches_set_appid = false;
            int arg_index = 0;
            if (fn && fn->kind == EXPR_MEMBER && str_eq_c(fn->as.member.name, "set_appid")) {
                matches_set_appid = true;
                arg_index = 0;
            } else if (fn && fn->kind == EXPR_IDENT && str_eq_c(fn->as.ident.name, "__cogito_app_set_appid")) {
                matches_set_appid = true;
                arg_index = 1;
            }
            if (matches_set_appid && e->as.call.args_len > (size_t)arg_index) {
                char candidate[256];
                if (expr_string_literal_as_filename(e->as.call.args[arg_index], candidate, sizeof(candidate))) {
                    snprintf(out, out_cap, "%s", candidate);
                    *found = true;
                }
            }
            program_find_cogito_appid_expr(fn, out, out_cap, found);
            for (size_t i = 0; i < e->as.call.args_len; i++) {
                program_find_cogito_appid_expr(e->as.call.args[i], out, out_cap, found);
            }
            break;
        }
        case EXPR_UNARY:
            program_find_cogito_appid_expr(e->as.unary.x, out, out_cap, found);
            break;
        case EXPR_BINARY:
            program_find_cogito_appid_expr(e->as.binary.a, out, out_cap, found);
            program_find_cogito_appid_expr(e->as.binary.b, out, out_cap, found);
            break;
        case EXPR_ASSIGN:
            program_find_cogito_appid_expr(e->as.assign.target, out, out_cap, found);
            program_find_cogito_appid_expr(e->as.assign.value, out, out_cap, found);
            break;
        case EXPR_INDEX:
            program_find_cogito_appid_expr(e->as.index.a, out, out_cap, found);
            program_find_cogito_appid_expr(e->as.index.i, out, out_cap, found);
            break;
        case EXPR_MEMBER:
            program_find_cogito_appid_expr(e->as.member.a, out, out_cap, found);
            break;
        case EXPR_PAREN:
            program_find_cogito_appid_expr(e->as.paren.x, out, out_cap, found);
            break;
        case EXPR_MATCH:
            program_find_cogito_appid_expr(e->as.match_expr.scrut, out, out_cap, found);
            for (size_t i = 0; i < e->as.match_expr.arms_len; i++) {
                MatchArm *arm = e->as.match_expr.arms[i];
                if (arm) program_find_cogito_appid_expr(arm->expr, out, out_cap, found);
            }
            break;
        case EXPR_LAMBDA:
            program_find_cogito_appid_expr(e->as.lambda.body, out, out_cap, found);
            break;
        case EXPR_BLOCK:
            program_find_cogito_appid_stmt(e->as.block_expr.block, out, out_cap, found);
            break;
        case EXPR_NEW:
            for (size_t i = 0; i < e->as.new_expr.args_len; i++) {
                program_find_cogito_appid_expr(e->as.new_expr.args[i], out, out_cap, found);
            }
            break;
        case EXPR_IF:
            for (size_t i = 0; i < e->as.if_expr.arms_len; i++) {
                ExprIfArm *arm = e->as.if_expr.arms[i];
                if (!arm) continue;
                program_find_cogito_appid_expr(arm->cond, out, out_cap, found);
                program_find_cogito_appid_expr(arm->value, out, out_cap, found);
            }
            break;
        case EXPR_TERNARY:
            program_find_cogito_appid_expr(e->as.ternary.cond, out, out_cap, found);
            program_find_cogito_appid_expr(e->as.ternary.then_expr, out, out_cap, found);
            program_find_cogito_appid_expr(e->as.ternary.else_expr, out, out_cap, found);
            break;
        case EXPR_MOVE:
            program_find_cogito_appid_expr(e->as.move.x, out, out_cap, found);
            break;
        case EXPR_TUPLE:
            for (size_t i = 0; i < e->as.tuple_lit.items_len; i++) {
                program_find_cogito_appid_expr(e->as.tuple_lit.items[i], out, out_cap, found);
            }
            break;
        case EXPR_ARRAY:
            for (size_t i = 0; i < e->as.array_lit.items_len; i++) {
                program_find_cogito_appid_expr(e->as.array_lit.items[i], out, out_cap, found);
            }
            break;
        case EXPR_INT:
        case EXPR_FLOAT:
        case EXPR_STR:
        case EXPR_IDENT:
        case EXPR_NULL:
        case EXPR_BOOL:
            break;
    }
}

static void program_find_cogito_appid_stmt(Stmt *s, char *out, size_t out_cap, bool *found) {
    if (!s) return;
    switch (s->kind) {
        case STMT_LET:
            program_find_cogito_appid_expr(s->as.let_s.expr, out, out_cap, found);
            break;
        case STMT_CONST:
            program_find_cogito_appid_expr(s->as.const_s.expr, out, out_cap, found);
            break;
        case STMT_IF:
            for (size_t i = 0; i < s->as.if_s.arms_len; i++) {
                IfArm *arm = s->as.if_s.arms[i];
                if (!arm) continue;
                program_find_cogito_appid_expr(arm->cond, out, out_cap, found);
                program_find_cogito_appid_stmt(arm->body, out, out_cap, found);
            }
            break;
        case STMT_FOR:
            program_find_cogito_appid_stmt(s->as.for_s.init, out, out_cap, found);
            program_find_cogito_appid_expr(s->as.for_s.cond, out, out_cap, found);
            program_find_cogito_appid_expr(s->as.for_s.step, out, out_cap, found);
            program_find_cogito_appid_stmt(s->as.for_s.body, out, out_cap, found);
            break;
        case STMT_FOREACH:
            program_find_cogito_appid_expr(s->as.foreach_s.expr, out, out_cap, found);
            program_find_cogito_appid_stmt(s->as.foreach_s.body, out, out_cap, found);
            break;
        case STMT_RETURN:
            program_find_cogito_appid_expr(s->as.ret_s.expr, out, out_cap, found);
            break;
        case STMT_BREAK:
        case STMT_CONTINUE:
            break;
        case STMT_EXPR:
            program_find_cogito_appid_expr(s->as.expr_s.expr, out, out_cap, found);
            break;
        case STMT_BLOCK:
            for (size_t i = 0; i < s->as.block_s.stmts_len; i++) {
                program_find_cogito_appid_stmt(s->as.block_s.stmts[i], out, out_cap, found);
            }
            break;
    }
}

static bool program_find_cogito_appid_name(Program *prog, char *out, size_t out_cap) {
    if (!prog || !out || out_cap == 0) return false;
    bool found = false;
    for (size_t i = 0; i < prog->mods_len; i++) {
        Module *m = prog->mods[i];
        if (!m) continue;
        for (size_t j = 0; j < m->decls_len; j++) {
            Decl *d = m->decls[j];
            if (!d) continue;
            switch (d->kind) {
                case DECL_ENTRY:
                    program_find_cogito_appid_stmt(d->as.entry.body, out, out_cap, &found);
                    break;
                case DECL_FUN:
                    program_find_cogito_appid_stmt(d->as.fun.body, out, out_cap, &found);
                    break;
                case DECL_CONST:
                    program_find_cogito_appid_expr(d->as.const_decl.expr, out, out_cap, &found);
                    break;
                case DECL_DEF:
                    program_find_cogito_appid_expr(d->as.def_decl.expr, out, out_cap, &found);
                    break;
                case DECL_CLASS:
                    for (size_t k = 0; k < d->as.class_decl.methods_len; k++) {
                        FunDecl *meth = d->as.class_decl.methods[k];
                        if (meth) program_find_cogito_appid_stmt(meth->body, out, out_cap, &found);
                    }
                    break;
            }
        }
    }
    return found;
}

#define ERGO_VERSION "0.1.0"

static bool verbose_mode = false;

static void print_usage(FILE *out) {
    fprintf(out, "Usage: ergo [OPTIONS] <source.ergo>\n");
    fprintf(out, "       ergo run [OPTIONS] <source.ergo>\n");
    fprintf(out, "\n");
    fprintf(out, "Options:\n");
    fprintf(out, "  -h, --help       Show this help message\n");
    fprintf(out, "  -v, --version    Show version information\n");
    fprintf(out, "  --verbose        Enable verbose error output with more context\n");
    fprintf(out, "\n");
    fprintf(out, "Examples:\n");
    fprintf(out, "  ergo init.ergo              # Compile and check init.ergo\n");
    fprintf(out, "  ergo run init.ergo          # Compile and run init.ergo\n");
    fprintf(out, "  ergo --help                 # Show this help\n");
    fprintf(out, "\n");
    fprintf(out, "Environment Variables:\n");
    fprintf(out, "  ERGO_STDLIB      Path to standard library (default: auto-detected, fallback: ergo/src/stdlib)\n");
    fprintf(out, "  ERGO_CACHE_DIR   Cache directory for compiled binaries\n");
    fprintf(out, "  ERGO_NO_CACHE    Set to 1 to disable caching\n");
    fprintf(out, "  ERGO_KEEP_C      Set to 1 to keep generated C files\n");
    fprintf(out, "  CC               C compiler to use (default: cc)\n");
    fprintf(out, "  ERGO_CC_FLAGS    Additional C compiler flags\n");
    fprintf(out, "  NO_COLOR         Set to disable colored output\n");
    fprintf(out, "\n");
    fprintf(out, "Cogito GUI Framework:\n");
    fprintf(out, "  To build GUI applications with Cogito:\n");
    fprintf(out, "    1. Build Cogito: cd cogito && meson setup build && ninja -C build\n");
    fprintf(out, "    2. Add 'bring cogito;' to your init.ergo\n");
    fprintf(out, "    3. Ensure raylib is installed (brew install raylib on macOS)\n");
    fprintf(out, "\n");
    fprintf(out, "  Cogito Environment Variables:\n");
    fprintf(out, "    ERGO_COGITO_CFLAGS   Additional C flags for Cogito compilation\n");
    fprintf(out, "    ERGO_COGITO_FLAGS    Additional linker flags for Cogito\n");
    fprintf(out, "    ERGO_RAYLIB_CFLAGS   C flags for raylib (auto-detected on macOS/Linux)\n");
    fprintf(out, "    ERGO_RAYLIB_FLAGS    Linker flags for raylib (auto-detected on macOS/Linux)\n");
}

static void print_version(void) {
    printf("ergo version %s\n", ERGO_VERSION);
    printf("Copyright (c) 2026 Ergo Contributors\n");
}

static int is_flag(const char *arg, const char *flag) {
    return arg && flag && strcmp(arg, flag) == 0;
}

static const char *cc_path(void) {
    const char *cc = getenv("CC");
    return cc && cc[0] ? cc : "cc";
}

static const char *cc_flags(void) {
    const char *flags = getenv("ERGO_CC_FLAGS");
    return flags && flags[0] ? flags : "-O3 -std=c11 -pipe";
}

static const char *join_flags(char *buf, size_t cap, const char *a, const char *b) {
    if (!(a && a[0])) {
        snprintf(buf, cap, "%s", b && b[0] ? b : "");
        return buf;
    }
    if (!(b && b[0])) {
        snprintf(buf, cap, "%s", a);
        return buf;
    }
    snprintf(buf, cap, "%s %s", a, b);
    return buf;
}

#if defined(__APPLE__) || defined(__linux__)
static const char *raylib_default_cflags(void) {
#if defined(__APPLE__)
    if (path_is_file("/opt/homebrew/include/raylib.h")) {
        return "-I/opt/homebrew/include";
    }
    if (path_is_file("/usr/local/include/raylib.h")) {
        return "-I/usr/local/include";
    }
#elif defined(__linux__)
    if (path_is_file("/usr/include/raylib.h")) {
        return "-I/usr/include";
    }
    if (path_is_file("/usr/local/include/raylib.h")) {
        return "-I/usr/local/include";
    }
#endif
    return "";
}

static const char *raylib_default_ldflags(void) {
#if defined(_WIN32)
    return "-lraylib -lopengl32 -lgdi32 -lwinmm";
#elif defined(__APPLE__)
    static char buf[512];
    if (path_is_file("/opt/homebrew/lib/libraylib.dylib")) {
        snprintf(buf, sizeof(buf),
                 "-L/opt/homebrew/lib -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo");
        return buf;
    }
    if (path_is_file("/usr/local/lib/libraylib.dylib")) {
        snprintf(buf, sizeof(buf),
                 "-L/usr/local/lib -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo");
        return buf;
    }
    return "-lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo";
#else
    if (path_is_file("/usr/local/lib/libraylib.so")) {
        return "-L/usr/local/lib -lraylib -lm -lpthread -ldl -lrt -lX11";
    }
    return "-lraylib -lm -lpthread -ldl -lrt -lX11";
#endif
}
#endif

static const char *cogito_default_cflags(void) {
    if (path_is_file("cogito/src/cogito.h")) {
        return "-Icogito/src";
    }
    if (path_is_file("../cogito/src/cogito.h")) {
        return "-I../cogito/src";
    }
    if (path_is_file("../../cogito/src/cogito.h")) {
        return "-I../../cogito/src";
    }
    if (path_is_file("cogito/include/cogito.h")) {
        return "-Icogito/include";
    }
    if (path_is_file("../cogito/include/cogito.h")) {
        return "-I../cogito/include";
    }
    if (path_is_file("../../cogito/include/cogito.h")) {
        return "-I../../cogito/include";
    }
    return "";
}

static const char *cogito_default_ldflags(void) {
#if defined(__APPLE__)
    const char *libname = "libcogito.dylib";
#elif defined(_WIN32)
    const char *libname = "cogito.dll";
#else
    const char *libname = "libcogito.so";
#endif
    static char buf[512];
    const char *dirs[] = {
        "cogito/_build",
        "cogito/build",
        "../cogito/_build",
        "../cogito/build",
        "../../cogito/_build",
        "../../cogito/build",
    };
    for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", dirs[i], libname);
        if (path_is_file(path)) {
#if defined(__APPLE__) || defined(__linux__)
            snprintf(buf, sizeof(buf), "-L%s -lcogito -Wl,-rpath,%s", dirs[i], dirs[i]);
#else
            snprintf(buf, sizeof(buf), "-L%s -lcogito", dirs[i]);
#endif
            return buf;
        }
    }
    return "-lcogito";
}

int main(int argc, char **argv) {
    ergo_set_stdout_buffered();

    if (argc < 2) {
        print_usage(stderr);
        return 2;
    }

    // Handle global flags
    if (is_flag(argv[1], "--help") || is_flag(argv[1], "-h")) {
        print_usage(stdout);
        return 0;
    }

    if (is_flag(argv[1], "--version") || is_flag(argv[1], "-v")) {
        print_version();
        return 0;
    }

    if (is_flag(argv[1], "--verbose")) {
        verbose_mode = true;
        // Shift arguments and continue
        if (argc < 3) {
            print_usage(stderr);
            return 2;
        }
        argv++;
        argc--;
    }

    if (is_flag(argv[1], "--emit-c")) {
        fprintf(stderr, "error: --emit-c is not supported in the C compiler\n");
        return 2;
    }

    if (is_flag(argv[1], "run")) {
        const char *entry = NULL;
        for (int i = 2; i < argc; i++) {
            if (argv[i][0] == '-') {
                fprintf(stderr, "error: unknown option %s\n", argv[i]);
                return 2;
            }
            if (entry) {
                fprintf(stderr, "error: multiple source paths provided\n");
                return 2;
            }
            entry = argv[i];
        }
        if (!entry) {
            fprintf(stderr, "error: run needs a source path\n");
            return 2;
        }
        Arena arena;
        arena_init(&arena);
        Diag err = {0};
        Program *prog = NULL;
        uint64_t proj_hash = 0;
    if (!load_project(entry, &arena, &prog, &proj_hash, &err)) {
        diag_print_enhanced(&err, verbose_mode);
        arena_free(&arena);
        return 1;
    }


        bool uses_cogito = program_uses_cogito(prog);
        const char *extra_cflags = "";
        const char *extra_ldflags = "";
        char extra_cflags_buf[2048] = {0};
        char extra_ldflags_buf[2048] = {0};
        const char *cogito_cflags_env = getenv("ERGO_COGITO_CFLAGS");
        const char *cogito_cflags = (cogito_cflags_env && cogito_cflags_env[0]) ? cogito_cflags_env : cogito_default_cflags();
        if (cogito_cflags && cogito_cflags[0]) {
            extra_cflags = cogito_cflags;
        }
        if (uses_cogito) {
            const char *ray_cflags = getenv("ERGO_RAYLIB_CFLAGS");
            if (!(ray_cflags && ray_cflags[0])) {
#if defined(__APPLE__) || defined(__linux__)
                ray_cflags = raylib_default_cflags();
#endif
            }
            extra_cflags = join_flags(extra_cflags_buf, sizeof(extra_cflags_buf), ray_cflags, cogito_cflags);

            const char *ray_flags = getenv("ERGO_RAYLIB_FLAGS");
            const char *cogito_flags = getenv("ERGO_COGITO_FLAGS");
            if (!(ray_flags && ray_flags[0])) {
                ray_flags = raylib_default_ldflags();
            }
            if (!(cogito_flags && cogito_flags[0])) {
                cogito_flags = cogito_default_ldflags();
            }
            // Keep Cogito search paths first so local cogito/build wins over system-installed libcogito.
            extra_ldflags = join_flags(extra_ldflags_buf, sizeof(extra_ldflags_buf), cogito_flags, ray_flags);
        }

        // Generate unique binary name from app id (if statically set) or entry file basename.
        char unique_bin_name[256];
        const char *entry_basename = strrchr(entry, '/');
        if (!entry_basename) {
            entry_basename = strrchr(entry, '\\');
        }
        entry_basename = entry_basename ? entry_basename + 1 : entry;

        // Remove .ergo extension if present
        char name_source[256];
        snprintf(name_source, sizeof(name_source), "%s", entry_basename);
        char *dot = strrchr(name_source, '.');
        if (dot && strcmp(dot, ".ergo") == 0) {
            *dot = '\0';
        }
        char name_without_ext[256];
        if (!sanitize_filename_component(name_source, strlen(name_source), name_without_ext, sizeof(name_without_ext))) {
            snprintf(name_without_ext, sizeof(name_without_ext), "main");
        }
        if (uses_cogito) {
            char appid_name[256];
            if (program_find_cogito_appid_name(prog, appid_name, sizeof(appid_name))) {
                snprintf(name_without_ext, sizeof(name_without_ext), "%s", appid_name);
            }
        }

#if defined(_WIN32)
        snprintf(unique_bin_name, sizeof(unique_bin_name), "%s.exe", name_without_ext);
#else
        snprintf(unique_bin_name, sizeof(unique_bin_name), "%s", name_without_ext);
#endif

        uint64_t build_hash = proj_hash;
        build_hash = hash_cstr(build_hash, cc_path());
        build_hash = hash_cstr(build_hash, cc_flags());
        build_hash = hash_cstr(build_hash, extra_cflags);
        build_hash = hash_cstr(build_hash, extra_ldflags);
        build_hash = hash_cstr(build_hash, ERGO_CACHE_VERSION);

        const char *no_cache_env = getenv("ERGO_NO_CACHE");
        bool cache_enabled = false;
        if (no_cache_env && no_cache_env[0]) {
            cache_enabled = (no_cache_env[0] == '0');
        }

        char *cache_base = NULL;
        char *cache_dir = NULL;
        char *cache_c = NULL;
        char *cache_bin = NULL;
        if (cache_enabled) {
            cache_base = cache_base_dir();
            if (cache_base && ensure_dir(cache_base)) {
                char hex[17];
                snprintf(hex, sizeof(hex), "%016llx", (unsigned long long)build_hash);
                cache_dir = path_join(cache_base, hex);
                if (cache_dir && ensure_dir(cache_dir)) {
                    char cache_c_name[512];
                    snprintf(cache_c_name, sizeof(cache_c_name), "%s.c", name_without_ext);
                    cache_c = path_join(cache_dir, cache_c_name);
                    cache_bin = path_join(cache_dir, unique_bin_name);
                }
            }
        }

        if (cache_enabled) {
            if (cache_bin && path_is_file(cache_bin)) {
                remove(cache_bin);
            }
            if (cache_c && path_is_file(cache_c)) {
                remove(cache_c);
            }
        }

        if (cache_enabled && cache_bin && path_is_file(cache_bin)) {
            int rc = run_binary(cache_bin);
            free(cache_base);
            free(cache_dir);
            free(cache_c);
            free(cache_bin);
            arena_free(&arena);
            return rc == 0 ? 0 : 1;
        }

        // When not using cache, check if binary is up-to-date.
        // For Cogito apps we skip this shortcut because SUM can be embedded
        // from external files (e.g. cogito.load_sum("...")), and those files
        // are not tracked by this single source mtime check.
        if (!cache_enabled && !uses_cogito && path_is_file(unique_bin_name)) {
            long long bin_mtime = path_mtime(unique_bin_name);
            long long src_mtime = path_mtime(entry);
            if (bin_mtime >= 0 && src_mtime >= 0 && bin_mtime >= src_mtime) {
                // Binary is newer than source, just run it
                char run_cmd_buf[512];
#if defined(_WIN32)
                snprintf(run_cmd_buf, sizeof(run_cmd_buf), ".\\%s", unique_bin_name);
#else
                snprintf(run_cmd_buf, sizeof(run_cmd_buf), "./%s", unique_bin_name);
#endif
                int rc = run_binary(run_cmd_buf);
                arena_free(&arena);
                return rc == 0 ? 0 : 1;
            }
        }

        prog = lower_program(prog, &arena, &err);
        if (!prog || err.message) {
            diag_print_enhanced(&err, verbose_mode);
            free(cache_base);
            free(cache_dir);
            free(cache_c);
            free(cache_bin);
            arena_free(&arena);
            return 1;
        }
        if (!typecheck_program(prog, &arena, &err)) {
            diag_print_enhanced(&err, verbose_mode);
            free(cache_base);
            free(cache_dir);
            free(cache_c);
            free(cache_bin);
            arena_free(&arena);
            return 1;
        }
        const char *c_path = cache_c ? cache_c : ".ergo_run.c";
        const char *bin_path = cache_bin ? cache_bin : unique_bin_name;

        char run_cmd_buf[512];
#if defined(_WIN32)
        snprintf(run_cmd_buf, sizeof(run_cmd_buf), ".\\%s", unique_bin_name);
#else
        snprintf(run_cmd_buf, sizeof(run_cmd_buf), "./%s", unique_bin_name);
#endif
        const char *run_cmd = cache_bin ? cache_bin : run_cmd_buf;
        if (!emit_c(prog, c_path, uses_cogito, &err)) {
            diag_print_enhanced(&err, verbose_mode);
            free(cache_base);
            free(cache_dir);
            free(cache_c);
            free(cache_bin);
            arena_free(&arena);
            return 1;
        }
        char cmd[4096];
        int n = snprintf(cmd, sizeof(cmd), "%s %s %s %s -o %s %s",
                         cc_path(), cc_flags(), extra_cflags, c_path, bin_path, extra_ldflags);
        if (n < 0 || (size_t)n >= sizeof(cmd)) {
            fprintf(stderr, "error: compile command too long\n");
            free(cache_base);
            free(cache_dir);
            free(cache_c);
            free(cache_bin);
            arena_free(&arena);
            return 1;
        }
        int rc = system(cmd);
        if (rc != 0) {
            fprintf(stderr, "error: C compiler failed (code %d)\n", rc);
            free(cache_base);
            free(cache_dir);
            free(cache_c);
            free(cache_bin);
            arena_free(&arena);
            return rc;
        }
        const char *keep_c = getenv("ERGO_KEEP_C");
        if (!(keep_c && keep_c[0] && keep_c[0] != '0')) {
            (void)remove(c_path);
        }
        rc = run_binary(run_cmd);
        free(cache_base);
        free(cache_dir);
        free(cache_c);
        free(cache_bin);
        arena_free(&arena);
        return rc == 0 ? 0 : 1;
    }

    if (argv[1][0] == '-') {
        fprintf(stderr, "error: unknown option %s\n", argv[1]);
        return 2;
    }

    if (argc > 2) {
        fprintf(stderr, "error: unexpected extra arguments\n");
        return 2;
    }

    Arena arena;
    arena_init(&arena);
    Diag err = {0};
    Program *prog = NULL;
    if (!load_project(argv[1], &arena, &prog, NULL, &err)) {
        diag_print_enhanced(&err, verbose_mode);
        arena_free(&arena);
        return 1;
    }
    prog = lower_program(prog, &arena, &err);
    if (!prog || err.message) {
        diag_print_enhanced(&err, verbose_mode);
        arena_free(&arena);
        return 1;
    }
    if (!typecheck_program(prog, &arena, &err)) {
        diag_print_enhanced(&err, verbose_mode);
        arena_free(&arena);
        return 1;
    }
    arena_free(&arena);
    return 0;
}
