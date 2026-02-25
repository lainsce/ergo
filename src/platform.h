#ifndef ERGO_PLATFORM_H
#define ERGO_PLATFORM_H

#include <stdbool.h>

bool ergo_stdout_isatty(void);
void ergo_set_stdout_buffered(void);

// Returns the directory containing the running executable (heap-allocated).
// Caller must free(). Returns NULL on failure.
char *ergo_exe_dir(void);

#endif
