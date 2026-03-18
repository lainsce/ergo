// ---- Yis runtime (minimal) ----
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>
#include <time.h>
#include <sys/stat.h>
#include <stddef.h>
#if defined(__APPLE__)
#include <CoreGraphics/CoreGraphics.h>
#endif

#if defined(_WIN32)
#include <windows.h>
#include <io.h>
#endif
#include <unistd.h>

static int yis_stdout_isatty = 0;

static int yis_argc = 0;
static char **yis_argv = NULL;

void yis_set_args(int argc, char **argv) {
  yis_argc = argc;
  yis_argv = argv;
}

static void yis_runtime_init(void) {
#if defined(_WIN32)
  yis_stdout_isatty = _isatty(_fileno(stdout));
#else
  yis_stdout_isatty = isatty(fileno(stdout));
#endif
  if (!yis_stdout_isatty) {
    setvbuf(stdout, NULL, _IOFBF, 1 << 16);
  }
}

typedef enum {
  EVT_NULL,
  EVT_INT,
  EVT_FLOAT,
  EVT_BOOL,
  EVT_STR,
  EVT_ARR,
  EVT_DICT,
  EVT_OBJ,
  EVT_FN
} YisTag;

typedef struct YisVal YisVal;

typedef struct YisStr {
  int ref;
  size_t len;
  char* data;
} YisStr;

typedef struct YisArr {
  int ref;
  size_t len;
  size_t cap;
  YisVal* items;
} YisArr;

struct YisVal {
  YisTag tag;
  union {
    int64_t i;
    double f;
    bool b;
    void* p;
  } as;
};

typedef struct YisDictEnt {
  YisStr* key;
  YisVal val;
} YisDictEnt;

typedef struct YisDict {
  int ref;
  size_t len;
  size_t cap;
  YisDictEnt* entries;
} YisDict;

typedef struct YisObj {
  int ref;
  void (*drop)(struct YisObj*);
} YisObj;

typedef struct YisFn {
  int ref;
  int arity;
  YisVal (*fn)(void* env, int argc, YisVal* argv);
  void* env;
  int env_size;
} YisFn;

static int stdr_len(YisVal v);
static int64_t stdr_num(YisVal v);

#define YV_NULLV ((YisVal){.tag=EVT_NULL})
#define YV_INT(x) ((YisVal){.tag=EVT_INT, .as.i=(int64_t)(x)})
#define YV_FLOAT(x) ((YisVal){.tag=EVT_FLOAT, .as.f=(double)(x)})
#define YV_BOOL(x) ((YisVal){.tag=EVT_BOOL, .as.b=(x)?true:false})
#define YV_STR(x) ((YisVal){.tag=EVT_STR, .as.p=(x)})
#define YV_ARR(x) ((YisVal){.tag=EVT_ARR, .as.p=(x)})
#define YV_DICT(x) ((YisVal){.tag=EVT_DICT, .as.p=(x)})
#define YV_OBJ(x) ((YisVal){.tag=EVT_OBJ, .as.p=(x)})
#define YV_FN(x) ((YisVal){.tag=EVT_FN, .as.p=(x)})

static void yis_trap(const char* msg) {
  fprintf(stderr, "runtime error: %s\n", msg ? msg : "unknown error");
  fprintf(stderr, "  (run with debugger for stack trace)\n");
  abort();
}

static void yis_retain_val(YisVal v);
static void yis_release_val(YisVal v);
static int64_t yis_as_int(YisVal v);

// Static constant strings (ref=INT32_MAX means never freed)
static YisStr yis_static_empty    = { INT32_MAX, 0, "" };
static YisStr yis_static_null     = { INT32_MAX, 4, "null" };
static YisStr yis_static_true     = { INT32_MAX, 4, "true" };
static YisStr yis_static_false    = { INT32_MAX, 5, "false" };
static YisStr yis_static_array    = { INT32_MAX, 7, "[array]" };
static YisStr yis_static_dict     = { INT32_MAX, 6, "[dict]" };
static YisStr yis_static_object   = { INT32_MAX, 8, "[object]" };
static YisStr yis_static_function = { INT32_MAX, 10, "[function]" };
static YisStr yis_static_unknown  = { INT32_MAX, 3, "<?>" };

static YisStr* stdr_str_lit(const char* s) {
  size_t n = strlen(s);
  YisStr* st = (YisStr*)malloc(sizeof(YisStr) + n + 1);
  st->ref = 1;
  st->len = n;
  st->data = (char*)(st + 1);
  memcpy(st->data, s, n + 1);
  return st;
}

static YisStr* stdr_str_from_parts(int n, YisVal* parts);
static YisStr* stdr_to_string(YisVal v);
static YisStr* stdr_str_from_slice(const char* s, size_t len);
static YisArr* stdr_arr_new(int n);
static void yis_arr_add(YisArr* a, YisVal v);
static YisVal yis_arr_get(YisArr* a, int64_t idx);
static void yis_arr_set(YisArr* a, int64_t idx, YisVal v);
static YisVal yis_arr_remove(YisArr* a, int64_t idx);

static YisDict* stdr_dict_new(void);
static void yis_dict_set(YisDict* d, YisVal key, YisVal val);
static YisVal yis_dict_get(YisDict* d, YisVal key);
static int yis_dict_len(YisDict* d);

static YisVal stdr_str_at(YisVal v, int64_t idx) {
  if (v.tag != EVT_STR) yis_trap("str_at expects string");
  YisStr* s = (YisStr*)v.as.p;
  if (idx < 0 || (size_t)idx >= s->len) return YV_STR(&yis_static_empty);
  return YV_STR(stdr_str_from_slice(s->data + idx, 1));
}

static YisVal stdr_slice(YisVal sv, int64_t start, int64_t end) {
  if (sv.tag != EVT_STR) yis_trap("slice expects string");
  YisStr* s = (YisStr*)sv.as.p;
  size_t len = s->len;
  if (start < 0) start = 0;
  if ((size_t)start > len) start = (int64_t)len;
  if (end < start) end = start;
  if ((size_t)end > len) end = (int64_t)len;
  size_t n = (size_t)(end - start);
  if (n == 0) return YV_STR(&yis_static_empty);
  return YV_STR(stdr_str_from_slice(s->data + start, n));
}

static YisVal stdr_str_concat(YisVal a, YisVal b) {
  YisVal parts[2] = { a, b };
  return YV_STR(stdr_str_from_parts(2, parts));
}

static int64_t stdr_char_code(YisVal cv) {
  if (cv.tag != EVT_STR) yis_trap("char_code expects string");
  YisStr* s = (YisStr*)cv.as.p;
  if (s->len == 0) return 0;
  return (unsigned char)s->data[0];
}

static int stdr_len(YisVal v) {
  if (v.tag == EVT_STR) return (int)((YisStr*)v.as.p)->len;
  if (v.tag == EVT_ARR) return (int)((YisArr*)v.as.p)->len;
  if (v.tag == EVT_DICT) return (int)((YisDict*)v.as.p)->len;
  return 0;
}

static int64_t stdr_num(YisVal v) {
  return yis_as_int(v);
}

static bool stdr_is_null(YisVal v) { return v.tag == EVT_NULL; }

static void stdr_write(YisVal v) {
  YisStr* s = stdr_to_string(v);
  fwrite(s->data, 1, s->len, stdout);
  fflush(stdout);
  yis_release_val(YV_STR(s));
}

static void writef(YisVal fmt, int argc, YisVal* argv) {
  if (fmt.tag != EVT_STR) yis_trap("writef expects string");
  YisStr* s = (YisStr*)fmt.as.p;
  size_t i = 0;
  size_t seg = 0;
  int argi = 0;
  while (i < s->len) {
    if (i + 1 < s->len && s->data[i] == '{' && s->data[i + 1] == '}') {
      if (i > seg) fwrite(s->data + seg, 1, i - seg, stdout);
      if (argi < argc) {
        YisStr* ps = stdr_to_string(argv[argi++]);
        fwrite(ps->data, 1, ps->len, stdout);
        yis_release_val(YV_STR(ps));
      }
      i += 2;
      seg = i;
      continue;
    }
    i++;
  }
  if (i > seg) fwrite(s->data + seg, 1, i - seg, stdout);
  if (yis_stdout_isatty) fflush(stdout);
}

static void stdr_writef_args(YisVal fmt, YisVal args) {
  if (args.tag != EVT_ARR) yis_trap("writef expects args tuple");
  YisArr* a = (YisArr*)args.as.p;
  writef(fmt, (int)a->len, a->items);
}

static YisStr* stdr_read_line(void) {
  size_t cap = 128;
  size_t len = 0;
  char* buf = (char*)malloc(cap);
  if (!buf) yis_trap("out of memory");
  int c;
  while ((c = fgetc(stdin)) != EOF) {
    if (c == '\n') break;
    if (len + 1 >= cap) {
      cap *= 2;
      buf = (char*)realloc(buf, cap);
      if (!buf) yis_trap("out of memory");
    }
    buf[len++] = (char)c;
  }
  if (len > 0 && buf[len - 1] == '\r') len--;
  YisStr* s = (YisStr*)malloc(sizeof(YisStr) + len + 1);
  if (!s) yis_trap("out of memory");
  s->ref = 1;
  s->len = len;
  s->data = (char*)(s + 1);
  memcpy(s->data, buf, len);
  s->data[len] = 0;
  free(buf);
  return s;
}

static YisVal stdr_read_text_file(YisVal pathv) {
  if (pathv.tag != EVT_STR) yis_trap("read_text_file expects string path");
  YisStr* path = (YisStr*)pathv.as.p;
  FILE* f = fopen(path->data, "rb");
  if (!f) return YV_NULLV;
  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return YV_NULLV;
  }
  long sz = ftell(f);
  if (sz < 0) {
    fclose(f);
    return YV_NULLV;
  }
  if (fseek(f, 0, SEEK_SET) != 0) {
    fclose(f);
    return YV_NULLV;
  }
  size_t len = (size_t)sz;
  YisStr* out = (YisStr*)malloc(sizeof(YisStr) + len + 1);
  if (!out) {
    fclose(f);
    yis_trap("out of memory");
  }
  out->data = (char*)(out + 1);
  size_t n = 0;
  if (len > 0) n = fread(out->data, 1, len, f);
  fclose(f);
  if (n != len) {
    free(out);
    return YV_NULLV;
  }
  out->ref = 1;
  out->len = len;
  out->data[len] = 0;
  return YV_STR(out);
}

static YisVal stdr_write_text_file(YisVal pathv, YisVal textv) {
  if (pathv.tag != EVT_STR) yis_trap("write_text_file expects string path");
  if (textv.tag != EVT_STR) yis_trap("write_text_file expects string text");
  YisStr* path = (YisStr*)pathv.as.p;
  YisStr* text = (YisStr*)textv.as.p;
  FILE* f = fopen(path->data, "wb");
  if (!f) return YV_BOOL(false);
  size_t n = 0;
  if (text->len > 0) n = fwrite(text->data, 1, text->len, f);
  bool ok = (n == text->len) && (fclose(f) == 0);
  return YV_BOOL(ok);
}

static YisVal stdr_run_command(YisVal cmdv) {
  if (cmdv.tag != EVT_STR) yis_trap("run_command expects string");
  YisStr* cmd = (YisStr*)cmdv.as.p;
#if defined(_WIN32)
  FILE* p = _popen(cmd->data, "r");
#else
  FILE* p = popen(cmd->data, "r");
#endif
  if (!p) {
    YisArr* out = stdr_arr_new(2);
    yis_arr_add(out, YV_INT(-1));
    yis_arr_add(out, YV_STR(stdr_str_lit("")));
    return YV_ARR(out);
  }
  size_t cap = 4096;
  size_t len = 0;
  char* buf = (char*)malloc(cap);
  if (!buf) {
#if defined(_WIN32)
    _pclose(p);
#else
    pclose(p);
#endif
    yis_trap("out of memory");
  }
  int c;
  while ((c = fgetc(p)) != EOF) {
    if (len + 1 >= cap) {
      cap *= 2;
      char* n = (char*)realloc(buf, cap);
      if (!n) {
        free(buf);
#if defined(_WIN32)
        _pclose(p);
#else
        pclose(p);
#endif
        yis_trap("out of memory");
      }
      buf = n;
    }
    buf[len++] = (char)c;
  }
  buf[len] = '\0';
#if defined(_WIN32)
  int status = _pclose(p);
#else
  int status = pclose(p);
#endif
  YisStr* out_str = stdr_str_from_slice(buf, len);
  free(buf);
  YisArr* out = stdr_arr_new(2);
  yis_arr_add(out, YV_INT(status));
  yis_arr_add(out, YV_STR(out_str));
  YisVal rv; rv.tag = EVT_ARR; rv.as.p = out;
  return rv;
}

static YisVal stdr_file_exists(YisVal pathv) {
  if (pathv.tag != EVT_STR) yis_trap("file_exists expects string");
  YisStr* s = (YisStr*)pathv.as.p;
  struct stat st;
  return YV_BOOL(stat(s->data, &st) == 0 && S_ISREG(st.st_mode));
}

static YisVal stdr_file_mtime(YisVal pathv) {
  if (pathv.tag != EVT_STR) yis_trap("file_mtime expects string");
  YisStr* s = (YisStr*)pathv.as.p;
  struct stat st;
  if (stat(s->data, &st) != 0) return YV_INT(-1);
#if defined(__APPLE__)
  return YV_INT((int64_t)st.st_mtimespec.tv_sec);
#elif defined(_WIN32)
  return YV_INT((int64_t)st.st_mtime);
#else
  return YV_INT((int64_t)st.st_mtim.tv_sec);
#endif
}

static YisVal stdr_getcwd(void) {
  char buf[4096];
  if (getcwd(buf, sizeof(buf))) return YV_STR(stdr_str_from_slice(buf, strlen(buf)));
  return YV_STR(stdr_str_from_slice("", 0));
}

static YisVal stdr_capture_shell_first_line(const char* cmd) {
  if (!cmd || !cmd[0]) return YV_NULLV;
#if defined(_WIN32)
  FILE* p = _popen(cmd, "r");
#else
  FILE* p = popen(cmd, "r");
#endif
  if (!p) return YV_NULLV;
  char buf[4096];
  if (!fgets(buf, sizeof(buf), p)) {
#if defined(_WIN32)
    _pclose(p);
#else
    pclose(p);
#endif
    return YV_NULLV;
  }
#if defined(_WIN32)
  _pclose(p);
#else
  pclose(p);
#endif
  size_t len = strlen(buf);
  while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) len--;
  if (len == 0) return YV_NULLV;
  return YV_STR(stdr_str_from_slice(buf, len));
}

static YisVal stdr_open_file_dialog(YisVal promptv, YisVal extv) {
  if (promptv.tag != EVT_STR) yis_trap("open_file_dialog expects prompt string");
  if (extv.tag != EVT_STR) yis_trap("open_file_dialog expects extension string");
  YisStr* prompt = (YisStr*)promptv.as.p;
  YisStr* ext = (YisStr*)extv.as.p;
#if defined(__APPLE__)
  char cmd[8192];
  snprintf(cmd, sizeof(cmd),
           "osascript -e 'set _p to POSIX path of (choose file of type {\"%s\"} with prompt \"%s\")' -e 'return _p' 2>/dev/null",
           ext ? ext->data : "", prompt ? prompt->data : "");
  return stdr_capture_shell_first_line(cmd);
#else
  (void)prompt;
  (void)ext;
  return YV_NULLV;
#endif
}

static YisVal stdr_save_file_dialog(YisVal promptv, YisVal default_namev, YisVal extv) {
  if (promptv.tag != EVT_STR) yis_trap("save_file_dialog expects prompt string");
  if (default_namev.tag != EVT_STR) yis_trap("save_file_dialog expects default_name string");
  if (extv.tag != EVT_STR) yis_trap("save_file_dialog expects extension string");
  YisStr* prompt = (YisStr*)promptv.as.p;
  YisStr* def = (YisStr*)default_namev.as.p;
  YisStr* ext = (YisStr*)extv.as.p;
#if defined(__APPLE__)
  char cmd[8192];
  snprintf(cmd, sizeof(cmd),
           "osascript -e 'set _p to POSIX path of (choose file name with prompt \"%s\" default name \"%s\")' -e 'return _p' 2>/dev/null",
           prompt ? prompt->data : "", def ? def->data : "");
  YisVal out = stdr_capture_shell_first_line(cmd);
  (void)ext;
  return out;
#else
  (void)prompt;
  (void)def;
  (void)ext;
  return YV_NULLV;
#endif
}

