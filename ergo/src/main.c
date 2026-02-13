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
#include "sum_validate.h"
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

static bool expr_string_literal_as_filename(Expr *e, char *out, size_t out_cap);

// Cogito build integration (lives in cogito/ergo/)
#include "cogito_build.inc"

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


#define ERGO_VERSION "0.1.0"

static bool verbose_mode = false;

static void print_usage(FILE *out) {
    fprintf(out, "Usage: ergo [OPTIONS] <source.ergo>\n");
    fprintf(out, "       ergo run [OPTIONS] <source.ergo>\n");
    fprintf(out, "       ergo lint [--mode warn|strict] <source.ergo>\n");
    fprintf(out, "       ergo sum validate [--mode off|warn|strict] <path>\n");
    fprintf(out, "\n");
    fprintf(out, "Options:\n");
    fprintf(out, "  -h, --help       Show this help message\n");
    fprintf(out, "  -v, --version    Show version information\n");
    fprintf(out, "  --verbose        Enable verbose error output with more context\n");
    fprintf(out, "\n");
    fprintf(out, "Examples:\n");
    fprintf(out, "  ergo init.ergo              # Compile and check init.ergo\n");
    fprintf(out, "  ergo run init.ergo          # Compile and run init.ergo\n");
    fprintf(out, "  ergo lint --mode strict init.ergo\n");
    fprintf(out, "  ergo sum validate theme.sum # Validate one SUM file\n");
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

    if (is_flag(argv[1], "sum")) {
        return sum_validate_cli(argc, argv);
    }

    if (is_flag(argv[1], "lint")) {
        ErgoLintMode lint_mode = ERGO_LINT_WARN;
        const char *entry = NULL;
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--mode") == 0) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "error: --mode requires one of warn|strict\n");
                    return 2;
                }
                i++;
                if (strcmp(argv[i], "warn") == 0) {
                    lint_mode = ERGO_LINT_WARN;
                } else if (strcmp(argv[i], "strict") == 0) {
                    lint_mode = ERGO_LINT_STRICT;
                } else {
                    fprintf(stderr, "error: unknown lint mode '%s'\n", argv[i]);
                    return 2;
                }
                continue;
            }
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
            fprintf(stderr, "error: lint needs a source path\n");
            return 2;
        }
        Arena arena;
        arena_init(&arena);
        Diag err = {0};
        Program *prog = NULL;
        if (!load_project(entry, &arena, &prog, NULL, &err)) {
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
        int warnings = 0;
        int errors = 0;
        bool ok = lint_program(prog, &arena, lint_mode, &warnings, &errors);
        fprintf(stderr, "lint summary: %d warning(s), %d error(s)\n", warnings, errors);
        arena_free(&arena);
        return ok ? 0 : 1;
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
        // Compile-time AST/type data is no longer needed after codegen/compile.
        // Release it before running user code to reduce peak RSS.
        arena_free(&arena);
        rc = run_binary(run_cmd);
        free(cache_base);
        free(cache_dir);
        free(cache_c);
        free(cache_bin);
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
