#ifndef ERGO_DIAG_H
#define ERGO_DIAG_H

#include <stdio.h>
#include <string.h>

typedef struct {
    const char *path;
    int line;
    int col;
    const char *message;
} Diag;

static inline const char* diag_get_tip(const char *msg) {
    if (!msg) return NULL;

    // Provide helpful tips based on common error patterns
    if (strstr(msg, "expected RPAR, got =>")) {
        return "Check for mismatched parentheses or lambda syntax. Lambdas use (param = Type) => expr syntax.";
    } else if (strstr(msg, "expected") && strstr(msg, "got")) {
        return "Check for syntax errors like missing punctuation or incorrect keywords.";
    } else if (strstr(msg, "missing required `bring stdr;`")) {
        return "Add 'bring stdr' at the top of your file.";
    } else if (strstr(msg, "failed to resolve")) {
        return "Check that the file path exists and is accessible.";
    } else if (strstr(msg, "out of memory")) {
        return "Try simplifying your code or checking for infinite recursion.";
    }

    return NULL;
}

static inline void diag_print(const Diag *d) {
    if (!d) {
        return;
    }
    const char *msg = d->message ? d->message : "unknown error";

    // Check if we have a valid path (not NULL and not empty)
    bool has_valid_path = d->path && d->path[0] != '\0';
    // Check if we have meaningful location info
    bool has_location = d->line > 0 || d->col > 0;

    // Print location and error message
    if (has_valid_path && has_location) {
        fprintf(stderr, "%s:\nIn %d:%d: %s\n", d->path, d->line, d->col, msg);
    } else if (has_valid_path) {
        fprintf(stderr, "%s:\n%s\n", d->path, msg);
    } else if (has_location) {
        fprintf(stderr, "In %d:%d: %s\n", d->line, d->col, msg);
    } else {
        fprintf(stderr, "Error: %s\n", msg);
    }

    // Print helpful tip if available
    const char *tip = diag_get_tip(msg);
    if (tip) {
        fprintf(stderr, "Tip: %s\n", tip);
    }
}

#endif
