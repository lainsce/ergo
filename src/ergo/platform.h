#ifndef ERGO_PLATFORM_H
#define ERGO_PLATFORM_H

#include <stdbool.h>

bool ergo_stdout_isatty(void);
void ergo_set_stdout_buffered(void);

#endif