static size_t stdr_find_sub(const char* s, size_t slen, const char* sub, size_t sublen, size_t start) {
  if (sublen == 0) return start;
  if (start > slen) return (size_t)-1;
  for (size_t i = start; i + sublen <= slen; i++) {
    if (memcmp(s + i, sub, sublen) == 0) return i;
  }
  return (size_t)-1;
}

static void stdr_trim_span(const char* s, size_t len, size_t* out_start, size_t* out_len) {
  size_t a = 0;
  while (a < len && (s[a] == ' ' || s[a] == '\t')) a++;
  size_t b = len;
  while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t')) b--;
  *out_start = a;
  *out_len = b - a;
}

static YisStr* stdr_str_from_slice(const char* s, size_t len) {
  YisStr* st = (YisStr*)malloc(sizeof(YisStr) + len + 1);
  if (!st) yis_trap("out of memory");
  st->ref = 1;
  st->len = len;
  st->data = (char*)(st + 1);
  if (len > 0) memcpy(st->data, s, len);
  st->data[len] = 0;
  return st;
}

static YisVal stdr_args(void) {
  YisArr* a = stdr_arr_new(yis_argc > 0 ? yis_argc : 1);
  for (int i = 0; i < yis_argc; i++) {
    const char* s = yis_argv && yis_argv[i] ? yis_argv[i] : "";
    size_t len = strlen(s);
    yis_arr_add(a, YV_STR(stdr_str_from_slice(s, len)));
  }
  return YV_ARR(a);
}

static int64_t stdr_parse_int_slice(const char* s, size_t len) {
  if (len == 0) return 0;
  char stack[64];
  char* tmp = (len < sizeof(stack)) ? stack : (char*)malloc(len + 1);
  if (!tmp) yis_trap("out of memory");
  memcpy(tmp, s, len);
  tmp[len] = 0;
  char* end = NULL;
  long long v = strtoll(tmp, &end, 10);
  if (tmp != stack) free(tmp);
  if (end == tmp) return 0;
  return (int64_t)v;
}

static double stdr_parse_float_slice(const char* s, size_t len) {
  if (len == 0) return 0.0;
  char stack[64];
  char* tmp = (len < sizeof(stack)) ? stack : (char*)malloc(len + 1);
  if (!tmp) yis_trap("out of memory");
  memcpy(tmp, s, len);
  tmp[len] = 0;
  char* end = NULL;
  double v = strtod(tmp, &end);
  if (tmp != stack) free(tmp);
  if (end == tmp) return 0.0;
  return v;
}

static bool stdr_parse_bool_slice(const char* s, size_t len) {
  if (len == 1) {
    if (s[0] == '1') return true;
    if (s[0] == '0') return false;
  }
  if (len == 4) {
    return ((s[0] == 't' || s[0] == 'T') &&
            (s[1] == 'r' || s[1] == 'R') &&
            (s[2] == 'u' || s[2] == 'U') &&
            (s[3] == 'e' || s[3] == 'E'));
  }
  return false;
}

static YisVal stdr_readf_parse(YisVal fmt, YisVal line, YisVal args) {
  if (fmt.tag != EVT_STR) yis_trap("readf expects string format");
  if (line.tag != EVT_STR) yis_trap("readf expects string input");
  if (args.tag != EVT_ARR) yis_trap("readf expects args tuple");

  YisStr* fs = (YisStr*)fmt.as.p;
  YisStr* ls = (YisStr*)line.as.p;
  YisArr* a = (YisArr*)args.as.p;

  const char* f = fs->data;
  size_t flen = fs->len;
  const char* s = ls->data;
  size_t slen = ls->len;

  int segs = 1;
  for (size_t i = 0; i + 1 < flen; i++) {
    if (f[i] == '{' && f[i + 1] == '}') {
      segs++;
      i++;
    }
  }

  const char* stack_ptrs[16];
  size_t stack_lens[16];
  const char** seg_ptrs = (segs <= 16) ? stack_ptrs : (const char**)malloc(sizeof(char*) * segs);
  size_t* seg_lens = (segs <= 16) ? stack_lens : (size_t*)malloc(sizeof(size_t) * segs);
  if (!seg_ptrs || !seg_lens) yis_trap("out of memory");

  size_t seg_start = 0;
  int seg_idx = 0;
  for (size_t i = 0; i + 1 < flen; i++) {
    if (f[i] == '{' && f[i + 1] == '}') {
      seg_ptrs[seg_idx] = f + seg_start;
      seg_lens[seg_idx] = i - seg_start;
      seg_idx++;
      i++;
      seg_start = i + 1;
    }
  }
  seg_ptrs[seg_idx] = f + seg_start;
  seg_lens[seg_idx] = flen - seg_start;

  int placeholders = segs - 1;

  size_t spos = 0;
  if (seg_lens[0] > 0) {
    size_t found = stdr_find_sub(s, slen, seg_ptrs[0], seg_lens[0], 0);
    if (found != (size_t)-1) spos = found + seg_lens[0];
  }

  YisArr* out = stdr_arr_new((int)a->len);

  for (size_t i = 0; i < a->len; i++) {
    size_t cap_start = spos;
    size_t cap_len = 0;
    if ((int)i < placeholders) {
      size_t found = stdr_find_sub(s, slen, seg_ptrs[i + 1], seg_lens[i + 1], spos);
      if (found == (size_t)-1) {
        cap_len = slen - spos;
        spos = slen;
      } else {
        cap_len = found - spos;
        spos = found + seg_lens[i + 1];
      }
    }

    size_t trim_start = 0;
    size_t trim_len = cap_len;
    stdr_trim_span(s + cap_start, cap_len, &trim_start, &trim_len);
    const char* cap = (cap_len > 0) ? (s + cap_start + trim_start) : "";

    YisVal hint = a->items[i];
    YisVal v;
    if (hint.tag == EVT_INT) {
      v = YV_INT(stdr_parse_int_slice(cap, trim_len));
    } else if (hint.tag == EVT_FLOAT) {
      v = YV_FLOAT(stdr_parse_float_slice(cap, trim_len));
    } else if (hint.tag == EVT_BOOL) {
      v = YV_BOOL(stdr_parse_bool_slice(cap, trim_len));
    } else if (hint.tag == EVT_STR) {
      v = YV_STR(stdr_str_from_slice(cap, trim_len));
    } else {
      v = YV_STR(stdr_str_from_slice(cap, trim_len));
    }
    yis_arr_add(out, v);
  }

  if (seg_ptrs != stack_ptrs) free(seg_ptrs);
  if (seg_lens != stack_lens) free(seg_lens);

  return YV_ARR(out);
}

static YisStr* stdr_to_string(YisVal v) {
  char buf[64];
  if (v.tag == EVT_NULL) return &yis_static_null;
  if (v.tag == EVT_BOOL) return v.as.b ? &yis_static_true : &yis_static_false;
  if (v.tag == EVT_INT) {
    snprintf(buf, sizeof(buf), "%lld", (long long)v.as.i);
    return stdr_str_lit(buf);
  }
  if (v.tag == EVT_FLOAT) {
    snprintf(buf, sizeof(buf), "%.6f", v.as.f);
    return stdr_str_lit(buf);
  }
  if (v.tag == EVT_STR) {
    yis_retain_val(v);
    return (YisStr*)v.as.p;
  }
  if (v.tag == EVT_ARR) return &yis_static_array;
  if (v.tag == EVT_DICT) return &yis_static_dict;
  if (v.tag == EVT_OBJ) return &yis_static_object;
  if (v.tag == EVT_FN) return &yis_static_function;
  return &yis_static_unknown;
}

static YisStr* stdr_str_from_parts(int n, YisVal* parts) {
  size_t total = 0;
  YisStr* stack_strs[16];
  YisStr** strs = (n <= 16) ? stack_strs : (YisStr**)malloc(sizeof(YisStr*) * (size_t)n);
  for (int i = 0; i < n; i++) {
    strs[i] = stdr_to_string(parts[i]);
    total += strs[i]->len;
  }
  YisStr* out = (YisStr*)malloc(sizeof(YisStr) + total + 1);
  out->ref = 1;
  out->len = total;
  out->data = (char*)(out + 1);
  size_t off = 0;
  for (int i = 0; i < n; i++) {
    memcpy(out->data + off, strs[i]->data, strs[i]->len);
    off += strs[i]->len;
    yis_release_val(YV_STR(strs[i]));
  }
  out->data[total] = 0;
  if (strs != stack_strs) free(strs);
  return out;
}

static void yis_retain_val(YisVal v) {
  if (v.tag == EVT_STR) { int* r = &((YisStr*)v.as.p)->ref; if (*r != INT32_MAX) (*r)++; }
  else if (v.tag == EVT_ARR) ((YisArr*)v.as.p)->ref++;
  else if (v.tag == EVT_DICT) ((YisDict*)v.as.p)->ref++;
  else if (v.tag == EVT_OBJ) ((YisObj*)v.as.p)->ref++;
  else if (v.tag == EVT_FN) ((YisFn*)v.as.p)->ref++;
}

static void yis_release_val(YisVal v) {
  if (v.tag == EVT_STR) {
    YisStr* s = (YisStr*)v.as.p;
    if (s->ref == INT32_MAX) return;
    if (--s->ref == 0) {
      if (s->data != (char*)(s + 1)) free(s->data);
      free(s);
    }
  } else if (v.tag == EVT_ARR) {
    YisArr* a = (YisArr*)v.as.p;
    if (--a->ref == 0) {
      for (size_t i = 0; i < a->len; i++) yis_release_val(a->items[i]);
      free(a->items);
      free(a);
    }
  } else if (v.tag == EVT_DICT) {
    YisDict* d = (YisDict*)v.as.p;
    if (--d->ref == 0) {
      for (size_t i = 0; i < d->len; i++) {
        yis_release_val(YV_STR(d->entries[i].key));
        yis_release_val(d->entries[i].val);
      }
      free(d->entries);
      free(d);
    }
  } else if (v.tag == EVT_OBJ) {
    YisObj* o = (YisObj*)v.as.p;
    if (--o->ref == 0) {
      if (o->drop) o->drop(o);
      free(o);
    }
  } else if (v.tag == EVT_FN) {
    YisFn* f = (YisFn*)v.as.p;
    if (--f->ref == 0) {
      if (f->env && f->env_size > 0) {
        YisVal* caps = (YisVal*)f->env;
        for (int i = 0; i < f->env_size; i++) yis_release_val(caps[i]);
        free(f->env);
      }
      free(f);
    }
  }
}

static YisVal yis_move(YisVal* slot) {
  YisVal v = *slot;
  *slot = YV_NULLV;
  return v;
}

static void yis_move_into(YisVal* slot, YisVal v) {
  yis_retain_val(v);
  yis_release_val(*slot);
  *slot = v;
}

static int64_t yis_as_int(YisVal v) {
  if (v.tag == EVT_INT) return v.as.i;
  if (v.tag == EVT_BOOL) return v.as.b ? 1 : 0;
  if (v.tag == EVT_FLOAT) return (int64_t)v.as.f;
  if (v.tag == EVT_STR) {
    /* Coerce string to int when possible; otherwise 0 (avoids trap until codegen is fully audited) */
    YisStr *s = (YisStr*)v.as.p;
    if (s && s->len > 0 && s->len < 32) {
      char buf[32];
      memcpy(buf, s->data, s->len);
      buf[s->len] = '\0';
      char *end = NULL;
      long x = strtol(buf, &end, 10);
      if (end && end == buf + (ptrdiff_t)s->len)
        return (int64_t)x;
    }
    return 0;
  }
  {
    const char *tag_name = "?";
    switch (v.tag) {
      case EVT_NULL: tag_name = "null"; break;
      case EVT_STR: tag_name = "string"; break;
      case EVT_ARR: tag_name = "array"; break;
      case EVT_DICT: tag_name = "dict"; break;
      case EVT_OBJ: tag_name = "object"; break;
      case EVT_FN: tag_name = "function"; break;
      default: tag_name = "unknown"; break;
    }
    char buf[80];
    snprintf(buf, sizeof(buf), "type mismatch: expected int (got %s)", tag_name);
    yis_trap(buf);
  }
  return 0;
}

static double yis_as_float(YisVal v) {
  if (v.tag == EVT_FLOAT) return v.as.f;
  if (v.tag == EVT_INT) return (double)v.as.i;
  yis_trap("type mismatch: expected float");
  return 0.0;
}

static bool yis_as_bool(YisVal v) {
  if (v.tag == EVT_BOOL) return v.as.b;
  if (v.tag == EVT_NULL) return false;
  if (v.tag == EVT_INT) return v.as.i != 0;
  if (v.tag == EVT_FLOAT) return v.as.f != 0.0;
  if (v.tag == EVT_STR) return ((YisStr*)v.as.p)->len != 0;
  if (v.tag == EVT_ARR) return ((YisArr*)v.as.p)->len != 0;
  if (v.tag == EVT_DICT) return ((YisDict*)v.as.p)->len != 0;
  return true;
}

static YisVal yis_add(YisVal a, YisVal b) {
  if (a.tag == EVT_STR || b.tag == EVT_STR) return stdr_str_concat(a, b);
  if (a.tag == EVT_FLOAT || b.tag == EVT_FLOAT) return YV_FLOAT(yis_as_float(a) + yis_as_float(b));
  return YV_INT(yis_as_int(a) + yis_as_int(b));
}

static YisVal yis_sub(YisVal a, YisVal b) {
  if (a.tag == EVT_FLOAT || b.tag == EVT_FLOAT) return YV_FLOAT(yis_as_float(a) - yis_as_float(b));
  return YV_INT(yis_as_int(a) - yis_as_int(b));
}

static YisVal yis_mul(YisVal a, YisVal b) {
  if (a.tag == EVT_FLOAT || b.tag == EVT_FLOAT) return YV_FLOAT(yis_as_float(a) * yis_as_float(b));
  return YV_INT(yis_as_int(a) * yis_as_int(b));
}

static YisVal yis_div(YisVal a, YisVal b) {
  if (a.tag == EVT_FLOAT || b.tag == EVT_FLOAT) return YV_FLOAT(yis_as_float(a) / yis_as_float(b));
  return YV_INT(yis_as_int(a) / yis_as_int(b));
}

static YisVal yis_mod(YisVal a, YisVal b) {
  if (a.tag == EVT_FLOAT || b.tag == EVT_FLOAT) yis_trap("% expects integer");
  return YV_INT(yis_as_int(a) % yis_as_int(b));
}

static YisVal yis_neg(YisVal a) {
  if (a.tag == EVT_FLOAT) return YV_FLOAT(-a.as.f);
  return YV_INT(-yis_as_int(a));
}

static YisVal yis_eq(YisVal a, YisVal b) {
  if (a.tag != b.tag) return YV_BOOL(false);
  switch (a.tag) {
    case EVT_NULL: return YV_BOOL(true);
    case EVT_BOOL: return YV_BOOL(a.as.b == b.as.b);
    case EVT_INT: return YV_BOOL(a.as.i == b.as.i);
    case EVT_FLOAT: return YV_BOOL(a.as.f == b.as.f);
    case EVT_STR: {
      YisStr* sa = (YisStr*)a.as.p;
      YisStr* sb = (YisStr*)b.as.p;
      if (sa->len != sb->len) return YV_BOOL(false);
      return YV_BOOL(memcmp(sa->data, sb->data, sa->len) == 0);
    }
    default: return YV_BOOL(a.as.p == b.as.p);
  }
}

static YisVal yis_ne(YisVal a, YisVal b) {
  YisVal v = yis_eq(a, b);
  return YV_BOOL(!v.as.b);
}

static YisVal yis_lt(YisVal a, YisVal b) { return YV_BOOL(yis_as_float(a) < yis_as_float(b)); }
static YisVal yis_le(YisVal a, YisVal b) { return YV_BOOL(yis_as_float(a) <= yis_as_float(b)); }
static YisVal yis_gt(YisVal a, YisVal b) { return YV_BOOL(yis_as_float(a) > yis_as_float(b)); }
static YisVal yis_ge(YisVal a, YisVal b) { return YV_BOOL(yis_as_float(a) >= yis_as_float(b)); }

static YisArr* stdr_arr_new(int n) {
  YisArr* a = (YisArr*)malloc(sizeof(YisArr));
  a->ref = 1;
  a->len = 0;
  a->cap = (n > 0) ? (size_t)n : 4;
  a->items = (YisVal*)malloc(sizeof(YisVal) * a->cap);
  return a;
}

static void yis_arr_add(YisArr* a, YisVal v) {
  if (a->len >= a->cap) {
    a->cap *= 2;
    a->items = (YisVal*)realloc(a->items, sizeof(YisVal) * a->cap);
  }
  a->items[a->len++] = v;
}

