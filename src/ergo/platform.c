#include "platform.h"

#include <stdio.h>

#if defined(_WIN32)
#include <io.h>
#define ergo_isatty _isatty
#define ergo_fileno _fileno
#else
#include <unistd.h>
#define ergo_isatty isatty
#define ergo_fileno fileno
#endif

bool ergo_stdout_isatty(void) {
    return ergo_isatty(ergo_fileno(stdout)) != 0;
}

void ergo_set_stdout_buffered(void) {
    if (!ergo_stdout_isatty()) {
        setvbuf(stdout, NULL, _IOFBF, 1 << 16);
    }
}
