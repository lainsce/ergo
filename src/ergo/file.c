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

static char *read_file_malloc(const char *path, size_t *out_len, Diag *err) {
    if (!path) {
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
    char *buf = (char *)malloc(len + 1);
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
        free(buf);
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

static bool append_buf(char **out, size_t *len, size_t *cap, const char *src, size_t n) {
    if (n == 0) {
        return true;
    }
    if (*len + n + 1 > *cap) {
        size_t next = *cap ? *cap * 2 : 1024;
        while (next < *len + n + 1) {
            next *= 2;
        }
        char *buf = (char *)realloc(*out, next);
        if (!buf) {
            return false;
        }
        *out = buf;
        *cap = next;
    }
    memcpy(*out + *len, src, n);
    *len += n;
    (*out)[*len] = '\0';
    return true;
}

static bool is_include_line(const char *line, size_t len, const char *directive, const char **out_path, size_t *out_len) {
    if (!line || !directive) {
        return false;
    }
    size_t i = 0;
    while (i < len && (line[i] == ' ' || line[i] == '\t')) {
        i++;
    }
    size_t dlen = strlen(directive);
    if (i + dlen > len) {
        return false;
    }
    if (memcmp(line + i, directive, dlen) != 0) {
        return false;
    }
    i += dlen;
    while (i < len && (line[i] == ' ' || line[i] == '\t')) {
        i++;
    }
    if (i >= len || line[i] != '"') {
        return false;
    }
    i++;
    size_t start = i;
    while (i < len && line[i] != '"') {
        i++;
    }
    if (i >= len) {
        return false;
    }
    *out_path = line + start;
    *out_len = i - start;
    return true;
}

static bool read_file_with_includes_rec(const char *path, const char *directive, int depth, char **out, size_t *len, size_t *cap, Diag *err) {
    if (depth > 32) {
        if (err) {
            err->path = path;
            err->line = 0;
            err->col = 0;
            err->message = "include nesting too deep";
        }
        return false;
    }
    size_t src_len = 0;
    char *src = read_file_malloc(path, &src_len, err);
    if (!src) {
        return false;
    }
    char *dir = path_dirname(path);
    size_t line_start = 0;
    for (size_t i = 0; i <= src_len; i++) {
        bool at_end = (i == src_len);
        if (at_end || src[i] == '\n') {
            size_t line_len = i - line_start;
            const char *inc_path = NULL;
            size_t inc_len = 0;
            if (is_include_line(src + line_start, line_len, directive, &inc_path, &inc_len)) {
                char *inc_name = (char *)malloc(inc_len + 1);
                if (!inc_name) {
                    free(src);
                    free(dir);
                    if (err) {
                        err->path = path;
                        err->line = 0;
                        err->col = 0;
                        err->message = "out of memory";
                    }
                    return false;
                }
                memcpy(inc_name, inc_path, inc_len);
                inc_name[inc_len] = '\0';
                char *inc_full = NULL;
                bool is_abs = false;
#if defined(_WIN32)
                if (inc_len >= 2 && inc_name[1] == ':') {
                    is_abs = true;
                }
#endif
                if (inc_name[0] == '/') {
                    is_abs = true;
                }
                if (is_abs) {
                    inc_full = strdup_or_null(inc_name);
                } else {
                    inc_full = path_join(dir ? dir : ".", inc_name);
                }
                free(inc_name);
                if (!inc_full) {
                    free(src);
                    free(dir);
                    if (err) {
                        err->path = path;
                        err->line = 0;
                        err->col = 0;
                        err->message = "out of memory";
                    }
                    return false;
                }
                bool ok = read_file_with_includes_rec(inc_full, directive, depth + 1, out, len, cap, err);
                free(inc_full);
                if (!ok) {
                    free(src);
                    free(dir);
                    return false;
                }
                if (!append_buf(out, len, cap, "\n", 1)) {
                    free(src);
                    free(dir);
                    if (err) {
                        err->path = path;
                        err->line = 0;
                        err->col = 0;
                        err->message = "out of memory";
                    }
                    return false;
                }
            } else {
                if (!append_buf(out, len, cap, src + line_start, line_len)) {
                    free(src);
                    free(dir);
                    if (err) {
                        err->path = path;
                        err->line = 0;
                        err->col = 0;
                        err->message = "out of memory";
                    }
                    return false;
                }
                if (!at_end) {
                    if (!append_buf(out, len, cap, "\n", 1)) {
                        free(src);
                        free(dir);
                        if (err) {
                            err->path = path;
                            err->line = 0;
                            err->col = 0;
                            err->message = "out of memory";
                        }
                        return false;
                    }
                }
            }
            line_start = i + 1;
        }
    }
    free(src);
    free(dir);
    return true;
}

char *read_file_with_includes(const char *path, const char *directive, Arena *arena, size_t *out_len, Diag *err) {
    if (!path || !arena || !directive) {
        return NULL;
    }
    char *out = NULL;
    size_t len = 0;
    size_t cap = 0;
    if (!read_file_with_includes_rec(path, directive, 0, &out, &len, &cap, err)) {
        free(out);
        return NULL;
    }
    char *buf = (char *)arena_alloc(arena, len + 1);
    if (!buf) {
        free(out);
        if (err) {
            err->path = path;
            err->line = 0;
            err->col = 0;
            err->message = "out of memory";
        }
        return NULL;
    }
    if (len > 0) {
        memcpy(buf, out, len);
    }
    buf[len] = '\0';
    if (out_len) {
        *out_len = len;
    }
    free(out);
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

long long path_mtime(const char *path) {
    if (!path) {
        return -1;
    }
    struct stat st;
    if (stat(path, &st) != 0) {
        return -1;
    }
    return (long long)st.st_mtime;
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
