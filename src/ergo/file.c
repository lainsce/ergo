#include "file.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if defined(_WIN32)
#include <direct.h>
#include <io.h>
#include <sys/stat.h>
#define stat _stat
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

static char *strdup_or_null(const char *s) {
    if (!s) {
        return NULL;
    }
    size_t len = strlen(s);
    char *out = (char *)malloc(len + 1);
    if (!out) {
        return NULL;
    }
    memcpy(out, s, len + 1);
    return out;
}

char *read_file_arena(const char *path, Arena *arena, size_t *out_len, Diag *err) {
    if (!path || !arena) {
        return NULL;
    }
    FILE *f = fopen(path, "rb");
    if (!f) {
        if (err) {
            err->path = path;
            err->line = 0;
            err->col = 0;
            err->message = "failed to open file";
        }
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        if (err) {
            err->path = path;
            err->line = 0;
            err->col = 0;
            err->message = "failed to seek file";
        }
        return NULL;
    }
    long size = ftell(f);
    if (size < 0) {
        fclose(f);
        if (err) {
            err->path = path;
            err->line = 0;
            err->col = 0;
            err->message = "failed to read file size";
        }
        return NULL;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        if (err) {
            err->path = path;
            err->line = 0;
            err->col = 0;
            err->message = "failed to seek file";
        }
        return NULL;
    }
    size_t len = (size_t)size;
    char *buf = (char *)arena_alloc(arena, len + 1);
    if (!buf) {
        fclose(f);
        if (err) {
            err->path = path;
            err->line = 0;
            err->col = 0;
            err->message = "out of memory";
        }
        return NULL;
    }
    size_t n = fread(buf, 1, len, f);
    fclose(f);
    if (n != len) {
        if (err) {
            err->path = path;
            err->line = 0;
            err->col = 0;
            err->message = "failed to read file";
        }
        return NULL;
    }
    buf[len] = '\0';
    if (out_len) {
        *out_len = len;
    }
    return buf;
}

char *path_abs(const char *path) {
    if (!path) {
        return NULL;
    }
#if defined(_WIN32)
    return _fullpath(NULL, path, 0);
#else
    char *resolved = realpath(path, NULL);
    if (resolved) {
        return resolved;
    }
    return strdup_or_null(path);
#endif
}

char *path_dirname(const char *path) {
    if (!path) {
        return NULL;
    }
    const char *last = strrchr(path, '/');
#if defined(_WIN32)
    const char *last_back = strrchr(path, '\\');
    if (!last || (last_back && last_back > last)) {
        last = last_back;
    }
#endif
    if (!last) {
        return strdup_or_null(".");
    }
    size_t len = (size_t)(last - path);
    if (len == 0) {
        len = 1;
    }
    char *out = (char *)malloc(len + 1);
    if (!out) {
        return NULL;
    }
    memcpy(out, path, len);
    out[len] = '\0';
    return out;
}

char *path_join(const char *a, const char *b) {
    if (!a || !b) {
        return NULL;
    }
    size_t alen = strlen(a);
    size_t blen = strlen(b);
    char sep = '/';
#if defined(_WIN32)
    sep = '\\';
#endif
    bool need_sep = true;
    if (alen > 0) {
        char last = a[alen - 1];
        if (last == '/' || last == '\\') {
            need_sep = false;
        }
    }
    size_t total = alen + blen + (need_sep ? 1 : 0);
    char *out = (char *)malloc(total + 1);
    if (!out) {
        return NULL;
    }
    memcpy(out, a, alen);
    size_t pos = alen;
    if (need_sep) {
        out[pos++] = sep;
    }
    memcpy(out + pos, b, blen);
    out[total] = '\0';
    return out;
}

bool path_is_file(const char *path) {
    if (!path) {
        return false;
    }
    struct stat st;
    if (stat(path, &st) != 0) {
        return false;
    }
#if defined(_WIN32)
    return (st.st_mode & _S_IFREG) != 0;
#else
    return S_ISREG(st.st_mode);
#endif
}

static char *path_normalize(const char *path) {
    if (!path) {
        return NULL;
    }
    size_t len = strlen(path);
    char *out = (char *)malloc(len + 1);
    if (!out) {
        return NULL;
    }
    for (size_t i = 0; i < len; i++) {
        char c = path[i];
        if (c == '\\') {
            c = '/';
        }
#if defined(_WIN32)
        if (c >= 'A' && c <= 'Z') {
            c = (char)(c - 'A' + 'a');
        }
#endif
        out[i] = c;
    }
    out[len] = '\0';
    return out;
}

bool path_has_prefix(const char *path, const char *dir) {
    if (!path || !dir) {
        return false;
    }
    char *norm_path = path_normalize(path);
    char *norm_dir = path_normalize(dir);
    if (!norm_path || !norm_dir) {
        free(norm_path);
        free(norm_dir);
        return false;
    }
    size_t dlen = strlen(norm_dir);
    bool has = false;
    if (strncmp(norm_path, norm_dir, dlen) == 0) {
        if (norm_path[dlen] == '\0' || norm_path[dlen] == '/') {
            has = true;
        }
    }
    free(norm_path);
    free(norm_dir);
    return has;
}