static void stdr_push(YisVal av, YisVal val) {
  if (av.tag != EVT_ARR) yis_trap("push expects array");
  YisArr* a = (YisArr*)av.as.p;
  yis_retain_val(val);
  yis_arr_add(a, val);
}

static YisVal stdr_join(YisVal av) {
  if (av.tag != EVT_ARR) yis_trap("join expects array");
  YisArr* a = (YisArr*)av.as.p;
  return YV_STR(stdr_str_from_parts((int)a->len, a->items));
}

static YisVal stdr_array_concat(YisVal av, YisVal bv) {
  if (av.tag != EVT_ARR || bv.tag != EVT_ARR) yis_trap("concat expects two arrays");
  YisArr* a = (YisArr*)av.as.p;
  YisArr* b = (YisArr*)bv.as.p;
  YisArr* out = stdr_arr_new((int)(a->len + b->len));
  for (size_t i = 0; i < a->len; i++) {
    yis_retain_val(a->items[i]);
    yis_arr_add(out, a->items[i]);
  }
  for (size_t i = 0; i < b->len; i++) {
    yis_retain_val(b->items[i]);
    yis_arr_add(out, b->items[i]);
  }
  return YV_ARR(out);
}

static YisVal yis_arr_get(YisArr* a, int64_t idx) {
  if (!a) yis_trap("array index on null");
  if (idx < 0 || (size_t)idx >= a->len) return YV_NULLV;
  YisVal v = a->items[idx];
  yis_retain_val(v);
  return v;
}

static void yis_arr_set(YisArr* a, int64_t idx, YisVal v) {
  if (idx < 0) return;
  size_t uidx = (size_t)idx;
  if (uidx >= a->len) {
    if (uidx >= a->cap) {
      size_t new_cap = a->cap ? a->cap : 4;
      while (new_cap <= uidx) new_cap *= 2;
      a->items = (YisVal*)realloc(a->items, sizeof(YisVal) * new_cap);
      a->cap = new_cap;
    }
    for (size_t i = a->len; i <= uidx; i++) a->items[i] = YV_NULLV;
    a->len = uidx + 1;
  } else {
    yis_release_val(a->items[uidx]);
  }
  yis_retain_val(v);
  a->items[uidx] = v;
}

static YisVal yis_arr_remove(YisArr* a, int64_t idx) {
  if (idx < 0 || (size_t)idx >= a->len) return YV_NULLV;
  YisVal v = a->items[idx];
  for (size_t i = (size_t)idx; i + 1 < a->len; i++) {
    a->items[i] = a->items[i + 1];
  }
  a->len--;
  return v;
}

static int yis_str_cmp(YisStr* a, YisStr* b) {
  if (a->len != b->len) return (a->len > b->len) ? 1 : -1;
  return memcmp(a->data, b->data, a->len);
}

static YisDict* stdr_dict_new(void) {
  YisDict* d = (YisDict*)malloc(sizeof(YisDict));
  d->ref = 1;
  d->len = 0;
  d->cap = 8;
  d->entries = (YisDictEnt*)malloc(sizeof(YisDictEnt) * d->cap);
  return d;
}

static void yis_dict_set(YisDict* d, YisVal key, YisVal val) {
  if (key.tag != EVT_STR) yis_trap("dict key must be string");
  YisStr* k = (YisStr*)key.as.p;
  for (size_t i = 0; i < d->len; i++) {
    if (yis_str_cmp(d->entries[i].key, k) == 0) {
      yis_retain_val(val);
      yis_release_val(d->entries[i].val);
      d->entries[i].val = val;
      return;
    }
  }
  if (d->len >= d->cap) {
    d->cap *= 2;
    d->entries = (YisDictEnt*)realloc(d->entries, sizeof(YisDictEnt) * d->cap);
  }
  yis_retain_val(key);
  yis_retain_val(val);
  d->entries[d->len].key = k;
  d->entries[d->len].val = val;
  d->len++;
}

static YisVal yis_dict_get(YisDict* d, YisVal key) {
  if (!d) return YV_NULLV;
  if ((uintptr_t)d < 4096u) return YV_NULLV;
  if (key.tag != EVT_STR) return YV_NULLV;
  YisStr* k = (YisStr*)key.as.p;
  if (!k) return YV_NULLV;
  if ((uintptr_t)k < 4096u) return YV_NULLV;
  for (size_t i = 0; i < d->len; i++) {
    if (yis_str_cmp(d->entries[i].key, k) == 0) {
      YisVal v = d->entries[i].val;
      yis_retain_val(v);
      return v;
    }
  }
  return YV_NULLV;
}

static int yis_dict_len(YisDict* d) {
  return (int)d->len;
}

static YisObj* yis_obj_new(size_t size, void (*drop)(YisObj*)) {
  YisObj* o = (YisObj*)malloc(size);
  o->ref = 1;
  o->drop = drop;
  return o;
}

static YisFn* yi_fn_new(YisVal (*fn)(void* env, int argc, YisVal* argv), int arity) {
  YisFn* f = (YisFn*)malloc(sizeof(YisFn));
  f->ref = 1;
  f->arity = arity;
  f->fn = fn;
  f->env = NULL;
  f->env_size = 0;
  return f;
}

static YisFn* yi_fn_new_with_env(YisVal (*fn)(void* env, int argc, YisVal* argv), int arity, void* env, int env_size) {
  YisFn* f = (YisFn*)malloc(sizeof(YisFn));
  f->ref = 1;
  f->arity = arity;
  f->fn = fn;
  f->env = env;
  f->env_size = env_size;
  return f;
}

static YisVal yis_call(YisVal fval, int argc, YisVal* argv) {
  if (fval.tag != EVT_FN) yis_trap("call expects function");
  YisFn* f = (YisFn*)fval.as.p;
  if (f->arity >= 0 && f->arity != argc) yis_trap("arity mismatch");
  return f->fn(f->env, argc, argv);
}

static YisVal stdr_parse_hex(YisVal sv) {
  if (sv.tag != EVT_STR) yis_trap("parse_hex expects string");
  YisStr* s = (YisStr*)sv.as.p;
  if (s->len == 0) return YV_INT(0);
  char* end = NULL;
  long long v = strtoll(s->data, &end, 16);
  if (end == s->data) return YV_INT(0);
  return YV_INT((int64_t)v);
}

static YisVal stdr_char_from_code(YisVal cv) {
  int64_t code = yis_as_int(cv);
  if (code < 0 || code > 0x10FFFF) return YV_STR(&yis_static_empty);
  char buf[4];
  int len = 0;
  if (code <= 0x7F) {
    buf[0] = (char)code; len = 1;
  } else if (code <= 0x7FF) {
    buf[0] = (char)(0xC0 | ((code >> 6) & 0x1F));
    buf[1] = (char)(0x80 | (code & 0x3F));
    len = 2;
  } else if (code <= 0xFFFF) {
    buf[0] = (char)(0xE0 | ((code >> 12) & 0x0F));
    buf[1] = (char)(0x80 | ((code >> 6) & 0x3F));
    buf[2] = (char)(0x80 | (code & 0x3F));
    len = 3;
  } else {
    buf[0] = (char)(0xF0 | ((code >> 18) & 0x07));
    buf[1] = (char)(0x80 | ((code >> 12) & 0x3F));
    buf[2] = (char)(0x80 | ((code >> 6) & 0x3F));
    buf[3] = (char)(0x80 | (code & 0x3F));
    len = 4;
  }
  return YV_STR(stdr_str_from_slice(buf, (size_t)len));
}

static YisVal stdr_floor(YisVal v) {
  double x = (v.tag == EVT_FLOAT) ? v.as.f : (double)yis_as_int(v);
  return YV_INT((int64_t)floor(x));
}

static YisVal stdr_ceil(YisVal v) {
  double x = (v.tag == EVT_FLOAT) ? v.as.f : (double)yis_as_int(v);
  return YV_INT((int64_t)ceil(x));
}

static YisVal stdr_keys(YisVal dv) {
  if (dv.tag != EVT_DICT) yis_trap("keys expects dict");
  YisDict* d = (YisDict*)dv.as.p;
  YisArr* out = stdr_arr_new((int)d->len);
  for (size_t i = 0; i < d->len; i++) {
    YisVal k = YV_STR(d->entries[i].key);
    yis_retain_val(k);
    yis_arr_add(out, k);
  }
  return YV_ARR(out);
}

static YisVal stdr_replace(YisVal textv, YisVal fromv, YisVal tov) {
  if (textv.tag != EVT_STR) yis_trap("replace expects string");
  if (fromv.tag != EVT_STR) yis_trap("replace expects string");
  if (tov.tag != EVT_STR) yis_trap("replace expects string");
  YisStr* text = (YisStr*)textv.as.p;
  YisStr* from = (YisStr*)fromv.as.p;
  YisStr* to = (YisStr*)tov.as.p;
  if (from->len == 0) { yis_retain_val(textv); return textv; }
  size_t cap = text->len + 64;
  char* buf = (char*)malloc(cap);
  if (!buf) yis_trap("out of memory");
  size_t out_len = 0;
  size_t i = 0;
  while (i + from->len <= text->len) {
    if (memcmp(text->data + i, from->data, from->len) == 0) {
      size_t need = out_len + to->len;
      if (need >= cap) {
        while (need >= cap) cap *= 2;
        buf = (char*)realloc(buf, cap);
        if (!buf) yis_trap("out of memory");
      }
      memcpy(buf + out_len, to->data, to->len);
      out_len += to->len;
      i += from->len;
    } else {
      if (out_len + 1 >= cap) {
        cap *= 2;
        buf = (char*)realloc(buf, cap);
        if (!buf) yis_trap("out of memory");
      }
      buf[out_len++] = text->data[i++];
    }
  }
  size_t tail = text->len - i;
  if (tail > 0) {
    if (out_len + tail >= cap) {
      cap = out_len + tail + 1;
      buf = (char*)realloc(buf, cap);
      if (!buf) yis_trap("out of memory");
    }
    memcpy(buf + out_len, text->data + i, tail);
    out_len += tail;
  }
  buf[out_len] = 0;
  YisStr* result = stdr_str_from_slice(buf, out_len);
  free(buf);
  return YV_STR(result);
}

// ---- External module bindings ----
// Injected by codegen when the program imports an external module.

/* inlined runtime helpers */
static YisVal yis_index(YisVal obj, YisVal idx) {
  if (obj.tag == EVT_ARR) return yis_arr_get((YisArr*)obj.as.p, yis_as_int(idx));
  if (obj.tag == EVT_DICT) return yis_dict_get((YisDict*)obj.as.p, idx);
  if (obj.tag == EVT_STR) return stdr_str_at(obj, yis_as_int(idx));
  return YV_NULLV;
}

static YisVal yis_index_set(YisVal obj, YisVal idx, YisVal val) {
  if (obj.tag == EVT_ARR) { yis_arr_set((YisArr*)obj.as.p, yis_as_int(idx), val); return val; }
  if (obj.tag == EVT_DICT) { yis_dict_set((YisDict*)obj.as.p, idx, val); return val; }
  return YV_NULLV;
}

static YisVal yis_arr_lit(int n, ...) {
  YisArr *a = stdr_arr_new(n);
  va_list ap; va_start(ap, n);
  for (int i = 0; i < n; i++) yis_arr_add(a, va_arg(ap, YisVal));
  va_end(ap);
  YisVal av; av.tag = EVT_ARR; av.as.p = a;
  return av;
}

static YisVal yis_dict_lit(int n, ...) {
  YisDict *d = stdr_dict_new();
  va_list ap; va_start(ap, n);
  for (int i = 0; i < n; i++) {
    YisVal k = va_arg(ap, YisVal);
    YisVal v = va_arg(ap, YisVal);
    yis_dict_set(d, k, v);
  }
  va_end(ap);
  YisVal dv; dv.tag = EVT_DICT; dv.as.p = d;
  return dv;
}


/* begin embedded module: stdr */
static void yis_stdr_writef(YisVal, YisVal);
static YisVal yis_stdr_readf(YisVal, YisVal);
static void yis_stdr_write(YisVal);
static YisVal yis_stdr_is_null(YisVal);
static YisVal yis_stdr_str(YisVal);
static YisVal yis_stdr_len(YisVal);
static YisVal yis_stdr_num(YisVal);
static YisVal yis_stdr_slice(YisVal, YisVal, YisVal);
static YisVal yis_stdr_concat(YisVal, YisVal);
static YisVal yis_stdr_join(YisVal);
static void yis_stdr_push(YisVal, YisVal);
static YisVal yis_stdr_str_concat(YisVal, YisVal);
static YisVal yis_stdr_char_code(YisVal);
static YisVal yis_stdr_char_from_code(YisVal);
static YisVal yis_stdr_char_at(YisVal, YisVal);
static YisVal yis_stdr_substring(YisVal, YisVal);
static YisVal yis_stdr_substring_len(YisVal, YisVal, YisVal);
static YisVal yis_stdr_replace(YisVal, YisVal, YisVal);
static YisVal yis_stdr_parse_hex(YisVal);
static YisVal yis_stdr_floor(YisVal);
static YisVal yis_stdr_ceil(YisVal);
static YisVal yis_stdr_keys(YisVal);
static YisVal yis_stdr_read_text_file(YisVal);
static YisVal yis_stdr_write_text_file(YisVal, YisVal);
static YisVal yis_stdr_run_command(YisVal);
static YisVal yis_stdr_file_exists(YisVal);
static YisVal yis_stdr_file_mtime(YisVal);
static YisVal yis_stdr_getcwd(void);
static YisVal yis_stdr_is_ws(YisVal);
static YisVal yis_stdr_trim(YisVal);
static YisVal yis_stdr_starts_with(YisVal, YisVal);
static YisVal yis_stdr_ends_with(YisVal, YisVal);
static YisVal yis_stdr_index_of(YisVal, YisVal);
static YisVal yis_stdr_contains(YisVal, YisVal);
static YisVal yis_stdr_last_index_of(YisVal, YisVal);
static YisVal yis_stdr_shell_quote(YisVal);
static YisVal yis_stdr_basename(YisVal);
static YisVal yis_stdr_dirname(YisVal);
static YisVal yis_stdr_stem(YisVal);
static YisVal yis_stdr_join_path(YisVal, YisVal);
static YisVal yis_stdr_args(void);
static YisVal yis_stdr_open_file_dialog(YisVal, YisVal);
static YisVal yis_stdr_save_file_dialog(YisVal, YisVal, YisVal);
static YisVal yis_stdr_is_none(YisVal v_this);
static YisVal __fnwrap_stdr_writef(void*,int,YisVal*);
static YisVal __fnwrap_stdr_readf(void*,int,YisVal*);
static YisVal __fnwrap_stdr_write(void*,int,YisVal*);
static YisVal __fnwrap_stdr_is_null(void*,int,YisVal*);
static YisVal __fnwrap_stdr_str(void*,int,YisVal*);
static YisVal __fnwrap_stdr_len(void*,int,YisVal*);
static YisVal __fnwrap_stdr_num(void*,int,YisVal*);
static YisVal __fnwrap_stdr_slice(void*,int,YisVal*);
static YisVal __fnwrap_stdr_concat(void*,int,YisVal*);
static YisVal __fnwrap_stdr_join(void*,int,YisVal*);
static YisVal __fnwrap_stdr_push(void*,int,YisVal*);
static YisVal __fnwrap_stdr_str_concat(void*,int,YisVal*);
static YisVal __fnwrap_stdr_char_code(void*,int,YisVal*);
static YisVal __fnwrap_stdr_char_from_code(void*,int,YisVal*);
static YisVal __fnwrap_stdr_char_at(void*,int,YisVal*);
static YisVal __fnwrap_stdr_substring(void*,int,YisVal*);
static YisVal __fnwrap_stdr_substring_len(void*,int,YisVal*);
static YisVal __fnwrap_stdr_replace(void*,int,YisVal*);
static YisVal __fnwrap_stdr_parse_hex(void*,int,YisVal*);
static YisVal __fnwrap_stdr_floor(void*,int,YisVal*);
static YisVal __fnwrap_stdr_ceil(void*,int,YisVal*);
static YisVal __fnwrap_stdr_keys(void*,int,YisVal*);
static YisVal __fnwrap_stdr_read_text_file(void*,int,YisVal*);
static YisVal __fnwrap_stdr_write_text_file(void*,int,YisVal*);
static YisVal __fnwrap_stdr_run_command(void*,int,YisVal*);
static YisVal __fnwrap_stdr_file_exists(void*,int,YisVal*);
static YisVal __fnwrap_stdr_file_mtime(void*,int,YisVal*);
static YisVal __fnwrap_stdr_getcwd(void*,int,YisVal*);
static YisVal __fnwrap_stdr_is_ws(void*,int,YisVal*);
static YisVal __fnwrap_stdr_trim(void*,int,YisVal*);
static YisVal __fnwrap_stdr_starts_with(void*,int,YisVal*);
static YisVal __fnwrap_stdr_ends_with(void*,int,YisVal*);
static YisVal __fnwrap_stdr_index_of(void*,int,YisVal*);
static YisVal __fnwrap_stdr_contains(void*,int,YisVal*);
static YisVal __fnwrap_stdr_last_index_of(void*,int,YisVal*);
static YisVal __fnwrap_stdr_shell_quote(void*,int,YisVal*);
static YisVal __fnwrap_stdr_basename(void*,int,YisVal*);
static YisVal __fnwrap_stdr_dirname(void*,int,YisVal*);
static YisVal __fnwrap_stdr_stem(void*,int,YisVal*);
static YisVal __fnwrap_stdr_join_path(void*,int,YisVal*);
static YisVal __fnwrap_stdr_args(void*,int,YisVal*);
static YisVal __fnwrap_stdr_open_file_dialog(void*,int,YisVal*);
static YisVal __fnwrap_stdr_save_file_dialog(void*,int,YisVal*);

