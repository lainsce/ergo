#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

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

static void print_usage(FILE *out) {
    fprintf(out, "usage: ergo <source.ergo>\n");
    fprintf(out, "       ergo run <source.ergo>\n");
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
    if (path_is_file("cogito/include/cogito.h")) {
        return "-Icogito/include";
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
    const char *dirs[] = {"cogito/_build", "cogito/build"};
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
            diag_print(&err);
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
            extra_ldflags = join_flags(extra_ldflags_buf, sizeof(extra_ldflags_buf), ray_flags, cogito_flags);
        }

        // Generate unique binary name from entry file
        char unique_bin_name[256];
        const char *entry_basename = strrchr(entry, '/');
        if (!entry_basename) {
            entry_basename = strrchr(entry, '\\');
        }
        entry_basename = entry_basename ? entry_basename + 1 : entry;

        // Remove .ergo extension if present
        char name_without_ext[256];
        snprintf(name_without_ext, sizeof(name_without_ext), "%s", entry_basename);
        char *dot = strrchr(name_without_ext, '.');
        if (dot && strcmp(dot, ".ergo") == 0) {
            *dot = '\0';
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

        prog = lower_program(prog, &arena, &err);
        if (!prog || err.message) {
            diag_print(&err);
            free(cache_base);
            free(cache_dir);
            free(cache_c);
            free(cache_bin);
            arena_free(&arena);
            return 1;
        }
        if (!typecheck_program(prog, &arena, &err)) {
            diag_print(&err);
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
        if (!emit_c(prog, c_path, &err)) {
            diag_print(&err);
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
        diag_print(&err);
        arena_free(&arena);
        return 1;
    }
    prog = lower_program(prog, &arena, &err);
    if (!prog || err.message) {
        diag_print(&err);
        arena_free(&arena);
        return 1;
    }
    if (!typecheck_program(prog, &arena, &err)) {
        diag_print(&err);
        arena_free(&arena);
        return 1;
    }
    arena_free(&arena);
    return 0;
}
