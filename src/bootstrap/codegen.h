#ifndef YIS_CODEGEN_H
#define YIS_CODEGEN_H

#include <stdbool.h>

#include "ast.h"
#include "diag.h"

bool emit_c(Program *prog, const char *out_path, bool uses_cogito, Diag *err);

#endif