// cask stdr
static YisVal yis_stdr_is_none(YisVal v_this) {
  return yis_eq(v_this, YV_NULLV);
}

static void yis_stdr_writef(YisVal v_fmt, YisVal v_args) {
#line 27 "src/stdlib/stdr.yi"
  (void)(stdr_writef_args(v_fmt, v_args));
}

static YisVal yis_stdr_readf(YisVal v_fmt, YisVal v_args) {
#line 32 "src/stdlib/stdr.yi"
  (void)(stdr_writef_args(v_fmt, v_args));
#line 33 "src/stdlib/stdr.yi"
  YisVal v_line = YV_STR(stdr_read_line()); yis_retain_val(v_line);
#line 34 "src/stdlib/stdr.yi"
  YisVal v_parsed = stdr_readf_parse(v_fmt, v_line, v_args); yis_retain_val(v_parsed);
#line 35 "src/stdlib/stdr.yi"
  return v_line;
  (void)(v_parsed);
}

static void yis_stdr_write(YisVal v_x) {
#line 40 "src/stdlib/stdr.yi"
  (void)(stdr_write(v_x));
}

static YisVal yis_stdr_is_null(YisVal v_x) {
#line 45 "src/stdlib/stdr.yi"
  return yis_stdr_is_none(v_x);
}

static YisVal yis_stdr_str(YisVal v_x) {
  return YV_STR(stdr_to_string(v_x));
}

static YisVal yis_stdr_len(YisVal v_x) {
#line 59 "src/stdlib/stdr.yi"
  return YV_INT(stdr_len(v_x));
}

static YisVal yis_stdr_num(YisVal v_x) {
#line 64 "src/stdlib/stdr.yi"
  return YV_INT(stdr_num(v_x));
}

static YisVal yis_stdr_slice(YisVal v_s, YisVal v_start, YisVal v_end) {
#line 70 "src/stdlib/stdr.yi"
  return stdr_slice(v_s, yis_as_int(v_start), yis_as_int(v_end));
}

static YisVal yis_stdr_concat(YisVal v_a, YisVal v_b) {
#line 76 "src/stdlib/stdr.yi"
  return stdr_array_concat(v_a, v_b);
}

static YisVal yis_stdr_join(YisVal v_arr) {
#line 81 "src/stdlib/stdr.yi"
  YisVal v_len = YV_INT(stdr_len(v_arr)); yis_retain_val(v_len);
#line 82 "src/stdlib/stdr.yi"
  if (yis_as_bool(yis_eq(v_len, YV_INT(0)))) {
    return YV_STR(stdr_str_lit(""));
  }
#line 83 "src/stdlib/stdr.yi"
  if (yis_as_bool(yis_eq(v_len, YV_INT(1)))) {
    return YV_STR(stdr_to_string(yis_index(v_arr, YV_INT(0))));
  }
#line 84 "src/stdlib/stdr.yi"
  YisVal v_a = yis_arr_lit(0); yis_retain_val(v_a);
#line 85 "src/stdlib/stdr.yi"
  YisVal v_i = YV_INT(0); yis_retain_val(v_i);
#line 86 "src/stdlib/stdr.yi"
  for (; yis_as_bool(yis_lt(v_i, v_len)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
    (void)(yis_stdr_push(v_a, YV_STR(stdr_to_string(yis_index(v_arr, v_i)))));
  }
#line 87 "src/stdlib/stdr.yi"
  for (; yis_as_bool(yis_gt(v_len, YV_INT(1)));) {
#line 88 "src/stdlib/stdr.yi"
    YisVal v_j = YV_INT(0); yis_retain_val(v_j);
#line 89 "src/stdlib/stdr.yi"
    YisVal v_k = YV_INT(0); yis_retain_val(v_k);
#line 90 "src/stdlib/stdr.yi"
    for (; yis_as_bool(yis_lt(yis_add(v_j, YV_INT(1)), v_len)); (void)((yis_move_into(&v_j, yis_add(v_j, YV_INT(2))), v_j))) {
#line 91 "src/stdlib/stdr.yi"
      (void)(yis_index_set(v_a, v_k, yis_stdr_str_concat(yis_index(v_a, v_j), yis_index(v_a, yis_add(v_j, YV_INT(1))))));
#line 92 "src/stdlib/stdr.yi"
      (void)((yis_move_into(&v_k, yis_add(v_k, YV_INT(1))), v_k));
    }
#line 93 "src/stdlib/stdr.yi"
    if (yis_as_bool(yis_lt(v_j, v_len))) {
      (void)(yis_index_set(v_a, v_k, yis_index(v_a, v_j)));
      (void)((yis_move_into(&v_k, yis_add(v_k, YV_INT(1))), v_k));
    }
#line 94 "src/stdlib/stdr.yi"
    (void)((yis_move_into(&v_len, v_k), v_len));
  }
#line 95 "src/stdlib/stdr.yi"
  return yis_index(v_a, YV_INT(0));
}

static void yis_stdr_push(YisVal v_arr, YisVal v_val) {
#line 100 "src/stdlib/stdr.yi"
  YisVal v_a_ref = v_arr; yis_retain_val(v_a_ref);
#line 101 "src/stdlib/stdr.yi"
  YisVal v_idx = YV_INT(stdr_len(v_a_ref)); yis_retain_val(v_idx);
#line 102 "src/stdlib/stdr.yi"
  (void)(yis_index_set(v_a_ref, v_idx, v_val));
}

static YisVal yis_stdr_str_concat(YisVal v_a, YisVal v_b) {
#line 108 "src/stdlib/stdr.yi"
  return stdr_str_concat(v_a, v_b);
}

static YisVal yis_stdr_char_code(YisVal v_c) {
#line 114 "src/stdlib/stdr.yi"
  return YV_INT(stdr_char_code(v_c));
}

static YisVal yis_stdr_char_from_code(YisVal v_code) {
#line 120 "src/stdlib/stdr.yi"
  return stdr_char_from_code(v_code);
}

static YisVal yis_stdr_char_at(YisVal v_s, YisVal v_idx) {
#line 125 "src/stdlib/stdr.yi"
  return yis_stdr_slice(v_s, v_idx, yis_add(v_idx, YV_INT(1)));
}

static YisVal yis_stdr_substring(YisVal v_s, YisVal v_start) {
#line 130 "src/stdlib/stdr.yi"
  return yis_stdr_slice(v_s, v_start, YV_INT(stdr_len(v_s)));
}

static YisVal yis_stdr_substring_len(YisVal v_s, YisVal v_start, YisVal v_n) {
#line 135 "src/stdlib/stdr.yi"
  return yis_stdr_slice(v_s, v_start, yis_add(v_start, v_n));
}

static YisVal yis_stdr_replace(YisVal v_text, YisVal v_from, YisVal v_to) {
#line 141 "src/stdlib/stdr.yi"
  return stdr_replace(v_text, v_from, v_to);
}

static YisVal yis_stdr_parse_hex(YisVal v_s) {
#line 147 "src/stdlib/stdr.yi"
  return stdr_parse_hex(v_s);
}

static YisVal yis_stdr_floor(YisVal v_x) {
#line 153 "src/stdlib/stdr.yi"
  return stdr_floor(v_x);
}

static YisVal yis_stdr_ceil(YisVal v_x) {
#line 159 "src/stdlib/stdr.yi"
  return stdr_ceil(v_x);
}

static YisVal yis_stdr_keys(YisVal v_d) {
#line 165 "src/stdlib/stdr.yi"
  return stdr_keys(v_d);
}

static YisVal yis_stdr_read_text_file(YisVal v_path) {
#line 170 "src/stdlib/stdr.yi"
  return stdr_read_text_file(v_path);
}

static YisVal yis_stdr_write_text_file(YisVal v_path, YisVal v_text) {
#line 175 "src/stdlib/stdr.yi"
  return stdr_write_text_file(v_path, v_text);
}

static YisVal yis_stdr_run_command(YisVal v_cmd) {
#line 182 "src/stdlib/stdr.yi"
  return stdr_run_command(v_cmd);
}

static YisVal yis_stdr_file_exists(YisVal v_path) {
#line 189 "src/stdlib/stdr.yi"
  return stdr_file_exists(v_path);
}

static YisVal yis_stdr_file_mtime(YisVal v_path) {
#line 196 "src/stdlib/stdr.yi"
  return stdr_file_mtime(v_path);
}

static YisVal yis_stdr_getcwd(void) {
#line 203 "src/stdlib/stdr.yi"
  return stdr_getcwd();
}

static YisVal yis_stdr_is_ws(YisVal v_ch) {
#line 207 "src/stdlib/stdr.yi"
  YisVal v_c = yis_stdr_char_code(v_ch); yis_retain_val(v_c);
#line 208 "src/stdlib/stdr.yi"
  return YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_c, YV_INT(32))) || yis_as_bool(yis_eq(v_c, YV_INT(9))))) || yis_as_bool(yis_eq(v_c, YV_INT(10))))) || yis_as_bool(yis_eq(v_c, YV_INT(13))));
}

static YisVal yis_stdr_trim(YisVal v_text) {
#line 212 "src/stdlib/stdr.yi"
  YisVal v_n = YV_INT(stdr_len(v_text)); yis_retain_val(v_n);
#line 213 "src/stdlib/stdr.yi"
  YisVal v_start = YV_INT(0); yis_retain_val(v_start);
#line 214 "src/stdlib/stdr.yi"
  for (; yis_as_bool(yis_lt(v_start, v_n)); (void)((yis_move_into(&v_start, yis_add(v_start, YV_INT(1))), v_start))) {
#line 215 "src/stdlib/stdr.yi"
    if (yis_as_bool(YV_BOOL(!yis_as_bool(yis_stdr_is_ws(yis_stdr_slice(v_text, v_start, yis_add(v_start, YV_INT(1)))))))) {
#line 216 "src/stdlib/stdr.yi"
      break;
    }
  }
#line 218 "src/stdlib/stdr.yi"
  YisVal v_end = v_n; yis_retain_val(v_end);
#line 219 "src/stdlib/stdr.yi"
  for (; yis_as_bool(yis_gt(v_end, v_start)); (void)((yis_move_into(&v_end, yis_sub(v_end, YV_INT(1))), v_end))) {
#line 220 "src/stdlib/stdr.yi"
    if (yis_as_bool(YV_BOOL(!yis_as_bool(yis_stdr_is_ws(yis_stdr_slice(v_text, yis_sub(v_end, YV_INT(1)), v_end)))))) {
#line 221 "src/stdlib/stdr.yi"
      break;
    }
  }
#line 223 "src/stdlib/stdr.yi"
  return yis_stdr_slice(v_text, v_start, v_end);
}

static YisVal yis_stdr_starts_with(YisVal v_text, YisVal v_prefix) {
#line 227 "src/stdlib/stdr.yi"
  YisVal v_tn = YV_INT(stdr_len(v_text)); yis_retain_val(v_tn);
#line 228 "src/stdlib/stdr.yi"
  YisVal v_pn = YV_INT(stdr_len(v_prefix)); yis_retain_val(v_pn);
#line 229 "src/stdlib/stdr.yi"
  if (yis_as_bool(yis_eq(v_pn, YV_INT(0)))) {
#line 230 "src/stdlib/stdr.yi"
    return YV_BOOL(true);
  }
#line 231 "src/stdlib/stdr.yi"
  if (yis_as_bool(yis_gt(v_pn, v_tn))) {
#line 232 "src/stdlib/stdr.yi"
    return YV_BOOL(false);
  }
#line 233 "src/stdlib/stdr.yi"
  return yis_eq(yis_stdr_slice(v_text, YV_INT(0), v_pn), v_prefix);
}

static YisVal yis_stdr_ends_with(YisVal v_text, YisVal v_suffix) {
#line 237 "src/stdlib/stdr.yi"
  YisVal v_tn = YV_INT(stdr_len(v_text)); yis_retain_val(v_tn);
#line 238 "src/stdlib/stdr.yi"
  YisVal v_sn = YV_INT(stdr_len(v_suffix)); yis_retain_val(v_sn);
#line 239 "src/stdlib/stdr.yi"
  if (yis_as_bool(yis_eq(v_sn, YV_INT(0)))) {
#line 240 "src/stdlib/stdr.yi"
    return YV_BOOL(true);
  }
#line 241 "src/stdlib/stdr.yi"
  if (yis_as_bool(yis_gt(v_sn, v_tn))) {
#line 242 "src/stdlib/stdr.yi"
    return YV_BOOL(false);
  }
#line 243 "src/stdlib/stdr.yi"
  return yis_eq(yis_stdr_slice(v_text, yis_sub(v_tn, v_sn), v_tn), v_suffix);
}

