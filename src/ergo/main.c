#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "codegen.h"
#include "diag.h"
#include "platform.h"
#include "project.h"
#include "typecheck.h"

static void print_usage(FILE *out) {
    fprintf(out, "usage: ergo <source.e>\n");
    fprintf(out, "       ergo run <source.e>\n");
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
        if (!load_project(entry, &arena, &prog, &err)) {
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
        const char *c_path = ".ergo_run.c";
#if defined(_WIN32)
        const char *bin_path = "run.exe";
        const char *run_cmd = ".\\run.exe";
#else
        const char *bin_path = "run";
        const char *run_cmd = "./run";
#endif
        if (!emit_c(prog, c_path, &err)) {
            diag_print(&err);
            arena_free(&arena);
            return 1;
        }
        char cmd[4096];
        int n = snprintf(cmd, sizeof(cmd), "%s %s %s -o %s", cc_path(), cc_flags(), c_path, bin_path);
        if (n < 0 || (size_t)n >= sizeof(cmd)) {
            fprintf(stderr, "error: compile command too long\n");
            arena_free(&arena);
            return 1;
        }
        int rc = system(cmd);
        if (rc != 0) {
            fprintf(stderr, "error: C compiler failed (code %d)\n", rc);
            arena_free(&arena);
            return rc;
        }
        (void)remove(c_path);
        rc = system(run_cmd);
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
    if (!load_project(argv[1], &arena, &prog, &err)) {
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
