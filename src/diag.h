#ifndef YIS_DIAG_H
#define YIS_DIAG_H

#include <stdbool.h>
#include <stdio.h>

// Diagnostic information structure
typedef struct {
    const char *path;
    int line;
    int col;
    const char *message;
} Diag;

// Print a diagnostic with enhanced formatting (colors, code snippets, tips)
// If verbose is true, shows more context lines around the error
void diag_print_enhanced(const Diag *d, bool verbose);

// Print a diagnostic (standard formatting, backward compatible)
void diag_print(const Diag *d);

// Print a simple error message without location info
void diag_print_simple(const char *msg);

// Print a warning message
void diag_print_warning(const char *path, int line, int col, const char *msg);

// Print an info/note message
void diag_print_note(const char *msg);

#endif