static YisVal yis_stdr_index_of(YisVal v_text, YisVal v_needle) {
#line 247 "src/stdlib/stdr.yi"
  YisVal v_tn = YV_INT(stdr_len(v_text)); yis_retain_val(v_tn);
#line 248 "src/stdlib/stdr.yi"
  YisVal v_nn = YV_INT(stdr_len(v_needle)); yis_retain_val(v_nn);
#line 249 "src/stdlib/stdr.yi"
  if (yis_as_bool(yis_eq(v_nn, YV_INT(0)))) {
#line 250 "src/stdlib/stdr.yi"
    return YV_INT(0);
  }
#line 251 "src/stdlib/stdr.yi"
  if (yis_as_bool(yis_gt(v_nn, v_tn))) {
#line 252 "src/stdlib/stdr.yi"
    return yis_neg(YV_INT(1));
  }
#line 254 "src/stdlib/stdr.yi"
  YisVal v_i = YV_INT(0); yis_retain_val(v_i);
#line 255 "src/stdlib/stdr.yi"
  YisVal v_end = yis_sub(v_tn, v_nn); yis_retain_val(v_end);
#line 256 "src/stdlib/stdr.yi"
  for (; yis_as_bool(yis_le(v_i, v_end)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 257 "src/stdlib/stdr.yi"
    if (yis_as_bool(yis_eq(yis_stdr_slice(v_text, v_i, yis_add(v_i, v_nn)), v_needle))) {
#line 258 "src/stdlib/stdr.yi"
      return v_i;
    }
  }
#line 259 "src/stdlib/stdr.yi"
  return yis_neg(YV_INT(1));
}

static YisVal yis_stdr_contains(YisVal v_text, YisVal v_needle) {
#line 263 "src/stdlib/stdr.yi"
  return yis_ge(yis_stdr_index_of(v_text, v_needle), YV_INT(0));
}

static YisVal yis_stdr_last_index_of(YisVal v_text, YisVal v_needle) {
#line 267 "src/stdlib/stdr.yi"
  YisVal v_tn = YV_INT(stdr_len(v_text)); yis_retain_val(v_tn);
#line 268 "src/stdlib/stdr.yi"
  YisVal v_nn = YV_INT(stdr_len(v_needle)); yis_retain_val(v_nn);
#line 269 "src/stdlib/stdr.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_nn, YV_INT(0))) || yis_as_bool(yis_gt(v_nn, v_tn))))) {
#line 270 "src/stdlib/stdr.yi"
    return yis_neg(YV_INT(1));
  }
#line 272 "src/stdlib/stdr.yi"
  YisVal v_pos = yis_neg(YV_INT(1)); yis_retain_val(v_pos);
#line 273 "src/stdlib/stdr.yi"
  YisVal v_i = YV_INT(0); yis_retain_val(v_i);
#line 274 "src/stdlib/stdr.yi"
  YisVal v_end = yis_sub(v_tn, v_nn); yis_retain_val(v_end);
#line 275 "src/stdlib/stdr.yi"
  for (; yis_as_bool(yis_le(v_i, v_end)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 276 "src/stdlib/stdr.yi"
    if (yis_as_bool(yis_eq(yis_stdr_slice(v_text, v_i, yis_add(v_i, v_nn)), v_needle))) {
#line 277 "src/stdlib/stdr.yi"
      (void)((yis_move_into(&v_pos, v_i), v_pos));
    }
  }
#line 278 "src/stdlib/stdr.yi"
  return v_pos;
}

static YisVal yis_stdr_shell_quote(YisVal v_text) {
#line 282 "src/stdlib/stdr.yi"
  YisVal v_p = yis_arr_lit(0); yis_retain_val(v_p);
#line 283 "src/stdlib/stdr.yi"
  (void)(yis_stdr_push(v_p, YV_STR(stdr_str_lit("'"))));
#line 284 "src/stdlib/stdr.yi"
  YisVal v_chunk_start = YV_INT(0); yis_retain_val(v_chunk_start);
#line 285 "src/stdlib/stdr.yi"
  YisVal v_n = YV_INT(stdr_len(v_text)); yis_retain_val(v_n);
#line 286 "src/stdlib/stdr.yi"
  YisVal v_i = YV_INT(0); yis_retain_val(v_i);
#line 287 "src/stdlib/stdr.yi"
  for (; yis_as_bool(yis_lt(v_i, v_n)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 288 "src/stdlib/stdr.yi"
    if (yis_as_bool(yis_eq(yis_stdr_slice(v_text, v_i, yis_add(v_i, YV_INT(1))), YV_STR(stdr_str_lit("'"))))) {
#line 289 "src/stdlib/stdr.yi"
      if (yis_as_bool(yis_lt(v_chunk_start, v_i))) {
        (void)(yis_stdr_push(v_p, yis_stdr_slice(v_text, v_chunk_start, v_i)));
      }
#line 290 "src/stdlib/stdr.yi"
      (void)(yis_stdr_push(v_p, YV_STR(stdr_str_lit("'\"'\"'"))));
#line 291 "src/stdlib/stdr.yi"
      (void)((yis_move_into(&v_chunk_start, yis_add(v_i, YV_INT(1))), v_chunk_start));
    }
  }
#line 292 "src/stdlib/stdr.yi"
  if (yis_as_bool(yis_lt(v_chunk_start, v_n))) {
    (void)(yis_stdr_push(v_p, yis_stdr_slice(v_text, v_chunk_start, v_n)));
  }
#line 293 "src/stdlib/stdr.yi"
  (void)(yis_stdr_push(v_p, YV_STR(stdr_str_lit("'"))));
#line 294 "src/stdlib/stdr.yi"
  return yis_stdr_join(v_p);
}

static YisVal yis_stdr_basename(YisVal v_path) {
#line 298 "src/stdlib/stdr.yi"
  YisVal v_p = yis_stdr_trim(v_path); yis_retain_val(v_p);
#line 299 "src/stdlib/stdr.yi"
  YisVal v_idx = yis_stdr_last_index_of(v_p, YV_STR(stdr_str_lit("/"))); yis_retain_val(v_idx);
#line 300 "src/stdlib/stdr.yi"
  if (yis_as_bool(yis_lt(v_idx, YV_INT(0)))) {
#line 301 "src/stdlib/stdr.yi"
    return v_p;
  }
#line 302 "src/stdlib/stdr.yi"
  return yis_stdr_slice(v_p, yis_add(v_idx, YV_INT(1)), YV_INT(stdr_len(v_p)));
}

static YisVal yis_stdr_dirname(YisVal v_path) {
#line 306 "src/stdlib/stdr.yi"
  YisVal v_p = yis_stdr_trim(v_path); yis_retain_val(v_p);
#line 307 "src/stdlib/stdr.yi"
  YisVal v_idx = yis_stdr_last_index_of(v_p, YV_STR(stdr_str_lit("/"))); yis_retain_val(v_idx);
#line 308 "src/stdlib/stdr.yi"
  if (yis_as_bool(yis_le(v_idx, YV_INT(0)))) {
#line 309 "src/stdlib/stdr.yi"
    return YV_STR(stdr_str_lit(""));
  }
#line 310 "src/stdlib/stdr.yi"
  return yis_stdr_slice(v_p, YV_INT(0), v_idx);
}

static YisVal yis_stdr_stem(YisVal v_file_name) {
#line 314 "src/stdlib/stdr.yi"
  YisVal v_idx = yis_stdr_last_index_of(v_file_name, YV_STR(stdr_str_lit("."))); yis_retain_val(v_idx);
#line 315 "src/stdlib/stdr.yi"
  if (yis_as_bool(yis_le(v_idx, YV_INT(0)))) {
#line 316 "src/stdlib/stdr.yi"
    return v_file_name;
  }
#line 317 "src/stdlib/stdr.yi"
  return yis_stdr_slice(v_file_name, YV_INT(0), v_idx);
}

static YisVal yis_stdr_join_path(YisVal v_dir, YisVal v_name) {
#line 321 "src/stdlib/stdr.yi"
  YisVal v_base = yis_stdr_trim(v_dir); yis_retain_val(v_base);
#line 322 "src/stdlib/stdr.yi"
  YisVal v_leaf = yis_stdr_trim(v_name); yis_retain_val(v_leaf);
#line 324 "src/stdlib/stdr.yi"
  if (yis_as_bool(yis_eq(YV_INT(stdr_len(v_base)), YV_INT(0)))) {
#line 325 "src/stdlib/stdr.yi"
    return v_leaf;
  }
#line 326 "src/stdlib/stdr.yi"
  if (yis_as_bool(yis_eq(YV_INT(stdr_len(v_leaf)), YV_INT(0)))) {
#line 327 "src/stdlib/stdr.yi"
    return v_base;
  }
#line 329 "src/stdlib/stdr.yi"
  YisVal v_out = v_base; yis_retain_val(v_out);
#line 330 "src/stdlib/stdr.yi"
  if (yis_as_bool(YV_BOOL(!yis_as_bool(yis_stdr_ends_with(v_out, YV_STR(stdr_str_lit("/"))))))) {
#line 331 "src/stdlib/stdr.yi"
    (void)((yis_move_into(&v_out, yis_stdr_str_concat(v_out, YV_STR(stdr_str_lit("/")))), v_out));
  }
#line 332 "src/stdlib/stdr.yi"
  if (yis_as_bool(yis_stdr_starts_with(v_leaf, YV_STR(stdr_str_lit("/"))))) {
#line 333 "src/stdlib/stdr.yi"
    (void)((yis_move_into(&v_leaf, yis_stdr_slice(v_leaf, YV_INT(1), YV_INT(stdr_len(v_leaf)))), v_leaf));
  }
#line 334 "src/stdlib/stdr.yi"
  return yis_stdr_str_concat(v_out, v_leaf);
}

static YisVal yis_stdr_args(void) {
#line 340 "src/stdlib/stdr.yi"
  return stdr_args();
}

static YisVal yis_stdr_open_file_dialog(YisVal v_prompt, YisVal v_extension) {
#line 345 "src/stdlib/stdr.yi"
  return stdr_open_file_dialog(v_prompt, v_extension);
}

static YisVal yis_stdr_save_file_dialog(YisVal v_prompt, YisVal v_default_name, YisVal v_extension) {
#line 350 "src/stdlib/stdr.yi"
  return stdr_save_file_dialog(v_prompt, v_default_name, v_extension);
}

static YisVal __fnwrap_stdr_writef(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  yis_stdr_writef(__a0, __a1);
  return YV_NULLV;
}

static YisVal __fnwrap_stdr_readf(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  return yis_stdr_readf(__a0, __a1);
}

static YisVal __fnwrap_stdr_write(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_stdr_write(__a0);
  return YV_NULLV;
}

static YisVal __fnwrap_stdr_is_null(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_is_null(__a0);
}

static YisVal __fnwrap_stdr_str(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_str(__a0);
}

static YisVal __fnwrap_stdr_len(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_len(__a0);
}

static YisVal __fnwrap_stdr_num(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_num(__a0);
}

static YisVal __fnwrap_stdr_slice(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  YisVal __a2 = argc > 2 ? argv[2] : YV_NULLV;
  return yis_stdr_slice(__a0, __a1, __a2);
}

static YisVal __fnwrap_stdr_concat(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  return yis_stdr_concat(__a0, __a1);
}

static YisVal __fnwrap_stdr_join(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_join(__a0);
}

static YisVal __fnwrap_stdr_push(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  yis_stdr_push(__a0, __a1);
  return YV_NULLV;
}

static YisVal __fnwrap_stdr_str_concat(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  return yis_stdr_str_concat(__a0, __a1);
}

static YisVal __fnwrap_stdr_char_code(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_char_code(__a0);
}

static YisVal __fnwrap_stdr_char_from_code(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_char_from_code(__a0);
}

static YisVal __fnwrap_stdr_char_at(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  return yis_stdr_char_at(__a0, __a1);
}

static YisVal __fnwrap_stdr_substring(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  return yis_stdr_substring(__a0, __a1);
}

static YisVal __fnwrap_stdr_substring_len(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  YisVal __a2 = argc > 2 ? argv[2] : YV_NULLV;
  return yis_stdr_substring_len(__a0, __a1, __a2);
}

static YisVal __fnwrap_stdr_replace(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  YisVal __a2 = argc > 2 ? argv[2] : YV_NULLV;
  return yis_stdr_replace(__a0, __a1, __a2);
}

static YisVal __fnwrap_stdr_parse_hex(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_parse_hex(__a0);
}

static YisVal __fnwrap_stdr_floor(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_floor(__a0);
}

static YisVal __fnwrap_stdr_ceil(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_ceil(__a0);
}

static YisVal __fnwrap_stdr_keys(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_keys(__a0);
}

static YisVal __fnwrap_stdr_read_text_file(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_read_text_file(__a0);
}

static YisVal __fnwrap_stdr_write_text_file(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  return yis_stdr_write_text_file(__a0, __a1);
}

static YisVal __fnwrap_stdr_run_command(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_run_command(__a0);
}

static YisVal __fnwrap_stdr_file_exists(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_file_exists(__a0);
}

static YisVal __fnwrap_stdr_file_mtime(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_file_mtime(__a0);
}

static YisVal __fnwrap_stdr_getcwd(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  return yis_stdr_getcwd();
}

static YisVal __fnwrap_stdr_is_ws(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_is_ws(__a0);
}

static YisVal __fnwrap_stdr_trim(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_trim(__a0);
}

static YisVal __fnwrap_stdr_starts_with(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  return yis_stdr_starts_with(__a0, __a1);
}

static YisVal __fnwrap_stdr_ends_with(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  return yis_stdr_ends_with(__a0, __a1);
}

static YisVal __fnwrap_stdr_index_of(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  return yis_stdr_index_of(__a0, __a1);
}

static YisVal __fnwrap_stdr_contains(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  return yis_stdr_contains(__a0, __a1);
}

static YisVal __fnwrap_stdr_last_index_of(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  return yis_stdr_last_index_of(__a0, __a1);
}

static YisVal __fnwrap_stdr_shell_quote(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_shell_quote(__a0);
}

static YisVal __fnwrap_stdr_basename(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_basename(__a0);
}

static YisVal __fnwrap_stdr_dirname(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_dirname(__a0);
}

static YisVal __fnwrap_stdr_stem(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_stdr_stem(__a0);
}

static YisVal __fnwrap_stdr_join_path(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  return yis_stdr_join_path(__a0, __a1);
}

static YisVal __fnwrap_stdr_args(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  return yis_stdr_args();
}

static YisVal __fnwrap_stdr_open_file_dialog(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  return yis_stdr_open_file_dialog(__a0, __a1);
}

static YisVal __fnwrap_stdr_save_file_dialog(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  YisVal __a2 = argc > 2 ? argv[2] : YV_NULLV;
  return yis_stdr_save_file_dialog(__a0, __a1, __a2);
}

/* end embedded module: stdr */

/* begin embedded module: json */
static YisVal yis_json_escape(YisVal);
static YisVal yis_json__state_new(YisVal);
static void yis_json__fail(YisVal, YisVal);
static YisVal yis_json__peek(YisVal);
static YisVal yis_json__next(YisVal);
static YisVal yis_json__is_ws(YisVal);
static void yis_json__skip_ws(YisVal);
static YisVal yis_json__is_digit(YisVal);
static YisVal yis_json__is_hex(YisVal);
static YisVal yis_json__expect(YisVal, YisVal);
static YisVal yis_json__read_lit(YisVal, YisVal);
static YisVal yis_json__pow10(YisVal);
static YisVal yis_json__parse_num_text(YisVal);
static YisVal yis_json__parse_string(YisVal);
static YisVal yis_json__parse_number(YisVal);
static YisVal yis_json__parse_value(YisVal);
static YisVal yis_json__parse_array(YisVal);
static YisVal yis_json__parse_object(YisVal);
static YisVal yis_json_parse_result(YisVal);
static YisVal yis_json_parse(YisVal);
static YisVal yis_json_is_object(YisVal);
static YisVal yis_json_is_array(YisVal);
static YisVal yis_json_get(YisVal, YisVal, YisVal);
static YisVal __fnwrap_json_escape(void*,int,YisVal*);
static YisVal __fnwrap_json__state_new(void*,int,YisVal*);
static YisVal __fnwrap_json__fail(void*,int,YisVal*);
static YisVal __fnwrap_json__peek(void*,int,YisVal*);
static YisVal __fnwrap_json__next(void*,int,YisVal*);
static YisVal __fnwrap_json__is_ws(void*,int,YisVal*);
static YisVal __fnwrap_json__skip_ws(void*,int,YisVal*);
static YisVal __fnwrap_json__is_digit(void*,int,YisVal*);
static YisVal __fnwrap_json__is_hex(void*,int,YisVal*);
static YisVal __fnwrap_json__expect(void*,int,YisVal*);
static YisVal __fnwrap_json__read_lit(void*,int,YisVal*);
static YisVal __fnwrap_json__pow10(void*,int,YisVal*);
static YisVal __fnwrap_json__parse_num_text(void*,int,YisVal*);
static YisVal __fnwrap_json__parse_string(void*,int,YisVal*);
static YisVal __fnwrap_json__parse_number(void*,int,YisVal*);
static YisVal __fnwrap_json__parse_value(void*,int,YisVal*);
static YisVal __fnwrap_json__parse_array(void*,int,YisVal*);
static YisVal __fnwrap_json__parse_object(void*,int,YisVal*);
static YisVal __fnwrap_json_parse_result(void*,int,YisVal*);
static YisVal __fnwrap_json_parse(void*,int,YisVal*);
static YisVal __fnwrap_json_is_object(void*,int,YisVal*);
static YisVal __fnwrap_json_is_array(void*,int,YisVal*);
static YisVal __fnwrap_json_get(void*,int,YisVal*);

// cask json
// bring stdr
static YisVal yis_json_escape(YisVal v_text) {
#line 6 "src/stdlib/json.yi"
  YisVal v_parts = yis_arr_lit(0); yis_retain_val(v_parts);
#line 7 "src/stdlib/json.yi"
  YisVal v_chunk_start = YV_INT(0); yis_retain_val(v_chunk_start);
#line 8 "src/stdlib/json.yi"
  YisVal v_n = YV_INT(stdr_len(v_text)); yis_retain_val(v_n);
#line 9 "src/stdlib/json.yi"
  YisVal v_i = YV_INT(0); yis_retain_val(v_i);
#line 10 "src/stdlib/json.yi"
  for (; yis_as_bool(yis_lt(v_i, v_n)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 11 "src/stdlib/json.yi"
    YisVal v_ch = stdr_slice(v_text, yis_as_int(v_i), yis_as_int(yis_add(v_i, YV_INT(1)))); yis_retain_val(v_ch);
#line 12 "src/stdlib/json.yi"
    YisVal v_esc = YV_STR(stdr_str_lit("")); yis_retain_val(v_esc);
#line 13 "src/stdlib/json.yi"
    if (yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("\\"))))) {
      (void)((yis_move_into(&v_esc, YV_STR(stdr_str_lit("\\\\"))), v_esc));
    } else if (yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("\""))))) {
#line 14 "src/stdlib/json.yi"
      (void)((yis_move_into(&v_esc, YV_STR(stdr_str_lit("\\\""))), v_esc));
    } else if (yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("\n"))))) {
#line 15 "src/stdlib/json.yi"
      (void)((yis_move_into(&v_esc, YV_STR(stdr_str_lit("\\n"))), v_esc));
    } else if (yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("\r"))))) {
#line 16 "src/stdlib/json.yi"
      (void)((yis_move_into(&v_esc, YV_STR(stdr_str_lit("\\r"))), v_esc));
    } else if (yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("\t"))))) {
#line 17 "src/stdlib/json.yi"
      (void)((yis_move_into(&v_esc, YV_STR(stdr_str_lit("\\t"))), v_esc));
    }




#line 18 "src/stdlib/json.yi"
    if (yis_as_bool(yis_gt(YV_INT(stdr_len(v_esc)), YV_INT(0)))) {
#line 19 "src/stdlib/json.yi"
      if (yis_as_bool(yis_lt(v_chunk_start, v_i))) {
        (void)(stdr_push(v_parts, stdr_slice(v_text, yis_as_int(v_chunk_start), yis_as_int(v_i))));
      }
#line 20 "src/stdlib/json.yi"
      (void)(stdr_push(v_parts, v_esc));
#line 21 "src/stdlib/json.yi"
      (void)((yis_move_into(&v_chunk_start, yis_add(v_i, YV_INT(1))), v_chunk_start));
    }
  }
#line 22 "src/stdlib/json.yi"
  if (yis_as_bool(yis_lt(v_chunk_start, v_n))) {
    (void)(stdr_push(v_parts, stdr_slice(v_text, yis_as_int(v_chunk_start), yis_as_int(v_n))));
  }
#line 23 "src/stdlib/json.yi"
  return yis_stdr_join(v_parts);
}

static YisVal yis_json__state_new(YisVal v_src) {
#line 30 "src/stdlib/json.yi"
  YisVal v_s = yis_dict_lit(0); yis_retain_val(v_s);
#line 31 "src/stdlib/json.yi"
  (void)(yis_index_set(v_s, YV_STR(stdr_str_lit("src")), v_src));
#line 32 "src/stdlib/json.yi"
  (void)(yis_index_set(v_s, YV_STR(stdr_str_lit("i")), YV_INT(0)));
#line 33 "src/stdlib/json.yi"
  (void)(yis_index_set(v_s, YV_STR(stdr_str_lit("n")), YV_INT(stdr_len(v_src))));
#line 34 "src/stdlib/json.yi"
  (void)(yis_index_set(v_s, YV_STR(stdr_str_lit("ok")), YV_BOOL(true)));
#line 35 "src/stdlib/json.yi"
  (void)(yis_index_set(v_s, YV_STR(stdr_str_lit("error")), YV_STR(stdr_str_lit(""))));
#line 36 "src/stdlib/json.yi"
  return v_s;
}

static void yis_json__fail(YisVal v_s, YisVal v_msg) {
#line 40 "src/stdlib/json.yi"
  YisVal v_st = v_s; yis_retain_val(v_st);
#line 41 "src/stdlib/json.yi"
  if (yis_as_bool(((yis_index(v_st, YV_STR(stdr_str_lit("ok")))).tag == EVT_NULL ? (YV_BOOL(false)) : (yis_index(v_st, YV_STR(stdr_str_lit("ok"))))))) {
#line 42 "src/stdlib/json.yi"
    (void)(yis_index_set(v_st, YV_STR(stdr_str_lit("ok")), YV_BOOL(false)));
#line 43 "src/stdlib/json.yi"
    (void)(yis_index_set(v_st, YV_STR(stdr_str_lit("error")), v_msg));
  }
}

static YisVal yis_json__peek(YisVal v_s) {
#line 47 "src/stdlib/json.yi"
  YisVal v_i = YV_INT(stdr_num(((yis_index(v_s, YV_STR(stdr_str_lit("i")))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_s, YV_STR(stdr_str_lit("i"))))))); yis_retain_val(v_i);
#line 48 "src/stdlib/json.yi"
  YisVal v_n = YV_INT(stdr_num(((yis_index(v_s, YV_STR(stdr_str_lit("n")))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_s, YV_STR(stdr_str_lit("n"))))))); yis_retain_val(v_n);
#line 49 "src/stdlib/json.yi"
  if (yis_as_bool(yis_ge(v_i, v_n))) {
    return YV_STR(stdr_str_lit(""));
  }
#line 50 "src/stdlib/json.yi"
  YisVal v_src = ((yis_index(v_s, YV_STR(stdr_str_lit("src")))).tag == EVT_NULL ? (YV_STR(stdr_str_lit(""))) : (yis_index(v_s, YV_STR(stdr_str_lit("src"))))); yis_retain_val(v_src);
#line 51 "src/stdlib/json.yi"
  return stdr_slice(v_src, yis_as_int(v_i), yis_as_int(yis_add(v_i, YV_INT(1))));
}

static YisVal yis_json__next(YisVal v_s) {
#line 55 "src/stdlib/json.yi"
  YisVal v_st = v_s; yis_retain_val(v_st);
#line 56 "src/stdlib/json.yi"
  YisVal v_ch = yis_json__peek(v_s); yis_retain_val(v_ch);
#line 57 "src/stdlib/json.yi"
  YisVal v_i = YV_INT(stdr_num(((yis_index(v_st, YV_STR(stdr_str_lit("i")))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_st, YV_STR(stdr_str_lit("i"))))))); yis_retain_val(v_i);
#line 58 "src/stdlib/json.yi"
  if (yis_as_bool(yis_lt(v_i, YV_INT(stdr_num(((yis_index(v_st, YV_STR(stdr_str_lit("n")))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_st, YV_STR(stdr_str_lit("n")))))))))) {
#line 59 "src/stdlib/json.yi"
    (void)(yis_index_set(v_st, YV_STR(stdr_str_lit("i")), yis_add(v_i, YV_INT(1))));
  }
#line 60 "src/stdlib/json.yi"
  return v_ch;
}

static YisVal yis_json__is_ws(YisVal v_ch) {
#line 64 "src/stdlib/json.yi"
  return YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit(" ")))) || yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("\t")))))) || yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("\n")))))) || yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("\r")))));
}

static void yis_json__skip_ws(YisVal v_s) {
#line 68 "src/stdlib/json.yi"
  for (; yis_as_bool(YV_BOOL(true));) {
#line 69 "src/stdlib/json.yi"
    YisVal v_ch = yis_json__peek(v_s); yis_retain_val(v_ch);
#line 70 "src/stdlib/json.yi"
    if (yis_as_bool(YV_BOOL(!yis_as_bool(yis_json__is_ws(v_ch))))) {
      break;
    }
#line 71 "src/stdlib/json.yi"
    (void)(yis_json__next(v_s));
  }
}

static YisVal yis_json__is_digit(YisVal v_ch) {
#line 75 "src/stdlib/json.yi"
  YisVal v_c = YV_INT(stdr_char_code(v_ch)); yis_retain_val(v_c);
#line 76 "src/stdlib/json.yi"
  return YV_BOOL(yis_as_bool(yis_ge(v_c, YV_INT(48))) && yis_as_bool(yis_le(v_c, YV_INT(57))));
}

static YisVal yis_json__is_hex(YisVal v_ch) {
#line 80 "src/stdlib/json.yi"
  YisVal v_c = YV_INT(stdr_char_code(v_ch)); yis_retain_val(v_c);
#line 81 "src/stdlib/json.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_ge(v_c, YV_INT(48))) && yis_as_bool(yis_le(v_c, YV_INT(57)))))) {
    return YV_BOOL(true);
  }
#line 82 "src/stdlib/json.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_ge(v_c, YV_INT(65))) && yis_as_bool(yis_le(v_c, YV_INT(70)))))) {
    return YV_BOOL(true);
  }
#line 83 "src/stdlib/json.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_ge(v_c, YV_INT(97))) && yis_as_bool(yis_le(v_c, YV_INT(102)))))) {
    return YV_BOOL(true);
  }
#line 84 "src/stdlib/json.yi"
  return YV_BOOL(false);
}

static YisVal yis_json__expect(YisVal v_s, YisVal v_want) {
#line 88 "src/stdlib/json.yi"
  YisVal v_ch = yis_json__next(v_s); yis_retain_val(v_ch);
#line 89 "src/stdlib/json.yi"
  if (yis_as_bool(yis_ne(v_ch, v_want))) {
#line 90 "src/stdlib/json.yi"
    (void)(yis_json__fail(v_s, YV_STR(stdr_str_lit("unexpected token"))));
#line 91 "src/stdlib/json.yi"
    return YV_BOOL(false);
  }
#line 92 "src/stdlib/json.yi"
  return YV_BOOL(true);
}

static YisVal yis_json__read_lit(YisVal v_s, YisVal v_lit) {
#line 96 "src/stdlib/json.yi"
  YisVal v_i = YV_INT(0); yis_retain_val(v_i);
#line 97 "src/stdlib/json.yi"
  YisVal v_n = YV_INT(stdr_len(v_lit)); yis_retain_val(v_n);
#line 98 "src/stdlib/json.yi"
  for (; yis_as_bool(yis_lt(v_i, v_n)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 99 "src/stdlib/json.yi"
    if (yis_as_bool(yis_ne(yis_json__next(v_s), stdr_slice(v_lit, yis_as_int(v_i), yis_as_int(yis_add(v_i, YV_INT(1))))))) {
#line 100 "src/stdlib/json.yi"
      (void)(yis_json__fail(v_s, YV_STR(stdr_str_lit("invalid literal"))));
#line 101 "src/stdlib/json.yi"
      return YV_BOOL(false);
    }
  }
#line 102 "src/stdlib/json.yi"
  return YV_BOOL(true);
}

static YisVal yis_json__pow10(YisVal v_exp) {
#line 106 "src/stdlib/json.yi"
  YisVal v_p = YV_FLOAT(1.0); yis_retain_val(v_p);
#line 107 "src/stdlib/json.yi"
  YisVal v_i = YV_INT(0); yis_retain_val(v_i);
#line 108 "src/stdlib/json.yi"
  for (; yis_as_bool(yis_lt(v_i, v_exp)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 109 "src/stdlib/json.yi"
    (void)((yis_move_into(&v_p, yis_mul(v_p, YV_FLOAT(10.0))), v_p));
  }
#line 110 "src/stdlib/json.yi"
  return v_p;
}

static YisVal yis_json__parse_num_text(YisVal v_text) {
#line 114 "src/stdlib/json.yi"
  YisVal v_i = YV_INT(0); yis_retain_val(v_i);
#line 115 "src/stdlib/json.yi"
  YisVal v_n = YV_INT(stdr_len(v_text)); yis_retain_val(v_n);
#line 116 "src/stdlib/json.yi"
  YisVal v_sign = YV_FLOAT(1.0); yis_retain_val(v_sign);
#line 118 "src/stdlib/json.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_lt(v_i, v_n)) && yis_as_bool(yis_eq(stdr_slice(v_text, yis_as_int(v_i), yis_as_int(yis_add(v_i, YV_INT(1)))), YV_STR(stdr_str_lit("-"))))))) {
#line 119 "src/stdlib/json.yi"
    (void)((yis_move_into(&v_sign, yis_neg(YV_FLOAT(1.0))), v_sign));
#line 120 "src/stdlib/json.yi"
    (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i));
  }
#line 122 "src/stdlib/json.yi"
  YisVal v_int_part = YV_FLOAT(0.0); yis_retain_val(v_int_part);
#line 123 "src/stdlib/json.yi"
  for (; yis_as_bool(yis_lt(v_i, v_n)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 124 "src/stdlib/json.yi"
    YisVal v_ch = stdr_slice(v_text, yis_as_int(v_i), yis_as_int(yis_add(v_i, YV_INT(1)))); yis_retain_val(v_ch);
#line 125 "src/stdlib/json.yi"
    if (yis_as_bool(YV_BOOL(!yis_as_bool(yis_json__is_digit(v_ch))))) {
      break;
    }
#line 126 "src/stdlib/json.yi"
    (void)((yis_move_into(&v_int_part, yis_add(yis_mul(v_int_part, YV_FLOAT(10.0)), yis_sub(YV_INT(stdr_char_code(v_ch)), YV_INT(48)))), v_int_part));
  }
#line 128 "src/stdlib/json.yi"
  YisVal v_frac_part = YV_FLOAT(0.0); yis_retain_val(v_frac_part);
#line 129 "src/stdlib/json.yi"
  YisVal v_frac_len = YV_INT(0); yis_retain_val(v_frac_len);
#line 130 "src/stdlib/json.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_lt(v_i, v_n)) && yis_as_bool(yis_eq(stdr_slice(v_text, yis_as_int(v_i), yis_as_int(yis_add(v_i, YV_INT(1)))), YV_STR(stdr_str_lit("."))))))) {
#line 131 "src/stdlib/json.yi"
    (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i));
#line 132 "src/stdlib/json.yi"
    for (; yis_as_bool(yis_lt(v_i, v_n)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 133 "src/stdlib/json.yi"
      YisVal v_ch = stdr_slice(v_text, yis_as_int(v_i), yis_as_int(yis_add(v_i, YV_INT(1)))); yis_retain_val(v_ch);
#line 134 "src/stdlib/json.yi"
      if (yis_as_bool(YV_BOOL(!yis_as_bool(yis_json__is_digit(v_ch))))) {
        break;
      }
#line 135 "src/stdlib/json.yi"
      (void)((yis_move_into(&v_frac_part, yis_add(yis_mul(v_frac_part, YV_FLOAT(10.0)), yis_sub(YV_INT(stdr_char_code(v_ch)), YV_INT(48)))), v_frac_part));
#line 136 "src/stdlib/json.yi"
      (void)((yis_move_into(&v_frac_len, yis_add(v_frac_len, YV_INT(1))), v_frac_len));
    }
  }
#line 138 "src/stdlib/json.yi"
  YisVal v_exp_sign = YV_FLOAT(1.0); yis_retain_val(v_exp_sign);
#line 139 "src/stdlib/json.yi"
  YisVal v_exp_val = YV_INT(0); yis_retain_val(v_exp_val);
#line 140 "src/stdlib/json.yi"
  if (yis_as_bool(yis_lt(v_i, v_n))) {
#line 141 "src/stdlib/json.yi"
    YisVal v_ech = stdr_slice(v_text, yis_as_int(v_i), yis_as_int(yis_add(v_i, YV_INT(1)))); yis_retain_val(v_ech);
#line 142 "src/stdlib/json.yi"
    if (yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_ech, YV_STR(stdr_str_lit("e")))) || yis_as_bool(yis_eq(v_ech, YV_STR(stdr_str_lit("E"))))))) {
#line 143 "src/stdlib/json.yi"
      (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i));
#line 144 "src/stdlib/json.yi"
      if (yis_as_bool(yis_lt(v_i, v_n))) {
#line 145 "src/stdlib/json.yi"
        YisVal v_sch = stdr_slice(v_text, yis_as_int(v_i), yis_as_int(yis_add(v_i, YV_INT(1)))); yis_retain_val(v_sch);
#line 146 "src/stdlib/json.yi"
        if (yis_as_bool(yis_eq(v_sch, YV_STR(stdr_str_lit("-"))))) {
#line 147 "src/stdlib/json.yi"
          (void)((yis_move_into(&v_exp_sign, yis_neg(YV_FLOAT(1.0))), v_exp_sign));
#line 148 "src/stdlib/json.yi"
          (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i));
        } else if (yis_as_bool(yis_eq(v_sch, YV_STR(stdr_str_lit("+"))))) {
#line 150 "src/stdlib/json.yi"
          (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i));
        }

      }
#line 151 "src/stdlib/json.yi"
      for (; yis_as_bool(yis_lt(v_i, v_n)); (void)((yis_move_into(&v_i, yis_add(v_i, YV_INT(1))), v_i))) {
#line 152 "src/stdlib/json.yi"
        YisVal v_ch = stdr_slice(v_text, yis_as_int(v_i), yis_as_int(yis_add(v_i, YV_INT(1)))); yis_retain_val(v_ch);
#line 153 "src/stdlib/json.yi"
        if (yis_as_bool(YV_BOOL(!yis_as_bool(yis_json__is_digit(v_ch))))) {
          break;
        }
#line 154 "src/stdlib/json.yi"
        (void)((yis_move_into(&v_exp_val, yis_add(yis_mul(v_exp_val, YV_INT(10)), yis_sub(YV_INT(stdr_char_code(v_ch)), YV_INT(48)))), v_exp_val));
      }
    }
  }
#line 156 "src/stdlib/json.yi"
  YisVal v_v = v_int_part; yis_retain_val(v_v);
#line 157 "src/stdlib/json.yi"
  if (yis_as_bool(yis_gt(v_frac_len, YV_INT(0)))) {
#line 158 "src/stdlib/json.yi"
    (void)((yis_move_into(&v_v, yis_add(v_v, yis_div(v_frac_part, yis_json__pow10(v_frac_len)))), v_v));
  }
#line 160 "src/stdlib/json.yi"
  if (yis_as_bool(yis_gt(v_exp_val, YV_INT(0)))) {
#line 161 "src/stdlib/json.yi"
    YisVal v_p = yis_json__pow10(v_exp_val); yis_retain_val(v_p);
#line 162 "src/stdlib/json.yi"
    if (yis_as_bool(yis_gt(v_exp_sign, YV_FLOAT(0.0)))) {
#line 163 "src/stdlib/json.yi"
      (void)((yis_move_into(&v_v, yis_mul(v_v, v_p)), v_v));
    } else {
#line 165 "src/stdlib/json.yi"
      (void)((yis_move_into(&v_v, yis_div(v_v, v_p)), v_v));
    }
  }
#line 167 "src/stdlib/json.yi"
  return yis_mul(v_v, v_sign);
}

static YisVal yis_json__parse_string(YisVal v_s) {
#line 171 "src/stdlib/json.yi"
  if (yis_as_bool(YV_BOOL(!yis_as_bool(yis_json__expect(v_s, YV_STR(stdr_str_lit("\""))))))) {
    return YV_NULLV;
  }
#line 172 "src/stdlib/json.yi"
  YisVal v_out = YV_STR(stdr_str_lit("")); yis_retain_val(v_out);
#line 174 "src/stdlib/json.yi"
  for (; yis_as_bool(YV_BOOL(true));) {
#line 175 "src/stdlib/json.yi"
    YisVal v_ch = yis_json__next(v_s); yis_retain_val(v_ch);
#line 176 "src/stdlib/json.yi"
    if (yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit(""))))) {
#line 177 "src/stdlib/json.yi"
      (void)(yis_json__fail(v_s, YV_STR(stdr_str_lit("unterminated string"))));
#line 178 "src/stdlib/json.yi"
      return YV_NULLV;
    }
#line 179 "src/stdlib/json.yi"
    if (yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("\""))))) {
#line 180 "src/stdlib/json.yi"
      return v_out;
    }
#line 181 "src/stdlib/json.yi"
    if (yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("\\"))))) {
#line 182 "src/stdlib/json.yi"
      YisVal v_esc = yis_json__next(v_s); yis_retain_val(v_esc);
#line 183 "src/stdlib/json.yi"
      if (yis_as_bool(yis_eq(v_esc, YV_STR(stdr_str_lit(""))))) {
#line 184 "src/stdlib/json.yi"
        (void)(yis_json__fail(v_s, YV_STR(stdr_str_lit("unterminated escape"))));
#line 185 "src/stdlib/json.yi"
        return YV_NULLV;
      }
#line 186 "src/stdlib/json.yi"
      if (yis_as_bool(YV_BOOL(yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_esc, YV_STR(stdr_str_lit("\"")))) || yis_as_bool(yis_eq(v_esc, YV_STR(stdr_str_lit("\\")))))) || yis_as_bool(yis_eq(v_esc, YV_STR(stdr_str_lit("/"))))))) {
#line 187 "src/stdlib/json.yi"
        (void)((yis_move_into(&v_out, yis_stdr_str_concat(v_out, v_esc)), v_out));
      } else if (yis_as_bool(yis_eq(v_esc, YV_STR(stdr_str_lit("b"))))) {
#line 189 "src/stdlib/json.yi"
        (void)((yis_move_into(&v_out, yis_stdr_str_concat(v_out, YV_STR(stdr_str_lit("\b")))), v_out));
      } else if (yis_as_bool(yis_eq(v_esc, YV_STR(stdr_str_lit("f"))))) {
#line 191 "src/stdlib/json.yi"
        (void)((yis_move_into(&v_out, yis_stdr_str_concat(v_out, YV_STR(stdr_str_lit("\f")))), v_out));
      } else if (yis_as_bool(yis_eq(v_esc, YV_STR(stdr_str_lit("n"))))) {
#line 193 "src/stdlib/json.yi"
        (void)((yis_move_into(&v_out, yis_stdr_str_concat(v_out, YV_STR(stdr_str_lit("\n")))), v_out));
      } else if (yis_as_bool(yis_eq(v_esc, YV_STR(stdr_str_lit("r"))))) {
#line 195 "src/stdlib/json.yi"
        (void)((yis_move_into(&v_out, yis_stdr_str_concat(v_out, YV_STR(stdr_str_lit("\r")))), v_out));
      } else if (yis_as_bool(yis_eq(v_esc, YV_STR(stdr_str_lit("t"))))) {
#line 197 "src/stdlib/json.yi"
        (void)((yis_move_into(&v_out, yis_stdr_str_concat(v_out, YV_STR(stdr_str_lit("\t")))), v_out));
      } else if (yis_as_bool(yis_eq(v_esc, YV_STR(stdr_str_lit("u"))))) {
#line 199 "src/stdlib/json.yi"
        YisVal v_k = YV_INT(0); yis_retain_val(v_k);
#line 200 "src/stdlib/json.yi"
        for (; yis_as_bool(yis_lt(v_k, YV_INT(4))); (void)((yis_move_into(&v_k, yis_add(v_k, YV_INT(1))), v_k))) {
#line 201 "src/stdlib/json.yi"
          YisVal v_h = yis_json__next(v_s); yis_retain_val(v_h);
#line 202 "src/stdlib/json.yi"
          if (yis_as_bool(YV_BOOL(!yis_as_bool(yis_json__is_hex(v_h))))) {
#line 203 "src/stdlib/json.yi"
            (void)(yis_json__fail(v_s, YV_STR(stdr_str_lit("invalid unicode escape"))));
#line 204 "src/stdlib/json.yi"
            return YV_NULLV;
          }
        }
#line 205 "src/stdlib/json.yi"
        (void)((yis_move_into(&v_out, yis_stdr_str_concat(v_out, YV_STR(stdr_str_lit("?")))), v_out));
      } else {
#line 207 "src/stdlib/json.yi"
        (void)(yis_json__fail(v_s, YV_STR(stdr_str_lit("invalid escape"))));
#line 208 "src/stdlib/json.yi"
        return YV_NULLV;
      }






#line 209 "src/stdlib/json.yi"
      continue;
    }
#line 210 "src/stdlib/json.yi"
    (void)((yis_move_into(&v_out, yis_stdr_str_concat(v_out, v_ch)), v_out));
  }
#line 211 "src/stdlib/json.yi"
  return YV_NULLV;
}

static YisVal yis_json__parse_number(YisVal v_s) {
#line 215 "src/stdlib/json.yi"
  YisVal v_src = ((yis_index(v_s, YV_STR(stdr_str_lit("src")))).tag == EVT_NULL ? (YV_STR(stdr_str_lit(""))) : (yis_index(v_s, YV_STR(stdr_str_lit("src"))))); yis_retain_val(v_src);
#line 216 "src/stdlib/json.yi"
  YisVal v_start = YV_INT(stdr_num(((yis_index(v_s, YV_STR(stdr_str_lit("i")))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_s, YV_STR(stdr_str_lit("i"))))))); yis_retain_val(v_start);
#line 217 "src/stdlib/json.yi"
  YisVal v_n = YV_INT(stdr_num(((yis_index(v_s, YV_STR(stdr_str_lit("n")))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_s, YV_STR(stdr_str_lit("n"))))))); yis_retain_val(v_n);
#line 219 "src/stdlib/json.yi"
  if (yis_as_bool(yis_eq(yis_json__peek(v_s), YV_STR(stdr_str_lit("-"))))) {
#line 220 "src/stdlib/json.yi"
    (void)(yis_json__next(v_s));
  }
#line 222 "src/stdlib/json.yi"
  YisVal v_digits = YV_INT(0); yis_retain_val(v_digits);
#line 223 "src/stdlib/json.yi"
  if (yis_as_bool(yis_eq(yis_json__peek(v_s), YV_STR(stdr_str_lit("0"))))) {
#line 224 "src/stdlib/json.yi"
    (void)(yis_json__next(v_s));
#line 225 "src/stdlib/json.yi"
    (void)((yis_move_into(&v_digits, YV_INT(1)), v_digits));
  } else {
#line 227 "src/stdlib/json.yi"
    for (; yis_as_bool(YV_BOOL(true));) {
#line 228 "src/stdlib/json.yi"
      YisVal v_ch = yis_json__peek(v_s); yis_retain_val(v_ch);
#line 229 "src/stdlib/json.yi"
      if (yis_as_bool(YV_BOOL(!yis_as_bool(yis_json__is_digit(v_ch))))) {
        break;
      }
#line 230 "src/stdlib/json.yi"
      (void)(yis_json__next(v_s));
#line 231 "src/stdlib/json.yi"
      (void)((yis_move_into(&v_digits, yis_add(v_digits, YV_INT(1))), v_digits));
    }
  }
#line 233 "src/stdlib/json.yi"
  if (yis_as_bool(yis_eq(v_digits, YV_INT(0)))) {
#line 234 "src/stdlib/json.yi"
    (void)(yis_json__fail(v_s, YV_STR(stdr_str_lit("invalid number"))));
#line 235 "src/stdlib/json.yi"
    return YV_NULLV;
  }
#line 237 "src/stdlib/json.yi"
  if (yis_as_bool(yis_eq(yis_json__peek(v_s), YV_STR(stdr_str_lit("."))))) {
#line 238 "src/stdlib/json.yi"
    (void)(yis_json__next(v_s));
#line 239 "src/stdlib/json.yi"
    YisVal v_frac_digits = YV_INT(0); yis_retain_val(v_frac_digits);
#line 240 "src/stdlib/json.yi"
    for (; yis_as_bool(YV_BOOL(true));) {
#line 241 "src/stdlib/json.yi"
      YisVal v_ch = yis_json__peek(v_s); yis_retain_val(v_ch);
#line 242 "src/stdlib/json.yi"
      if (yis_as_bool(YV_BOOL(!yis_as_bool(yis_json__is_digit(v_ch))))) {
        break;
      }
#line 243 "src/stdlib/json.yi"
      (void)(yis_json__next(v_s));
#line 244 "src/stdlib/json.yi"
      (void)((yis_move_into(&v_frac_digits, yis_add(v_frac_digits, YV_INT(1))), v_frac_digits));
    }
#line 245 "src/stdlib/json.yi"
    if (yis_as_bool(yis_eq(v_frac_digits, YV_INT(0)))) {
#line 246 "src/stdlib/json.yi"
      (void)(yis_json__fail(v_s, YV_STR(stdr_str_lit("invalid number"))));
#line 247 "src/stdlib/json.yi"
      return YV_NULLV;
    }
  }
#line 249 "src/stdlib/json.yi"
  YisVal v_ech = yis_json__peek(v_s); yis_retain_val(v_ech);
#line 250 "src/stdlib/json.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_ech, YV_STR(stdr_str_lit("e")))) || yis_as_bool(yis_eq(v_ech, YV_STR(stdr_str_lit("E"))))))) {
#line 251 "src/stdlib/json.yi"
    (void)(yis_json__next(v_s));
#line 252 "src/stdlib/json.yi"
    YisVal v_sch = yis_json__peek(v_s); yis_retain_val(v_sch);
#line 253 "src/stdlib/json.yi"
    if (yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_sch, YV_STR(stdr_str_lit("+")))) || yis_as_bool(yis_eq(v_sch, YV_STR(stdr_str_lit("-"))))))) {
#line 254 "src/stdlib/json.yi"
      (void)(yis_json__next(v_s));
    }
#line 255 "src/stdlib/json.yi"
    YisVal v_exp_digits = YV_INT(0); yis_retain_val(v_exp_digits);
#line 256 "src/stdlib/json.yi"
    for (; yis_as_bool(YV_BOOL(true));) {
#line 257 "src/stdlib/json.yi"
      YisVal v_ch = yis_json__peek(v_s); yis_retain_val(v_ch);
#line 258 "src/stdlib/json.yi"
      if (yis_as_bool(YV_BOOL(!yis_as_bool(yis_json__is_digit(v_ch))))) {
        break;
      }
#line 259 "src/stdlib/json.yi"
      (void)(yis_json__next(v_s));
#line 260 "src/stdlib/json.yi"
      (void)((yis_move_into(&v_exp_digits, yis_add(v_exp_digits, YV_INT(1))), v_exp_digits));
    }
#line 261 "src/stdlib/json.yi"
    if (yis_as_bool(yis_eq(v_exp_digits, YV_INT(0)))) {
#line 262 "src/stdlib/json.yi"
      (void)(yis_json__fail(v_s, YV_STR(stdr_str_lit("invalid number"))));
#line 263 "src/stdlib/json.yi"
      return YV_NULLV;
    }
  }
#line 265 "src/stdlib/json.yi"
  YisVal v_end = YV_INT(stdr_num(((yis_index(v_s, YV_STR(stdr_str_lit("i")))).tag == EVT_NULL ? (v_start) : (yis_index(v_s, YV_STR(stdr_str_lit("i"))))))); yis_retain_val(v_end);
#line 266 "src/stdlib/json.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_le(v_end, v_start)) || yis_as_bool(yis_gt(v_end, v_n))))) {
#line 267 "src/stdlib/json.yi"
    (void)(yis_json__fail(v_s, YV_STR(stdr_str_lit("invalid number"))));
#line 268 "src/stdlib/json.yi"
    return YV_NULLV;
  }
#line 270 "src/stdlib/json.yi"
  YisVal v_text = stdr_slice(v_src, yis_as_int(v_start), yis_as_int(v_end)); yis_retain_val(v_text);
#line 271 "src/stdlib/json.yi"
  return yis_json__parse_num_text(v_text);
}

static YisVal yis_json__parse_value(YisVal v_s) {
#line 275 "src/stdlib/json.yi"
  (void)(yis_json__skip_ws(v_s));
#line 276 "src/stdlib/json.yi"
  YisVal v_ch = yis_json__peek(v_s); yis_retain_val(v_ch);
#line 278 "src/stdlib/json.yi"
  if (yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("\""))))) {
    return yis_json__parse_string(v_s);
  }
#line 279 "src/stdlib/json.yi"
  if (yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("{"))))) {
    return yis_json__parse_object(v_s);
  }
#line 280 "src/stdlib/json.yi"
  if (yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("["))))) {
    return yis_json__parse_array(v_s);
  }
#line 281 "src/stdlib/json.yi"
  if (yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("t"))))) {
#line 282 "src/stdlib/json.yi"
    if (yis_as_bool(yis_json__read_lit(v_s, YV_STR(stdr_str_lit("true"))))) {
      return YV_BOOL(true);
    }
#line 283 "src/stdlib/json.yi"
    return YV_NULLV;
  }
#line 284 "src/stdlib/json.yi"
  if (yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("f"))))) {
#line 285 "src/stdlib/json.yi"
    if (yis_as_bool(yis_json__read_lit(v_s, YV_STR(stdr_str_lit("false"))))) {
      return YV_BOOL(false);
    }
#line 286 "src/stdlib/json.yi"
    return YV_NULLV;
  }
#line 287 "src/stdlib/json.yi"
  if (yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("n"))))) {
#line 288 "src/stdlib/json.yi"
    if (yis_as_bool(yis_json__read_lit(v_s, YV_STR(stdr_str_lit("null"))))) {
      return YV_NULLV;
    }
#line 289 "src/stdlib/json.yi"
    return YV_NULLV;
  }
#line 290 "src/stdlib/json.yi"
  if (yis_as_bool(YV_BOOL(yis_as_bool(yis_eq(v_ch, YV_STR(stdr_str_lit("-")))) || yis_as_bool(yis_json__is_digit(v_ch))))) {
#line 291 "src/stdlib/json.yi"
    return yis_json__parse_number(v_s);
  }
#line 293 "src/stdlib/json.yi"
  (void)(yis_json__fail(v_s, YV_STR(stdr_str_lit("invalid value"))));
#line 294 "src/stdlib/json.yi"
  return YV_NULLV;
}

static YisVal yis_json__parse_array(YisVal v_s) {
#line 298 "src/stdlib/json.yi"
  if (yis_as_bool(YV_BOOL(!yis_as_bool(yis_json__expect(v_s, YV_STR(stdr_str_lit("["))))))) {
    return YV_NULLV;
  }
#line 299 "src/stdlib/json.yi"
  YisVal v_arr = yis_arr_lit(0); yis_retain_val(v_arr);
#line 301 "src/stdlib/json.yi"
  (void)(yis_json__skip_ws(v_s));
#line 302 "src/stdlib/json.yi"
  if (yis_as_bool(yis_eq(yis_json__peek(v_s), YV_STR(stdr_str_lit("]"))))) {
#line 303 "src/stdlib/json.yi"
    (void)(yis_json__next(v_s));
#line 304 "src/stdlib/json.yi"
    return v_arr;
  }
#line 306 "src/stdlib/json.yi"
  for (; yis_as_bool(YV_BOOL(true));) {
#line 307 "src/stdlib/json.yi"
    YisVal v_v = yis_json__parse_value(v_s); yis_retain_val(v_v);
#line 308 "src/stdlib/json.yi"
    if (yis_as_bool(YV_BOOL(!yis_as_bool(((yis_index(v_s, YV_STR(stdr_str_lit("ok")))).tag == EVT_NULL ? (YV_BOOL(false)) : (yis_index(v_s, YV_STR(stdr_str_lit("ok"))))))))) {
      return YV_NULLV;
    }
#line 309 "src/stdlib/json.yi"
    (void)(stdr_push(v_arr, v_v));
#line 311 "src/stdlib/json.yi"
    (void)(yis_json__skip_ws(v_s));
#line 312 "src/stdlib/json.yi"
    YisVal v_sep = yis_json__peek(v_s); yis_retain_val(v_sep);
#line 313 "src/stdlib/json.yi"
    if (yis_as_bool(yis_eq(v_sep, YV_STR(stdr_str_lit(","))))) {
#line 314 "src/stdlib/json.yi"
      (void)(yis_json__next(v_s));
#line 315 "src/stdlib/json.yi"
      continue;
    }
#line 316 "src/stdlib/json.yi"
    if (yis_as_bool(yis_eq(v_sep, YV_STR(stdr_str_lit("]"))))) {
#line 317 "src/stdlib/json.yi"
      (void)(yis_json__next(v_s));
#line 318 "src/stdlib/json.yi"
      return v_arr;
    }
#line 319 "src/stdlib/json.yi"
    (void)(yis_json__fail(v_s, YV_STR(stdr_str_lit("expected ',' or ']' in array"))));
#line 320 "src/stdlib/json.yi"
    return YV_NULLV;
  }
#line 321 "src/stdlib/json.yi"
  return YV_NULLV;
}

static YisVal yis_json__parse_object(YisVal v_s) {
#line 325 "src/stdlib/json.yi"
  if (yis_as_bool(YV_BOOL(!yis_as_bool(yis_json__expect(v_s, YV_STR(stdr_str_lit("{"))))))) {
    return YV_NULLV;
  }
#line 326 "src/stdlib/json.yi"
  YisVal v_obj = yis_dict_lit(0); yis_retain_val(v_obj);
#line 328 "src/stdlib/json.yi"
  (void)(yis_json__skip_ws(v_s));
#line 329 "src/stdlib/json.yi"
  if (yis_as_bool(yis_eq(yis_json__peek(v_s), YV_STR(stdr_str_lit("}"))))) {
#line 330 "src/stdlib/json.yi"
    (void)(yis_json__next(v_s));
#line 331 "src/stdlib/json.yi"
    return v_obj;
  }
#line 333 "src/stdlib/json.yi"
  for (; yis_as_bool(YV_BOOL(true));) {
#line 334 "src/stdlib/json.yi"
    (void)(yis_json__skip_ws(v_s));
#line 335 "src/stdlib/json.yi"
    if (yis_as_bool(yis_ne(yis_json__peek(v_s), YV_STR(stdr_str_lit("\""))))) {
#line 336 "src/stdlib/json.yi"
      (void)(yis_json__fail(v_s, YV_STR(stdr_str_lit("object key must be string"))));
#line 337 "src/stdlib/json.yi"
      return YV_NULLV;
    }
#line 338 "src/stdlib/json.yi"
    YisVal v_key = yis_json__parse_string(v_s); yis_retain_val(v_key);
#line 339 "src/stdlib/json.yi"
    if (yis_as_bool(YV_BOOL(!yis_as_bool(((yis_index(v_s, YV_STR(stdr_str_lit("ok")))).tag == EVT_NULL ? (YV_BOOL(false)) : (yis_index(v_s, YV_STR(stdr_str_lit("ok"))))))))) {
      return YV_NULLV;
    }
#line 341 "src/stdlib/json.yi"
    (void)(yis_json__skip_ws(v_s));
#line 342 "src/stdlib/json.yi"
    if (yis_as_bool(YV_BOOL(!yis_as_bool(yis_json__expect(v_s, YV_STR(stdr_str_lit(":"))))))) {
      return YV_NULLV;
    }
#line 344 "src/stdlib/json.yi"
    YisVal v_value = yis_json__parse_value(v_s); yis_retain_val(v_value);
#line 345 "src/stdlib/json.yi"
    if (yis_as_bool(YV_BOOL(!yis_as_bool(((yis_index(v_s, YV_STR(stdr_str_lit("ok")))).tag == EVT_NULL ? (YV_BOOL(false)) : (yis_index(v_s, YV_STR(stdr_str_lit("ok"))))))))) {
      return YV_NULLV;
    }
#line 346 "src/stdlib/json.yi"
    (void)(yis_index_set(v_obj, v_key, v_value));
#line 348 "src/stdlib/json.yi"
    (void)(yis_json__skip_ws(v_s));
#line 349 "src/stdlib/json.yi"
    YisVal v_sep = yis_json__peek(v_s); yis_retain_val(v_sep);
#line 350 "src/stdlib/json.yi"
    if (yis_as_bool(yis_eq(v_sep, YV_STR(stdr_str_lit(","))))) {
#line 351 "src/stdlib/json.yi"
      (void)(yis_json__next(v_s));
#line 352 "src/stdlib/json.yi"
      continue;
    }
#line 353 "src/stdlib/json.yi"
    if (yis_as_bool(yis_eq(v_sep, YV_STR(stdr_str_lit("}"))))) {
#line 354 "src/stdlib/json.yi"
      (void)(yis_json__next(v_s));
#line 355 "src/stdlib/json.yi"
      return v_obj;
    }
#line 356 "src/stdlib/json.yi"
    (void)(yis_json__fail(v_s, YV_STR(stdr_str_lit("expected ',' or '}' in object"))));
#line 357 "src/stdlib/json.yi"
    return YV_NULLV;
  }
#line 358 "src/stdlib/json.yi"
  return YV_NULLV;
}

static YisVal yis_json_parse_result(YisVal v_src) {
#line 362 "src/stdlib/json.yi"
  YisVal v_s = yis_json__state_new(v_src); yis_retain_val(v_s);
#line 363 "src/stdlib/json.yi"
  YisVal v_value = yis_json__parse_value(v_s); yis_retain_val(v_value);
#line 364 "src/stdlib/json.yi"
  if (yis_as_bool(((yis_index(v_s, YV_STR(stdr_str_lit("ok")))).tag == EVT_NULL ? (YV_BOOL(false)) : (yis_index(v_s, YV_STR(stdr_str_lit("ok"))))))) {
#line 365 "src/stdlib/json.yi"
    (void)(yis_json__skip_ws(v_s));
#line 366 "src/stdlib/json.yi"
    YisVal v_cur_i = YV_INT(stdr_num(((yis_index(v_s, YV_STR(stdr_str_lit("i")))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_s, YV_STR(stdr_str_lit("i"))))))); yis_retain_val(v_cur_i);
#line 367 "src/stdlib/json.yi"
    YisVal v_total_n = YV_INT(stdr_num(((yis_index(v_s, YV_STR(stdr_str_lit("n")))).tag == EVT_NULL ? (YV_INT(0)) : (yis_index(v_s, YV_STR(stdr_str_lit("n"))))))); yis_retain_val(v_total_n);
#line 368 "src/stdlib/json.yi"
    if (yis_as_bool(yis_ne(v_cur_i, v_total_n))) {
#line 369 "src/stdlib/json.yi"
      (void)(yis_json__fail(v_s, YV_STR(stdr_str_lit("trailing characters"))));
    }
  }
#line 371 "src/stdlib/json.yi"
  YisVal v_r = yis_dict_lit(0); yis_retain_val(v_r);
#line 372 "src/stdlib/json.yi"
  (void)(yis_index_set(v_r, YV_STR(stdr_str_lit("ok")), ((yis_index(v_s, YV_STR(stdr_str_lit("ok")))).tag == EVT_NULL ? (YV_BOOL(false)) : (yis_index(v_s, YV_STR(stdr_str_lit("ok")))))));
#line 373 "src/stdlib/json.yi"
  (void)(yis_index_set(v_r, YV_STR(stdr_str_lit("value")), v_value));
#line 374 "src/stdlib/json.yi"
  (void)(yis_index_set(v_r, YV_STR(stdr_str_lit("error")), ((yis_index(v_s, YV_STR(stdr_str_lit("error")))).tag == EVT_NULL ? (YV_STR(stdr_str_lit(""))) : (yis_index(v_s, YV_STR(stdr_str_lit("error")))))));
#line 375 "src/stdlib/json.yi"
  return v_r;
}

static YisVal yis_json_parse(YisVal v_src) {
#line 379 "src/stdlib/json.yi"
  YisVal v_r = yis_json_parse_result(v_src); yis_retain_val(v_r);
#line 380 "src/stdlib/json.yi"
  if (yis_as_bool(((yis_index(v_r, YV_STR(stdr_str_lit("ok")))).tag == EVT_NULL ? (YV_BOOL(false)) : (yis_index(v_r, YV_STR(stdr_str_lit("ok"))))))) {
#line 381 "src/stdlib/json.yi"
    return yis_index(v_r, YV_STR(stdr_str_lit("value")));
  }
#line 382 "src/stdlib/json.yi"
  return YV_NULLV;
}

static YisVal yis_json_is_object(YisVal v_v) {
#line 386 "src/stdlib/json.yi"
  return yis_eq(YV_STR(stdr_to_string(v_v)), YV_STR(stdr_str_lit("[dict]")));
}

static YisVal yis_json_is_array(YisVal v_v) {
#line 390 "src/stdlib/json.yi"
  return yis_eq(YV_STR(stdr_to_string(v_v)), YV_STR(stdr_str_lit("[array]")));
}

static YisVal yis_json_get(YisVal v_obj, YisVal v_key, YisVal v_fallback) {
#line 394 "src/stdlib/json.yi"
  if (yis_as_bool(YV_BOOL(!yis_as_bool(yis_json_is_object(v_obj))))) {
    return v_fallback;
  }
#line 395 "src/stdlib/json.yi"
  YisVal v_v = yis_index(v_obj, v_key); yis_retain_val(v_v);
#line 396 "src/stdlib/json.yi"
  if (yis_as_bool(YV_BOOL(stdr_is_null(v_v)))) {
    return v_fallback;
  }
#line 397 "src/stdlib/json.yi"
  return v_v;
}

static YisVal __fnwrap_json_escape(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_json_escape(__a0);
}

static YisVal __fnwrap_json__state_new(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_json__state_new(__a0);
}

static YisVal __fnwrap_json__fail(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  yis_json__fail(__a0, __a1);
  return YV_NULLV;
}

static YisVal __fnwrap_json__peek(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_json__peek(__a0);
}

static YisVal __fnwrap_json__next(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_json__next(__a0);
}

static YisVal __fnwrap_json__is_ws(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_json__is_ws(__a0);
}

static YisVal __fnwrap_json__skip_ws(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  yis_json__skip_ws(__a0);
  return YV_NULLV;
}

static YisVal __fnwrap_json__is_digit(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_json__is_digit(__a0);
}

static YisVal __fnwrap_json__is_hex(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_json__is_hex(__a0);
}

static YisVal __fnwrap_json__expect(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  return yis_json__expect(__a0, __a1);
}

static YisVal __fnwrap_json__read_lit(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  return yis_json__read_lit(__a0, __a1);
}

static YisVal __fnwrap_json__pow10(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_json__pow10(__a0);
}

static YisVal __fnwrap_json__parse_num_text(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_json__parse_num_text(__a0);
}

static YisVal __fnwrap_json__parse_string(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_json__parse_string(__a0);
}

static YisVal __fnwrap_json__parse_number(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_json__parse_number(__a0);
}

static YisVal __fnwrap_json__parse_value(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_json__parse_value(__a0);
}

static YisVal __fnwrap_json__parse_array(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_json__parse_array(__a0);
}

static YisVal __fnwrap_json__parse_object(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_json__parse_object(__a0);
}

static YisVal __fnwrap_json_parse_result(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_json_parse_result(__a0);
}

static YisVal __fnwrap_json_parse(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_json_parse(__a0);
}

static YisVal __fnwrap_json_is_object(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_json_is_object(__a0);
}

static YisVal __fnwrap_json_is_array(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  return yis_json_is_array(__a0);
}

static YisVal __fnwrap_json_get(void* env, int argc, YisVal* argv) {
  (void)env; (void)argc;
  YisVal __a0 = argc > 0 ? argv[0] : YV_NULLV;
  YisVal __a1 = argc > 1 ? argv[1] : YV_NULLV;
  YisVal __a2 = argc > 2 ? argv[2] : YV_NULLV;
  return yis_json_get(__a0, __a1, __a2);
}

/* end embedded module: json */

/* begin main unit */
static void yis_entry(void);

// cask json_parse_demo
// bring stdr
// bring json
static void yis_entry(void) {
  YisVal v_src = YV_STR(stdr_str_lit("{\"name\":\"Yis\",\"ok\":true,\"nums\":[1,2,3],\"meta\":{\"lang\":\"yi\"}}")); yis_retain_val(v_src);
  YisVal v_r = yis_json_parse_result(YV_STR(stdr_str_lit("{\"name\":\"Yis\",\"ok\":true,\"nums\":[1,2,3],\"meta\":{\"lang\":\"yi\"}}"))); yis_retain_val(v_r);
  if (yis_as_bool(YV_BOOL(!yis_as_bool(((yis_index(v_r, YV_STR(stdr_str_lit("ok")))).tag == EVT_NULL ? (YV_BOOL(false)) : (yis_index(v_r, YV_STR(stdr_str_lit("ok"))))))))) {
    YisVal v_err_msg = ((yis_index(v_r, YV_STR(stdr_str_lit("error")))).tag == EVT_NULL ? (YV_STR(stdr_str_lit("unknown"))) : (yis_index(v_r, YV_STR(stdr_str_lit("error"))))); yis_retain_val(v_err_msg);
    (void)(stdr_write(({ YisVal __ip[3]; __ip[0] = YV_STR(stdr_str_lit("Parse error: ")); __ip[1] = YV_STR(stdr_to_string(v_err_msg)); __ip[2] = YV_STR(stdr_str_lit("\n")); YV_STR(stdr_str_from_parts(3, __ip)); })));
    return;
  }
  YisVal v_root = yis_index(v_r, YV_STR(stdr_str_lit("value"))); yis_retain_val(v_root);
  YisVal v_name = yis_json_get(v_root, YV_STR(stdr_str_lit("name")), YV_STR(stdr_str_lit(""))); yis_retain_val(v_name);
  YisVal v_ok = yis_json_get(v_root, YV_STR(stdr_str_lit("ok")), YV_BOOL(false)); yis_retain_val(v_ok);
  YisVal v_nums = yis_json_get(v_root, YV_STR(stdr_str_lit("nums")), yis_arr_lit(0)); yis_retain_val(v_nums);
  YisVal v_meta = yis_json_get(v_root, YV_STR(stdr_str_lit("meta")), yis_dict_lit(0)); yis_retain_val(v_meta);
  YisVal v_lang = yis_json_get(v_meta, YV_STR(stdr_str_lit("lang")), YV_STR(stdr_str_lit(""))); yis_retain_val(v_lang);
  YisVal v_nums_len = YV_INT(stdr_len(v_nums)); yis_retain_val(v_nums_len);
  (void)(stdr_write(({ YisVal __ip[9]; __ip[0] = YV_STR(stdr_str_lit("name=")); __ip[1] = YV_STR(stdr_to_string(v_name)); __ip[2] = YV_STR(stdr_str_lit(" ok=")); __ip[3] = YV_STR(stdr_to_string(v_ok)); __ip[4] = YV_STR(stdr_str_lit(" nums_len=")); __ip[5] = YV_STR(stdr_to_string(v_nums_len)); __ip[6] = YV_STR(stdr_str_lit(" lang=")); __ip[7] = YV_STR(stdr_to_string(v_lang)); __ip[8] = YV_STR(stdr_str_lit("\n")); YV_STR(stdr_str_from_parts(9, __ip)); })));
  YisVal v_bee = yis_add(YV_STR(stdr_str_lit("B")), YV_STR(stdr_str_lit("ee"))); yis_retain_val(v_bee);
  (void)(stdr_write(({ YisVal __ip[2]; __ip[0] = YV_STR(stdr_to_string(v_bee)); __ip[1] = YV_STR(stdr_str_lit("\n")); YV_STR(stdr_str_from_parts(2, __ip)); })));
}

/* end main unit */

int main(int argc, char **argv) {
  yis_set_args(argc, argv);
  yis_runtime_init();
  yis_entry();
  return 0;
}
