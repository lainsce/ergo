// ---- Ergo runtime (minimal) ----
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
#if defined(__APPLE__)
#include <CoreGraphics/CoreGraphics.h>
#endif
#if defined(_WIN32)
#include <windows.h>
#include <io.h>
#else
int isatty(int);
int fileno(FILE*);
#endif

static int ergo_stdout_isatty = 0;

static bool cogito_debug_enabled(void);
static const char* cogito_font_path_active = NULL;

static void ergo_runtime_init(void) {
#if defined(__APPLE__)
  if (cogito_debug_enabled()) {
    fprintf(stderr, "cogito: runtime_init\n");
    fflush(stderr);
  }
#endif
#if defined(_WIN32)
  ergo_stdout_isatty = _isatty(_fileno(stdout));
#else
  ergo_stdout_isatty = isatty(fileno(stdout));
#endif
  if (!ergo_stdout_isatty) {
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
  EVT_OBJ,
  EVT_FN
} ErgoTag;

typedef struct ErgoVal ErgoVal;

typedef struct ErgoStr {
  int ref;
  size_t len;
  char* data;
} ErgoStr;

typedef struct ErgoArr {
  int ref;
  size_t len;
  size_t cap;
  ErgoVal* items;
} ErgoArr;

typedef struct ErgoObj {
  int ref;
  void (*drop)(struct ErgoObj*);
} ErgoObj;

typedef struct ErgoFn {
  int ref;
  int arity;
  ErgoVal (*fn)(void* env, int argc, ErgoVal* argv);
  void* env;
  int env_size;
} ErgoFn;

struct ErgoVal {
  ErgoTag tag;
  union {
    int64_t i;
    double f;
    bool b;
    void* p;
  } as;
};

#define EV_NULLV ((ErgoVal){.tag=EVT_NULL})
#define EV_INT(x) ((ErgoVal){.tag=EVT_INT, .as.i=(int64_t)(x)})
#define EV_FLOAT(x) ((ErgoVal){.tag=EVT_FLOAT, .as.f=(double)(x)})
#define EV_BOOL(x) ((ErgoVal){.tag=EVT_BOOL, .as.b=(x)?true:false})
#define EV_STR(x) ((ErgoVal){.tag=EVT_STR, .as.p=(x)})
#define EV_ARR(x) ((ErgoVal){.tag=EVT_ARR, .as.p=(x)})
#define EV_OBJ(x) ((ErgoVal){.tag=EVT_OBJ, .as.p=(x)})
#define EV_FN(x) ((ErgoVal){.tag=EVT_FN, .as.p=(x)})

static void ergo_trap(const char* msg) {
  fprintf(stderr, "runtime error: %s\n", msg ? msg : "unknown error");
  fprintf(stderr, "  (run with debugger for stack trace)\n");
  abort();
}

static void ergo_retain_val(ErgoVal v);
static void ergo_release_val(ErgoVal v);

static ErgoStr* stdr_str_lit(const char* s) {
  size_t n = strlen(s);
  ErgoStr* st = (ErgoStr*)malloc(sizeof(ErgoStr));
  st->ref = 1;
  st->len = n;
  st->data = (char*)malloc(n + 1);
  memcpy(st->data, s, n + 1);
  return st;
}

static ErgoStr* stdr_str_from_parts(int n, ErgoVal* parts);
static ErgoStr* stdr_to_string(ErgoVal v);
static ErgoStr* stdr_str_from_slice(const char* s, size_t len);
static ErgoArr* stdr_arr_new(int n);
static void ergo_arr_add(ErgoArr* a, ErgoVal v);
static ErgoVal ergo_arr_get(ErgoArr* a, int64_t idx);
static void ergo_arr_set(ErgoArr* a, int64_t idx, ErgoVal v);
static ErgoVal ergo_arr_remove(ErgoArr* a, int64_t idx);

static ErgoVal stdr_str_at(ErgoVal v, int64_t idx) {
  if (v.tag != EVT_STR) ergo_trap("str_at expects string");
  ErgoStr* s = (ErgoStr*)v.as.p;
  if (idx < 0 || (size_t)idx >= s->len) return EV_STR(stdr_str_lit(""));
  return EV_STR(stdr_str_from_slice(s->data + idx, 1));
}

static int stdr_len(ErgoVal v) {
  if (v.tag == EVT_STR) return (int)((ErgoStr*)v.as.p)->len;
  if (v.tag == EVT_ARR) return (int)((ErgoArr*)v.as.p)->len;
  return 0;
}

static bool stdr_is_null(ErgoVal v) { return v.tag == EVT_NULL; }

static void stdr_write(ErgoVal v) {
  ErgoStr* s = stdr_to_string(v);
  fwrite(s->data, 1, s->len, stdout);
  if (ergo_stdout_isatty) fflush(stdout);
  ergo_release_val(EV_STR(s));
}

static void writef(ErgoVal fmt, int argc, ErgoVal* argv) {
  if (fmt.tag != EVT_STR) ergo_trap("writef expects string");
  ErgoStr* s = (ErgoStr*)fmt.as.p;
  size_t i = 0;
  size_t seg = 0;
  int argi = 0;
  while (i < s->len) {
    if (i + 1 < s->len && s->data[i] == '{' && s->data[i + 1] == '}') {
      if (i > seg) fwrite(s->data + seg, 1, i - seg, stdout);
      if (argi < argc) {
        ErgoStr* ps = stdr_to_string(argv[argi++]);
        fwrite(ps->data, 1, ps->len, stdout);
        ergo_release_val(EV_STR(ps));
      }
      i += 2;
      seg = i;
      continue;
    }
    i++;
  }
  if (i > seg) fwrite(s->data + seg, 1, i - seg, stdout);
  if (ergo_stdout_isatty) fflush(stdout);
}

static void stdr_writef_args(ErgoVal fmt, ErgoVal args) {
  if (args.tag != EVT_ARR) ergo_trap("writef expects args tuple");
  ErgoArr* a = (ErgoArr*)args.as.p;
  writef(fmt, (int)a->len, a->items);
}

static ErgoStr* stdr_read_line(void) {
  size_t cap = 128;
  size_t len = 0;
  char* buf = (char*)malloc(cap);
  if (!buf) ergo_trap("out of memory");
  int c;
  while ((c = fgetc(stdin)) != EOF) {
    if (c == '\n') break;
    if (len + 1 >= cap) {
      cap *= 2;
      buf = (char*)realloc(buf, cap);
      if (!buf) ergo_trap("out of memory");
    }
    buf[len++] = (char)c;
  }
  if (len > 0 && buf[len - 1] == '\r') len--;
  buf[len] = 0;
  ErgoStr* s = (ErgoStr*)malloc(sizeof(ErgoStr));
  if (!s) ergo_trap("out of memory");
  s->ref = 1;
  s->len = len;
  s->data = buf;
  return s;
}

static ErgoVal stdr_read_text_file(ErgoVal pathv) {
  if (pathv.tag != EVT_STR) ergo_trap("read_text_file expects string path");
  ErgoStr* path = (ErgoStr*)pathv.as.p;
  FILE* f = fopen(path->data, "rb");
  if (!f) return EV_NULLV;
  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return EV_NULLV;
  }
  long sz = ftell(f);
  if (sz < 0) {
    fclose(f);
    return EV_NULLV;
  }
  if (fseek(f, 0, SEEK_SET) != 0) {
    fclose(f);
    return EV_NULLV;
  }
  size_t len = (size_t)sz;
  ErgoStr* out = (ErgoStr*)malloc(sizeof(ErgoStr));
  if (!out) {
    fclose(f);
    ergo_trap("out of memory");
  }
  out->data = (char*)malloc(len + 1);
  if (!out->data) {
    fclose(f);
    free(out);
    ergo_trap("out of memory");
  }
  size_t n = 0;
  if (len > 0) n = fread(out->data, 1, len, f);
  fclose(f);
  if (n != len) {
    free(out->data);
    free(out);
    return EV_NULLV;
  }
  out->ref = 1;
  out->len = len;
  out->data[len] = 0;
  return EV_STR(out);
}

static ErgoVal stdr_write_text_file(ErgoVal pathv, ErgoVal textv) {
  if (pathv.tag != EVT_STR) ergo_trap("write_text_file expects string path");
  if (textv.tag != EVT_STR) ergo_trap("write_text_file expects string text");
  ErgoStr* path = (ErgoStr*)pathv.as.p;
  ErgoStr* text = (ErgoStr*)textv.as.p;
  FILE* f = fopen(path->data, "wb");
  if (!f) return EV_BOOL(false);
  size_t n = 0;
  if (text->len > 0) n = fwrite(text->data, 1, text->len, f);
  bool ok = (n == text->len) && (fclose(f) == 0);
  return EV_BOOL(ok);
}

static ErgoVal stdr_capture_shell_first_line(const char* cmd) {
  if (!cmd || !cmd[0]) return EV_NULLV;
#if defined(_WIN32)
  FILE* p = _popen(cmd, "r");
#else
  FILE* p = popen(cmd, "r");
#endif
  if (!p) return EV_NULLV;
  char buf[4096];
  if (!fgets(buf, sizeof(buf), p)) {
#if defined(_WIN32)
    _pclose(p);
#else
    pclose(p);
#endif
    return EV_NULLV;
  }
#if defined(_WIN32)
  _pclose(p);
#else
  pclose(p);
#endif
  size_t len = strlen(buf);
  while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) len--;
  if (len == 0) return EV_NULLV;
  return EV_STR(stdr_str_from_slice(buf, len));
}

static ErgoVal stdr_open_file_dialog(ErgoVal promptv, ErgoVal extv) {
  if (promptv.tag != EVT_STR) ergo_trap("open_file_dialog expects prompt string");
  if (extv.tag != EVT_STR) ergo_trap("open_file_dialog expects extension string");
  ErgoStr* prompt = (ErgoStr*)promptv.as.p;
  ErgoStr* ext = (ErgoStr*)extv.as.p;
#if defined(__APPLE__)
  char cmd[8192];
  snprintf(cmd, sizeof(cmd),
           "osascript -e 'set _p to POSIX path of (choose file of type {\"%s\"} with prompt \"%s\")' -e 'return _p' 2>/dev/null",
           ext ? ext->data : "", prompt ? prompt->data : "");
  return stdr_capture_shell_first_line(cmd);
#else
  (void)prompt;
  (void)ext;
  return EV_NULLV;
#endif
}

static ErgoVal stdr_save_file_dialog(ErgoVal promptv, ErgoVal default_namev, ErgoVal extv) {
  if (promptv.tag != EVT_STR) ergo_trap("save_file_dialog expects prompt string");
  if (default_namev.tag != EVT_STR) ergo_trap("save_file_dialog expects default_name string");
  if (extv.tag != EVT_STR) ergo_trap("save_file_dialog expects extension string");
  ErgoStr* prompt = (ErgoStr*)promptv.as.p;
  ErgoStr* def = (ErgoStr*)default_namev.as.p;
  ErgoStr* ext = (ErgoStr*)extv.as.p;
#if defined(__APPLE__)
  char cmd[8192];
  snprintf(cmd, sizeof(cmd),
           "osascript -e 'set _p to POSIX path of (choose file name with prompt \"%s\" default name \"%s\")' -e 'return _p' 2>/dev/null",
           prompt ? prompt->data : "", def ? def->data : "");
  ErgoVal out = stdr_capture_shell_first_line(cmd);
  (void)ext;
  return out;
#else
  (void)prompt;
  (void)def;
  (void)ext;
  return EV_NULLV;
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

static ErgoStr* stdr_str_from_slice(const char* s, size_t len) {
  ErgoStr* st = (ErgoStr*)malloc(sizeof(ErgoStr));
  if (!st) ergo_trap("out of memory");
  st->ref = 1;
  st->len = len;
  st->data = (char*)malloc(len + 1);
  if (!st->data) ergo_trap("out of memory");
  if (len > 0) memcpy(st->data, s, len);
  st->data[len] = 0;
  return st;
}

static int64_t stdr_parse_int_slice(const char* s, size_t len) {
  if (len == 0) return 0;
  char* tmp = (char*)malloc(len + 1);
  if (!tmp) ergo_trap("out of memory");
  memcpy(tmp, s, len);
  tmp[len] = 0;
  char* end = NULL;
  long long v = strtoll(tmp, &end, 10);
  free(tmp);
  if (end == tmp) return 0;
  return (int64_t)v;
}

static double stdr_parse_float_slice(const char* s, size_t len) {
  if (len == 0) return 0.0;
  char* tmp = (char*)malloc(len + 1);
  if (!tmp) ergo_trap("out of memory");
  memcpy(tmp, s, len);
  tmp[len] = 0;
  char* end = NULL;
  double v = strtod(tmp, &end);
  free(tmp);
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

static ErgoVal stdr_readf_parse(ErgoVal fmt, ErgoVal line, ErgoVal args) {
  if (fmt.tag != EVT_STR) ergo_trap("readf expects string format");
  if (line.tag != EVT_STR) ergo_trap("readf expects string input");
  if (args.tag != EVT_ARR) ergo_trap("readf expects args tuple");

  ErgoStr* fs = (ErgoStr*)fmt.as.p;
  ErgoStr* ls = (ErgoStr*)line.as.p;
  ErgoArr* a = (ErgoArr*)args.as.p;

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

  const char** seg_ptrs = (const char**)malloc(sizeof(char*) * segs);
  size_t* seg_lens = (size_t*)malloc(sizeof(size_t) * segs);
  if (!seg_ptrs || !seg_lens) ergo_trap("out of memory");

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

  ErgoArr* out = stdr_arr_new((int)a->len);

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

    ErgoVal hint = a->items[i];
    ErgoVal v;
    if (hint.tag == EVT_INT) {
      v = EV_INT(stdr_parse_int_slice(cap, trim_len));
    } else if (hint.tag == EVT_FLOAT) {
      v = EV_FLOAT(stdr_parse_float_slice(cap, trim_len));
    } else if (hint.tag == EVT_BOOL) {
      v = EV_BOOL(stdr_parse_bool_slice(cap, trim_len));
    } else if (hint.tag == EVT_STR) {
      v = EV_STR(stdr_str_from_slice(cap, trim_len));
    } else {
      v = EV_STR(stdr_str_from_slice(cap, trim_len));
    }
    ergo_arr_add(out, v);
  }

  free(seg_ptrs);
  free(seg_lens);

  return EV_ARR(out);
}

static ErgoStr* stdr_to_string(ErgoVal v) {
  char buf[64];
  if (v.tag == EVT_NULL) return stdr_str_lit("null");
  if (v.tag == EVT_BOOL) return stdr_str_lit(v.as.b ? "true" : "false");
  if (v.tag == EVT_INT) {
    snprintf(buf, sizeof(buf), "%lld", (long long)v.as.i);
    return stdr_str_lit(buf);
  }
  if (v.tag == EVT_FLOAT) {
    snprintf(buf, sizeof(buf), "%.6f", v.as.f);
    return stdr_str_lit(buf);
  }
  if (v.tag == EVT_STR) {
    ergo_retain_val(v);
    return (ErgoStr*)v.as.p;
  }
  if (v.tag == EVT_ARR) return stdr_str_lit("[array]");
  if (v.tag == EVT_OBJ) return stdr_str_lit("[object]");
  if (v.tag == EVT_FN) return stdr_str_lit("[function]");
  return stdr_str_lit("<?>");
}

static ErgoStr* stdr_str_from_parts(int n, ErgoVal* parts) {
  size_t total = 0;
  ErgoStr** strs = (ErgoStr**)malloc(sizeof(ErgoStr*) * (size_t)n);
  for (int i = 0; i < n; i++) {
    strs[i] = stdr_to_string(parts[i]);
    total += strs[i]->len;
  }
  ErgoStr* out = (ErgoStr*)malloc(sizeof(ErgoStr));
  out->ref = 1;
  out->len = total;
  out->data = (char*)malloc(total + 1);
  size_t off = 0;
  for (int i = 0; i < n; i++) {
    memcpy(out->data + off, strs[i]->data, strs[i]->len);
    off += strs[i]->len;
    ergo_release_val(EV_STR(strs[i]));
  }
  out->data[total] = 0;
  free(strs);
  return out;
}

static void ergo_retain_val(ErgoVal v) {
  if (v.tag == EVT_STR) ((ErgoStr*)v.as.p)->ref++;
  else if (v.tag == EVT_ARR) ((ErgoArr*)v.as.p)->ref++;
  else if (v.tag == EVT_OBJ) ((ErgoObj*)v.as.p)->ref++;
  else if (v.tag == EVT_FN) ((ErgoFn*)v.as.p)->ref++;
}

static void ergo_release_val(ErgoVal v) {
  if (v.tag == EVT_STR) {
    ErgoStr* s = (ErgoStr*)v.as.p;
    if (--s->ref == 0) {
      free(s->data);
      free(s);
    }
  } else if (v.tag == EVT_ARR) {
    ErgoArr* a = (ErgoArr*)v.as.p;
    if (--a->ref == 0) {
      for (size_t i = 0; i < a->len; i++) ergo_release_val(a->items[i]);
      free(a->items);
      free(a);
    }
  } else if (v.tag == EVT_OBJ) {
    ErgoObj* o = (ErgoObj*)v.as.p;
    if (--o->ref == 0) {
      if (o->drop) o->drop(o);
      free(o);
    }
  } else if (v.tag == EVT_FN) {
    ErgoFn* f = (ErgoFn*)v.as.p;
    if (--f->ref == 0) {
      if (f->env && f->env_size > 0) {
        ErgoVal* caps = (ErgoVal*)f->env;
        for (int i = 0; i < f->env_size; i++) ergo_release_val(caps[i]);
        free(f->env);
      }
      free(f);
    }
  }
}

static ErgoVal ergo_move(ErgoVal* slot) {
  ErgoVal v = *slot;
  *slot = EV_NULLV;
  return v;
}

static void ergo_move_into(ErgoVal* slot, ErgoVal v) {
  ergo_release_val(*slot);
  *slot = v;
}

static int64_t ergo_as_int(ErgoVal v) {
  if (v.tag == EVT_INT) return v.as.i;
  if (v.tag == EVT_BOOL) return v.as.b ? 1 : 0;
  if (v.tag == EVT_FLOAT) return (int64_t)v.as.f;
  ergo_trap("type mismatch: expected int");
  return 0;
}

static double ergo_as_float(ErgoVal v) {
  if (v.tag == EVT_FLOAT) return v.as.f;
  if (v.tag == EVT_INT) return (double)v.as.i;
  ergo_trap("type mismatch: expected float");
  return 0.0;
}

static bool ergo_as_bool(ErgoVal v) {
  if (v.tag == EVT_BOOL) return v.as.b;
  if (v.tag == EVT_NULL) return false;
  if (v.tag == EVT_INT) return v.as.i != 0;
  if (v.tag == EVT_FLOAT) return v.as.f != 0.0;
  if (v.tag == EVT_STR) return ((ErgoStr*)v.as.p)->len != 0;
  if (v.tag == EVT_ARR) return ((ErgoArr*)v.as.p)->len != 0;
  return true;
}

static ErgoVal ergo_add(ErgoVal a, ErgoVal b) {
  if (a.tag == EVT_FLOAT || b.tag == EVT_FLOAT) return EV_FLOAT(ergo_as_float(a) + ergo_as_float(b));
  return EV_INT(ergo_as_int(a) + ergo_as_int(b));
}

static ErgoVal ergo_sub(ErgoVal a, ErgoVal b) {
  if (a.tag == EVT_FLOAT || b.tag == EVT_FLOAT) return EV_FLOAT(ergo_as_float(a) - ergo_as_float(b));
  return EV_INT(ergo_as_int(a) - ergo_as_int(b));
}

static ErgoVal ergo_mul(ErgoVal a, ErgoVal b) {
  if (a.tag == EVT_FLOAT || b.tag == EVT_FLOAT) return EV_FLOAT(ergo_as_float(a) * ergo_as_float(b));
  return EV_INT(ergo_as_int(a) * ergo_as_int(b));
}

static ErgoVal ergo_div(ErgoVal a, ErgoVal b) {
  if (a.tag == EVT_FLOAT || b.tag == EVT_FLOAT) return EV_FLOAT(ergo_as_float(a) / ergo_as_float(b));
  return EV_INT(ergo_as_int(a) / ergo_as_int(b));
}

static ErgoVal ergo_mod(ErgoVal a, ErgoVal b) {
  if (a.tag == EVT_FLOAT || b.tag == EVT_FLOAT) ergo_trap("% expects integer");
  return EV_INT(ergo_as_int(a) % ergo_as_int(b));
}

static ErgoVal ergo_neg(ErgoVal a) {
  if (a.tag == EVT_FLOAT) return EV_FLOAT(-a.as.f);
  return EV_INT(-ergo_as_int(a));
}

static ErgoVal ergo_eq(ErgoVal a, ErgoVal b) {
  if (a.tag != b.tag) return EV_BOOL(false);
  switch (a.tag) {
    case EVT_NULL: return EV_BOOL(true);
    case EVT_BOOL: return EV_BOOL(a.as.b == b.as.b);
    case EVT_INT: return EV_BOOL(a.as.i == b.as.i);
    case EVT_FLOAT: return EV_BOOL(a.as.f == b.as.f);
    case EVT_STR: {
      ErgoStr* sa = (ErgoStr*)a.as.p;
      ErgoStr* sb = (ErgoStr*)b.as.p;
      if (sa->len != sb->len) return EV_BOOL(false);
      return EV_BOOL(memcmp(sa->data, sb->data, sa->len) == 0);
    }
    default: return EV_BOOL(a.as.p == b.as.p);
  }
}

static ErgoVal ergo_ne(ErgoVal a, ErgoVal b) {
  ErgoVal v = ergo_eq(a, b);
  return EV_BOOL(!v.as.b);
}

static ErgoVal ergo_lt(ErgoVal a, ErgoVal b) { return EV_BOOL(ergo_as_float(a) < ergo_as_float(b)); }
static ErgoVal ergo_le(ErgoVal a, ErgoVal b) { return EV_BOOL(ergo_as_float(a) <= ergo_as_float(b)); }
static ErgoVal ergo_gt(ErgoVal a, ErgoVal b) { return EV_BOOL(ergo_as_float(a) > ergo_as_float(b)); }
static ErgoVal ergo_ge(ErgoVal a, ErgoVal b) { return EV_BOOL(ergo_as_float(a) >= ergo_as_float(b)); }

static ErgoArr* stdr_arr_new(int n) {
  ErgoArr* a = (ErgoArr*)malloc(sizeof(ErgoArr));
  a->ref = 1;
  a->len = 0;
  a->cap = (n > 0) ? (size_t)n : 4;
  a->items = (ErgoVal*)malloc(sizeof(ErgoVal) * a->cap);
  return a;
}

static void ergo_arr_add(ErgoArr* a, ErgoVal v) {
  if (a->len >= a->cap) {
    a->cap *= 2;
    a->items = (ErgoVal*)realloc(a->items, sizeof(ErgoVal) * a->cap);
  }
  a->items[a->len++] = v;
}

static ErgoVal ergo_arr_get(ErgoArr* a, int64_t idx) {
  if (idx < 0 || (size_t)idx >= a->len) return EV_NULLV;
  ErgoVal v = a->items[idx];
  ergo_retain_val(v);
  return v;
}

static void ergo_arr_set(ErgoArr* a, int64_t idx, ErgoVal v) {
  if (idx < 0 || (size_t)idx >= a->len) return;
  ergo_release_val(a->items[idx]);
  a->items[idx] = v;
}

static ErgoVal ergo_arr_remove(ErgoArr* a, int64_t idx) {
  if (idx < 0 || (size_t)idx >= a->len) return EV_NULLV;
  ErgoVal v = a->items[idx];
  for (size_t i = (size_t)idx; i + 1 < a->len; i++) {
    a->items[i] = a->items[i + 1];
  }
  a->len--;
  return v;
}

static ErgoObj* ergo_obj_new(size_t size, void (*drop)(ErgoObj*)) {
  ErgoObj* o = (ErgoObj*)malloc(size);
  o->ref = 1;
  o->drop = drop;
  return o;
}

static ErgoFn* ergo_fn_new(ErgoVal (*fn)(void* env, int argc, ErgoVal* argv), int arity) {
  ErgoFn* f = (ErgoFn*)malloc(sizeof(ErgoFn));
  f->ref = 1;
  f->arity = arity;
  f->fn = fn;
  f->env = NULL;
  f->env_size = 0;
  return f;
}

static ErgoFn* ergo_fn_new_with_env(ErgoVal (*fn)(void* env, int argc, ErgoVal* argv), int arity, void* env, int env_size) {
  ErgoFn* f = (ErgoFn*)malloc(sizeof(ErgoFn));
  f->ref = 1;
  f->arity = arity;
  f->fn = fn;
  f->env = env;
  f->env_size = env_size;
  return f;
}

static ErgoVal ergo_call(ErgoVal fval, int argc, ErgoVal* argv) {
  if (fval.tag != EVT_FN) ergo_trap("call expects function");
  ErgoFn* f = (ErgoFn*)fval.as.p;
  if (f->arity >= 0 && f->arity != argc) ergo_trap("arity mismatch");
  return f->fn(f->env, argc, argv);
}

// ---- Cogito GUI (shared library bindings) ----
// ---- Cogito bindings (shared library) ----
#include <cogito.h>

#undef cogito_app_new
#undef cogito_app_free
#undef cogito_app_run
#undef cogito_app_set_appid
#undef cogito_app_set_app_name
#undef cogito_app_set_accent_color
#undef cogito_open_url
#undef cogito_window_new
#undef cogito_window_free
#undef cogito_window_set_resizable
#undef cogito_window_set_autosize
#undef cogito_window_set_a11y_label
#undef cogito_window_set_builder
#undef cogito_rebuild_active_window
#undef cogito_node_new
#undef cogito_grid_new_with_cols
#undef cogito_label_new
#undef cogito_button_new
#undef cogito_iconbtn_new
#undef cogito_checkbox_new
#undef cogito_switch_new
#undef cogito_textfield_new
#undef cogito_textview_new
#undef cogito_searchfield_new
#undef cogito_dropdown_new
#undef cogito_slider_new
#undef cogito_tabs_new
#undef cogito_view_switcher_new
#undef cogito_progress_new
#undef cogito_datepicker_new
#undef cogito_colorpicker_new
#undef cogito_stepper_new
#undef cogito_segmented_new
#undef cogito_treeview_new
#undef cogito_toasts_new
#undef cogito_toast_new
#undef cogito_bottom_toolbar_new
#undef cogito_dialog_new
#undef cogito_dialog_slot_new
#undef cogito_appbar_new
#undef cogito_image_new
#undef cogito_node_add
#undef cogito_node_remove
#undef cogito_node_free
#undef cogito_node_set_margins
#undef cogito_node_set_padding
#undef cogito_node_set_align
#undef cogito_node_set_halign
#undef cogito_node_set_valign
#undef cogito_node_set_id
#undef cogito_node_set_text
#undef cogito_node_get_text
#undef cogito_node_set_disabled
#undef cogito_node_set_editable
#undef cogito_node_get_editable
#undef cogito_node_set_class
#undef cogito_node_set_a11y_label
#undef cogito_node_set_a11y_role
#undef cogito_node_set_tooltip
#undef cogito_node_build
#undef cogito_node_on_click
#undef cogito_node_on_change
#undef cogito_node_on_select
#undef cogito_node_on_activate
#undef cogito_pointer_capture
#undef cogito_pointer_release
#undef cogito_dropdown_set_items
#undef cogito_dropdown_get_selected
#undef cogito_dropdown_set_selected
#undef cogito_tabs_set_items
#undef cogito_tabs_set_ids
#undef cogito_tabs_get_selected
#undef cogito_tabs_set_selected
#undef cogito_tabs_bind
#undef cogito_slider_get_value
#undef cogito_slider_set_value
#undef cogito_checkbox_get_checked
#undef cogito_checkbox_set_checked
#undef cogito_switch_get_checked
#undef cogito_switch_set_checked
#undef cogito_textfield_set_text
#undef cogito_textfield_get_text
#undef cogito_textview_set_text
#undef cogito_textview_get_text
#undef cogito_searchfield_set_text
#undef cogito_searchfield_get_text
#undef cogito_progress_set_value
#undef cogito_progress_get_value
#undef cogito_stepper_set_value
#undef cogito_stepper_get_value
#undef cogito_stepper_on_change
#undef cogito_segmented_on_select
#undef cogito_load_sum_file
#undef cogito_label_set_text
#undef cogito_label_set_wrap
#undef cogito_label_set_ellipsis
#undef cogito_label_set_align
#undef cogito_image_set_icon
#undef cogito_appbar_add_button
#undef cogito_appbar_set_controls
#undef cogito_appbar_set_title
#undef cogito_appbar_set_subtitle
#undef cogito_dialog_slot_show
#undef cogito_dialog_slot_clear
#undef cogito_window_set_dialog
#undef cogito_window_clear_dialog
#undef cogito_fixed_set_pos
#undef cogito_scroller_set_axes
#undef cogito_grid_set_gap
#undef cogito_grid_set_span
#undef cogito_grid_set_align
#undef cogito_button_set_text
#undef cogito_button_add_menu
#undef cogito_iconbtn_add_menu
#undef cogito_checkbox_on_change
#undef cogito_switch_on_change
#undef cogito_textfield_on_change
#undef cogito_textview_on_change
#undef cogito_searchfield_on_change
#undef cogito_dropdown_on_change
#undef cogito_slider_on_change
#undef cogito_tabs_on_change
#undef cogito_datepicker_on_change
#undef cogito_colorpicker_on_change
#undef cogito_list_on_select
#undef cogito_list_on_activate
#undef cogito_grid_on_select
#undef cogito_grid_on_activate
#undef cogito_view_switcher_set_active
#undef cogito_toast_on_click
#undef cogito_toast_set_action
#undef cogito_node_window
#undef cogito_find_parent
#undef cogito_find_children
#undef cogito_carousel_item_set_text
#undef cogito_carousel_item_set_halign
#undef cogito_carousel_item_set_valign

static bool cogito_debug_enabled(void) {
  const char* env = getenv("COGITO_DEBUG");
  return env && env[0] && env[0] != '0';
}

typedef enum {
  COGITO_HANDLE_APP = 1,
  COGITO_HANDLE_WINDOW,
  COGITO_HANDLE_NODE,
  COGITO_HANDLE_STATE
} CogitoHandleKind;

typedef struct CogitoHandle {
  ErgoObj base;
  void* ptr;
  int kind;
  ErgoVal on_click;
  ErgoVal on_change;
  ErgoVal on_select;
  ErgoVal on_activate;
  ErgoVal on_action;
  ErgoVal builder;
} CogitoHandle;

typedef struct CogitoHandleEntry {
  cogito_node* node;
  CogitoHandle* handle;
  struct CogitoHandleEntry* next;
} CogitoHandleEntry;

typedef struct CogitoMenuHandler {
  ErgoVal fn;
} CogitoMenuHandler;

static void __cogito_button_on_click(ErgoVal btnv, ErgoVal handler);

typedef struct CogitoState {
  ErgoObj base;
  ErgoVal value;
} CogitoState;

static CogitoHandleEntry* cogito_handle_entries = NULL;

static CogitoHandle* cogito_handle_lookup(cogito_node* node) {
  for (CogitoHandleEntry* e = cogito_handle_entries; e; e = e->next) {
    if (e->node == node) return e->handle;
  }
  return NULL;
}

static void cogito_handle_register(cogito_node* node, CogitoHandle* handle) {
  CogitoHandleEntry* e = (CogitoHandleEntry*)malloc(sizeof(*e));
  e->node = node;
  e->handle = handle;
  e->next = cogito_handle_entries;
  cogito_handle_entries = e;
}

static void cogito_handle_unregister(cogito_node* node) {
  CogitoHandleEntry** cur = &cogito_handle_entries;
  while (*cur) {
    CogitoHandleEntry* e = *cur;
    if (e->node == node) {
      *cur = e->next;
      free(e);
      return;
    }
    cur = &e->next;
  }
}

static void cogito_handle_drop(ErgoObj* o) {
  CogitoHandle* h = (CogitoHandle*)o;
  if (!h) return;
  if (h->on_click.tag != EVT_NULL) ergo_release_val(h->on_click);
  if (h->on_change.tag != EVT_NULL) ergo_release_val(h->on_change);
  if (h->on_select.tag != EVT_NULL) ergo_release_val(h->on_select);
  if (h->on_activate.tag != EVT_NULL) ergo_release_val(h->on_activate);
  if (h->builder.tag != EVT_NULL) ergo_release_val(h->builder);
  if (h->kind == COGITO_HANDLE_WINDOW || h->kind == COGITO_HANDLE_NODE) {
    cogito_handle_unregister((cogito_node*)h->ptr);
  }
  h->ptr = NULL;
}

static CogitoHandle* cogito_handle_new(void* ptr, int kind) {
  CogitoHandle* h = (CogitoHandle*)ergo_obj_new(sizeof(CogitoHandle), cogito_handle_drop);
  h->ptr = ptr;
  h->kind = kind;
  h->on_click = EV_NULLV;
  h->on_change = EV_NULLV;
  h->on_select = EV_NULLV;
  h->on_activate = EV_NULLV;
  h->builder = EV_NULLV;
  return h;
}

static ErgoVal cogito_wrap_node(cogito_node* node, int kind) {
  if (!node) return EV_NULLV;
  CogitoHandle* h = cogito_handle_lookup(node);
  if (!h) {
    h = cogito_handle_new(node, kind);
    cogito_handle_register(node, h);
  }
  return EV_OBJ(h);
}

static CogitoHandle* cogito_handle_from_val(ErgoVal v, const char* what) {
  if (v.tag != EVT_OBJ) ergo_trap(what);
  return (CogitoHandle*)v.as.p;
}

static cogito_app* cogito_app_from_val(ErgoVal v) {
  CogitoHandle* h = cogito_handle_from_val(v, "cogito.app expects app");
  return (cogito_app*)h->ptr;
}

static cogito_window* cogito_window_from_val(ErgoVal v) {
  CogitoHandle* h = cogito_handle_from_val(v, "cogito.window expects window");
  return (cogito_window*)h->ptr;
}

static cogito_node* cogito_node_from_val(ErgoVal v) {
  CogitoHandle* h = cogito_handle_from_val(v, "cogito.node expects node");
  return (cogito_node*)h->ptr;
}

static const char* cogito_required_cstr(ErgoVal v, ErgoStr** tmp) {
  if (v.tag == EVT_NULL) return "";
  if (v.tag == EVT_STR) return ((ErgoStr*)v.as.p)->data;
  ErgoStr* s = stdr_to_string(v);
  if (tmp) *tmp = s;
  return s ? s->data : "";
}

static const char* cogito_optional_cstr(ErgoVal v, ErgoStr** tmp) {
  if (v.tag == EVT_NULL) return NULL;
  if (v.tag == EVT_STR) return ((ErgoStr*)v.as.p)->data;
  ErgoStr* s = stdr_to_string(v);
  if (tmp) *tmp = s;
  return s ? s->data : NULL;
}

static void cogito_set_handler(CogitoHandle* h, ErgoVal* slot, ErgoVal handler) {
  bool had = slot->tag != EVT_NULL;
  if (had) ergo_release_val(*slot);
  *slot = handler;
  bool has = handler.tag != EVT_NULL;
  if (has) ergo_retain_val(handler);
  if (h) {
    if (!had && has) ergo_retain_val(EV_OBJ(h));
    if (had && !has) ergo_release_val(EV_OBJ(h));
  }
}

static void cogito_invoke_node_handler(ErgoVal handler, cogito_node* node) {
  if (handler.tag != EVT_FN) return;
  ErgoVal arg = cogito_wrap_node(node, COGITO_HANDLE_NODE);
  ergo_retain_val(arg);
  ErgoVal ret = ergo_call(handler, 1, &arg);
  ergo_release_val(arg);
  ergo_release_val(ret);
}

static void cogito_invoke_index_handler(ErgoVal handler, int idx) {
  if (handler.tag != EVT_FN) return;
  ErgoVal arg = EV_INT(idx);
  ErgoVal ret = ergo_call(handler, 1, &arg);
  ergo_release_val(ret);
}

static void cogito_cb_click(cogito_node* node, void* user) {
  CogitoHandle* h = (CogitoHandle*)user;
  if (!h) return;
  cogito_invoke_node_handler(h->on_click, node);
}

static void cogito_cb_change(cogito_node* node, void* user) {
  CogitoHandle* h = (CogitoHandle*)user;
  if (!h) return;
  cogito_invoke_node_handler(h->on_change, node);
}

static void cogito_cb_action(cogito_node* node, void* user) {
  CogitoHandle* h = (CogitoHandle*)user;
  if (!h) return;
  cogito_invoke_node_handler(h->on_action, node);
}

static void cogito_cb_select(cogito_node* node, int idx, void* user) {
  (void)node;
  CogitoHandle* h = (CogitoHandle*)user;
  if (!h) return;
  cogito_invoke_index_handler(h->on_select, idx);
}

static void cogito_cb_activate(cogito_node* node, int idx, void* user) {
  (void)node;
  CogitoHandle* h = (CogitoHandle*)user;
  if (!h) return;
  cogito_invoke_index_handler(h->on_activate, idx);
}

static void cogito_cb_builder(cogito_node* node, void* user) {
  CogitoHandle* h = (CogitoHandle*)user;
  if (!h) return;
  cogito_invoke_node_handler(h->builder, node);
}

static CogitoMenuHandler* cogito_menu_handler_new(ErgoVal handler) {
  CogitoMenuHandler* mh = (CogitoMenuHandler*)calloc(1, sizeof(*mh));
  mh->fn = handler;
  if (handler.tag != EVT_NULL) ergo_retain_val(handler);
  return mh;
}

static void cogito_cb_menu(cogito_node* node, void* user) {
  CogitoMenuHandler* mh = (CogitoMenuHandler*)user;
  if (!mh) return;
  cogito_invoke_node_handler(mh->fn, node);
}

static void cogito_state_drop(ErgoObj* o) {
  CogitoState* s = (CogitoState*)o;
  if (s->value.tag != EVT_NULL) {
    ergo_release_val(s->value);
    s->value = EV_NULLV;
  }
}

static ErgoVal cogito_state_new_val(ErgoVal initial) {
  CogitoState* s = (CogitoState*)ergo_obj_new(sizeof(CogitoState), cogito_state_drop);
  s->value = initial;
  if (initial.tag != EVT_NULL) ergo_retain_val(initial);
  return EV_OBJ(s);
}

static ErgoVal cogito_state_get_val(ErgoVal sv) {
  if (sv.tag != EVT_OBJ) ergo_trap("cogito.state_get expects state");
  CogitoState* s = (CogitoState*)sv.as.p;
  ErgoVal v = s->value;
  if (v.tag != EVT_NULL) ergo_retain_val(v);
  return v;
}

static void cogito_state_set_val(ErgoVal sv, ErgoVal nv) {
  if (sv.tag != EVT_OBJ) ergo_trap("cogito.state_set expects state");
  CogitoState* s = (CogitoState*)sv.as.p;
  if (s->value.tag != EVT_NULL) ergo_release_val(s->value);
  s->value = nv;
  if (nv.tag != EVT_NULL) ergo_retain_val(nv);
  cogito_rebuild_active_window();
}

static ErgoVal __cogito_app(void) {
  cogito_app* app = cogito_app_new();
  CogitoHandle* h = cogito_handle_new(app, COGITO_HANDLE_APP);
  return EV_OBJ(h);
}

static void __cogito_app_set_appid(ErgoVal appv, ErgoVal idv) {
  cogito_app* app = cogito_app_from_val(appv);
  ErgoStr* tmp = NULL;
  const char* id = cogito_optional_cstr(idv, &tmp);
  if (id) cogito_app_set_appid(app, id);
  if (tmp) ergo_release_val(EV_STR(tmp));
}

static void __cogito_app_set_app_name(ErgoVal appv, ErgoVal namev) {
  cogito_app* app = cogito_app_from_val(appv);
  ErgoStr* tmp = NULL;
  const char* name = cogito_optional_cstr(namev, &tmp);
  if (name) cogito_app_set_app_name(app, name);
  if (tmp) ergo_release_val(EV_STR(tmp));
}

static void __cogito_app_set_accent_color(ErgoVal appv, ErgoVal colorv, ErgoVal overridev) {
  cogito_app* app = cogito_app_from_val(appv);
  ErgoStr* tmp = NULL;
  const char* color = cogito_optional_cstr(colorv, &tmp);
  bool ov = overridev.tag == EVT_BOOL ? overridev.as.b : false;
  if (color) cogito_app_set_accent_color(app, color, ov);
  if (tmp) ergo_release_val(EV_STR(tmp));
}

static ErgoVal __cogito_window(ErgoVal titlev, ErgoVal wv, ErgoVal hv) {
  ErgoStr* tmp = NULL;
  const char* title = cogito_required_cstr(titlev, &tmp);
  int w = (int)ergo_as_int(wv);
  int h = (int)ergo_as_int(hv);
  cogito_window* win = cogito_window_new(title, w, h);
  if (tmp) ergo_release_val(EV_STR(tmp));
  return cogito_wrap_node((cogito_node*)win, COGITO_HANDLE_WINDOW);
}

static void __cogito_window_set_resizable(ErgoVal winv, ErgoVal onv) {
  cogito_window* win = cogito_window_from_val(winv);
  cogito_window_set_resizable(win, onv.tag == EVT_BOOL ? onv.as.b : false);
}

static void __cogito_window_set_autosize(ErgoVal winv, ErgoVal onv) {
  cogito_window* win = cogito_window_from_val(winv);
  cogito_window_set_autosize(win, onv.tag == EVT_BOOL ? onv.as.b : false);
}

static void __cogito_window_set_a11y_label(ErgoVal winv, ErgoVal labelv) {
  cogito_window* win = cogito_window_from_val(winv);
  ErgoStr* tmp = NULL;
  const char* label = cogito_optional_cstr(labelv, &tmp);
  if (label) cogito_window_set_a11y_label(win, label);
  if (tmp) ergo_release_val(EV_STR(tmp));
}

static void __cogito_window_set_builder(ErgoVal winv, ErgoVal builder) {
  cogito_window* win = cogito_window_from_val(winv);
  CogitoHandle* h = (CogitoHandle*)winv.as.p;
  if (builder.tag != EVT_FN) {
    cogito_set_handler(h, &h->builder, builder);
    cogito_window_set_builder(win, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->builder, builder);
  cogito_window_set_builder(win, cogito_cb_builder, h);
}

static ErgoVal __cogito_label(ErgoVal textv) {
  ErgoStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_node* n = cogito_label_new(text);
  if (tmp) ergo_release_val(EV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_button(ErgoVal textv) {
  ErgoStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_node* n = cogito_button_new(text);
  if (tmp) ergo_release_val(EV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_iconbtn(ErgoVal textv) {
  ErgoStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_node* n = cogito_iconbtn_new(text);
  if (tmp) ergo_release_val(EV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_fab(ErgoVal iconv) {
  ErgoStr* tmp = NULL;
  const char* icon = cogito_required_cstr(iconv, &tmp);
  cogito_node* n = cogito_fab_new(icon);
  if (tmp) ergo_release_val(EV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static void __cogito_fab_set_extended(ErgoVal fabv, ErgoVal extendedv, ErgoVal labelv) {
  cogito_node* fab = cogito_node_from_val(fabv);
  bool extended = ergo_as_bool(extendedv);
  ErgoStr* tmp = NULL;
  const char* label = cogito_optional_cstr(labelv, &tmp);
  cogito_fab_set_extended(fab, extended, label ? label : "");
  if (tmp) ergo_release_val(EV_STR(tmp));
}

static void __cogito_fab_on_click(ErgoVal fabv, ErgoVal handler) {
  cogito_node* fab = cogito_node_from_val(fabv);
  CogitoHandle* h = (CogitoHandle*)fabv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_click, handler);
    cogito_fab_on_click(fab, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_click, handler);
  cogito_fab_on_click(fab, cogito_cb_click, h);
}

static ErgoVal __cogito_chip(ErgoVal textv) {
  ErgoStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_node* n = cogito_chip_new(text);
  if (tmp) ergo_release_val(EV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static void __cogito_chip_set_selected(ErgoVal chipv, ErgoVal sel) {
  cogito_node* n = cogito_node_from_val(chipv);
  cogito_chip_set_selected(n, ergo_as_bool(sel));
}

static ErgoVal __cogito_chip_get_selected(ErgoVal chipv) {
  cogito_node* n = cogito_node_from_val(chipv);
  return EV_BOOL(cogito_chip_get_selected(n));
}

static void __cogito_chip_set_closable(ErgoVal chipv, ErgoVal closable) {
  cogito_node* n = cogito_node_from_val(chipv);
  cogito_chip_set_closable(n, ergo_as_bool(closable));
}

static void __cogito_chip_on_click(ErgoVal chipv, ErgoVal handler) {
  cogito_node* chip = cogito_node_from_val(chipv);
  CogitoHandle* h = (CogitoHandle*)chipv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_click, handler);
    cogito_chip_on_click(chip, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_click, handler);
  cogito_chip_on_click(chip, cogito_cb_click, h);
}

static void __cogito_chip_on_close(ErgoVal chipv, ErgoVal handler) {
  cogito_node* chip = cogito_node_from_val(chipv);
  CogitoHandle* h = (CogitoHandle*)chipv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_action, handler);
    cogito_chip_on_close(chip, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_action, handler);
  cogito_chip_on_close(chip, cogito_cb_action, h);
}

static ErgoVal __cogito_image(ErgoVal iconv) {
  ErgoStr* tmp = NULL;
  const char* icon = cogito_required_cstr(iconv, &tmp);
  cogito_node* n = cogito_image_new(icon);
  if (tmp) ergo_release_val(EV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static void __cogito_image_set_icon(ErgoVal imgv, ErgoVal iconv) {
  cogito_node* n = cogito_node_from_val(imgv);
  ErgoStr* tmp = NULL;
  const char* icon = cogito_required_cstr(iconv, &tmp);
  cogito_image_set_icon(n, icon);
  if (tmp) ergo_release_val(EV_STR(tmp));
}

static void __cogito_image_set_size(ErgoVal imgv, ErgoVal wv, ErgoVal hv) {
  cogito_node* n = cogito_node_from_val(imgv);
  int w = (int)ergo_as_int(wv);
  int h = (int)ergo_as_int(hv);
  cogito_image_set_size(n, w, h);
}

static void __cogito_image_set_radius(ErgoVal imgv, ErgoVal rv) {
  cogito_node* n = cogito_node_from_val(imgv);
  int r = (int)ergo_as_int(rv);
  cogito_image_set_radius(n, r);
}

static ErgoVal __cogito_appbar(ErgoVal titlev, ErgoVal subtitlev) {
  ErgoStr* ttmp = NULL;
  ErgoStr* stmp = NULL;
  const char* title = cogito_required_cstr(titlev, &ttmp);
  const char* subtitle = cogito_required_cstr(subtitlev, &stmp);
  cogito_node* n = cogito_appbar_new(title, subtitle);
  if (ttmp) ergo_release_val(EV_STR(ttmp));
  if (stmp) ergo_release_val(EV_STR(stmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_appbar_add_button(ErgoVal appbarv, ErgoVal iconv, ErgoVal handler) {
  cogito_node* appbar = cogito_node_from_val(appbarv);
  ErgoStr* tmp = NULL;
  const char* icon = cogito_required_cstr(iconv, &tmp);
  cogito_node* btn = cogito_appbar_add_button(appbar, icon, NULL, NULL);
  if (tmp) ergo_release_val(EV_STR(tmp));
  ErgoVal btnv = cogito_wrap_node(btn, COGITO_HANDLE_NODE);
  if (handler.tag == EVT_FN) __cogito_button_on_click(btnv, handler);
  return btnv;
}

static void __cogito_appbar_set_controls(ErgoVal appbarv, ErgoVal layoutv) {
  cogito_node* appbar = cogito_node_from_val(appbarv);
  ErgoStr* tmp = NULL;
  const char* layout = cogito_optional_cstr(layoutv, &tmp);
  if (layout) cogito_appbar_set_controls(appbar, layout);
  if (tmp) ergo_release_val(EV_STR(tmp));
}

static void __cogito_appbar_set_title(ErgoVal appbarv, ErgoVal titlev) {
  cogito_node* appbar = cogito_node_from_val(appbarv);
  ErgoStr* tmp = NULL;
  const char* title = cogito_optional_cstr(titlev, &tmp);
  cogito_appbar_set_title(appbar, title ? title : "");
  if (tmp) ergo_release_val(EV_STR(tmp));
}

static void __cogito_appbar_set_subtitle(ErgoVal appbarv, ErgoVal subtitlev) {
  cogito_node* appbar = cogito_node_from_val(appbarv);
  ErgoStr* tmp = NULL;
  const char* subtitle = cogito_optional_cstr(subtitlev, &tmp);
  cogito_appbar_set_subtitle(appbar, subtitle ? subtitle : "");
  if (tmp) ergo_release_val(EV_STR(tmp));
}

static ErgoVal __cogito_dialog(ErgoVal titlev) {
  ErgoStr* tmp = NULL;
  const char* title = cogito_required_cstr(titlev, &tmp);
  cogito_node* n = cogito_dialog_new(title);
  if (tmp) ergo_release_val(EV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_dialog_slot(void) {
  cogito_node* n = cogito_dialog_slot_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static void __cogito_dialog_slot_show(ErgoVal slotv, ErgoVal dialogv) {
  cogito_node* slot = cogito_node_from_val(slotv);
  cogito_node* dialog = cogito_node_from_val(dialogv);
  cogito_dialog_slot_show(slot, dialog);
}

static void __cogito_dialog_slot_clear(ErgoVal slotv) {
  cogito_node* slot = cogito_node_from_val(slotv);
  cogito_dialog_slot_clear(slot);
}

static void __cogito_window_set_dialog(ErgoVal winv, ErgoVal dialogv) {
  cogito_window* win = cogito_window_from_val(winv);
  cogito_node* dialog = cogito_node_from_val(dialogv);
  cogito_window_set_dialog(win, dialog);
}

static void __cogito_window_clear_dialog(ErgoVal winv) {
  cogito_window* win = cogito_window_from_val(winv);
  cogito_window_clear_dialog(win);
}

static ErgoVal __cogito_node_window(ErgoVal nodev) {
  cogito_node* n = cogito_node_from_val(nodev);
  cogito_window* win = cogito_node_window(n);
  return cogito_wrap_node((cogito_node*)win, COGITO_HANDLE_WINDOW);
}

static ErgoVal __cogito_find_parent(ErgoVal nodev) {
  cogito_node* n = cogito_node_from_val(nodev);
  cogito_node* p = cogito_node_get_parent(n);
  if (!p) return EV_NULLV;
  return cogito_wrap_node(p, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_find_children(ErgoVal nodev) {
  cogito_node* n = cogito_node_from_val(nodev);
  size_t count = cogito_node_get_child_count(n);
  ErgoArr* arr = stdr_arr_new((int)count);
  for (size_t i = 0; i < count; i++) {
    cogito_node* child = cogito_node_get_child(n, i);
    ergo_arr_add(arr, cogito_wrap_node(child, COGITO_HANDLE_NODE));
  }
  return EV_ARR(arr);
}

static void __cogito_label_set_class(ErgoVal labelv, ErgoVal classv) {
  cogito_node* n = cogito_node_from_val(labelv);
  ErgoStr* tmp = NULL;
  const char* cls = cogito_optional_cstr(classv, &tmp);
  if (cls) cogito_node_set_class(n, cls);
  if (tmp) ergo_release_val(EV_STR(tmp));
}

static void __cogito_label_set_text(ErgoVal labelv, ErgoVal textv) {
  cogito_node* n = cogito_node_from_val(labelv);
  ErgoStr* tmp = NULL;
  const char* text = cogito_optional_cstr(textv, &tmp);
  if (text) cogito_node_set_text(n, text);
  if (tmp) ergo_release_val(EV_STR(tmp));
}

static void __cogito_node_set_class(ErgoVal nodev, ErgoVal classv) {
  cogito_node* n = cogito_node_from_val(nodev);
  ErgoStr* tmp = NULL;
  const char* cls = cogito_optional_cstr(classv, &tmp);
  if (cls) cogito_node_set_class(n, cls);
  if (tmp) ergo_release_val(EV_STR(tmp));
}

static void __cogito_node_set_a11y_label(ErgoVal nodev, ErgoVal labelv) {
  cogito_node* n = cogito_node_from_val(nodev);
  ErgoStr* tmp = NULL;
  const char* label = cogito_optional_cstr(labelv, &tmp);
  if (label) cogito_node_set_a11y_label(n, label);
  if (tmp) ergo_release_val(EV_STR(tmp));
}

static void __cogito_node_set_a11y_role(ErgoVal nodev, ErgoVal rolev) {
  cogito_node* n = cogito_node_from_val(nodev);
  ErgoStr* tmp = NULL;
  const char* role = cogito_optional_cstr(rolev, &tmp);
  if (role) cogito_node_set_a11y_role(n, role);
  if (tmp) ergo_release_val(EV_STR(tmp));
}

static void __cogito_node_set_tooltip(ErgoVal nodev, ErgoVal textv) {
  cogito_node* n = cogito_node_from_val(nodev);
  ErgoStr* tmp = NULL;
  const char* text = cogito_optional_cstr(textv, &tmp);
  if (text) cogito_node_set_tooltip(n, text);
  if (tmp) ergo_release_val(EV_STR(tmp));
}

static void __cogito_pointer_capture(ErgoVal nodev) {
  if (nodev.tag == EVT_NULL) {
    cogito_pointer_release();
    return;
  }
  cogito_node* n = cogito_node_from_val(nodev);
  cogito_pointer_capture(n);
}

static void __cogito_pointer_release(void) {
  cogito_pointer_release();
}

static void __cogito_label_set_wrap(ErgoVal labelv, ErgoVal onv) {
  cogito_node* n = cogito_node_from_val(labelv);
  cogito_label_set_wrap(n, onv.tag == EVT_BOOL ? onv.as.b : false);
}

static void __cogito_label_set_ellipsis(ErgoVal labelv, ErgoVal onv) {
  cogito_node* n = cogito_node_from_val(labelv);
  cogito_label_set_ellipsis(n, onv.tag == EVT_BOOL ? onv.as.b : false);
}

static void __cogito_label_set_align(ErgoVal labelv, ErgoVal alignv) {
  cogito_node* n = cogito_node_from_val(labelv);
  cogito_label_set_align(n, (int)ergo_as_int(alignv));
}

static ErgoVal __cogito_checkbox(ErgoVal textv, ErgoVal groupv) {
  ErgoStr* ttmp = NULL;
  ErgoStr* gtmp = NULL;
  const char* text = cogito_required_cstr(textv, &ttmp);
  const char* group = cogito_optional_cstr(groupv, &gtmp);
  cogito_node* n = cogito_checkbox_new(text, group);
  if (ttmp) ergo_release_val(EV_STR(ttmp));
  if (gtmp) ergo_release_val(EV_STR(gtmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_switch(ErgoVal textv) {
  ErgoStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_node* n = cogito_switch_new(text);
  if (tmp) ergo_release_val(EV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_textfield(ErgoVal textv) {
  ErgoStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_node* n = cogito_textfield_new(text);
  if (tmp) ergo_release_val(EV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_textview(ErgoVal textv) {
  ErgoStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_node* n = cogito_textview_new(text);
  if (tmp) ergo_release_val(EV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_searchfield(ErgoVal textv) {
  ErgoStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_node* n = cogito_searchfield_new(text);
  if (tmp) ergo_release_val(EV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static void __cogito_searchfield_set_text(ErgoVal sfv, ErgoVal textv) {
  cogito_node* sf = cogito_node_from_val(sfv);
  ErgoStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_searchfield_set_text(sf, text);
  if (tmp) ergo_release_val(EV_STR(tmp));
}

static ErgoVal __cogito_searchfield_get_text(ErgoVal sfv) {
  cogito_node* sf = cogito_node_from_val(sfv);
  const char* text = cogito_searchfield_get_text(sf);
  return EV_STR(stdr_str_lit(text ? text : ""));
}

static void __cogito_searchfield_on_change(ErgoVal sfv, ErgoVal handler) {
  cogito_node* sf = cogito_node_from_val(sfv);
  CogitoHandle* h = (CogitoHandle*)sfv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_change, handler);
    cogito_searchfield_on_change(sf, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_change, handler);
  cogito_searchfield_on_change(sf, cogito_cb_change, h);
}

static ErgoVal __cogito_dropdown(void) {
  cogito_node* n = cogito_dropdown_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_datepicker(void) {
  cogito_node* n = cogito_datepicker_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static void __cogito_datepicker_on_change(ErgoVal dpv, ErgoVal handler) {
  cogito_node* dp = cogito_node_from_val(dpv);
  CogitoHandle* h = (CogitoHandle*)dpv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_change, handler);
    cogito_datepicker_on_change(dp, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_change, handler);
  cogito_datepicker_on_change(dp, cogito_cb_change, h);
}

static ErgoVal __cogito_stepper(ErgoVal minv, ErgoVal maxv, ErgoVal valuev, ErgoVal stepv) {
  cogito_node* n = cogito_stepper_new(ergo_as_float(minv), ergo_as_float(maxv), ergo_as_float(valuev), ergo_as_float(stepv));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_slider(ErgoVal minv, ErgoVal maxv, ErgoVal valuev) {
  cogito_node* n = cogito_slider_new(ergo_as_float(minv), ergo_as_float(maxv), ergo_as_float(valuev));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_tabs(void) {
  cogito_node* n = cogito_tabs_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_nav_rail(void) {
  cogito_node* n = cogito_nav_rail_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_segmented(void) {
  cogito_node* n = cogito_segmented_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_view_switcher(void) {
  cogito_node* n = cogito_view_switcher_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_progress(ErgoVal valuev) {
  cogito_node* n = cogito_progress_new(ergo_as_float(valuev));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_treeview(void) {
  cogito_node* n = cogito_treeview_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_colorpicker(void) {
  cogito_node* n = cogito_colorpicker_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static void __cogito_colorpicker_on_change(ErgoVal cpv, ErgoVal handler) {
  cogito_node* cp = cogito_node_from_val(cpv);
  CogitoHandle* h = (CogitoHandle*)cpv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_change, handler);
    cogito_colorpicker_on_change(cp, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_change, handler);
  cogito_colorpicker_on_change(cp, cogito_cb_change, h);
}

static ErgoVal __cogito_toasts(void) {
  cogito_node* n = cogito_toasts_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_toast(ErgoVal textv) {
  ErgoStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_node* n = cogito_toast_new(text);
  if (tmp) ergo_release_val(EV_STR(tmp));
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_toolbar(void) {
  cogito_node* n = cogito_bottom_toolbar_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_vstack(void) {
  return cogito_wrap_node(cogito_node_new(COGITO_NODE_VSTACK), COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_hstack(void) {
  return cogito_wrap_node(cogito_node_new(COGITO_NODE_HSTACK), COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_zstack(void) {
  return cogito_wrap_node(cogito_node_new(COGITO_NODE_ZSTACK), COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_fixed(void) {
  return cogito_wrap_node(cogito_node_new(COGITO_NODE_FIXED), COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_scroller(void) {
  return cogito_wrap_node(cogito_node_new(COGITO_NODE_SCROLLER), COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_carousel(void) {
  return cogito_wrap_node(cogito_node_new(COGITO_NODE_CAROUSEL), COGITO_HANDLE_NODE);
}

static void __cogito_carousel_set_active_index(ErgoVal carouselv, ErgoVal idxv) {
  cogito_node* carousel = cogito_node_from_val(carouselv);
  cogito_carousel_set_active_index(carousel, (int)ergo_as_int(idxv));
}

static ErgoVal __cogito_carousel_get_active_index(ErgoVal carouselv) {
  cogito_node* carousel = cogito_node_from_val(carouselv);
  int idx = cogito_carousel_get_active_index(carousel);
  return EV_INT(idx);
}

static ErgoVal __cogito_carousel_item(void) {
  return cogito_wrap_node(cogito_carousel_item_new(), COGITO_HANDLE_NODE);
}

static void __cogito_carousel_item_set_text(ErgoVal itemv, ErgoVal textv) {
  cogito_node* item = cogito_node_from_val(itemv);
  ErgoStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_carousel_item_set_text(item, text);
  if (tmp) ergo_release_val(EV_STR(tmp));
}

static void __cogito_carousel_item_set_halign(ErgoVal itemv, ErgoVal alignv) {
  cogito_node* item = cogito_node_from_val(itemv);
  cogito_carousel_item_set_halign(item, (int)ergo_as_int(alignv));
}

static void __cogito_carousel_item_set_valign(ErgoVal itemv, ErgoVal alignv) {
  cogito_node* item = cogito_node_from_val(itemv);
  cogito_carousel_item_set_valign(item, (int)ergo_as_int(alignv));
}

static ErgoVal __cogito_list(void) {
  return cogito_wrap_node(cogito_node_new(COGITO_NODE_LIST), COGITO_HANDLE_NODE);
}

static ErgoVal __cogito_grid(ErgoVal cols) {
  return cogito_wrap_node(cogito_grid_new_with_cols((int)ergo_as_int(cols)), COGITO_HANDLE_NODE);
}

static void __cogito_container_add(ErgoVal parentv, ErgoVal childv) {
  cogito_node* parent = cogito_node_from_val(parentv);
  cogito_node* child = cogito_node_from_val(childv);
  cogito_node_add(parent, child);
}

static void __cogito_container_set_margins(ErgoVal nodev, ErgoVal left, ErgoVal top, ErgoVal right, ErgoVal bottom) {
  cogito_node* node = cogito_node_from_val(nodev);
  cogito_node_set_margins(node, (int)ergo_as_int(left), (int)ergo_as_int(top), (int)ergo_as_int(right), (int)ergo_as_int(bottom));
}

static void __cogito_container_set_align(ErgoVal nodev, ErgoVal align) {
  cogito_node* node = cogito_node_from_val(nodev);
  cogito_node_set_align(node, (int)ergo_as_int(align));
}

static void __cogito_container_set_halign(ErgoVal nodev, ErgoVal align) {
  cogito_node* node = cogito_node_from_val(nodev);
  cogito_node_set_halign(node, (int)ergo_as_int(align));
}

static void __cogito_container_set_valign(ErgoVal nodev, ErgoVal align) {
  cogito_node* node = cogito_node_from_val(nodev);
  cogito_node_set_valign(node, (int)ergo_as_int(align));
}

static void __cogito_container_set_hexpand(ErgoVal nodev, ErgoVal expand) {
  cogito_node* node = cogito_node_from_val(nodev);
  cogito_node_set_hexpand(node, ergo_as_bool(expand));
}

static void __cogito_container_set_vexpand(ErgoVal nodev, ErgoVal expand) {
  cogito_node* node = cogito_node_from_val(nodev);
  bool expand_bool = ergo_as_bool(expand);
  cogito_node_set_vexpand(node, expand_bool);
}

static void __cogito_container_set_gap(ErgoVal nodev, ErgoVal gap) {
  cogito_node* node = cogito_node_from_val(nodev);
  cogito_node_set_gap(node, (int)ergo_as_int(gap));
}

static void __cogito_container_set_padding(ErgoVal nodev, ErgoVal left, ErgoVal top, ErgoVal right, ErgoVal bottom) {
  cogito_node* node = cogito_node_from_val(nodev);
  cogito_node_set_padding(node, (int)ergo_as_int(left), (int)ergo_as_int(top), (int)ergo_as_int(right), (int)ergo_as_int(bottom));
}

static void __cogito_fixed_set_pos(ErgoVal fixedv, ErgoVal childv, ErgoVal xv, ErgoVal yv) {
  cogito_node* fixed = cogito_node_from_val(fixedv);
  cogito_node* child = cogito_node_from_val(childv);
  cogito_fixed_set_pos(fixed, child, (int)ergo_as_int(xv), (int)ergo_as_int(yv));
}

static void __cogito_scroller_set_axes(ErgoVal scv, ErgoVal hv, ErgoVal vv) {
  cogito_node* sc = cogito_node_from_val(scv);
  bool h = hv.tag == EVT_BOOL ? hv.as.b : false;
  bool v = vv.tag == EVT_BOOL ? vv.as.b : false;
  cogito_scroller_set_axes(sc, h, v);
}

static void __cogito_grid_set_gap(ErgoVal gridv, ErgoVal xv, ErgoVal yv) {
  cogito_node* grid = cogito_node_from_val(gridv);
  cogito_grid_set_gap(grid, (int)ergo_as_int(xv), (int)ergo_as_int(yv));
}

static void __cogito_grid_set_span(ErgoVal childv, ErgoVal col_span, ErgoVal row_span) {
  cogito_node* child = cogito_node_from_val(childv);
  cogito_grid_set_span(child, (int)ergo_as_int(col_span), (int)ergo_as_int(row_span));
}

static void __cogito_grid_set_align(ErgoVal childv, ErgoVal halign, ErgoVal valign) {
  cogito_node* child = cogito_node_from_val(childv);
  cogito_grid_set_align(child, (int)ergo_as_int(halign), (int)ergo_as_int(valign));
}

static void __cogito_node_set_disabled(ErgoVal nodev, ErgoVal onv) {
  cogito_node* node = cogito_node_from_val(nodev);
  cogito_node_set_disabled(node, onv.tag == EVT_BOOL ? onv.as.b : false);
}

static void __cogito_node_set_editable(ErgoVal nodev, ErgoVal onv) {
  cogito_node* node = cogito_node_from_val(nodev);
  cogito_node_set_editable(node, onv.tag == EVT_BOOL ? onv.as.b : false);
}

static ErgoVal __cogito_node_get_editable(ErgoVal nodev) {
  cogito_node* node = cogito_node_from_val(nodev);
  return EV_BOOL(cogito_node_get_editable(node));
}

static void __cogito_node_set_id(ErgoVal nodev, ErgoVal idv) {
  cogito_node* node = cogito_node_from_val(nodev);
  ErgoStr* tmp = NULL;
  const char* id = cogito_optional_cstr(idv, &tmp);
  if (id) cogito_node_set_id(node, id);
  if (tmp) ergo_release_val(EV_STR(tmp));
}

static void __cogito_button_set_text(ErgoVal btnv, ErgoVal textv) {
  cogito_node* btn = cogito_node_from_val(btnv);
  ErgoStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_button_set_text(btn, text);
  if (tmp) ergo_release_val(EV_STR(tmp));
}

static void __cogito_button_on_click(ErgoVal btnv, ErgoVal handler) {
  cogito_node* btn = cogito_node_from_val(btnv);
  CogitoHandle* h = (CogitoHandle*)btnv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_click, handler);
    cogito_node_on_click(btn, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_click, handler);
  cogito_node_on_click(btn, cogito_cb_click, h);
}

static void __cogito_button_add_menu(ErgoVal btnv, ErgoVal labelv, ErgoVal handler) {
  cogito_node* btn = cogito_node_from_val(btnv);
  ErgoStr* tmp = NULL;
  const char* label = cogito_required_cstr(labelv, &tmp);
  CogitoMenuHandler* mh = handler.tag == EVT_FN ? cogito_menu_handler_new(handler) : NULL;
  cogito_button_add_menu(btn, label, mh ? cogito_cb_menu : NULL, mh);
  if (tmp) ergo_release_val(EV_STR(tmp));
}

static void __cogito_iconbtn_add_menu(ErgoVal btnv, ErgoVal labelv, ErgoVal handler) {
  cogito_node* btn = cogito_node_from_val(btnv);
  ErgoStr* tmp = NULL;
  const char* label = cogito_required_cstr(labelv, &tmp);
  CogitoMenuHandler* mh = handler.tag == EVT_FN ? cogito_menu_handler_new(handler) : NULL;
  cogito_iconbtn_add_menu(btn, label, mh ? cogito_cb_menu : NULL, mh);
  if (tmp) ergo_release_val(EV_STR(tmp));
}

static void __cogito_checkbox_set_checked(ErgoVal cbv, ErgoVal checkedv) {
  cogito_node* cb = cogito_node_from_val(cbv);
  cogito_checkbox_set_checked(cb, checkedv.tag == EVT_BOOL ? checkedv.as.b : false);
}

static ErgoVal __cogito_checkbox_get_checked(ErgoVal cbv) {
  cogito_node* cb = cogito_node_from_val(cbv);
  return EV_BOOL(cogito_checkbox_get_checked(cb));
}

static void __cogito_switch_set_checked(ErgoVal swv, ErgoVal checkedv) {
  cogito_node* sw = cogito_node_from_val(swv);
  cogito_switch_set_checked(sw, checkedv.tag == EVT_BOOL ? checkedv.as.b : false);
}

static ErgoVal __cogito_switch_get_checked(ErgoVal swv) {
  cogito_node* sw = cogito_node_from_val(swv);
  return EV_BOOL(cogito_switch_get_checked(sw));
}

static void __cogito_checkbox_on_change(ErgoVal cbv, ErgoVal handler) {
  cogito_node* cb = cogito_node_from_val(cbv);
  CogitoHandle* h = (CogitoHandle*)cbv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_change, handler);
    cogito_checkbox_on_change(cb, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_change, handler);
  cogito_checkbox_on_change(cb, cogito_cb_change, h);
}

static void __cogito_switch_on_change(ErgoVal swv, ErgoVal handler) {
  cogito_node* sw = cogito_node_from_val(swv);
  CogitoHandle* h = (CogitoHandle*)swv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_change, handler);
    cogito_switch_on_change(sw, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_change, handler);
  cogito_switch_on_change(sw, cogito_cb_change, h);
}

static void __cogito_textfield_set_text(ErgoVal tfv, ErgoVal textv) {
  cogito_node* tf = cogito_node_from_val(tfv);
  ErgoStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_textfield_set_text(tf, text);
  if (tmp) ergo_release_val(EV_STR(tmp));
}

static ErgoVal __cogito_textfield_get_text(ErgoVal tfv) {
  cogito_node* tf = cogito_node_from_val(tfv);
  const char* text = cogito_textfield_get_text(tf);
  return EV_STR(stdr_str_lit(text ? text : ""));
}

static void __cogito_textfield_on_change(ErgoVal tfv, ErgoVal handler) {
  cogito_node* tf = cogito_node_from_val(tfv);
  CogitoHandle* h = (CogitoHandle*)tfv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_change, handler);
    cogito_textfield_on_change(tf, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_change, handler);
  cogito_textfield_on_change(tf, cogito_cb_change, h);
}

static void __cogito_textview_set_text(ErgoVal tvv, ErgoVal textv) {
  cogito_node* tv = cogito_node_from_val(tvv);
  ErgoStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_textview_set_text(tv, text);
  if (tmp) ergo_release_val(EV_STR(tmp));
}

static ErgoVal __cogito_textview_get_text(ErgoVal tvv) {
  cogito_node* tv = cogito_node_from_val(tvv);
  const char* text = cogito_textview_get_text(tv);
  return EV_STR(stdr_str_lit(text ? text : ""));
}

static void __cogito_textview_on_change(ErgoVal tvv, ErgoVal handler) {
  cogito_node* tv = cogito_node_from_val(tvv);
  CogitoHandle* h = (CogitoHandle*)tvv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_change, handler);
    cogito_textview_on_change(tv, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_change, handler);
  cogito_textview_on_change(tv, cogito_cb_change, h);
}

static void __cogito_dropdown_set_items(ErgoVal ddv, ErgoVal itemsv) {
  cogito_node* dd = cogito_node_from_val(ddv);
  if (itemsv.tag != EVT_ARR) ergo_trap("cogito.dropdown_set_items expects array");
  ErgoArr* arr = (ErgoArr*)itemsv.as.p;
  size_t count = arr->len;
  const char** items = (const char**)calloc(count, sizeof(char*));
  ErgoStr** temps = (ErgoStr**)calloc(count, sizeof(ErgoStr*));
  for (size_t i = 0; i < count; i++) {
    items[i] = cogito_required_cstr(arr->items[i], &temps[i]);
  }
  cogito_dropdown_set_items(dd, items, count);
  for (size_t i = 0; i < count; i++) {
    if (temps[i]) ergo_release_val(EV_STR(temps[i]));
  }
  free(temps);
  free(items);
}

static void __cogito_dropdown_set_selected(ErgoVal ddv, ErgoVal idxv) {
  cogito_node* dd = cogito_node_from_val(ddv);
  cogito_dropdown_set_selected(dd, (int)ergo_as_int(idxv));
}

static ErgoVal __cogito_dropdown_get_selected(ErgoVal ddv) {
  cogito_node* dd = cogito_node_from_val(ddv);
  return EV_INT(cogito_dropdown_get_selected(dd));
}

static void __cogito_dropdown_on_change(ErgoVal ddv, ErgoVal handler) {
  cogito_node* dd = cogito_node_from_val(ddv);
  CogitoHandle* h = (CogitoHandle*)ddv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_change, handler);
    cogito_dropdown_on_change(dd, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_change, handler);
  cogito_dropdown_on_change(dd, cogito_cb_change, h);
}

static void __cogito_slider_set_value(ErgoVal slv, ErgoVal valuev) {
  cogito_node* sl = cogito_node_from_val(slv);
  cogito_slider_set_value(sl, ergo_as_float(valuev));
}

static ErgoVal __cogito_slider_get_value(ErgoVal slv) {
  cogito_node* sl = cogito_node_from_val(slv);
  return EV_FLOAT(cogito_slider_get_value(sl));
}

static void __cogito_slider_on_change(ErgoVal slv, ErgoVal handler) {
  cogito_node* sl = cogito_node_from_val(slv);
  CogitoHandle* h = (CogitoHandle*)slv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_change, handler);
    cogito_slider_on_change(sl, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_change, handler);
  cogito_slider_on_change(sl, cogito_cb_change, h);
}

static void __cogito_stepper_set_value(ErgoVal stv, ErgoVal valuev) {
  cogito_node* st = cogito_node_from_val(stv);
  cogito_stepper_set_value(st, ergo_as_float(valuev));
}

static ErgoVal __cogito_stepper_get_value(ErgoVal stv) {
  cogito_node* st = cogito_node_from_val(stv);
  return EV_FLOAT(cogito_stepper_get_value(st));
}

static void __cogito_stepper_on_change(ErgoVal stv, ErgoVal handler) {
  cogito_node* st = cogito_node_from_val(stv);
  CogitoHandle* h = (CogitoHandle*)stv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_change, handler);
    cogito_stepper_on_change(st, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_change, handler);
  cogito_stepper_on_change(st, cogito_cb_change, h);
}

static void __cogito_segmented_on_select(ErgoVal segv, ErgoVal handler) {
  cogito_node* seg = cogito_node_from_val(segv);
  CogitoHandle* h = (CogitoHandle*)segv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_change, handler);
    cogito_segmented_on_select(seg, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_change, handler);
  cogito_segmented_on_select(seg, cogito_cb_change, h);
}

static void __cogito_tabs_set_items(ErgoVal tabsv, ErgoVal itemsv) {
  cogito_node* tabs = cogito_node_from_val(tabsv);
  if (itemsv.tag != EVT_ARR) ergo_trap("cogito.tabs_set_items expects array");
  ErgoArr* arr = (ErgoArr*)itemsv.as.p;
  size_t count = arr->len;
  const char** items = (const char**)calloc(count, sizeof(char*));
  ErgoStr** temps = (ErgoStr**)calloc(count, sizeof(ErgoStr*));
  for (size_t i = 0; i < count; i++) {
    items[i] = cogito_required_cstr(arr->items[i], &temps[i]);
  }
  cogito_tabs_set_items(tabs, items, count);
  for (size_t i = 0; i < count; i++) {
    if (temps[i]) ergo_release_val(EV_STR(temps[i]));
  }
  free(temps);
  free(items);
}

static void __cogito_tabs_set_ids(ErgoVal tabsv, ErgoVal itemsv) {
  cogito_node* tabs = cogito_node_from_val(tabsv);
  if (itemsv.tag != EVT_ARR) ergo_trap("cogito.tabs_set_ids expects array");
  ErgoArr* arr = (ErgoArr*)itemsv.as.p;
  size_t count = arr->len;
  const char** items = (const char**)calloc(count, sizeof(char*));
  ErgoStr** temps = (ErgoStr**)calloc(count, sizeof(ErgoStr*));
  for (size_t i = 0; i < count; i++) {
    items[i] = cogito_required_cstr(arr->items[i], &temps[i]);
  }
  cogito_tabs_set_ids(tabs, items, count);
  for (size_t i = 0; i < count; i++) {
    if (temps[i]) ergo_release_val(EV_STR(temps[i]));
  }
  free(temps);
  free(items);
}

static void __cogito_tabs_set_selected(ErgoVal tabsv, ErgoVal idxv) {
  cogito_node* tabs = cogito_node_from_val(tabsv);
  cogito_tabs_set_selected(tabs, (int)ergo_as_int(idxv));
}

static ErgoVal __cogito_tabs_get_selected(ErgoVal tabsv) {
  cogito_node* tabs = cogito_node_from_val(tabsv);
  return EV_INT(cogito_tabs_get_selected(tabs));
}

static void __cogito_tabs_on_change(ErgoVal tabsv, ErgoVal handler) {
  cogito_node* tabs = cogito_node_from_val(tabsv);
  CogitoHandle* h = (CogitoHandle*)tabsv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_change, handler);
    cogito_tabs_on_change(tabs, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_change, handler);
  cogito_tabs_on_change(tabs, cogito_cb_change, h);
}

static void __cogito_tabs_bind(ErgoVal tabsv, ErgoVal viewv) {
  cogito_node* tabs = cogito_node_from_val(tabsv);
  cogito_node* view = cogito_node_from_val(viewv);
  cogito_tabs_bind(tabs, view);
}

static void __cogito_nav_rail_set_items(ErgoVal railv, ErgoVal labelsv, ErgoVal iconsv) {
  cogito_node* rail = cogito_node_from_val(railv);
  if (labelsv.tag != EVT_ARR) ergo_trap("cogito.nav_rail_set_items expects array of labels");
  ErgoArr* labels = (ErgoArr*)labelsv.as.p;
  size_t count = labels->len;

  const char** label_strs = (const char**)calloc(count, sizeof(char*));
  ErgoStr** label_temps = (ErgoStr**)calloc(count, sizeof(ErgoStr*));
  for (size_t i = 0; i < count; i++) {
    label_strs[i] = cogito_required_cstr(labels->items[i], &label_temps[i]);
  }

  const char** icon_strs = NULL;
  ErgoStr** icon_temps = NULL;
  if (iconsv.tag == EVT_ARR) {
    ErgoArr* icons = (ErgoArr*)iconsv.as.p;
    size_t icon_count = icons->len;
    icon_strs = (const char**)calloc(icon_count, sizeof(char*));
    icon_temps = (ErgoStr**)calloc(icon_count, sizeof(ErgoStr*));
    for (size_t i = 0; i < icon_count; i++) {
      icon_strs[i] = cogito_required_cstr(icons->items[i], &icon_temps[i]);
    }
    cogito_nav_rail_set_items(rail, label_strs, icon_strs, icon_count < count ? icon_count : count);
    for (size_t i = 0; i < icon_count; i++) {
      if (icon_temps[i]) ergo_release_val(EV_STR(icon_temps[i]));
    }
    free(icon_temps);
    free(icon_strs);
  } else {
    cogito_nav_rail_set_items(rail, label_strs, NULL, count);
  }

  for (size_t i = 0; i < count; i++) {
    if (label_temps[i]) ergo_release_val(EV_STR(label_temps[i]));
  }
  free(label_temps);
  free(label_strs);
}

static void __cogito_nav_rail_set_selected(ErgoVal railv, ErgoVal idxv) {
  cogito_node* rail = cogito_node_from_val(railv);
  cogito_nav_rail_set_selected(rail, (int)ergo_as_int(idxv));
}

static ErgoVal __cogito_nav_rail_get_selected(ErgoVal railv) {
  cogito_node* rail = cogito_node_from_val(railv);
  return EV_INT(cogito_nav_rail_get_selected(rail));
}

static ErgoVal __cogito_bottom_nav(void) {
  cogito_node* n = cogito_bottom_nav_new();
  return cogito_wrap_node(n, COGITO_HANDLE_NODE);
}

static void __cogito_nav_rail_on_change(ErgoVal railv, ErgoVal handler) {
  cogito_node* rail = cogito_node_from_val(railv);
  CogitoHandle* h = (CogitoHandle*)railv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_select, handler);
    cogito_nav_rail_on_change(rail, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_select, handler);
  cogito_nav_rail_on_change(rail, cogito_cb_select, h);
}

static void __cogito_bottom_nav_set_items(ErgoVal navv, ErgoVal labelsv, ErgoVal iconsv) {
  cogito_node* nav = cogito_node_from_val(navv);
  if (labelsv.tag != EVT_ARR) ergo_trap("cogito.bottom_nav_set_items expects array of labels");
  ErgoArr* labels = (ErgoArr*)labelsv.as.p;
  size_t count = labels->len;
  const char** label_strs = (const char**)calloc(count, sizeof(char*));
  ErgoStr** label_temps = (ErgoStr**)calloc(count, sizeof(ErgoStr*));
  for (size_t i = 0; i < count; i++) {
    label_strs[i] = cogito_required_cstr(labels->items[i], &label_temps[i]);
  }
  const char** icon_strs = NULL;
  ErgoStr** icon_temps = NULL;
  if (iconsv.tag == EVT_ARR) {
    ErgoArr* icons = (ErgoArr*)iconsv.as.p;
    size_t icon_count = icons->len;
    icon_strs = (const char**)calloc(icon_count, sizeof(char*));
    icon_temps = (ErgoStr**)calloc(icon_count, sizeof(ErgoStr*));
    for (size_t i = 0; i < icon_count; i++) {
      icon_strs[i] = cogito_required_cstr(icons->items[i], &icon_temps[i]);
    }
    cogito_bottom_nav_set_items(nav, label_strs, icon_strs, icon_count < count ? icon_count : count);
    for (size_t i = 0; i < icon_count; i++) {
      if (icon_temps[i]) ergo_release_val(EV_STR(icon_temps[i]));
    }
    free(icon_temps);
    free(icon_strs);
  } else {
    cogito_bottom_nav_set_items(nav, label_strs, NULL, count);
  }
  for (size_t i = 0; i < count; i++) {
    if (label_temps[i]) ergo_release_val(EV_STR(label_temps[i]));
  }
  free(label_temps);
  free(label_strs);
}

static void __cogito_bottom_nav_set_selected(ErgoVal navv, ErgoVal idxv) {
  cogito_node* nav = cogito_node_from_val(navv);
  cogito_bottom_nav_set_selected(nav, (int)ergo_as_int(idxv));
}

static ErgoVal __cogito_bottom_nav_get_selected(ErgoVal navv) {
  cogito_node* nav = cogito_node_from_val(navv);
  return EV_INT(cogito_bottom_nav_get_selected(nav));
}

static void __cogito_bottom_nav_on_change(ErgoVal navv, ErgoVal handler) {
  cogito_node* nav = cogito_node_from_val(navv);
  CogitoHandle* h = (CogitoHandle*)navv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_select, handler);
    cogito_bottom_nav_on_change(nav, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_select, handler);
  cogito_bottom_nav_on_change(nav, cogito_cb_select, h);
}

static void __cogito_view_switcher_set_active(ErgoVal viewv, ErgoVal idv) {
  cogito_node* view = cogito_node_from_val(viewv);
  ErgoStr* tmp = NULL;
  const char* id = cogito_required_cstr(idv, &tmp);
  cogito_view_switcher_set_active(view, id);
  if (tmp) ergo_release_val(EV_STR(tmp));
}

static void __cogito_progress_set_value(ErgoVal pv, ErgoVal valuev) {
  cogito_node* p = cogito_node_from_val(pv);
  cogito_progress_set_value(p, ergo_as_float(valuev));
}

static ErgoVal __cogito_progress_get_value(ErgoVal pv) {
  cogito_node* p = cogito_node_from_val(pv);
  return EV_FLOAT(cogito_progress_get_value(p));
}

static void __cogito_toast_set_text(ErgoVal tv, ErgoVal textv) {
  cogito_node* t = cogito_node_from_val(tv);
  ErgoStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  cogito_toast_set_text(t, text);
  if (tmp) ergo_release_val(EV_STR(tmp));
}

static void __cogito_toast_on_click(ErgoVal tv, ErgoVal handler) {
  cogito_node* t = cogito_node_from_val(tv);
  CogitoHandle* h = (CogitoHandle*)tv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_click, handler);
    cogito_toast_on_click(t, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_click, handler);
  cogito_toast_on_click(t, cogito_cb_click, h);
}

static void __cogito_toast_set_action(ErgoVal tv, ErgoVal textv, ErgoVal handler) {
  cogito_node* t = cogito_node_from_val(tv);
  CogitoHandle* h = (CogitoHandle*)tv.as.p;
  ErgoStr* tmp = NULL;
  const char* text = cogito_required_cstr(textv, &tmp);
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_action, handler);
    cogito_toast_set_action(t, text, NULL, NULL);
    if (tmp) ergo_release_val(EV_STR(tmp));
    return;
  }
  cogito_set_handler(h, &h->on_action, handler);
  cogito_toast_set_action(t, text, cogito_cb_action, h);
  if (tmp) ergo_release_val(EV_STR(tmp));
}

static void __cogito_list_on_select(ErgoVal listv, ErgoVal handler) {
  cogito_node* list = cogito_node_from_val(listv);
  CogitoHandle* h = (CogitoHandle*)listv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_select, handler);
    cogito_list_on_select(list, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_select, handler);
  cogito_list_on_select(list, cogito_cb_select, h);
}

static void __cogito_list_on_activate(ErgoVal listv, ErgoVal handler) {
  cogito_node* list = cogito_node_from_val(listv);
  CogitoHandle* h = (CogitoHandle*)listv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_activate, handler);
    cogito_list_on_activate(list, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_activate, handler);
  cogito_list_on_activate(list, cogito_cb_activate, h);
}

static void __cogito_grid_on_select(ErgoVal gridv, ErgoVal handler) {
  cogito_node* grid = cogito_node_from_val(gridv);
  CogitoHandle* h = (CogitoHandle*)gridv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_select, handler);
    cogito_grid_on_select(grid, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_select, handler);
  cogito_grid_on_select(grid, cogito_cb_select, h);
}

static void __cogito_grid_on_activate(ErgoVal gridv, ErgoVal handler) {
  cogito_node* grid = cogito_node_from_val(gridv);
  CogitoHandle* h = (CogitoHandle*)gridv.as.p;
  if (handler.tag != EVT_FN) {
    cogito_set_handler(h, &h->on_activate, handler);
    cogito_grid_on_activate(grid, NULL, NULL);
    return;
  }
  cogito_set_handler(h, &h->on_activate, handler);
  cogito_grid_on_activate(grid, cogito_cb_activate, h);
}

static void __cogito_build(ErgoVal nodev, ErgoVal builder) {
  if (builder.tag != EVT_FN) ergo_trap("cogito.build expects function");
  ErgoVal arg = nodev;
  ergo_retain_val(arg);
  ErgoVal ret = ergo_call(builder, 1, &arg);
  ergo_release_val(arg);
  ergo_release_val(ret);
}

static ErgoVal __cogito_state_new(ErgoVal initial) { return cogito_state_new_val(initial); }
static ErgoVal __cogito_state_get(ErgoVal state) { return cogito_state_get_val(state); }
static void __cogito_state_set(ErgoVal state, ErgoVal value) { cogito_state_set_val(state, value); }

static void __cogito_run(ErgoVal appv, ErgoVal winv) {
  cogito_app* app = cogito_app_from_val(appv);
  cogito_window* win = cogito_window_from_val(winv);
  cogito_app_run(app, win);
}

static void __cogito_load_sum(ErgoVal pathv) {
  ErgoStr* tmp = NULL;
  const char* path = cogito_required_cstr(pathv, &tmp);
  cogito_load_sum_file(path);
  if (tmp) ergo_release_val(EV_STR(tmp));
}

static void __cogito_set_script_dir(ErgoVal dirv) {
  ErgoStr* tmp = NULL;
  const char* dir = cogito_optional_cstr(dirv, &tmp);
  if (dir && dir[0]) cogito_set_script_dir(dir);  // Call the actual cogito library function
  if (tmp) ergo_release_val(EV_STR(tmp));
}

static ErgoVal __cogito_open_url(ErgoVal urlv) {
  ErgoStr* tmp = NULL;
  const char* url = cogito_optional_cstr(urlv, &tmp);
  bool ok = false;
  if (url && url[0]) ok = cogito_open_url(url);
  if (tmp) ergo_release_val(EV_STR(tmp));
  return EV_BOOL(ok);
}

// ---- Codegen aliases ----
#define cogito_app_new __cogito_app
#define cogito_app_set_appid __cogito_app_set_appid
#define cogito_app_set_app_name __cogito_app_set_app_name
#define cogito_app_set_accent_color __cogito_app_set_accent_color
#define cogito_open_url __cogito_open_url
#define cogito_window_new __cogito_window
#define cogito_window_set_resizable __cogito_window_set_resizable
#define cogito_window_set_autosize __cogito_window_set_autosize
#define cogito_window_set_a11y_label __cogito_window_set_a11y_label
#define cogito_window_set_builder __cogito_window_set_builder
#define cogito_button_new __cogito_button
#define cogito_iconbtn_new __cogito_iconbtn
#define cogito_fab_new __cogito_fab
#define cogito_label_new __cogito_label
#define cogito_dialog_new __cogito_dialog
#define cogito_dialog_slot_new __cogito_dialog_slot
#define cogito_image_new __cogito_image
#define cogito_checkbox_new __cogito_checkbox
#define cogito_switch_new __cogito_switch
#define cogito_textfield_new __cogito_textfield
#define cogito_searchfield_new __cogito_searchfield
#define cogito_textview_new __cogito_textview
#define cogito_dropdown_new __cogito_dropdown
#define cogito_datepicker_new __cogito_datepicker
#define cogito_stepper_new __cogito_stepper
#define cogito_slider_new __cogito_slider
#define cogito_tabs_new __cogito_tabs
#define cogito_nav_rail_new __cogito_nav_rail
#define cogito_bottom_nav_new __cogito_bottom_nav
#define cogito_segmented_new __cogito_segmented
#define cogito_view_switcher_new __cogito_view_switcher
#define cogito_progress_new __cogito_progress
#define cogito_treeview_new __cogito_treeview
#define cogito_colorpicker_new __cogito_colorpicker
#define cogito_toasts_new __cogito_toasts
#define cogito_toast_new __cogito_toast
#define cogito_appbar_new __cogito_appbar
#define cogito_toolbar_new __cogito_toolbar
#define cogito_vstack_new __cogito_vstack
#define cogito_hstack_new __cogito_hstack
#define cogito_zstack_new __cogito_zstack
#define cogito_fixed_new __cogito_fixed
#define cogito_scroller_new __cogito_scroller
#define cogito_carousel_new __cogito_carousel
#define cogito_carousel_item_new __cogito_carousel_item
#define cogito_carousel_item_set_text __cogito_carousel_item_set_text
#define cogito_carousel_item_set_halign __cogito_carousel_item_set_halign
#define cogito_carousel_item_set_valign __cogito_carousel_item_set_valign
#define cogito_carousel_set_active_index __cogito_carousel_set_active_index
#define cogito_carousel_get_active_index __cogito_carousel_get_active_index
#define cogito_list_new __cogito_list
#define cogito_grid_new __cogito_grid
#define cogito_container_add __cogito_container_add
#define cogito_container_set_margins __cogito_container_set_margins
#define cogito_container_set_align __cogito_container_set_align
#define cogito_container_set_halign __cogito_container_set_halign
#define cogito_container_set_valign __cogito_container_set_valign
#define cogito_container_set_hexpand __cogito_container_set_hexpand
#define cogito_container_set_vexpand __cogito_container_set_vexpand
#define cogito_container_set_gap __cogito_container_set_gap
#define cogito_dialog_slot_show __cogito_dialog_slot_show
#define cogito_dialog_slot_clear __cogito_dialog_slot_clear
#define cogito_container_set_padding __cogito_container_set_padding
#define cogito_fixed_set_pos __cogito_fixed_set_pos
#define cogito_scroller_set_axes __cogito_scroller_set_axes
#define cogito_grid_set_gap __cogito_grid_set_gap
#define cogito_grid_set_span __cogito_grid_set_span
#define cogito_grid_set_align __cogito_grid_set_align
#define cogito_label_set_class __cogito_label_set_class
#define cogito_label_set_text __cogito_label_set_text
#define cogito_label_set_wrap __cogito_label_set_wrap
#define cogito_label_set_ellipsis __cogito_label_set_ellipsis
#define cogito_label_set_align __cogito_label_set_align
#define cogito_node_set_disabled __cogito_node_set_disabled
#define cogito_node_set_editable __cogito_node_set_editable
#define cogito_node_get_editable __cogito_node_get_editable
#define cogito_node_set_id __cogito_node_set_id
#define cogito_node_set_class __cogito_node_set_class
#define cogito_node_set_a11y_label __cogito_node_set_a11y_label
#define cogito_node_set_a11y_role __cogito_node_set_a11y_role
#define cogito_node_set_tooltip_val __cogito_node_set_tooltip
#define cogito_app_set_appid __cogito_app_set_appid
#define cogito_app_set_app_name __cogito_app_set_app_name
#define cogito_app_set_accent_color __cogito_app_set_accent_color
#define cogito_pointer_capture_set __cogito_pointer_capture
#define cogito_pointer_capture_clear __cogito_pointer_release
#define cogito_view_switcher_set_active __cogito_view_switcher_set_active
#define cogito_textfield_set_text __cogito_textfield_set_text
#define cogito_textfield_get_text __cogito_textfield_get_text
#define cogito_searchfield_set_text __cogito_searchfield_set_text
#define cogito_searchfield_get_text __cogito_searchfield_get_text
#define cogito_searchfield_on_change __cogito_searchfield_on_change
#define cogito_textfield_on_change __cogito_textfield_on_change
#define cogito_textview_set_text __cogito_textview_set_text
#define cogito_textview_get_text __cogito_textview_get_text
#define cogito_textview_on_change __cogito_textview_on_change
#define cogito_datepicker_on_change __cogito_datepicker_on_change
#define cogito_dropdown_set_items __cogito_dropdown_set_items
#define cogito_dropdown_set_selected __cogito_dropdown_set_selected
#define cogito_dropdown_get_selected __cogito_dropdown_get_selected
#define cogito_dropdown_on_change __cogito_dropdown_on_change
#define cogito_slider_set_value __cogito_slider_set_value
#define cogito_slider_get_value __cogito_slider_get_value
#define cogito_slider_on_change __cogito_slider_on_change
#define cogito_colorpicker_on_change __cogito_colorpicker_on_change
#define cogito_tabs_set_items __cogito_tabs_set_items
#define cogito_tabs_set_ids __cogito_tabs_set_ids
#define cogito_tabs_set_selected __cogito_tabs_set_selected
#define cogito_tabs_get_selected __cogito_tabs_get_selected
#define cogito_tabs_on_change __cogito_tabs_on_change
#define cogito_tabs_bind __cogito_tabs_bind
#define cogito_nav_rail_set_items __cogito_nav_rail_set_items
#define cogito_nav_rail_set_selected __cogito_nav_rail_set_selected
#define cogito_nav_rail_get_selected __cogito_nav_rail_get_selected
#define cogito_nav_rail_on_change __cogito_nav_rail_on_change
#define cogito_bottom_nav_set_items __cogito_bottom_nav_set_items
#define cogito_bottom_nav_set_selected __cogito_bottom_nav_set_selected
#define cogito_bottom_nav_get_selected __cogito_bottom_nav_get_selected
#define cogito_bottom_nav_on_change __cogito_bottom_nav_on_change
#define cogito_progress_set_value __cogito_progress_set_value
#define cogito_progress_get_value __cogito_progress_get_value
#define cogito_toast_set_text __cogito_toast_set_text
#define cogito_toast_on_click __cogito_toast_on_click
#define cogito_toast_set_action __cogito_toast_set_action
#define cogito_window_set_autosize __cogito_window_set_autosize
#define cogito_window_set_resizable __cogito_window_set_resizable
#define cogito_window_set_dialog __cogito_window_set_dialog
#define cogito_window_clear_dialog __cogito_window_clear_dialog
#define cogito_node_window_val __cogito_node_window
#define cogito_find_parent __cogito_find_parent
#define cogito_find_children __cogito_find_children
#define cogito_build __cogito_build
#define cogito_window_set_builder __cogito_window_set_builder
#define cogito_state_new __cogito_state_new
#define cogito_state_get __cogito_state_get
#define cogito_state_set __cogito_state_set
#define cogito_button_set_text __cogito_button_set_text
#define cogito_image_set_icon __cogito_image_set_icon
#define cogito_image_set_size __cogito_image_set_size
#define cogito_image_set_radius __cogito_image_set_radius
#define cogito_checkbox_set_checked __cogito_checkbox_set_checked
#define cogito_checkbox_get_checked __cogito_checkbox_get_checked
#define cogito_switch_set_checked __cogito_switch_set_checked
#define cogito_switch_get_checked __cogito_switch_get_checked
#define cogito_checkbox_on_change __cogito_checkbox_on_change
#define cogito_switch_on_change __cogito_switch_on_change
#define cogito_list_on_select __cogito_list_on_select
#define cogito_list_on_activate __cogito_list_on_activate
#define cogito_grid_on_select __cogito_grid_on_select
#define cogito_grid_on_activate __cogito_grid_on_activate
// Forward declare FFI functions with ErgoVal signature
void __cogito_button_on_click(ErgoVal btn, ErgoVal handler);
#define cogito_button_on_click __cogito_button_on_click
#define cogito_button_add_menu __cogito_button_add_menu
#define cogito_fab_set_extended __cogito_fab_set_extended
#define cogito_fab_on_click __cogito_fab_on_click
#define cogito_chip_new __cogito_chip
#define cogito_chip_set_selected __cogito_chip_set_selected
#define cogito_chip_get_selected __cogito_chip_get_selected
#define cogito_chip_set_closable __cogito_chip_set_closable
#define cogito_chip_on_click __cogito_chip_on_click
#define cogito_chip_on_close __cogito_chip_on_close
#define cogito_appbar_add_button __cogito_appbar_add_button
#define cogito_appbar_set_controls __cogito_appbar_set_controls
#define cogito_appbar_set_title __cogito_appbar_set_title
#define cogito_appbar_set_subtitle __cogito_appbar_set_subtitle
#define cogito_iconbtn_add_menu __cogito_iconbtn_add_menu
#define cogito_run __cogito_run
#define cogito_load_sum __cogito_load_sum
#define cogito_set_script_dir __cogito_set_script_dir
// cogito_set_script_dir is provided by the cogito library

// ---- cask globals ----
static ErgoVal ergo_g_main_display_expression = EV_NULLV;
static ErgoVal ergo_g_main_display_working = EV_NULLV;
static ErgoVal ergo_g_main_current_value = EV_NULLV;
static ErgoVal ergo_g_main_stored_value = EV_NULLV;
static ErgoVal ergo_g_main_pending_op = EV_NULLV;
static ErgoVal ergo_g_main_reset_input = EV_NULLV;
static ErgoVal ergo_g_main_has_error = EV_NULLV;
static ErgoVal ergo_g_main_showing_converter = EV_NULLV;
static ErgoVal ergo_g_main_conv_input = EV_NULLV;
static ErgoVal ergo_g_main_conv_output = EV_NULLV;
static ErgoVal ergo_g_main_conv_from_idx = EV_NULLV;
static ErgoVal ergo_g_main_conv_to_idx = EV_NULLV;
static ErgoVal ergo_g_main_conv_from_dd = EV_NULLV;
static ErgoVal ergo_g_main_conv_to_dd = EV_NULLV;
static ErgoVal ergo_g_main_conv_category = EV_NULLV;
static ErgoVal ergo_g_main_ABOUT_MORE_INFO_URL = EV_NULLV;
static ErgoVal ergo_g_main_ABOUT_REPORT_BUG_URL = EV_NULLV;

// ---- class definitions ----
typedef struct ErgoObj_cogito_App {
  ErgoObj base;
} ErgoObj_cogito_App;
static void ergo_drop_cogito_App(ErgoObj* o);

typedef struct ErgoObj_cogito_Window {
  ErgoObj base;
} ErgoObj_cogito_Window;
static void ergo_drop_cogito_Window(ErgoObj* o);

typedef struct ErgoObj_cogito_AppBar {
  ErgoObj base;
} ErgoObj_cogito_AppBar;
static void ergo_drop_cogito_AppBar(ErgoObj* o);

typedef struct ErgoObj_cogito_Image {
  ErgoObj base;
} ErgoObj_cogito_Image;
static void ergo_drop_cogito_Image(ErgoObj* o);

typedef struct ErgoObj_cogito_Dialog {
  ErgoObj base;
} ErgoObj_cogito_Dialog;
static void ergo_drop_cogito_Dialog(ErgoObj* o);

typedef struct ErgoObj_cogito_DialogSlot {
  ErgoObj base;
} ErgoObj_cogito_DialogSlot;
static void ergo_drop_cogito_DialogSlot(ErgoObj* o);

typedef struct ErgoObj_cogito_VStack {
  ErgoObj base;
} ErgoObj_cogito_VStack;
static void ergo_drop_cogito_VStack(ErgoObj* o);

typedef struct ErgoObj_cogito_HStack {
  ErgoObj base;
} ErgoObj_cogito_HStack;
static void ergo_drop_cogito_HStack(ErgoObj* o);

typedef struct ErgoObj_cogito_ZStack {
  ErgoObj base;
} ErgoObj_cogito_ZStack;
static void ergo_drop_cogito_ZStack(ErgoObj* o);

typedef struct ErgoObj_cogito_Fixed {
  ErgoObj base;
} ErgoObj_cogito_Fixed;
static void ergo_drop_cogito_Fixed(ErgoObj* o);

typedef struct ErgoObj_cogito_Scroller {
  ErgoObj base;
} ErgoObj_cogito_Scroller;
static void ergo_drop_cogito_Scroller(ErgoObj* o);

typedef struct ErgoObj_cogito_Carousel {
  ErgoObj base;
} ErgoObj_cogito_Carousel;
static void ergo_drop_cogito_Carousel(ErgoObj* o);

typedef struct ErgoObj_cogito_CarouselItem {
  ErgoObj base;
} ErgoObj_cogito_CarouselItem;
static void ergo_drop_cogito_CarouselItem(ErgoObj* o);

typedef struct ErgoObj_cogito_List {
  ErgoObj base;
} ErgoObj_cogito_List;
static void ergo_drop_cogito_List(ErgoObj* o);

typedef struct ErgoObj_cogito_Grid {
  ErgoObj base;
} ErgoObj_cogito_Grid;
static void ergo_drop_cogito_Grid(ErgoObj* o);

typedef struct ErgoObj_cogito_Label {
  ErgoObj base;
} ErgoObj_cogito_Label;
static void ergo_drop_cogito_Label(ErgoObj* o);

typedef struct ErgoObj_cogito_Button {
  ErgoObj base;
} ErgoObj_cogito_Button;
static void ergo_drop_cogito_Button(ErgoObj* o);

typedef struct ErgoObj_cogito_Checkbox {
  ErgoObj base;
} ErgoObj_cogito_Checkbox;
static void ergo_drop_cogito_Checkbox(ErgoObj* o);

typedef struct ErgoObj_cogito_Switch {
  ErgoObj base;
} ErgoObj_cogito_Switch;
static void ergo_drop_cogito_Switch(ErgoObj* o);

typedef struct ErgoObj_cogito_SearchField {
  ErgoObj base;
} ErgoObj_cogito_SearchField;
static void ergo_drop_cogito_SearchField(ErgoObj* o);

typedef struct ErgoObj_cogito_TextField {
  ErgoObj base;
} ErgoObj_cogito_TextField;
static void ergo_drop_cogito_TextField(ErgoObj* o);

typedef struct ErgoObj_cogito_TextView {
  ErgoObj base;
} ErgoObj_cogito_TextView;
static void ergo_drop_cogito_TextView(ErgoObj* o);

typedef struct ErgoObj_cogito_DatePicker {
  ErgoObj base;
} ErgoObj_cogito_DatePicker;
static void ergo_drop_cogito_DatePicker(ErgoObj* o);

typedef struct ErgoObj_cogito_Stepper {
  ErgoObj base;
} ErgoObj_cogito_Stepper;
static void ergo_drop_cogito_Stepper(ErgoObj* o);

typedef struct ErgoObj_cogito_Dropdown {
  ErgoObj base;
} ErgoObj_cogito_Dropdown;
static void ergo_drop_cogito_Dropdown(ErgoObj* o);

typedef struct ErgoObj_cogito_Slider {
  ErgoObj base;
} ErgoObj_cogito_Slider;
static void ergo_drop_cogito_Slider(ErgoObj* o);

typedef struct ErgoObj_cogito_Tabs {
  ErgoObj base;
} ErgoObj_cogito_Tabs;
static void ergo_drop_cogito_Tabs(ErgoObj* o);

typedef struct ErgoObj_cogito_SegmentedControl {
  ErgoObj base;
} ErgoObj_cogito_SegmentedControl;
static void ergo_drop_cogito_SegmentedControl(ErgoObj* o);

typedef struct ErgoObj_cogito_ViewSwitcher {
  ErgoObj base;
} ErgoObj_cogito_ViewSwitcher;
static void ergo_drop_cogito_ViewSwitcher(ErgoObj* o);

typedef struct ErgoObj_cogito_Progress {
  ErgoObj base;
} ErgoObj_cogito_Progress;
static void ergo_drop_cogito_Progress(ErgoObj* o);

typedef struct ErgoObj_cogito_Divider {
  ErgoObj base;
} ErgoObj_cogito_Divider;
static void ergo_drop_cogito_Divider(ErgoObj* o);

typedef struct ErgoObj_cogito_TreeView {
  ErgoObj base;
} ErgoObj_cogito_TreeView;
static void ergo_drop_cogito_TreeView(ErgoObj* o);

typedef struct ErgoObj_cogito_ColorPicker {
  ErgoObj base;
} ErgoObj_cogito_ColorPicker;
static void ergo_drop_cogito_ColorPicker(ErgoObj* o);

typedef struct ErgoObj_cogito_Toasts {
  ErgoObj base;
} ErgoObj_cogito_Toasts;
static void ergo_drop_cogito_Toasts(ErgoObj* o);

typedef struct ErgoObj_cogito_Toast {
  ErgoObj base;
} ErgoObj_cogito_Toast;
static void ergo_drop_cogito_Toast(ErgoObj* o);

typedef struct ErgoObj_cogito_BottomToolbar {
  ErgoObj base;
} ErgoObj_cogito_BottomToolbar;
static void ergo_drop_cogito_BottomToolbar(ErgoObj* o);

typedef struct ErgoObj_cogito_Chip {
  ErgoObj base;
} ErgoObj_cogito_Chip;
static void ergo_drop_cogito_Chip(ErgoObj* o);

typedef struct ErgoObj_cogito_FAB {
  ErgoObj base;
} ErgoObj_cogito_FAB;
static void ergo_drop_cogito_FAB(ErgoObj* o);

typedef struct ErgoObj_cogito_NavRail {
  ErgoObj base;
} ErgoObj_cogito_NavRail;
static void ergo_drop_cogito_NavRail(ErgoObj* o);

typedef struct ErgoObj_cogito_BottomNav {
  ErgoObj base;
} ErgoObj_cogito_BottomNav;
static void ergo_drop_cogito_BottomNav(ErgoObj* o);

typedef struct ErgoObj_cogito_State {
  ErgoObj base;
} ErgoObj_cogito_State;
static void ergo_drop_cogito_State(ErgoObj* o);

static void ergo_drop_cogito_App(ErgoObj* o) {
  ErgoObj_cogito_App* self = (ErgoObj_cogito_App*)o;
}

static void ergo_drop_cogito_Window(ErgoObj* o) {
  ErgoObj_cogito_Window* self = (ErgoObj_cogito_Window*)o;
}

static void ergo_drop_cogito_AppBar(ErgoObj* o) {
  ErgoObj_cogito_AppBar* self = (ErgoObj_cogito_AppBar*)o;
}

static void ergo_drop_cogito_Image(ErgoObj* o) {
  ErgoObj_cogito_Image* self = (ErgoObj_cogito_Image*)o;
}

static void ergo_drop_cogito_Dialog(ErgoObj* o) {
  ErgoObj_cogito_Dialog* self = (ErgoObj_cogito_Dialog*)o;
}

static void ergo_drop_cogito_DialogSlot(ErgoObj* o) {
  ErgoObj_cogito_DialogSlot* self = (ErgoObj_cogito_DialogSlot*)o;
}

static void ergo_drop_cogito_VStack(ErgoObj* o) {
  ErgoObj_cogito_VStack* self = (ErgoObj_cogito_VStack*)o;
}

static void ergo_drop_cogito_HStack(ErgoObj* o) {
  ErgoObj_cogito_HStack* self = (ErgoObj_cogito_HStack*)o;
}

static void ergo_drop_cogito_ZStack(ErgoObj* o) {
  ErgoObj_cogito_ZStack* self = (ErgoObj_cogito_ZStack*)o;
}

static void ergo_drop_cogito_Fixed(ErgoObj* o) {
  ErgoObj_cogito_Fixed* self = (ErgoObj_cogito_Fixed*)o;
}

static void ergo_drop_cogito_Scroller(ErgoObj* o) {
  ErgoObj_cogito_Scroller* self = (ErgoObj_cogito_Scroller*)o;
}

static void ergo_drop_cogito_Carousel(ErgoObj* o) {
  ErgoObj_cogito_Carousel* self = (ErgoObj_cogito_Carousel*)o;
}

static void ergo_drop_cogito_CarouselItem(ErgoObj* o) {
  ErgoObj_cogito_CarouselItem* self = (ErgoObj_cogito_CarouselItem*)o;
}

static void ergo_drop_cogito_List(ErgoObj* o) {
  ErgoObj_cogito_List* self = (ErgoObj_cogito_List*)o;
}

static void ergo_drop_cogito_Grid(ErgoObj* o) {
  ErgoObj_cogito_Grid* self = (ErgoObj_cogito_Grid*)o;
}

static void ergo_drop_cogito_Label(ErgoObj* o) {
  ErgoObj_cogito_Label* self = (ErgoObj_cogito_Label*)o;
}

static void ergo_drop_cogito_Button(ErgoObj* o) {
  ErgoObj_cogito_Button* self = (ErgoObj_cogito_Button*)o;
}

static void ergo_drop_cogito_Checkbox(ErgoObj* o) {
  ErgoObj_cogito_Checkbox* self = (ErgoObj_cogito_Checkbox*)o;
}

static void ergo_drop_cogito_Switch(ErgoObj* o) {
  ErgoObj_cogito_Switch* self = (ErgoObj_cogito_Switch*)o;
}

static void ergo_drop_cogito_SearchField(ErgoObj* o) {
  ErgoObj_cogito_SearchField* self = (ErgoObj_cogito_SearchField*)o;
}

static void ergo_drop_cogito_TextField(ErgoObj* o) {
  ErgoObj_cogito_TextField* self = (ErgoObj_cogito_TextField*)o;
}

static void ergo_drop_cogito_TextView(ErgoObj* o) {
  ErgoObj_cogito_TextView* self = (ErgoObj_cogito_TextView*)o;
}

static void ergo_drop_cogito_DatePicker(ErgoObj* o) {
  ErgoObj_cogito_DatePicker* self = (ErgoObj_cogito_DatePicker*)o;
}

static void ergo_drop_cogito_Stepper(ErgoObj* o) {
  ErgoObj_cogito_Stepper* self = (ErgoObj_cogito_Stepper*)o;
}

static void ergo_drop_cogito_Dropdown(ErgoObj* o) {
  ErgoObj_cogito_Dropdown* self = (ErgoObj_cogito_Dropdown*)o;
}

static void ergo_drop_cogito_Slider(ErgoObj* o) {
  ErgoObj_cogito_Slider* self = (ErgoObj_cogito_Slider*)o;
}

static void ergo_drop_cogito_Tabs(ErgoObj* o) {
  ErgoObj_cogito_Tabs* self = (ErgoObj_cogito_Tabs*)o;
}

static void ergo_drop_cogito_SegmentedControl(ErgoObj* o) {
  ErgoObj_cogito_SegmentedControl* self = (ErgoObj_cogito_SegmentedControl*)o;
}

static void ergo_drop_cogito_ViewSwitcher(ErgoObj* o) {
  ErgoObj_cogito_ViewSwitcher* self = (ErgoObj_cogito_ViewSwitcher*)o;
}

static void ergo_drop_cogito_Progress(ErgoObj* o) {
  ErgoObj_cogito_Progress* self = (ErgoObj_cogito_Progress*)o;
}

static void ergo_drop_cogito_Divider(ErgoObj* o) {
  ErgoObj_cogito_Divider* self = (ErgoObj_cogito_Divider*)o;
}

static void ergo_drop_cogito_TreeView(ErgoObj* o) {
  ErgoObj_cogito_TreeView* self = (ErgoObj_cogito_TreeView*)o;
}

static void ergo_drop_cogito_ColorPicker(ErgoObj* o) {
  ErgoObj_cogito_ColorPicker* self = (ErgoObj_cogito_ColorPicker*)o;
}

static void ergo_drop_cogito_Toasts(ErgoObj* o) {
  ErgoObj_cogito_Toasts* self = (ErgoObj_cogito_Toasts*)o;
}

static void ergo_drop_cogito_Toast(ErgoObj* o) {
  ErgoObj_cogito_Toast* self = (ErgoObj_cogito_Toast*)o;
}

static void ergo_drop_cogito_BottomToolbar(ErgoObj* o) {
  ErgoObj_cogito_BottomToolbar* self = (ErgoObj_cogito_BottomToolbar*)o;
}

static void ergo_drop_cogito_Chip(ErgoObj* o) {
  ErgoObj_cogito_Chip* self = (ErgoObj_cogito_Chip*)o;
}

static void ergo_drop_cogito_FAB(ErgoObj* o) {
  ErgoObj_cogito_FAB* self = (ErgoObj_cogito_FAB*)o;
}

static void ergo_drop_cogito_NavRail(ErgoObj* o) {
  ErgoObj_cogito_NavRail* self = (ErgoObj_cogito_NavRail*)o;
}

static void ergo_drop_cogito_BottomNav(ErgoObj* o) {
  ErgoObj_cogito_BottomNav* self = (ErgoObj_cogito_BottomNav*)o;
}

static void ergo_drop_cogito_State(ErgoObj* o) {
  ErgoObj_cogito_State* self = (ErgoObj_cogito_State*)o;
}


// ---- lambda forward decls ----
static ErgoVal ergo_lambda_1(void* env, int argc, ErgoVal* argv);
static ErgoVal ergo_lambda_2(void* env, int argc, ErgoVal* argv);
static ErgoVal ergo_lambda_3(void* env, int argc, ErgoVal* argv);
static ErgoVal ergo_lambda_4(void* env, int argc, ErgoVal* argv);
static ErgoVal ergo_lambda_5(void* env, int argc, ErgoVal* argv);
static ErgoVal ergo_lambda_6(void* env, int argc, ErgoVal* argv);
static ErgoVal ergo_lambda_7(void* env, int argc, ErgoVal* argv);
static ErgoVal ergo_lambda_8(void* env, int argc, ErgoVal* argv);
static ErgoVal ergo_lambda_9(void* env, int argc, ErgoVal* argv);
static ErgoVal ergo_lambda_10(void* env, int argc, ErgoVal* argv);
static ErgoVal ergo_lambda_11(void* env, int argc, ErgoVal* argv);
static ErgoVal ergo_lambda_12(void* env, int argc, ErgoVal* argv);
static ErgoVal ergo_lambda_13(void* env, int argc, ErgoVal* argv);
static ErgoVal ergo_lambda_14(void* env, int argc, ErgoVal* argv);
static ErgoVal ergo_lambda_15(void* env, int argc, ErgoVal* argv);

// ---- function value forward decls ----
static ErgoVal __fnwrap_main_build_ui(void* env, int argc, ErgoVal* argv);
static ErgoVal __fnwrap_cogito_label(void* env, int argc, ErgoVal* argv);
static ErgoVal __fnwrap_cogito_dialog(void* env, int argc, ErgoVal* argv);

// ---- forward decls ----
static void ergo_main_update_display_value(ErgoVal a0);
static void ergo_main_clear_expression(void);
static void ergo_main_set_expression(ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_main_clear_all(void);
static void ergo_main_commit_pending(ErgoVal a0);
static void ergo_main_input_digit(ErgoVal a0);
static void ergo_main_choose_operator(ErgoVal a0);
static void ergo_main_evaluate(void);
static ErgoVal ergo_main_digit_button(ErgoVal a0);
static ErgoVal ergo_main_operator_button(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_main_clear_button(void);
static ErgoVal ergo_main_equals_button(void);
static ErgoVal ergo_main_spacer(void);
static ErgoVal ergo_main_aton(ErgoVal a0);
static ErgoVal ergo_main_conv_unit_names(void);
static ErgoVal ergo_main_conv_to_base(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_main_conv_from_base(ErgoVal a0, ErgoVal a1);
static void ergo_main_do_convert(void);
static void ergo_main_swap_conv(void);
static void ergo_main_refresh_conv_units(void);
static ErgoVal ergo_main_build_converter_ui(void);
static void ergo_main_show_about_window(ErgoVal a0);
static void ergo_main_build_ui(ErgoVal a0);
static void ergo_init_main(void);
static void ergo_stdr___writef(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_stdr___read_line(void);
static ErgoVal ergo_stdr___readf_parse(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static ErgoVal ergo_stdr___read_text_file(ErgoVal a0);
static ErgoVal ergo_stdr___write_text_file(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_stdr___open_file_dialog(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_stdr___save_file_dialog(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static void ergo_stdr_writef(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_stdr_readf(ErgoVal a0, ErgoVal a1);
static void ergo_stdr_write(ErgoVal a0);
static ErgoVal ergo_stdr_is_null(ErgoVal a0);
static ErgoVal ergo_stdr_str(ErgoVal a0);
static ErgoVal ergo_stdr___len(ErgoVal a0);
static ErgoVal ergo_stdr_len(ErgoVal a0);
static ErgoVal ergo_stdr_read_text_file(ErgoVal a0);
static ErgoVal ergo_stdr_write_text_file(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_stdr_open_file_dialog(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_stdr_save_file_dialog(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static ErgoVal ergo_cogito___cogito_app(void);
static void ergo_cogito___cogito_app_set_appid(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_app_set_app_name(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_app_set_accent_color(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static ErgoVal ergo_cogito___cogito_window(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static ErgoVal ergo_cogito___cogito_button(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_iconbtn(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_label(ErgoVal a0);
static void ergo_cogito___cogito_label_set_class(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_label_set_text(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_node_set_class(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_node_set_a11y_label(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_node_set_a11y_role(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_node_set_tooltip(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_pointer_capture(ErgoVal a0);
static void ergo_cogito___cogito_pointer_release(void);
static void ergo_cogito___cogito_label_set_wrap(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_label_set_ellipsis(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_label_set_align(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_image(ErgoVal a0);
static void ergo_cogito___cogito_image_set_icon(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_image_set_source(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_image_set_size(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static void ergo_cogito___cogito_image_set_radius(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_dialog(ErgoVal a0);
static void ergo_cogito___cogito_dialog_close(ErgoVal a0);
static void ergo_cogito___cogito_dialog_remove(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_find_parent(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_find_children(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_dialog_slot(void);
static void ergo_cogito___cogito_dialog_slot_show(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_dialog_slot_clear(ErgoVal a0);
static void ergo_cogito___cogito_window_set_dialog(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_window_clear_dialog(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_node_window(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_checkbox(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_switch(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_textfield(ErgoVal a0);
static void ergo_cogito___cogito_searchfield_set_text(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_searchfield_get_text(ErgoVal a0);
static void ergo_cogito___cogito_searchfield_on_change(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_textview(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_searchfield(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_dropdown(void);
static ErgoVal ergo_cogito___cogito_datepicker(void);
static void ergo_cogito___cogito_datepicker_on_change(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_stepper(ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_cogito___cogito_slider(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static ErgoVal ergo_cogito___cogito_tabs(void);
static ErgoVal ergo_cogito___cogito_segmented(void);
static ErgoVal ergo_cogito___cogito_view_switcher(void);
static ErgoVal ergo_cogito___cogito_progress(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_divider(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_treeview(void);
static ErgoVal ergo_cogito___cogito_colorpicker(void);
static void ergo_cogito___cogito_colorpicker_on_change(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_toasts(void);
static ErgoVal ergo_cogito___cogito_toast(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_toolbar(void);
static ErgoVal ergo_cogito___cogito_vstack(void);
static ErgoVal ergo_cogito___cogito_hstack(void);
static ErgoVal ergo_cogito___cogito_zstack(void);
static ErgoVal ergo_cogito___cogito_fixed(void);
static ErgoVal ergo_cogito___cogito_scroller(void);
static ErgoVal ergo_cogito___cogito_carousel(void);
static ErgoVal ergo_cogito___cogito_carousel_item(void);
static void ergo_cogito___cogito_carousel_item_set_text(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_carousel_item_set_halign(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_carousel_item_set_valign(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_carousel_set_active_index(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_carousel_get_active_index(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_list(void);
static ErgoVal ergo_cogito___cogito_grid(ErgoVal a0);
static void ergo_cogito___cogito_container_add(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_container_set_margins(ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3, ErgoVal a4);
static void ergo_cogito___cogito_build(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_window_set_builder(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_state_new(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_state_get(ErgoVal a0);
static void ergo_cogito___cogito_state_set(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_container_set_align(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_container_set_halign(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_container_set_valign(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_container_set_hexpand(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_container_set_vexpand(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_container_set_gap(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_container_set_padding(ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3, ErgoVal a4);
static void ergo_cogito___cogito_fixed_set_pos(ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_cogito___cogito_scroller_set_axes(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static void ergo_cogito___cogito_grid_set_gap(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static void ergo_cogito___cogito_grid_set_span(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static void ergo_cogito___cogito_grid_set_align(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static void ergo_cogito___cogito_node_set_disabled(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_node_set_editable(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_node_get_editable(ErgoVal a0);
static void ergo_cogito___cogito_node_set_id(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_window_set_autosize(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_window_set_resizable(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_appbar(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_appbar_add_button(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static void ergo_cogito___cogito_appbar_set_controls(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_appbar_set_title(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_appbar_set_subtitle(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_button_set_text(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_button_add_menu(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static void ergo_cogito___cogito_iconbtn_add_menu(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static void ergo_cogito___cogito_checkbox_set_checked(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_checkbox_get_checked(ErgoVal a0);
static void ergo_cogito___cogito_switch_set_checked(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_switch_get_checked(ErgoVal a0);
static void ergo_cogito___cogito_checkbox_on_change(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_switch_on_change(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_textfield_set_text(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_textfield_get_text(ErgoVal a0);
static void ergo_cogito___cogito_textfield_on_change(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_textview_set_text(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_textview_get_text(ErgoVal a0);
static void ergo_cogito___cogito_textview_on_change(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_dropdown_set_items(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_dropdown_set_selected(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_dropdown_get_selected(ErgoVal a0);
static void ergo_cogito___cogito_dropdown_on_change(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_slider_set_value(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_slider_get_value(ErgoVal a0);
static void ergo_cogito___cogito_slider_on_change(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_tabs_set_items(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_tabs_set_ids(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_tabs_set_selected(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_tabs_get_selected(ErgoVal a0);
static void ergo_cogito___cogito_tabs_on_change(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_tabs_bind(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_view_switcher_set_active(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_progress_set_value(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_progress_get_value(ErgoVal a0);
static void ergo_cogito___cogito_toast_set_text(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_toast_on_click(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_list_on_select(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_list_on_activate(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_grid_on_select(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_grid_on_activate(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_button_on_click(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_chip(ErgoVal a0);
static void ergo_cogito___cogito_chip_set_selected(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_chip_get_selected(ErgoVal a0);
static void ergo_cogito___cogito_chip_set_closable(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_chip_on_click(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_chip_on_close(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_fab(ErgoVal a0);
static void ergo_cogito___cogito_fab_set_extended(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static void ergo_cogito___cogito_fab_on_click(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_nav_rail(void);
static void ergo_cogito___cogito_nav_rail_set_items(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static void ergo_cogito___cogito_nav_rail_set_selected(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_nav_rail_get_selected(ErgoVal a0);
static void ergo_cogito___cogito_nav_rail_on_change(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_bottom_nav(void);
static void ergo_cogito___cogito_bottom_nav_set_items(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static void ergo_cogito___cogito_bottom_nav_set_selected(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito___cogito_bottom_nav_get_selected(ErgoVal a0);
static void ergo_cogito___cogito_bottom_nav_on_change(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_run(ErgoVal a0, ErgoVal a1);
static void ergo_cogito___cogito_load_sum(ErgoVal a0);
static void ergo_cogito___cogito_set_script_dir(ErgoVal a0);
static ErgoVal ergo_cogito___cogito_open_url(ErgoVal a0);
static void ergo_m_cogito_App_run(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_App_set_appid(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_App_set_app_name(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_App_set_accent_color(ErgoVal self, ErgoVal a0, ErgoVal a1);
static void ergo_m_cogito_Window_add(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Window_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_Window_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_Window_set_autosize(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Window_set_resizable(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Window_set_a11y_label(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Window_set_disabled(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Window_set_dialog(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Window_clear_dialog(ErgoVal self);
static ErgoVal ergo_m_cogito_Window_build(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Window_set_class(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Window_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_AppBar_add_button(ErgoVal self, ErgoVal a0, ErgoVal a1);
static void ergo_m_cogito_AppBar_set_window_controls(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_AppBar_set_title(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_AppBar_set_subtitle(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_AppBar_set_hexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_AppBar_set_vexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_AppBar_set_disabled(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_AppBar_set_class(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_AppBar_set_id(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Image_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_Image_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_Image_set_align(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Image_set_halign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Image_set_valign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Image_set_icon(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Image_set_source(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Image_set_size(ErgoVal self, ErgoVal a0, ErgoVal a1);
static void ergo_m_cogito_Image_set_radius(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Image_set_hexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Image_set_vexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Image_set_disabled(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Image_set_class(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Image_set_id(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Dialog_add(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Dialog_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_Dialog_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Dialog_build(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Dialog_window(ErgoVal self);
static void ergo_m_cogito_Dialog_set_hexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Dialog_set_vexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Dialog_close(ErgoVal self);
static void ergo_m_cogito_Dialog_remove(ErgoVal self);
static void ergo_m_cogito_Dialog_set_disabled(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Dialog_set_class(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Dialog_set_id(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_DialogSlot_show(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_DialogSlot_clear(ErgoVal self);
static void ergo_m_cogito_DialogSlot_set_hexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_DialogSlot_set_vexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_DialogSlot_set_disabled(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_DialogSlot_set_class(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_DialogSlot_set_id(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_VStack_add(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_VStack_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_VStack_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_VStack_set_align(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_VStack_halign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_VStack_valign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_VStack_set_hexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_VStack_set_vexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_VStack_set_gap(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_VStack_align_begin(ErgoVal self);
static void ergo_m_cogito_VStack_align_center(ErgoVal self);
static void ergo_m_cogito_VStack_align_end(ErgoVal self);
static ErgoVal ergo_m_cogito_VStack_build(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_VStack_set_disabled(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_VStack_set_class(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_VStack_set_id(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_HStack_add(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_HStack_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_HStack_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_HStack_set_align(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_HStack_halign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_HStack_valign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_HStack_set_hexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_HStack_set_vexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_HStack_set_gap(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_HStack_align_begin(ErgoVal self);
static void ergo_m_cogito_HStack_align_center(ErgoVal self);
static void ergo_m_cogito_HStack_align_end(ErgoVal self);
static ErgoVal ergo_m_cogito_HStack_build(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_HStack_set_disabled(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_HStack_set_class(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_HStack_set_id(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_ZStack_add(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_ZStack_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_ZStack_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_ZStack_set_align(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_ZStack_halign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_ZStack_valign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_ZStack_align_begin(ErgoVal self);
static void ergo_m_cogito_ZStack_align_center(ErgoVal self);
static void ergo_m_cogito_ZStack_align_end(ErgoVal self);
static ErgoVal ergo_m_cogito_ZStack_build(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_ZStack_set_hexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_ZStack_set_vexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_ZStack_set_disabled(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_ZStack_set_class(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_ZStack_set_id(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Fixed_add(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Fixed_set_pos(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2);
static void ergo_m_cogito_Fixed_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Fixed_build(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Fixed_set_hexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Fixed_set_vexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Fixed_set_disabled(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Fixed_set_class(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Fixed_set_id(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Scroller_add(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Scroller_set_axes(ErgoVal self, ErgoVal a0, ErgoVal a1);
static void ergo_m_cogito_Scroller_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_m_cogito_Scroller_build(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Scroller_set_hexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Scroller_set_vexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Scroller_set_disabled(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Scroller_set_class(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Scroller_set_id(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Carousel_add(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Carousel_set_active_index(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Carousel_active_index(ErgoVal self);
static void ergo_m_cogito_Carousel_set_hexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Carousel_set_vexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Carousel_set_class(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Carousel_set_id(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_CarouselItem_add(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_CarouselItem_set_hexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_CarouselItem_set_vexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_CarouselItem_set_class(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_CarouselItem_set_id(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_CarouselItem_set_text(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_CarouselItem_set_halign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_CarouselItem_set_valign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_List_add(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_List_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_List_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_List_set_align(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_List_halign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_List_valign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_List_align_begin(ErgoVal self);
static void ergo_m_cogito_List_align_center(ErgoVal self);
static void ergo_m_cogito_List_align_end(ErgoVal self);
static void ergo_m_cogito_List_on_select(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_List_on_activate(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_List_build(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_List_set_hexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_List_set_vexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_List_set_disabled(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_List_set_class(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_List_set_id(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Grid_add(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Grid_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_Grid_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_Grid_set_gap(ErgoVal self, ErgoVal a0, ErgoVal a1);
static void ergo_m_cogito_Grid_set_span(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2);
static void ergo_m_cogito_Grid_set_cell_align(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2);
static void ergo_m_cogito_Grid_set_align(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Grid_halign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Grid_valign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Grid_align_begin(ErgoVal self);
static void ergo_m_cogito_Grid_align_center(ErgoVal self);
static void ergo_m_cogito_Grid_align_end(ErgoVal self);
static void ergo_m_cogito_Grid_on_select(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Grid_on_activate(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Grid_build(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Grid_set_hexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Grid_set_vexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Grid_set_disabled(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Grid_set_class(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Grid_set_id(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Label_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_Label_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_Label_set_align(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Label_set_halign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Label_set_valign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Label_align_begin(ErgoVal self);
static void ergo_m_cogito_Label_align_center(ErgoVal self);
static void ergo_m_cogito_Label_align_end(ErgoVal self);
static void ergo_m_cogito_Label_set_class(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Label_set_text(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Label_set_wrap(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Label_set_ellipsis(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Label_set_text_align(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Label_set_hexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Label_set_vexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Label_set_disabled(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Label_set_id(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Button_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_Button_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_Button_set_align(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Button_set_halign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Button_set_valign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Button_set_hexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Button_set_vexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Button_align_begin(ErgoVal self);
static void ergo_m_cogito_Button_align_center(ErgoVal self);
static void ergo_m_cogito_Button_align_end(ErgoVal self);
static void ergo_m_cogito_Button_set_text(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Button_on_click(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Button_add_menu(ErgoVal self, ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_m_cogito_Button_window(ErgoVal self);
static void ergo_m_cogito_Button_set_disabled(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Button_set_class(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Button_set_id(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Checkbox_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_Checkbox_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_Checkbox_set_align(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Checkbox_set_halign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Checkbox_set_valign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Checkbox_align_begin(ErgoVal self);
static void ergo_m_cogito_Checkbox_align_center(ErgoVal self);
static void ergo_m_cogito_Checkbox_align_end(ErgoVal self);
static void ergo_m_cogito_Checkbox_set_checked(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Checkbox_checked(ErgoVal self);
static void ergo_m_cogito_Checkbox_on_change(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Checkbox_window(ErgoVal self);
static void ergo_m_cogito_Checkbox_set_hexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Checkbox_set_vexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Checkbox_set_disabled(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Checkbox_set_class(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Checkbox_set_id(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Switch_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_Switch_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_Switch_set_align(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Switch_set_halign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Switch_set_valign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Switch_align_begin(ErgoVal self);
static void ergo_m_cogito_Switch_align_center(ErgoVal self);
static void ergo_m_cogito_Switch_align_end(ErgoVal self);
static void ergo_m_cogito_Switch_set_checked(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Switch_checked(ErgoVal self);
static void ergo_m_cogito_Switch_on_change(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Switch_window(ErgoVal self);
static void ergo_m_cogito_Switch_set_hexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Switch_set_vexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Switch_set_disabled(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Switch_set_class(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Switch_set_id(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_SearchField_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_SearchField_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_SearchField_set_align(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_SearchField_set_halign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_SearchField_set_valign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_SearchField_set_text(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SearchField_text(ErgoVal self);
static void ergo_m_cogito_SearchField_on_change(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_SearchField_set_hexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_SearchField_set_vexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_SearchField_set_editable(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SearchField_editable(ErgoVal self);
static void ergo_m_cogito_SearchField_set_class(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_SearchField_set_id(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_TextField_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_TextField_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_TextField_set_align(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_TextField_set_halign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_TextField_set_valign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_TextField_align_begin(ErgoVal self);
static void ergo_m_cogito_TextField_align_center(ErgoVal self);
static void ergo_m_cogito_TextField_align_end(ErgoVal self);
static void ergo_m_cogito_TextField_set_text(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TextField_text(ErgoVal self);
static void ergo_m_cogito_TextField_on_change(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_TextField_set_hexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_TextField_set_vexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_TextField_set_editable(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TextField_editable(ErgoVal self);
static void ergo_m_cogito_TextField_set_disabled(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_TextField_set_class(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_TextField_set_id(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_TextView_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_TextView_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_TextView_set_align(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_TextView_set_halign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_TextView_set_valign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_TextView_align_begin(ErgoVal self);
static void ergo_m_cogito_TextView_align_center(ErgoVal self);
static void ergo_m_cogito_TextView_align_end(ErgoVal self);
static void ergo_m_cogito_TextView_set_text(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TextView_text(ErgoVal self);
static void ergo_m_cogito_TextView_on_change(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_TextView_set_hexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_TextView_set_vexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_TextView_set_editable(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_TextView_editable(ErgoVal self);
static void ergo_m_cogito_TextView_set_disabled(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_TextView_set_class(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_TextView_set_id(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_DatePicker_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_DatePicker_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_DatePicker_set_align(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_DatePicker_set_halign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_DatePicker_set_valign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_DatePicker_set_date(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2);
static ErgoVal ergo_m_cogito_DatePicker_date(ErgoVal self);
static void ergo_m_cogito_DatePicker_on_change(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_DatePicker_set_a11y_label(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_DatePicker_set_a11y_role(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_DatePicker_set_hexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_DatePicker_set_vexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_DatePicker_set_disabled(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_DatePicker_set_class(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_DatePicker_set_id(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Stepper_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_Stepper_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_Stepper_set_align(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Stepper_set_halign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Stepper_set_valign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Stepper_set_value(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Stepper_value(ErgoVal self);
static void ergo_m_cogito_Stepper_on_change(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Stepper_set_hexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Stepper_set_vexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Stepper_set_class(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Stepper_set_id(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Dropdown_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_Dropdown_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_Dropdown_set_align(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Dropdown_set_halign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Dropdown_set_valign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Dropdown_align_begin(ErgoVal self);
static void ergo_m_cogito_Dropdown_align_center(ErgoVal self);
static void ergo_m_cogito_Dropdown_align_end(ErgoVal self);
static void ergo_m_cogito_Dropdown_set_items(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Dropdown_set_selected(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Dropdown_selected(ErgoVal self);
static void ergo_m_cogito_Dropdown_on_change(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Dropdown_set_hexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Dropdown_set_vexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Dropdown_set_disabled(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Dropdown_set_class(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Dropdown_set_id(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Slider_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_Slider_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_Slider_set_align(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Slider_set_halign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Slider_set_valign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Slider_align_begin(ErgoVal self);
static void ergo_m_cogito_Slider_align_center(ErgoVal self);
static void ergo_m_cogito_Slider_align_end(ErgoVal self);
static void ergo_m_cogito_Slider_set_value(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Slider_value(ErgoVal self);
static void ergo_m_cogito_Slider_on_change(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Slider_set_hexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Slider_set_vexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Slider_set_disabled(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Slider_set_class(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Slider_set_id(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Tabs_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_Tabs_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_Tabs_set_align(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Tabs_set_halign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Tabs_set_valign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Tabs_set_items(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Tabs_set_ids(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Tabs_set_selected(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Tabs_selected(ErgoVal self);
static void ergo_m_cogito_Tabs_on_change(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Tabs_bind(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Tabs_set_hexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Tabs_set_vexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Tabs_set_disabled(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Tabs_set_class(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Tabs_set_id(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_SegmentedControl_add(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_SegmentedControl_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_SegmentedControl_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_SegmentedControl_set_align(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_SegmentedControl_set_halign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_SegmentedControl_set_valign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_SegmentedControl_set_items(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_SegmentedControl_set_selected(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_SegmentedControl_selected(ErgoVal self);
static void ergo_m_cogito_SegmentedControl_on_change(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_SegmentedControl_set_hexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_SegmentedControl_set_vexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_SegmentedControl_set_class(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_SegmentedControl_set_id(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_ViewSwitcher_add(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_ViewSwitcher_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_ViewSwitcher_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_ViewSwitcher_set_align(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_ViewSwitcher_halign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_ViewSwitcher_valign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_ViewSwitcher_set_active(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ViewSwitcher_build(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_ViewSwitcher_set_hexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_ViewSwitcher_set_vexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_ViewSwitcher_set_disabled(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_ViewSwitcher_set_class(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_ViewSwitcher_set_id(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Progress_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_Progress_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_Progress_set_align(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Progress_set_halign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Progress_set_valign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Progress_set_value(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Progress_value(ErgoVal self);
static void ergo_m_cogito_Progress_set_hexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Progress_set_vexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Progress_set_disabled(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Progress_set_class(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Progress_set_id(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Divider_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_Divider_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_Divider_set_align(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Divider_set_halign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Divider_set_valign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Divider_set_hexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Divider_set_vexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Divider_set_disabled(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Divider_set_class(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Divider_set_id(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_TreeView_add(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_TreeView_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_TreeView_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_TreeView_set_align(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_TreeView_set_halign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_TreeView_set_valign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_TreeView_set_hexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_TreeView_set_vexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_TreeView_set_class(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_TreeView_set_id(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_ColorPicker_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_ColorPicker_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_ColorPicker_set_align(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_ColorPicker_set_halign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_ColorPicker_set_valign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_ColorPicker_set_hex(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_ColorPicker_hex(ErgoVal self);
static void ergo_m_cogito_ColorPicker_on_change(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_ColorPicker_set_a11y_label(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_ColorPicker_set_a11y_role(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_ColorPicker_set_hexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_ColorPicker_set_vexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_ColorPicker_set_disabled(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_ColorPicker_set_class(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_ColorPicker_set_id(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Toasts_add(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Toasts_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_Toasts_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_Toasts_set_align(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Toasts_build(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Toasts_set_hexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Toasts_set_vexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Toasts_set_disabled(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Toasts_set_class(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Toasts_set_id(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Toast_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_Toast_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_Toast_set_align(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Toast_set_halign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Toast_set_valign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Toast_set_text(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Toast_on_click(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Toast_set_hexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Toast_set_vexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Toast_set_disabled(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Toast_set_class(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Toast_set_id(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_BottomToolbar_add(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_BottomToolbar_set_hexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_BottomToolbar_set_vexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_BottomToolbar_set_class(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_BottomToolbar_set_id(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Chip_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_Chip_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_Chip_set_align(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Chip_set_halign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Chip_set_valign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Chip_set_hexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Chip_set_vexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Chip_set_selected(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_Chip_selected(ErgoVal self);
static void ergo_m_cogito_Chip_set_closable(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Chip_on_click(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Chip_on_close(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Chip_set_disabled(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Chip_set_class(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_Chip_set_id(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_FAB_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_FAB_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_FAB_set_align(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_FAB_set_halign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_FAB_set_valign(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_FAB_align_begin(ErgoVal self);
static void ergo_m_cogito_FAB_align_center(ErgoVal self);
static void ergo_m_cogito_FAB_align_end(ErgoVal self);
static void ergo_m_cogito_FAB_set_hexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_FAB_set_vexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_FAB_set_extended(ErgoVal self, ErgoVal a0, ErgoVal a1);
static void ergo_m_cogito_FAB_on_click(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_FAB_set_disabled(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_FAB_set_class(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_FAB_set_id(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_NavRail_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_NavRail_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_NavRail_set_hexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_NavRail_set_vexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_NavRail_set_items(ErgoVal self, ErgoVal a0, ErgoVal a1);
static void ergo_m_cogito_NavRail_set_selected(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_NavRail_selected(ErgoVal self);
static void ergo_m_cogito_NavRail_on_change(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_NavRail_set_disabled(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_NavRail_set_class(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_NavRail_set_id(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_BottomNav_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_BottomNav_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static void ergo_m_cogito_BottomNav_set_hexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_BottomNav_set_vexpand(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_BottomNav_set_items(ErgoVal self, ErgoVal a0, ErgoVal a1);
static void ergo_m_cogito_BottomNav_set_selected(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_BottomNav_selected(ErgoVal self);
static void ergo_m_cogito_BottomNav_on_change(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_BottomNav_set_disabled(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_BottomNav_set_class(ErgoVal self, ErgoVal a0);
static void ergo_m_cogito_BottomNav_set_id(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_m_cogito_State_get(ErgoVal self);
static void ergo_m_cogito_State_set(ErgoVal self, ErgoVal a0);
static ErgoVal ergo_cogito_app(void);
static void ergo_cogito_load_sum(ErgoVal a0);
static void ergo_cogito_set_script_dir(ErgoVal a0);
static ErgoVal ergo_cogito_open_url(ErgoVal a0);
static void ergo_cogito_set_class(ErgoVal a0, ErgoVal a1);
static void ergo_cogito_set_a11y_label(ErgoVal a0, ErgoVal a1);
static void ergo_cogito_set_a11y_role(ErgoVal a0, ErgoVal a1);
static void ergo_cogito_set_tooltip(ErgoVal a0, ErgoVal a1);
static void ergo_cogito_pointer_capture(ErgoVal a0);
static void ergo_cogito_pointer_release(void);
static ErgoVal ergo_cogito_window(void);
static ErgoVal ergo_cogito_window_title(ErgoVal a0);
static ErgoVal ergo_cogito_window_size(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static ErgoVal ergo_cogito_about_window(ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3, ErgoVal a4);
static ErgoVal ergo_cogito_build(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito_state(ErgoVal a0);
static void ergo_cogito_set_id(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito_vstack(void);
static ErgoVal ergo_cogito_hstack(void);
static ErgoVal ergo_cogito_zstack(void);
static ErgoVal ergo_cogito_fixed(void);
static ErgoVal ergo_cogito_scroller(void);
static ErgoVal ergo_cogito_carousel(void);
static ErgoVal ergo_cogito_carousel_item(void);
static ErgoVal ergo_cogito_list(void);
static ErgoVal ergo_cogito_grid(ErgoVal a0);
static ErgoVal ergo_cogito_tabs(void);
static ErgoVal ergo_cogito_view_switcher(void);
static ErgoVal ergo_cogito_progress(ErgoVal a0);
static ErgoVal ergo_cogito_divider(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito_toasts(void);
static ErgoVal ergo_cogito_toast(ErgoVal a0);
static ErgoVal ergo_cogito_label(ErgoVal a0);
static ErgoVal ergo_cogito_image(ErgoVal a0);
static ErgoVal ergo_cogito_dialog(ErgoVal a0);
static ErgoVal ergo_cogito_dialog_slot(void);
static ErgoVal ergo_cogito_button(ErgoVal a0);
static ErgoVal ergo_cogito_iconbtn(ErgoVal a0);
static ErgoVal ergo_cogito_appbar(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito_checkbox(ErgoVal a0, ErgoVal a1);
static ErgoVal ergo_cogito_switch(ErgoVal a0);
static ErgoVal ergo_cogito_textfield(ErgoVal a0);
static ErgoVal ergo_cogito_searchfield(ErgoVal a0);
static ErgoVal ergo_cogito_textview(ErgoVal a0);
static ErgoVal ergo_cogito_dropdown(void);
static ErgoVal ergo_cogito_datepicker(void);
static ErgoVal ergo_cogito_stepper(ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3);
static ErgoVal ergo_cogito_slider(ErgoVal a0, ErgoVal a1, ErgoVal a2);
static ErgoVal ergo_cogito_segmented(void);
static ErgoVal ergo_cogito_treeview(void);
static ErgoVal ergo_cogito_colorpicker(void);
static ErgoVal ergo_cogito_bottom_toolbar(void);
static ErgoVal ergo_cogito_chip(ErgoVal a0);
static ErgoVal ergo_cogito_fab(ErgoVal a0);
static ErgoVal ergo_cogito_nav_rail(void);
static ErgoVal ergo_cogito_bottom_nav(void);
static ErgoVal ergo_cogito_find_parent(ErgoVal a0);
static void ergo_cogito_dialog_slot_clear(ErgoVal a0);
static ErgoVal ergo_cogito_find_children(ErgoVal a0);
static void ergo_entry(void);

// ---- function value defs ----
static ErgoVal __fnwrap_main_build_ui(void* env, int argc, ErgoVal* argv) {
  (void)env;
  if (argc != 1) ergo_trap("fn arity mismatch");
  ErgoVal arg0 = argv[0];
  ergo_main_build_ui(arg0);
  return EV_NULLV;
}
static ErgoVal __fnwrap_cogito_label(void* env, int argc, ErgoVal* argv) {
  (void)env;
  if (argc != 1) ergo_trap("fn arity mismatch");
  ErgoVal arg0 = argv[0];
  return ergo_cogito_label(arg0);
}
static ErgoVal __fnwrap_cogito_dialog(void* env, int argc, ErgoVal* argv) {
  (void)env;
  if (argc != 1) ergo_trap("fn arity mismatch");
  ErgoVal arg0 = argv[0];
  return ergo_cogito_dialog(arg0);
}

// ---- cask global init ----
static void ergo_init_main(void) {
  ErgoVal __t1 = EV_STR(stdr_str_lit(""));
  ErgoVal __t2 = ergo_cogito_label(__t1);
  ergo_release_val(__t1);
  ergo_move_into(&ergo_g_main_display_expression, __t2);
  ErgoVal __t3 = EV_STR(stdr_str_lit("0"));
  ErgoVal __t4 = EV_NULLV;
  {
    ErgoVal __parts1[1] = { __t3 };
    ErgoStr* __s2 = stdr_str_from_parts(1, __parts1);
    __t4 = EV_STR(__s2);
  }
  ergo_release_val(__t3);
  ErgoVal __t5 = ergo_cogito_label(__t4);
  ergo_release_val(__t4);
  ergo_move_into(&ergo_g_main_display_working, __t5);
  ErgoVal __t6 = EV_FLOAT(0);
  ergo_move_into(&ergo_g_main_current_value, __t6);
  ErgoVal __t7 = EV_FLOAT(0);
  ergo_move_into(&ergo_g_main_stored_value, __t7);
  ErgoVal __t8 = EV_STR(stdr_str_lit(""));
  ergo_move_into(&ergo_g_main_pending_op, __t8);
  ErgoVal __t9 = EV_BOOL(true);
  ergo_move_into(&ergo_g_main_reset_input, __t9);
  ErgoVal __t10 = EV_BOOL(false);
  ergo_move_into(&ergo_g_main_has_error, __t10);
  ErgoVal __t11 = EV_BOOL(false);
  ergo_move_into(&ergo_g_main_showing_converter, __t11);
  ErgoVal __t12 = EV_STR(stdr_str_lit("1"));
  ErgoVal __t13 = EV_NULLV;
  {
    ErgoVal __parts3[1] = { __t12 };
    ErgoStr* __s4 = stdr_str_from_parts(1, __parts3);
    __t13 = EV_STR(__s4);
  }
  ergo_release_val(__t12);
  ErgoVal __t14 = ergo_cogito_textfield(__t13);
  ergo_release_val(__t13);
  ergo_move_into(&ergo_g_main_conv_input, __t14);
  ErgoVal __t15 = EV_STR(stdr_str_lit("0"));
  ErgoVal __t16 = EV_NULLV;
  {
    ErgoVal __parts5[1] = { __t15 };
    ErgoStr* __s6 = stdr_str_from_parts(1, __parts5);
    __t16 = EV_STR(__s6);
  }
  ergo_release_val(__t15);
  ErgoVal __t17 = ergo_cogito_textfield(__t16);
  ergo_release_val(__t16);
  ergo_move_into(&ergo_g_main_conv_output, __t17);
  ErgoVal __t18 = EV_INT(0);
  ergo_move_into(&ergo_g_main_conv_from_idx, __t18);
  ErgoVal __t19 = EV_INT(1);
  ergo_move_into(&ergo_g_main_conv_to_idx, __t19);
  ErgoVal __t20 = ergo_cogito_dropdown();
  ergo_move_into(&ergo_g_main_conv_from_dd, __t20);
  ErgoVal __t21 = ergo_cogito_dropdown();
  ergo_move_into(&ergo_g_main_conv_to_dd, __t21);
  ErgoVal __t22 = EV_INT(0);
  ergo_move_into(&ergo_g_main_conv_category, __t22);
  ErgoVal __t23 = EV_STR(stdr_str_lit("https://github.com/lainsce/ergo"));
  ErgoVal __t24 = EV_NULLV;
  {
    ErgoVal __parts7[1] = { __t23 };
    ErgoStr* __s8 = stdr_str_from_parts(1, __parts7);
    __t24 = EV_STR(__s8);
  }
  ergo_release_val(__t23);
  ergo_move_into(&ergo_g_main_ABOUT_MORE_INFO_URL, __t24);
  ErgoVal __t25 = EV_STR(stdr_str_lit("https://github.com/lainsce/ergo/issues"));
  ErgoVal __t26 = EV_NULLV;
  {
    ErgoVal __parts9[1] = { __t25 };
    ErgoStr* __s10 = stdr_str_from_parts(1, __parts9);
    __t26 = EV_STR(__s10);
  }
  ergo_release_val(__t25);
  ergo_move_into(&ergo_g_main_ABOUT_REPORT_BUG_URL, __t26);
}

// ---- compiled functions ----
static void ergo_main_update_display_value(ErgoVal a0) {
  ErgoVal __t27 = ergo_g_main_display_working; ergo_retain_val(__t27);
  ErgoVal __t28 = a0; ergo_retain_val(__t28);
  ergo_m_cogito_Label_set_text(__t27, __t28);
  ergo_release_val(__t27);
  ergo_release_val(__t28);
  ErgoVal __t29 = EV_NULLV;
  ergo_release_val(__t29);
}

static void ergo_main_clear_expression(void) {
  ErgoVal __t30 = ergo_g_main_display_expression; ergo_retain_val(__t30);
  ErgoVal __t31 = EV_STR(stdr_str_lit(""));
  ergo_m_cogito_Label_set_text(__t30, __t31);
  ergo_release_val(__t30);
  ergo_release_val(__t31);
  ErgoVal __t32 = EV_NULLV;
  ergo_release_val(__t32);
}

static void ergo_main_set_expression(ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal text__1 = EV_NULLV;
  ErgoVal __t33 = EV_NULLV;
  ErgoVal __t34 = a2; ergo_retain_val(__t34);
  ErgoVal __t35 = EV_STR(stdr_str_lit(""));
  ErgoVal __t36 = ergo_eq(__t34, __t35);
  ergo_release_val(__t34);
  ergo_release_val(__t35);
  bool __b2 = ergo_as_bool(__t36);
  ergo_release_val(__t36);
  if (__b2) {
    ErgoVal __t37 = EV_NULLV;
    ErgoVal __t38 = a3; ergo_retain_val(__t38);
    bool __b3 = ergo_as_bool(__t38);
    ergo_release_val(__t38);
    if (__b3) {
      ErgoVal __t39 = a0; ergo_retain_val(__t39);
      ErgoVal __t40 = EV_STR(stdr_str_lit(" "));
      ErgoVal __t41 = a1; ergo_retain_val(__t41);
      ErgoVal __t42 = EV_STR(stdr_str_lit(" ="));
      ErgoVal __t43 = EV_NULLV;
      {
        ErgoVal __parts11[4] = { __t39, __t40, __t41, __t42 };
        ErgoStr* __s12 = stdr_str_from_parts(4, __parts11);
        __t43 = EV_STR(__s12);
      }
      ergo_release_val(__t39);
      ergo_release_val(__t40);
      ergo_release_val(__t41);
      ergo_release_val(__t42);
      ergo_move_into(&__t37, __t43);
    } else {
      ErgoVal __t44 = a0; ergo_retain_val(__t44);
      ErgoVal __t45 = EV_STR(stdr_str_lit(" "));
      ErgoVal __t46 = a1; ergo_retain_val(__t46);
      ErgoVal __t47 = EV_NULLV;
      {
        ErgoVal __parts13[3] = { __t44, __t45, __t46 };
        ErgoStr* __s14 = stdr_str_from_parts(3, __parts13);
        __t47 = EV_STR(__s14);
      }
      ergo_release_val(__t44);
      ergo_release_val(__t45);
      ergo_release_val(__t46);
      ergo_move_into(&__t37, __t47);
    }
    ergo_move_into(&__t33, __t37);
  } else {
    ErgoVal __t48 = EV_NULLV;
    ErgoVal __t49 = a3; ergo_retain_val(__t49);
    bool __b4 = ergo_as_bool(__t49);
    ergo_release_val(__t49);
    if (__b4) {
      ErgoVal __t50 = a0; ergo_retain_val(__t50);
      ErgoVal __t51 = EV_STR(stdr_str_lit(" "));
      ErgoVal __t52 = a1; ergo_retain_val(__t52);
      ErgoVal __t53 = EV_STR(stdr_str_lit(" "));
      ErgoVal __t54 = a2; ergo_retain_val(__t54);
      ErgoVal __t55 = EV_STR(stdr_str_lit(" ="));
      ErgoVal __t56 = EV_NULLV;
      {
        ErgoVal __parts15[6] = { __t50, __t51, __t52, __t53, __t54, __t55 };
        ErgoStr* __s16 = stdr_str_from_parts(6, __parts15);
        __t56 = EV_STR(__s16);
      }
      ergo_release_val(__t50);
      ergo_release_val(__t51);
      ergo_release_val(__t52);
      ergo_release_val(__t53);
      ergo_release_val(__t54);
      ergo_release_val(__t55);
      ergo_move_into(&__t48, __t56);
    } else {
      ErgoVal __t57 = a0; ergo_retain_val(__t57);
      ErgoVal __t58 = EV_STR(stdr_str_lit(" "));
      ErgoVal __t59 = a1; ergo_retain_val(__t59);
      ErgoVal __t60 = EV_STR(stdr_str_lit(" "));
      ErgoVal __t61 = a2; ergo_retain_val(__t61);
      ErgoVal __t62 = EV_NULLV;
      {
        ErgoVal __parts17[5] = { __t57, __t58, __t59, __t60, __t61 };
        ErgoStr* __s18 = stdr_str_from_parts(5, __parts17);
        __t62 = EV_STR(__s18);
      }
      ergo_release_val(__t57);
      ergo_release_val(__t58);
      ergo_release_val(__t59);
      ergo_release_val(__t60);
      ergo_release_val(__t61);
      ergo_move_into(&__t48, __t62);
    }
    ergo_move_into(&__t33, __t48);
  }
  ergo_move_into(&text__1, __t33);
  ErgoVal __t63 = ergo_g_main_display_expression; ergo_retain_val(__t63);
  ErgoVal __t64 = text__1; ergo_retain_val(__t64);
  ergo_m_cogito_Label_set_text(__t63, __t64);
  ergo_release_val(__t63);
  ergo_release_val(__t64);
  ErgoVal __t65 = EV_NULLV;
  ergo_release_val(__t65);
  ergo_release_val(text__1);
}

static void ergo_main_clear_all(void) {
  ErgoVal __t66 = EV_FLOAT(0);
  ErgoVal __t67 = __t66; ergo_retain_val(__t67);
  ergo_move_into(&ergo_g_main_current_value, __t66);
  ergo_release_val(__t67);
  ErgoVal __t68 = EV_FLOAT(0);
  ErgoVal __t69 = __t68; ergo_retain_val(__t69);
  ergo_move_into(&ergo_g_main_stored_value, __t68);
  ergo_release_val(__t69);
  ErgoVal __t70 = EV_STR(stdr_str_lit(""));
  ErgoVal __t71 = __t70; ergo_retain_val(__t71);
  ergo_move_into(&ergo_g_main_pending_op, __t70);
  ergo_release_val(__t71);
  ErgoVal __t72 = EV_BOOL(true);
  ErgoVal __t73 = __t72; ergo_retain_val(__t73);
  ergo_move_into(&ergo_g_main_reset_input, __t72);
  ergo_release_val(__t73);
  ErgoVal __t74 = EV_BOOL(false);
  ErgoVal __t75 = __t74; ergo_retain_val(__t75);
  ergo_move_into(&ergo_g_main_has_error, __t74);
  ergo_release_val(__t75);
  ergo_main_clear_expression();
  ErgoVal __t76 = EV_NULLV;
  ergo_release_val(__t76);
  ErgoVal __t77 = EV_STR(stdr_str_lit("0"));
  ErgoVal __t78 = EV_NULLV;
  {
    ErgoVal __parts19[1] = { __t77 };
    ErgoStr* __s20 = stdr_str_from_parts(1, __parts19);
    __t78 = EV_STR(__s20);
  }
  ergo_release_val(__t77);
  ergo_main_update_display_value(__t78);
  ergo_release_val(__t78);
  ErgoVal __t79 = EV_NULLV;
  ergo_release_val(__t79);
}

static void ergo_main_commit_pending(ErgoVal a0) {
  ErgoVal op__5 = EV_NULLV;
  ErgoVal __t80 = ergo_g_main_pending_op; ergo_retain_val(__t80);
  ergo_move_into(&op__5, __t80);
  ErgoVal __t81 = op__5; ergo_retain_val(__t81);
  ErgoVal __t82 = EV_STR(stdr_str_lit(""));
  ErgoVal __t83 = ergo_eq(__t81, __t82);
  ergo_release_val(__t81);
  ergo_release_val(__t82);
  bool __b6 = ergo_as_bool(__t83);
  ergo_release_val(__t83);
  if (__b6) {
    return;
  }
  ErgoVal __t84 = ergo_g_main_reset_input; ergo_retain_val(__t84);
  bool __b7 = ergo_as_bool(__t84);
  ergo_release_val(__t84);
  if (__b7) {
    return;
  }
  ErgoVal lhs__8 = EV_NULLV;
  ErgoVal __t85 = ergo_g_main_stored_value; ergo_retain_val(__t85);
  ergo_move_into(&lhs__8, __t85);
  ErgoVal rhs__9 = EV_NULLV;
  ErgoVal __t86 = ergo_g_main_current_value; ergo_retain_val(__t86);
  ergo_move_into(&rhs__9, __t86);
  ErgoVal __t87 = op__5; ergo_retain_val(__t87);
  ErgoVal __t88 = EV_STR(stdr_str_lit("/"));
  ErgoVal __t89 = EV_NULLV;
  {
    ErgoVal __parts21[1] = { __t88 };
    ErgoStr* __s22 = stdr_str_from_parts(1, __parts21);
    __t89 = EV_STR(__s22);
  }
  ergo_release_val(__t88);
  ErgoVal __t90 = ergo_eq(__t87, __t89);
  ergo_release_val(__t87);
  ergo_release_val(__t89);
  ErgoVal __t91 = EV_BOOL(false);
  if (ergo_as_bool(__t90)) {
    ErgoVal __t92 = rhs__9; ergo_retain_val(__t92);
    ErgoVal __t93 = EV_INT(0);
    ErgoVal __t94 = ergo_eq(__t92, __t93);
    ergo_release_val(__t92);
    ergo_release_val(__t93);
    ergo_move_into(&__t91, EV_BOOL(ergo_as_bool(__t94)));
    ergo_release_val(__t94);
  } else {
    ergo_move_into(&__t91, EV_BOOL(false));
  }
  ergo_release_val(__t90);
  bool __b10 = ergo_as_bool(__t91);
  ergo_release_val(__t91);
  if (__b10) {
    ErgoVal __t95 = EV_BOOL(true);
    ErgoVal __t96 = __t95; ergo_retain_val(__t96);
    ergo_move_into(&ergo_g_main_has_error, __t95);
    ergo_release_val(__t96);
    ErgoVal __t97 = EV_STR(stdr_str_lit(""));
    ErgoVal __t98 = __t97; ergo_retain_val(__t98);
    ergo_move_into(&ergo_g_main_pending_op, __t97);
    ergo_release_val(__t98);
    ErgoVal __t99 = EV_BOOL(true);
    ErgoVal __t100 = __t99; ergo_retain_val(__t100);
    ergo_move_into(&ergo_g_main_reset_input, __t99);
    ergo_release_val(__t100);
    ergo_main_clear_expression();
    ErgoVal __t101 = EV_NULLV;
    ergo_release_val(__t101);
    ErgoVal __t102 = EV_STR(stdr_str_lit("Error"));
    ErgoVal __t103 = EV_NULLV;
    {
      ErgoVal __parts23[1] = { __t102 };
      ErgoStr* __s24 = stdr_str_from_parts(1, __parts23);
      __t103 = EV_STR(__s24);
    }
    ergo_release_val(__t102);
    ergo_main_update_display_value(__t103);
    ergo_release_val(__t103);
    ErgoVal __t104 = EV_NULLV;
    ergo_release_val(__t104);
    return;
  }
  ErgoVal result__11 = EV_NULLV;
  ErgoVal __t105 = EV_NULLV;
  ErgoVal __t106 = op__5; ergo_retain_val(__t106);
  ErgoVal __t107 = EV_STR(stdr_str_lit("+"));
  ErgoVal __t108 = EV_NULLV;
  {
    ErgoVal __parts25[1] = { __t107 };
    ErgoStr* __s26 = stdr_str_from_parts(1, __parts25);
    __t108 = EV_STR(__s26);
  }
  ergo_release_val(__t107);
  ErgoVal __t109 = ergo_eq(__t106, __t108);
  ergo_release_val(__t106);
  ergo_release_val(__t108);
  bool __b12 = ergo_as_bool(__t109);
  ergo_release_val(__t109);
  if (__b12) {
    ErgoVal __t110 = lhs__8; ergo_retain_val(__t110);
    ErgoVal __t111 = rhs__9; ergo_retain_val(__t111);
    ErgoVal __t112 = ergo_add(__t110, __t111);
    ergo_release_val(__t110);
    ergo_release_val(__t111);
    ergo_move_into(&__t105, __t112);
  } else {
    ErgoVal __t113 = op__5; ergo_retain_val(__t113);
    ErgoVal __t114 = EV_STR(stdr_str_lit("-"));
    ErgoVal __t115 = EV_NULLV;
    {
      ErgoVal __parts27[1] = { __t114 };
      ErgoStr* __s28 = stdr_str_from_parts(1, __parts27);
      __t115 = EV_STR(__s28);
    }
    ergo_release_val(__t114);
    ErgoVal __t116 = ergo_eq(__t113, __t115);
    ergo_release_val(__t113);
    ergo_release_val(__t115);
    bool __b13 = ergo_as_bool(__t116);
    ergo_release_val(__t116);
    if (__b13) {
      ErgoVal __t117 = lhs__8; ergo_retain_val(__t117);
      ErgoVal __t118 = rhs__9; ergo_retain_val(__t118);
      ErgoVal __t119 = ergo_sub(__t117, __t118);
      ergo_release_val(__t117);
      ergo_release_val(__t118);
      ergo_move_into(&__t105, __t119);
    } else {
      ErgoVal __t120 = op__5; ergo_retain_val(__t120);
      ErgoVal __t121 = EV_STR(stdr_str_lit("*"));
      ErgoVal __t122 = EV_NULLV;
      {
        ErgoVal __parts29[1] = { __t121 };
        ErgoStr* __s30 = stdr_str_from_parts(1, __parts29);
        __t122 = EV_STR(__s30);
      }
      ergo_release_val(__t121);
      ErgoVal __t123 = ergo_eq(__t120, __t122);
      ergo_release_val(__t120);
      ergo_release_val(__t122);
      bool __b14 = ergo_as_bool(__t123);
      ergo_release_val(__t123);
      if (__b14) {
        ErgoVal __t124 = lhs__8; ergo_retain_val(__t124);
        ErgoVal __t125 = rhs__9; ergo_retain_val(__t125);
        ErgoVal __t126 = ergo_mul(__t124, __t125);
        ergo_release_val(__t124);
        ergo_release_val(__t125);
        ergo_move_into(&__t105, __t126);
      } else {
        ErgoVal __t127 = lhs__8; ergo_retain_val(__t127);
        ErgoVal __t128 = rhs__9; ergo_retain_val(__t128);
        ErgoVal __t129 = ergo_div(__t127, __t128);
        ergo_release_val(__t127);
        ergo_release_val(__t128);
        ergo_move_into(&__t105, __t129);
      }
    }
  }
  ergo_move_into(&result__11, __t105);
  ErgoVal __t130 = result__11; ergo_retain_val(__t130);
  ErgoVal __t131 = __t130; ergo_retain_val(__t131);
  ergo_move_into(&ergo_g_main_current_value, __t130);
  ergo_release_val(__t131);
  ErgoVal __t132 = result__11; ergo_retain_val(__t132);
  ErgoVal __t133 = __t132; ergo_retain_val(__t133);
  ergo_move_into(&ergo_g_main_stored_value, __t132);
  ergo_release_val(__t133);
  ErgoVal __t134 = lhs__8; ergo_retain_val(__t134);
  ErgoVal __t135 = EV_STR(stdr_to_string(__t134));
  ergo_release_val(__t134);
  ErgoVal __t136 = op__5; ergo_retain_val(__t136);
  ErgoVal __t137 = rhs__9; ergo_retain_val(__t137);
  ErgoVal __t138 = EV_STR(stdr_to_string(__t137));
  ergo_release_val(__t137);
  ErgoVal __t139 = a0; ergo_retain_val(__t139);
  ergo_main_set_expression(__t135, __t136, __t138, __t139);
  ergo_release_val(__t135);
  ergo_release_val(__t136);
  ergo_release_val(__t138);
  ergo_release_val(__t139);
  ErgoVal __t140 = EV_NULLV;
  ergo_release_val(__t140);
  ErgoVal __t141 = result__11; ergo_retain_val(__t141);
  ErgoVal __t142 = EV_STR(stdr_to_string(__t141));
  ergo_release_val(__t141);
  ergo_main_update_display_value(__t142);
  ergo_release_val(__t142);
  ErgoVal __t143 = EV_NULLV;
  ergo_release_val(__t143);
  ergo_release_val(result__11);
  ergo_release_val(rhs__9);
  ergo_release_val(lhs__8);
  ergo_release_val(op__5);
}

static void ergo_main_input_digit(ErgoVal a0) {
  ErgoVal __t144 = ergo_g_main_has_error; ergo_retain_val(__t144);
  bool __b15 = ergo_as_bool(__t144);
  ergo_release_val(__t144);
  if (__b15) {
    ergo_main_clear_all();
    ErgoVal __t145 = EV_NULLV;
    ergo_release_val(__t145);
  }
  ErgoVal next__16 = EV_NULLV;
  ErgoVal __t146 = ergo_g_main_current_value; ergo_retain_val(__t146);
  ergo_move_into(&next__16, __t146);
  ErgoVal __t147 = ergo_g_main_reset_input; ergo_retain_val(__t147);
  bool __b17 = ergo_as_bool(__t147);
  ergo_release_val(__t147);
  if (__b17) {
    ErgoVal __t148 = a0; ergo_retain_val(__t148);
    ErgoVal __t149 = __t148; ergo_retain_val(__t149);
    ergo_move_into(&next__16, __t148);
    ergo_release_val(__t149);
    ErgoVal __t150 = EV_BOOL(false);
    ErgoVal __t151 = __t150; ergo_retain_val(__t151);
    ergo_move_into(&ergo_g_main_reset_input, __t150);
    ergo_release_val(__t151);
  } else {
    ErgoVal __t152 = EV_INT(10);
    ErgoVal __t153 = ergo_mul(next__16, __t152);
    ergo_retain_val(__t153);
    ergo_move_into(&next__16, __t153);
    ergo_release_val(__t152);
    ergo_release_val(__t153);
    ErgoVal __t154 = a0; ergo_retain_val(__t154);
    ErgoVal __t155 = ergo_add(next__16, __t154);
    ergo_retain_val(__t155);
    ergo_move_into(&next__16, __t155);
    ergo_release_val(__t154);
    ergo_release_val(__t155);
  }
  ErgoVal __t156 = next__16; ergo_retain_val(__t156);
  ErgoVal __t157 = __t156; ergo_retain_val(__t157);
  ergo_move_into(&ergo_g_main_current_value, __t156);
  ergo_release_val(__t157);
  ErgoVal __t158 = ergo_g_main_pending_op; ergo_retain_val(__t158);
  ErgoVal __t159 = EV_STR(stdr_str_lit(""));
  ErgoVal __t160 = ergo_ne(__t158, __t159);
  ergo_release_val(__t158);
  ergo_release_val(__t159);
  bool __b18 = ergo_as_bool(__t160);
  ergo_release_val(__t160);
  if (__b18) {
    ErgoVal __t161 = ergo_g_main_stored_value; ergo_retain_val(__t161);
    ErgoVal __t162 = EV_STR(stdr_to_string(__t161));
    ergo_release_val(__t161);
    ErgoVal __t163 = ergo_g_main_pending_op; ergo_retain_val(__t163);
    ErgoVal __t164 = next__16; ergo_retain_val(__t164);
    ErgoVal __t165 = EV_STR(stdr_to_string(__t164));
    ergo_release_val(__t164);
    ErgoVal __t166 = EV_BOOL(false);
    ergo_main_set_expression(__t162, __t163, __t165, __t166);
    ergo_release_val(__t162);
    ergo_release_val(__t163);
    ergo_release_val(__t165);
    ergo_release_val(__t166);
    ErgoVal __t167 = EV_NULLV;
    ergo_release_val(__t167);
  } else {
    ergo_main_clear_expression();
    ErgoVal __t168 = EV_NULLV;
    ergo_release_val(__t168);
  }
  ErgoVal __t169 = next__16; ergo_retain_val(__t169);
  ErgoVal __t170 = EV_STR(stdr_to_string(__t169));
  ergo_release_val(__t169);
  ergo_main_update_display_value(__t170);
  ergo_release_val(__t170);
  ErgoVal __t171 = EV_NULLV;
  ergo_release_val(__t171);
  ergo_release_val(next__16);
}

static void ergo_main_choose_operator(ErgoVal a0) {
  ErgoVal __t172 = ergo_g_main_has_error; ergo_retain_val(__t172);
  bool __b19 = ergo_as_bool(__t172);
  ergo_release_val(__t172);
  if (__b19) {
    ergo_main_clear_all();
    ErgoVal __t173 = EV_NULLV;
    ergo_release_val(__t173);
  }
  ErgoVal __t174 = ergo_g_main_pending_op; ergo_retain_val(__t174);
  ErgoVal __t175 = EV_STR(stdr_str_lit(""));
  ErgoVal __t176 = ergo_ne(__t174, __t175);
  ergo_release_val(__t174);
  ergo_release_val(__t175);
  ErgoVal __t177 = EV_BOOL(false);
  if (ergo_as_bool(__t176)) {
    ErgoVal __t178 = ergo_g_main_reset_input; ergo_retain_val(__t178);
    ErgoVal __t179 = EV_BOOL(!ergo_as_bool(__t178));
    ergo_release_val(__t178);
    ergo_move_into(&__t177, EV_BOOL(ergo_as_bool(__t179)));
    ergo_release_val(__t179);
  } else {
    ergo_move_into(&__t177, EV_BOOL(false));
  }
  ergo_release_val(__t176);
  bool __b20 = ergo_as_bool(__t177);
  ergo_release_val(__t177);
  if (__b20) {
    ErgoVal __t180 = EV_BOOL(false);
    ergo_main_commit_pending(__t180);
    ergo_release_val(__t180);
    ErgoVal __t181 = EV_NULLV;
    ergo_release_val(__t181);
    ErgoVal __t182 = ergo_g_main_has_error; ergo_retain_val(__t182);
    bool __b21 = ergo_as_bool(__t182);
    ergo_release_val(__t182);
    if (__b21) {
      return;
    }
  } else {
    ErgoVal __t183 = ergo_g_main_current_value; ergo_retain_val(__t183);
    ErgoVal __t184 = __t183; ergo_retain_val(__t184);
    ergo_move_into(&ergo_g_main_stored_value, __t183);
    ergo_release_val(__t184);
  }
  ErgoVal __t185 = a0; ergo_retain_val(__t185);
  ErgoVal __t186 = __t185; ergo_retain_val(__t186);
  ergo_move_into(&ergo_g_main_pending_op, __t185);
  ergo_release_val(__t186);
  ErgoVal __t187 = EV_BOOL(true);
  ErgoVal __t188 = __t187; ergo_retain_val(__t188);
  ergo_move_into(&ergo_g_main_reset_input, __t187);
  ergo_release_val(__t188);
  ErgoVal __t189 = ergo_g_main_stored_value; ergo_retain_val(__t189);
  ErgoVal __t190 = EV_STR(stdr_to_string(__t189));
  ergo_release_val(__t189);
  ErgoVal __t191 = ergo_g_main_pending_op; ergo_retain_val(__t191);
  ErgoVal __t192 = EV_STR(stdr_str_lit(""));
  ErgoVal __t193 = EV_BOOL(false);
  ergo_main_set_expression(__t190, __t191, __t192, __t193);
  ergo_release_val(__t190);
  ergo_release_val(__t191);
  ergo_release_val(__t192);
  ergo_release_val(__t193);
  ErgoVal __t194 = EV_NULLV;
  ergo_release_val(__t194);
  ErgoVal __t195 = ergo_g_main_current_value; ergo_retain_val(__t195);
  ErgoVal __t196 = EV_STR(stdr_to_string(__t195));
  ergo_release_val(__t195);
  ergo_main_update_display_value(__t196);
  ergo_release_val(__t196);
  ErgoVal __t197 = EV_NULLV;
  ergo_release_val(__t197);
}

static void ergo_main_evaluate(void) {
  ErgoVal __t198 = ergo_g_main_has_error; ergo_retain_val(__t198);
  bool __b22 = ergo_as_bool(__t198);
  ergo_release_val(__t198);
  if (__b22) {
    ergo_main_clear_all();
    ErgoVal __t199 = EV_NULLV;
    ergo_release_val(__t199);
    return;
  }
  ErgoVal __t200 = ergo_g_main_pending_op; ergo_retain_val(__t200);
  ErgoVal __t201 = EV_STR(stdr_str_lit(""));
  ErgoVal __t202 = ergo_eq(__t200, __t201);
  ergo_release_val(__t200);
  ergo_release_val(__t201);
  bool __b23 = ergo_as_bool(__t202);
  ergo_release_val(__t202);
  if (__b23) {
    return;
  }
  ErgoVal __t203 = EV_BOOL(true);
  ergo_main_commit_pending(__t203);
  ergo_release_val(__t203);
  ErgoVal __t204 = EV_NULLV;
  ergo_release_val(__t204);
  ErgoVal __t205 = ergo_g_main_has_error; ergo_retain_val(__t205);
  bool __b24 = ergo_as_bool(__t205);
  ergo_release_val(__t205);
  if (__b24) {
    return;
  }
  ErgoVal __t206 = EV_STR(stdr_str_lit(""));
  ErgoVal __t207 = __t206; ergo_retain_val(__t207);
  ergo_move_into(&ergo_g_main_pending_op, __t206);
  ergo_release_val(__t207);
  ErgoVal __t208 = EV_BOOL(true);
  ErgoVal __t209 = __t208; ergo_retain_val(__t209);
  ergo_move_into(&ergo_g_main_reset_input, __t208);
  ergo_release_val(__t209);
}

static ErgoVal ergo_main_digit_button(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal btn__25 = EV_NULLV;
  ErgoVal __t210 = a0; ergo_retain_val(__t210);
  ErgoVal __t211 = EV_STR(stdr_to_string(__t210));
  ergo_release_val(__t210);
  ErgoVal __t212 = ergo_cogito_button(__t211);
  ergo_release_val(__t211);
  ergo_move_into(&btn__25, __t212);
  ErgoVal __t213 = btn__25; ergo_retain_val(__t213);
  ErgoVal* __env31 = (ErgoVal*)malloc(sizeof(ErgoVal) * 1);
  __env31[0] = a0; ergo_retain_val(__env31[0]);
  ErgoVal __t214 = EV_FN(ergo_fn_new_with_env(ergo_lambda_1, 1, __env31, 1));
  ergo_m_cogito_Button_on_click(__t213, __t214);
  ergo_release_val(__t213);
  ergo_release_val(__t214);
  ErgoVal __t215 = EV_NULLV;
  ergo_release_val(__t215);
  ErgoVal __t216 = btn__25; ergo_retain_val(__t216);
  ergo_move_into(&__ret, __t216);
  return __ret;
  ergo_release_val(btn__25);
  return __ret;
}

static ErgoVal ergo_main_operator_button(ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal btn__26 = EV_NULLV;
  ErgoVal __t217 = a0; ergo_retain_val(__t217);
  ErgoVal __t218 = ergo_cogito_button(__t217);
  ergo_release_val(__t217);
  ergo_move_into(&btn__26, __t218);
  ErgoVal __t219 = btn__26; ergo_retain_val(__t219);
  ErgoVal __t220 = EV_STR(stdr_str_lit("outlined"));
  ErgoVal __t221 = EV_NULLV;
  {
    ErgoVal __parts32[1] = { __t220 };
    ErgoStr* __s33 = stdr_str_from_parts(1, __parts32);
    __t221 = EV_STR(__s33);
  }
  ergo_release_val(__t220);
  ergo_cogito_set_class(__t219, __t221);
  ergo_release_val(__t219);
  ergo_release_val(__t221);
  ErgoVal __t222 = EV_NULLV;
  ergo_release_val(__t222);
  ErgoVal __t223 = btn__26; ergo_retain_val(__t223);
  ErgoVal* __env34 = (ErgoVal*)malloc(sizeof(ErgoVal) * 1);
  __env34[0] = a1; ergo_retain_val(__env34[0]);
  ErgoVal __t224 = EV_FN(ergo_fn_new_with_env(ergo_lambda_2, 1, __env34, 1));
  ergo_m_cogito_Button_on_click(__t223, __t224);
  ergo_release_val(__t223);
  ergo_release_val(__t224);
  ErgoVal __t225 = EV_NULLV;
  ergo_release_val(__t225);
  ErgoVal __t226 = btn__26; ergo_retain_val(__t226);
  ergo_move_into(&__ret, __t226);
  return __ret;
  ergo_release_val(btn__26);
  return __ret;
}

static ErgoVal ergo_main_clear_button(void) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal btn__27 = EV_NULLV;
  ErgoVal __t227 = EV_STR(stdr_str_lit("C"));
  ErgoVal __t228 = EV_NULLV;
  {
    ErgoVal __parts35[1] = { __t227 };
    ErgoStr* __s36 = stdr_str_from_parts(1, __parts35);
    __t228 = EV_STR(__s36);
  }
  ergo_release_val(__t227);
  ErgoVal __t229 = ergo_cogito_button(__t228);
  ergo_release_val(__t228);
  ergo_move_into(&btn__27, __t229);
  ErgoVal __t230 = btn__27; ergo_retain_val(__t230);
  ErgoVal __t231 = EV_STR(stdr_str_lit("text"));
  ErgoVal __t232 = EV_NULLV;
  {
    ErgoVal __parts37[1] = { __t231 };
    ErgoStr* __s38 = stdr_str_from_parts(1, __parts37);
    __t232 = EV_STR(__s38);
  }
  ergo_release_val(__t231);
  ergo_cogito_set_class(__t230, __t232);
  ergo_release_val(__t230);
  ergo_release_val(__t232);
  ErgoVal __t233 = EV_NULLV;
  ergo_release_val(__t233);
  ErgoVal __t234 = btn__27; ergo_retain_val(__t234);
  ErgoVal __t235 = EV_FN(ergo_fn_new(ergo_lambda_3, 1));
  ergo_m_cogito_Button_on_click(__t234, __t235);
  ergo_release_val(__t234);
  ergo_release_val(__t235);
  ErgoVal __t236 = EV_NULLV;
  ergo_release_val(__t236);
  ErgoVal __t237 = btn__27; ergo_retain_val(__t237);
  ergo_move_into(&__ret, __t237);
  return __ret;
  ergo_release_val(btn__27);
  return __ret;
}

static ErgoVal ergo_main_equals_button(void) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal btn__28 = EV_NULLV;
  ErgoVal __t238 = EV_STR(stdr_str_lit("="));
  ErgoVal __t239 = EV_NULLV;
  {
    ErgoVal __parts39[1] = { __t238 };
    ErgoStr* __s40 = stdr_str_from_parts(1, __parts39);
    __t239 = EV_STR(__s40);
  }
  ergo_release_val(__t238);
  ErgoVal __t240 = ergo_cogito_button(__t239);
  ergo_release_val(__t239);
  ergo_move_into(&btn__28, __t240);
  ErgoVal __t241 = btn__28; ergo_retain_val(__t241);
  ErgoVal __t242 = EV_STR(stdr_str_lit("calc-equals"));
  ErgoVal __t243 = EV_NULLV;
  {
    ErgoVal __parts41[1] = { __t242 };
    ErgoStr* __s42 = stdr_str_from_parts(1, __parts41);
    __t243 = EV_STR(__s42);
  }
  ergo_release_val(__t242);
  ergo_cogito_set_class(__t241, __t243);
  ergo_release_val(__t241);
  ergo_release_val(__t243);
  ErgoVal __t244 = EV_NULLV;
  ergo_release_val(__t244);
  ErgoVal __t245 = btn__28; ergo_retain_val(__t245);
  ErgoVal __t246 = EV_FN(ergo_fn_new(ergo_lambda_4, 1));
  ergo_m_cogito_Button_on_click(__t245, __t246);
  ergo_release_val(__t245);
  ergo_release_val(__t246);
  ErgoVal __t247 = EV_NULLV;
  ergo_release_val(__t247);
  ErgoVal __t248 = btn__28; ergo_retain_val(__t248);
  ergo_move_into(&__ret, __t248);
  return __ret;
  ergo_release_val(btn__28);
  return __ret;
}

static ErgoVal ergo_main_spacer(void) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal s__29 = EV_NULLV;
  ErgoVal __t249 = EV_STR(stdr_str_lit(""));
  ErgoVal __t250 = ergo_cogito_label(__t249);
  ergo_release_val(__t249);
  ergo_move_into(&s__29, __t250);
  ErgoVal __t251 = s__29; ergo_retain_val(__t251);
  ErgoVal __t252 = EV_STR(stdr_str_lit("calc-spacer"));
  ErgoVal __t253 = EV_NULLV;
  {
    ErgoVal __parts43[1] = { __t252 };
    ErgoStr* __s44 = stdr_str_from_parts(1, __parts43);
    __t253 = EV_STR(__s44);
  }
  ergo_release_val(__t252);
  ergo_cogito_set_class(__t251, __t253);
  ergo_release_val(__t251);
  ergo_release_val(__t253);
  ErgoVal __t254 = EV_NULLV;
  ergo_release_val(__t254);
  ErgoVal __t255 = s__29; ergo_retain_val(__t255);
  ergo_move_into(&__ret, __t255);
  return __ret;
  ergo_release_val(s__29);
  return __ret;
}

static ErgoVal ergo_main_aton(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal result__30 = EV_NULLV;
  ErgoVal __t256 = EV_FLOAT(0);
  ergo_move_into(&result__30, __t256);
  ErgoVal i__31 = EV_NULLV;
  ErgoVal __t257 = EV_INT(0);
  ergo_move_into(&i__31, __t257);
  ErgoVal neg__32 = EV_NULLV;
  ErgoVal __t258 = EV_BOOL(false);
  ergo_move_into(&neg__32, __t258);
  ErgoVal slen__33 = EV_NULLV;
  ErgoVal __t259 = a0; ergo_retain_val(__t259);
  ErgoVal __t260 = ergo_stdr_len(__t259);
  ergo_release_val(__t259);
  ergo_move_into(&slen__33, __t260);
  ErgoVal __t261 = slen__33; ergo_retain_val(__t261);
  ErgoVal __t262 = EV_INT(0);
  ErgoVal __t263 = ergo_eq(__t261, __t262);
  ergo_release_val(__t261);
  ergo_release_val(__t262);
  bool __b34 = ergo_as_bool(__t263);
  ergo_release_val(__t263);
  if (__b34) {
    ErgoVal __t264 = EV_FLOAT(0);
    ergo_move_into(&__ret, __t264);
    return __ret;
  }
  ErgoVal __t265 = a0; ergo_retain_val(__t265);
  ErgoVal __t266 = EV_INT(0);
  ErgoVal __t267 = stdr_str_at(__t265, ergo_as_int(__t266));
  ergo_release_val(__t265);
  ergo_release_val(__t266);
  ErgoVal __t268 = EV_STR(stdr_str_lit("-"));
  ErgoVal __t269 = EV_NULLV;
  {
    ErgoVal __parts45[1] = { __t268 };
    ErgoStr* __s46 = stdr_str_from_parts(1, __parts45);
    __t269 = EV_STR(__s46);
  }
  ergo_release_val(__t268);
  ErgoVal __t270 = ergo_eq(__t267, __t269);
  ergo_release_val(__t267);
  ergo_release_val(__t269);
  bool __b35 = ergo_as_bool(__t270);
  ergo_release_val(__t270);
  if (__b35) {
    ErgoVal __t271 = EV_BOOL(true);
    ErgoVal __t272 = __t271; ergo_retain_val(__t272);
    ergo_move_into(&neg__32, __t271);
    ergo_release_val(__t272);
    ErgoVal __t273 = EV_INT(1);
    ErgoVal __t274 = __t273; ergo_retain_val(__t274);
    ergo_move_into(&i__31, __t273);
    ergo_release_val(__t274);
  }
  ErgoVal frac__36 = EV_NULLV;
  ErgoVal __t275 = EV_BOOL(false);
  ergo_move_into(&frac__36, __t275);
  ErgoVal frac_div__37 = EV_NULLV;
  ErgoVal __t276 = EV_FLOAT(1);
  ergo_move_into(&frac_div__37, __t276);
  ErgoVal done__38 = EV_NULLV;
  ErgoVal __t277 = EV_BOOL(false);
  ergo_move_into(&done__38, __t277);
  for (;;) {
    ErgoVal __t278 = i__31; ergo_retain_val(__t278);
    ErgoVal __t279 = slen__33; ergo_retain_val(__t279);
    ErgoVal __t280 = ergo_lt(__t278, __t279);
    ergo_release_val(__t278);
    ergo_release_val(__t279);
    ErgoVal __t281 = EV_BOOL(false);
    if (ergo_as_bool(__t280)) {
      ErgoVal __t282 = done__38; ergo_retain_val(__t282);
      ErgoVal __t283 = EV_BOOL(!ergo_as_bool(__t282));
      ergo_release_val(__t282);
      ergo_move_into(&__t281, EV_BOOL(ergo_as_bool(__t283)));
      ergo_release_val(__t283);
    } else {
      ergo_move_into(&__t281, EV_BOOL(false));
    }
    ergo_release_val(__t280);
    bool __b39 = ergo_as_bool(__t281);
    ergo_release_val(__t281);
    if (!__b39) {
      break;
    }
    ErgoVal c__40 = EV_NULLV;
    ErgoVal __t284 = a0; ergo_retain_val(__t284);
    ErgoVal __t285 = i__31; ergo_retain_val(__t285);
    ErgoVal __t286 = stdr_str_at(__t284, ergo_as_int(__t285));
    ergo_release_val(__t284);
    ergo_release_val(__t285);
    ergo_move_into(&c__40, __t286);
    ErgoVal __t287 = c__40; ergo_retain_val(__t287);
    ErgoVal __t288 = EV_STR(stdr_str_lit("."));
    ErgoVal __t289 = EV_NULLV;
    {
      ErgoVal __parts48[1] = { __t288 };
      ErgoStr* __s49 = stdr_str_from_parts(1, __parts48);
      __t289 = EV_STR(__s49);
    }
    ergo_release_val(__t288);
    ErgoVal __t290 = ergo_eq(__t287, __t289);
    ergo_release_val(__t287);
    ergo_release_val(__t289);
    bool __b41 = ergo_as_bool(__t290);
    ergo_release_val(__t290);
    if (__b41) {
      ErgoVal __t291 = EV_BOOL(true);
      ErgoVal __t292 = __t291; ergo_retain_val(__t292);
      ergo_move_into(&frac__36, __t291);
      ergo_release_val(__t292);
    } else {
      ErgoVal d__42 = EV_NULLV;
      ErgoVal __t293 = EV_INT(0);
      ErgoVal __t294 = EV_INT(1);
      ErgoVal __t295 = ergo_sub(__t293, __t294);
      ergo_release_val(__t293);
      ergo_release_val(__t294);
      ergo_move_into(&d__42, __t295);
      ErgoVal __t296 = c__40; ergo_retain_val(__t296);
      ErgoVal __t297 = EV_STR(stdr_str_lit("0"));
      ErgoVal __t298 = EV_NULLV;
      {
        ErgoVal __parts50[1] = { __t297 };
        ErgoStr* __s51 = stdr_str_from_parts(1, __parts50);
        __t298 = EV_STR(__s51);
      }
      ergo_release_val(__t297);
      ErgoVal __t299 = ergo_eq(__t296, __t298);
      ergo_release_val(__t296);
      ergo_release_val(__t298);
      bool __b43 = ergo_as_bool(__t299);
      ergo_release_val(__t299);
      if (__b43) {
        ErgoVal __t300 = EV_INT(0);
        ErgoVal __t301 = __t300; ergo_retain_val(__t301);
        ergo_move_into(&d__42, __t300);
        ergo_release_val(__t301);
      } else {
        ErgoVal __t302 = c__40; ergo_retain_val(__t302);
        ErgoVal __t303 = EV_STR(stdr_str_lit("1"));
        ErgoVal __t304 = EV_NULLV;
        {
          ErgoVal __parts52[1] = { __t303 };
          ErgoStr* __s53 = stdr_str_from_parts(1, __parts52);
          __t304 = EV_STR(__s53);
        }
        ergo_release_val(__t303);
        ErgoVal __t305 = ergo_eq(__t302, __t304);
        ergo_release_val(__t302);
        ergo_release_val(__t304);
        bool __b44 = ergo_as_bool(__t305);
        ergo_release_val(__t305);
        if (__b44) {
          ErgoVal __t306 = EV_INT(1);
          ErgoVal __t307 = __t306; ergo_retain_val(__t307);
          ergo_move_into(&d__42, __t306);
          ergo_release_val(__t307);
        } else {
          ErgoVal __t308 = c__40; ergo_retain_val(__t308);
          ErgoVal __t309 = EV_STR(stdr_str_lit("2"));
          ErgoVal __t310 = EV_NULLV;
          {
            ErgoVal __parts54[1] = { __t309 };
            ErgoStr* __s55 = stdr_str_from_parts(1, __parts54);
            __t310 = EV_STR(__s55);
          }
          ergo_release_val(__t309);
          ErgoVal __t311 = ergo_eq(__t308, __t310);
          ergo_release_val(__t308);
          ergo_release_val(__t310);
          bool __b45 = ergo_as_bool(__t311);
          ergo_release_val(__t311);
          if (__b45) {
            ErgoVal __t312 = EV_INT(2);
            ErgoVal __t313 = __t312; ergo_retain_val(__t313);
            ergo_move_into(&d__42, __t312);
            ergo_release_val(__t313);
          } else {
            ErgoVal __t314 = c__40; ergo_retain_val(__t314);
            ErgoVal __t315 = EV_STR(stdr_str_lit("3"));
            ErgoVal __t316 = EV_NULLV;
            {
              ErgoVal __parts56[1] = { __t315 };
              ErgoStr* __s57 = stdr_str_from_parts(1, __parts56);
              __t316 = EV_STR(__s57);
            }
            ergo_release_val(__t315);
            ErgoVal __t317 = ergo_eq(__t314, __t316);
            ergo_release_val(__t314);
            ergo_release_val(__t316);
            bool __b46 = ergo_as_bool(__t317);
            ergo_release_val(__t317);
            if (__b46) {
              ErgoVal __t318 = EV_INT(3);
              ErgoVal __t319 = __t318; ergo_retain_val(__t319);
              ergo_move_into(&d__42, __t318);
              ergo_release_val(__t319);
            } else {
              ErgoVal __t320 = c__40; ergo_retain_val(__t320);
              ErgoVal __t321 = EV_STR(stdr_str_lit("4"));
              ErgoVal __t322 = EV_NULLV;
              {
                ErgoVal __parts58[1] = { __t321 };
                ErgoStr* __s59 = stdr_str_from_parts(1, __parts58);
                __t322 = EV_STR(__s59);
              }
              ergo_release_val(__t321);
              ErgoVal __t323 = ergo_eq(__t320, __t322);
              ergo_release_val(__t320);
              ergo_release_val(__t322);
              bool __b47 = ergo_as_bool(__t323);
              ergo_release_val(__t323);
              if (__b47) {
                ErgoVal __t324 = EV_INT(4);
                ErgoVal __t325 = __t324; ergo_retain_val(__t325);
                ergo_move_into(&d__42, __t324);
                ergo_release_val(__t325);
              } else {
                ErgoVal __t326 = c__40; ergo_retain_val(__t326);
                ErgoVal __t327 = EV_STR(stdr_str_lit("5"));
                ErgoVal __t328 = EV_NULLV;
                {
                  ErgoVal __parts60[1] = { __t327 };
                  ErgoStr* __s61 = stdr_str_from_parts(1, __parts60);
                  __t328 = EV_STR(__s61);
                }
                ergo_release_val(__t327);
                ErgoVal __t329 = ergo_eq(__t326, __t328);
                ergo_release_val(__t326);
                ergo_release_val(__t328);
                bool __b48 = ergo_as_bool(__t329);
                ergo_release_val(__t329);
                if (__b48) {
                  ErgoVal __t330 = EV_INT(5);
                  ErgoVal __t331 = __t330; ergo_retain_val(__t331);
                  ergo_move_into(&d__42, __t330);
                  ergo_release_val(__t331);
                } else {
                  ErgoVal __t332 = c__40; ergo_retain_val(__t332);
                  ErgoVal __t333 = EV_STR(stdr_str_lit("6"));
                  ErgoVal __t334 = EV_NULLV;
                  {
                    ErgoVal __parts62[1] = { __t333 };
                    ErgoStr* __s63 = stdr_str_from_parts(1, __parts62);
                    __t334 = EV_STR(__s63);
                  }
                  ergo_release_val(__t333);
                  ErgoVal __t335 = ergo_eq(__t332, __t334);
                  ergo_release_val(__t332);
                  ergo_release_val(__t334);
                  bool __b49 = ergo_as_bool(__t335);
                  ergo_release_val(__t335);
                  if (__b49) {
                    ErgoVal __t336 = EV_INT(6);
                    ErgoVal __t337 = __t336; ergo_retain_val(__t337);
                    ergo_move_into(&d__42, __t336);
                    ergo_release_val(__t337);
                  } else {
                    ErgoVal __t338 = c__40; ergo_retain_val(__t338);
                    ErgoVal __t339 = EV_STR(stdr_str_lit("7"));
                    ErgoVal __t340 = EV_NULLV;
                    {
                      ErgoVal __parts64[1] = { __t339 };
                      ErgoStr* __s65 = stdr_str_from_parts(1, __parts64);
                      __t340 = EV_STR(__s65);
                    }
                    ergo_release_val(__t339);
                    ErgoVal __t341 = ergo_eq(__t338, __t340);
                    ergo_release_val(__t338);
                    ergo_release_val(__t340);
                    bool __b50 = ergo_as_bool(__t341);
                    ergo_release_val(__t341);
                    if (__b50) {
                      ErgoVal __t342 = EV_INT(7);
                      ErgoVal __t343 = __t342; ergo_retain_val(__t343);
                      ergo_move_into(&d__42, __t342);
                      ergo_release_val(__t343);
                    } else {
                      ErgoVal __t344 = c__40; ergo_retain_val(__t344);
                      ErgoVal __t345 = EV_STR(stdr_str_lit("8"));
                      ErgoVal __t346 = EV_NULLV;
                      {
                        ErgoVal __parts66[1] = { __t345 };
                        ErgoStr* __s67 = stdr_str_from_parts(1, __parts66);
                        __t346 = EV_STR(__s67);
                      }
                      ergo_release_val(__t345);
                      ErgoVal __t347 = ergo_eq(__t344, __t346);
                      ergo_release_val(__t344);
                      ergo_release_val(__t346);
                      bool __b51 = ergo_as_bool(__t347);
                      ergo_release_val(__t347);
                      if (__b51) {
                        ErgoVal __t348 = EV_INT(8);
                        ErgoVal __t349 = __t348; ergo_retain_val(__t349);
                        ergo_move_into(&d__42, __t348);
                        ergo_release_val(__t349);
                      } else {
                        ErgoVal __t350 = c__40; ergo_retain_val(__t350);
                        ErgoVal __t351 = EV_STR(stdr_str_lit("9"));
                        ErgoVal __t352 = EV_NULLV;
                        {
                          ErgoVal __parts68[1] = { __t351 };
                          ErgoStr* __s69 = stdr_str_from_parts(1, __parts68);
                          __t352 = EV_STR(__s69);
                        }
                        ergo_release_val(__t351);
                        ErgoVal __t353 = ergo_eq(__t350, __t352);
                        ergo_release_val(__t350);
                        ergo_release_val(__t352);
                        bool __b52 = ergo_as_bool(__t353);
                        ergo_release_val(__t353);
                        if (__b52) {
                          ErgoVal __t354 = EV_INT(9);
                          ErgoVal __t355 = __t354; ergo_retain_val(__t355);
                          ergo_move_into(&d__42, __t354);
                          ergo_release_val(__t355);
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
      ErgoVal __t356 = d__42; ergo_retain_val(__t356);
      ErgoVal __t357 = EV_INT(0);
      ErgoVal __t358 = ergo_lt(__t356, __t357);
      ergo_release_val(__t356);
      ergo_release_val(__t357);
      bool __b53 = ergo_as_bool(__t358);
      ergo_release_val(__t358);
      if (__b53) {
        ErgoVal __t359 = EV_BOOL(true);
        ErgoVal __t360 = __t359; ergo_retain_val(__t360);
        ergo_move_into(&done__38, __t359);
        ergo_release_val(__t360);
      } else {
        ErgoVal __t361 = frac__36; ergo_retain_val(__t361);
        bool __b54 = ergo_as_bool(__t361);
        ergo_release_val(__t361);
        if (__b54) {
          ErgoVal __t362 = EV_FLOAT(10);
          ErgoVal __t363 = ergo_mul(frac_div__37, __t362);
          ergo_retain_val(__t363);
          ergo_move_into(&frac_div__37, __t363);
          ergo_release_val(__t362);
          ergo_release_val(__t363);
          ErgoVal __t364 = d__42; ergo_retain_val(__t364);
          ErgoVal __t365 = frac_div__37; ergo_retain_val(__t365);
          ErgoVal __t366 = ergo_div(__t364, __t365);
          ergo_release_val(__t364);
          ergo_release_val(__t365);
          ErgoVal __t367 = ergo_add(result__30, __t366);
          ergo_retain_val(__t367);
          ergo_move_into(&result__30, __t367);
          ergo_release_val(__t366);
          ergo_release_val(__t367);
        } else {
          ErgoVal __t368 = result__30; ergo_retain_val(__t368);
          ErgoVal __t369 = EV_INT(10);
          ErgoVal __t370 = ergo_mul(__t368, __t369);
          ergo_release_val(__t368);
          ergo_release_val(__t369);
          ErgoVal __t371 = d__42; ergo_retain_val(__t371);
          ErgoVal __t372 = ergo_add(__t370, __t371);
          ergo_release_val(__t370);
          ergo_release_val(__t371);
          ErgoVal __t373 = __t372; ergo_retain_val(__t373);
          ergo_move_into(&result__30, __t372);
          ergo_release_val(__t373);
        }
      }
    }
    ergo_release_val(d__42);
    ergo_release_val(c__40);
    __for_continue47: ;
    ErgoVal __t374 = EV_INT(1);
    ErgoVal __t375 = ergo_add(i__31, __t374);
    ergo_retain_val(__t375);
    ergo_move_into(&i__31, __t375);
    ergo_release_val(__t374);
    ergo_release_val(__t375);
  }
  ErgoVal __t376 = neg__32; ergo_retain_val(__t376);
  bool __b55 = ergo_as_bool(__t376);
  ergo_release_val(__t376);
  if (__b55) {
    ErgoVal __t377 = EV_INT(0);
    ErgoVal __t378 = result__30; ergo_retain_val(__t378);
    ErgoVal __t379 = ergo_sub(__t377, __t378);
    ergo_release_val(__t377);
    ergo_release_val(__t378);
    ErgoVal __t380 = __t379; ergo_retain_val(__t380);
    ergo_move_into(&result__30, __t379);
    ergo_release_val(__t380);
  }
  ErgoVal __t381 = result__30; ergo_retain_val(__t381);
  ergo_move_into(&__ret, __t381);
  return __ret;
  ergo_release_val(done__38);
  ergo_release_val(frac_div__37);
  ergo_release_val(frac__36);
  ergo_release_val(slen__33);
  ergo_release_val(neg__32);
  ergo_release_val(i__31);
  ergo_release_val(result__30);
  return __ret;
}

static ErgoVal ergo_main_conv_unit_names(void) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t382 = ergo_g_main_conv_category; ergo_retain_val(__t382);
  ErgoVal __t383 = EV_INT(0);
  ErgoVal __t384 = ergo_eq(__t382, __t383);
  ergo_release_val(__t382);
  ergo_release_val(__t383);
  bool __b56 = ergo_as_bool(__t384);
  ergo_release_val(__t384);
  if (__b56) {
    ErgoArr* __a1 = stdr_arr_new(8);
    ErgoVal __t385 = EV_ARR(__a1);
    ErgoVal __t386 = EV_STR(stdr_str_lit("Meter"));
    ErgoVal __t387 = EV_NULLV;
    {
      ErgoVal __parts70[1] = { __t386 };
      ErgoStr* __s71 = stdr_str_from_parts(1, __parts70);
      __t387 = EV_STR(__s71);
    }
    ergo_release_val(__t386);
    ergo_arr_add(__a1, __t387);
    ErgoVal __t388 = EV_STR(stdr_str_lit("Kilometer"));
    ErgoVal __t389 = EV_NULLV;
    {
      ErgoVal __parts72[1] = { __t388 };
      ErgoStr* __s73 = stdr_str_from_parts(1, __parts72);
      __t389 = EV_STR(__s73);
    }
    ergo_release_val(__t388);
    ergo_arr_add(__a1, __t389);
    ErgoVal __t390 = EV_STR(stdr_str_lit("Centimeter"));
    ErgoVal __t391 = EV_NULLV;
    {
      ErgoVal __parts74[1] = { __t390 };
      ErgoStr* __s75 = stdr_str_from_parts(1, __parts74);
      __t391 = EV_STR(__s75);
    }
    ergo_release_val(__t390);
    ergo_arr_add(__a1, __t391);
    ErgoVal __t392 = EV_STR(stdr_str_lit("Millimeter"));
    ErgoVal __t393 = EV_NULLV;
    {
      ErgoVal __parts76[1] = { __t392 };
      ErgoStr* __s77 = stdr_str_from_parts(1, __parts76);
      __t393 = EV_STR(__s77);
    }
    ergo_release_val(__t392);
    ergo_arr_add(__a1, __t393);
    ErgoVal __t394 = EV_STR(stdr_str_lit("Mile"));
    ErgoVal __t395 = EV_NULLV;
    {
      ErgoVal __parts78[1] = { __t394 };
      ErgoStr* __s79 = stdr_str_from_parts(1, __parts78);
      __t395 = EV_STR(__s79);
    }
    ergo_release_val(__t394);
    ergo_arr_add(__a1, __t395);
    ErgoVal __t396 = EV_STR(stdr_str_lit("Yard"));
    ErgoVal __t397 = EV_NULLV;
    {
      ErgoVal __parts80[1] = { __t396 };
      ErgoStr* __s81 = stdr_str_from_parts(1, __parts80);
      __t397 = EV_STR(__s81);
    }
    ergo_release_val(__t396);
    ergo_arr_add(__a1, __t397);
    ErgoVal __t398 = EV_STR(stdr_str_lit("Foot"));
    ErgoVal __t399 = EV_NULLV;
    {
      ErgoVal __parts82[1] = { __t398 };
      ErgoStr* __s83 = stdr_str_from_parts(1, __parts82);
      __t399 = EV_STR(__s83);
    }
    ergo_release_val(__t398);
    ergo_arr_add(__a1, __t399);
    ErgoVal __t400 = EV_STR(stdr_str_lit("Inch"));
    ErgoVal __t401 = EV_NULLV;
    {
      ErgoVal __parts84[1] = { __t400 };
      ErgoStr* __s85 = stdr_str_from_parts(1, __parts84);
      __t401 = EV_STR(__s85);
    }
    ergo_release_val(__t400);
    ergo_arr_add(__a1, __t401);
    ergo_move_into(&__ret, __t385);
    return __ret;
  } else {
    ErgoVal __t402 = ergo_g_main_conv_category; ergo_retain_val(__t402);
    ErgoVal __t403 = EV_INT(1);
    ErgoVal __t404 = ergo_eq(__t402, __t403);
    ergo_release_val(__t402);
    ergo_release_val(__t403);
    bool __b57 = ergo_as_bool(__t404);
    ergo_release_val(__t404);
    if (__b57) {
      ErgoArr* __a2 = stdr_arr_new(6);
      ErgoVal __t405 = EV_ARR(__a2);
      ErgoVal __t406 = EV_STR(stdr_str_lit("Kilogram"));
      ErgoVal __t407 = EV_NULLV;
      {
        ErgoVal __parts86[1] = { __t406 };
        ErgoStr* __s87 = stdr_str_from_parts(1, __parts86);
        __t407 = EV_STR(__s87);
      }
      ergo_release_val(__t406);
      ergo_arr_add(__a2, __t407);
      ErgoVal __t408 = EV_STR(stdr_str_lit("Gram"));
      ErgoVal __t409 = EV_NULLV;
      {
        ErgoVal __parts88[1] = { __t408 };
        ErgoStr* __s89 = stdr_str_from_parts(1, __parts88);
        __t409 = EV_STR(__s89);
      }
      ergo_release_val(__t408);
      ergo_arr_add(__a2, __t409);
      ErgoVal __t410 = EV_STR(stdr_str_lit("Milligram"));
      ErgoVal __t411 = EV_NULLV;
      {
        ErgoVal __parts90[1] = { __t410 };
        ErgoStr* __s91 = stdr_str_from_parts(1, __parts90);
        __t411 = EV_STR(__s91);
      }
      ergo_release_val(__t410);
      ergo_arr_add(__a2, __t411);
      ErgoVal __t412 = EV_STR(stdr_str_lit("Pound"));
      ErgoVal __t413 = EV_NULLV;
      {
        ErgoVal __parts92[1] = { __t412 };
        ErgoStr* __s93 = stdr_str_from_parts(1, __parts92);
        __t413 = EV_STR(__s93);
      }
      ergo_release_val(__t412);
      ergo_arr_add(__a2, __t413);
      ErgoVal __t414 = EV_STR(stdr_str_lit("Ounce"));
      ErgoVal __t415 = EV_NULLV;
      {
        ErgoVal __parts94[1] = { __t414 };
        ErgoStr* __s95 = stdr_str_from_parts(1, __parts94);
        __t415 = EV_STR(__s95);
      }
      ergo_release_val(__t414);
      ergo_arr_add(__a2, __t415);
      ErgoVal __t416 = EV_STR(stdr_str_lit("Ton"));
      ErgoVal __t417 = EV_NULLV;
      {
        ErgoVal __parts96[1] = { __t416 };
        ErgoStr* __s97 = stdr_str_from_parts(1, __parts96);
        __t417 = EV_STR(__s97);
      }
      ergo_release_val(__t416);
      ergo_arr_add(__a2, __t417);
      ergo_move_into(&__ret, __t405);
      return __ret;
    } else {
      ErgoVal __t418 = ergo_g_main_conv_category; ergo_retain_val(__t418);
      ErgoVal __t419 = EV_INT(2);
      ErgoVal __t420 = ergo_eq(__t418, __t419);
      ergo_release_val(__t418);
      ergo_release_val(__t419);
      bool __b58 = ergo_as_bool(__t420);
      ergo_release_val(__t420);
      if (__b58) {
        ErgoArr* __a3 = stdr_arr_new(3);
        ErgoVal __t421 = EV_ARR(__a3);
        ErgoVal __t422 = EV_STR(stdr_str_lit("Celsius"));
        ErgoVal __t423 = EV_NULLV;
        {
          ErgoVal __parts98[1] = { __t422 };
          ErgoStr* __s99 = stdr_str_from_parts(1, __parts98);
          __t423 = EV_STR(__s99);
        }
        ergo_release_val(__t422);
        ergo_arr_add(__a3, __t423);
        ErgoVal __t424 = EV_STR(stdr_str_lit("Fahrenheit"));
        ErgoVal __t425 = EV_NULLV;
        {
          ErgoVal __parts100[1] = { __t424 };
          ErgoStr* __s101 = stdr_str_from_parts(1, __parts100);
          __t425 = EV_STR(__s101);
        }
        ergo_release_val(__t424);
        ergo_arr_add(__a3, __t425);
        ErgoVal __t426 = EV_STR(stdr_str_lit("Kelvin"));
        ErgoVal __t427 = EV_NULLV;
        {
          ErgoVal __parts102[1] = { __t426 };
          ErgoStr* __s103 = stdr_str_from_parts(1, __parts102);
          __t427 = EV_STR(__s103);
        }
        ergo_release_val(__t426);
        ergo_arr_add(__a3, __t427);
        ergo_move_into(&__ret, __t421);
        return __ret;
      } else {
        ErgoArr* __a4 = stdr_arr_new(6);
        ErgoVal __t428 = EV_ARR(__a4);
        ErgoVal __t429 = EV_STR(stdr_str_lit("Liter"));
        ErgoVal __t430 = EV_NULLV;
        {
          ErgoVal __parts104[1] = { __t429 };
          ErgoStr* __s105 = stdr_str_from_parts(1, __parts104);
          __t430 = EV_STR(__s105);
        }
        ergo_release_val(__t429);
        ergo_arr_add(__a4, __t430);
        ErgoVal __t431 = EV_STR(stdr_str_lit("Milliliter"));
        ErgoVal __t432 = EV_NULLV;
        {
          ErgoVal __parts106[1] = { __t431 };
          ErgoStr* __s107 = stdr_str_from_parts(1, __parts106);
          __t432 = EV_STR(__s107);
        }
        ergo_release_val(__t431);
        ergo_arr_add(__a4, __t432);
        ErgoVal __t433 = EV_STR(stdr_str_lit("Gallon"));
        ErgoVal __t434 = EV_NULLV;
        {
          ErgoVal __parts108[1] = { __t433 };
          ErgoStr* __s109 = stdr_str_from_parts(1, __parts108);
          __t434 = EV_STR(__s109);
        }
        ergo_release_val(__t433);
        ergo_arr_add(__a4, __t434);
        ErgoVal __t435 = EV_STR(stdr_str_lit("Quart"));
        ErgoVal __t436 = EV_NULLV;
        {
          ErgoVal __parts110[1] = { __t435 };
          ErgoStr* __s111 = stdr_str_from_parts(1, __parts110);
          __t436 = EV_STR(__s111);
        }
        ergo_release_val(__t435);
        ergo_arr_add(__a4, __t436);
        ErgoVal __t437 = EV_STR(stdr_str_lit("Cup"));
        ErgoVal __t438 = EV_NULLV;
        {
          ErgoVal __parts112[1] = { __t437 };
          ErgoStr* __s113 = stdr_str_from_parts(1, __parts112);
          __t438 = EV_STR(__s113);
        }
        ergo_release_val(__t437);
        ergo_arr_add(__a4, __t438);
        ErgoVal __t439 = EV_STR(stdr_str_lit("Fluid Oz"));
        ErgoVal __t440 = EV_NULLV;
        {
          ErgoVal __parts114[1] = { __t439 };
          ErgoStr* __s115 = stdr_str_from_parts(1, __parts114);
          __t440 = EV_STR(__s115);
        }
        ergo_release_val(__t439);
        ergo_arr_add(__a4, __t440);
        ergo_move_into(&__ret, __t428);
        return __ret;
      }
    }
  }
  return __ret;
}

static ErgoVal ergo_main_conv_to_base(ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t441 = ergo_g_main_conv_category; ergo_retain_val(__t441);
  ErgoVal __t442 = EV_INT(0);
  ErgoVal __t443 = ergo_eq(__t441, __t442);
  ergo_release_val(__t441);
  ergo_release_val(__t442);
  bool __b59 = ergo_as_bool(__t443);
  ergo_release_val(__t443);
  if (__b59) {
    ErgoVal __t444 = a1; ergo_retain_val(__t444);
    ErgoVal __t445 = EV_INT(0);
    ErgoVal __t446 = ergo_eq(__t444, __t445);
    ergo_release_val(__t444);
    ergo_release_val(__t445);
    bool __b60 = ergo_as_bool(__t446);
    ergo_release_val(__t446);
    if (__b60) {
      ErgoVal __t447 = a0; ergo_retain_val(__t447);
      ergo_move_into(&__ret, __t447);
      return __ret;
    } else {
      ErgoVal __t448 = a1; ergo_retain_val(__t448);
      ErgoVal __t449 = EV_INT(1);
      ErgoVal __t450 = ergo_eq(__t448, __t449);
      ergo_release_val(__t448);
      ergo_release_val(__t449);
      bool __b61 = ergo_as_bool(__t450);
      ergo_release_val(__t450);
      if (__b61) {
        ErgoVal __t451 = a0; ergo_retain_val(__t451);
        ErgoVal __t452 = EV_INT(1000);
        ErgoVal __t453 = ergo_mul(__t451, __t452);
        ergo_release_val(__t451);
        ergo_release_val(__t452);
        ergo_move_into(&__ret, __t453);
        return __ret;
      } else {
        ErgoVal __t454 = a1; ergo_retain_val(__t454);
        ErgoVal __t455 = EV_INT(2);
        ErgoVal __t456 = ergo_eq(__t454, __t455);
        ergo_release_val(__t454);
        ergo_release_val(__t455);
        bool __b62 = ergo_as_bool(__t456);
        ergo_release_val(__t456);
        if (__b62) {
          ErgoVal __t457 = a0; ergo_retain_val(__t457);
          ErgoVal __t458 = EV_INT(100);
          ErgoVal __t459 = ergo_div(__t457, __t458);
          ergo_release_val(__t457);
          ergo_release_val(__t458);
          ergo_move_into(&__ret, __t459);
          return __ret;
        } else {
          ErgoVal __t460 = a1; ergo_retain_val(__t460);
          ErgoVal __t461 = EV_INT(3);
          ErgoVal __t462 = ergo_eq(__t460, __t461);
          ergo_release_val(__t460);
          ergo_release_val(__t461);
          bool __b63 = ergo_as_bool(__t462);
          ergo_release_val(__t462);
          if (__b63) {
            ErgoVal __t463 = a0; ergo_retain_val(__t463);
            ErgoVal __t464 = EV_INT(1000);
            ErgoVal __t465 = ergo_div(__t463, __t464);
            ergo_release_val(__t463);
            ergo_release_val(__t464);
            ergo_move_into(&__ret, __t465);
            return __ret;
          } else {
            ErgoVal __t466 = a1; ergo_retain_val(__t466);
            ErgoVal __t467 = EV_INT(4);
            ErgoVal __t468 = ergo_eq(__t466, __t467);
            ergo_release_val(__t466);
            ergo_release_val(__t467);
            bool __b64 = ergo_as_bool(__t468);
            ergo_release_val(__t468);
            if (__b64) {
              ErgoVal __t469 = a0; ergo_retain_val(__t469);
              ErgoVal __t470 = EV_FLOAT(1609.3440000000001);
              ErgoVal __t471 = ergo_mul(__t469, __t470);
              ergo_release_val(__t469);
              ergo_release_val(__t470);
              ergo_move_into(&__ret, __t471);
              return __ret;
            } else {
              ErgoVal __t472 = a1; ergo_retain_val(__t472);
              ErgoVal __t473 = EV_INT(5);
              ErgoVal __t474 = ergo_eq(__t472, __t473);
              ergo_release_val(__t472);
              ergo_release_val(__t473);
              bool __b65 = ergo_as_bool(__t474);
              ergo_release_val(__t474);
              if (__b65) {
                ErgoVal __t475 = a0; ergo_retain_val(__t475);
                ErgoVal __t476 = EV_FLOAT(0.91439999999999999);
                ErgoVal __t477 = ergo_mul(__t475, __t476);
                ergo_release_val(__t475);
                ergo_release_val(__t476);
                ergo_move_into(&__ret, __t477);
                return __ret;
              } else {
                ErgoVal __t478 = a1; ergo_retain_val(__t478);
                ErgoVal __t479 = EV_INT(6);
                ErgoVal __t480 = ergo_eq(__t478, __t479);
                ergo_release_val(__t478);
                ergo_release_val(__t479);
                bool __b66 = ergo_as_bool(__t480);
                ergo_release_val(__t480);
                if (__b66) {
                  ErgoVal __t481 = a0; ergo_retain_val(__t481);
                  ErgoVal __t482 = EV_FLOAT(0.30480000000000002);
                  ErgoVal __t483 = ergo_mul(__t481, __t482);
                  ergo_release_val(__t481);
                  ergo_release_val(__t482);
                  ergo_move_into(&__ret, __t483);
                  return __ret;
                } else {
                  ErgoVal __t484 = a0; ergo_retain_val(__t484);
                  ErgoVal __t485 = EV_FLOAT(0.025399999999999999);
                  ErgoVal __t486 = ergo_mul(__t484, __t485);
                  ergo_release_val(__t484);
                  ergo_release_val(__t485);
                  ergo_move_into(&__ret, __t486);
                  return __ret;
                }
              }
            }
          }
        }
      }
    }
  } else {
    ErgoVal __t487 = ergo_g_main_conv_category; ergo_retain_val(__t487);
    ErgoVal __t488 = EV_INT(1);
    ErgoVal __t489 = ergo_eq(__t487, __t488);
    ergo_release_val(__t487);
    ergo_release_val(__t488);
    bool __b67 = ergo_as_bool(__t489);
    ergo_release_val(__t489);
    if (__b67) {
      ErgoVal __t490 = a1; ergo_retain_val(__t490);
      ErgoVal __t491 = EV_INT(0);
      ErgoVal __t492 = ergo_eq(__t490, __t491);
      ergo_release_val(__t490);
      ergo_release_val(__t491);
      bool __b68 = ergo_as_bool(__t492);
      ergo_release_val(__t492);
      if (__b68) {
        ErgoVal __t493 = a0; ergo_retain_val(__t493);
        ergo_move_into(&__ret, __t493);
        return __ret;
      } else {
        ErgoVal __t494 = a1; ergo_retain_val(__t494);
        ErgoVal __t495 = EV_INT(1);
        ErgoVal __t496 = ergo_eq(__t494, __t495);
        ergo_release_val(__t494);
        ergo_release_val(__t495);
        bool __b69 = ergo_as_bool(__t496);
        ergo_release_val(__t496);
        if (__b69) {
          ErgoVal __t497 = a0; ergo_retain_val(__t497);
          ErgoVal __t498 = EV_INT(1000);
          ErgoVal __t499 = ergo_div(__t497, __t498);
          ergo_release_val(__t497);
          ergo_release_val(__t498);
          ergo_move_into(&__ret, __t499);
          return __ret;
        } else {
          ErgoVal __t500 = a1; ergo_retain_val(__t500);
          ErgoVal __t501 = EV_INT(2);
          ErgoVal __t502 = ergo_eq(__t500, __t501);
          ergo_release_val(__t500);
          ergo_release_val(__t501);
          bool __b70 = ergo_as_bool(__t502);
          ergo_release_val(__t502);
          if (__b70) {
            ErgoVal __t503 = a0; ergo_retain_val(__t503);
            ErgoVal __t504 = EV_INT(1000000);
            ErgoVal __t505 = ergo_div(__t503, __t504);
            ergo_release_val(__t503);
            ergo_release_val(__t504);
            ergo_move_into(&__ret, __t505);
            return __ret;
          } else {
            ErgoVal __t506 = a1; ergo_retain_val(__t506);
            ErgoVal __t507 = EV_INT(3);
            ErgoVal __t508 = ergo_eq(__t506, __t507);
            ergo_release_val(__t506);
            ergo_release_val(__t507);
            bool __b71 = ergo_as_bool(__t508);
            ergo_release_val(__t508);
            if (__b71) {
              ErgoVal __t509 = a0; ergo_retain_val(__t509);
              ErgoVal __t510 = EV_FLOAT(0.453592);
              ErgoVal __t511 = ergo_mul(__t509, __t510);
              ergo_release_val(__t509);
              ergo_release_val(__t510);
              ergo_move_into(&__ret, __t511);
              return __ret;
            } else {
              ErgoVal __t512 = a1; ergo_retain_val(__t512);
              ErgoVal __t513 = EV_INT(4);
              ErgoVal __t514 = ergo_eq(__t512, __t513);
              ergo_release_val(__t512);
              ergo_release_val(__t513);
              bool __b72 = ergo_as_bool(__t514);
              ergo_release_val(__t514);
              if (__b72) {
                ErgoVal __t515 = a0; ergo_retain_val(__t515);
                ErgoVal __t516 = EV_FLOAT(0.0283495);
                ErgoVal __t517 = ergo_mul(__t515, __t516);
                ergo_release_val(__t515);
                ergo_release_val(__t516);
                ergo_move_into(&__ret, __t517);
                return __ret;
              } else {
                ErgoVal __t518 = a0; ergo_retain_val(__t518);
                ErgoVal __t519 = EV_FLOAT(907.18499999999995);
                ErgoVal __t520 = ergo_mul(__t518, __t519);
                ergo_release_val(__t518);
                ergo_release_val(__t519);
                ergo_move_into(&__ret, __t520);
                return __ret;
              }
            }
          }
        }
      }
    } else {
      ErgoVal __t521 = ergo_g_main_conv_category; ergo_retain_val(__t521);
      ErgoVal __t522 = EV_INT(2);
      ErgoVal __t523 = ergo_eq(__t521, __t522);
      ergo_release_val(__t521);
      ergo_release_val(__t522);
      bool __b73 = ergo_as_bool(__t523);
      ergo_release_val(__t523);
      if (__b73) {
        ErgoVal __t524 = a1; ergo_retain_val(__t524);
        ErgoVal __t525 = EV_INT(0);
        ErgoVal __t526 = ergo_eq(__t524, __t525);
        ergo_release_val(__t524);
        ergo_release_val(__t525);
        bool __b74 = ergo_as_bool(__t526);
        ergo_release_val(__t526);
        if (__b74) {
          ErgoVal __t527 = a0; ergo_retain_val(__t527);
          ergo_move_into(&__ret, __t527);
          return __ret;
        } else {
          ErgoVal __t528 = a1; ergo_retain_val(__t528);
          ErgoVal __t529 = EV_INT(1);
          ErgoVal __t530 = ergo_eq(__t528, __t529);
          ergo_release_val(__t528);
          ergo_release_val(__t529);
          bool __b75 = ergo_as_bool(__t530);
          ergo_release_val(__t530);
          if (__b75) {
            ErgoVal __t531 = a0; ergo_retain_val(__t531);
            ErgoVal __t532 = EV_INT(32);
            ErgoVal __t533 = ergo_sub(__t531, __t532);
            ergo_release_val(__t531);
            ergo_release_val(__t532);
            ErgoVal __t534 = EV_INT(5);
            ErgoVal __t535 = ergo_mul(__t533, __t534);
            ergo_release_val(__t533);
            ergo_release_val(__t534);
            ErgoVal __t536 = EV_INT(9);
            ErgoVal __t537 = ergo_div(__t535, __t536);
            ergo_release_val(__t535);
            ergo_release_val(__t536);
            ergo_move_into(&__ret, __t537);
            return __ret;
          } else {
            ErgoVal __t538 = a0; ergo_retain_val(__t538);
            ErgoVal __t539 = EV_FLOAT(273.14999999999998);
            ErgoVal __t540 = ergo_sub(__t538, __t539);
            ergo_release_val(__t538);
            ergo_release_val(__t539);
            ergo_move_into(&__ret, __t540);
            return __ret;
          }
        }
      } else {
        ErgoVal __t541 = a1; ergo_retain_val(__t541);
        ErgoVal __t542 = EV_INT(0);
        ErgoVal __t543 = ergo_eq(__t541, __t542);
        ergo_release_val(__t541);
        ergo_release_val(__t542);
        bool __b76 = ergo_as_bool(__t543);
        ergo_release_val(__t543);
        if (__b76) {
          ErgoVal __t544 = a0; ergo_retain_val(__t544);
          ergo_move_into(&__ret, __t544);
          return __ret;
        } else {
          ErgoVal __t545 = a1; ergo_retain_val(__t545);
          ErgoVal __t546 = EV_INT(1);
          ErgoVal __t547 = ergo_eq(__t545, __t546);
          ergo_release_val(__t545);
          ergo_release_val(__t546);
          bool __b77 = ergo_as_bool(__t547);
          ergo_release_val(__t547);
          if (__b77) {
            ErgoVal __t548 = a0; ergo_retain_val(__t548);
            ErgoVal __t549 = EV_INT(1000);
            ErgoVal __t550 = ergo_div(__t548, __t549);
            ergo_release_val(__t548);
            ergo_release_val(__t549);
            ergo_move_into(&__ret, __t550);
            return __ret;
          } else {
            ErgoVal __t551 = a1; ergo_retain_val(__t551);
            ErgoVal __t552 = EV_INT(2);
            ErgoVal __t553 = ergo_eq(__t551, __t552);
            ergo_release_val(__t551);
            ergo_release_val(__t552);
            bool __b78 = ergo_as_bool(__t553);
            ergo_release_val(__t553);
            if (__b78) {
              ErgoVal __t554 = a0; ergo_retain_val(__t554);
              ErgoVal __t555 = EV_FLOAT(3.7854100000000002);
              ErgoVal __t556 = ergo_mul(__t554, __t555);
              ergo_release_val(__t554);
              ergo_release_val(__t555);
              ergo_move_into(&__ret, __t556);
              return __ret;
            } else {
              ErgoVal __t557 = a1; ergo_retain_val(__t557);
              ErgoVal __t558 = EV_INT(3);
              ErgoVal __t559 = ergo_eq(__t557, __t558);
              ergo_release_val(__t557);
              ergo_release_val(__t558);
              bool __b79 = ergo_as_bool(__t559);
              ergo_release_val(__t559);
              if (__b79) {
                ErgoVal __t560 = a0; ergo_retain_val(__t560);
                ErgoVal __t561 = EV_FLOAT(0.946353);
                ErgoVal __t562 = ergo_mul(__t560, __t561);
                ergo_release_val(__t560);
                ergo_release_val(__t561);
                ergo_move_into(&__ret, __t562);
                return __ret;
              } else {
                ErgoVal __t563 = a1; ergo_retain_val(__t563);
                ErgoVal __t564 = EV_INT(4);
                ErgoVal __t565 = ergo_eq(__t563, __t564);
                ergo_release_val(__t563);
                ergo_release_val(__t564);
                bool __b80 = ergo_as_bool(__t565);
                ergo_release_val(__t565);
                if (__b80) {
                  ErgoVal __t566 = a0; ergo_retain_val(__t566);
                  ErgoVal __t567 = EV_FLOAT(0.23658799999999999);
                  ErgoVal __t568 = ergo_mul(__t566, __t567);
                  ergo_release_val(__t566);
                  ergo_release_val(__t567);
                  ergo_move_into(&__ret, __t568);
                  return __ret;
                } else {
                  ErgoVal __t569 = a0; ergo_retain_val(__t569);
                  ErgoVal __t570 = EV_FLOAT(0.029573499999999999);
                  ErgoVal __t571 = ergo_mul(__t569, __t570);
                  ergo_release_val(__t569);
                  ergo_release_val(__t570);
                  ergo_move_into(&__ret, __t571);
                  return __ret;
                }
              }
            }
          }
        }
      }
    }
  }
  return __ret;
}

static ErgoVal ergo_main_conv_from_base(ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t572 = ergo_g_main_conv_category; ergo_retain_val(__t572);
  ErgoVal __t573 = EV_INT(0);
  ErgoVal __t574 = ergo_eq(__t572, __t573);
  ergo_release_val(__t572);
  ergo_release_val(__t573);
  bool __b81 = ergo_as_bool(__t574);
  ergo_release_val(__t574);
  if (__b81) {
    ErgoVal __t575 = a1; ergo_retain_val(__t575);
    ErgoVal __t576 = EV_INT(0);
    ErgoVal __t577 = ergo_eq(__t575, __t576);
    ergo_release_val(__t575);
    ergo_release_val(__t576);
    bool __b82 = ergo_as_bool(__t577);
    ergo_release_val(__t577);
    if (__b82) {
      ErgoVal __t578 = a0; ergo_retain_val(__t578);
      ergo_move_into(&__ret, __t578);
      return __ret;
    } else {
      ErgoVal __t579 = a1; ergo_retain_val(__t579);
      ErgoVal __t580 = EV_INT(1);
      ErgoVal __t581 = ergo_eq(__t579, __t580);
      ergo_release_val(__t579);
      ergo_release_val(__t580);
      bool __b83 = ergo_as_bool(__t581);
      ergo_release_val(__t581);
      if (__b83) {
        ErgoVal __t582 = a0; ergo_retain_val(__t582);
        ErgoVal __t583 = EV_INT(1000);
        ErgoVal __t584 = ergo_div(__t582, __t583);
        ergo_release_val(__t582);
        ergo_release_val(__t583);
        ergo_move_into(&__ret, __t584);
        return __ret;
      } else {
        ErgoVal __t585 = a1; ergo_retain_val(__t585);
        ErgoVal __t586 = EV_INT(2);
        ErgoVal __t587 = ergo_eq(__t585, __t586);
        ergo_release_val(__t585);
        ergo_release_val(__t586);
        bool __b84 = ergo_as_bool(__t587);
        ergo_release_val(__t587);
        if (__b84) {
          ErgoVal __t588 = a0; ergo_retain_val(__t588);
          ErgoVal __t589 = EV_INT(100);
          ErgoVal __t590 = ergo_mul(__t588, __t589);
          ergo_release_val(__t588);
          ergo_release_val(__t589);
          ergo_move_into(&__ret, __t590);
          return __ret;
        } else {
          ErgoVal __t591 = a1; ergo_retain_val(__t591);
          ErgoVal __t592 = EV_INT(3);
          ErgoVal __t593 = ergo_eq(__t591, __t592);
          ergo_release_val(__t591);
          ergo_release_val(__t592);
          bool __b85 = ergo_as_bool(__t593);
          ergo_release_val(__t593);
          if (__b85) {
            ErgoVal __t594 = a0; ergo_retain_val(__t594);
            ErgoVal __t595 = EV_INT(1000);
            ErgoVal __t596 = ergo_mul(__t594, __t595);
            ergo_release_val(__t594);
            ergo_release_val(__t595);
            ergo_move_into(&__ret, __t596);
            return __ret;
          } else {
            ErgoVal __t597 = a1; ergo_retain_val(__t597);
            ErgoVal __t598 = EV_INT(4);
            ErgoVal __t599 = ergo_eq(__t597, __t598);
            ergo_release_val(__t597);
            ergo_release_val(__t598);
            bool __b86 = ergo_as_bool(__t599);
            ergo_release_val(__t599);
            if (__b86) {
              ErgoVal __t600 = a0; ergo_retain_val(__t600);
              ErgoVal __t601 = EV_FLOAT(1609.3440000000001);
              ErgoVal __t602 = ergo_div(__t600, __t601);
              ergo_release_val(__t600);
              ergo_release_val(__t601);
              ergo_move_into(&__ret, __t602);
              return __ret;
            } else {
              ErgoVal __t603 = a1; ergo_retain_val(__t603);
              ErgoVal __t604 = EV_INT(5);
              ErgoVal __t605 = ergo_eq(__t603, __t604);
              ergo_release_val(__t603);
              ergo_release_val(__t604);
              bool __b87 = ergo_as_bool(__t605);
              ergo_release_val(__t605);
              if (__b87) {
                ErgoVal __t606 = a0; ergo_retain_val(__t606);
                ErgoVal __t607 = EV_FLOAT(0.91439999999999999);
                ErgoVal __t608 = ergo_div(__t606, __t607);
                ergo_release_val(__t606);
                ergo_release_val(__t607);
                ergo_move_into(&__ret, __t608);
                return __ret;
              } else {
                ErgoVal __t609 = a1; ergo_retain_val(__t609);
                ErgoVal __t610 = EV_INT(6);
                ErgoVal __t611 = ergo_eq(__t609, __t610);
                ergo_release_val(__t609);
                ergo_release_val(__t610);
                bool __b88 = ergo_as_bool(__t611);
                ergo_release_val(__t611);
                if (__b88) {
                  ErgoVal __t612 = a0; ergo_retain_val(__t612);
                  ErgoVal __t613 = EV_FLOAT(0.30480000000000002);
                  ErgoVal __t614 = ergo_div(__t612, __t613);
                  ergo_release_val(__t612);
                  ergo_release_val(__t613);
                  ergo_move_into(&__ret, __t614);
                  return __ret;
                } else {
                  ErgoVal __t615 = a0; ergo_retain_val(__t615);
                  ErgoVal __t616 = EV_FLOAT(0.025399999999999999);
                  ErgoVal __t617 = ergo_div(__t615, __t616);
                  ergo_release_val(__t615);
                  ergo_release_val(__t616);
                  ergo_move_into(&__ret, __t617);
                  return __ret;
                }
              }
            }
          }
        }
      }
    }
  } else {
    ErgoVal __t618 = ergo_g_main_conv_category; ergo_retain_val(__t618);
    ErgoVal __t619 = EV_INT(1);
    ErgoVal __t620 = ergo_eq(__t618, __t619);
    ergo_release_val(__t618);
    ergo_release_val(__t619);
    bool __b89 = ergo_as_bool(__t620);
    ergo_release_val(__t620);
    if (__b89) {
      ErgoVal __t621 = a1; ergo_retain_val(__t621);
      ErgoVal __t622 = EV_INT(0);
      ErgoVal __t623 = ergo_eq(__t621, __t622);
      ergo_release_val(__t621);
      ergo_release_val(__t622);
      bool __b90 = ergo_as_bool(__t623);
      ergo_release_val(__t623);
      if (__b90) {
        ErgoVal __t624 = a0; ergo_retain_val(__t624);
        ergo_move_into(&__ret, __t624);
        return __ret;
      } else {
        ErgoVal __t625 = a1; ergo_retain_val(__t625);
        ErgoVal __t626 = EV_INT(1);
        ErgoVal __t627 = ergo_eq(__t625, __t626);
        ergo_release_val(__t625);
        ergo_release_val(__t626);
        bool __b91 = ergo_as_bool(__t627);
        ergo_release_val(__t627);
        if (__b91) {
          ErgoVal __t628 = a0; ergo_retain_val(__t628);
          ErgoVal __t629 = EV_INT(1000);
          ErgoVal __t630 = ergo_mul(__t628, __t629);
          ergo_release_val(__t628);
          ergo_release_val(__t629);
          ergo_move_into(&__ret, __t630);
          return __ret;
        } else {
          ErgoVal __t631 = a1; ergo_retain_val(__t631);
          ErgoVal __t632 = EV_INT(2);
          ErgoVal __t633 = ergo_eq(__t631, __t632);
          ergo_release_val(__t631);
          ergo_release_val(__t632);
          bool __b92 = ergo_as_bool(__t633);
          ergo_release_val(__t633);
          if (__b92) {
            ErgoVal __t634 = a0; ergo_retain_val(__t634);
            ErgoVal __t635 = EV_INT(1000000);
            ErgoVal __t636 = ergo_mul(__t634, __t635);
            ergo_release_val(__t634);
            ergo_release_val(__t635);
            ergo_move_into(&__ret, __t636);
            return __ret;
          } else {
            ErgoVal __t637 = a1; ergo_retain_val(__t637);
            ErgoVal __t638 = EV_INT(3);
            ErgoVal __t639 = ergo_eq(__t637, __t638);
            ergo_release_val(__t637);
            ergo_release_val(__t638);
            bool __b93 = ergo_as_bool(__t639);
            ergo_release_val(__t639);
            if (__b93) {
              ErgoVal __t640 = a0; ergo_retain_val(__t640);
              ErgoVal __t641 = EV_FLOAT(0.453592);
              ErgoVal __t642 = ergo_div(__t640, __t641);
              ergo_release_val(__t640);
              ergo_release_val(__t641);
              ergo_move_into(&__ret, __t642);
              return __ret;
            } else {
              ErgoVal __t643 = a1; ergo_retain_val(__t643);
              ErgoVal __t644 = EV_INT(4);
              ErgoVal __t645 = ergo_eq(__t643, __t644);
              ergo_release_val(__t643);
              ergo_release_val(__t644);
              bool __b94 = ergo_as_bool(__t645);
              ergo_release_val(__t645);
              if (__b94) {
                ErgoVal __t646 = a0; ergo_retain_val(__t646);
                ErgoVal __t647 = EV_FLOAT(0.0283495);
                ErgoVal __t648 = ergo_div(__t646, __t647);
                ergo_release_val(__t646);
                ergo_release_val(__t647);
                ergo_move_into(&__ret, __t648);
                return __ret;
              } else {
                ErgoVal __t649 = a0; ergo_retain_val(__t649);
                ErgoVal __t650 = EV_FLOAT(907.18499999999995);
                ErgoVal __t651 = ergo_div(__t649, __t650);
                ergo_release_val(__t649);
                ergo_release_val(__t650);
                ergo_move_into(&__ret, __t651);
                return __ret;
              }
            }
          }
        }
      }
    } else {
      ErgoVal __t652 = ergo_g_main_conv_category; ergo_retain_val(__t652);
      ErgoVal __t653 = EV_INT(2);
      ErgoVal __t654 = ergo_eq(__t652, __t653);
      ergo_release_val(__t652);
      ergo_release_val(__t653);
      bool __b95 = ergo_as_bool(__t654);
      ergo_release_val(__t654);
      if (__b95) {
        ErgoVal __t655 = a1; ergo_retain_val(__t655);
        ErgoVal __t656 = EV_INT(0);
        ErgoVal __t657 = ergo_eq(__t655, __t656);
        ergo_release_val(__t655);
        ergo_release_val(__t656);
        bool __b96 = ergo_as_bool(__t657);
        ergo_release_val(__t657);
        if (__b96) {
          ErgoVal __t658 = a0; ergo_retain_val(__t658);
          ergo_move_into(&__ret, __t658);
          return __ret;
        } else {
          ErgoVal __t659 = a1; ergo_retain_val(__t659);
          ErgoVal __t660 = EV_INT(1);
          ErgoVal __t661 = ergo_eq(__t659, __t660);
          ergo_release_val(__t659);
          ergo_release_val(__t660);
          bool __b97 = ergo_as_bool(__t661);
          ergo_release_val(__t661);
          if (__b97) {
            ErgoVal __t662 = a0; ergo_retain_val(__t662);
            ErgoVal __t663 = EV_INT(9);
            ErgoVal __t664 = ergo_mul(__t662, __t663);
            ergo_release_val(__t662);
            ergo_release_val(__t663);
            ErgoVal __t665 = EV_INT(5);
            ErgoVal __t666 = ergo_div(__t664, __t665);
            ergo_release_val(__t664);
            ergo_release_val(__t665);
            ErgoVal __t667 = EV_INT(32);
            ErgoVal __t668 = ergo_add(__t666, __t667);
            ergo_release_val(__t666);
            ergo_release_val(__t667);
            ergo_move_into(&__ret, __t668);
            return __ret;
          } else {
            ErgoVal __t669 = a0; ergo_retain_val(__t669);
            ErgoVal __t670 = EV_FLOAT(273.14999999999998);
            ErgoVal __t671 = ergo_add(__t669, __t670);
            ergo_release_val(__t669);
            ergo_release_val(__t670);
            ergo_move_into(&__ret, __t671);
            return __ret;
          }
        }
      } else {
        ErgoVal __t672 = a1; ergo_retain_val(__t672);
        ErgoVal __t673 = EV_INT(0);
        ErgoVal __t674 = ergo_eq(__t672, __t673);
        ergo_release_val(__t672);
        ergo_release_val(__t673);
        bool __b98 = ergo_as_bool(__t674);
        ergo_release_val(__t674);
        if (__b98) {
          ErgoVal __t675 = a0; ergo_retain_val(__t675);
          ergo_move_into(&__ret, __t675);
          return __ret;
        } else {
          ErgoVal __t676 = a1; ergo_retain_val(__t676);
          ErgoVal __t677 = EV_INT(1);
          ErgoVal __t678 = ergo_eq(__t676, __t677);
          ergo_release_val(__t676);
          ergo_release_val(__t677);
          bool __b99 = ergo_as_bool(__t678);
          ergo_release_val(__t678);
          if (__b99) {
            ErgoVal __t679 = a0; ergo_retain_val(__t679);
            ErgoVal __t680 = EV_INT(1000);
            ErgoVal __t681 = ergo_mul(__t679, __t680);
            ergo_release_val(__t679);
            ergo_release_val(__t680);
            ergo_move_into(&__ret, __t681);
            return __ret;
          } else {
            ErgoVal __t682 = a1; ergo_retain_val(__t682);
            ErgoVal __t683 = EV_INT(2);
            ErgoVal __t684 = ergo_eq(__t682, __t683);
            ergo_release_val(__t682);
            ergo_release_val(__t683);
            bool __b100 = ergo_as_bool(__t684);
            ergo_release_val(__t684);
            if (__b100) {
              ErgoVal __t685 = a0; ergo_retain_val(__t685);
              ErgoVal __t686 = EV_FLOAT(3.7854100000000002);
              ErgoVal __t687 = ergo_div(__t685, __t686);
              ergo_release_val(__t685);
              ergo_release_val(__t686);
              ergo_move_into(&__ret, __t687);
              return __ret;
            } else {
              ErgoVal __t688 = a1; ergo_retain_val(__t688);
              ErgoVal __t689 = EV_INT(3);
              ErgoVal __t690 = ergo_eq(__t688, __t689);
              ergo_release_val(__t688);
              ergo_release_val(__t689);
              bool __b101 = ergo_as_bool(__t690);
              ergo_release_val(__t690);
              if (__b101) {
                ErgoVal __t691 = a0; ergo_retain_val(__t691);
                ErgoVal __t692 = EV_FLOAT(0.946353);
                ErgoVal __t693 = ergo_div(__t691, __t692);
                ergo_release_val(__t691);
                ergo_release_val(__t692);
                ergo_move_into(&__ret, __t693);
                return __ret;
              } else {
                ErgoVal __t694 = a1; ergo_retain_val(__t694);
                ErgoVal __t695 = EV_INT(4);
                ErgoVal __t696 = ergo_eq(__t694, __t695);
                ergo_release_val(__t694);
                ergo_release_val(__t695);
                bool __b102 = ergo_as_bool(__t696);
                ergo_release_val(__t696);
                if (__b102) {
                  ErgoVal __t697 = a0; ergo_retain_val(__t697);
                  ErgoVal __t698 = EV_FLOAT(0.23658799999999999);
                  ErgoVal __t699 = ergo_div(__t697, __t698);
                  ergo_release_val(__t697);
                  ergo_release_val(__t698);
                  ergo_move_into(&__ret, __t699);
                  return __ret;
                } else {
                  ErgoVal __t700 = a0; ergo_retain_val(__t700);
                  ErgoVal __t701 = EV_FLOAT(0.029573499999999999);
                  ErgoVal __t702 = ergo_div(__t700, __t701);
                  ergo_release_val(__t700);
                  ergo_release_val(__t701);
                  ergo_move_into(&__ret, __t702);
                  return __ret;
                }
              }
            }
          }
        }
      }
    }
  }
  return __ret;
}

static void ergo_main_do_convert(void) {
  ErgoVal input_text__103 = EV_NULLV;
  ErgoVal __t703 = ergo_g_main_conv_input; ergo_retain_val(__t703);
  ErgoVal __t704 = ergo_m_cogito_TextField_text(__t703);
  ergo_release_val(__t703);
  ergo_move_into(&input_text__103, __t704);
  ErgoVal input_val__104 = EV_NULLV;
  ErgoVal __t705 = input_text__103; ergo_retain_val(__t705);
  ErgoVal __t706 = ergo_main_aton(__t705);
  ergo_release_val(__t705);
  ergo_move_into(&input_val__104, __t706);
  ErgoVal base__105 = EV_NULLV;
  ErgoVal __t707 = input_val__104; ergo_retain_val(__t707);
  ErgoVal __t708 = ergo_g_main_conv_from_idx; ergo_retain_val(__t708);
  ErgoVal __t709 = ergo_main_conv_to_base(__t707, __t708);
  ergo_release_val(__t707);
  ergo_release_val(__t708);
  ergo_move_into(&base__105, __t709);
  ErgoVal result__106 = EV_NULLV;
  ErgoVal __t710 = base__105; ergo_retain_val(__t710);
  ErgoVal __t711 = ergo_g_main_conv_to_idx; ergo_retain_val(__t711);
  ErgoVal __t712 = ergo_main_conv_from_base(__t710, __t711);
  ergo_release_val(__t710);
  ergo_release_val(__t711);
  ergo_move_into(&result__106, __t712);
  ErgoVal __t713 = ergo_g_main_conv_output; ergo_retain_val(__t713);
  ErgoVal __t714 = result__106; ergo_retain_val(__t714);
  ErgoVal __t715 = EV_STR(stdr_to_string(__t714));
  ergo_release_val(__t714);
  ergo_m_cogito_TextField_set_text(__t713, __t715);
  ergo_release_val(__t713);
  ergo_release_val(__t715);
  ErgoVal __t716 = EV_NULLV;
  ergo_release_val(__t716);
  ergo_release_val(result__106);
  ergo_release_val(base__105);
  ergo_release_val(input_val__104);
  ergo_release_val(input_text__103);
}

static void ergo_main_swap_conv(void) {
  ErgoVal old_from__107 = EV_NULLV;
  ErgoVal __t717 = ergo_g_main_conv_from_idx; ergo_retain_val(__t717);
  ergo_move_into(&old_from__107, __t717);
  ErgoVal old_to__108 = EV_NULLV;
  ErgoVal __t718 = ergo_g_main_conv_to_idx; ergo_retain_val(__t718);
  ergo_move_into(&old_to__108, __t718);
  ErgoVal __t719 = old_to__108; ergo_retain_val(__t719);
  ErgoVal __t720 = __t719; ergo_retain_val(__t720);
  ergo_move_into(&ergo_g_main_conv_from_idx, __t719);
  ergo_release_val(__t720);
  ErgoVal __t721 = old_from__107; ergo_retain_val(__t721);
  ErgoVal __t722 = __t721; ergo_retain_val(__t722);
  ergo_move_into(&ergo_g_main_conv_to_idx, __t721);
  ergo_release_val(__t722);
  ErgoVal __t723 = ergo_g_main_conv_from_dd; ergo_retain_val(__t723);
  ErgoVal __t724 = ergo_g_main_conv_from_idx; ergo_retain_val(__t724);
  ergo_m_cogito_Dropdown_set_selected(__t723, __t724);
  ergo_release_val(__t723);
  ergo_release_val(__t724);
  ErgoVal __t725 = EV_NULLV;
  ergo_release_val(__t725);
  ErgoVal __t726 = ergo_g_main_conv_to_dd; ergo_retain_val(__t726);
  ErgoVal __t727 = ergo_g_main_conv_to_idx; ergo_retain_val(__t727);
  ergo_m_cogito_Dropdown_set_selected(__t726, __t727);
  ergo_release_val(__t726);
  ergo_release_val(__t727);
  ErgoVal __t728 = EV_NULLV;
  ergo_release_val(__t728);
  ErgoVal old_input__109 = EV_NULLV;
  ErgoVal __t729 = ergo_g_main_conv_input; ergo_retain_val(__t729);
  ErgoVal __t730 = ergo_m_cogito_TextField_text(__t729);
  ergo_release_val(__t729);
  ergo_move_into(&old_input__109, __t730);
  ErgoVal old_output__110 = EV_NULLV;
  ErgoVal __t731 = ergo_g_main_conv_output; ergo_retain_val(__t731);
  ErgoVal __t732 = ergo_m_cogito_TextField_text(__t731);
  ergo_release_val(__t731);
  ergo_move_into(&old_output__110, __t732);
  ErgoVal __t733 = ergo_g_main_conv_input; ergo_retain_val(__t733);
  ErgoVal __t734 = old_output__110; ergo_retain_val(__t734);
  ergo_m_cogito_TextField_set_text(__t733, __t734);
  ergo_release_val(__t733);
  ergo_release_val(__t734);
  ErgoVal __t735 = EV_NULLV;
  ergo_release_val(__t735);
  ErgoVal __t736 = ergo_g_main_conv_output; ergo_retain_val(__t736);
  ErgoVal __t737 = old_input__109; ergo_retain_val(__t737);
  ergo_m_cogito_TextField_set_text(__t736, __t737);
  ergo_release_val(__t736);
  ergo_release_val(__t737);
  ErgoVal __t738 = EV_NULLV;
  ergo_release_val(__t738);
  ergo_release_val(old_output__110);
  ergo_release_val(old_input__109);
  ergo_release_val(old_to__108);
  ergo_release_val(old_from__107);
}

static void ergo_main_refresh_conv_units(void) {
  ErgoVal names__111 = EV_NULLV;
  ErgoVal __t739 = ergo_main_conv_unit_names();
  ergo_move_into(&names__111, __t739);
  ErgoVal __t740 = ergo_g_main_conv_from_dd; ergo_retain_val(__t740);
  ErgoVal __t741 = names__111; ergo_retain_val(__t741);
  ergo_m_cogito_Dropdown_set_items(__t740, __t741);
  ergo_release_val(__t740);
  ergo_release_val(__t741);
  ErgoVal __t742 = EV_NULLV;
  ergo_release_val(__t742);
  ErgoVal __t743 = ergo_g_main_conv_to_dd; ergo_retain_val(__t743);
  ErgoVal __t744 = names__111; ergo_retain_val(__t744);
  ergo_m_cogito_Dropdown_set_items(__t743, __t744);
  ergo_release_val(__t743);
  ergo_release_val(__t744);
  ErgoVal __t745 = EV_NULLV;
  ergo_release_val(__t745);
  ErgoVal __t746 = EV_INT(0);
  ErgoVal __t747 = __t746; ergo_retain_val(__t747);
  ergo_move_into(&ergo_g_main_conv_from_idx, __t746);
  ergo_release_val(__t747);
  ErgoVal __t748 = EV_INT(1);
  ErgoVal __t749 = __t748; ergo_retain_val(__t749);
  ergo_move_into(&ergo_g_main_conv_to_idx, __t748);
  ergo_release_val(__t749);
  ErgoVal __t750 = ergo_g_main_conv_from_dd; ergo_retain_val(__t750);
  ErgoVal __t751 = EV_INT(0);
  ergo_m_cogito_Dropdown_set_selected(__t750, __t751);
  ergo_release_val(__t750);
  ergo_release_val(__t751);
  ErgoVal __t752 = EV_NULLV;
  ergo_release_val(__t752);
  ErgoVal __t753 = ergo_g_main_conv_to_dd; ergo_retain_val(__t753);
  ErgoVal __t754 = EV_INT(1);
  ergo_m_cogito_Dropdown_set_selected(__t753, __t754);
  ergo_release_val(__t753);
  ergo_release_val(__t754);
  ErgoVal __t755 = EV_NULLV;
  ergo_release_val(__t755);
  ErgoVal __t756 = ergo_g_main_conv_input; ergo_retain_val(__t756);
  ErgoVal __t757 = EV_STR(stdr_str_lit("1"));
  ErgoVal __t758 = EV_NULLV;
  {
    ErgoVal __parts116[1] = { __t757 };
    ErgoStr* __s117 = stdr_str_from_parts(1, __parts116);
    __t758 = EV_STR(__s117);
  }
  ergo_release_val(__t757);
  ergo_m_cogito_TextField_set_text(__t756, __t758);
  ergo_release_val(__t756);
  ergo_release_val(__t758);
  ErgoVal __t759 = EV_NULLV;
  ergo_release_val(__t759);
  ergo_main_do_convert();
  ErgoVal __t760 = EV_NULLV;
  ergo_release_val(__t760);
  ergo_release_val(names__111);
}

static ErgoVal ergo_main_build_converter_ui(void) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal panel__112 = EV_NULLV;
  ErgoVal __t761 = ergo_cogito_vstack();
  ergo_move_into(&panel__112, __t761);
  ErgoVal __t762 = panel__112; ergo_retain_val(__t762);
  ErgoVal __t763 = EV_BOOL(true);
  ergo_m_cogito_VStack_set_hexpand(__t762, __t763);
  ergo_release_val(__t762);
  ergo_release_val(__t763);
  ErgoVal __t764 = EV_NULLV;
  ergo_release_val(__t764);
  ErgoVal __t765 = panel__112; ergo_retain_val(__t765);
  ErgoVal __t766 = EV_BOOL(true);
  ergo_m_cogito_VStack_set_vexpand(__t765, __t766);
  ergo_release_val(__t765);
  ergo_release_val(__t766);
  ErgoVal __t767 = EV_NULLV;
  ergo_release_val(__t767);
  ErgoVal __t768 = panel__112; ergo_retain_val(__t768);
  ErgoVal __t769 = EV_INT(12);
  ergo_m_cogito_VStack_set_gap(__t768, __t769);
  ergo_release_val(__t768);
  ergo_release_val(__t769);
  ErgoVal __t770 = EV_NULLV;
  ergo_release_val(__t770);
  ErgoVal __t771 = panel__112; ergo_retain_val(__t771);
  ErgoVal __t772 = EV_STR(stdr_str_lit("conv-panel"));
  ErgoVal __t773 = EV_NULLV;
  {
    ErgoVal __parts118[1] = { __t772 };
    ErgoStr* __s119 = stdr_str_from_parts(1, __parts118);
    __t773 = EV_STR(__s119);
  }
  ergo_release_val(__t772);
  ergo_cogito_set_class(__t771, __t773);
  ergo_release_val(__t771);
  ergo_release_val(__t773);
  ErgoVal __t774 = EV_NULLV;
  ergo_release_val(__t774);
  ErgoVal cat_dd__113 = EV_NULLV;
  ErgoVal __t775 = ergo_cogito_dropdown();
  ergo_move_into(&cat_dd__113, __t775);
  ErgoVal __t776 = cat_dd__113; ergo_retain_val(__t776);
  ErgoArr* __a5 = stdr_arr_new(4);
  ErgoVal __t777 = EV_ARR(__a5);
  ErgoVal __t778 = EV_STR(stdr_str_lit("Length"));
  ErgoVal __t779 = EV_NULLV;
  {
    ErgoVal __parts120[1] = { __t778 };
    ErgoStr* __s121 = stdr_str_from_parts(1, __parts120);
    __t779 = EV_STR(__s121);
  }
  ergo_release_val(__t778);
  ergo_arr_add(__a5, __t779);
  ErgoVal __t780 = EV_STR(stdr_str_lit("Weight"));
  ErgoVal __t781 = EV_NULLV;
  {
    ErgoVal __parts122[1] = { __t780 };
    ErgoStr* __s123 = stdr_str_from_parts(1, __parts122);
    __t781 = EV_STR(__s123);
  }
  ergo_release_val(__t780);
  ergo_arr_add(__a5, __t781);
  ErgoVal __t782 = EV_STR(stdr_str_lit("Temperature"));
  ErgoVal __t783 = EV_NULLV;
  {
    ErgoVal __parts124[1] = { __t782 };
    ErgoStr* __s125 = stdr_str_from_parts(1, __parts124);
    __t783 = EV_STR(__s125);
  }
  ergo_release_val(__t782);
  ergo_arr_add(__a5, __t783);
  ErgoVal __t784 = EV_STR(stdr_str_lit("Volume"));
  ErgoVal __t785 = EV_NULLV;
  {
    ErgoVal __parts126[1] = { __t784 };
    ErgoStr* __s127 = stdr_str_from_parts(1, __parts126);
    __t785 = EV_STR(__s127);
  }
  ergo_release_val(__t784);
  ergo_arr_add(__a5, __t785);
  ergo_m_cogito_Dropdown_set_items(__t776, __t777);
  ergo_release_val(__t776);
  ergo_release_val(__t777);
  ErgoVal __t786 = EV_NULLV;
  ergo_release_val(__t786);
  ErgoVal __t787 = cat_dd__113; ergo_retain_val(__t787);
  ErgoVal __t788 = EV_INT(0);
  ergo_m_cogito_Dropdown_set_selected(__t787, __t788);
  ergo_release_val(__t787);
  ergo_release_val(__t788);
  ErgoVal __t789 = EV_NULLV;
  ergo_release_val(__t789);
  ErgoVal __t790 = cat_dd__113; ergo_retain_val(__t790);
  ErgoVal __t791 = EV_BOOL(true);
  ergo_m_cogito_Dropdown_set_hexpand(__t790, __t791);
  ergo_release_val(__t790);
  ergo_release_val(__t791);
  ErgoVal __t792 = EV_NULLV;
  ergo_release_val(__t792);
  ErgoVal __t793 = cat_dd__113; ergo_retain_val(__t793);
  ErgoVal __t794 = EV_STR(stdr_str_lit("conv-category"));
  ErgoVal __t795 = EV_NULLV;
  {
    ErgoVal __parts128[1] = { __t794 };
    ErgoStr* __s129 = stdr_str_from_parts(1, __parts128);
    __t795 = EV_STR(__s129);
  }
  ergo_release_val(__t794);
  ergo_cogito_set_class(__t793, __t795);
  ergo_release_val(__t793);
  ergo_release_val(__t795);
  ErgoVal __t796 = EV_NULLV;
  ergo_release_val(__t796);
  ErgoVal __t797 = cat_dd__113; ergo_retain_val(__t797);
  ErgoVal __t798 = EV_FN(ergo_fn_new(ergo_lambda_5, 1));
  ergo_m_cogito_Dropdown_on_change(__t797, __t798);
  ergo_release_val(__t797);
  ergo_release_val(__t798);
  ErgoVal __t799 = EV_NULLV;
  ergo_release_val(__t799);
  ErgoVal __t800 = panel__112; ergo_retain_val(__t800);
  ErgoVal __t801 = cat_dd__113; ergo_retain_val(__t801);
  ergo_m_cogito_VStack_add(__t800, __t801);
  ergo_release_val(__t800);
  ergo_release_val(__t801);
  ErgoVal __t802 = EV_NULLV;
  ergo_release_val(__t802);
  ErgoVal from_row__114 = EV_NULLV;
  ErgoVal __t803 = ergo_cogito_vstack();
  ergo_move_into(&from_row__114, __t803);
  ErgoVal __t804 = from_row__114; ergo_retain_val(__t804);
  ErgoVal __t805 = EV_BOOL(true);
  ergo_m_cogito_VStack_set_hexpand(__t804, __t805);
  ergo_release_val(__t804);
  ergo_release_val(__t805);
  ErgoVal __t806 = EV_NULLV;
  ergo_release_val(__t806);
  ErgoVal __t807 = from_row__114; ergo_retain_val(__t807);
  ErgoVal __t808 = EV_INT(6);
  ergo_m_cogito_VStack_set_gap(__t807, __t808);
  ergo_release_val(__t807);
  ergo_release_val(__t808);
  ErgoVal __t809 = EV_NULLV;
  ergo_release_val(__t809);
  ErgoVal from_label__115 = EV_NULLV;
  ErgoVal __t810 = EV_STR(stdr_str_lit("From"));
  ErgoVal __t811 = EV_NULLV;
  {
    ErgoVal __parts130[1] = { __t810 };
    ErgoStr* __s131 = stdr_str_from_parts(1, __parts130);
    __t811 = EV_STR(__s131);
  }
  ergo_release_val(__t810);
  ErgoVal __t812 = ergo_cogito_label(__t811);
  ergo_release_val(__t811);
  ergo_move_into(&from_label__115, __t812);
  ErgoVal __t813 = from_label__115; ergo_retain_val(__t813);
  ErgoVal __t814 = EV_STR(stdr_str_lit("conv-label"));
  ErgoVal __t815 = EV_NULLV;
  {
    ErgoVal __parts132[1] = { __t814 };
    ErgoStr* __s133 = stdr_str_from_parts(1, __parts132);
    __t815 = EV_STR(__s133);
  }
  ergo_release_val(__t814);
  ergo_cogito_set_class(__t813, __t815);
  ergo_release_val(__t813);
  ergo_release_val(__t815);
  ErgoVal __t816 = EV_NULLV;
  ergo_release_val(__t816);
  ErgoVal __t817 = from_row__114; ergo_retain_val(__t817);
  ErgoVal __t818 = from_label__115; ergo_retain_val(__t818);
  ergo_m_cogito_VStack_add(__t817, __t818);
  ergo_release_val(__t817);
  ergo_release_val(__t818);
  ErgoVal __t819 = EV_NULLV;
  ergo_release_val(__t819);
  ErgoVal names__116 = EV_NULLV;
  ErgoVal __t820 = ergo_main_conv_unit_names();
  ergo_move_into(&names__116, __t820);
  ErgoVal __t821 = ergo_g_main_conv_from_dd; ergo_retain_val(__t821);
  ErgoVal __t822 = names__116; ergo_retain_val(__t822);
  ergo_m_cogito_Dropdown_set_items(__t821, __t822);
  ergo_release_val(__t821);
  ergo_release_val(__t822);
  ErgoVal __t823 = EV_NULLV;
  ergo_release_val(__t823);
  ErgoVal __t824 = ergo_g_main_conv_from_dd; ergo_retain_val(__t824);
  ErgoVal __t825 = EV_INT(0);
  ergo_m_cogito_Dropdown_set_selected(__t824, __t825);
  ergo_release_val(__t824);
  ergo_release_val(__t825);
  ErgoVal __t826 = EV_NULLV;
  ergo_release_val(__t826);
  ErgoVal __t827 = ergo_g_main_conv_from_dd; ergo_retain_val(__t827);
  ErgoVal __t828 = EV_BOOL(true);
  ergo_m_cogito_Dropdown_set_hexpand(__t827, __t828);
  ergo_release_val(__t827);
  ergo_release_val(__t828);
  ErgoVal __t829 = EV_NULLV;
  ergo_release_val(__t829);
  ErgoVal __t830 = ergo_g_main_conv_from_dd; ergo_retain_val(__t830);
  ErgoVal __t831 = EV_FN(ergo_fn_new(ergo_lambda_6, 1));
  ergo_m_cogito_Dropdown_on_change(__t830, __t831);
  ergo_release_val(__t830);
  ergo_release_val(__t831);
  ErgoVal __t832 = EV_NULLV;
  ergo_release_val(__t832);
  ErgoVal __t833 = from_row__114; ergo_retain_val(__t833);
  ErgoVal __t834 = ergo_g_main_conv_from_dd; ergo_retain_val(__t834);
  ergo_m_cogito_VStack_add(__t833, __t834);
  ergo_release_val(__t833);
  ergo_release_val(__t834);
  ErgoVal __t835 = EV_NULLV;
  ergo_release_val(__t835);
  ErgoVal __t836 = ergo_g_main_conv_input; ergo_retain_val(__t836);
  ErgoVal __t837 = EV_BOOL(true);
  ergo_m_cogito_TextField_set_hexpand(__t836, __t837);
  ergo_release_val(__t836);
  ergo_release_val(__t837);
  ErgoVal __t838 = EV_NULLV;
  ergo_release_val(__t838);
  ErgoVal __t839 = ergo_g_main_conv_input; ergo_retain_val(__t839);
  ErgoVal __t840 = EV_STR(stdr_str_lit("conv-input"));
  ErgoVal __t841 = EV_NULLV;
  {
    ErgoVal __parts134[1] = { __t840 };
    ErgoStr* __s135 = stdr_str_from_parts(1, __parts134);
    __t841 = EV_STR(__s135);
  }
  ergo_release_val(__t840);
  ergo_cogito_set_class(__t839, __t841);
  ergo_release_val(__t839);
  ergo_release_val(__t841);
  ErgoVal __t842 = EV_NULLV;
  ergo_release_val(__t842);
  ErgoVal __t843 = ergo_g_main_conv_input; ergo_retain_val(__t843);
  ErgoVal __t844 = EV_FN(ergo_fn_new(ergo_lambda_7, 1));
  ergo_m_cogito_TextField_on_change(__t843, __t844);
  ergo_release_val(__t843);
  ergo_release_val(__t844);
  ErgoVal __t845 = EV_NULLV;
  ergo_release_val(__t845);
  ErgoVal __t846 = from_row__114; ergo_retain_val(__t846);
  ErgoVal __t847 = ergo_g_main_conv_input; ergo_retain_val(__t847);
  ergo_m_cogito_VStack_add(__t846, __t847);
  ergo_release_val(__t846);
  ergo_release_val(__t847);
  ErgoVal __t848 = EV_NULLV;
  ergo_release_val(__t848);
  ErgoVal __t849 = panel__112; ergo_retain_val(__t849);
  ErgoVal __t850 = from_row__114; ergo_retain_val(__t850);
  ergo_m_cogito_VStack_add(__t849, __t850);
  ergo_release_val(__t849);
  ergo_release_val(__t850);
  ErgoVal __t851 = EV_NULLV;
  ergo_release_val(__t851);
  ErgoVal swap_row__117 = EV_NULLV;
  ErgoVal __t852 = ergo_cogito_hstack();
  ergo_move_into(&swap_row__117, __t852);
  ErgoVal __t853 = swap_row__117; ergo_retain_val(__t853);
  ergo_m_cogito_HStack_align_center(__t853);
  ergo_release_val(__t853);
  ErgoVal __t854 = EV_NULLV;
  ergo_release_val(__t854);
  ErgoVal swap_btn__118 = EV_NULLV;
  ErgoVal __t855 = EV_STR(stdr_str_lit("sf:arrow.up.arrow.down"));
  ErgoVal __t856 = EV_NULLV;
  {
    ErgoVal __parts136[1] = { __t855 };
    ErgoStr* __s137 = stdr_str_from_parts(1, __parts136);
    __t856 = EV_STR(__s137);
  }
  ergo_release_val(__t855);
  ErgoVal __t857 = ergo_cogito_iconbtn(__t856);
  ergo_release_val(__t856);
  ergo_move_into(&swap_btn__118, __t857);
  ErgoVal __t858 = swap_btn__118; ergo_retain_val(__t858);
  ErgoVal __t859 = EV_STR(stdr_str_lit("conv-swap"));
  ErgoVal __t860 = EV_NULLV;
  {
    ErgoVal __parts138[1] = { __t859 };
    ErgoStr* __s139 = stdr_str_from_parts(1, __parts138);
    __t860 = EV_STR(__s139);
  }
  ergo_release_val(__t859);
  ergo_cogito_set_class(__t858, __t860);
  ergo_release_val(__t858);
  ergo_release_val(__t860);
  ErgoVal __t861 = EV_NULLV;
  ergo_release_val(__t861);
  ErgoVal __t862 = swap_btn__118; ergo_retain_val(__t862);
  ErgoVal __t863 = EV_FN(ergo_fn_new(ergo_lambda_8, 1));
  ergo_m_cogito_Button_on_click(__t862, __t863);
  ergo_release_val(__t862);
  ergo_release_val(__t863);
  ErgoVal __t864 = EV_NULLV;
  ergo_release_val(__t864);
  ErgoVal __t865 = swap_row__117; ergo_retain_val(__t865);
  ErgoVal __t866 = swap_btn__118; ergo_retain_val(__t866);
  ergo_m_cogito_HStack_add(__t865, __t866);
  ergo_release_val(__t865);
  ergo_release_val(__t866);
  ErgoVal __t867 = EV_NULLV;
  ergo_release_val(__t867);
  ErgoVal __t868 = panel__112; ergo_retain_val(__t868);
  ErgoVal __t869 = swap_row__117; ergo_retain_val(__t869);
  ergo_m_cogito_VStack_add(__t868, __t869);
  ergo_release_val(__t868);
  ergo_release_val(__t869);
  ErgoVal __t870 = EV_NULLV;
  ergo_release_val(__t870);
  ErgoVal to_row__119 = EV_NULLV;
  ErgoVal __t871 = ergo_cogito_vstack();
  ergo_move_into(&to_row__119, __t871);
  ErgoVal __t872 = to_row__119; ergo_retain_val(__t872);
  ErgoVal __t873 = EV_BOOL(true);
  ergo_m_cogito_VStack_set_hexpand(__t872, __t873);
  ergo_release_val(__t872);
  ergo_release_val(__t873);
  ErgoVal __t874 = EV_NULLV;
  ergo_release_val(__t874);
  ErgoVal __t875 = to_row__119; ergo_retain_val(__t875);
  ErgoVal __t876 = EV_INT(6);
  ergo_m_cogito_VStack_set_gap(__t875, __t876);
  ergo_release_val(__t875);
  ergo_release_val(__t876);
  ErgoVal __t877 = EV_NULLV;
  ergo_release_val(__t877);
  ErgoVal to_label__120 = EV_NULLV;
  ErgoVal __t878 = EV_STR(stdr_str_lit("To"));
  ErgoVal __t879 = EV_NULLV;
  {
    ErgoVal __parts140[1] = { __t878 };
    ErgoStr* __s141 = stdr_str_from_parts(1, __parts140);
    __t879 = EV_STR(__s141);
  }
  ergo_release_val(__t878);
  ErgoVal __t880 = ergo_cogito_label(__t879);
  ergo_release_val(__t879);
  ergo_move_into(&to_label__120, __t880);
  ErgoVal __t881 = to_label__120; ergo_retain_val(__t881);
  ErgoVal __t882 = EV_STR(stdr_str_lit("conv-label"));
  ErgoVal __t883 = EV_NULLV;
  {
    ErgoVal __parts142[1] = { __t882 };
    ErgoStr* __s143 = stdr_str_from_parts(1, __parts142);
    __t883 = EV_STR(__s143);
  }
  ergo_release_val(__t882);
  ergo_cogito_set_class(__t881, __t883);
  ergo_release_val(__t881);
  ergo_release_val(__t883);
  ErgoVal __t884 = EV_NULLV;
  ergo_release_val(__t884);
  ErgoVal __t885 = to_row__119; ergo_retain_val(__t885);
  ErgoVal __t886 = to_label__120; ergo_retain_val(__t886);
  ergo_m_cogito_VStack_add(__t885, __t886);
  ergo_release_val(__t885);
  ergo_release_val(__t886);
  ErgoVal __t887 = EV_NULLV;
  ergo_release_val(__t887);
  ErgoVal __t888 = ergo_g_main_conv_to_dd; ergo_retain_val(__t888);
  ErgoVal __t889 = names__116; ergo_retain_val(__t889);
  ergo_m_cogito_Dropdown_set_items(__t888, __t889);
  ergo_release_val(__t888);
  ergo_release_val(__t889);
  ErgoVal __t890 = EV_NULLV;
  ergo_release_val(__t890);
  ErgoVal __t891 = ergo_g_main_conv_to_dd; ergo_retain_val(__t891);
  ErgoVal __t892 = EV_INT(1);
  ergo_m_cogito_Dropdown_set_selected(__t891, __t892);
  ergo_release_val(__t891);
  ergo_release_val(__t892);
  ErgoVal __t893 = EV_NULLV;
  ergo_release_val(__t893);
  ErgoVal __t894 = ergo_g_main_conv_to_dd; ergo_retain_val(__t894);
  ErgoVal __t895 = EV_BOOL(true);
  ergo_m_cogito_Dropdown_set_hexpand(__t894, __t895);
  ergo_release_val(__t894);
  ergo_release_val(__t895);
  ErgoVal __t896 = EV_NULLV;
  ergo_release_val(__t896);
  ErgoVal __t897 = ergo_g_main_conv_to_dd; ergo_retain_val(__t897);
  ErgoVal __t898 = EV_FN(ergo_fn_new(ergo_lambda_9, 1));
  ergo_m_cogito_Dropdown_on_change(__t897, __t898);
  ergo_release_val(__t897);
  ergo_release_val(__t898);
  ErgoVal __t899 = EV_NULLV;
  ergo_release_val(__t899);
  ErgoVal __t900 = to_row__119; ergo_retain_val(__t900);
  ErgoVal __t901 = ergo_g_main_conv_to_dd; ergo_retain_val(__t901);
  ergo_m_cogito_VStack_add(__t900, __t901);
  ergo_release_val(__t900);
  ergo_release_val(__t901);
  ErgoVal __t902 = EV_NULLV;
  ergo_release_val(__t902);
  ErgoVal __t903 = ergo_g_main_conv_output; ergo_retain_val(__t903);
  ErgoVal __t904 = EV_BOOL(true);
  ergo_m_cogito_TextField_set_hexpand(__t903, __t904);
  ergo_release_val(__t903);
  ergo_release_val(__t904);
  ErgoVal __t905 = EV_NULLV;
  ergo_release_val(__t905);
  ErgoVal __t906 = ergo_g_main_conv_output; ergo_retain_val(__t906);
  ErgoVal __t907 = EV_BOOL(false);
  ergo_m_cogito_TextField_set_editable(__t906, __t907);
  ergo_release_val(__t906);
  ergo_release_val(__t907);
  ErgoVal __t908 = EV_NULLV;
  ergo_release_val(__t908);
  ErgoVal __t909 = ergo_g_main_conv_output; ergo_retain_val(__t909);
  ErgoVal __t910 = EV_STR(stdr_str_lit("conv-output"));
  ErgoVal __t911 = EV_NULLV;
  {
    ErgoVal __parts144[1] = { __t910 };
    ErgoStr* __s145 = stdr_str_from_parts(1, __parts144);
    __t911 = EV_STR(__s145);
  }
  ergo_release_val(__t910);
  ergo_cogito_set_class(__t909, __t911);
  ergo_release_val(__t909);
  ergo_release_val(__t911);
  ErgoVal __t912 = EV_NULLV;
  ergo_release_val(__t912);
  ErgoVal __t913 = to_row__119; ergo_retain_val(__t913);
  ErgoVal __t914 = ergo_g_main_conv_output; ergo_retain_val(__t914);
  ergo_m_cogito_VStack_add(__t913, __t914);
  ergo_release_val(__t913);
  ergo_release_val(__t914);
  ErgoVal __t915 = EV_NULLV;
  ergo_release_val(__t915);
  ErgoVal __t916 = panel__112; ergo_retain_val(__t916);
  ErgoVal __t917 = to_row__119; ergo_retain_val(__t917);
  ergo_m_cogito_VStack_add(__t916, __t917);
  ergo_release_val(__t916);
  ergo_release_val(__t917);
  ErgoVal __t918 = EV_NULLV;
  ergo_release_val(__t918);
  ergo_main_do_convert();
  ErgoVal __t919 = EV_NULLV;
  ergo_release_val(__t919);
  ErgoVal __t920 = panel__112; ergo_retain_val(__t920);
  ergo_move_into(&__ret, __t920);
  return __ret;
  ergo_release_val(to_label__120);
  ergo_release_val(to_row__119);
  ergo_release_val(swap_btn__118);
  ergo_release_val(swap_row__117);
  ergo_release_val(names__116);
  ergo_release_val(from_label__115);
  ergo_release_val(from_row__114);
  ergo_release_val(cat_dd__113);
  ergo_release_val(panel__112);
  return __ret;
}

static void ergo_main_show_about_window(ErgoVal a0) {
  ErgoVal dlg__121 = EV_NULLV;
  ErgoVal __t921 = EV_STR(stdr_str_lit(""));
  ErgoVal __t922 = ergo_cogito_dialog(__t921);
  ergo_release_val(__t921);
  ergo_move_into(&dlg__121, __t922);
  ErgoVal root__122 = EV_NULLV;
  ErgoVal __t923 = ergo_cogito_vstack();
  ergo_move_into(&root__122, __t923);
  ErgoVal __t924 = root__122; ergo_retain_val(__t924);
  ErgoVal __t925 = EV_INT(12);
  ergo_m_cogito_VStack_set_gap(__t924, __t925);
  ergo_release_val(__t924);
  ergo_release_val(__t925);
  ErgoVal __t926 = EV_NULLV;
  ergo_release_val(__t926);
  ErgoVal __t927 = root__122; ergo_retain_val(__t927);
  ergo_m_cogito_VStack_align_center(__t927);
  ergo_release_val(__t927);
  ErgoVal __t928 = EV_NULLV;
  ergo_release_val(__t928);
  ErgoVal icon__123 = EV_NULLV;
  ErgoVal __t929 = EV_STR(stdr_str_lit("sf:equal"));
  ErgoVal __t930 = EV_NULLV;
  {
    ErgoVal __parts146[1] = { __t929 };
    ErgoStr* __s147 = stdr_str_from_parts(1, __parts146);
    __t930 = EV_STR(__s147);
  }
  ergo_release_val(__t929);
  ErgoVal __t931 = ergo_cogito_image(__t930);
  ergo_release_val(__t930);
  ergo_move_into(&icon__123, __t931);
  ErgoVal __t932 = icon__123; ergo_retain_val(__t932);
  ErgoVal __t933 = EV_STR(stdr_str_lit("about-window-icon"));
  ErgoVal __t934 = EV_NULLV;
  {
    ErgoVal __parts148[1] = { __t933 };
    ErgoStr* __s149 = stdr_str_from_parts(1, __parts148);
    __t934 = EV_STR(__s149);
  }
  ergo_release_val(__t933);
  ergo_cogito_set_class(__t932, __t934);
  ergo_release_val(__t932);
  ergo_release_val(__t934);
  ErgoVal __t935 = EV_NULLV;
  ergo_release_val(__t935);
  ErgoVal __t936 = root__122; ergo_retain_val(__t936);
  ErgoVal __t937 = icon__123; ergo_retain_val(__t937);
  ergo_m_cogito_VStack_add(__t936, __t937);
  ergo_release_val(__t936);
  ergo_release_val(__t937);
  ErgoVal __t938 = EV_NULLV;
  ergo_release_val(__t938);
  ErgoVal title__124 = EV_NULLV;
  ErgoVal __t939 = EV_STR(stdr_str_lit("ErgoCalc"));
  ErgoVal __t940 = EV_NULLV;
  {
    ErgoVal __parts150[1] = { __t939 };
    ErgoStr* __s151 = stdr_str_from_parts(1, __parts150);
    __t940 = EV_STR(__s151);
  }
  ergo_release_val(__t939);
  ErgoVal __t941 = ergo_cogito_label(__t940);
  ergo_release_val(__t940);
  ergo_move_into(&title__124, __t941);
  ErgoVal __t942 = title__124; ergo_retain_val(__t942);
  ErgoVal __t943 = EV_STR(stdr_str_lit("about-window-title"));
  ErgoVal __t944 = EV_NULLV;
  {
    ErgoVal __parts152[1] = { __t943 };
    ErgoStr* __s153 = stdr_str_from_parts(1, __parts152);
    __t944 = EV_STR(__s153);
  }
  ergo_release_val(__t943);
  ergo_cogito_set_class(__t942, __t944);
  ergo_release_val(__t942);
  ergo_release_val(__t944);
  ErgoVal __t945 = EV_NULLV;
  ergo_release_val(__t945);
  ErgoVal __t946 = title__124; ergo_retain_val(__t946);
  ErgoVal __t947 = EV_INT(1);
  ergo_m_cogito_Label_set_text_align(__t946, __t947);
  ergo_release_val(__t946);
  ergo_release_val(__t947);
  ErgoVal __t948 = EV_NULLV;
  ergo_release_val(__t948);
  ErgoVal __t949 = root__122; ergo_retain_val(__t949);
  ErgoVal __t950 = title__124; ergo_retain_val(__t950);
  ergo_m_cogito_VStack_add(__t949, __t950);
  ergo_release_val(__t949);
  ergo_release_val(__t950);
  ErgoVal __t951 = EV_NULLV;
  ergo_release_val(__t951);
  ErgoVal license__125 = EV_NULLV;
  ErgoVal __t952 = EV_STR(stdr_str_lit("MIT License"));
  ErgoVal __t953 = EV_NULLV;
  {
    ErgoVal __parts154[1] = { __t952 };
    ErgoStr* __s155 = stdr_str_from_parts(1, __parts154);
    __t953 = EV_STR(__s155);
  }
  ergo_release_val(__t952);
  ErgoVal __t954 = ergo_cogito_label(__t953);
  ergo_release_val(__t953);
  ergo_move_into(&license__125, __t954);
  ErgoVal __t955 = license__125; ergo_retain_val(__t955);
  ErgoVal __t956 = EV_STR(stdr_str_lit("about-window-license"));
  ErgoVal __t957 = EV_NULLV;
  {
    ErgoVal __parts156[1] = { __t956 };
    ErgoStr* __s157 = stdr_str_from_parts(1, __parts156);
    __t957 = EV_STR(__s157);
  }
  ergo_release_val(__t956);
  ergo_cogito_set_class(__t955, __t957);
  ergo_release_val(__t955);
  ergo_release_val(__t957);
  ErgoVal __t958 = EV_NULLV;
  ergo_release_val(__t958);
  ErgoVal __t959 = license__125; ergo_retain_val(__t959);
  ErgoVal __t960 = EV_INT(1);
  ergo_m_cogito_Label_set_text_align(__t959, __t960);
  ergo_release_val(__t959);
  ergo_release_val(__t960);
  ErgoVal __t961 = EV_NULLV;
  ergo_release_val(__t961);
  ErgoVal __t962 = root__122; ergo_retain_val(__t962);
  ErgoVal __t963 = license__125; ergo_retain_val(__t963);
  ergo_m_cogito_VStack_add(__t962, __t963);
  ergo_release_val(__t962);
  ergo_release_val(__t963);
  ErgoVal __t964 = EV_NULLV;
  ergo_release_val(__t964);
  ErgoVal links__126 = EV_NULLV;
  ErgoVal __t965 = ergo_cogito_hstack();
  ergo_move_into(&links__126, __t965);
  ErgoVal __t966 = links__126; ergo_retain_val(__t966);
  ErgoVal __t967 = EV_INT(12);
  ergo_m_cogito_HStack_set_gap(__t966, __t967);
  ergo_release_val(__t966);
  ergo_release_val(__t967);
  ErgoVal __t968 = EV_NULLV;
  ergo_release_val(__t968);
  ErgoVal __t969 = links__126; ergo_retain_val(__t969);
  ergo_m_cogito_HStack_align_center(__t969);
  ergo_release_val(__t969);
  ErgoVal __t970 = EV_NULLV;
  ergo_release_val(__t970);
  ErgoVal __t971 = links__126; ergo_retain_val(__t971);
  ErgoVal __t972 = EV_STR(stdr_str_lit("about-window-actions"));
  ErgoVal __t973 = EV_NULLV;
  {
    ErgoVal __parts158[1] = { __t972 };
    ErgoStr* __s159 = stdr_str_from_parts(1, __parts158);
    __t973 = EV_STR(__s159);
  }
  ergo_release_val(__t972);
  ergo_cogito_set_class(__t971, __t973);
  ergo_release_val(__t971);
  ergo_release_val(__t973);
  ErgoVal __t974 = EV_NULLV;
  ergo_release_val(__t974);
  ErgoVal more_btn__127 = EV_NULLV;
  ErgoVal __t975 = EV_STR(stdr_str_lit("More info"));
  ErgoVal __t976 = EV_NULLV;
  {
    ErgoVal __parts160[1] = { __t975 };
    ErgoStr* __s161 = stdr_str_from_parts(1, __parts160);
    __t976 = EV_STR(__s161);
  }
  ergo_release_val(__t975);
  ErgoVal __t977 = ergo_cogito_button(__t976);
  ergo_release_val(__t976);
  ergo_move_into(&more_btn__127, __t977);
  ErgoVal __t978 = more_btn__127; ergo_retain_val(__t978);
  ErgoVal __t979 = EV_STR(stdr_str_lit("outlined"));
  ErgoVal __t980 = EV_NULLV;
  {
    ErgoVal __parts162[1] = { __t979 };
    ErgoStr* __s163 = stdr_str_from_parts(1, __parts162);
    __t980 = EV_STR(__s163);
  }
  ergo_release_val(__t979);
  ergo_cogito_set_class(__t978, __t980);
  ergo_release_val(__t978);
  ergo_release_val(__t980);
  ErgoVal __t981 = EV_NULLV;
  ergo_release_val(__t981);
  ErgoVal __t982 = more_btn__127; ergo_retain_val(__t982);
  ErgoVal* __env164 = (ErgoVal*)malloc(sizeof(ErgoVal) * 1);
  __env164[0] = dlg__121; ergo_retain_val(__env164[0]);
  ErgoVal __t983 = EV_FN(ergo_fn_new_with_env(ergo_lambda_10, 1, __env164, 1));
  ergo_m_cogito_Button_on_click(__t982, __t983);
  ergo_release_val(__t982);
  ergo_release_val(__t983);
  ErgoVal __t984 = EV_NULLV;
  ergo_release_val(__t984);
  ErgoVal __t985 = links__126; ergo_retain_val(__t985);
  ErgoVal __t986 = more_btn__127; ergo_retain_val(__t986);
  ergo_m_cogito_HStack_add(__t985, __t986);
  ergo_release_val(__t985);
  ergo_release_val(__t986);
  ErgoVal __t987 = EV_NULLV;
  ergo_release_val(__t987);
  ErgoVal bug_btn__128 = EV_NULLV;
  ErgoVal __t988 = EV_STR(stdr_str_lit("Report a Bug"));
  ErgoVal __t989 = EV_NULLV;
  {
    ErgoVal __parts165[1] = { __t988 };
    ErgoStr* __s166 = stdr_str_from_parts(1, __parts165);
    __t989 = EV_STR(__s166);
  }
  ergo_release_val(__t988);
  ErgoVal __t990 = ergo_cogito_button(__t989);
  ergo_release_val(__t989);
  ergo_move_into(&bug_btn__128, __t990);
  ErgoVal __t991 = bug_btn__128; ergo_retain_val(__t991);
  ErgoVal __t992 = EV_STR(stdr_str_lit("outlined"));
  ErgoVal __t993 = EV_NULLV;
  {
    ErgoVal __parts167[1] = { __t992 };
    ErgoStr* __s168 = stdr_str_from_parts(1, __parts167);
    __t993 = EV_STR(__s168);
  }
  ergo_release_val(__t992);
  ergo_cogito_set_class(__t991, __t993);
  ergo_release_val(__t991);
  ergo_release_val(__t993);
  ErgoVal __t994 = EV_NULLV;
  ergo_release_val(__t994);
  ErgoVal __t995 = bug_btn__128; ergo_retain_val(__t995);
  ErgoVal* __env169 = (ErgoVal*)malloc(sizeof(ErgoVal) * 1);
  __env169[0] = dlg__121; ergo_retain_val(__env169[0]);
  ErgoVal __t996 = EV_FN(ergo_fn_new_with_env(ergo_lambda_11, 1, __env169, 1));
  ergo_m_cogito_Button_on_click(__t995, __t996);
  ergo_release_val(__t995);
  ergo_release_val(__t996);
  ErgoVal __t997 = EV_NULLV;
  ergo_release_val(__t997);
  ErgoVal __t998 = links__126; ergo_retain_val(__t998);
  ErgoVal __t999 = bug_btn__128; ergo_retain_val(__t999);
  ergo_m_cogito_HStack_add(__t998, __t999);
  ergo_release_val(__t998);
  ergo_release_val(__t999);
  ErgoVal __t1000 = EV_NULLV;
  ergo_release_val(__t1000);
  ErgoVal __t1001 = root__122; ergo_retain_val(__t1001);
  ErgoVal __t1002 = links__126; ergo_retain_val(__t1002);
  ergo_m_cogito_VStack_add(__t1001, __t1002);
  ergo_release_val(__t1001);
  ergo_release_val(__t1002);
  ErgoVal __t1003 = EV_NULLV;
  ergo_release_val(__t1003);
  ErgoVal __t1004 = dlg__121; ergo_retain_val(__t1004);
  ErgoVal __t1005 = root__122; ergo_retain_val(__t1005);
  ergo_m_cogito_Dialog_add(__t1004, __t1005);
  ergo_release_val(__t1004);
  ergo_release_val(__t1005);
  ErgoVal __t1006 = EV_NULLV;
  ergo_release_val(__t1006);
  ErgoVal __t1007 = a0; ergo_retain_val(__t1007);
  ErgoVal __t1008 = dlg__121; ergo_retain_val(__t1008);
  ergo_m_cogito_Window_set_dialog(__t1007, __t1008);
  ergo_release_val(__t1007);
  ergo_release_val(__t1008);
  ErgoVal __t1009 = EV_NULLV;
  ergo_release_val(__t1009);
  ergo_release_val(bug_btn__128);
  ergo_release_val(more_btn__127);
  ergo_release_val(links__126);
  ergo_release_val(license__125);
  ergo_release_val(title__124);
  ergo_release_val(icon__123);
  ergo_release_val(root__122);
  ergo_release_val(dlg__121);
}

static void ergo_main_build_ui(ErgoVal a0) {
  ErgoVal root__129 = EV_NULLV;
  ErgoVal __t1010 = ergo_cogito_vstack();
  ergo_move_into(&root__129, __t1010);
  ErgoVal __t1011 = root__129; ergo_retain_val(__t1011);
  ErgoVal __t1012 = EV_STR(stdr_str_lit("calc-root"));
  ErgoVal __t1013 = EV_NULLV;
  {
    ErgoVal __parts170[1] = { __t1012 };
    ErgoStr* __s171 = stdr_str_from_parts(1, __parts170);
    __t1013 = EV_STR(__s171);
  }
  ergo_release_val(__t1012);
  ergo_cogito_set_class(__t1011, __t1013);
  ergo_release_val(__t1011);
  ergo_release_val(__t1013);
  ErgoVal __t1014 = EV_NULLV;
  ergo_release_val(__t1014);
  ErgoVal switcher__130 = EV_NULLV;
  ErgoVal __t1015 = ergo_cogito_view_switcher();
  ergo_move_into(&switcher__130, __t1015);
  ErgoVal bar__131 = EV_NULLV;
  ErgoVal __t1016 = EV_STR(stdr_str_lit(""));
  ErgoVal __t1017 = EV_STR(stdr_str_lit(""));
  ErgoVal __t1018 = ergo_cogito_appbar(__t1016, __t1017);
  ergo_release_val(__t1016);
  ergo_release_val(__t1017);
  ergo_move_into(&bar__131, __t1018);
  ErgoVal __t1019 = bar__131; ergo_retain_val(__t1019);
  ErgoVal __t1020 = EV_BOOL(true);
  ergo_m_cogito_AppBar_set_hexpand(__t1019, __t1020);
  ergo_release_val(__t1019);
  ergo_release_val(__t1020);
  ErgoVal __t1021 = EV_NULLV;
  ergo_release_val(__t1021);
  ErgoVal toggle_btn__132 = EV_NULLV;
  ErgoVal __t1022 = bar__131; ergo_retain_val(__t1022);
  ErgoVal __t1023 = EV_STR(stdr_str_lit("sf:arrow.left.arrow.right"));
  ErgoVal __t1024 = EV_NULLV;
  {
    ErgoVal __parts172[1] = { __t1023 };
    ErgoStr* __s173 = stdr_str_from_parts(1, __parts172);
    __t1024 = EV_STR(__s173);
  }
  ergo_release_val(__t1023);
  ErgoVal* __env174 = (ErgoVal*)malloc(sizeof(ErgoVal) * 1);
  __env174[0] = switcher__130; ergo_retain_val(__env174[0]);
  ErgoVal __t1025 = EV_FN(ergo_fn_new_with_env(ergo_lambda_12, 1, __env174, 1));
  ErgoVal __t1026 = ergo_m_cogito_AppBar_add_button(__t1022, __t1024, __t1025);
  ergo_release_val(__t1022);
  ergo_release_val(__t1024);
  ergo_release_val(__t1025);
  ergo_move_into(&toggle_btn__132, __t1026);
  ErgoVal __t1027 = toggle_btn__132; ergo_retain_val(__t1027);
  ErgoVal __t1028 = EV_STR(stdr_str_lit("Unit Converter"));
  ErgoVal __t1029 = EV_NULLV;
  {
    ErgoVal __parts175[1] = { __t1028 };
    ErgoStr* __s176 = stdr_str_from_parts(1, __parts175);
    __t1029 = EV_STR(__s176);
  }
  ergo_release_val(__t1028);
  ergo_cogito_set_tooltip(__t1027, __t1029);
  ergo_release_val(__t1027);
  ergo_release_val(__t1029);
  ErgoVal __t1030 = EV_NULLV;
  ergo_release_val(__t1030);
  ErgoVal about_btn__133 = EV_NULLV;
  ErgoVal __t1031 = bar__131; ergo_retain_val(__t1031);
  ErgoVal __t1032 = EV_STR(stdr_str_lit("sf:questionmark"));
  ErgoVal __t1033 = EV_NULLV;
  {
    ErgoVal __parts177[1] = { __t1032 };
    ErgoStr* __s178 = stdr_str_from_parts(1, __parts177);
    __t1033 = EV_STR(__s178);
  }
  ergo_release_val(__t1032);
  ErgoVal* __env179 = (ErgoVal*)malloc(sizeof(ErgoVal) * 1);
  __env179[0] = a0; ergo_retain_val(__env179[0]);
  ErgoVal __t1034 = EV_FN(ergo_fn_new_with_env(ergo_lambda_13, 1, __env179, 1));
  ErgoVal __t1035 = ergo_m_cogito_AppBar_add_button(__t1031, __t1033, __t1034);
  ergo_release_val(__t1031);
  ergo_release_val(__t1033);
  ergo_release_val(__t1034);
  ergo_move_into(&about_btn__133, __t1035);
  ErgoVal __t1036 = about_btn__133; ergo_retain_val(__t1036);
  ErgoVal __t1037 = EV_STR(stdr_str_lit("About ErgoCalc"));
  ErgoVal __t1038 = EV_NULLV;
  {
    ErgoVal __parts180[1] = { __t1037 };
    ErgoStr* __s181 = stdr_str_from_parts(1, __parts180);
    __t1038 = EV_STR(__s181);
  }
  ergo_release_val(__t1037);
  ergo_cogito_set_tooltip(__t1036, __t1038);
  ergo_release_val(__t1036);
  ergo_release_val(__t1038);
  ErgoVal __t1039 = EV_NULLV;
  ergo_release_val(__t1039);
  ErgoVal __t1040 = root__129; ergo_retain_val(__t1040);
  ErgoVal __t1041 = bar__131; ergo_retain_val(__t1041);
  ergo_m_cogito_VStack_add(__t1040, __t1041);
  ergo_release_val(__t1040);
  ergo_release_val(__t1041);
  ErgoVal __t1042 = EV_NULLV;
  ergo_release_val(__t1042);
  ErgoVal calc_view__134 = EV_NULLV;
  ErgoVal __t1043 = ergo_cogito_vstack();
  ergo_move_into(&calc_view__134, __t1043);
  ErgoVal __t1044 = calc_view__134; ergo_retain_val(__t1044);
  ErgoVal __t1045 = EV_BOOL(true);
  ergo_m_cogito_VStack_set_hexpand(__t1044, __t1045);
  ergo_release_val(__t1044);
  ergo_release_val(__t1045);
  ErgoVal __t1046 = EV_NULLV;
  ergo_release_val(__t1046);
  ErgoVal __t1047 = calc_view__134; ergo_retain_val(__t1047);
  ErgoVal __t1048 = EV_BOOL(true);
  ergo_m_cogito_VStack_set_vexpand(__t1047, __t1048);
  ergo_release_val(__t1047);
  ergo_release_val(__t1048);
  ErgoVal __t1049 = EV_NULLV;
  ergo_release_val(__t1049);
  ErgoVal __t1050 = calc_view__134; ergo_retain_val(__t1050);
  ErgoVal __t1051 = EV_STR(stdr_str_lit("calculator"));
  ErgoVal __t1052 = EV_NULLV;
  {
    ErgoVal __parts182[1] = { __t1051 };
    ErgoStr* __s183 = stdr_str_from_parts(1, __parts182);
    __t1052 = EV_STR(__s183);
  }
  ergo_release_val(__t1051);
  ergo_cogito_set_id(__t1050, __t1052);
  ergo_release_val(__t1050);
  ergo_release_val(__t1052);
  ErgoVal __t1053 = EV_NULLV;
  ergo_release_val(__t1053);
  ErgoVal display_box__135 = EV_NULLV;
  ErgoVal __t1054 = ergo_cogito_vstack();
  ergo_move_into(&display_box__135, __t1054);
  ErgoVal __t1055 = display_box__135; ergo_retain_val(__t1055);
  ErgoVal __t1056 = EV_BOOL(true);
  ergo_m_cogito_VStack_set_hexpand(__t1055, __t1056);
  ergo_release_val(__t1055);
  ergo_release_val(__t1056);
  ErgoVal __t1057 = EV_NULLV;
  ergo_release_val(__t1057);
  ErgoVal __t1058 = display_box__135; ergo_retain_val(__t1058);
  ErgoVal __t1059 = EV_INT(4);
  ergo_m_cogito_VStack_set_gap(__t1058, __t1059);
  ergo_release_val(__t1058);
  ergo_release_val(__t1059);
  ErgoVal __t1060 = EV_NULLV;
  ergo_release_val(__t1060);
  ErgoVal __t1061 = display_box__135; ergo_retain_val(__t1061);
  ErgoVal __t1062 = EV_STR(stdr_str_lit("calc-display"));
  ErgoVal __t1063 = EV_NULLV;
  {
    ErgoVal __parts184[1] = { __t1062 };
    ErgoStr* __s185 = stdr_str_from_parts(1, __parts184);
    __t1063 = EV_STR(__s185);
  }
  ergo_release_val(__t1062);
  ergo_cogito_set_class(__t1061, __t1063);
  ergo_release_val(__t1061);
  ergo_release_val(__t1063);
  ErgoVal __t1064 = EV_NULLV;
  ergo_release_val(__t1064);
  ErgoVal __t1065 = ergo_g_main_display_expression; ergo_retain_val(__t1065);
  ErgoVal __t1066 = EV_BOOL(true);
  ergo_m_cogito_Label_set_hexpand(__t1065, __t1066);
  ergo_release_val(__t1065);
  ergo_release_val(__t1066);
  ErgoVal __t1067 = EV_NULLV;
  ergo_release_val(__t1067);
  ErgoVal __t1068 = ergo_g_main_display_expression; ergo_retain_val(__t1068);
  ErgoVal __t1069 = EV_INT(2);
  ergo_m_cogito_Label_set_text_align(__t1068, __t1069);
  ergo_release_val(__t1068);
  ergo_release_val(__t1069);
  ErgoVal __t1070 = EV_NULLV;
  ergo_release_val(__t1070);
  ErgoVal __t1071 = ergo_g_main_display_expression; ergo_retain_val(__t1071);
  ErgoVal __t1072 = EV_STR(stdr_str_lit("calc-display-expression"));
  ErgoVal __t1073 = EV_NULLV;
  {
    ErgoVal __parts186[1] = { __t1072 };
    ErgoStr* __s187 = stdr_str_from_parts(1, __parts186);
    __t1073 = EV_STR(__s187);
  }
  ergo_release_val(__t1072);
  ergo_cogito_set_class(__t1071, __t1073);
  ergo_release_val(__t1071);
  ergo_release_val(__t1073);
  ErgoVal __t1074 = EV_NULLV;
  ergo_release_val(__t1074);
  ErgoVal __t1075 = display_box__135; ergo_retain_val(__t1075);
  ErgoVal __t1076 = ergo_g_main_display_expression; ergo_retain_val(__t1076);
  ergo_m_cogito_VStack_add(__t1075, __t1076);
  ergo_release_val(__t1075);
  ergo_release_val(__t1076);
  ErgoVal __t1077 = EV_NULLV;
  ergo_release_val(__t1077);
  ErgoVal __t1078 = ergo_g_main_display_working; ergo_retain_val(__t1078);
  ErgoVal __t1079 = EV_BOOL(true);
  ergo_m_cogito_Label_set_hexpand(__t1078, __t1079);
  ergo_release_val(__t1078);
  ergo_release_val(__t1079);
  ErgoVal __t1080 = EV_NULLV;
  ergo_release_val(__t1080);
  ErgoVal __t1081 = ergo_g_main_display_working; ergo_retain_val(__t1081);
  ErgoVal __t1082 = EV_INT(2);
  ergo_m_cogito_Label_set_text_align(__t1081, __t1082);
  ergo_release_val(__t1081);
  ergo_release_val(__t1082);
  ErgoVal __t1083 = EV_NULLV;
  ergo_release_val(__t1083);
  ErgoVal __t1084 = ergo_g_main_display_working; ergo_retain_val(__t1084);
  ErgoVal __t1085 = EV_STR(stdr_str_lit("calc-display-working"));
  ErgoVal __t1086 = EV_NULLV;
  {
    ErgoVal __parts188[1] = { __t1085 };
    ErgoStr* __s189 = stdr_str_from_parts(1, __parts188);
    __t1086 = EV_STR(__s189);
  }
  ergo_release_val(__t1085);
  ergo_cogito_set_class(__t1084, __t1086);
  ergo_release_val(__t1084);
  ergo_release_val(__t1086);
  ErgoVal __t1087 = EV_NULLV;
  ergo_release_val(__t1087);
  ErgoVal __t1088 = display_box__135; ergo_retain_val(__t1088);
  ErgoVal __t1089 = ergo_g_main_display_working; ergo_retain_val(__t1089);
  ergo_m_cogito_VStack_add(__t1088, __t1089);
  ergo_release_val(__t1088);
  ergo_release_val(__t1089);
  ErgoVal __t1090 = EV_NULLV;
  ergo_release_val(__t1090);
  ErgoVal __t1091 = calc_view__134; ergo_retain_val(__t1091);
  ErgoVal __t1092 = display_box__135; ergo_retain_val(__t1092);
  ergo_m_cogito_VStack_add(__t1091, __t1092);
  ergo_release_val(__t1091);
  ergo_release_val(__t1092);
  ErgoVal __t1093 = EV_NULLV;
  ergo_release_val(__t1093);
  ErgoVal keypad__136 = EV_NULLV;
  ErgoVal __t1094 = EV_INT(5);
  ErgoVal __t1095 = ergo_cogito_grid(__t1094);
  ergo_release_val(__t1094);
  ergo_move_into(&keypad__136, __t1095);
  ErgoVal __t1096 = keypad__136; ergo_retain_val(__t1096);
  ErgoVal __t1097 = EV_BOOL(true);
  ergo_m_cogito_Grid_set_hexpand(__t1096, __t1097);
  ergo_release_val(__t1096);
  ergo_release_val(__t1097);
  ErgoVal __t1098 = EV_NULLV;
  ergo_release_val(__t1098);
  ErgoVal __t1099 = keypad__136; ergo_retain_val(__t1099);
  ErgoVal __t1100 = EV_BOOL(true);
  ergo_m_cogito_Grid_set_vexpand(__t1099, __t1100);
  ergo_release_val(__t1099);
  ergo_release_val(__t1100);
  ErgoVal __t1101 = EV_NULLV;
  ergo_release_val(__t1101);
  ErgoVal __t1102 = keypad__136; ergo_retain_val(__t1102);
  ErgoVal __t1103 = EV_STR(stdr_str_lit("calc-keypad"));
  ErgoVal __t1104 = EV_NULLV;
  {
    ErgoVal __parts190[1] = { __t1103 };
    ErgoStr* __s191 = stdr_str_from_parts(1, __parts190);
    __t1104 = EV_STR(__s191);
  }
  ergo_release_val(__t1103);
  ergo_cogito_set_class(__t1102, __t1104);
  ergo_release_val(__t1102);
  ergo_release_val(__t1104);
  ErgoVal __t1105 = EV_NULLV;
  ergo_release_val(__t1105);
  ErgoVal __t1106 = keypad__136; ergo_retain_val(__t1106);
  ErgoVal __t1107 = EV_INT(7);
  ErgoVal __t1108 = ergo_main_digit_button(__t1107);
  ergo_release_val(__t1107);
  ergo_m_cogito_Grid_add(__t1106, __t1108);
  ergo_release_val(__t1106);
  ergo_release_val(__t1108);
  ErgoVal __t1109 = EV_NULLV;
  ergo_release_val(__t1109);
  ErgoVal __t1110 = keypad__136; ergo_retain_val(__t1110);
  ErgoVal __t1111 = EV_INT(8);
  ErgoVal __t1112 = ergo_main_digit_button(__t1111);
  ergo_release_val(__t1111);
  ergo_m_cogito_Grid_add(__t1110, __t1112);
  ergo_release_val(__t1110);
  ergo_release_val(__t1112);
  ErgoVal __t1113 = EV_NULLV;
  ergo_release_val(__t1113);
  ErgoVal __t1114 = keypad__136; ergo_retain_val(__t1114);
  ErgoVal __t1115 = EV_INT(9);
  ErgoVal __t1116 = ergo_main_digit_button(__t1115);
  ergo_release_val(__t1115);
  ergo_m_cogito_Grid_add(__t1114, __t1116);
  ergo_release_val(__t1114);
  ergo_release_val(__t1116);
  ErgoVal __t1117 = EV_NULLV;
  ergo_release_val(__t1117);
  ErgoVal c__137 = EV_NULLV;
  ErgoVal __t1118 = ergo_main_clear_button();
  ergo_move_into(&c__137, __t1118);
  ErgoVal __t1119 = keypad__136; ergo_retain_val(__t1119);
  ErgoVal __t1120 = c__137; ergo_retain_val(__t1120);
  ergo_m_cogito_Grid_add(__t1119, __t1120);
  ergo_release_val(__t1119);
  ergo_release_val(__t1120);
  ErgoVal __t1121 = EV_NULLV;
  ergo_release_val(__t1121);
  ErgoVal __t1122 = keypad__136; ergo_retain_val(__t1122);
  ErgoVal __t1123 = c__137; ergo_retain_val(__t1123);
  ErgoVal __t1124 = EV_INT(2);
  ErgoVal __t1125 = EV_INT(1);
  ergo_m_cogito_Grid_set_span(__t1122, __t1123, __t1124, __t1125);
  ergo_release_val(__t1122);
  ergo_release_val(__t1123);
  ergo_release_val(__t1124);
  ergo_release_val(__t1125);
  ErgoVal __t1126 = EV_NULLV;
  ergo_release_val(__t1126);
  ErgoVal __t1127 = keypad__136; ergo_retain_val(__t1127);
  ErgoVal __t1128 = EV_INT(4);
  ErgoVal __t1129 = ergo_main_digit_button(__t1128);
  ergo_release_val(__t1128);
  ergo_m_cogito_Grid_add(__t1127, __t1129);
  ergo_release_val(__t1127);
  ergo_release_val(__t1129);
  ErgoVal __t1130 = EV_NULLV;
  ergo_release_val(__t1130);
  ErgoVal __t1131 = keypad__136; ergo_retain_val(__t1131);
  ErgoVal __t1132 = EV_INT(5);
  ErgoVal __t1133 = ergo_main_digit_button(__t1132);
  ergo_release_val(__t1132);
  ergo_m_cogito_Grid_add(__t1131, __t1133);
  ergo_release_val(__t1131);
  ergo_release_val(__t1133);
  ErgoVal __t1134 = EV_NULLV;
  ergo_release_val(__t1134);
  ErgoVal __t1135 = keypad__136; ergo_retain_val(__t1135);
  ErgoVal __t1136 = EV_INT(6);
  ErgoVal __t1137 = ergo_main_digit_button(__t1136);
  ergo_release_val(__t1136);
  ergo_m_cogito_Grid_add(__t1135, __t1137);
  ergo_release_val(__t1135);
  ergo_release_val(__t1137);
  ErgoVal __t1138 = EV_NULLV;
  ergo_release_val(__t1138);
  ErgoVal __t1139 = keypad__136; ergo_retain_val(__t1139);
  ErgoVal __t1140 = EV_STR(stdr_str_lit("/"));
  ErgoVal __t1141 = EV_NULLV;
  {
    ErgoVal __parts192[1] = { __t1140 };
    ErgoStr* __s193 = stdr_str_from_parts(1, __parts192);
    __t1141 = EV_STR(__s193);
  }
  ergo_release_val(__t1140);
  ErgoVal __t1142 = EV_STR(stdr_str_lit("/"));
  ErgoVal __t1143 = EV_NULLV;
  {
    ErgoVal __parts194[1] = { __t1142 };
    ErgoStr* __s195 = stdr_str_from_parts(1, __parts194);
    __t1143 = EV_STR(__s195);
  }
  ergo_release_val(__t1142);
  ErgoVal __t1144 = ergo_main_operator_button(__t1141, __t1143);
  ergo_release_val(__t1141);
  ergo_release_val(__t1143);
  ergo_m_cogito_Grid_add(__t1139, __t1144);
  ergo_release_val(__t1139);
  ergo_release_val(__t1144);
  ErgoVal __t1145 = EV_NULLV;
  ergo_release_val(__t1145);
  ErgoVal __t1146 = keypad__136; ergo_retain_val(__t1146);
  ErgoVal __t1147 = EV_STR(stdr_str_lit("*"));
  ErgoVal __t1148 = EV_NULLV;
  {
    ErgoVal __parts196[1] = { __t1147 };
    ErgoStr* __s197 = stdr_str_from_parts(1, __parts196);
    __t1148 = EV_STR(__s197);
  }
  ergo_release_val(__t1147);
  ErgoVal __t1149 = EV_STR(stdr_str_lit("*"));
  ErgoVal __t1150 = EV_NULLV;
  {
    ErgoVal __parts198[1] = { __t1149 };
    ErgoStr* __s199 = stdr_str_from_parts(1, __parts198);
    __t1150 = EV_STR(__s199);
  }
  ergo_release_val(__t1149);
  ErgoVal __t1151 = ergo_main_operator_button(__t1148, __t1150);
  ergo_release_val(__t1148);
  ergo_release_val(__t1150);
  ergo_m_cogito_Grid_add(__t1146, __t1151);
  ergo_release_val(__t1146);
  ergo_release_val(__t1151);
  ErgoVal __t1152 = EV_NULLV;
  ergo_release_val(__t1152);
  ErgoVal __t1153 = keypad__136; ergo_retain_val(__t1153);
  ErgoVal __t1154 = EV_INT(1);
  ErgoVal __t1155 = ergo_main_digit_button(__t1154);
  ergo_release_val(__t1154);
  ergo_m_cogito_Grid_add(__t1153, __t1155);
  ergo_release_val(__t1153);
  ergo_release_val(__t1155);
  ErgoVal __t1156 = EV_NULLV;
  ergo_release_val(__t1156);
  ErgoVal __t1157 = keypad__136; ergo_retain_val(__t1157);
  ErgoVal __t1158 = EV_INT(2);
  ErgoVal __t1159 = ergo_main_digit_button(__t1158);
  ergo_release_val(__t1158);
  ergo_m_cogito_Grid_add(__t1157, __t1159);
  ergo_release_val(__t1157);
  ergo_release_val(__t1159);
  ErgoVal __t1160 = EV_NULLV;
  ergo_release_val(__t1160);
  ErgoVal __t1161 = keypad__136; ergo_retain_val(__t1161);
  ErgoVal __t1162 = EV_INT(3);
  ErgoVal __t1163 = ergo_main_digit_button(__t1162);
  ergo_release_val(__t1162);
  ergo_m_cogito_Grid_add(__t1161, __t1163);
  ergo_release_val(__t1161);
  ergo_release_val(__t1163);
  ErgoVal __t1164 = EV_NULLV;
  ergo_release_val(__t1164);
  ErgoVal __t1165 = keypad__136; ergo_retain_val(__t1165);
  ErgoVal __t1166 = EV_STR(stdr_str_lit("+"));
  ErgoVal __t1167 = EV_NULLV;
  {
    ErgoVal __parts200[1] = { __t1166 };
    ErgoStr* __s201 = stdr_str_from_parts(1, __parts200);
    __t1167 = EV_STR(__s201);
  }
  ergo_release_val(__t1166);
  ErgoVal __t1168 = EV_STR(stdr_str_lit("+"));
  ErgoVal __t1169 = EV_NULLV;
  {
    ErgoVal __parts202[1] = { __t1168 };
    ErgoStr* __s203 = stdr_str_from_parts(1, __parts202);
    __t1169 = EV_STR(__s203);
  }
  ergo_release_val(__t1168);
  ErgoVal __t1170 = ergo_main_operator_button(__t1167, __t1169);
  ergo_release_val(__t1167);
  ergo_release_val(__t1169);
  ergo_m_cogito_Grid_add(__t1165, __t1170);
  ergo_release_val(__t1165);
  ergo_release_val(__t1170);
  ErgoVal __t1171 = EV_NULLV;
  ergo_release_val(__t1171);
  ErgoVal __t1172 = keypad__136; ergo_retain_val(__t1172);
  ErgoVal __t1173 = EV_STR(stdr_str_lit("-"));
  ErgoVal __t1174 = EV_NULLV;
  {
    ErgoVal __parts204[1] = { __t1173 };
    ErgoStr* __s205 = stdr_str_from_parts(1, __parts204);
    __t1174 = EV_STR(__s205);
  }
  ergo_release_val(__t1173);
  ErgoVal __t1175 = EV_STR(stdr_str_lit("-"));
  ErgoVal __t1176 = EV_NULLV;
  {
    ErgoVal __parts206[1] = { __t1175 };
    ErgoStr* __s207 = stdr_str_from_parts(1, __parts206);
    __t1176 = EV_STR(__s207);
  }
  ergo_release_val(__t1175);
  ErgoVal __t1177 = ergo_main_operator_button(__t1174, __t1176);
  ergo_release_val(__t1174);
  ergo_release_val(__t1176);
  ergo_m_cogito_Grid_add(__t1172, __t1177);
  ergo_release_val(__t1172);
  ergo_release_val(__t1177);
  ErgoVal __t1178 = EV_NULLV;
  ergo_release_val(__t1178);
  ErgoVal zero__138 = EV_NULLV;
  ErgoVal __t1179 = EV_INT(0);
  ErgoVal __t1180 = ergo_main_digit_button(__t1179);
  ergo_release_val(__t1179);
  ergo_move_into(&zero__138, __t1180);
  ErgoVal __t1181 = keypad__136; ergo_retain_val(__t1181);
  ErgoVal __t1182 = zero__138; ergo_retain_val(__t1182);
  ergo_m_cogito_Grid_add(__t1181, __t1182);
  ergo_release_val(__t1181);
  ergo_release_val(__t1182);
  ErgoVal __t1183 = EV_NULLV;
  ergo_release_val(__t1183);
  ErgoVal __t1184 = keypad__136; ergo_retain_val(__t1184);
  ErgoVal __t1185 = zero__138; ergo_retain_val(__t1185);
  ErgoVal __t1186 = EV_INT(3);
  ErgoVal __t1187 = EV_INT(1);
  ergo_m_cogito_Grid_set_span(__t1184, __t1185, __t1186, __t1187);
  ergo_release_val(__t1184);
  ergo_release_val(__t1185);
  ergo_release_val(__t1186);
  ergo_release_val(__t1187);
  ErgoVal __t1188 = EV_NULLV;
  ergo_release_val(__t1188);
  ErgoVal eq__139 = EV_NULLV;
  ErgoVal __t1189 = ergo_main_equals_button();
  ergo_move_into(&eq__139, __t1189);
  ErgoVal __t1190 = keypad__136; ergo_retain_val(__t1190);
  ErgoVal __t1191 = eq__139; ergo_retain_val(__t1191);
  ergo_m_cogito_Grid_add(__t1190, __t1191);
  ergo_release_val(__t1190);
  ergo_release_val(__t1191);
  ErgoVal __t1192 = EV_NULLV;
  ergo_release_val(__t1192);
  ErgoVal __t1193 = keypad__136; ergo_retain_val(__t1193);
  ErgoVal __t1194 = eq__139; ergo_retain_val(__t1194);
  ErgoVal __t1195 = EV_INT(2);
  ErgoVal __t1196 = EV_INT(1);
  ergo_m_cogito_Grid_set_span(__t1193, __t1194, __t1195, __t1196);
  ergo_release_val(__t1193);
  ergo_release_val(__t1194);
  ergo_release_val(__t1195);
  ergo_release_val(__t1196);
  ErgoVal __t1197 = EV_NULLV;
  ergo_release_val(__t1197);
  ErgoVal __t1198 = calc_view__134; ergo_retain_val(__t1198);
  ErgoVal __t1199 = keypad__136; ergo_retain_val(__t1199);
  ergo_m_cogito_VStack_add(__t1198, __t1199);
  ergo_release_val(__t1198);
  ergo_release_val(__t1199);
  ErgoVal __t1200 = EV_NULLV;
  ergo_release_val(__t1200);
  ErgoVal conv_view__140 = EV_NULLV;
  ErgoVal __t1201 = ergo_main_build_converter_ui();
  ergo_move_into(&conv_view__140, __t1201);
  ErgoVal __t1202 = conv_view__140; ergo_retain_val(__t1202);
  ErgoVal __t1203 = EV_STR(stdr_str_lit("converter"));
  ErgoVal __t1204 = EV_NULLV;
  {
    ErgoVal __parts208[1] = { __t1203 };
    ErgoStr* __s209 = stdr_str_from_parts(1, __parts208);
    __t1204 = EV_STR(__s209);
  }
  ergo_release_val(__t1203);
  ergo_cogito_set_id(__t1202, __t1204);
  ergo_release_val(__t1202);
  ergo_release_val(__t1204);
  ErgoVal __t1205 = EV_NULLV;
  ergo_release_val(__t1205);
  ErgoVal __t1206 = switcher__130; ergo_retain_val(__t1206);
  ErgoVal __t1207 = calc_view__134; ergo_retain_val(__t1207);
  ergo_m_cogito_ViewSwitcher_add(__t1206, __t1207);
  ergo_release_val(__t1206);
  ergo_release_val(__t1207);
  ErgoVal __t1208 = EV_NULLV;
  ergo_release_val(__t1208);
  ErgoVal __t1209 = switcher__130; ergo_retain_val(__t1209);
  ErgoVal __t1210 = conv_view__140; ergo_retain_val(__t1210);
  ergo_m_cogito_ViewSwitcher_add(__t1209, __t1210);
  ergo_release_val(__t1209);
  ergo_release_val(__t1210);
  ErgoVal __t1211 = EV_NULLV;
  ergo_release_val(__t1211);
  ErgoVal __t1212 = switcher__130; ergo_retain_val(__t1212);
  ErgoVal __t1213 = EV_STR(stdr_str_lit("calculator"));
  ErgoVal __t1214 = EV_NULLV;
  {
    ErgoVal __parts210[1] = { __t1213 };
    ErgoStr* __s211 = stdr_str_from_parts(1, __parts210);
    __t1214 = EV_STR(__s211);
  }
  ergo_release_val(__t1213);
  ergo_m_cogito_ViewSwitcher_set_active(__t1212, __t1214);
  ergo_release_val(__t1212);
  ergo_release_val(__t1214);
  ErgoVal __t1215 = EV_NULLV;
  ergo_release_val(__t1215);
  ErgoVal __t1216 = switcher__130; ergo_retain_val(__t1216);
  ErgoVal __t1217 = EV_BOOL(true);
  ergo_m_cogito_ViewSwitcher_set_hexpand(__t1216, __t1217);
  ergo_release_val(__t1216);
  ergo_release_val(__t1217);
  ErgoVal __t1218 = EV_NULLV;
  ergo_release_val(__t1218);
  ErgoVal __t1219 = switcher__130; ergo_retain_val(__t1219);
  ErgoVal __t1220 = EV_BOOL(true);
  ergo_m_cogito_ViewSwitcher_set_vexpand(__t1219, __t1220);
  ergo_release_val(__t1219);
  ergo_release_val(__t1220);
  ErgoVal __t1221 = EV_NULLV;
  ergo_release_val(__t1221);
  ErgoVal __t1222 = root__129; ergo_retain_val(__t1222);
  ErgoVal __t1223 = switcher__130; ergo_retain_val(__t1223);
  ergo_m_cogito_VStack_add(__t1222, __t1223);
  ergo_release_val(__t1222);
  ergo_release_val(__t1223);
  ErgoVal __t1224 = EV_NULLV;
  ergo_release_val(__t1224);
  ErgoVal __t1225 = a0; ergo_retain_val(__t1225);
  ErgoVal __t1226 = root__129; ergo_retain_val(__t1226);
  ergo_m_cogito_Window_add(__t1225, __t1226);
  ergo_release_val(__t1225);
  ergo_release_val(__t1226);
  ErgoVal __t1227 = EV_NULLV;
  ergo_release_val(__t1227);
  ergo_main_clear_all();
  ErgoVal __t1228 = EV_NULLV;
  ergo_release_val(__t1228);
  ergo_release_val(conv_view__140);
  ergo_release_val(eq__139);
  ergo_release_val(zero__138);
  ergo_release_val(c__137);
  ergo_release_val(keypad__136);
  ergo_release_val(display_box__135);
  ergo_release_val(calc_view__134);
  ergo_release_val(about_btn__133);
  ergo_release_val(toggle_btn__132);
  ergo_release_val(bar__131);
  ergo_release_val(switcher__130);
  ergo_release_val(root__129);
}

static void ergo_stdr___writef(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_stdr___read_line(void) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static ErgoVal ergo_stdr___readf_parse(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static ErgoVal ergo_stdr___read_text_file(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static ErgoVal ergo_stdr___write_text_file(ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static ErgoVal ergo_stdr___open_file_dialog(ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static ErgoVal ergo_stdr___save_file_dialog(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static void ergo_stdr_writef(ErgoVal a0, ErgoVal a1) {
  ErgoVal __t1229 = a0; ergo_retain_val(__t1229);
  ErgoVal __t1230 = a1; ergo_retain_val(__t1230);
  stdr_writef_args(__t1229, __t1230);
  ergo_release_val(__t1229);
  ergo_release_val(__t1230);
  ErgoVal __t1231 = EV_NULLV;
  ergo_release_val(__t1231);
}

static ErgoVal ergo_stdr_readf(ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t1232 = a0; ergo_retain_val(__t1232);
  ErgoVal __t1233 = a1; ergo_retain_val(__t1233);
  stdr_writef_args(__t1232, __t1233);
  ergo_release_val(__t1232);
  ergo_release_val(__t1233);
  ErgoVal __t1234 = EV_NULLV;
  ergo_release_val(__t1234);
  ErgoVal line__141 = EV_NULLV;
  ErgoVal __t1235 = EV_STR(stdr_read_line());
  ergo_move_into(&line__141, __t1235);
  ErgoVal parsed__142 = EV_NULLV;
  ErgoVal __t1236 = a0; ergo_retain_val(__t1236);
  ErgoVal __t1237 = line__141; ergo_retain_val(__t1237);
  ErgoVal __t1238 = a1; ergo_retain_val(__t1238);
  ErgoVal __t1239 = stdr_readf_parse(__t1236, __t1237, __t1238);
  ergo_release_val(__t1236);
  ergo_release_val(__t1237);
  ergo_release_val(__t1238);
  ergo_move_into(&parsed__142, __t1239);
  ErgoArr* __tup6 = stdr_arr_new(2);
  ErgoVal __t1240 = EV_ARR(__tup6);
  ErgoVal __t1241 = line__141; ergo_retain_val(__t1241);
  ergo_arr_add(__tup6, __t1241);
  ErgoVal __t1242 = parsed__142; ergo_retain_val(__t1242);
  ergo_arr_add(__tup6, __t1242);
  ergo_move_into(&__ret, __t1240);
  return __ret;
  ergo_release_val(parsed__142);
  ergo_release_val(line__141);
  return __ret;
}

static void ergo_stdr_write(ErgoVal a0) {
  ErgoVal __t1243 = EV_STR(stdr_str_lit("{}"));
  ErgoVal __t1244 = EV_NULLV;
  {
    ErgoVal __parts212[1] = { __t1243 };
    ErgoStr* __s213 = stdr_str_from_parts(1, __parts212);
    __t1244 = EV_STR(__s213);
  }
  ergo_release_val(__t1243);
  ErgoArr* __tup7 = stdr_arr_new(1);
  ErgoVal __t1245 = EV_ARR(__tup7);
  ErgoVal __t1246 = a0; ergo_retain_val(__t1246);
  ergo_arr_add(__tup7, __t1246);
  ergo_stdr_writef(__t1244, __t1245);
  ergo_release_val(__t1244);
  ergo_release_val(__t1245);
  ErgoVal __t1247 = EV_NULLV;
  ergo_release_val(__t1247);
}

static ErgoVal ergo_stdr_is_null(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t1248 = a0; ergo_retain_val(__t1248);
  ErgoVal __t1249 = EV_NULLV;
  ErgoVal __t1250 = ergo_eq(__t1248, __t1249);
  ergo_release_val(__t1248);
  ergo_release_val(__t1249);
  ergo_move_into(&__ret, __t1250);
  return __ret;
  return __ret;
}

static ErgoVal ergo_stdr_str(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static ErgoVal ergo_stdr___len(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static ErgoVal ergo_stdr_len(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t1251 = a0; ergo_retain_val(__t1251);
  ErgoVal __t1252 = EV_INT(stdr_len(__t1251));
  ergo_release_val(__t1251);
  ergo_move_into(&__ret, __t1252);
  return __ret;
  return __ret;
}

static ErgoVal ergo_stdr_read_text_file(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t1253 = a0; ergo_retain_val(__t1253);
  ErgoVal __t1254 = stdr_read_text_file(__t1253);
  ergo_release_val(__t1253);
  ergo_move_into(&__ret, __t1254);
  return __ret;
  return __ret;
}

static ErgoVal ergo_stdr_write_text_file(ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t1255 = a0; ergo_retain_val(__t1255);
  ErgoVal __t1256 = a1; ergo_retain_val(__t1256);
  ErgoVal __t1257 = stdr_write_text_file(__t1255, __t1256);
  ergo_release_val(__t1255);
  ergo_release_val(__t1256);
  ergo_move_into(&__ret, __t1257);
  return __ret;
  return __ret;
}

static ErgoVal ergo_stdr_open_file_dialog(ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t1258 = a0; ergo_retain_val(__t1258);
  ErgoVal __t1259 = a1; ergo_retain_val(__t1259);
  ErgoVal __t1260 = stdr_open_file_dialog(__t1258, __t1259);
  ergo_release_val(__t1258);
  ergo_release_val(__t1259);
  ergo_move_into(&__ret, __t1260);
  return __ret;
  return __ret;
}

static ErgoVal ergo_stdr_save_file_dialog(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t1261 = a0; ergo_retain_val(__t1261);
  ErgoVal __t1262 = a1; ergo_retain_val(__t1262);
  ErgoVal __t1263 = a2; ergo_retain_val(__t1263);
  ErgoVal __t1264 = stdr_save_file_dialog(__t1261, __t1262, __t1263);
  ergo_release_val(__t1261);
  ergo_release_val(__t1262);
  ergo_release_val(__t1263);
  ergo_move_into(&__ret, __t1264);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_app(void) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_app_set_appid(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_app_set_app_name(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_app_set_accent_color(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
}

static ErgoVal ergo_cogito___cogito_window(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_button(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_iconbtn(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_label(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_label_set_class(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_label_set_text(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_node_set_class(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_node_set_a11y_label(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_node_set_a11y_role(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_node_set_tooltip(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_pointer_capture(ErgoVal a0) {
}

static void ergo_cogito___cogito_pointer_release(void) {
}

static void ergo_cogito___cogito_label_set_wrap(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_label_set_ellipsis(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_label_set_align(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_image(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_image_set_icon(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_image_set_source(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_image_set_size(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
}

static void ergo_cogito___cogito_image_set_radius(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_dialog(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_dialog_close(ErgoVal a0) {
}

static void ergo_cogito___cogito_dialog_remove(ErgoVal a0) {
}

static ErgoVal ergo_cogito___cogito_find_parent(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_find_children(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_dialog_slot(void) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_dialog_slot_show(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_dialog_slot_clear(ErgoVal a0) {
}

static void ergo_cogito___cogito_window_set_dialog(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_window_clear_dialog(ErgoVal a0) {
}

static ErgoVal ergo_cogito___cogito_node_window(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_checkbox(ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_switch(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_textfield(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_searchfield_set_text(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_searchfield_get_text(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_searchfield_on_change(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_textview(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_searchfield(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_dropdown(void) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_datepicker(void) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_datepicker_on_change(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_stepper(ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_slider(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_tabs(void) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_segmented(void) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_view_switcher(void) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_progress(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_divider(ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_treeview(void) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_colorpicker(void) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_colorpicker_on_change(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_toasts(void) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_toast(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_toolbar(void) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_vstack(void) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_hstack(void) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_zstack(void) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_fixed(void) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_scroller(void) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_carousel(void) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_carousel_item(void) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_carousel_item_set_text(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_carousel_item_set_halign(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_carousel_item_set_valign(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_carousel_set_active_index(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_carousel_get_active_index(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_list(void) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_grid(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_container_add(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_container_set_margins(ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3, ErgoVal a4) {
}

static void ergo_cogito___cogito_build(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_window_set_builder(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_state_new(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_state_get(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_state_set(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_container_set_align(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_container_set_halign(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_container_set_valign(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_container_set_hexpand(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_container_set_vexpand(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_container_set_gap(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_container_set_padding(ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3, ErgoVal a4) {
}

static void ergo_cogito___cogito_fixed_set_pos(ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
}

static void ergo_cogito___cogito_scroller_set_axes(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
}

static void ergo_cogito___cogito_grid_set_gap(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
}

static void ergo_cogito___cogito_grid_set_span(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
}

static void ergo_cogito___cogito_grid_set_align(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
}

static void ergo_cogito___cogito_node_set_disabled(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_node_set_editable(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_node_get_editable(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_node_set_id(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_window_set_autosize(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_window_set_resizable(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_appbar(ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static ErgoVal ergo_cogito___cogito_appbar_add_button(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_appbar_set_controls(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_appbar_set_title(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_appbar_set_subtitle(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_button_set_text(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_button_add_menu(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
}

static void ergo_cogito___cogito_iconbtn_add_menu(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
}

static void ergo_cogito___cogito_checkbox_set_checked(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_checkbox_get_checked(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_switch_set_checked(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_switch_get_checked(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_checkbox_on_change(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_switch_on_change(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_textfield_set_text(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_textfield_get_text(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_textfield_on_change(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_textview_set_text(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_textview_get_text(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_textview_on_change(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_dropdown_set_items(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_dropdown_set_selected(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_dropdown_get_selected(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_dropdown_on_change(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_slider_set_value(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_slider_get_value(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_slider_on_change(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_tabs_set_items(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_tabs_set_ids(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_tabs_set_selected(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_tabs_get_selected(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_tabs_on_change(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_tabs_bind(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_view_switcher_set_active(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_progress_set_value(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_progress_get_value(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_toast_set_text(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_toast_on_click(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_list_on_select(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_list_on_activate(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_grid_on_select(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_grid_on_activate(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_button_on_click(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_chip(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_chip_set_selected(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_chip_get_selected(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_chip_set_closable(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_chip_on_click(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_chip_on_close(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_fab(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_fab_set_extended(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
}

static void ergo_cogito___cogito_fab_on_click(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_nav_rail(void) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_nav_rail_set_items(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
}

static void ergo_cogito___cogito_nav_rail_set_selected(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_nav_rail_get_selected(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_nav_rail_on_change(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_bottom_nav(void) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_bottom_nav_set_items(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
}

static void ergo_cogito___cogito_bottom_nav_set_selected(ErgoVal a0, ErgoVal a1) {
}

static ErgoVal ergo_cogito___cogito_bottom_nav_get_selected(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static void ergo_cogito___cogito_bottom_nav_on_change(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_run(ErgoVal a0, ErgoVal a1) {
}

static void ergo_cogito___cogito_load_sum(ErgoVal a0) {
}

static void ergo_cogito___cogito_set_script_dir(ErgoVal a0) {
}

static ErgoVal ergo_cogito___cogito_open_url(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static void ergo_m_cogito_App_run(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1265 = self; ergo_retain_val(__t1265);
  ErgoVal __t1266 = a0; ergo_retain_val(__t1266);
  cogito_run(__t1265, __t1266);
  ergo_release_val(__t1265);
  ergo_release_val(__t1266);
  ErgoVal __t1267 = EV_NULLV;
  ergo_release_val(__t1267);
}

static void ergo_m_cogito_App_set_appid(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1268 = self; ergo_retain_val(__t1268);
  ErgoVal __t1269 = a0; ergo_retain_val(__t1269);
  cogito_app_set_appid(__t1268, __t1269);
  ergo_release_val(__t1268);
  ergo_release_val(__t1269);
  ErgoVal __t1270 = EV_NULLV;
  ergo_release_val(__t1270);
}

static void ergo_m_cogito_App_set_app_name(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1271 = self; ergo_retain_val(__t1271);
  ErgoVal __t1272 = a0; ergo_retain_val(__t1272);
  cogito_app_set_app_name(__t1271, __t1272);
  ergo_release_val(__t1271);
  ergo_release_val(__t1272);
  ErgoVal __t1273 = EV_NULLV;
  ergo_release_val(__t1273);
}

static void ergo_m_cogito_App_set_accent_color(ErgoVal self, ErgoVal a0, ErgoVal a1) {
  ErgoVal __t1274 = self; ergo_retain_val(__t1274);
  ErgoVal __t1275 = a0; ergo_retain_val(__t1275);
  ErgoVal __t1276 = a1; ergo_retain_val(__t1276);
  cogito_app_set_accent_color(__t1274, __t1275, __t1276);
  ergo_release_val(__t1274);
  ergo_release_val(__t1275);
  ergo_release_val(__t1276);
  ErgoVal __t1277 = EV_NULLV;
  ergo_release_val(__t1277);
}

static void ergo_m_cogito_Window_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1278 = self; ergo_retain_val(__t1278);
  ErgoVal __t1279 = a0; ergo_retain_val(__t1279);
  cogito_container_add(__t1278, __t1279);
  ergo_release_val(__t1278);
  ergo_release_val(__t1279);
  ErgoVal __t1280 = EV_NULLV;
  ergo_release_val(__t1280);
}

static void ergo_m_cogito_Window_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t1281 = self; ergo_retain_val(__t1281);
  ErgoVal __t1282 = a0; ergo_retain_val(__t1282);
  ErgoVal __t1283 = a1; ergo_retain_val(__t1283);
  ErgoVal __t1284 = a2; ergo_retain_val(__t1284);
  ErgoVal __t1285 = a3; ergo_retain_val(__t1285);
  cogito_container_set_margins(__t1281, __t1282, __t1283, __t1284, __t1285);
  ergo_release_val(__t1281);
  ergo_release_val(__t1282);
  ergo_release_val(__t1283);
  ergo_release_val(__t1284);
  ergo_release_val(__t1285);
  ErgoVal __t1286 = EV_NULLV;
  ergo_release_val(__t1286);
}

static void ergo_m_cogito_Window_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t1287 = self; ergo_retain_val(__t1287);
  ErgoVal __t1288 = a0; ergo_retain_val(__t1288);
  ErgoVal __t1289 = a1; ergo_retain_val(__t1289);
  ErgoVal __t1290 = a2; ergo_retain_val(__t1290);
  ErgoVal __t1291 = a3; ergo_retain_val(__t1291);
  cogito_container_set_padding(__t1287, __t1288, __t1289, __t1290, __t1291);
  ergo_release_val(__t1287);
  ergo_release_val(__t1288);
  ergo_release_val(__t1289);
  ergo_release_val(__t1290);
  ergo_release_val(__t1291);
  ErgoVal __t1292 = EV_NULLV;
  ergo_release_val(__t1292);
}

static void ergo_m_cogito_Window_set_autosize(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1293 = self; ergo_retain_val(__t1293);
  ErgoVal __t1294 = a0; ergo_retain_val(__t1294);
  cogito_window_set_autosize(__t1293, __t1294);
  ergo_release_val(__t1293);
  ergo_release_val(__t1294);
  ErgoVal __t1295 = EV_NULLV;
  ergo_release_val(__t1295);
}

static void ergo_m_cogito_Window_set_resizable(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1296 = self; ergo_retain_val(__t1296);
  ErgoVal __t1297 = a0; ergo_retain_val(__t1297);
  cogito_window_set_resizable(__t1296, __t1297);
  ergo_release_val(__t1296);
  ergo_release_val(__t1297);
  ErgoVal __t1298 = EV_NULLV;
  ergo_release_val(__t1298);
}

static void ergo_m_cogito_Window_set_a11y_label(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1299 = self; ergo_retain_val(__t1299);
  ErgoVal __t1300 = a0; ergo_retain_val(__t1300);
  cogito_node_set_a11y_label(__t1299, __t1300);
  ergo_release_val(__t1299);
  ergo_release_val(__t1300);
  ErgoVal __t1301 = EV_NULLV;
  ergo_release_val(__t1301);
}

static void ergo_m_cogito_Window_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1302 = self; ergo_retain_val(__t1302);
  ErgoVal __t1303 = a0; ergo_retain_val(__t1303);
  cogito_node_set_disabled(__t1302, __t1303);
  ergo_release_val(__t1302);
  ergo_release_val(__t1303);
  ErgoVal __t1304 = EV_NULLV;
  ergo_release_val(__t1304);
}

static void ergo_m_cogito_Window_set_dialog(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1305 = self; ergo_retain_val(__t1305);
  ErgoVal __t1306 = a0; ergo_retain_val(__t1306);
  cogito_window_set_dialog(__t1305, __t1306);
  ergo_release_val(__t1305);
  ergo_release_val(__t1306);
  ErgoVal __t1307 = EV_NULLV;
  ergo_release_val(__t1307);
}

static void ergo_m_cogito_Window_clear_dialog(ErgoVal self) {
  ErgoVal __t1308 = self; ergo_retain_val(__t1308);
  cogito_window_clear_dialog(__t1308);
  ergo_release_val(__t1308);
  ErgoVal __t1309 = EV_NULLV;
  ergo_release_val(__t1309);
}

static ErgoVal ergo_m_cogito_Window_build(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t1310 = self; ergo_retain_val(__t1310);
  ErgoVal __t1311 = a0; ergo_retain_val(__t1311);
  cogito_window_set_builder(__t1310, __t1311);
  ergo_release_val(__t1310);
  ergo_release_val(__t1311);
  ErgoVal __t1312 = EV_NULLV;
  ergo_release_val(__t1312);
  ErgoVal __t1313 = self; ergo_retain_val(__t1313);
  ErgoVal __t1314 = a0; ergo_retain_val(__t1314);
  cogito_build(__t1313, __t1314);
  ergo_release_val(__t1313);
  ergo_release_val(__t1314);
  ErgoVal __t1315 = EV_NULLV;
  ergo_release_val(__t1315);
  ErgoVal __t1316 = self; ergo_retain_val(__t1316);
  ergo_move_into(&__ret, __t1316);
  return __ret;
  return __ret;
}

static void ergo_m_cogito_Window_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1317 = self; ergo_retain_val(__t1317);
  ErgoVal __t1318 = a0; ergo_retain_val(__t1318);
  cogito_node_set_class(__t1317, __t1318);
  ergo_release_val(__t1317);
  ergo_release_val(__t1318);
  ErgoVal __t1319 = EV_NULLV;
  ergo_release_val(__t1319);
}

static void ergo_m_cogito_Window_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1320 = self; ergo_retain_val(__t1320);
  ErgoVal __t1321 = a0; ergo_retain_val(__t1321);
  cogito_node_set_id(__t1320, __t1321);
  ergo_release_val(__t1320);
  ergo_release_val(__t1321);
  ErgoVal __t1322 = EV_NULLV;
  ergo_release_val(__t1322);
}

static ErgoVal ergo_m_cogito_AppBar_add_button(ErgoVal self, ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t1323 = self; ergo_retain_val(__t1323);
  ErgoVal __t1324 = a0; ergo_retain_val(__t1324);
  ErgoVal __t1325 = a1; ergo_retain_val(__t1325);
  ErgoVal __t1326 = cogito_appbar_add_button(__t1323, __t1324, __t1325);
  ergo_release_val(__t1323);
  ergo_release_val(__t1324);
  ergo_release_val(__t1325);
  ergo_move_into(&__ret, __t1326);
  return __ret;
  return __ret;
}

static void ergo_m_cogito_AppBar_set_window_controls(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1327 = self; ergo_retain_val(__t1327);
  ErgoVal __t1328 = a0; ergo_retain_val(__t1328);
  cogito_appbar_set_controls(__t1327, __t1328);
  ergo_release_val(__t1327);
  ergo_release_val(__t1328);
  ErgoVal __t1329 = EV_NULLV;
  ergo_release_val(__t1329);
}

static void ergo_m_cogito_AppBar_set_title(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1330 = self; ergo_retain_val(__t1330);
  ErgoVal __t1331 = a0; ergo_retain_val(__t1331);
  cogito_appbar_set_title(__t1330, __t1331);
  ergo_release_val(__t1330);
  ergo_release_val(__t1331);
  ErgoVal __t1332 = EV_NULLV;
  ergo_release_val(__t1332);
}

static void ergo_m_cogito_AppBar_set_subtitle(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1333 = self; ergo_retain_val(__t1333);
  ErgoVal __t1334 = a0; ergo_retain_val(__t1334);
  cogito_appbar_set_subtitle(__t1333, __t1334);
  ergo_release_val(__t1333);
  ergo_release_val(__t1334);
  ErgoVal __t1335 = EV_NULLV;
  ergo_release_val(__t1335);
}

static void ergo_m_cogito_AppBar_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1336 = self; ergo_retain_val(__t1336);
  ErgoVal __t1337 = a0; ergo_retain_val(__t1337);
  cogito_container_set_hexpand(__t1336, __t1337);
  ergo_release_val(__t1336);
  ergo_release_val(__t1337);
  ErgoVal __t1338 = EV_NULLV;
  ergo_release_val(__t1338);
}

static void ergo_m_cogito_AppBar_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1339 = self; ergo_retain_val(__t1339);
  ErgoVal __t1340 = a0; ergo_retain_val(__t1340);
  cogito_container_set_vexpand(__t1339, __t1340);
  ergo_release_val(__t1339);
  ergo_release_val(__t1340);
  ErgoVal __t1341 = EV_NULLV;
  ergo_release_val(__t1341);
}

static void ergo_m_cogito_AppBar_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1342 = self; ergo_retain_val(__t1342);
  ErgoVal __t1343 = a0; ergo_retain_val(__t1343);
  cogito_node_set_disabled(__t1342, __t1343);
  ergo_release_val(__t1342);
  ergo_release_val(__t1343);
  ErgoVal __t1344 = EV_NULLV;
  ergo_release_val(__t1344);
}

static void ergo_m_cogito_AppBar_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1345 = self; ergo_retain_val(__t1345);
  ErgoVal __t1346 = a0; ergo_retain_val(__t1346);
  cogito_node_set_class(__t1345, __t1346);
  ergo_release_val(__t1345);
  ergo_release_val(__t1346);
  ErgoVal __t1347 = EV_NULLV;
  ergo_release_val(__t1347);
}

static void ergo_m_cogito_AppBar_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1348 = self; ergo_retain_val(__t1348);
  ErgoVal __t1349 = a0; ergo_retain_val(__t1349);
  cogito_node_set_id(__t1348, __t1349);
  ergo_release_val(__t1348);
  ergo_release_val(__t1349);
  ErgoVal __t1350 = EV_NULLV;
  ergo_release_val(__t1350);
}

static void ergo_m_cogito_Image_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t1351 = self; ergo_retain_val(__t1351);
  ErgoVal __t1352 = a0; ergo_retain_val(__t1352);
  ErgoVal __t1353 = a1; ergo_retain_val(__t1353);
  ErgoVal __t1354 = a2; ergo_retain_val(__t1354);
  ErgoVal __t1355 = a3; ergo_retain_val(__t1355);
  cogito_container_set_margins(__t1351, __t1352, __t1353, __t1354, __t1355);
  ergo_release_val(__t1351);
  ergo_release_val(__t1352);
  ergo_release_val(__t1353);
  ergo_release_val(__t1354);
  ergo_release_val(__t1355);
  ErgoVal __t1356 = EV_NULLV;
  ergo_release_val(__t1356);
}

static void ergo_m_cogito_Image_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t1357 = self; ergo_retain_val(__t1357);
  ErgoVal __t1358 = a0; ergo_retain_val(__t1358);
  ErgoVal __t1359 = a1; ergo_retain_val(__t1359);
  ErgoVal __t1360 = a2; ergo_retain_val(__t1360);
  ErgoVal __t1361 = a3; ergo_retain_val(__t1361);
  cogito_container_set_padding(__t1357, __t1358, __t1359, __t1360, __t1361);
  ergo_release_val(__t1357);
  ergo_release_val(__t1358);
  ergo_release_val(__t1359);
  ergo_release_val(__t1360);
  ergo_release_val(__t1361);
  ErgoVal __t1362 = EV_NULLV;
  ergo_release_val(__t1362);
}

static void ergo_m_cogito_Image_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1363 = self; ergo_retain_val(__t1363);
  ErgoVal __t1364 = a0; ergo_retain_val(__t1364);
  cogito_container_set_align(__t1363, __t1364);
  ergo_release_val(__t1363);
  ergo_release_val(__t1364);
  ErgoVal __t1365 = EV_NULLV;
  ergo_release_val(__t1365);
}

static void ergo_m_cogito_Image_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1366 = self; ergo_retain_val(__t1366);
  ErgoVal __t1367 = a0; ergo_retain_val(__t1367);
  cogito_container_set_halign(__t1366, __t1367);
  ergo_release_val(__t1366);
  ergo_release_val(__t1367);
  ErgoVal __t1368 = EV_NULLV;
  ergo_release_val(__t1368);
}

static void ergo_m_cogito_Image_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1369 = self; ergo_retain_val(__t1369);
  ErgoVal __t1370 = a0; ergo_retain_val(__t1370);
  cogito_container_set_valign(__t1369, __t1370);
  ergo_release_val(__t1369);
  ergo_release_val(__t1370);
  ErgoVal __t1371 = EV_NULLV;
  ergo_release_val(__t1371);
}

static void ergo_m_cogito_Image_set_icon(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1372 = self; ergo_retain_val(__t1372);
  ErgoVal __t1373 = a0; ergo_retain_val(__t1373);
  cogito_image_set_icon(__t1372, __t1373);
  ergo_release_val(__t1372);
  ergo_release_val(__t1373);
  ErgoVal __t1374 = EV_NULLV;
  ergo_release_val(__t1374);
}

static void ergo_m_cogito_Image_set_source(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1375 = self; ergo_retain_val(__t1375);
  ErgoVal __t1376 = a0; ergo_retain_val(__t1376);
  cogito_image_set_icon(__t1375, __t1376);
  ergo_release_val(__t1375);
  ergo_release_val(__t1376);
  ErgoVal __t1377 = EV_NULLV;
  ergo_release_val(__t1377);
}

static void ergo_m_cogito_Image_set_size(ErgoVal self, ErgoVal a0, ErgoVal a1) {
  ErgoVal __t1378 = self; ergo_retain_val(__t1378);
  ErgoVal __t1379 = a0; ergo_retain_val(__t1379);
  ErgoVal __t1380 = a1; ergo_retain_val(__t1380);
  cogito_image_set_size(__t1378, __t1379, __t1380);
  ergo_release_val(__t1378);
  ergo_release_val(__t1379);
  ergo_release_val(__t1380);
  ErgoVal __t1381 = EV_NULLV;
  ergo_release_val(__t1381);
}

static void ergo_m_cogito_Image_set_radius(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1382 = self; ergo_retain_val(__t1382);
  ErgoVal __t1383 = a0; ergo_retain_val(__t1383);
  cogito_image_set_radius(__t1382, __t1383);
  ergo_release_val(__t1382);
  ergo_release_val(__t1383);
  ErgoVal __t1384 = EV_NULLV;
  ergo_release_val(__t1384);
}

static void ergo_m_cogito_Image_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1385 = self; ergo_retain_val(__t1385);
  ErgoVal __t1386 = a0; ergo_retain_val(__t1386);
  cogito_container_set_hexpand(__t1385, __t1386);
  ergo_release_val(__t1385);
  ergo_release_val(__t1386);
  ErgoVal __t1387 = EV_NULLV;
  ergo_release_val(__t1387);
}

static void ergo_m_cogito_Image_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1388 = self; ergo_retain_val(__t1388);
  ErgoVal __t1389 = a0; ergo_retain_val(__t1389);
  cogito_container_set_vexpand(__t1388, __t1389);
  ergo_release_val(__t1388);
  ergo_release_val(__t1389);
  ErgoVal __t1390 = EV_NULLV;
  ergo_release_val(__t1390);
}

static void ergo_m_cogito_Image_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1391 = self; ergo_retain_val(__t1391);
  ErgoVal __t1392 = a0; ergo_retain_val(__t1392);
  cogito_node_set_disabled(__t1391, __t1392);
  ergo_release_val(__t1391);
  ergo_release_val(__t1392);
  ErgoVal __t1393 = EV_NULLV;
  ergo_release_val(__t1393);
}

static void ergo_m_cogito_Image_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1394 = self; ergo_retain_val(__t1394);
  ErgoVal __t1395 = a0; ergo_retain_val(__t1395);
  cogito_node_set_class(__t1394, __t1395);
  ergo_release_val(__t1394);
  ergo_release_val(__t1395);
  ErgoVal __t1396 = EV_NULLV;
  ergo_release_val(__t1396);
}

static void ergo_m_cogito_Image_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1397 = self; ergo_retain_val(__t1397);
  ErgoVal __t1398 = a0; ergo_retain_val(__t1398);
  cogito_node_set_id(__t1397, __t1398);
  ergo_release_val(__t1397);
  ergo_release_val(__t1398);
  ErgoVal __t1399 = EV_NULLV;
  ergo_release_val(__t1399);
}

static void ergo_m_cogito_Dialog_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1400 = self; ergo_retain_val(__t1400);
  ErgoVal __t1401 = a0; ergo_retain_val(__t1401);
  cogito_container_add(__t1400, __t1401);
  ergo_release_val(__t1400);
  ergo_release_val(__t1401);
  ErgoVal __t1402 = EV_NULLV;
  ergo_release_val(__t1402);
}

static void ergo_m_cogito_Dialog_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t1403 = self; ergo_retain_val(__t1403);
  ErgoVal __t1404 = a0; ergo_retain_val(__t1404);
  ErgoVal __t1405 = a1; ergo_retain_val(__t1405);
  ErgoVal __t1406 = a2; ergo_retain_val(__t1406);
  ErgoVal __t1407 = a3; ergo_retain_val(__t1407);
  cogito_container_set_padding(__t1403, __t1404, __t1405, __t1406, __t1407);
  ergo_release_val(__t1403);
  ergo_release_val(__t1404);
  ergo_release_val(__t1405);
  ergo_release_val(__t1406);
  ergo_release_val(__t1407);
  ErgoVal __t1408 = EV_NULLV;
  ergo_release_val(__t1408);
}

static void ergo_m_cogito_Dialog_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t1409 = self; ergo_retain_val(__t1409);
  ErgoVal __t1410 = a0; ergo_retain_val(__t1410);
  ErgoVal __t1411 = a1; ergo_retain_val(__t1411);
  ErgoVal __t1412 = a2; ergo_retain_val(__t1412);
  ErgoVal __t1413 = a3; ergo_retain_val(__t1413);
  cogito_container_set_margins(__t1409, __t1410, __t1411, __t1412, __t1413);
  ergo_release_val(__t1409);
  ergo_release_val(__t1410);
  ergo_release_val(__t1411);
  ergo_release_val(__t1412);
  ergo_release_val(__t1413);
  ErgoVal __t1414 = EV_NULLV;
  ergo_release_val(__t1414);
}

static ErgoVal ergo_m_cogito_Dialog_build(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t1415 = self; ergo_retain_val(__t1415);
  ErgoVal __t1416 = a0; ergo_retain_val(__t1416);
  cogito_build(__t1415, __t1416);
  ergo_release_val(__t1415);
  ergo_release_val(__t1416);
  ErgoVal __t1417 = EV_NULLV;
  ergo_release_val(__t1417);
  ErgoVal __t1418 = self; ergo_retain_val(__t1418);
  ergo_move_into(&__ret, __t1418);
  return __ret;
  return __ret;
}

static ErgoVal ergo_m_cogito_Dialog_window(ErgoVal self) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t1419 = self; ergo_retain_val(__t1419);
  ErgoVal __t1420 = cogito_node_window_val(__t1419);
  ergo_release_val(__t1419);
  ergo_move_into(&__ret, __t1420);
  return __ret;
  return __ret;
}

static void ergo_m_cogito_Dialog_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1421 = self; ergo_retain_val(__t1421);
  ErgoVal __t1422 = a0; ergo_retain_val(__t1422);
  cogito_container_set_hexpand(__t1421, __t1422);
  ergo_release_val(__t1421);
  ergo_release_val(__t1422);
  ErgoVal __t1423 = EV_NULLV;
  ergo_release_val(__t1423);
}

static void ergo_m_cogito_Dialog_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1424 = self; ergo_retain_val(__t1424);
  ErgoVal __t1425 = a0; ergo_retain_val(__t1425);
  cogito_container_set_vexpand(__t1424, __t1425);
  ergo_release_val(__t1424);
  ergo_release_val(__t1425);
  ErgoVal __t1426 = EV_NULLV;
  ergo_release_val(__t1426);
}

static void ergo_m_cogito_Dialog_close(ErgoVal self) {
  ErgoVal __t1427 = self; ergo_retain_val(__t1427);
  ergo_cogito___cogito_dialog_close(__t1427);
  ergo_release_val(__t1427);
  ErgoVal __t1428 = EV_NULLV;
  ergo_release_val(__t1428);
}

static void ergo_m_cogito_Dialog_remove(ErgoVal self) {
  ErgoVal __t1429 = self; ergo_retain_val(__t1429);
  ergo_cogito___cogito_dialog_remove(__t1429);
  ergo_release_val(__t1429);
  ErgoVal __t1430 = EV_NULLV;
  ergo_release_val(__t1430);
}

static void ergo_m_cogito_Dialog_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1431 = self; ergo_retain_val(__t1431);
  ErgoVal __t1432 = a0; ergo_retain_val(__t1432);
  cogito_node_set_disabled(__t1431, __t1432);
  ergo_release_val(__t1431);
  ergo_release_val(__t1432);
  ErgoVal __t1433 = EV_NULLV;
  ergo_release_val(__t1433);
}

static void ergo_m_cogito_Dialog_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1434 = self; ergo_retain_val(__t1434);
  ErgoVal __t1435 = a0; ergo_retain_val(__t1435);
  cogito_node_set_class(__t1434, __t1435);
  ergo_release_val(__t1434);
  ergo_release_val(__t1435);
  ErgoVal __t1436 = EV_NULLV;
  ergo_release_val(__t1436);
}

static void ergo_m_cogito_Dialog_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1437 = self; ergo_retain_val(__t1437);
  ErgoVal __t1438 = a0; ergo_retain_val(__t1438);
  cogito_node_set_id(__t1437, __t1438);
  ergo_release_val(__t1437);
  ergo_release_val(__t1438);
  ErgoVal __t1439 = EV_NULLV;
  ergo_release_val(__t1439);
}

static void ergo_m_cogito_DialogSlot_show(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1440 = self; ergo_retain_val(__t1440);
  ErgoVal __t1441 = a0; ergo_retain_val(__t1441);
  cogito_dialog_slot_show(__t1440, __t1441);
  ergo_release_val(__t1440);
  ergo_release_val(__t1441);
  ErgoVal __t1442 = EV_NULLV;
  ergo_release_val(__t1442);
}

static void ergo_m_cogito_DialogSlot_clear(ErgoVal self) {
  ErgoVal __t1443 = self; ergo_retain_val(__t1443);
  cogito_dialog_slot_clear(__t1443);
  ergo_release_val(__t1443);
  ErgoVal __t1444 = EV_NULLV;
  ergo_release_val(__t1444);
}

static void ergo_m_cogito_DialogSlot_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1445 = self; ergo_retain_val(__t1445);
  ErgoVal __t1446 = a0; ergo_retain_val(__t1446);
  cogito_container_set_hexpand(__t1445, __t1446);
  ergo_release_val(__t1445);
  ergo_release_val(__t1446);
  ErgoVal __t1447 = EV_NULLV;
  ergo_release_val(__t1447);
}

static void ergo_m_cogito_DialogSlot_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1448 = self; ergo_retain_val(__t1448);
  ErgoVal __t1449 = a0; ergo_retain_val(__t1449);
  cogito_container_set_vexpand(__t1448, __t1449);
  ergo_release_val(__t1448);
  ergo_release_val(__t1449);
  ErgoVal __t1450 = EV_NULLV;
  ergo_release_val(__t1450);
}

static void ergo_m_cogito_DialogSlot_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1451 = self; ergo_retain_val(__t1451);
  ErgoVal __t1452 = a0; ergo_retain_val(__t1452);
  cogito_node_set_disabled(__t1451, __t1452);
  ergo_release_val(__t1451);
  ergo_release_val(__t1452);
  ErgoVal __t1453 = EV_NULLV;
  ergo_release_val(__t1453);
}

static void ergo_m_cogito_DialogSlot_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1454 = self; ergo_retain_val(__t1454);
  ErgoVal __t1455 = a0; ergo_retain_val(__t1455);
  cogito_node_set_class(__t1454, __t1455);
  ergo_release_val(__t1454);
  ergo_release_val(__t1455);
  ErgoVal __t1456 = EV_NULLV;
  ergo_release_val(__t1456);
}

static void ergo_m_cogito_DialogSlot_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1457 = self; ergo_retain_val(__t1457);
  ErgoVal __t1458 = a0; ergo_retain_val(__t1458);
  cogito_node_set_id(__t1457, __t1458);
  ergo_release_val(__t1457);
  ergo_release_val(__t1458);
  ErgoVal __t1459 = EV_NULLV;
  ergo_release_val(__t1459);
}

static void ergo_m_cogito_VStack_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1460 = self; ergo_retain_val(__t1460);
  ErgoVal __t1461 = a0; ergo_retain_val(__t1461);
  cogito_container_add(__t1460, __t1461);
  ergo_release_val(__t1460);
  ergo_release_val(__t1461);
  ErgoVal __t1462 = EV_NULLV;
  ergo_release_val(__t1462);
}

static void ergo_m_cogito_VStack_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t1463 = self; ergo_retain_val(__t1463);
  ErgoVal __t1464 = a0; ergo_retain_val(__t1464);
  ErgoVal __t1465 = a1; ergo_retain_val(__t1465);
  ErgoVal __t1466 = a2; ergo_retain_val(__t1466);
  ErgoVal __t1467 = a3; ergo_retain_val(__t1467);
  cogito_container_set_margins(__t1463, __t1464, __t1465, __t1466, __t1467);
  ergo_release_val(__t1463);
  ergo_release_val(__t1464);
  ergo_release_val(__t1465);
  ergo_release_val(__t1466);
  ergo_release_val(__t1467);
  ErgoVal __t1468 = EV_NULLV;
  ergo_release_val(__t1468);
}

static void ergo_m_cogito_VStack_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t1469 = self; ergo_retain_val(__t1469);
  ErgoVal __t1470 = a0; ergo_retain_val(__t1470);
  ErgoVal __t1471 = a1; ergo_retain_val(__t1471);
  ErgoVal __t1472 = a2; ergo_retain_val(__t1472);
  ErgoVal __t1473 = a3; ergo_retain_val(__t1473);
  cogito_container_set_padding(__t1469, __t1470, __t1471, __t1472, __t1473);
  ergo_release_val(__t1469);
  ergo_release_val(__t1470);
  ergo_release_val(__t1471);
  ergo_release_val(__t1472);
  ergo_release_val(__t1473);
  ErgoVal __t1474 = EV_NULLV;
  ergo_release_val(__t1474);
}

static void ergo_m_cogito_VStack_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1475 = self; ergo_retain_val(__t1475);
  ErgoVal __t1476 = a0; ergo_retain_val(__t1476);
  cogito_container_set_align(__t1475, __t1476);
  ergo_release_val(__t1475);
  ergo_release_val(__t1476);
  ErgoVal __t1477 = EV_NULLV;
  ergo_release_val(__t1477);
}

static void ergo_m_cogito_VStack_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1478 = self; ergo_retain_val(__t1478);
  ErgoVal __t1479 = a0; ergo_retain_val(__t1479);
  cogito_container_set_halign(__t1478, __t1479);
  ergo_release_val(__t1478);
  ergo_release_val(__t1479);
  ErgoVal __t1480 = EV_NULLV;
  ergo_release_val(__t1480);
}

static void ergo_m_cogito_VStack_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1481 = self; ergo_retain_val(__t1481);
  ErgoVal __t1482 = a0; ergo_retain_val(__t1482);
  cogito_container_set_valign(__t1481, __t1482);
  ergo_release_val(__t1481);
  ergo_release_val(__t1482);
  ErgoVal __t1483 = EV_NULLV;
  ergo_release_val(__t1483);
}

static void ergo_m_cogito_VStack_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1484 = self; ergo_retain_val(__t1484);
  ErgoVal __t1485 = a0; ergo_retain_val(__t1485);
  cogito_container_set_hexpand(__t1484, __t1485);
  ergo_release_val(__t1484);
  ergo_release_val(__t1485);
  ErgoVal __t1486 = EV_NULLV;
  ergo_release_val(__t1486);
}

static void ergo_m_cogito_VStack_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1487 = self; ergo_retain_val(__t1487);
  ErgoVal __t1488 = a0; ergo_retain_val(__t1488);
  cogito_container_set_vexpand(__t1487, __t1488);
  ergo_release_val(__t1487);
  ergo_release_val(__t1488);
  ErgoVal __t1489 = EV_NULLV;
  ergo_release_val(__t1489);
}

static void ergo_m_cogito_VStack_set_gap(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1490 = self; ergo_retain_val(__t1490);
  ErgoVal __t1491 = a0; ergo_retain_val(__t1491);
  cogito_container_set_gap(__t1490, __t1491);
  ergo_release_val(__t1490);
  ergo_release_val(__t1491);
  ErgoVal __t1492 = EV_NULLV;
  ergo_release_val(__t1492);
}

static void ergo_m_cogito_VStack_align_begin(ErgoVal self) {
  ErgoVal __t1493 = self; ergo_retain_val(__t1493);
  ErgoVal __t1494 = EV_INT(0);
  cogito_container_set_align(__t1493, __t1494);
  ergo_release_val(__t1493);
  ergo_release_val(__t1494);
  ErgoVal __t1495 = EV_NULLV;
  ergo_release_val(__t1495);
}

static void ergo_m_cogito_VStack_align_center(ErgoVal self) {
  ErgoVal __t1496 = self; ergo_retain_val(__t1496);
  ErgoVal __t1497 = EV_INT(1);
  cogito_container_set_align(__t1496, __t1497);
  ergo_release_val(__t1496);
  ergo_release_val(__t1497);
  ErgoVal __t1498 = EV_NULLV;
  ergo_release_val(__t1498);
}

static void ergo_m_cogito_VStack_align_end(ErgoVal self) {
  ErgoVal __t1499 = self; ergo_retain_val(__t1499);
  ErgoVal __t1500 = EV_INT(2);
  cogito_container_set_align(__t1499, __t1500);
  ergo_release_val(__t1499);
  ergo_release_val(__t1500);
  ErgoVal __t1501 = EV_NULLV;
  ergo_release_val(__t1501);
}

static ErgoVal ergo_m_cogito_VStack_build(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t1502 = self; ergo_retain_val(__t1502);
  ErgoVal __t1503 = a0; ergo_retain_val(__t1503);
  cogito_build(__t1502, __t1503);
  ergo_release_val(__t1502);
  ergo_release_val(__t1503);
  ErgoVal __t1504 = EV_NULLV;
  ergo_release_val(__t1504);
  ErgoVal __t1505 = self; ergo_retain_val(__t1505);
  ergo_move_into(&__ret, __t1505);
  return __ret;
  return __ret;
}

static void ergo_m_cogito_VStack_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1506 = self; ergo_retain_val(__t1506);
  ErgoVal __t1507 = a0; ergo_retain_val(__t1507);
  cogito_node_set_disabled(__t1506, __t1507);
  ergo_release_val(__t1506);
  ergo_release_val(__t1507);
  ErgoVal __t1508 = EV_NULLV;
  ergo_release_val(__t1508);
}

static void ergo_m_cogito_VStack_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1509 = self; ergo_retain_val(__t1509);
  ErgoVal __t1510 = a0; ergo_retain_val(__t1510);
  cogito_node_set_class(__t1509, __t1510);
  ergo_release_val(__t1509);
  ergo_release_val(__t1510);
  ErgoVal __t1511 = EV_NULLV;
  ergo_release_val(__t1511);
}

static void ergo_m_cogito_VStack_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1512 = self; ergo_retain_val(__t1512);
  ErgoVal __t1513 = a0; ergo_retain_val(__t1513);
  cogito_node_set_id(__t1512, __t1513);
  ergo_release_val(__t1512);
  ergo_release_val(__t1513);
  ErgoVal __t1514 = EV_NULLV;
  ergo_release_val(__t1514);
}

static void ergo_m_cogito_HStack_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1515 = self; ergo_retain_val(__t1515);
  ErgoVal __t1516 = a0; ergo_retain_val(__t1516);
  cogito_container_add(__t1515, __t1516);
  ergo_release_val(__t1515);
  ergo_release_val(__t1516);
  ErgoVal __t1517 = EV_NULLV;
  ergo_release_val(__t1517);
}

static void ergo_m_cogito_HStack_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t1518 = self; ergo_retain_val(__t1518);
  ErgoVal __t1519 = a0; ergo_retain_val(__t1519);
  ErgoVal __t1520 = a1; ergo_retain_val(__t1520);
  ErgoVal __t1521 = a2; ergo_retain_val(__t1521);
  ErgoVal __t1522 = a3; ergo_retain_val(__t1522);
  cogito_container_set_margins(__t1518, __t1519, __t1520, __t1521, __t1522);
  ergo_release_val(__t1518);
  ergo_release_val(__t1519);
  ergo_release_val(__t1520);
  ergo_release_val(__t1521);
  ergo_release_val(__t1522);
  ErgoVal __t1523 = EV_NULLV;
  ergo_release_val(__t1523);
}

static void ergo_m_cogito_HStack_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t1524 = self; ergo_retain_val(__t1524);
  ErgoVal __t1525 = a0; ergo_retain_val(__t1525);
  ErgoVal __t1526 = a1; ergo_retain_val(__t1526);
  ErgoVal __t1527 = a2; ergo_retain_val(__t1527);
  ErgoVal __t1528 = a3; ergo_retain_val(__t1528);
  cogito_container_set_padding(__t1524, __t1525, __t1526, __t1527, __t1528);
  ergo_release_val(__t1524);
  ergo_release_val(__t1525);
  ergo_release_val(__t1526);
  ergo_release_val(__t1527);
  ergo_release_val(__t1528);
  ErgoVal __t1529 = EV_NULLV;
  ergo_release_val(__t1529);
}

static void ergo_m_cogito_HStack_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1530 = self; ergo_retain_val(__t1530);
  ErgoVal __t1531 = a0; ergo_retain_val(__t1531);
  cogito_container_set_align(__t1530, __t1531);
  ergo_release_val(__t1530);
  ergo_release_val(__t1531);
  ErgoVal __t1532 = EV_NULLV;
  ergo_release_val(__t1532);
}

static void ergo_m_cogito_HStack_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1533 = self; ergo_retain_val(__t1533);
  ErgoVal __t1534 = a0; ergo_retain_val(__t1534);
  cogito_container_set_halign(__t1533, __t1534);
  ergo_release_val(__t1533);
  ergo_release_val(__t1534);
  ErgoVal __t1535 = EV_NULLV;
  ergo_release_val(__t1535);
}

static void ergo_m_cogito_HStack_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1536 = self; ergo_retain_val(__t1536);
  ErgoVal __t1537 = a0; ergo_retain_val(__t1537);
  cogito_container_set_valign(__t1536, __t1537);
  ergo_release_val(__t1536);
  ergo_release_val(__t1537);
  ErgoVal __t1538 = EV_NULLV;
  ergo_release_val(__t1538);
}

static void ergo_m_cogito_HStack_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1539 = self; ergo_retain_val(__t1539);
  ErgoVal __t1540 = a0; ergo_retain_val(__t1540);
  cogito_container_set_hexpand(__t1539, __t1540);
  ergo_release_val(__t1539);
  ergo_release_val(__t1540);
  ErgoVal __t1541 = EV_NULLV;
  ergo_release_val(__t1541);
}

static void ergo_m_cogito_HStack_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1542 = self; ergo_retain_val(__t1542);
  ErgoVal __t1543 = a0; ergo_retain_val(__t1543);
  cogito_container_set_vexpand(__t1542, __t1543);
  ergo_release_val(__t1542);
  ergo_release_val(__t1543);
  ErgoVal __t1544 = EV_NULLV;
  ergo_release_val(__t1544);
}

static void ergo_m_cogito_HStack_set_gap(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1545 = self; ergo_retain_val(__t1545);
  ErgoVal __t1546 = a0; ergo_retain_val(__t1546);
  cogito_container_set_gap(__t1545, __t1546);
  ergo_release_val(__t1545);
  ergo_release_val(__t1546);
  ErgoVal __t1547 = EV_NULLV;
  ergo_release_val(__t1547);
}

static void ergo_m_cogito_HStack_align_begin(ErgoVal self) {
  ErgoVal __t1548 = self; ergo_retain_val(__t1548);
  ErgoVal __t1549 = EV_INT(0);
  cogito_container_set_align(__t1548, __t1549);
  ergo_release_val(__t1548);
  ergo_release_val(__t1549);
  ErgoVal __t1550 = EV_NULLV;
  ergo_release_val(__t1550);
}

static void ergo_m_cogito_HStack_align_center(ErgoVal self) {
  ErgoVal __t1551 = self; ergo_retain_val(__t1551);
  ErgoVal __t1552 = EV_INT(1);
  cogito_container_set_align(__t1551, __t1552);
  ergo_release_val(__t1551);
  ergo_release_val(__t1552);
  ErgoVal __t1553 = EV_NULLV;
  ergo_release_val(__t1553);
}

static void ergo_m_cogito_HStack_align_end(ErgoVal self) {
  ErgoVal __t1554 = self; ergo_retain_val(__t1554);
  ErgoVal __t1555 = EV_INT(2);
  cogito_container_set_align(__t1554, __t1555);
  ergo_release_val(__t1554);
  ergo_release_val(__t1555);
  ErgoVal __t1556 = EV_NULLV;
  ergo_release_val(__t1556);
}

static ErgoVal ergo_m_cogito_HStack_build(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t1557 = self; ergo_retain_val(__t1557);
  ErgoVal __t1558 = a0; ergo_retain_val(__t1558);
  cogito_build(__t1557, __t1558);
  ergo_release_val(__t1557);
  ergo_release_val(__t1558);
  ErgoVal __t1559 = EV_NULLV;
  ergo_release_val(__t1559);
  ErgoVal __t1560 = self; ergo_retain_val(__t1560);
  ergo_move_into(&__ret, __t1560);
  return __ret;
  return __ret;
}

static void ergo_m_cogito_HStack_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1561 = self; ergo_retain_val(__t1561);
  ErgoVal __t1562 = a0; ergo_retain_val(__t1562);
  cogito_node_set_disabled(__t1561, __t1562);
  ergo_release_val(__t1561);
  ergo_release_val(__t1562);
  ErgoVal __t1563 = EV_NULLV;
  ergo_release_val(__t1563);
}

static void ergo_m_cogito_HStack_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1564 = self; ergo_retain_val(__t1564);
  ErgoVal __t1565 = a0; ergo_retain_val(__t1565);
  cogito_node_set_class(__t1564, __t1565);
  ergo_release_val(__t1564);
  ergo_release_val(__t1565);
  ErgoVal __t1566 = EV_NULLV;
  ergo_release_val(__t1566);
}

static void ergo_m_cogito_HStack_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1567 = self; ergo_retain_val(__t1567);
  ErgoVal __t1568 = a0; ergo_retain_val(__t1568);
  cogito_node_set_id(__t1567, __t1568);
  ergo_release_val(__t1567);
  ergo_release_val(__t1568);
  ErgoVal __t1569 = EV_NULLV;
  ergo_release_val(__t1569);
}

static void ergo_m_cogito_ZStack_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1570 = self; ergo_retain_val(__t1570);
  ErgoVal __t1571 = a0; ergo_retain_val(__t1571);
  cogito_container_add(__t1570, __t1571);
  ergo_release_val(__t1570);
  ergo_release_val(__t1571);
  ErgoVal __t1572 = EV_NULLV;
  ergo_release_val(__t1572);
}

static void ergo_m_cogito_ZStack_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t1573 = self; ergo_retain_val(__t1573);
  ErgoVal __t1574 = a0; ergo_retain_val(__t1574);
  ErgoVal __t1575 = a1; ergo_retain_val(__t1575);
  ErgoVal __t1576 = a2; ergo_retain_val(__t1576);
  ErgoVal __t1577 = a3; ergo_retain_val(__t1577);
  cogito_container_set_margins(__t1573, __t1574, __t1575, __t1576, __t1577);
  ergo_release_val(__t1573);
  ergo_release_val(__t1574);
  ergo_release_val(__t1575);
  ergo_release_val(__t1576);
  ergo_release_val(__t1577);
  ErgoVal __t1578 = EV_NULLV;
  ergo_release_val(__t1578);
}

static void ergo_m_cogito_ZStack_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t1579 = self; ergo_retain_val(__t1579);
  ErgoVal __t1580 = a0; ergo_retain_val(__t1580);
  ErgoVal __t1581 = a1; ergo_retain_val(__t1581);
  ErgoVal __t1582 = a2; ergo_retain_val(__t1582);
  ErgoVal __t1583 = a3; ergo_retain_val(__t1583);
  cogito_container_set_padding(__t1579, __t1580, __t1581, __t1582, __t1583);
  ergo_release_val(__t1579);
  ergo_release_val(__t1580);
  ergo_release_val(__t1581);
  ergo_release_val(__t1582);
  ergo_release_val(__t1583);
  ErgoVal __t1584 = EV_NULLV;
  ergo_release_val(__t1584);
}

static void ergo_m_cogito_ZStack_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1585 = self; ergo_retain_val(__t1585);
  ErgoVal __t1586 = a0; ergo_retain_val(__t1586);
  cogito_container_set_align(__t1585, __t1586);
  ergo_release_val(__t1585);
  ergo_release_val(__t1586);
  ErgoVal __t1587 = EV_NULLV;
  ergo_release_val(__t1587);
}

static void ergo_m_cogito_ZStack_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1588 = self; ergo_retain_val(__t1588);
  ErgoVal __t1589 = a0; ergo_retain_val(__t1589);
  cogito_container_set_halign(__t1588, __t1589);
  ergo_release_val(__t1588);
  ergo_release_val(__t1589);
  ErgoVal __t1590 = EV_NULLV;
  ergo_release_val(__t1590);
}

static void ergo_m_cogito_ZStack_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1591 = self; ergo_retain_val(__t1591);
  ErgoVal __t1592 = a0; ergo_retain_val(__t1592);
  cogito_container_set_valign(__t1591, __t1592);
  ergo_release_val(__t1591);
  ergo_release_val(__t1592);
  ErgoVal __t1593 = EV_NULLV;
  ergo_release_val(__t1593);
}

static void ergo_m_cogito_ZStack_align_begin(ErgoVal self) {
  ErgoVal __t1594 = self; ergo_retain_val(__t1594);
  ErgoVal __t1595 = EV_INT(0);
  cogito_container_set_align(__t1594, __t1595);
  ergo_release_val(__t1594);
  ergo_release_val(__t1595);
  ErgoVal __t1596 = EV_NULLV;
  ergo_release_val(__t1596);
}

static void ergo_m_cogito_ZStack_align_center(ErgoVal self) {
  ErgoVal __t1597 = self; ergo_retain_val(__t1597);
  ErgoVal __t1598 = EV_INT(1);
  cogito_container_set_align(__t1597, __t1598);
  ergo_release_val(__t1597);
  ergo_release_val(__t1598);
  ErgoVal __t1599 = EV_NULLV;
  ergo_release_val(__t1599);
}

static void ergo_m_cogito_ZStack_align_end(ErgoVal self) {
  ErgoVal __t1600 = self; ergo_retain_val(__t1600);
  ErgoVal __t1601 = EV_INT(2);
  cogito_container_set_align(__t1600, __t1601);
  ergo_release_val(__t1600);
  ergo_release_val(__t1601);
  ErgoVal __t1602 = EV_NULLV;
  ergo_release_val(__t1602);
}

static ErgoVal ergo_m_cogito_ZStack_build(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t1603 = self; ergo_retain_val(__t1603);
  ErgoVal __t1604 = a0; ergo_retain_val(__t1604);
  cogito_build(__t1603, __t1604);
  ergo_release_val(__t1603);
  ergo_release_val(__t1604);
  ErgoVal __t1605 = EV_NULLV;
  ergo_release_val(__t1605);
  ErgoVal __t1606 = self; ergo_retain_val(__t1606);
  ergo_move_into(&__ret, __t1606);
  return __ret;
  return __ret;
}

static void ergo_m_cogito_ZStack_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1607 = self; ergo_retain_val(__t1607);
  ErgoVal __t1608 = a0; ergo_retain_val(__t1608);
  cogito_container_set_hexpand(__t1607, __t1608);
  ergo_release_val(__t1607);
  ergo_release_val(__t1608);
  ErgoVal __t1609 = EV_NULLV;
  ergo_release_val(__t1609);
}

static void ergo_m_cogito_ZStack_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1610 = self; ergo_retain_val(__t1610);
  ErgoVal __t1611 = a0; ergo_retain_val(__t1611);
  cogito_container_set_vexpand(__t1610, __t1611);
  ergo_release_val(__t1610);
  ergo_release_val(__t1611);
  ErgoVal __t1612 = EV_NULLV;
  ergo_release_val(__t1612);
}

static void ergo_m_cogito_ZStack_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1613 = self; ergo_retain_val(__t1613);
  ErgoVal __t1614 = a0; ergo_retain_val(__t1614);
  cogito_node_set_disabled(__t1613, __t1614);
  ergo_release_val(__t1613);
  ergo_release_val(__t1614);
  ErgoVal __t1615 = EV_NULLV;
  ergo_release_val(__t1615);
}

static void ergo_m_cogito_ZStack_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1616 = self; ergo_retain_val(__t1616);
  ErgoVal __t1617 = a0; ergo_retain_val(__t1617);
  cogito_node_set_class(__t1616, __t1617);
  ergo_release_val(__t1616);
  ergo_release_val(__t1617);
  ErgoVal __t1618 = EV_NULLV;
  ergo_release_val(__t1618);
}

static void ergo_m_cogito_ZStack_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1619 = self; ergo_retain_val(__t1619);
  ErgoVal __t1620 = a0; ergo_retain_val(__t1620);
  cogito_node_set_id(__t1619, __t1620);
  ergo_release_val(__t1619);
  ergo_release_val(__t1620);
  ErgoVal __t1621 = EV_NULLV;
  ergo_release_val(__t1621);
}

static void ergo_m_cogito_Fixed_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1622 = self; ergo_retain_val(__t1622);
  ErgoVal __t1623 = a0; ergo_retain_val(__t1623);
  cogito_container_add(__t1622, __t1623);
  ergo_release_val(__t1622);
  ergo_release_val(__t1623);
  ErgoVal __t1624 = EV_NULLV;
  ergo_release_val(__t1624);
}

static void ergo_m_cogito_Fixed_set_pos(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2) {
  ErgoVal __t1625 = self; ergo_retain_val(__t1625);
  ErgoVal __t1626 = a0; ergo_retain_val(__t1626);
  ErgoVal __t1627 = a1; ergo_retain_val(__t1627);
  ErgoVal __t1628 = a2; ergo_retain_val(__t1628);
  cogito_fixed_set_pos(__t1625, __t1626, __t1627, __t1628);
  ergo_release_val(__t1625);
  ergo_release_val(__t1626);
  ergo_release_val(__t1627);
  ergo_release_val(__t1628);
  ErgoVal __t1629 = EV_NULLV;
  ergo_release_val(__t1629);
}

static void ergo_m_cogito_Fixed_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t1630 = self; ergo_retain_val(__t1630);
  ErgoVal __t1631 = a0; ergo_retain_val(__t1631);
  ErgoVal __t1632 = a1; ergo_retain_val(__t1632);
  ErgoVal __t1633 = a2; ergo_retain_val(__t1633);
  ErgoVal __t1634 = a3; ergo_retain_val(__t1634);
  cogito_container_set_padding(__t1630, __t1631, __t1632, __t1633, __t1634);
  ergo_release_val(__t1630);
  ergo_release_val(__t1631);
  ergo_release_val(__t1632);
  ergo_release_val(__t1633);
  ergo_release_val(__t1634);
  ErgoVal __t1635 = EV_NULLV;
  ergo_release_val(__t1635);
}

static ErgoVal ergo_m_cogito_Fixed_build(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t1636 = self; ergo_retain_val(__t1636);
  ErgoVal __t1637 = a0; ergo_retain_val(__t1637);
  cogito_build(__t1636, __t1637);
  ergo_release_val(__t1636);
  ergo_release_val(__t1637);
  ErgoVal __t1638 = EV_NULLV;
  ergo_release_val(__t1638);
  ErgoVal __t1639 = self; ergo_retain_val(__t1639);
  ergo_move_into(&__ret, __t1639);
  return __ret;
  return __ret;
}

static void ergo_m_cogito_Fixed_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1640 = self; ergo_retain_val(__t1640);
  ErgoVal __t1641 = a0; ergo_retain_val(__t1641);
  cogito_container_set_hexpand(__t1640, __t1641);
  ergo_release_val(__t1640);
  ergo_release_val(__t1641);
  ErgoVal __t1642 = EV_NULLV;
  ergo_release_val(__t1642);
}

static void ergo_m_cogito_Fixed_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1643 = self; ergo_retain_val(__t1643);
  ErgoVal __t1644 = a0; ergo_retain_val(__t1644);
  cogito_container_set_vexpand(__t1643, __t1644);
  ergo_release_val(__t1643);
  ergo_release_val(__t1644);
  ErgoVal __t1645 = EV_NULLV;
  ergo_release_val(__t1645);
}

static void ergo_m_cogito_Fixed_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1646 = self; ergo_retain_val(__t1646);
  ErgoVal __t1647 = a0; ergo_retain_val(__t1647);
  cogito_node_set_disabled(__t1646, __t1647);
  ergo_release_val(__t1646);
  ergo_release_val(__t1647);
  ErgoVal __t1648 = EV_NULLV;
  ergo_release_val(__t1648);
}

static void ergo_m_cogito_Fixed_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1649 = self; ergo_retain_val(__t1649);
  ErgoVal __t1650 = a0; ergo_retain_val(__t1650);
  cogito_node_set_class(__t1649, __t1650);
  ergo_release_val(__t1649);
  ergo_release_val(__t1650);
  ErgoVal __t1651 = EV_NULLV;
  ergo_release_val(__t1651);
}

static void ergo_m_cogito_Fixed_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1652 = self; ergo_retain_val(__t1652);
  ErgoVal __t1653 = a0; ergo_retain_val(__t1653);
  cogito_node_set_id(__t1652, __t1653);
  ergo_release_val(__t1652);
  ergo_release_val(__t1653);
  ErgoVal __t1654 = EV_NULLV;
  ergo_release_val(__t1654);
}

static void ergo_m_cogito_Scroller_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1655 = self; ergo_retain_val(__t1655);
  ErgoVal __t1656 = a0; ergo_retain_val(__t1656);
  cogito_container_add(__t1655, __t1656);
  ergo_release_val(__t1655);
  ergo_release_val(__t1656);
  ErgoVal __t1657 = EV_NULLV;
  ergo_release_val(__t1657);
}

static void ergo_m_cogito_Scroller_set_axes(ErgoVal self, ErgoVal a0, ErgoVal a1) {
  ErgoVal __t1658 = self; ergo_retain_val(__t1658);
  ErgoVal __t1659 = a0; ergo_retain_val(__t1659);
  ErgoVal __t1660 = a1; ergo_retain_val(__t1660);
  cogito_scroller_set_axes(__t1658, __t1659, __t1660);
  ergo_release_val(__t1658);
  ergo_release_val(__t1659);
  ergo_release_val(__t1660);
  ErgoVal __t1661 = EV_NULLV;
  ergo_release_val(__t1661);
}

static void ergo_m_cogito_Scroller_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t1662 = self; ergo_retain_val(__t1662);
  ErgoVal __t1663 = a0; ergo_retain_val(__t1663);
  ErgoVal __t1664 = a1; ergo_retain_val(__t1664);
  ErgoVal __t1665 = a2; ergo_retain_val(__t1665);
  ErgoVal __t1666 = a3; ergo_retain_val(__t1666);
  cogito_container_set_padding(__t1662, __t1663, __t1664, __t1665, __t1666);
  ergo_release_val(__t1662);
  ergo_release_val(__t1663);
  ergo_release_val(__t1664);
  ergo_release_val(__t1665);
  ergo_release_val(__t1666);
  ErgoVal __t1667 = EV_NULLV;
  ergo_release_val(__t1667);
}

static ErgoVal ergo_m_cogito_Scroller_build(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t1668 = self; ergo_retain_val(__t1668);
  ErgoVal __t1669 = a0; ergo_retain_val(__t1669);
  cogito_build(__t1668, __t1669);
  ergo_release_val(__t1668);
  ergo_release_val(__t1669);
  ErgoVal __t1670 = EV_NULLV;
  ergo_release_val(__t1670);
  ErgoVal __t1671 = self; ergo_retain_val(__t1671);
  ergo_move_into(&__ret, __t1671);
  return __ret;
  return __ret;
}

static void ergo_m_cogito_Scroller_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1672 = self; ergo_retain_val(__t1672);
  ErgoVal __t1673 = a0; ergo_retain_val(__t1673);
  cogito_container_set_hexpand(__t1672, __t1673);
  ergo_release_val(__t1672);
  ergo_release_val(__t1673);
  ErgoVal __t1674 = EV_NULLV;
  ergo_release_val(__t1674);
}

static void ergo_m_cogito_Scroller_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1675 = self; ergo_retain_val(__t1675);
  ErgoVal __t1676 = a0; ergo_retain_val(__t1676);
  cogito_container_set_vexpand(__t1675, __t1676);
  ergo_release_val(__t1675);
  ergo_release_val(__t1676);
  ErgoVal __t1677 = EV_NULLV;
  ergo_release_val(__t1677);
}

static void ergo_m_cogito_Scroller_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1678 = self; ergo_retain_val(__t1678);
  ErgoVal __t1679 = a0; ergo_retain_val(__t1679);
  cogito_node_set_disabled(__t1678, __t1679);
  ergo_release_val(__t1678);
  ergo_release_val(__t1679);
  ErgoVal __t1680 = EV_NULLV;
  ergo_release_val(__t1680);
}

static void ergo_m_cogito_Scroller_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1681 = self; ergo_retain_val(__t1681);
  ErgoVal __t1682 = a0; ergo_retain_val(__t1682);
  cogito_node_set_class(__t1681, __t1682);
  ergo_release_val(__t1681);
  ergo_release_val(__t1682);
  ErgoVal __t1683 = EV_NULLV;
  ergo_release_val(__t1683);
}

static void ergo_m_cogito_Scroller_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1684 = self; ergo_retain_val(__t1684);
  ErgoVal __t1685 = a0; ergo_retain_val(__t1685);
  cogito_node_set_id(__t1684, __t1685);
  ergo_release_val(__t1684);
  ergo_release_val(__t1685);
  ErgoVal __t1686 = EV_NULLV;
  ergo_release_val(__t1686);
}

static void ergo_m_cogito_Carousel_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1687 = self; ergo_retain_val(__t1687);
  ErgoVal __t1688 = a0; ergo_retain_val(__t1688);
  cogito_container_add(__t1687, __t1688);
  ergo_release_val(__t1687);
  ergo_release_val(__t1688);
  ErgoVal __t1689 = EV_NULLV;
  ergo_release_val(__t1689);
}

static void ergo_m_cogito_Carousel_set_active_index(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1690 = self; ergo_retain_val(__t1690);
  ErgoVal __t1691 = a0; ergo_retain_val(__t1691);
  cogito_carousel_set_active_index(__t1690, __t1691);
  ergo_release_val(__t1690);
  ergo_release_val(__t1691);
  ErgoVal __t1692 = EV_NULLV;
  ergo_release_val(__t1692);
}

static ErgoVal ergo_m_cogito_Carousel_active_index(ErgoVal self) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t1693 = self; ergo_retain_val(__t1693);
  ErgoVal __t1694 = cogito_carousel_get_active_index(__t1693);
  ergo_release_val(__t1693);
  ergo_move_into(&__ret, __t1694);
  return __ret;
  return __ret;
}

static void ergo_m_cogito_Carousel_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1695 = self; ergo_retain_val(__t1695);
  ErgoVal __t1696 = a0; ergo_retain_val(__t1696);
  cogito_container_set_hexpand(__t1695, __t1696);
  ergo_release_val(__t1695);
  ergo_release_val(__t1696);
  ErgoVal __t1697 = EV_NULLV;
  ergo_release_val(__t1697);
}

static void ergo_m_cogito_Carousel_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1698 = self; ergo_retain_val(__t1698);
  ErgoVal __t1699 = a0; ergo_retain_val(__t1699);
  cogito_container_set_vexpand(__t1698, __t1699);
  ergo_release_val(__t1698);
  ergo_release_val(__t1699);
  ErgoVal __t1700 = EV_NULLV;
  ergo_release_val(__t1700);
}

static void ergo_m_cogito_Carousel_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1701 = self; ergo_retain_val(__t1701);
  ErgoVal __t1702 = a0; ergo_retain_val(__t1702);
  cogito_node_set_class(__t1701, __t1702);
  ergo_release_val(__t1701);
  ergo_release_val(__t1702);
  ErgoVal __t1703 = EV_NULLV;
  ergo_release_val(__t1703);
}

static void ergo_m_cogito_Carousel_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1704 = self; ergo_retain_val(__t1704);
  ErgoVal __t1705 = a0; ergo_retain_val(__t1705);
  cogito_node_set_id(__t1704, __t1705);
  ergo_release_val(__t1704);
  ergo_release_val(__t1705);
  ErgoVal __t1706 = EV_NULLV;
  ergo_release_val(__t1706);
}

static void ergo_m_cogito_CarouselItem_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1707 = self; ergo_retain_val(__t1707);
  ErgoVal __t1708 = a0; ergo_retain_val(__t1708);
  cogito_container_add(__t1707, __t1708);
  ergo_release_val(__t1707);
  ergo_release_val(__t1708);
  ErgoVal __t1709 = EV_NULLV;
  ergo_release_val(__t1709);
}

static void ergo_m_cogito_CarouselItem_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1710 = self; ergo_retain_val(__t1710);
  ErgoVal __t1711 = a0; ergo_retain_val(__t1711);
  cogito_container_set_hexpand(__t1710, __t1711);
  ergo_release_val(__t1710);
  ergo_release_val(__t1711);
  ErgoVal __t1712 = EV_NULLV;
  ergo_release_val(__t1712);
}

static void ergo_m_cogito_CarouselItem_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1713 = self; ergo_retain_val(__t1713);
  ErgoVal __t1714 = a0; ergo_retain_val(__t1714);
  cogito_container_set_vexpand(__t1713, __t1714);
  ergo_release_val(__t1713);
  ergo_release_val(__t1714);
  ErgoVal __t1715 = EV_NULLV;
  ergo_release_val(__t1715);
}

static void ergo_m_cogito_CarouselItem_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1716 = self; ergo_retain_val(__t1716);
  ErgoVal __t1717 = a0; ergo_retain_val(__t1717);
  cogito_node_set_class(__t1716, __t1717);
  ergo_release_val(__t1716);
  ergo_release_val(__t1717);
  ErgoVal __t1718 = EV_NULLV;
  ergo_release_val(__t1718);
}

static void ergo_m_cogito_CarouselItem_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1719 = self; ergo_retain_val(__t1719);
  ErgoVal __t1720 = a0; ergo_retain_val(__t1720);
  cogito_node_set_id(__t1719, __t1720);
  ergo_release_val(__t1719);
  ergo_release_val(__t1720);
  ErgoVal __t1721 = EV_NULLV;
  ergo_release_val(__t1721);
}

static void ergo_m_cogito_CarouselItem_set_text(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1722 = self; ergo_retain_val(__t1722);
  ErgoVal __t1723 = a0; ergo_retain_val(__t1723);
  cogito_carousel_item_set_text(__t1722, __t1723);
  ergo_release_val(__t1722);
  ergo_release_val(__t1723);
  ErgoVal __t1724 = EV_NULLV;
  ergo_release_val(__t1724);
}

static void ergo_m_cogito_CarouselItem_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1725 = self; ergo_retain_val(__t1725);
  ErgoVal __t1726 = a0; ergo_retain_val(__t1726);
  cogito_carousel_item_set_halign(__t1725, __t1726);
  ergo_release_val(__t1725);
  ergo_release_val(__t1726);
  ErgoVal __t1727 = EV_NULLV;
  ergo_release_val(__t1727);
}

static void ergo_m_cogito_CarouselItem_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1728 = self; ergo_retain_val(__t1728);
  ErgoVal __t1729 = a0; ergo_retain_val(__t1729);
  cogito_carousel_item_set_valign(__t1728, __t1729);
  ergo_release_val(__t1728);
  ergo_release_val(__t1729);
  ErgoVal __t1730 = EV_NULLV;
  ergo_release_val(__t1730);
}

static void ergo_m_cogito_List_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1731 = self; ergo_retain_val(__t1731);
  ErgoVal __t1732 = a0; ergo_retain_val(__t1732);
  cogito_container_add(__t1731, __t1732);
  ergo_release_val(__t1731);
  ergo_release_val(__t1732);
  ErgoVal __t1733 = EV_NULLV;
  ergo_release_val(__t1733);
}

static void ergo_m_cogito_List_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t1734 = self; ergo_retain_val(__t1734);
  ErgoVal __t1735 = a0; ergo_retain_val(__t1735);
  ErgoVal __t1736 = a1; ergo_retain_val(__t1736);
  ErgoVal __t1737 = a2; ergo_retain_val(__t1737);
  ErgoVal __t1738 = a3; ergo_retain_val(__t1738);
  cogito_container_set_margins(__t1734, __t1735, __t1736, __t1737, __t1738);
  ergo_release_val(__t1734);
  ergo_release_val(__t1735);
  ergo_release_val(__t1736);
  ergo_release_val(__t1737);
  ergo_release_val(__t1738);
  ErgoVal __t1739 = EV_NULLV;
  ergo_release_val(__t1739);
}

static void ergo_m_cogito_List_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t1740 = self; ergo_retain_val(__t1740);
  ErgoVal __t1741 = a0; ergo_retain_val(__t1741);
  ErgoVal __t1742 = a1; ergo_retain_val(__t1742);
  ErgoVal __t1743 = a2; ergo_retain_val(__t1743);
  ErgoVal __t1744 = a3; ergo_retain_val(__t1744);
  cogito_container_set_padding(__t1740, __t1741, __t1742, __t1743, __t1744);
  ergo_release_val(__t1740);
  ergo_release_val(__t1741);
  ergo_release_val(__t1742);
  ergo_release_val(__t1743);
  ergo_release_val(__t1744);
  ErgoVal __t1745 = EV_NULLV;
  ergo_release_val(__t1745);
}

static void ergo_m_cogito_List_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1746 = self; ergo_retain_val(__t1746);
  ErgoVal __t1747 = a0; ergo_retain_val(__t1747);
  cogito_container_set_align(__t1746, __t1747);
  ergo_release_val(__t1746);
  ergo_release_val(__t1747);
  ErgoVal __t1748 = EV_NULLV;
  ergo_release_val(__t1748);
}

static void ergo_m_cogito_List_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1749 = self; ergo_retain_val(__t1749);
  ErgoVal __t1750 = a0; ergo_retain_val(__t1750);
  cogito_container_set_halign(__t1749, __t1750);
  ergo_release_val(__t1749);
  ergo_release_val(__t1750);
  ErgoVal __t1751 = EV_NULLV;
  ergo_release_val(__t1751);
}

static void ergo_m_cogito_List_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1752 = self; ergo_retain_val(__t1752);
  ErgoVal __t1753 = a0; ergo_retain_val(__t1753);
  cogito_container_set_valign(__t1752, __t1753);
  ergo_release_val(__t1752);
  ergo_release_val(__t1753);
  ErgoVal __t1754 = EV_NULLV;
  ergo_release_val(__t1754);
}

static void ergo_m_cogito_List_align_begin(ErgoVal self) {
  ErgoVal __t1755 = self; ergo_retain_val(__t1755);
  ErgoVal __t1756 = EV_INT(0);
  cogito_container_set_align(__t1755, __t1756);
  ergo_release_val(__t1755);
  ergo_release_val(__t1756);
  ErgoVal __t1757 = EV_NULLV;
  ergo_release_val(__t1757);
}

static void ergo_m_cogito_List_align_center(ErgoVal self) {
  ErgoVal __t1758 = self; ergo_retain_val(__t1758);
  ErgoVal __t1759 = EV_INT(1);
  cogito_container_set_align(__t1758, __t1759);
  ergo_release_val(__t1758);
  ergo_release_val(__t1759);
  ErgoVal __t1760 = EV_NULLV;
  ergo_release_val(__t1760);
}

static void ergo_m_cogito_List_align_end(ErgoVal self) {
  ErgoVal __t1761 = self; ergo_retain_val(__t1761);
  ErgoVal __t1762 = EV_INT(2);
  cogito_container_set_align(__t1761, __t1762);
  ergo_release_val(__t1761);
  ergo_release_val(__t1762);
  ErgoVal __t1763 = EV_NULLV;
  ergo_release_val(__t1763);
}

static void ergo_m_cogito_List_on_select(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1764 = self; ergo_retain_val(__t1764);
  ErgoVal __t1765 = a0; ergo_retain_val(__t1765);
  cogito_list_on_select(__t1764, __t1765);
  ergo_release_val(__t1764);
  ergo_release_val(__t1765);
  ErgoVal __t1766 = EV_NULLV;
  ergo_release_val(__t1766);
}

static void ergo_m_cogito_List_on_activate(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1767 = self; ergo_retain_val(__t1767);
  ErgoVal __t1768 = a0; ergo_retain_val(__t1768);
  cogito_list_on_activate(__t1767, __t1768);
  ergo_release_val(__t1767);
  ergo_release_val(__t1768);
  ErgoVal __t1769 = EV_NULLV;
  ergo_release_val(__t1769);
}

static ErgoVal ergo_m_cogito_List_build(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t1770 = self; ergo_retain_val(__t1770);
  ErgoVal __t1771 = a0; ergo_retain_val(__t1771);
  cogito_build(__t1770, __t1771);
  ergo_release_val(__t1770);
  ergo_release_val(__t1771);
  ErgoVal __t1772 = EV_NULLV;
  ergo_release_val(__t1772);
  ErgoVal __t1773 = self; ergo_retain_val(__t1773);
  ergo_move_into(&__ret, __t1773);
  return __ret;
  return __ret;
}

static void ergo_m_cogito_List_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1774 = self; ergo_retain_val(__t1774);
  ErgoVal __t1775 = a0; ergo_retain_val(__t1775);
  cogito_container_set_hexpand(__t1774, __t1775);
  ergo_release_val(__t1774);
  ergo_release_val(__t1775);
  ErgoVal __t1776 = EV_NULLV;
  ergo_release_val(__t1776);
}

static void ergo_m_cogito_List_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1777 = self; ergo_retain_val(__t1777);
  ErgoVal __t1778 = a0; ergo_retain_val(__t1778);
  cogito_container_set_vexpand(__t1777, __t1778);
  ergo_release_val(__t1777);
  ergo_release_val(__t1778);
  ErgoVal __t1779 = EV_NULLV;
  ergo_release_val(__t1779);
}

static void ergo_m_cogito_List_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1780 = self; ergo_retain_val(__t1780);
  ErgoVal __t1781 = a0; ergo_retain_val(__t1781);
  cogito_node_set_disabled(__t1780, __t1781);
  ergo_release_val(__t1780);
  ergo_release_val(__t1781);
  ErgoVal __t1782 = EV_NULLV;
  ergo_release_val(__t1782);
}

static void ergo_m_cogito_List_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1783 = self; ergo_retain_val(__t1783);
  ErgoVal __t1784 = a0; ergo_retain_val(__t1784);
  cogito_node_set_class(__t1783, __t1784);
  ergo_release_val(__t1783);
  ergo_release_val(__t1784);
  ErgoVal __t1785 = EV_NULLV;
  ergo_release_val(__t1785);
}

static void ergo_m_cogito_List_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1786 = self; ergo_retain_val(__t1786);
  ErgoVal __t1787 = a0; ergo_retain_val(__t1787);
  cogito_node_set_id(__t1786, __t1787);
  ergo_release_val(__t1786);
  ergo_release_val(__t1787);
  ErgoVal __t1788 = EV_NULLV;
  ergo_release_val(__t1788);
}

static void ergo_m_cogito_Grid_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1789 = self; ergo_retain_val(__t1789);
  ErgoVal __t1790 = a0; ergo_retain_val(__t1790);
  cogito_container_add(__t1789, __t1790);
  ergo_release_val(__t1789);
  ergo_release_val(__t1790);
  ErgoVal __t1791 = EV_NULLV;
  ergo_release_val(__t1791);
}

static void ergo_m_cogito_Grid_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t1792 = self; ergo_retain_val(__t1792);
  ErgoVal __t1793 = a0; ergo_retain_val(__t1793);
  ErgoVal __t1794 = a1; ergo_retain_val(__t1794);
  ErgoVal __t1795 = a2; ergo_retain_val(__t1795);
  ErgoVal __t1796 = a3; ergo_retain_val(__t1796);
  cogito_container_set_margins(__t1792, __t1793, __t1794, __t1795, __t1796);
  ergo_release_val(__t1792);
  ergo_release_val(__t1793);
  ergo_release_val(__t1794);
  ergo_release_val(__t1795);
  ergo_release_val(__t1796);
  ErgoVal __t1797 = EV_NULLV;
  ergo_release_val(__t1797);
}

static void ergo_m_cogito_Grid_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t1798 = self; ergo_retain_val(__t1798);
  ErgoVal __t1799 = a0; ergo_retain_val(__t1799);
  ErgoVal __t1800 = a1; ergo_retain_val(__t1800);
  ErgoVal __t1801 = a2; ergo_retain_val(__t1801);
  ErgoVal __t1802 = a3; ergo_retain_val(__t1802);
  cogito_container_set_padding(__t1798, __t1799, __t1800, __t1801, __t1802);
  ergo_release_val(__t1798);
  ergo_release_val(__t1799);
  ergo_release_val(__t1800);
  ergo_release_val(__t1801);
  ergo_release_val(__t1802);
  ErgoVal __t1803 = EV_NULLV;
  ergo_release_val(__t1803);
}

static void ergo_m_cogito_Grid_set_gap(ErgoVal self, ErgoVal a0, ErgoVal a1) {
  ErgoVal __t1804 = self; ergo_retain_val(__t1804);
  ErgoVal __t1805 = a0; ergo_retain_val(__t1805);
  ErgoVal __t1806 = a1; ergo_retain_val(__t1806);
  cogito_grid_set_gap(__t1804, __t1805, __t1806);
  ergo_release_val(__t1804);
  ergo_release_val(__t1805);
  ergo_release_val(__t1806);
  ErgoVal __t1807 = EV_NULLV;
  ergo_release_val(__t1807);
}

static void ergo_m_cogito_Grid_set_span(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2) {
  ErgoVal __t1808 = a0; ergo_retain_val(__t1808);
  ErgoVal __t1809 = a1; ergo_retain_val(__t1809);
  ErgoVal __t1810 = a2; ergo_retain_val(__t1810);
  cogito_grid_set_span(__t1808, __t1809, __t1810);
  ergo_release_val(__t1808);
  ergo_release_val(__t1809);
  ergo_release_val(__t1810);
  ErgoVal __t1811 = EV_NULLV;
  ergo_release_val(__t1811);
}

static void ergo_m_cogito_Grid_set_cell_align(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2) {
  ErgoVal __t1812 = a0; ergo_retain_val(__t1812);
  ErgoVal __t1813 = a1; ergo_retain_val(__t1813);
  ErgoVal __t1814 = a2; ergo_retain_val(__t1814);
  cogito_grid_set_align(__t1812, __t1813, __t1814);
  ergo_release_val(__t1812);
  ergo_release_val(__t1813);
  ergo_release_val(__t1814);
  ErgoVal __t1815 = EV_NULLV;
  ergo_release_val(__t1815);
}

static void ergo_m_cogito_Grid_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1816 = self; ergo_retain_val(__t1816);
  ErgoVal __t1817 = a0; ergo_retain_val(__t1817);
  cogito_container_set_align(__t1816, __t1817);
  ergo_release_val(__t1816);
  ergo_release_val(__t1817);
  ErgoVal __t1818 = EV_NULLV;
  ergo_release_val(__t1818);
}

static void ergo_m_cogito_Grid_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1819 = self; ergo_retain_val(__t1819);
  ErgoVal __t1820 = a0; ergo_retain_val(__t1820);
  cogito_container_set_halign(__t1819, __t1820);
  ergo_release_val(__t1819);
  ergo_release_val(__t1820);
  ErgoVal __t1821 = EV_NULLV;
  ergo_release_val(__t1821);
}

static void ergo_m_cogito_Grid_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1822 = self; ergo_retain_val(__t1822);
  ErgoVal __t1823 = a0; ergo_retain_val(__t1823);
  cogito_container_set_valign(__t1822, __t1823);
  ergo_release_val(__t1822);
  ergo_release_val(__t1823);
  ErgoVal __t1824 = EV_NULLV;
  ergo_release_val(__t1824);
}

static void ergo_m_cogito_Grid_align_begin(ErgoVal self) {
  ErgoVal __t1825 = self; ergo_retain_val(__t1825);
  ErgoVal __t1826 = EV_INT(0);
  cogito_container_set_align(__t1825, __t1826);
  ergo_release_val(__t1825);
  ergo_release_val(__t1826);
  ErgoVal __t1827 = EV_NULLV;
  ergo_release_val(__t1827);
}

static void ergo_m_cogito_Grid_align_center(ErgoVal self) {
  ErgoVal __t1828 = self; ergo_retain_val(__t1828);
  ErgoVal __t1829 = EV_INT(1);
  cogito_container_set_align(__t1828, __t1829);
  ergo_release_val(__t1828);
  ergo_release_val(__t1829);
  ErgoVal __t1830 = EV_NULLV;
  ergo_release_val(__t1830);
}

static void ergo_m_cogito_Grid_align_end(ErgoVal self) {
  ErgoVal __t1831 = self; ergo_retain_val(__t1831);
  ErgoVal __t1832 = EV_INT(2);
  cogito_container_set_align(__t1831, __t1832);
  ergo_release_val(__t1831);
  ergo_release_val(__t1832);
  ErgoVal __t1833 = EV_NULLV;
  ergo_release_val(__t1833);
}

static void ergo_m_cogito_Grid_on_select(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1834 = self; ergo_retain_val(__t1834);
  ErgoVal __t1835 = a0; ergo_retain_val(__t1835);
  cogito_grid_on_select(__t1834, __t1835);
  ergo_release_val(__t1834);
  ergo_release_val(__t1835);
  ErgoVal __t1836 = EV_NULLV;
  ergo_release_val(__t1836);
}

static void ergo_m_cogito_Grid_on_activate(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1837 = self; ergo_retain_val(__t1837);
  ErgoVal __t1838 = a0; ergo_retain_val(__t1838);
  cogito_grid_on_activate(__t1837, __t1838);
  ergo_release_val(__t1837);
  ergo_release_val(__t1838);
  ErgoVal __t1839 = EV_NULLV;
  ergo_release_val(__t1839);
}

static ErgoVal ergo_m_cogito_Grid_build(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t1840 = self; ergo_retain_val(__t1840);
  ErgoVal __t1841 = a0; ergo_retain_val(__t1841);
  cogito_build(__t1840, __t1841);
  ergo_release_val(__t1840);
  ergo_release_val(__t1841);
  ErgoVal __t1842 = EV_NULLV;
  ergo_release_val(__t1842);
  ErgoVal __t1843 = self; ergo_retain_val(__t1843);
  ergo_move_into(&__ret, __t1843);
  return __ret;
  return __ret;
}

static void ergo_m_cogito_Grid_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1844 = self; ergo_retain_val(__t1844);
  ErgoVal __t1845 = a0; ergo_retain_val(__t1845);
  cogito_container_set_hexpand(__t1844, __t1845);
  ergo_release_val(__t1844);
  ergo_release_val(__t1845);
  ErgoVal __t1846 = EV_NULLV;
  ergo_release_val(__t1846);
}

static void ergo_m_cogito_Grid_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1847 = self; ergo_retain_val(__t1847);
  ErgoVal __t1848 = a0; ergo_retain_val(__t1848);
  cogito_container_set_vexpand(__t1847, __t1848);
  ergo_release_val(__t1847);
  ergo_release_val(__t1848);
  ErgoVal __t1849 = EV_NULLV;
  ergo_release_val(__t1849);
}

static void ergo_m_cogito_Grid_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1850 = self; ergo_retain_val(__t1850);
  ErgoVal __t1851 = a0; ergo_retain_val(__t1851);
  cogito_node_set_disabled(__t1850, __t1851);
  ergo_release_val(__t1850);
  ergo_release_val(__t1851);
  ErgoVal __t1852 = EV_NULLV;
  ergo_release_val(__t1852);
}

static void ergo_m_cogito_Grid_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1853 = self; ergo_retain_val(__t1853);
  ErgoVal __t1854 = a0; ergo_retain_val(__t1854);
  cogito_node_set_class(__t1853, __t1854);
  ergo_release_val(__t1853);
  ergo_release_val(__t1854);
  ErgoVal __t1855 = EV_NULLV;
  ergo_release_val(__t1855);
}

static void ergo_m_cogito_Grid_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1856 = self; ergo_retain_val(__t1856);
  ErgoVal __t1857 = a0; ergo_retain_val(__t1857);
  cogito_node_set_id(__t1856, __t1857);
  ergo_release_val(__t1856);
  ergo_release_val(__t1857);
  ErgoVal __t1858 = EV_NULLV;
  ergo_release_val(__t1858);
}

static void ergo_m_cogito_Label_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t1859 = self; ergo_retain_val(__t1859);
  ErgoVal __t1860 = a0; ergo_retain_val(__t1860);
  ErgoVal __t1861 = a1; ergo_retain_val(__t1861);
  ErgoVal __t1862 = a2; ergo_retain_val(__t1862);
  ErgoVal __t1863 = a3; ergo_retain_val(__t1863);
  cogito_container_set_margins(__t1859, __t1860, __t1861, __t1862, __t1863);
  ergo_release_val(__t1859);
  ergo_release_val(__t1860);
  ergo_release_val(__t1861);
  ergo_release_val(__t1862);
  ergo_release_val(__t1863);
  ErgoVal __t1864 = EV_NULLV;
  ergo_release_val(__t1864);
}

static void ergo_m_cogito_Label_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t1865 = self; ergo_retain_val(__t1865);
  ErgoVal __t1866 = a0; ergo_retain_val(__t1866);
  ErgoVal __t1867 = a1; ergo_retain_val(__t1867);
  ErgoVal __t1868 = a2; ergo_retain_val(__t1868);
  ErgoVal __t1869 = a3; ergo_retain_val(__t1869);
  cogito_container_set_padding(__t1865, __t1866, __t1867, __t1868, __t1869);
  ergo_release_val(__t1865);
  ergo_release_val(__t1866);
  ergo_release_val(__t1867);
  ergo_release_val(__t1868);
  ergo_release_val(__t1869);
  ErgoVal __t1870 = EV_NULLV;
  ergo_release_val(__t1870);
}

static void ergo_m_cogito_Label_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1871 = self; ergo_retain_val(__t1871);
  ErgoVal __t1872 = a0; ergo_retain_val(__t1872);
  cogito_container_set_align(__t1871, __t1872);
  ergo_release_val(__t1871);
  ergo_release_val(__t1872);
  ErgoVal __t1873 = EV_NULLV;
  ergo_release_val(__t1873);
}

static void ergo_m_cogito_Label_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1874 = self; ergo_retain_val(__t1874);
  ErgoVal __t1875 = a0; ergo_retain_val(__t1875);
  cogito_container_set_halign(__t1874, __t1875);
  ergo_release_val(__t1874);
  ergo_release_val(__t1875);
  ErgoVal __t1876 = EV_NULLV;
  ergo_release_val(__t1876);
}

static void ergo_m_cogito_Label_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1877 = self; ergo_retain_val(__t1877);
  ErgoVal __t1878 = a0; ergo_retain_val(__t1878);
  cogito_container_set_valign(__t1877, __t1878);
  ergo_release_val(__t1877);
  ergo_release_val(__t1878);
  ErgoVal __t1879 = EV_NULLV;
  ergo_release_val(__t1879);
}

static void ergo_m_cogito_Label_align_begin(ErgoVal self) {
  ErgoVal __t1880 = self; ergo_retain_val(__t1880);
  ErgoVal __t1881 = EV_INT(0);
  cogito_container_set_align(__t1880, __t1881);
  ergo_release_val(__t1880);
  ergo_release_val(__t1881);
  ErgoVal __t1882 = EV_NULLV;
  ergo_release_val(__t1882);
}

static void ergo_m_cogito_Label_align_center(ErgoVal self) {
  ErgoVal __t1883 = self; ergo_retain_val(__t1883);
  ErgoVal __t1884 = EV_INT(1);
  cogito_container_set_align(__t1883, __t1884);
  ergo_release_val(__t1883);
  ergo_release_val(__t1884);
  ErgoVal __t1885 = EV_NULLV;
  ergo_release_val(__t1885);
}

static void ergo_m_cogito_Label_align_end(ErgoVal self) {
  ErgoVal __t1886 = self; ergo_retain_val(__t1886);
  ErgoVal __t1887 = EV_INT(2);
  cogito_container_set_align(__t1886, __t1887);
  ergo_release_val(__t1886);
  ergo_release_val(__t1887);
  ErgoVal __t1888 = EV_NULLV;
  ergo_release_val(__t1888);
}

static void ergo_m_cogito_Label_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1889 = self; ergo_retain_val(__t1889);
  ErgoVal __t1890 = a0; ergo_retain_val(__t1890);
  cogito_label_set_class(__t1889, __t1890);
  ergo_release_val(__t1889);
  ergo_release_val(__t1890);
  ErgoVal __t1891 = EV_NULLV;
  ergo_release_val(__t1891);
}

static void ergo_m_cogito_Label_set_text(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1892 = self; ergo_retain_val(__t1892);
  ErgoVal __t1893 = a0; ergo_retain_val(__t1893);
  cogito_label_set_text(__t1892, __t1893);
  ergo_release_val(__t1892);
  ergo_release_val(__t1893);
  ErgoVal __t1894 = EV_NULLV;
  ergo_release_val(__t1894);
}

static void ergo_m_cogito_Label_set_wrap(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1895 = self; ergo_retain_val(__t1895);
  ErgoVal __t1896 = a0; ergo_retain_val(__t1896);
  cogito_label_set_wrap(__t1895, __t1896);
  ergo_release_val(__t1895);
  ergo_release_val(__t1896);
  ErgoVal __t1897 = EV_NULLV;
  ergo_release_val(__t1897);
}

static void ergo_m_cogito_Label_set_ellipsis(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1898 = self; ergo_retain_val(__t1898);
  ErgoVal __t1899 = a0; ergo_retain_val(__t1899);
  cogito_label_set_ellipsis(__t1898, __t1899);
  ergo_release_val(__t1898);
  ergo_release_val(__t1899);
  ErgoVal __t1900 = EV_NULLV;
  ergo_release_val(__t1900);
}

static void ergo_m_cogito_Label_set_text_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1901 = self; ergo_retain_val(__t1901);
  ErgoVal __t1902 = a0; ergo_retain_val(__t1902);
  cogito_label_set_align(__t1901, __t1902);
  ergo_release_val(__t1901);
  ergo_release_val(__t1902);
  ErgoVal __t1903 = EV_NULLV;
  ergo_release_val(__t1903);
}

static void ergo_m_cogito_Label_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1904 = self; ergo_retain_val(__t1904);
  ErgoVal __t1905 = a0; ergo_retain_val(__t1905);
  cogito_container_set_hexpand(__t1904, __t1905);
  ergo_release_val(__t1904);
  ergo_release_val(__t1905);
  ErgoVal __t1906 = EV_NULLV;
  ergo_release_val(__t1906);
}

static void ergo_m_cogito_Label_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1907 = self; ergo_retain_val(__t1907);
  ErgoVal __t1908 = a0; ergo_retain_val(__t1908);
  cogito_container_set_vexpand(__t1907, __t1908);
  ergo_release_val(__t1907);
  ergo_release_val(__t1908);
  ErgoVal __t1909 = EV_NULLV;
  ergo_release_val(__t1909);
}

static void ergo_m_cogito_Label_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1910 = self; ergo_retain_val(__t1910);
  ErgoVal __t1911 = a0; ergo_retain_val(__t1911);
  cogito_node_set_disabled(__t1910, __t1911);
  ergo_release_val(__t1910);
  ergo_release_val(__t1911);
  ErgoVal __t1912 = EV_NULLV;
  ergo_release_val(__t1912);
}

static void ergo_m_cogito_Label_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1913 = self; ergo_retain_val(__t1913);
  ErgoVal __t1914 = a0; ergo_retain_val(__t1914);
  cogito_node_set_id(__t1913, __t1914);
  ergo_release_val(__t1913);
  ergo_release_val(__t1914);
  ErgoVal __t1915 = EV_NULLV;
  ergo_release_val(__t1915);
}

static void ergo_m_cogito_Button_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t1916 = self; ergo_retain_val(__t1916);
  ErgoVal __t1917 = a0; ergo_retain_val(__t1917);
  ErgoVal __t1918 = a1; ergo_retain_val(__t1918);
  ErgoVal __t1919 = a2; ergo_retain_val(__t1919);
  ErgoVal __t1920 = a3; ergo_retain_val(__t1920);
  cogito_container_set_margins(__t1916, __t1917, __t1918, __t1919, __t1920);
  ergo_release_val(__t1916);
  ergo_release_val(__t1917);
  ergo_release_val(__t1918);
  ergo_release_val(__t1919);
  ergo_release_val(__t1920);
  ErgoVal __t1921 = EV_NULLV;
  ergo_release_val(__t1921);
}

static void ergo_m_cogito_Button_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t1922 = self; ergo_retain_val(__t1922);
  ErgoVal __t1923 = a0; ergo_retain_val(__t1923);
  ErgoVal __t1924 = a1; ergo_retain_val(__t1924);
  ErgoVal __t1925 = a2; ergo_retain_val(__t1925);
  ErgoVal __t1926 = a3; ergo_retain_val(__t1926);
  cogito_container_set_padding(__t1922, __t1923, __t1924, __t1925, __t1926);
  ergo_release_val(__t1922);
  ergo_release_val(__t1923);
  ergo_release_val(__t1924);
  ergo_release_val(__t1925);
  ergo_release_val(__t1926);
  ErgoVal __t1927 = EV_NULLV;
  ergo_release_val(__t1927);
}

static void ergo_m_cogito_Button_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1928 = self; ergo_retain_val(__t1928);
  ErgoVal __t1929 = a0; ergo_retain_val(__t1929);
  cogito_container_set_align(__t1928, __t1929);
  ergo_release_val(__t1928);
  ergo_release_val(__t1929);
  ErgoVal __t1930 = EV_NULLV;
  ergo_release_val(__t1930);
}

static void ergo_m_cogito_Button_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1931 = self; ergo_retain_val(__t1931);
  ErgoVal __t1932 = a0; ergo_retain_val(__t1932);
  cogito_container_set_halign(__t1931, __t1932);
  ergo_release_val(__t1931);
  ergo_release_val(__t1932);
  ErgoVal __t1933 = EV_NULLV;
  ergo_release_val(__t1933);
}

static void ergo_m_cogito_Button_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1934 = self; ergo_retain_val(__t1934);
  ErgoVal __t1935 = a0; ergo_retain_val(__t1935);
  cogito_container_set_valign(__t1934, __t1935);
  ergo_release_val(__t1934);
  ergo_release_val(__t1935);
  ErgoVal __t1936 = EV_NULLV;
  ergo_release_val(__t1936);
}

static void ergo_m_cogito_Button_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1937 = self; ergo_retain_val(__t1937);
  ErgoVal __t1938 = a0; ergo_retain_val(__t1938);
  cogito_container_set_hexpand(__t1937, __t1938);
  ergo_release_val(__t1937);
  ergo_release_val(__t1938);
  ErgoVal __t1939 = EV_NULLV;
  ergo_release_val(__t1939);
}

static void ergo_m_cogito_Button_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1940 = self; ergo_retain_val(__t1940);
  ErgoVal __t1941 = a0; ergo_retain_val(__t1941);
  cogito_container_set_vexpand(__t1940, __t1941);
  ergo_release_val(__t1940);
  ergo_release_val(__t1941);
  ErgoVal __t1942 = EV_NULLV;
  ergo_release_val(__t1942);
}

static void ergo_m_cogito_Button_align_begin(ErgoVal self) {
  ErgoVal __t1943 = self; ergo_retain_val(__t1943);
  ErgoVal __t1944 = EV_INT(0);
  cogito_container_set_align(__t1943, __t1944);
  ergo_release_val(__t1943);
  ergo_release_val(__t1944);
  ErgoVal __t1945 = EV_NULLV;
  ergo_release_val(__t1945);
}

static void ergo_m_cogito_Button_align_center(ErgoVal self) {
  ErgoVal __t1946 = self; ergo_retain_val(__t1946);
  ErgoVal __t1947 = EV_INT(1);
  cogito_container_set_align(__t1946, __t1947);
  ergo_release_val(__t1946);
  ergo_release_val(__t1947);
  ErgoVal __t1948 = EV_NULLV;
  ergo_release_val(__t1948);
}

static void ergo_m_cogito_Button_align_end(ErgoVal self) {
  ErgoVal __t1949 = self; ergo_retain_val(__t1949);
  ErgoVal __t1950 = EV_INT(2);
  cogito_container_set_align(__t1949, __t1950);
  ergo_release_val(__t1949);
  ergo_release_val(__t1950);
  ErgoVal __t1951 = EV_NULLV;
  ergo_release_val(__t1951);
}

static void ergo_m_cogito_Button_set_text(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1952 = self; ergo_retain_val(__t1952);
  ErgoVal __t1953 = a0; ergo_retain_val(__t1953);
  cogito_button_set_text(__t1952, __t1953);
  ergo_release_val(__t1952);
  ergo_release_val(__t1953);
  ErgoVal __t1954 = EV_NULLV;
  ergo_release_val(__t1954);
}

static void ergo_m_cogito_Button_on_click(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1955 = self; ergo_retain_val(__t1955);
  ErgoVal __t1956 = a0; ergo_retain_val(__t1956);
  cogito_button_on_click(__t1955, __t1956);
  ergo_release_val(__t1955);
  ergo_release_val(__t1956);
  ErgoVal __t1957 = EV_NULLV;
  ergo_release_val(__t1957);
}

static void ergo_m_cogito_Button_add_menu(ErgoVal self, ErgoVal a0, ErgoVal a1) {
  ErgoVal __t1958 = self; ergo_retain_val(__t1958);
  ErgoVal __t1959 = a0; ergo_retain_val(__t1959);
  ErgoVal __t1960 = a1; ergo_retain_val(__t1960);
  cogito_button_add_menu(__t1958, __t1959, __t1960);
  ergo_release_val(__t1958);
  ergo_release_val(__t1959);
  ergo_release_val(__t1960);
  ErgoVal __t1961 = EV_NULLV;
  ergo_release_val(__t1961);
}

static ErgoVal ergo_m_cogito_Button_window(ErgoVal self) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t1962 = self; ergo_retain_val(__t1962);
  ErgoVal __t1963 = cogito_node_window_val(__t1962);
  ergo_release_val(__t1962);
  ergo_move_into(&__ret, __t1963);
  return __ret;
  return __ret;
}

static void ergo_m_cogito_Button_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1964 = self; ergo_retain_val(__t1964);
  ErgoVal __t1965 = a0; ergo_retain_val(__t1965);
  cogito_node_set_disabled(__t1964, __t1965);
  ergo_release_val(__t1964);
  ergo_release_val(__t1965);
  ErgoVal __t1966 = EV_NULLV;
  ergo_release_val(__t1966);
}

static void ergo_m_cogito_Button_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1967 = self; ergo_retain_val(__t1967);
  ErgoVal __t1968 = a0; ergo_retain_val(__t1968);
  cogito_node_set_class(__t1967, __t1968);
  ergo_release_val(__t1967);
  ergo_release_val(__t1968);
  ErgoVal __t1969 = EV_NULLV;
  ergo_release_val(__t1969);
}

static void ergo_m_cogito_Button_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1970 = self; ergo_retain_val(__t1970);
  ErgoVal __t1971 = a0; ergo_retain_val(__t1971);
  cogito_node_set_id(__t1970, __t1971);
  ergo_release_val(__t1970);
  ergo_release_val(__t1971);
  ErgoVal __t1972 = EV_NULLV;
  ergo_release_val(__t1972);
}

static void ergo_m_cogito_Checkbox_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t1973 = self; ergo_retain_val(__t1973);
  ErgoVal __t1974 = a0; ergo_retain_val(__t1974);
  ErgoVal __t1975 = a1; ergo_retain_val(__t1975);
  ErgoVal __t1976 = a2; ergo_retain_val(__t1976);
  ErgoVal __t1977 = a3; ergo_retain_val(__t1977);
  cogito_container_set_margins(__t1973, __t1974, __t1975, __t1976, __t1977);
  ergo_release_val(__t1973);
  ergo_release_val(__t1974);
  ergo_release_val(__t1975);
  ergo_release_val(__t1976);
  ergo_release_val(__t1977);
  ErgoVal __t1978 = EV_NULLV;
  ergo_release_val(__t1978);
}

static void ergo_m_cogito_Checkbox_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t1979 = self; ergo_retain_val(__t1979);
  ErgoVal __t1980 = a0; ergo_retain_val(__t1980);
  ErgoVal __t1981 = a1; ergo_retain_val(__t1981);
  ErgoVal __t1982 = a2; ergo_retain_val(__t1982);
  ErgoVal __t1983 = a3; ergo_retain_val(__t1983);
  cogito_container_set_padding(__t1979, __t1980, __t1981, __t1982, __t1983);
  ergo_release_val(__t1979);
  ergo_release_val(__t1980);
  ergo_release_val(__t1981);
  ergo_release_val(__t1982);
  ergo_release_val(__t1983);
  ErgoVal __t1984 = EV_NULLV;
  ergo_release_val(__t1984);
}

static void ergo_m_cogito_Checkbox_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1985 = self; ergo_retain_val(__t1985);
  ErgoVal __t1986 = a0; ergo_retain_val(__t1986);
  cogito_container_set_align(__t1985, __t1986);
  ergo_release_val(__t1985);
  ergo_release_val(__t1986);
  ErgoVal __t1987 = EV_NULLV;
  ergo_release_val(__t1987);
}

static void ergo_m_cogito_Checkbox_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1988 = self; ergo_retain_val(__t1988);
  ErgoVal __t1989 = a0; ergo_retain_val(__t1989);
  cogito_container_set_halign(__t1988, __t1989);
  ergo_release_val(__t1988);
  ergo_release_val(__t1989);
  ErgoVal __t1990 = EV_NULLV;
  ergo_release_val(__t1990);
}

static void ergo_m_cogito_Checkbox_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t1991 = self; ergo_retain_val(__t1991);
  ErgoVal __t1992 = a0; ergo_retain_val(__t1992);
  cogito_container_set_valign(__t1991, __t1992);
  ergo_release_val(__t1991);
  ergo_release_val(__t1992);
  ErgoVal __t1993 = EV_NULLV;
  ergo_release_val(__t1993);
}

static void ergo_m_cogito_Checkbox_align_begin(ErgoVal self) {
  ErgoVal __t1994 = self; ergo_retain_val(__t1994);
  ErgoVal __t1995 = EV_INT(0);
  cogito_container_set_align(__t1994, __t1995);
  ergo_release_val(__t1994);
  ergo_release_val(__t1995);
  ErgoVal __t1996 = EV_NULLV;
  ergo_release_val(__t1996);
}

static void ergo_m_cogito_Checkbox_align_center(ErgoVal self) {
  ErgoVal __t1997 = self; ergo_retain_val(__t1997);
  ErgoVal __t1998 = EV_INT(1);
  cogito_container_set_align(__t1997, __t1998);
  ergo_release_val(__t1997);
  ergo_release_val(__t1998);
  ErgoVal __t1999 = EV_NULLV;
  ergo_release_val(__t1999);
}

static void ergo_m_cogito_Checkbox_align_end(ErgoVal self) {
  ErgoVal __t2000 = self; ergo_retain_val(__t2000);
  ErgoVal __t2001 = EV_INT(2);
  cogito_container_set_align(__t2000, __t2001);
  ergo_release_val(__t2000);
  ergo_release_val(__t2001);
  ErgoVal __t2002 = EV_NULLV;
  ergo_release_val(__t2002);
}

static void ergo_m_cogito_Checkbox_set_checked(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2003 = self; ergo_retain_val(__t2003);
  ErgoVal __t2004 = a0; ergo_retain_val(__t2004);
  cogito_checkbox_set_checked(__t2003, __t2004);
  ergo_release_val(__t2003);
  ergo_release_val(__t2004);
  ErgoVal __t2005 = EV_NULLV;
  ergo_release_val(__t2005);
}

static ErgoVal ergo_m_cogito_Checkbox_checked(ErgoVal self) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t2006 = self; ergo_retain_val(__t2006);
  ErgoVal __t2007 = cogito_checkbox_get_checked(__t2006);
  ergo_release_val(__t2006);
  ergo_move_into(&__ret, __t2007);
  return __ret;
  return __ret;
}

static void ergo_m_cogito_Checkbox_on_change(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2008 = self; ergo_retain_val(__t2008);
  ErgoVal __t2009 = a0; ergo_retain_val(__t2009);
  cogito_checkbox_on_change(__t2008, __t2009);
  ergo_release_val(__t2008);
  ergo_release_val(__t2009);
  ErgoVal __t2010 = EV_NULLV;
  ergo_release_val(__t2010);
}

static ErgoVal ergo_m_cogito_Checkbox_window(ErgoVal self) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t2011 = self; ergo_retain_val(__t2011);
  ErgoVal __t2012 = cogito_node_window_val(__t2011);
  ergo_release_val(__t2011);
  ergo_move_into(&__ret, __t2012);
  return __ret;
  return __ret;
}

static void ergo_m_cogito_Checkbox_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2013 = self; ergo_retain_val(__t2013);
  ErgoVal __t2014 = a0; ergo_retain_val(__t2014);
  cogito_container_set_hexpand(__t2013, __t2014);
  ergo_release_val(__t2013);
  ergo_release_val(__t2014);
  ErgoVal __t2015 = EV_NULLV;
  ergo_release_val(__t2015);
}

static void ergo_m_cogito_Checkbox_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2016 = self; ergo_retain_val(__t2016);
  ErgoVal __t2017 = a0; ergo_retain_val(__t2017);
  cogito_container_set_vexpand(__t2016, __t2017);
  ergo_release_val(__t2016);
  ergo_release_val(__t2017);
  ErgoVal __t2018 = EV_NULLV;
  ergo_release_val(__t2018);
}

static void ergo_m_cogito_Checkbox_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2019 = self; ergo_retain_val(__t2019);
  ErgoVal __t2020 = a0; ergo_retain_val(__t2020);
  cogito_node_set_disabled(__t2019, __t2020);
  ergo_release_val(__t2019);
  ergo_release_val(__t2020);
  ErgoVal __t2021 = EV_NULLV;
  ergo_release_val(__t2021);
}

static void ergo_m_cogito_Checkbox_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2022 = self; ergo_retain_val(__t2022);
  ErgoVal __t2023 = a0; ergo_retain_val(__t2023);
  cogito_node_set_class(__t2022, __t2023);
  ergo_release_val(__t2022);
  ergo_release_val(__t2023);
  ErgoVal __t2024 = EV_NULLV;
  ergo_release_val(__t2024);
}

static void ergo_m_cogito_Checkbox_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2025 = self; ergo_retain_val(__t2025);
  ErgoVal __t2026 = a0; ergo_retain_val(__t2026);
  cogito_node_set_id(__t2025, __t2026);
  ergo_release_val(__t2025);
  ergo_release_val(__t2026);
  ErgoVal __t2027 = EV_NULLV;
  ergo_release_val(__t2027);
}

static void ergo_m_cogito_Switch_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t2028 = self; ergo_retain_val(__t2028);
  ErgoVal __t2029 = a0; ergo_retain_val(__t2029);
  ErgoVal __t2030 = a1; ergo_retain_val(__t2030);
  ErgoVal __t2031 = a2; ergo_retain_val(__t2031);
  ErgoVal __t2032 = a3; ergo_retain_val(__t2032);
  cogito_container_set_margins(__t2028, __t2029, __t2030, __t2031, __t2032);
  ergo_release_val(__t2028);
  ergo_release_val(__t2029);
  ergo_release_val(__t2030);
  ergo_release_val(__t2031);
  ergo_release_val(__t2032);
  ErgoVal __t2033 = EV_NULLV;
  ergo_release_val(__t2033);
}

static void ergo_m_cogito_Switch_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t2034 = self; ergo_retain_val(__t2034);
  ErgoVal __t2035 = a0; ergo_retain_val(__t2035);
  ErgoVal __t2036 = a1; ergo_retain_val(__t2036);
  ErgoVal __t2037 = a2; ergo_retain_val(__t2037);
  ErgoVal __t2038 = a3; ergo_retain_val(__t2038);
  cogito_container_set_padding(__t2034, __t2035, __t2036, __t2037, __t2038);
  ergo_release_val(__t2034);
  ergo_release_val(__t2035);
  ergo_release_val(__t2036);
  ergo_release_val(__t2037);
  ergo_release_val(__t2038);
  ErgoVal __t2039 = EV_NULLV;
  ergo_release_val(__t2039);
}

static void ergo_m_cogito_Switch_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2040 = self; ergo_retain_val(__t2040);
  ErgoVal __t2041 = a0; ergo_retain_val(__t2041);
  cogito_container_set_align(__t2040, __t2041);
  ergo_release_val(__t2040);
  ergo_release_val(__t2041);
  ErgoVal __t2042 = EV_NULLV;
  ergo_release_val(__t2042);
}

static void ergo_m_cogito_Switch_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2043 = self; ergo_retain_val(__t2043);
  ErgoVal __t2044 = a0; ergo_retain_val(__t2044);
  cogito_container_set_halign(__t2043, __t2044);
  ergo_release_val(__t2043);
  ergo_release_val(__t2044);
  ErgoVal __t2045 = EV_NULLV;
  ergo_release_val(__t2045);
}

static void ergo_m_cogito_Switch_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2046 = self; ergo_retain_val(__t2046);
  ErgoVal __t2047 = a0; ergo_retain_val(__t2047);
  cogito_container_set_valign(__t2046, __t2047);
  ergo_release_val(__t2046);
  ergo_release_val(__t2047);
  ErgoVal __t2048 = EV_NULLV;
  ergo_release_val(__t2048);
}

static void ergo_m_cogito_Switch_align_begin(ErgoVal self) {
  ErgoVal __t2049 = self; ergo_retain_val(__t2049);
  ErgoVal __t2050 = EV_INT(0);
  cogito_container_set_align(__t2049, __t2050);
  ergo_release_val(__t2049);
  ergo_release_val(__t2050);
  ErgoVal __t2051 = EV_NULLV;
  ergo_release_val(__t2051);
}

static void ergo_m_cogito_Switch_align_center(ErgoVal self) {
  ErgoVal __t2052 = self; ergo_retain_val(__t2052);
  ErgoVal __t2053 = EV_INT(1);
  cogito_container_set_align(__t2052, __t2053);
  ergo_release_val(__t2052);
  ergo_release_val(__t2053);
  ErgoVal __t2054 = EV_NULLV;
  ergo_release_val(__t2054);
}

static void ergo_m_cogito_Switch_align_end(ErgoVal self) {
  ErgoVal __t2055 = self; ergo_retain_val(__t2055);
  ErgoVal __t2056 = EV_INT(2);
  cogito_container_set_align(__t2055, __t2056);
  ergo_release_val(__t2055);
  ergo_release_val(__t2056);
  ErgoVal __t2057 = EV_NULLV;
  ergo_release_val(__t2057);
}

static void ergo_m_cogito_Switch_set_checked(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2058 = self; ergo_retain_val(__t2058);
  ErgoVal __t2059 = a0; ergo_retain_val(__t2059);
  cogito_switch_set_checked(__t2058, __t2059);
  ergo_release_val(__t2058);
  ergo_release_val(__t2059);
  ErgoVal __t2060 = EV_NULLV;
  ergo_release_val(__t2060);
}

static ErgoVal ergo_m_cogito_Switch_checked(ErgoVal self) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t2061 = self; ergo_retain_val(__t2061);
  ErgoVal __t2062 = cogito_switch_get_checked(__t2061);
  ergo_release_val(__t2061);
  ergo_move_into(&__ret, __t2062);
  return __ret;
  return __ret;
}

static void ergo_m_cogito_Switch_on_change(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2063 = self; ergo_retain_val(__t2063);
  ErgoVal __t2064 = a0; ergo_retain_val(__t2064);
  cogito_switch_on_change(__t2063, __t2064);
  ergo_release_val(__t2063);
  ergo_release_val(__t2064);
  ErgoVal __t2065 = EV_NULLV;
  ergo_release_val(__t2065);
}

static ErgoVal ergo_m_cogito_Switch_window(ErgoVal self) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t2066 = self; ergo_retain_val(__t2066);
  ErgoVal __t2067 = cogito_node_window_val(__t2066);
  ergo_release_val(__t2066);
  ergo_move_into(&__ret, __t2067);
  return __ret;
  return __ret;
}

static void ergo_m_cogito_Switch_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2068 = self; ergo_retain_val(__t2068);
  ErgoVal __t2069 = a0; ergo_retain_val(__t2069);
  cogito_container_set_hexpand(__t2068, __t2069);
  ergo_release_val(__t2068);
  ergo_release_val(__t2069);
  ErgoVal __t2070 = EV_NULLV;
  ergo_release_val(__t2070);
}

static void ergo_m_cogito_Switch_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2071 = self; ergo_retain_val(__t2071);
  ErgoVal __t2072 = a0; ergo_retain_val(__t2072);
  cogito_container_set_vexpand(__t2071, __t2072);
  ergo_release_val(__t2071);
  ergo_release_val(__t2072);
  ErgoVal __t2073 = EV_NULLV;
  ergo_release_val(__t2073);
}

static void ergo_m_cogito_Switch_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2074 = self; ergo_retain_val(__t2074);
  ErgoVal __t2075 = a0; ergo_retain_val(__t2075);
  cogito_node_set_disabled(__t2074, __t2075);
  ergo_release_val(__t2074);
  ergo_release_val(__t2075);
  ErgoVal __t2076 = EV_NULLV;
  ergo_release_val(__t2076);
}

static void ergo_m_cogito_Switch_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2077 = self; ergo_retain_val(__t2077);
  ErgoVal __t2078 = a0; ergo_retain_val(__t2078);
  cogito_node_set_class(__t2077, __t2078);
  ergo_release_val(__t2077);
  ergo_release_val(__t2078);
  ErgoVal __t2079 = EV_NULLV;
  ergo_release_val(__t2079);
}

static void ergo_m_cogito_Switch_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2080 = self; ergo_retain_val(__t2080);
  ErgoVal __t2081 = a0; ergo_retain_val(__t2081);
  cogito_node_set_id(__t2080, __t2081);
  ergo_release_val(__t2080);
  ergo_release_val(__t2081);
  ErgoVal __t2082 = EV_NULLV;
  ergo_release_val(__t2082);
}

static void ergo_m_cogito_SearchField_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t2083 = self; ergo_retain_val(__t2083);
  ErgoVal __t2084 = a0; ergo_retain_val(__t2084);
  ErgoVal __t2085 = a1; ergo_retain_val(__t2085);
  ErgoVal __t2086 = a2; ergo_retain_val(__t2086);
  ErgoVal __t2087 = a3; ergo_retain_val(__t2087);
  cogito_container_set_margins(__t2083, __t2084, __t2085, __t2086, __t2087);
  ergo_release_val(__t2083);
  ergo_release_val(__t2084);
  ergo_release_val(__t2085);
  ergo_release_val(__t2086);
  ergo_release_val(__t2087);
  ErgoVal __t2088 = EV_NULLV;
  ergo_release_val(__t2088);
}

static void ergo_m_cogito_SearchField_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t2089 = self; ergo_retain_val(__t2089);
  ErgoVal __t2090 = a0; ergo_retain_val(__t2090);
  ErgoVal __t2091 = a1; ergo_retain_val(__t2091);
  ErgoVal __t2092 = a2; ergo_retain_val(__t2092);
  ErgoVal __t2093 = a3; ergo_retain_val(__t2093);
  cogito_container_set_padding(__t2089, __t2090, __t2091, __t2092, __t2093);
  ergo_release_val(__t2089);
  ergo_release_val(__t2090);
  ergo_release_val(__t2091);
  ergo_release_val(__t2092);
  ergo_release_val(__t2093);
  ErgoVal __t2094 = EV_NULLV;
  ergo_release_val(__t2094);
}

static void ergo_m_cogito_SearchField_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2095 = self; ergo_retain_val(__t2095);
  ErgoVal __t2096 = a0; ergo_retain_val(__t2096);
  cogito_container_set_align(__t2095, __t2096);
  ergo_release_val(__t2095);
  ergo_release_val(__t2096);
  ErgoVal __t2097 = EV_NULLV;
  ergo_release_val(__t2097);
}

static void ergo_m_cogito_SearchField_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2098 = self; ergo_retain_val(__t2098);
  ErgoVal __t2099 = a0; ergo_retain_val(__t2099);
  cogito_container_set_halign(__t2098, __t2099);
  ergo_release_val(__t2098);
  ergo_release_val(__t2099);
  ErgoVal __t2100 = EV_NULLV;
  ergo_release_val(__t2100);
}

static void ergo_m_cogito_SearchField_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2101 = self; ergo_retain_val(__t2101);
  ErgoVal __t2102 = a0; ergo_retain_val(__t2102);
  cogito_container_set_valign(__t2101, __t2102);
  ergo_release_val(__t2101);
  ergo_release_val(__t2102);
  ErgoVal __t2103 = EV_NULLV;
  ergo_release_val(__t2103);
}

static void ergo_m_cogito_SearchField_set_text(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2104 = self; ergo_retain_val(__t2104);
  ErgoVal __t2105 = a0; ergo_retain_val(__t2105);
  cogito_searchfield_set_text(__t2104, __t2105);
  ergo_release_val(__t2104);
  ergo_release_val(__t2105);
  ErgoVal __t2106 = EV_NULLV;
  ergo_release_val(__t2106);
}

static ErgoVal ergo_m_cogito_SearchField_text(ErgoVal self) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t2107 = self; ergo_retain_val(__t2107);
  ErgoVal __t2108 = cogito_searchfield_get_text(__t2107);
  ergo_release_val(__t2107);
  ergo_move_into(&__ret, __t2108);
  return __ret;
  return __ret;
}

static void ergo_m_cogito_SearchField_on_change(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2109 = self; ergo_retain_val(__t2109);
  ErgoVal __t2110 = a0; ergo_retain_val(__t2110);
  cogito_searchfield_on_change(__t2109, __t2110);
  ergo_release_val(__t2109);
  ergo_release_val(__t2110);
  ErgoVal __t2111 = EV_NULLV;
  ergo_release_val(__t2111);
}

static void ergo_m_cogito_SearchField_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2112 = self; ergo_retain_val(__t2112);
  ErgoVal __t2113 = a0; ergo_retain_val(__t2113);
  cogito_container_set_hexpand(__t2112, __t2113);
  ergo_release_val(__t2112);
  ergo_release_val(__t2113);
  ErgoVal __t2114 = EV_NULLV;
  ergo_release_val(__t2114);
}

static void ergo_m_cogito_SearchField_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2115 = self; ergo_retain_val(__t2115);
  ErgoVal __t2116 = a0; ergo_retain_val(__t2116);
  cogito_container_set_vexpand(__t2115, __t2116);
  ergo_release_val(__t2115);
  ergo_release_val(__t2116);
  ErgoVal __t2117 = EV_NULLV;
  ergo_release_val(__t2117);
}

static void ergo_m_cogito_SearchField_set_editable(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2118 = self; ergo_retain_val(__t2118);
  ErgoVal __t2119 = a0; ergo_retain_val(__t2119);
  cogito_node_set_editable(__t2118, __t2119);
  ergo_release_val(__t2118);
  ergo_release_val(__t2119);
  ErgoVal __t2120 = EV_NULLV;
  ergo_release_val(__t2120);
}

static ErgoVal ergo_m_cogito_SearchField_editable(ErgoVal self) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t2121 = self; ergo_retain_val(__t2121);
  ErgoVal __t2122 = cogito_node_get_editable(__t2121);
  ergo_release_val(__t2121);
  ergo_move_into(&__ret, __t2122);
  return __ret;
  return __ret;
}

static void ergo_m_cogito_SearchField_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2123 = self; ergo_retain_val(__t2123);
  ErgoVal __t2124 = a0; ergo_retain_val(__t2124);
  cogito_node_set_class(__t2123, __t2124);
  ergo_release_val(__t2123);
  ergo_release_val(__t2124);
  ErgoVal __t2125 = EV_NULLV;
  ergo_release_val(__t2125);
}

static void ergo_m_cogito_SearchField_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2126 = self; ergo_retain_val(__t2126);
  ErgoVal __t2127 = a0; ergo_retain_val(__t2127);
  cogito_node_set_id(__t2126, __t2127);
  ergo_release_val(__t2126);
  ergo_release_val(__t2127);
  ErgoVal __t2128 = EV_NULLV;
  ergo_release_val(__t2128);
}

static void ergo_m_cogito_TextField_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t2129 = self; ergo_retain_val(__t2129);
  ErgoVal __t2130 = a0; ergo_retain_val(__t2130);
  ErgoVal __t2131 = a1; ergo_retain_val(__t2131);
  ErgoVal __t2132 = a2; ergo_retain_val(__t2132);
  ErgoVal __t2133 = a3; ergo_retain_val(__t2133);
  cogito_container_set_margins(__t2129, __t2130, __t2131, __t2132, __t2133);
  ergo_release_val(__t2129);
  ergo_release_val(__t2130);
  ergo_release_val(__t2131);
  ergo_release_val(__t2132);
  ergo_release_val(__t2133);
  ErgoVal __t2134 = EV_NULLV;
  ergo_release_val(__t2134);
}

static void ergo_m_cogito_TextField_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t2135 = self; ergo_retain_val(__t2135);
  ErgoVal __t2136 = a0; ergo_retain_val(__t2136);
  ErgoVal __t2137 = a1; ergo_retain_val(__t2137);
  ErgoVal __t2138 = a2; ergo_retain_val(__t2138);
  ErgoVal __t2139 = a3; ergo_retain_val(__t2139);
  cogito_container_set_padding(__t2135, __t2136, __t2137, __t2138, __t2139);
  ergo_release_val(__t2135);
  ergo_release_val(__t2136);
  ergo_release_val(__t2137);
  ergo_release_val(__t2138);
  ergo_release_val(__t2139);
  ErgoVal __t2140 = EV_NULLV;
  ergo_release_val(__t2140);
}

static void ergo_m_cogito_TextField_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2141 = self; ergo_retain_val(__t2141);
  ErgoVal __t2142 = a0; ergo_retain_val(__t2142);
  cogito_container_set_align(__t2141, __t2142);
  ergo_release_val(__t2141);
  ergo_release_val(__t2142);
  ErgoVal __t2143 = EV_NULLV;
  ergo_release_val(__t2143);
}

static void ergo_m_cogito_TextField_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2144 = self; ergo_retain_val(__t2144);
  ErgoVal __t2145 = a0; ergo_retain_val(__t2145);
  cogito_container_set_halign(__t2144, __t2145);
  ergo_release_val(__t2144);
  ergo_release_val(__t2145);
  ErgoVal __t2146 = EV_NULLV;
  ergo_release_val(__t2146);
}

static void ergo_m_cogito_TextField_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2147 = self; ergo_retain_val(__t2147);
  ErgoVal __t2148 = a0; ergo_retain_val(__t2148);
  cogito_container_set_valign(__t2147, __t2148);
  ergo_release_val(__t2147);
  ergo_release_val(__t2148);
  ErgoVal __t2149 = EV_NULLV;
  ergo_release_val(__t2149);
}

static void ergo_m_cogito_TextField_align_begin(ErgoVal self) {
  ErgoVal __t2150 = self; ergo_retain_val(__t2150);
  ErgoVal __t2151 = EV_INT(0);
  cogito_container_set_align(__t2150, __t2151);
  ergo_release_val(__t2150);
  ergo_release_val(__t2151);
  ErgoVal __t2152 = EV_NULLV;
  ergo_release_val(__t2152);
}

static void ergo_m_cogito_TextField_align_center(ErgoVal self) {
  ErgoVal __t2153 = self; ergo_retain_val(__t2153);
  ErgoVal __t2154 = EV_INT(1);
  cogito_container_set_align(__t2153, __t2154);
  ergo_release_val(__t2153);
  ergo_release_val(__t2154);
  ErgoVal __t2155 = EV_NULLV;
  ergo_release_val(__t2155);
}

static void ergo_m_cogito_TextField_align_end(ErgoVal self) {
  ErgoVal __t2156 = self; ergo_retain_val(__t2156);
  ErgoVal __t2157 = EV_INT(2);
  cogito_container_set_align(__t2156, __t2157);
  ergo_release_val(__t2156);
  ergo_release_val(__t2157);
  ErgoVal __t2158 = EV_NULLV;
  ergo_release_val(__t2158);
}

static void ergo_m_cogito_TextField_set_text(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2159 = self; ergo_retain_val(__t2159);
  ErgoVal __t2160 = a0; ergo_retain_val(__t2160);
  cogito_textfield_set_text(__t2159, __t2160);
  ergo_release_val(__t2159);
  ergo_release_val(__t2160);
  ErgoVal __t2161 = EV_NULLV;
  ergo_release_val(__t2161);
}

static ErgoVal ergo_m_cogito_TextField_text(ErgoVal self) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t2162 = self; ergo_retain_val(__t2162);
  ErgoVal __t2163 = cogito_textfield_get_text(__t2162);
  ergo_release_val(__t2162);
  ergo_move_into(&__ret, __t2163);
  return __ret;
  return __ret;
}

static void ergo_m_cogito_TextField_on_change(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2164 = self; ergo_retain_val(__t2164);
  ErgoVal __t2165 = a0; ergo_retain_val(__t2165);
  cogito_textfield_on_change(__t2164, __t2165);
  ergo_release_val(__t2164);
  ergo_release_val(__t2165);
  ErgoVal __t2166 = EV_NULLV;
  ergo_release_val(__t2166);
}

static void ergo_m_cogito_TextField_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2167 = self; ergo_retain_val(__t2167);
  ErgoVal __t2168 = a0; ergo_retain_val(__t2168);
  cogito_container_set_hexpand(__t2167, __t2168);
  ergo_release_val(__t2167);
  ergo_release_val(__t2168);
  ErgoVal __t2169 = EV_NULLV;
  ergo_release_val(__t2169);
}

static void ergo_m_cogito_TextField_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2170 = self; ergo_retain_val(__t2170);
  ErgoVal __t2171 = a0; ergo_retain_val(__t2171);
  cogito_container_set_vexpand(__t2170, __t2171);
  ergo_release_val(__t2170);
  ergo_release_val(__t2171);
  ErgoVal __t2172 = EV_NULLV;
  ergo_release_val(__t2172);
}

static void ergo_m_cogito_TextField_set_editable(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2173 = self; ergo_retain_val(__t2173);
  ErgoVal __t2174 = a0; ergo_retain_val(__t2174);
  cogito_node_set_editable(__t2173, __t2174);
  ergo_release_val(__t2173);
  ergo_release_val(__t2174);
  ErgoVal __t2175 = EV_NULLV;
  ergo_release_val(__t2175);
}

static ErgoVal ergo_m_cogito_TextField_editable(ErgoVal self) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t2176 = self; ergo_retain_val(__t2176);
  ErgoVal __t2177 = cogito_node_get_editable(__t2176);
  ergo_release_val(__t2176);
  ergo_move_into(&__ret, __t2177);
  return __ret;
  return __ret;
}

static void ergo_m_cogito_TextField_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2178 = self; ergo_retain_val(__t2178);
  ErgoVal __t2179 = a0; ergo_retain_val(__t2179);
  cogito_node_set_disabled(__t2178, __t2179);
  ergo_release_val(__t2178);
  ergo_release_val(__t2179);
  ErgoVal __t2180 = EV_NULLV;
  ergo_release_val(__t2180);
}

static void ergo_m_cogito_TextField_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2181 = self; ergo_retain_val(__t2181);
  ErgoVal __t2182 = a0; ergo_retain_val(__t2182);
  cogito_node_set_class(__t2181, __t2182);
  ergo_release_val(__t2181);
  ergo_release_val(__t2182);
  ErgoVal __t2183 = EV_NULLV;
  ergo_release_val(__t2183);
}

static void ergo_m_cogito_TextField_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2184 = self; ergo_retain_val(__t2184);
  ErgoVal __t2185 = a0; ergo_retain_val(__t2185);
  cogito_node_set_id(__t2184, __t2185);
  ergo_release_val(__t2184);
  ergo_release_val(__t2185);
  ErgoVal __t2186 = EV_NULLV;
  ergo_release_val(__t2186);
}

static void ergo_m_cogito_TextView_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t2187 = self; ergo_retain_val(__t2187);
  ErgoVal __t2188 = a0; ergo_retain_val(__t2188);
  ErgoVal __t2189 = a1; ergo_retain_val(__t2189);
  ErgoVal __t2190 = a2; ergo_retain_val(__t2190);
  ErgoVal __t2191 = a3; ergo_retain_val(__t2191);
  cogito_container_set_margins(__t2187, __t2188, __t2189, __t2190, __t2191);
  ergo_release_val(__t2187);
  ergo_release_val(__t2188);
  ergo_release_val(__t2189);
  ergo_release_val(__t2190);
  ergo_release_val(__t2191);
  ErgoVal __t2192 = EV_NULLV;
  ergo_release_val(__t2192);
}

static void ergo_m_cogito_TextView_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t2193 = self; ergo_retain_val(__t2193);
  ErgoVal __t2194 = a0; ergo_retain_val(__t2194);
  ErgoVal __t2195 = a1; ergo_retain_val(__t2195);
  ErgoVal __t2196 = a2; ergo_retain_val(__t2196);
  ErgoVal __t2197 = a3; ergo_retain_val(__t2197);
  cogito_container_set_padding(__t2193, __t2194, __t2195, __t2196, __t2197);
  ergo_release_val(__t2193);
  ergo_release_val(__t2194);
  ergo_release_val(__t2195);
  ergo_release_val(__t2196);
  ergo_release_val(__t2197);
  ErgoVal __t2198 = EV_NULLV;
  ergo_release_val(__t2198);
}

static void ergo_m_cogito_TextView_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2199 = self; ergo_retain_val(__t2199);
  ErgoVal __t2200 = a0; ergo_retain_val(__t2200);
  cogito_container_set_align(__t2199, __t2200);
  ergo_release_val(__t2199);
  ergo_release_val(__t2200);
  ErgoVal __t2201 = EV_NULLV;
  ergo_release_val(__t2201);
}

static void ergo_m_cogito_TextView_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2202 = self; ergo_retain_val(__t2202);
  ErgoVal __t2203 = a0; ergo_retain_val(__t2203);
  cogito_container_set_halign(__t2202, __t2203);
  ergo_release_val(__t2202);
  ergo_release_val(__t2203);
  ErgoVal __t2204 = EV_NULLV;
  ergo_release_val(__t2204);
}

static void ergo_m_cogito_TextView_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2205 = self; ergo_retain_val(__t2205);
  ErgoVal __t2206 = a0; ergo_retain_val(__t2206);
  cogito_container_set_valign(__t2205, __t2206);
  ergo_release_val(__t2205);
  ergo_release_val(__t2206);
  ErgoVal __t2207 = EV_NULLV;
  ergo_release_val(__t2207);
}

static void ergo_m_cogito_TextView_align_begin(ErgoVal self) {
  ErgoVal __t2208 = self; ergo_retain_val(__t2208);
  ErgoVal __t2209 = EV_INT(0);
  cogito_container_set_align(__t2208, __t2209);
  ergo_release_val(__t2208);
  ergo_release_val(__t2209);
  ErgoVal __t2210 = EV_NULLV;
  ergo_release_val(__t2210);
}

static void ergo_m_cogito_TextView_align_center(ErgoVal self) {
  ErgoVal __t2211 = self; ergo_retain_val(__t2211);
  ErgoVal __t2212 = EV_INT(1);
  cogito_container_set_align(__t2211, __t2212);
  ergo_release_val(__t2211);
  ergo_release_val(__t2212);
  ErgoVal __t2213 = EV_NULLV;
  ergo_release_val(__t2213);
}

static void ergo_m_cogito_TextView_align_end(ErgoVal self) {
  ErgoVal __t2214 = self; ergo_retain_val(__t2214);
  ErgoVal __t2215 = EV_INT(2);
  cogito_container_set_align(__t2214, __t2215);
  ergo_release_val(__t2214);
  ergo_release_val(__t2215);
  ErgoVal __t2216 = EV_NULLV;
  ergo_release_val(__t2216);
}

static void ergo_m_cogito_TextView_set_text(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2217 = self; ergo_retain_val(__t2217);
  ErgoVal __t2218 = a0; ergo_retain_val(__t2218);
  cogito_textview_set_text(__t2217, __t2218);
  ergo_release_val(__t2217);
  ergo_release_val(__t2218);
  ErgoVal __t2219 = EV_NULLV;
  ergo_release_val(__t2219);
}

static ErgoVal ergo_m_cogito_TextView_text(ErgoVal self) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t2220 = self; ergo_retain_val(__t2220);
  ErgoVal __t2221 = cogito_textview_get_text(__t2220);
  ergo_release_val(__t2220);
  ergo_move_into(&__ret, __t2221);
  return __ret;
  return __ret;
}

static void ergo_m_cogito_TextView_on_change(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2222 = self; ergo_retain_val(__t2222);
  ErgoVal __t2223 = a0; ergo_retain_val(__t2223);
  cogito_textview_on_change(__t2222, __t2223);
  ergo_release_val(__t2222);
  ergo_release_val(__t2223);
  ErgoVal __t2224 = EV_NULLV;
  ergo_release_val(__t2224);
}

static void ergo_m_cogito_TextView_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2225 = self; ergo_retain_val(__t2225);
  ErgoVal __t2226 = a0; ergo_retain_val(__t2226);
  cogito_container_set_hexpand(__t2225, __t2226);
  ergo_release_val(__t2225);
  ergo_release_val(__t2226);
  ErgoVal __t2227 = EV_NULLV;
  ergo_release_val(__t2227);
}

static void ergo_m_cogito_TextView_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2228 = self; ergo_retain_val(__t2228);
  ErgoVal __t2229 = a0; ergo_retain_val(__t2229);
  cogito_container_set_vexpand(__t2228, __t2229);
  ergo_release_val(__t2228);
  ergo_release_val(__t2229);
  ErgoVal __t2230 = EV_NULLV;
  ergo_release_val(__t2230);
}

static void ergo_m_cogito_TextView_set_editable(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2231 = self; ergo_retain_val(__t2231);
  ErgoVal __t2232 = a0; ergo_retain_val(__t2232);
  cogito_node_set_editable(__t2231, __t2232);
  ergo_release_val(__t2231);
  ergo_release_val(__t2232);
  ErgoVal __t2233 = EV_NULLV;
  ergo_release_val(__t2233);
}

static ErgoVal ergo_m_cogito_TextView_editable(ErgoVal self) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t2234 = self; ergo_retain_val(__t2234);
  ErgoVal __t2235 = cogito_node_get_editable(__t2234);
  ergo_release_val(__t2234);
  ergo_move_into(&__ret, __t2235);
  return __ret;
  return __ret;
}

static void ergo_m_cogito_TextView_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2236 = self; ergo_retain_val(__t2236);
  ErgoVal __t2237 = a0; ergo_retain_val(__t2237);
  cogito_node_set_disabled(__t2236, __t2237);
  ergo_release_val(__t2236);
  ergo_release_val(__t2237);
  ErgoVal __t2238 = EV_NULLV;
  ergo_release_val(__t2238);
}

static void ergo_m_cogito_TextView_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2239 = self; ergo_retain_val(__t2239);
  ErgoVal __t2240 = a0; ergo_retain_val(__t2240);
  cogito_node_set_class(__t2239, __t2240);
  ergo_release_val(__t2239);
  ergo_release_val(__t2240);
  ErgoVal __t2241 = EV_NULLV;
  ergo_release_val(__t2241);
}

static void ergo_m_cogito_TextView_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2242 = self; ergo_retain_val(__t2242);
  ErgoVal __t2243 = a0; ergo_retain_val(__t2243);
  cogito_node_set_id(__t2242, __t2243);
  ergo_release_val(__t2242);
  ergo_release_val(__t2243);
  ErgoVal __t2244 = EV_NULLV;
  ergo_release_val(__t2244);
}

static void ergo_m_cogito_DatePicker_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t2245 = self; ergo_retain_val(__t2245);
  ErgoVal __t2246 = a0; ergo_retain_val(__t2246);
  ErgoVal __t2247 = a1; ergo_retain_val(__t2247);
  ErgoVal __t2248 = a2; ergo_retain_val(__t2248);
  ErgoVal __t2249 = a3; ergo_retain_val(__t2249);
  cogito_container_set_margins(__t2245, __t2246, __t2247, __t2248, __t2249);
  ergo_release_val(__t2245);
  ergo_release_val(__t2246);
  ergo_release_val(__t2247);
  ergo_release_val(__t2248);
  ergo_release_val(__t2249);
  ErgoVal __t2250 = EV_NULLV;
  ergo_release_val(__t2250);
}

static void ergo_m_cogito_DatePicker_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t2251 = self; ergo_retain_val(__t2251);
  ErgoVal __t2252 = a0; ergo_retain_val(__t2252);
  ErgoVal __t2253 = a1; ergo_retain_val(__t2253);
  ErgoVal __t2254 = a2; ergo_retain_val(__t2254);
  ErgoVal __t2255 = a3; ergo_retain_val(__t2255);
  cogito_container_set_padding(__t2251, __t2252, __t2253, __t2254, __t2255);
  ergo_release_val(__t2251);
  ergo_release_val(__t2252);
  ergo_release_val(__t2253);
  ergo_release_val(__t2254);
  ergo_release_val(__t2255);
  ErgoVal __t2256 = EV_NULLV;
  ergo_release_val(__t2256);
}

static void ergo_m_cogito_DatePicker_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2257 = self; ergo_retain_val(__t2257);
  ErgoVal __t2258 = a0; ergo_retain_val(__t2258);
  cogito_container_set_align(__t2257, __t2258);
  ergo_release_val(__t2257);
  ergo_release_val(__t2258);
  ErgoVal __t2259 = EV_NULLV;
  ergo_release_val(__t2259);
}

static void ergo_m_cogito_DatePicker_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2260 = self; ergo_retain_val(__t2260);
  ErgoVal __t2261 = a0; ergo_retain_val(__t2261);
  cogito_container_set_halign(__t2260, __t2261);
  ergo_release_val(__t2260);
  ergo_release_val(__t2261);
  ErgoVal __t2262 = EV_NULLV;
  ergo_release_val(__t2262);
}

static void ergo_m_cogito_DatePicker_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2263 = self; ergo_retain_val(__t2263);
  ErgoVal __t2264 = a0; ergo_retain_val(__t2264);
  cogito_container_set_valign(__t2263, __t2264);
  ergo_release_val(__t2263);
  ergo_release_val(__t2264);
  ErgoVal __t2265 = EV_NULLV;
  ergo_release_val(__t2265);
}

static void ergo_m_cogito_DatePicker_set_date(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2) {
}

static ErgoVal ergo_m_cogito_DatePicker_date(ErgoVal self) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static void ergo_m_cogito_DatePicker_on_change(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2266 = self; ergo_retain_val(__t2266);
  ErgoVal __t2267 = a0; ergo_retain_val(__t2267);
  cogito_datepicker_on_change(__t2266, __t2267);
  ergo_release_val(__t2266);
  ergo_release_val(__t2267);
  ErgoVal __t2268 = EV_NULLV;
  ergo_release_val(__t2268);
}

static void ergo_m_cogito_DatePicker_set_a11y_label(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2269 = self; ergo_retain_val(__t2269);
  ErgoVal __t2270 = a0; ergo_retain_val(__t2270);
  cogito_node_set_a11y_label(__t2269, __t2270);
  ergo_release_val(__t2269);
  ergo_release_val(__t2270);
  ErgoVal __t2271 = EV_NULLV;
  ergo_release_val(__t2271);
}

static void ergo_m_cogito_DatePicker_set_a11y_role(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2272 = self; ergo_retain_val(__t2272);
  ErgoVal __t2273 = a0; ergo_retain_val(__t2273);
  cogito_node_set_a11y_role(__t2272, __t2273);
  ergo_release_val(__t2272);
  ergo_release_val(__t2273);
  ErgoVal __t2274 = EV_NULLV;
  ergo_release_val(__t2274);
}

static void ergo_m_cogito_DatePicker_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2275 = self; ergo_retain_val(__t2275);
  ErgoVal __t2276 = a0; ergo_retain_val(__t2276);
  cogito_container_set_hexpand(__t2275, __t2276);
  ergo_release_val(__t2275);
  ergo_release_val(__t2276);
  ErgoVal __t2277 = EV_NULLV;
  ergo_release_val(__t2277);
}

static void ergo_m_cogito_DatePicker_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2278 = self; ergo_retain_val(__t2278);
  ErgoVal __t2279 = a0; ergo_retain_val(__t2279);
  cogito_container_set_vexpand(__t2278, __t2279);
  ergo_release_val(__t2278);
  ergo_release_val(__t2279);
  ErgoVal __t2280 = EV_NULLV;
  ergo_release_val(__t2280);
}

static void ergo_m_cogito_DatePicker_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2281 = self; ergo_retain_val(__t2281);
  ErgoVal __t2282 = a0; ergo_retain_val(__t2282);
  cogito_node_set_disabled(__t2281, __t2282);
  ergo_release_val(__t2281);
  ergo_release_val(__t2282);
  ErgoVal __t2283 = EV_NULLV;
  ergo_release_val(__t2283);
}

static void ergo_m_cogito_DatePicker_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2284 = self; ergo_retain_val(__t2284);
  ErgoVal __t2285 = a0; ergo_retain_val(__t2285);
  cogito_node_set_class(__t2284, __t2285);
  ergo_release_val(__t2284);
  ergo_release_val(__t2285);
  ErgoVal __t2286 = EV_NULLV;
  ergo_release_val(__t2286);
}

static void ergo_m_cogito_DatePicker_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2287 = self; ergo_retain_val(__t2287);
  ErgoVal __t2288 = a0; ergo_retain_val(__t2288);
  cogito_node_set_id(__t2287, __t2288);
  ergo_release_val(__t2287);
  ergo_release_val(__t2288);
  ErgoVal __t2289 = EV_NULLV;
  ergo_release_val(__t2289);
}

static void ergo_m_cogito_Stepper_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t2290 = self; ergo_retain_val(__t2290);
  ErgoVal __t2291 = a0; ergo_retain_val(__t2291);
  ErgoVal __t2292 = a1; ergo_retain_val(__t2292);
  ErgoVal __t2293 = a2; ergo_retain_val(__t2293);
  ErgoVal __t2294 = a3; ergo_retain_val(__t2294);
  cogito_container_set_margins(__t2290, __t2291, __t2292, __t2293, __t2294);
  ergo_release_val(__t2290);
  ergo_release_val(__t2291);
  ergo_release_val(__t2292);
  ergo_release_val(__t2293);
  ergo_release_val(__t2294);
  ErgoVal __t2295 = EV_NULLV;
  ergo_release_val(__t2295);
}

static void ergo_m_cogito_Stepper_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t2296 = self; ergo_retain_val(__t2296);
  ErgoVal __t2297 = a0; ergo_retain_val(__t2297);
  ErgoVal __t2298 = a1; ergo_retain_val(__t2298);
  ErgoVal __t2299 = a2; ergo_retain_val(__t2299);
  ErgoVal __t2300 = a3; ergo_retain_val(__t2300);
  cogito_container_set_padding(__t2296, __t2297, __t2298, __t2299, __t2300);
  ergo_release_val(__t2296);
  ergo_release_val(__t2297);
  ergo_release_val(__t2298);
  ergo_release_val(__t2299);
  ergo_release_val(__t2300);
  ErgoVal __t2301 = EV_NULLV;
  ergo_release_val(__t2301);
}

static void ergo_m_cogito_Stepper_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2302 = self; ergo_retain_val(__t2302);
  ErgoVal __t2303 = a0; ergo_retain_val(__t2303);
  cogito_container_set_align(__t2302, __t2303);
  ergo_release_val(__t2302);
  ergo_release_val(__t2303);
  ErgoVal __t2304 = EV_NULLV;
  ergo_release_val(__t2304);
}

static void ergo_m_cogito_Stepper_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2305 = self; ergo_retain_val(__t2305);
  ErgoVal __t2306 = a0; ergo_retain_val(__t2306);
  cogito_container_set_halign(__t2305, __t2306);
  ergo_release_val(__t2305);
  ergo_release_val(__t2306);
  ErgoVal __t2307 = EV_NULLV;
  ergo_release_val(__t2307);
}

static void ergo_m_cogito_Stepper_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2308 = self; ergo_retain_val(__t2308);
  ErgoVal __t2309 = a0; ergo_retain_val(__t2309);
  cogito_container_set_valign(__t2308, __t2309);
  ergo_release_val(__t2308);
  ergo_release_val(__t2309);
  ErgoVal __t2310 = EV_NULLV;
  ergo_release_val(__t2310);
}

static void ergo_m_cogito_Stepper_set_value(ErgoVal self, ErgoVal a0) {
}

static ErgoVal ergo_m_cogito_Stepper_value(ErgoVal self) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static void ergo_m_cogito_Stepper_on_change(ErgoVal self, ErgoVal a0) {
}

static void ergo_m_cogito_Stepper_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2311 = self; ergo_retain_val(__t2311);
  ErgoVal __t2312 = a0; ergo_retain_val(__t2312);
  cogito_container_set_hexpand(__t2311, __t2312);
  ergo_release_val(__t2311);
  ergo_release_val(__t2312);
  ErgoVal __t2313 = EV_NULLV;
  ergo_release_val(__t2313);
}

static void ergo_m_cogito_Stepper_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2314 = self; ergo_retain_val(__t2314);
  ErgoVal __t2315 = a0; ergo_retain_val(__t2315);
  cogito_container_set_vexpand(__t2314, __t2315);
  ergo_release_val(__t2314);
  ergo_release_val(__t2315);
  ErgoVal __t2316 = EV_NULLV;
  ergo_release_val(__t2316);
}

static void ergo_m_cogito_Stepper_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2317 = self; ergo_retain_val(__t2317);
  ErgoVal __t2318 = a0; ergo_retain_val(__t2318);
  cogito_node_set_class(__t2317, __t2318);
  ergo_release_val(__t2317);
  ergo_release_val(__t2318);
  ErgoVal __t2319 = EV_NULLV;
  ergo_release_val(__t2319);
}

static void ergo_m_cogito_Stepper_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2320 = self; ergo_retain_val(__t2320);
  ErgoVal __t2321 = a0; ergo_retain_val(__t2321);
  cogito_node_set_id(__t2320, __t2321);
  ergo_release_val(__t2320);
  ergo_release_val(__t2321);
  ErgoVal __t2322 = EV_NULLV;
  ergo_release_val(__t2322);
}

static void ergo_m_cogito_Dropdown_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t2323 = self; ergo_retain_val(__t2323);
  ErgoVal __t2324 = a0; ergo_retain_val(__t2324);
  ErgoVal __t2325 = a1; ergo_retain_val(__t2325);
  ErgoVal __t2326 = a2; ergo_retain_val(__t2326);
  ErgoVal __t2327 = a3; ergo_retain_val(__t2327);
  cogito_container_set_margins(__t2323, __t2324, __t2325, __t2326, __t2327);
  ergo_release_val(__t2323);
  ergo_release_val(__t2324);
  ergo_release_val(__t2325);
  ergo_release_val(__t2326);
  ergo_release_val(__t2327);
  ErgoVal __t2328 = EV_NULLV;
  ergo_release_val(__t2328);
}

static void ergo_m_cogito_Dropdown_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t2329 = self; ergo_retain_val(__t2329);
  ErgoVal __t2330 = a0; ergo_retain_val(__t2330);
  ErgoVal __t2331 = a1; ergo_retain_val(__t2331);
  ErgoVal __t2332 = a2; ergo_retain_val(__t2332);
  ErgoVal __t2333 = a3; ergo_retain_val(__t2333);
  cogito_container_set_padding(__t2329, __t2330, __t2331, __t2332, __t2333);
  ergo_release_val(__t2329);
  ergo_release_val(__t2330);
  ergo_release_val(__t2331);
  ergo_release_val(__t2332);
  ergo_release_val(__t2333);
  ErgoVal __t2334 = EV_NULLV;
  ergo_release_val(__t2334);
}

static void ergo_m_cogito_Dropdown_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2335 = self; ergo_retain_val(__t2335);
  ErgoVal __t2336 = a0; ergo_retain_val(__t2336);
  cogito_container_set_align(__t2335, __t2336);
  ergo_release_val(__t2335);
  ergo_release_val(__t2336);
  ErgoVal __t2337 = EV_NULLV;
  ergo_release_val(__t2337);
}

static void ergo_m_cogito_Dropdown_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2338 = self; ergo_retain_val(__t2338);
  ErgoVal __t2339 = a0; ergo_retain_val(__t2339);
  cogito_container_set_halign(__t2338, __t2339);
  ergo_release_val(__t2338);
  ergo_release_val(__t2339);
  ErgoVal __t2340 = EV_NULLV;
  ergo_release_val(__t2340);
}

static void ergo_m_cogito_Dropdown_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2341 = self; ergo_retain_val(__t2341);
  ErgoVal __t2342 = a0; ergo_retain_val(__t2342);
  cogito_container_set_valign(__t2341, __t2342);
  ergo_release_val(__t2341);
  ergo_release_val(__t2342);
  ErgoVal __t2343 = EV_NULLV;
  ergo_release_val(__t2343);
}

static void ergo_m_cogito_Dropdown_align_begin(ErgoVal self) {
  ErgoVal __t2344 = self; ergo_retain_val(__t2344);
  ErgoVal __t2345 = EV_INT(0);
  cogito_container_set_align(__t2344, __t2345);
  ergo_release_val(__t2344);
  ergo_release_val(__t2345);
  ErgoVal __t2346 = EV_NULLV;
  ergo_release_val(__t2346);
}

static void ergo_m_cogito_Dropdown_align_center(ErgoVal self) {
  ErgoVal __t2347 = self; ergo_retain_val(__t2347);
  ErgoVal __t2348 = EV_INT(1);
  cogito_container_set_align(__t2347, __t2348);
  ergo_release_val(__t2347);
  ergo_release_val(__t2348);
  ErgoVal __t2349 = EV_NULLV;
  ergo_release_val(__t2349);
}

static void ergo_m_cogito_Dropdown_align_end(ErgoVal self) {
  ErgoVal __t2350 = self; ergo_retain_val(__t2350);
  ErgoVal __t2351 = EV_INT(2);
  cogito_container_set_align(__t2350, __t2351);
  ergo_release_val(__t2350);
  ergo_release_val(__t2351);
  ErgoVal __t2352 = EV_NULLV;
  ergo_release_val(__t2352);
}

static void ergo_m_cogito_Dropdown_set_items(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2353 = self; ergo_retain_val(__t2353);
  ErgoVal __t2354 = a0; ergo_retain_val(__t2354);
  cogito_dropdown_set_items(__t2353, __t2354);
  ergo_release_val(__t2353);
  ergo_release_val(__t2354);
  ErgoVal __t2355 = EV_NULLV;
  ergo_release_val(__t2355);
}

static void ergo_m_cogito_Dropdown_set_selected(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2356 = self; ergo_retain_val(__t2356);
  ErgoVal __t2357 = a0; ergo_retain_val(__t2357);
  cogito_dropdown_set_selected(__t2356, __t2357);
  ergo_release_val(__t2356);
  ergo_release_val(__t2357);
  ErgoVal __t2358 = EV_NULLV;
  ergo_release_val(__t2358);
}

static ErgoVal ergo_m_cogito_Dropdown_selected(ErgoVal self) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t2359 = self; ergo_retain_val(__t2359);
  ErgoVal __t2360 = cogito_dropdown_get_selected(__t2359);
  ergo_release_val(__t2359);
  ergo_move_into(&__ret, __t2360);
  return __ret;
  return __ret;
}

static void ergo_m_cogito_Dropdown_on_change(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2361 = self; ergo_retain_val(__t2361);
  ErgoVal __t2362 = a0; ergo_retain_val(__t2362);
  cogito_dropdown_on_change(__t2361, __t2362);
  ergo_release_val(__t2361);
  ergo_release_val(__t2362);
  ErgoVal __t2363 = EV_NULLV;
  ergo_release_val(__t2363);
}

static void ergo_m_cogito_Dropdown_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2364 = self; ergo_retain_val(__t2364);
  ErgoVal __t2365 = a0; ergo_retain_val(__t2365);
  cogito_container_set_hexpand(__t2364, __t2365);
  ergo_release_val(__t2364);
  ergo_release_val(__t2365);
  ErgoVal __t2366 = EV_NULLV;
  ergo_release_val(__t2366);
}

static void ergo_m_cogito_Dropdown_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2367 = self; ergo_retain_val(__t2367);
  ErgoVal __t2368 = a0; ergo_retain_val(__t2368);
  cogito_container_set_vexpand(__t2367, __t2368);
  ergo_release_val(__t2367);
  ergo_release_val(__t2368);
  ErgoVal __t2369 = EV_NULLV;
  ergo_release_val(__t2369);
}

static void ergo_m_cogito_Dropdown_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2370 = self; ergo_retain_val(__t2370);
  ErgoVal __t2371 = a0; ergo_retain_val(__t2371);
  cogito_node_set_disabled(__t2370, __t2371);
  ergo_release_val(__t2370);
  ergo_release_val(__t2371);
  ErgoVal __t2372 = EV_NULLV;
  ergo_release_val(__t2372);
}

static void ergo_m_cogito_Dropdown_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2373 = self; ergo_retain_val(__t2373);
  ErgoVal __t2374 = a0; ergo_retain_val(__t2374);
  cogito_node_set_class(__t2373, __t2374);
  ergo_release_val(__t2373);
  ergo_release_val(__t2374);
  ErgoVal __t2375 = EV_NULLV;
  ergo_release_val(__t2375);
}

static void ergo_m_cogito_Dropdown_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2376 = self; ergo_retain_val(__t2376);
  ErgoVal __t2377 = a0; ergo_retain_val(__t2377);
  cogito_node_set_id(__t2376, __t2377);
  ergo_release_val(__t2376);
  ergo_release_val(__t2377);
  ErgoVal __t2378 = EV_NULLV;
  ergo_release_val(__t2378);
}

static void ergo_m_cogito_Slider_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t2379 = self; ergo_retain_val(__t2379);
  ErgoVal __t2380 = a0; ergo_retain_val(__t2380);
  ErgoVal __t2381 = a1; ergo_retain_val(__t2381);
  ErgoVal __t2382 = a2; ergo_retain_val(__t2382);
  ErgoVal __t2383 = a3; ergo_retain_val(__t2383);
  cogito_container_set_margins(__t2379, __t2380, __t2381, __t2382, __t2383);
  ergo_release_val(__t2379);
  ergo_release_val(__t2380);
  ergo_release_val(__t2381);
  ergo_release_val(__t2382);
  ergo_release_val(__t2383);
  ErgoVal __t2384 = EV_NULLV;
  ergo_release_val(__t2384);
}

static void ergo_m_cogito_Slider_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t2385 = self; ergo_retain_val(__t2385);
  ErgoVal __t2386 = a0; ergo_retain_val(__t2386);
  ErgoVal __t2387 = a1; ergo_retain_val(__t2387);
  ErgoVal __t2388 = a2; ergo_retain_val(__t2388);
  ErgoVal __t2389 = a3; ergo_retain_val(__t2389);
  cogito_container_set_padding(__t2385, __t2386, __t2387, __t2388, __t2389);
  ergo_release_val(__t2385);
  ergo_release_val(__t2386);
  ergo_release_val(__t2387);
  ergo_release_val(__t2388);
  ergo_release_val(__t2389);
  ErgoVal __t2390 = EV_NULLV;
  ergo_release_val(__t2390);
}

static void ergo_m_cogito_Slider_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2391 = self; ergo_retain_val(__t2391);
  ErgoVal __t2392 = a0; ergo_retain_val(__t2392);
  cogito_container_set_align(__t2391, __t2392);
  ergo_release_val(__t2391);
  ergo_release_val(__t2392);
  ErgoVal __t2393 = EV_NULLV;
  ergo_release_val(__t2393);
}

static void ergo_m_cogito_Slider_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2394 = self; ergo_retain_val(__t2394);
  ErgoVal __t2395 = a0; ergo_retain_val(__t2395);
  cogito_container_set_halign(__t2394, __t2395);
  ergo_release_val(__t2394);
  ergo_release_val(__t2395);
  ErgoVal __t2396 = EV_NULLV;
  ergo_release_val(__t2396);
}

static void ergo_m_cogito_Slider_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2397 = self; ergo_retain_val(__t2397);
  ErgoVal __t2398 = a0; ergo_retain_val(__t2398);
  cogito_container_set_valign(__t2397, __t2398);
  ergo_release_val(__t2397);
  ergo_release_val(__t2398);
  ErgoVal __t2399 = EV_NULLV;
  ergo_release_val(__t2399);
}

static void ergo_m_cogito_Slider_align_begin(ErgoVal self) {
  ErgoVal __t2400 = self; ergo_retain_val(__t2400);
  ErgoVal __t2401 = EV_INT(0);
  cogito_container_set_align(__t2400, __t2401);
  ergo_release_val(__t2400);
  ergo_release_val(__t2401);
  ErgoVal __t2402 = EV_NULLV;
  ergo_release_val(__t2402);
}

static void ergo_m_cogito_Slider_align_center(ErgoVal self) {
  ErgoVal __t2403 = self; ergo_retain_val(__t2403);
  ErgoVal __t2404 = EV_INT(1);
  cogito_container_set_align(__t2403, __t2404);
  ergo_release_val(__t2403);
  ergo_release_val(__t2404);
  ErgoVal __t2405 = EV_NULLV;
  ergo_release_val(__t2405);
}

static void ergo_m_cogito_Slider_align_end(ErgoVal self) {
  ErgoVal __t2406 = self; ergo_retain_val(__t2406);
  ErgoVal __t2407 = EV_INT(2);
  cogito_container_set_align(__t2406, __t2407);
  ergo_release_val(__t2406);
  ergo_release_val(__t2407);
  ErgoVal __t2408 = EV_NULLV;
  ergo_release_val(__t2408);
}

static void ergo_m_cogito_Slider_set_value(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2409 = self; ergo_retain_val(__t2409);
  ErgoVal __t2410 = a0; ergo_retain_val(__t2410);
  cogito_slider_set_value(__t2409, __t2410);
  ergo_release_val(__t2409);
  ergo_release_val(__t2410);
  ErgoVal __t2411 = EV_NULLV;
  ergo_release_val(__t2411);
}

static ErgoVal ergo_m_cogito_Slider_value(ErgoVal self) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t2412 = self; ergo_retain_val(__t2412);
  ErgoVal __t2413 = cogito_slider_get_value(__t2412);
  ergo_release_val(__t2412);
  ergo_move_into(&__ret, __t2413);
  return __ret;
  return __ret;
}

static void ergo_m_cogito_Slider_on_change(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2414 = self; ergo_retain_val(__t2414);
  ErgoVal __t2415 = a0; ergo_retain_val(__t2415);
  cogito_slider_on_change(__t2414, __t2415);
  ergo_release_val(__t2414);
  ergo_release_val(__t2415);
  ErgoVal __t2416 = EV_NULLV;
  ergo_release_val(__t2416);
}

static void ergo_m_cogito_Slider_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2417 = self; ergo_retain_val(__t2417);
  ErgoVal __t2418 = a0; ergo_retain_val(__t2418);
  cogito_container_set_hexpand(__t2417, __t2418);
  ergo_release_val(__t2417);
  ergo_release_val(__t2418);
  ErgoVal __t2419 = EV_NULLV;
  ergo_release_val(__t2419);
}

static void ergo_m_cogito_Slider_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2420 = self; ergo_retain_val(__t2420);
  ErgoVal __t2421 = a0; ergo_retain_val(__t2421);
  cogito_container_set_vexpand(__t2420, __t2421);
  ergo_release_val(__t2420);
  ergo_release_val(__t2421);
  ErgoVal __t2422 = EV_NULLV;
  ergo_release_val(__t2422);
}

static void ergo_m_cogito_Slider_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2423 = self; ergo_retain_val(__t2423);
  ErgoVal __t2424 = a0; ergo_retain_val(__t2424);
  cogito_node_set_disabled(__t2423, __t2424);
  ergo_release_val(__t2423);
  ergo_release_val(__t2424);
  ErgoVal __t2425 = EV_NULLV;
  ergo_release_val(__t2425);
}

static void ergo_m_cogito_Slider_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2426 = self; ergo_retain_val(__t2426);
  ErgoVal __t2427 = a0; ergo_retain_val(__t2427);
  cogito_node_set_class(__t2426, __t2427);
  ergo_release_val(__t2426);
  ergo_release_val(__t2427);
  ErgoVal __t2428 = EV_NULLV;
  ergo_release_val(__t2428);
}

static void ergo_m_cogito_Slider_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2429 = self; ergo_retain_val(__t2429);
  ErgoVal __t2430 = a0; ergo_retain_val(__t2430);
  cogito_node_set_id(__t2429, __t2430);
  ergo_release_val(__t2429);
  ergo_release_val(__t2430);
  ErgoVal __t2431 = EV_NULLV;
  ergo_release_val(__t2431);
}

static void ergo_m_cogito_Tabs_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t2432 = self; ergo_retain_val(__t2432);
  ErgoVal __t2433 = a0; ergo_retain_val(__t2433);
  ErgoVal __t2434 = a1; ergo_retain_val(__t2434);
  ErgoVal __t2435 = a2; ergo_retain_val(__t2435);
  ErgoVal __t2436 = a3; ergo_retain_val(__t2436);
  cogito_container_set_margins(__t2432, __t2433, __t2434, __t2435, __t2436);
  ergo_release_val(__t2432);
  ergo_release_val(__t2433);
  ergo_release_val(__t2434);
  ergo_release_val(__t2435);
  ergo_release_val(__t2436);
  ErgoVal __t2437 = EV_NULLV;
  ergo_release_val(__t2437);
}

static void ergo_m_cogito_Tabs_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t2438 = self; ergo_retain_val(__t2438);
  ErgoVal __t2439 = a0; ergo_retain_val(__t2439);
  ErgoVal __t2440 = a1; ergo_retain_val(__t2440);
  ErgoVal __t2441 = a2; ergo_retain_val(__t2441);
  ErgoVal __t2442 = a3; ergo_retain_val(__t2442);
  cogito_container_set_padding(__t2438, __t2439, __t2440, __t2441, __t2442);
  ergo_release_val(__t2438);
  ergo_release_val(__t2439);
  ergo_release_val(__t2440);
  ergo_release_val(__t2441);
  ergo_release_val(__t2442);
  ErgoVal __t2443 = EV_NULLV;
  ergo_release_val(__t2443);
}

static void ergo_m_cogito_Tabs_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2444 = self; ergo_retain_val(__t2444);
  ErgoVal __t2445 = a0; ergo_retain_val(__t2445);
  cogito_container_set_align(__t2444, __t2445);
  ergo_release_val(__t2444);
  ergo_release_val(__t2445);
  ErgoVal __t2446 = EV_NULLV;
  ergo_release_val(__t2446);
}

static void ergo_m_cogito_Tabs_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2447 = self; ergo_retain_val(__t2447);
  ErgoVal __t2448 = a0; ergo_retain_val(__t2448);
  cogito_container_set_halign(__t2447, __t2448);
  ergo_release_val(__t2447);
  ergo_release_val(__t2448);
  ErgoVal __t2449 = EV_NULLV;
  ergo_release_val(__t2449);
}

static void ergo_m_cogito_Tabs_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2450 = self; ergo_retain_val(__t2450);
  ErgoVal __t2451 = a0; ergo_retain_val(__t2451);
  cogito_container_set_valign(__t2450, __t2451);
  ergo_release_val(__t2450);
  ergo_release_val(__t2451);
  ErgoVal __t2452 = EV_NULLV;
  ergo_release_val(__t2452);
}

static void ergo_m_cogito_Tabs_set_items(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2453 = self; ergo_retain_val(__t2453);
  ErgoVal __t2454 = a0; ergo_retain_val(__t2454);
  cogito_tabs_set_items(__t2453, __t2454);
  ergo_release_val(__t2453);
  ergo_release_val(__t2454);
  ErgoVal __t2455 = EV_NULLV;
  ergo_release_val(__t2455);
}

static void ergo_m_cogito_Tabs_set_ids(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2456 = self; ergo_retain_val(__t2456);
  ErgoVal __t2457 = a0; ergo_retain_val(__t2457);
  cogito_tabs_set_ids(__t2456, __t2457);
  ergo_release_val(__t2456);
  ergo_release_val(__t2457);
  ErgoVal __t2458 = EV_NULLV;
  ergo_release_val(__t2458);
}

static void ergo_m_cogito_Tabs_set_selected(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2459 = self; ergo_retain_val(__t2459);
  ErgoVal __t2460 = a0; ergo_retain_val(__t2460);
  cogito_tabs_set_selected(__t2459, __t2460);
  ergo_release_val(__t2459);
  ergo_release_val(__t2460);
  ErgoVal __t2461 = EV_NULLV;
  ergo_release_val(__t2461);
}

static ErgoVal ergo_m_cogito_Tabs_selected(ErgoVal self) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t2462 = self; ergo_retain_val(__t2462);
  ErgoVal __t2463 = cogito_tabs_get_selected(__t2462);
  ergo_release_val(__t2462);
  ergo_move_into(&__ret, __t2463);
  return __ret;
  return __ret;
}

static void ergo_m_cogito_Tabs_on_change(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2464 = self; ergo_retain_val(__t2464);
  ErgoVal __t2465 = a0; ergo_retain_val(__t2465);
  cogito_tabs_on_change(__t2464, __t2465);
  ergo_release_val(__t2464);
  ergo_release_val(__t2465);
  ErgoVal __t2466 = EV_NULLV;
  ergo_release_val(__t2466);
}

static void ergo_m_cogito_Tabs_bind(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2467 = self; ergo_retain_val(__t2467);
  ErgoVal __t2468 = a0; ergo_retain_val(__t2468);
  cogito_tabs_bind(__t2467, __t2468);
  ergo_release_val(__t2467);
  ergo_release_val(__t2468);
  ErgoVal __t2469 = EV_NULLV;
  ergo_release_val(__t2469);
}

static void ergo_m_cogito_Tabs_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2470 = self; ergo_retain_val(__t2470);
  ErgoVal __t2471 = a0; ergo_retain_val(__t2471);
  cogito_container_set_hexpand(__t2470, __t2471);
  ergo_release_val(__t2470);
  ergo_release_val(__t2471);
  ErgoVal __t2472 = EV_NULLV;
  ergo_release_val(__t2472);
}

static void ergo_m_cogito_Tabs_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2473 = self; ergo_retain_val(__t2473);
  ErgoVal __t2474 = a0; ergo_retain_val(__t2474);
  cogito_container_set_vexpand(__t2473, __t2474);
  ergo_release_val(__t2473);
  ergo_release_val(__t2474);
  ErgoVal __t2475 = EV_NULLV;
  ergo_release_val(__t2475);
}

static void ergo_m_cogito_Tabs_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2476 = self; ergo_retain_val(__t2476);
  ErgoVal __t2477 = a0; ergo_retain_val(__t2477);
  cogito_node_set_disabled(__t2476, __t2477);
  ergo_release_val(__t2476);
  ergo_release_val(__t2477);
  ErgoVal __t2478 = EV_NULLV;
  ergo_release_val(__t2478);
}

static void ergo_m_cogito_Tabs_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2479 = self; ergo_retain_val(__t2479);
  ErgoVal __t2480 = a0; ergo_retain_val(__t2480);
  cogito_node_set_class(__t2479, __t2480);
  ergo_release_val(__t2479);
  ergo_release_val(__t2480);
  ErgoVal __t2481 = EV_NULLV;
  ergo_release_val(__t2481);
}

static void ergo_m_cogito_Tabs_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2482 = self; ergo_retain_val(__t2482);
  ErgoVal __t2483 = a0; ergo_retain_val(__t2483);
  cogito_node_set_id(__t2482, __t2483);
  ergo_release_val(__t2482);
  ergo_release_val(__t2483);
  ErgoVal __t2484 = EV_NULLV;
  ergo_release_val(__t2484);
}

static void ergo_m_cogito_SegmentedControl_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2485 = self; ergo_retain_val(__t2485);
  ErgoVal __t2486 = a0; ergo_retain_val(__t2486);
  cogito_container_add(__t2485, __t2486);
  ergo_release_val(__t2485);
  ergo_release_val(__t2486);
  ErgoVal __t2487 = EV_NULLV;
  ergo_release_val(__t2487);
}

static void ergo_m_cogito_SegmentedControl_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t2488 = self; ergo_retain_val(__t2488);
  ErgoVal __t2489 = a0; ergo_retain_val(__t2489);
  ErgoVal __t2490 = a1; ergo_retain_val(__t2490);
  ErgoVal __t2491 = a2; ergo_retain_val(__t2491);
  ErgoVal __t2492 = a3; ergo_retain_val(__t2492);
  cogito_container_set_margins(__t2488, __t2489, __t2490, __t2491, __t2492);
  ergo_release_val(__t2488);
  ergo_release_val(__t2489);
  ergo_release_val(__t2490);
  ergo_release_val(__t2491);
  ergo_release_val(__t2492);
  ErgoVal __t2493 = EV_NULLV;
  ergo_release_val(__t2493);
}

static void ergo_m_cogito_SegmentedControl_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t2494 = self; ergo_retain_val(__t2494);
  ErgoVal __t2495 = a0; ergo_retain_val(__t2495);
  ErgoVal __t2496 = a1; ergo_retain_val(__t2496);
  ErgoVal __t2497 = a2; ergo_retain_val(__t2497);
  ErgoVal __t2498 = a3; ergo_retain_val(__t2498);
  cogito_container_set_padding(__t2494, __t2495, __t2496, __t2497, __t2498);
  ergo_release_val(__t2494);
  ergo_release_val(__t2495);
  ergo_release_val(__t2496);
  ergo_release_val(__t2497);
  ergo_release_val(__t2498);
  ErgoVal __t2499 = EV_NULLV;
  ergo_release_val(__t2499);
}

static void ergo_m_cogito_SegmentedControl_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2500 = self; ergo_retain_val(__t2500);
  ErgoVal __t2501 = a0; ergo_retain_val(__t2501);
  cogito_container_set_align(__t2500, __t2501);
  ergo_release_val(__t2500);
  ergo_release_val(__t2501);
  ErgoVal __t2502 = EV_NULLV;
  ergo_release_val(__t2502);
}

static void ergo_m_cogito_SegmentedControl_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2503 = self; ergo_retain_val(__t2503);
  ErgoVal __t2504 = a0; ergo_retain_val(__t2504);
  cogito_container_set_halign(__t2503, __t2504);
  ergo_release_val(__t2503);
  ergo_release_val(__t2504);
  ErgoVal __t2505 = EV_NULLV;
  ergo_release_val(__t2505);
}

static void ergo_m_cogito_SegmentedControl_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2506 = self; ergo_retain_val(__t2506);
  ErgoVal __t2507 = a0; ergo_retain_val(__t2507);
  cogito_container_set_valign(__t2506, __t2507);
  ergo_release_val(__t2506);
  ergo_release_val(__t2507);
  ErgoVal __t2508 = EV_NULLV;
  ergo_release_val(__t2508);
}

static void ergo_m_cogito_SegmentedControl_set_items(ErgoVal self, ErgoVal a0) {
}

static void ergo_m_cogito_SegmentedControl_set_selected(ErgoVal self, ErgoVal a0) {
}

static ErgoVal ergo_m_cogito_SegmentedControl_selected(ErgoVal self) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static void ergo_m_cogito_SegmentedControl_on_change(ErgoVal self, ErgoVal a0) {
}

static void ergo_m_cogito_SegmentedControl_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2509 = self; ergo_retain_val(__t2509);
  ErgoVal __t2510 = a0; ergo_retain_val(__t2510);
  cogito_container_set_hexpand(__t2509, __t2510);
  ergo_release_val(__t2509);
  ergo_release_val(__t2510);
  ErgoVal __t2511 = EV_NULLV;
  ergo_release_val(__t2511);
}

static void ergo_m_cogito_SegmentedControl_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2512 = self; ergo_retain_val(__t2512);
  ErgoVal __t2513 = a0; ergo_retain_val(__t2513);
  cogito_container_set_vexpand(__t2512, __t2513);
  ergo_release_val(__t2512);
  ergo_release_val(__t2513);
  ErgoVal __t2514 = EV_NULLV;
  ergo_release_val(__t2514);
}

static void ergo_m_cogito_SegmentedControl_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2515 = self; ergo_retain_val(__t2515);
  ErgoVal __t2516 = a0; ergo_retain_val(__t2516);
  cogito_node_set_class(__t2515, __t2516);
  ergo_release_val(__t2515);
  ergo_release_val(__t2516);
  ErgoVal __t2517 = EV_NULLV;
  ergo_release_val(__t2517);
}

static void ergo_m_cogito_SegmentedControl_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2518 = self; ergo_retain_val(__t2518);
  ErgoVal __t2519 = a0; ergo_retain_val(__t2519);
  cogito_node_set_id(__t2518, __t2519);
  ergo_release_val(__t2518);
  ergo_release_val(__t2519);
  ErgoVal __t2520 = EV_NULLV;
  ergo_release_val(__t2520);
}

static void ergo_m_cogito_ViewSwitcher_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2521 = self; ergo_retain_val(__t2521);
  ErgoVal __t2522 = a0; ergo_retain_val(__t2522);
  cogito_container_add(__t2521, __t2522);
  ergo_release_val(__t2521);
  ergo_release_val(__t2522);
  ErgoVal __t2523 = EV_NULLV;
  ergo_release_val(__t2523);
}

static void ergo_m_cogito_ViewSwitcher_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t2524 = self; ergo_retain_val(__t2524);
  ErgoVal __t2525 = a0; ergo_retain_val(__t2525);
  ErgoVal __t2526 = a1; ergo_retain_val(__t2526);
  ErgoVal __t2527 = a2; ergo_retain_val(__t2527);
  ErgoVal __t2528 = a3; ergo_retain_val(__t2528);
  cogito_container_set_margins(__t2524, __t2525, __t2526, __t2527, __t2528);
  ergo_release_val(__t2524);
  ergo_release_val(__t2525);
  ergo_release_val(__t2526);
  ergo_release_val(__t2527);
  ergo_release_val(__t2528);
  ErgoVal __t2529 = EV_NULLV;
  ergo_release_val(__t2529);
}

static void ergo_m_cogito_ViewSwitcher_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t2530 = self; ergo_retain_val(__t2530);
  ErgoVal __t2531 = a0; ergo_retain_val(__t2531);
  ErgoVal __t2532 = a1; ergo_retain_val(__t2532);
  ErgoVal __t2533 = a2; ergo_retain_val(__t2533);
  ErgoVal __t2534 = a3; ergo_retain_val(__t2534);
  cogito_container_set_padding(__t2530, __t2531, __t2532, __t2533, __t2534);
  ergo_release_val(__t2530);
  ergo_release_val(__t2531);
  ergo_release_val(__t2532);
  ergo_release_val(__t2533);
  ergo_release_val(__t2534);
  ErgoVal __t2535 = EV_NULLV;
  ergo_release_val(__t2535);
}

static void ergo_m_cogito_ViewSwitcher_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2536 = self; ergo_retain_val(__t2536);
  ErgoVal __t2537 = a0; ergo_retain_val(__t2537);
  cogito_container_set_align(__t2536, __t2537);
  ergo_release_val(__t2536);
  ergo_release_val(__t2537);
  ErgoVal __t2538 = EV_NULLV;
  ergo_release_val(__t2538);
}

static void ergo_m_cogito_ViewSwitcher_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2539 = self; ergo_retain_val(__t2539);
  ErgoVal __t2540 = a0; ergo_retain_val(__t2540);
  cogito_container_set_halign(__t2539, __t2540);
  ergo_release_val(__t2539);
  ergo_release_val(__t2540);
  ErgoVal __t2541 = EV_NULLV;
  ergo_release_val(__t2541);
}

static void ergo_m_cogito_ViewSwitcher_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2542 = self; ergo_retain_val(__t2542);
  ErgoVal __t2543 = a0; ergo_retain_val(__t2543);
  cogito_container_set_valign(__t2542, __t2543);
  ergo_release_val(__t2542);
  ergo_release_val(__t2543);
  ErgoVal __t2544 = EV_NULLV;
  ergo_release_val(__t2544);
}

static void ergo_m_cogito_ViewSwitcher_set_active(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2545 = self; ergo_retain_val(__t2545);
  ErgoVal __t2546 = a0; ergo_retain_val(__t2546);
  cogito_view_switcher_set_active(__t2545, __t2546);
  ergo_release_val(__t2545);
  ergo_release_val(__t2546);
  ErgoVal __t2547 = EV_NULLV;
  ergo_release_val(__t2547);
}

static ErgoVal ergo_m_cogito_ViewSwitcher_build(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t2548 = self; ergo_retain_val(__t2548);
  ErgoVal __t2549 = a0; ergo_retain_val(__t2549);
  cogito_build(__t2548, __t2549);
  ergo_release_val(__t2548);
  ergo_release_val(__t2549);
  ErgoVal __t2550 = EV_NULLV;
  ergo_release_val(__t2550);
  ErgoVal __t2551 = self; ergo_retain_val(__t2551);
  ergo_move_into(&__ret, __t2551);
  return __ret;
  return __ret;
}

static void ergo_m_cogito_ViewSwitcher_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2552 = self; ergo_retain_val(__t2552);
  ErgoVal __t2553 = a0; ergo_retain_val(__t2553);
  cogito_container_set_hexpand(__t2552, __t2553);
  ergo_release_val(__t2552);
  ergo_release_val(__t2553);
  ErgoVal __t2554 = EV_NULLV;
  ergo_release_val(__t2554);
}

static void ergo_m_cogito_ViewSwitcher_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2555 = self; ergo_retain_val(__t2555);
  ErgoVal __t2556 = a0; ergo_retain_val(__t2556);
  cogito_container_set_vexpand(__t2555, __t2556);
  ergo_release_val(__t2555);
  ergo_release_val(__t2556);
  ErgoVal __t2557 = EV_NULLV;
  ergo_release_val(__t2557);
}

static void ergo_m_cogito_ViewSwitcher_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2558 = self; ergo_retain_val(__t2558);
  ErgoVal __t2559 = a0; ergo_retain_val(__t2559);
  cogito_node_set_disabled(__t2558, __t2559);
  ergo_release_val(__t2558);
  ergo_release_val(__t2559);
  ErgoVal __t2560 = EV_NULLV;
  ergo_release_val(__t2560);
}

static void ergo_m_cogito_ViewSwitcher_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2561 = self; ergo_retain_val(__t2561);
  ErgoVal __t2562 = a0; ergo_retain_val(__t2562);
  cogito_node_set_class(__t2561, __t2562);
  ergo_release_val(__t2561);
  ergo_release_val(__t2562);
  ErgoVal __t2563 = EV_NULLV;
  ergo_release_val(__t2563);
}

static void ergo_m_cogito_ViewSwitcher_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2564 = self; ergo_retain_val(__t2564);
  ErgoVal __t2565 = a0; ergo_retain_val(__t2565);
  cogito_node_set_id(__t2564, __t2565);
  ergo_release_val(__t2564);
  ergo_release_val(__t2565);
  ErgoVal __t2566 = EV_NULLV;
  ergo_release_val(__t2566);
}

static void ergo_m_cogito_Progress_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t2567 = self; ergo_retain_val(__t2567);
  ErgoVal __t2568 = a0; ergo_retain_val(__t2568);
  ErgoVal __t2569 = a1; ergo_retain_val(__t2569);
  ErgoVal __t2570 = a2; ergo_retain_val(__t2570);
  ErgoVal __t2571 = a3; ergo_retain_val(__t2571);
  cogito_container_set_margins(__t2567, __t2568, __t2569, __t2570, __t2571);
  ergo_release_val(__t2567);
  ergo_release_val(__t2568);
  ergo_release_val(__t2569);
  ergo_release_val(__t2570);
  ergo_release_val(__t2571);
  ErgoVal __t2572 = EV_NULLV;
  ergo_release_val(__t2572);
}

static void ergo_m_cogito_Progress_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t2573 = self; ergo_retain_val(__t2573);
  ErgoVal __t2574 = a0; ergo_retain_val(__t2574);
  ErgoVal __t2575 = a1; ergo_retain_val(__t2575);
  ErgoVal __t2576 = a2; ergo_retain_val(__t2576);
  ErgoVal __t2577 = a3; ergo_retain_val(__t2577);
  cogito_container_set_padding(__t2573, __t2574, __t2575, __t2576, __t2577);
  ergo_release_val(__t2573);
  ergo_release_val(__t2574);
  ergo_release_val(__t2575);
  ergo_release_val(__t2576);
  ergo_release_val(__t2577);
  ErgoVal __t2578 = EV_NULLV;
  ergo_release_val(__t2578);
}

static void ergo_m_cogito_Progress_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2579 = self; ergo_retain_val(__t2579);
  ErgoVal __t2580 = a0; ergo_retain_val(__t2580);
  cogito_container_set_align(__t2579, __t2580);
  ergo_release_val(__t2579);
  ergo_release_val(__t2580);
  ErgoVal __t2581 = EV_NULLV;
  ergo_release_val(__t2581);
}

static void ergo_m_cogito_Progress_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2582 = self; ergo_retain_val(__t2582);
  ErgoVal __t2583 = a0; ergo_retain_val(__t2583);
  cogito_container_set_halign(__t2582, __t2583);
  ergo_release_val(__t2582);
  ergo_release_val(__t2583);
  ErgoVal __t2584 = EV_NULLV;
  ergo_release_val(__t2584);
}

static void ergo_m_cogito_Progress_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2585 = self; ergo_retain_val(__t2585);
  ErgoVal __t2586 = a0; ergo_retain_val(__t2586);
  cogito_container_set_valign(__t2585, __t2586);
  ergo_release_val(__t2585);
  ergo_release_val(__t2586);
  ErgoVal __t2587 = EV_NULLV;
  ergo_release_val(__t2587);
}

static void ergo_m_cogito_Progress_set_value(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2588 = self; ergo_retain_val(__t2588);
  ErgoVal __t2589 = a0; ergo_retain_val(__t2589);
  cogito_progress_set_value(__t2588, __t2589);
  ergo_release_val(__t2588);
  ergo_release_val(__t2589);
  ErgoVal __t2590 = EV_NULLV;
  ergo_release_val(__t2590);
}

static ErgoVal ergo_m_cogito_Progress_value(ErgoVal self) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t2591 = self; ergo_retain_val(__t2591);
  ErgoVal __t2592 = cogito_progress_get_value(__t2591);
  ergo_release_val(__t2591);
  ergo_move_into(&__ret, __t2592);
  return __ret;
  return __ret;
}

static void ergo_m_cogito_Progress_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2593 = self; ergo_retain_val(__t2593);
  ErgoVal __t2594 = a0; ergo_retain_val(__t2594);
  cogito_container_set_hexpand(__t2593, __t2594);
  ergo_release_val(__t2593);
  ergo_release_val(__t2594);
  ErgoVal __t2595 = EV_NULLV;
  ergo_release_val(__t2595);
}

static void ergo_m_cogito_Progress_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2596 = self; ergo_retain_val(__t2596);
  ErgoVal __t2597 = a0; ergo_retain_val(__t2597);
  cogito_container_set_vexpand(__t2596, __t2597);
  ergo_release_val(__t2596);
  ergo_release_val(__t2597);
  ErgoVal __t2598 = EV_NULLV;
  ergo_release_val(__t2598);
}

static void ergo_m_cogito_Progress_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2599 = self; ergo_retain_val(__t2599);
  ErgoVal __t2600 = a0; ergo_retain_val(__t2600);
  cogito_node_set_disabled(__t2599, __t2600);
  ergo_release_val(__t2599);
  ergo_release_val(__t2600);
  ErgoVal __t2601 = EV_NULLV;
  ergo_release_val(__t2601);
}

static void ergo_m_cogito_Progress_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2602 = self; ergo_retain_val(__t2602);
  ErgoVal __t2603 = a0; ergo_retain_val(__t2603);
  cogito_node_set_class(__t2602, __t2603);
  ergo_release_val(__t2602);
  ergo_release_val(__t2603);
  ErgoVal __t2604 = EV_NULLV;
  ergo_release_val(__t2604);
}

static void ergo_m_cogito_Progress_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2605 = self; ergo_retain_val(__t2605);
  ErgoVal __t2606 = a0; ergo_retain_val(__t2606);
  cogito_node_set_id(__t2605, __t2606);
  ergo_release_val(__t2605);
  ergo_release_val(__t2606);
  ErgoVal __t2607 = EV_NULLV;
  ergo_release_val(__t2607);
}

static void ergo_m_cogito_Divider_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t2608 = self; ergo_retain_val(__t2608);
  ErgoVal __t2609 = a0; ergo_retain_val(__t2609);
  ErgoVal __t2610 = a1; ergo_retain_val(__t2610);
  ErgoVal __t2611 = a2; ergo_retain_val(__t2611);
  ErgoVal __t2612 = a3; ergo_retain_val(__t2612);
  cogito_container_set_margins(__t2608, __t2609, __t2610, __t2611, __t2612);
  ergo_release_val(__t2608);
  ergo_release_val(__t2609);
  ergo_release_val(__t2610);
  ergo_release_val(__t2611);
  ergo_release_val(__t2612);
  ErgoVal __t2613 = EV_NULLV;
  ergo_release_val(__t2613);
}

static void ergo_m_cogito_Divider_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t2614 = self; ergo_retain_val(__t2614);
  ErgoVal __t2615 = a0; ergo_retain_val(__t2615);
  ErgoVal __t2616 = a1; ergo_retain_val(__t2616);
  ErgoVal __t2617 = a2; ergo_retain_val(__t2617);
  ErgoVal __t2618 = a3; ergo_retain_val(__t2618);
  cogito_container_set_padding(__t2614, __t2615, __t2616, __t2617, __t2618);
  ergo_release_val(__t2614);
  ergo_release_val(__t2615);
  ergo_release_val(__t2616);
  ergo_release_val(__t2617);
  ergo_release_val(__t2618);
  ErgoVal __t2619 = EV_NULLV;
  ergo_release_val(__t2619);
}

static void ergo_m_cogito_Divider_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2620 = self; ergo_retain_val(__t2620);
  ErgoVal __t2621 = a0; ergo_retain_val(__t2621);
  cogito_container_set_align(__t2620, __t2621);
  ergo_release_val(__t2620);
  ergo_release_val(__t2621);
  ErgoVal __t2622 = EV_NULLV;
  ergo_release_val(__t2622);
}

static void ergo_m_cogito_Divider_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2623 = self; ergo_retain_val(__t2623);
  ErgoVal __t2624 = a0; ergo_retain_val(__t2624);
  cogito_container_set_halign(__t2623, __t2624);
  ergo_release_val(__t2623);
  ergo_release_val(__t2624);
  ErgoVal __t2625 = EV_NULLV;
  ergo_release_val(__t2625);
}

static void ergo_m_cogito_Divider_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2626 = self; ergo_retain_val(__t2626);
  ErgoVal __t2627 = a0; ergo_retain_val(__t2627);
  cogito_container_set_valign(__t2626, __t2627);
  ergo_release_val(__t2626);
  ergo_release_val(__t2627);
  ErgoVal __t2628 = EV_NULLV;
  ergo_release_val(__t2628);
}

static void ergo_m_cogito_Divider_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2629 = self; ergo_retain_val(__t2629);
  ErgoVal __t2630 = a0; ergo_retain_val(__t2630);
  cogito_container_set_hexpand(__t2629, __t2630);
  ergo_release_val(__t2629);
  ergo_release_val(__t2630);
  ErgoVal __t2631 = EV_NULLV;
  ergo_release_val(__t2631);
}

static void ergo_m_cogito_Divider_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2632 = self; ergo_retain_val(__t2632);
  ErgoVal __t2633 = a0; ergo_retain_val(__t2633);
  cogito_container_set_vexpand(__t2632, __t2633);
  ergo_release_val(__t2632);
  ergo_release_val(__t2633);
  ErgoVal __t2634 = EV_NULLV;
  ergo_release_val(__t2634);
}

static void ergo_m_cogito_Divider_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2635 = self; ergo_retain_val(__t2635);
  ErgoVal __t2636 = a0; ergo_retain_val(__t2636);
  cogito_node_set_disabled(__t2635, __t2636);
  ergo_release_val(__t2635);
  ergo_release_val(__t2636);
  ErgoVal __t2637 = EV_NULLV;
  ergo_release_val(__t2637);
}

static void ergo_m_cogito_Divider_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2638 = self; ergo_retain_val(__t2638);
  ErgoVal __t2639 = a0; ergo_retain_val(__t2639);
  cogito_node_set_class(__t2638, __t2639);
  ergo_release_val(__t2638);
  ergo_release_val(__t2639);
  ErgoVal __t2640 = EV_NULLV;
  ergo_release_val(__t2640);
}

static void ergo_m_cogito_Divider_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2641 = self; ergo_retain_val(__t2641);
  ErgoVal __t2642 = a0; ergo_retain_val(__t2642);
  cogito_node_set_id(__t2641, __t2642);
  ergo_release_val(__t2641);
  ergo_release_val(__t2642);
  ErgoVal __t2643 = EV_NULLV;
  ergo_release_val(__t2643);
}

static void ergo_m_cogito_TreeView_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2644 = self; ergo_retain_val(__t2644);
  ErgoVal __t2645 = a0; ergo_retain_val(__t2645);
  cogito_container_add(__t2644, __t2645);
  ergo_release_val(__t2644);
  ergo_release_val(__t2645);
  ErgoVal __t2646 = EV_NULLV;
  ergo_release_val(__t2646);
}

static void ergo_m_cogito_TreeView_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t2647 = self; ergo_retain_val(__t2647);
  ErgoVal __t2648 = a0; ergo_retain_val(__t2648);
  ErgoVal __t2649 = a1; ergo_retain_val(__t2649);
  ErgoVal __t2650 = a2; ergo_retain_val(__t2650);
  ErgoVal __t2651 = a3; ergo_retain_val(__t2651);
  cogito_container_set_margins(__t2647, __t2648, __t2649, __t2650, __t2651);
  ergo_release_val(__t2647);
  ergo_release_val(__t2648);
  ergo_release_val(__t2649);
  ergo_release_val(__t2650);
  ergo_release_val(__t2651);
  ErgoVal __t2652 = EV_NULLV;
  ergo_release_val(__t2652);
}

static void ergo_m_cogito_TreeView_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t2653 = self; ergo_retain_val(__t2653);
  ErgoVal __t2654 = a0; ergo_retain_val(__t2654);
  ErgoVal __t2655 = a1; ergo_retain_val(__t2655);
  ErgoVal __t2656 = a2; ergo_retain_val(__t2656);
  ErgoVal __t2657 = a3; ergo_retain_val(__t2657);
  cogito_container_set_padding(__t2653, __t2654, __t2655, __t2656, __t2657);
  ergo_release_val(__t2653);
  ergo_release_val(__t2654);
  ergo_release_val(__t2655);
  ergo_release_val(__t2656);
  ergo_release_val(__t2657);
  ErgoVal __t2658 = EV_NULLV;
  ergo_release_val(__t2658);
}

static void ergo_m_cogito_TreeView_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2659 = self; ergo_retain_val(__t2659);
  ErgoVal __t2660 = a0; ergo_retain_val(__t2660);
  cogito_container_set_align(__t2659, __t2660);
  ergo_release_val(__t2659);
  ergo_release_val(__t2660);
  ErgoVal __t2661 = EV_NULLV;
  ergo_release_val(__t2661);
}

static void ergo_m_cogito_TreeView_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2662 = self; ergo_retain_val(__t2662);
  ErgoVal __t2663 = a0; ergo_retain_val(__t2663);
  cogito_container_set_halign(__t2662, __t2663);
  ergo_release_val(__t2662);
  ergo_release_val(__t2663);
  ErgoVal __t2664 = EV_NULLV;
  ergo_release_val(__t2664);
}

static void ergo_m_cogito_TreeView_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2665 = self; ergo_retain_val(__t2665);
  ErgoVal __t2666 = a0; ergo_retain_val(__t2666);
  cogito_container_set_valign(__t2665, __t2666);
  ergo_release_val(__t2665);
  ergo_release_val(__t2666);
  ErgoVal __t2667 = EV_NULLV;
  ergo_release_val(__t2667);
}

static void ergo_m_cogito_TreeView_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2668 = self; ergo_retain_val(__t2668);
  ErgoVal __t2669 = a0; ergo_retain_val(__t2669);
  cogito_container_set_hexpand(__t2668, __t2669);
  ergo_release_val(__t2668);
  ergo_release_val(__t2669);
  ErgoVal __t2670 = EV_NULLV;
  ergo_release_val(__t2670);
}

static void ergo_m_cogito_TreeView_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2671 = self; ergo_retain_val(__t2671);
  ErgoVal __t2672 = a0; ergo_retain_val(__t2672);
  cogito_container_set_vexpand(__t2671, __t2672);
  ergo_release_val(__t2671);
  ergo_release_val(__t2672);
  ErgoVal __t2673 = EV_NULLV;
  ergo_release_val(__t2673);
}

static void ergo_m_cogito_TreeView_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2674 = self; ergo_retain_val(__t2674);
  ErgoVal __t2675 = a0; ergo_retain_val(__t2675);
  cogito_node_set_class(__t2674, __t2675);
  ergo_release_val(__t2674);
  ergo_release_val(__t2675);
  ErgoVal __t2676 = EV_NULLV;
  ergo_release_val(__t2676);
}

static void ergo_m_cogito_TreeView_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2677 = self; ergo_retain_val(__t2677);
  ErgoVal __t2678 = a0; ergo_retain_val(__t2678);
  cogito_node_set_id(__t2677, __t2678);
  ergo_release_val(__t2677);
  ergo_release_val(__t2678);
  ErgoVal __t2679 = EV_NULLV;
  ergo_release_val(__t2679);
}

static void ergo_m_cogito_ColorPicker_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t2680 = self; ergo_retain_val(__t2680);
  ErgoVal __t2681 = a0; ergo_retain_val(__t2681);
  ErgoVal __t2682 = a1; ergo_retain_val(__t2682);
  ErgoVal __t2683 = a2; ergo_retain_val(__t2683);
  ErgoVal __t2684 = a3; ergo_retain_val(__t2684);
  cogito_container_set_margins(__t2680, __t2681, __t2682, __t2683, __t2684);
  ergo_release_val(__t2680);
  ergo_release_val(__t2681);
  ergo_release_val(__t2682);
  ergo_release_val(__t2683);
  ergo_release_val(__t2684);
  ErgoVal __t2685 = EV_NULLV;
  ergo_release_val(__t2685);
}

static void ergo_m_cogito_ColorPicker_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t2686 = self; ergo_retain_val(__t2686);
  ErgoVal __t2687 = a0; ergo_retain_val(__t2687);
  ErgoVal __t2688 = a1; ergo_retain_val(__t2688);
  ErgoVal __t2689 = a2; ergo_retain_val(__t2689);
  ErgoVal __t2690 = a3; ergo_retain_val(__t2690);
  cogito_container_set_padding(__t2686, __t2687, __t2688, __t2689, __t2690);
  ergo_release_val(__t2686);
  ergo_release_val(__t2687);
  ergo_release_val(__t2688);
  ergo_release_val(__t2689);
  ergo_release_val(__t2690);
  ErgoVal __t2691 = EV_NULLV;
  ergo_release_val(__t2691);
}

static void ergo_m_cogito_ColorPicker_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2692 = self; ergo_retain_val(__t2692);
  ErgoVal __t2693 = a0; ergo_retain_val(__t2693);
  cogito_container_set_align(__t2692, __t2693);
  ergo_release_val(__t2692);
  ergo_release_val(__t2693);
  ErgoVal __t2694 = EV_NULLV;
  ergo_release_val(__t2694);
}

static void ergo_m_cogito_ColorPicker_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2695 = self; ergo_retain_val(__t2695);
  ErgoVal __t2696 = a0; ergo_retain_val(__t2696);
  cogito_container_set_halign(__t2695, __t2696);
  ergo_release_val(__t2695);
  ergo_release_val(__t2696);
  ErgoVal __t2697 = EV_NULLV;
  ergo_release_val(__t2697);
}

static void ergo_m_cogito_ColorPicker_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2698 = self; ergo_retain_val(__t2698);
  ErgoVal __t2699 = a0; ergo_retain_val(__t2699);
  cogito_container_set_valign(__t2698, __t2699);
  ergo_release_val(__t2698);
  ergo_release_val(__t2699);
  ErgoVal __t2700 = EV_NULLV;
  ergo_release_val(__t2700);
}

static void ergo_m_cogito_ColorPicker_set_hex(ErgoVal self, ErgoVal a0) {
}

static ErgoVal ergo_m_cogito_ColorPicker_hex(ErgoVal self) {
  ErgoVal __ret = EV_NULLV;
  return __ret;
}

static void ergo_m_cogito_ColorPicker_on_change(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2701 = self; ergo_retain_val(__t2701);
  ErgoVal __t2702 = a0; ergo_retain_val(__t2702);
  cogito_colorpicker_on_change(__t2701, __t2702);
  ergo_release_val(__t2701);
  ergo_release_val(__t2702);
  ErgoVal __t2703 = EV_NULLV;
  ergo_release_val(__t2703);
}

static void ergo_m_cogito_ColorPicker_set_a11y_label(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2704 = self; ergo_retain_val(__t2704);
  ErgoVal __t2705 = a0; ergo_retain_val(__t2705);
  cogito_node_set_a11y_label(__t2704, __t2705);
  ergo_release_val(__t2704);
  ergo_release_val(__t2705);
  ErgoVal __t2706 = EV_NULLV;
  ergo_release_val(__t2706);
}

static void ergo_m_cogito_ColorPicker_set_a11y_role(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2707 = self; ergo_retain_val(__t2707);
  ErgoVal __t2708 = a0; ergo_retain_val(__t2708);
  cogito_node_set_a11y_role(__t2707, __t2708);
  ergo_release_val(__t2707);
  ergo_release_val(__t2708);
  ErgoVal __t2709 = EV_NULLV;
  ergo_release_val(__t2709);
}

static void ergo_m_cogito_ColorPicker_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2710 = self; ergo_retain_val(__t2710);
  ErgoVal __t2711 = a0; ergo_retain_val(__t2711);
  cogito_container_set_hexpand(__t2710, __t2711);
  ergo_release_val(__t2710);
  ergo_release_val(__t2711);
  ErgoVal __t2712 = EV_NULLV;
  ergo_release_val(__t2712);
}

static void ergo_m_cogito_ColorPicker_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2713 = self; ergo_retain_val(__t2713);
  ErgoVal __t2714 = a0; ergo_retain_val(__t2714);
  cogito_container_set_vexpand(__t2713, __t2714);
  ergo_release_val(__t2713);
  ergo_release_val(__t2714);
  ErgoVal __t2715 = EV_NULLV;
  ergo_release_val(__t2715);
}

static void ergo_m_cogito_ColorPicker_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2716 = self; ergo_retain_val(__t2716);
  ErgoVal __t2717 = a0; ergo_retain_val(__t2717);
  cogito_node_set_disabled(__t2716, __t2717);
  ergo_release_val(__t2716);
  ergo_release_val(__t2717);
  ErgoVal __t2718 = EV_NULLV;
  ergo_release_val(__t2718);
}

static void ergo_m_cogito_ColorPicker_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2719 = self; ergo_retain_val(__t2719);
  ErgoVal __t2720 = a0; ergo_retain_val(__t2720);
  cogito_node_set_class(__t2719, __t2720);
  ergo_release_val(__t2719);
  ergo_release_val(__t2720);
  ErgoVal __t2721 = EV_NULLV;
  ergo_release_val(__t2721);
}

static void ergo_m_cogito_ColorPicker_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2722 = self; ergo_retain_val(__t2722);
  ErgoVal __t2723 = a0; ergo_retain_val(__t2723);
  cogito_node_set_id(__t2722, __t2723);
  ergo_release_val(__t2722);
  ergo_release_val(__t2723);
  ErgoVal __t2724 = EV_NULLV;
  ergo_release_val(__t2724);
}

static void ergo_m_cogito_Toasts_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2725 = self; ergo_retain_val(__t2725);
  ErgoVal __t2726 = a0; ergo_retain_val(__t2726);
  cogito_container_add(__t2725, __t2726);
  ergo_release_val(__t2725);
  ergo_release_val(__t2726);
  ErgoVal __t2727 = EV_NULLV;
  ergo_release_val(__t2727);
}

static void ergo_m_cogito_Toasts_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t2728 = self; ergo_retain_val(__t2728);
  ErgoVal __t2729 = a0; ergo_retain_val(__t2729);
  ErgoVal __t2730 = a1; ergo_retain_val(__t2730);
  ErgoVal __t2731 = a2; ergo_retain_val(__t2731);
  ErgoVal __t2732 = a3; ergo_retain_val(__t2732);
  cogito_container_set_margins(__t2728, __t2729, __t2730, __t2731, __t2732);
  ergo_release_val(__t2728);
  ergo_release_val(__t2729);
  ergo_release_val(__t2730);
  ergo_release_val(__t2731);
  ergo_release_val(__t2732);
  ErgoVal __t2733 = EV_NULLV;
  ergo_release_val(__t2733);
}

static void ergo_m_cogito_Toasts_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t2734 = self; ergo_retain_val(__t2734);
  ErgoVal __t2735 = a0; ergo_retain_val(__t2735);
  ErgoVal __t2736 = a1; ergo_retain_val(__t2736);
  ErgoVal __t2737 = a2; ergo_retain_val(__t2737);
  ErgoVal __t2738 = a3; ergo_retain_val(__t2738);
  cogito_container_set_padding(__t2734, __t2735, __t2736, __t2737, __t2738);
  ergo_release_val(__t2734);
  ergo_release_val(__t2735);
  ergo_release_val(__t2736);
  ergo_release_val(__t2737);
  ergo_release_val(__t2738);
  ErgoVal __t2739 = EV_NULLV;
  ergo_release_val(__t2739);
}

static void ergo_m_cogito_Toasts_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2740 = self; ergo_retain_val(__t2740);
  ErgoVal __t2741 = a0; ergo_retain_val(__t2741);
  cogito_container_set_align(__t2740, __t2741);
  ergo_release_val(__t2740);
  ergo_release_val(__t2741);
  ErgoVal __t2742 = EV_NULLV;
  ergo_release_val(__t2742);
}

static ErgoVal ergo_m_cogito_Toasts_build(ErgoVal self, ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t2743 = self; ergo_retain_val(__t2743);
  ErgoVal __t2744 = a0; ergo_retain_val(__t2744);
  cogito_build(__t2743, __t2744);
  ergo_release_val(__t2743);
  ergo_release_val(__t2744);
  ErgoVal __t2745 = EV_NULLV;
  ergo_release_val(__t2745);
  ErgoVal __t2746 = self; ergo_retain_val(__t2746);
  ergo_move_into(&__ret, __t2746);
  return __ret;
  return __ret;
}

static void ergo_m_cogito_Toasts_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2747 = self; ergo_retain_val(__t2747);
  ErgoVal __t2748 = a0; ergo_retain_val(__t2748);
  cogito_container_set_hexpand(__t2747, __t2748);
  ergo_release_val(__t2747);
  ergo_release_val(__t2748);
  ErgoVal __t2749 = EV_NULLV;
  ergo_release_val(__t2749);
}

static void ergo_m_cogito_Toasts_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2750 = self; ergo_retain_val(__t2750);
  ErgoVal __t2751 = a0; ergo_retain_val(__t2751);
  cogito_container_set_vexpand(__t2750, __t2751);
  ergo_release_val(__t2750);
  ergo_release_val(__t2751);
  ErgoVal __t2752 = EV_NULLV;
  ergo_release_val(__t2752);
}

static void ergo_m_cogito_Toasts_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2753 = self; ergo_retain_val(__t2753);
  ErgoVal __t2754 = a0; ergo_retain_val(__t2754);
  cogito_node_set_disabled(__t2753, __t2754);
  ergo_release_val(__t2753);
  ergo_release_val(__t2754);
  ErgoVal __t2755 = EV_NULLV;
  ergo_release_val(__t2755);
}

static void ergo_m_cogito_Toasts_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2756 = self; ergo_retain_val(__t2756);
  ErgoVal __t2757 = a0; ergo_retain_val(__t2757);
  cogito_node_set_class(__t2756, __t2757);
  ergo_release_val(__t2756);
  ergo_release_val(__t2757);
  ErgoVal __t2758 = EV_NULLV;
  ergo_release_val(__t2758);
}

static void ergo_m_cogito_Toasts_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2759 = self; ergo_retain_val(__t2759);
  ErgoVal __t2760 = a0; ergo_retain_val(__t2760);
  cogito_node_set_id(__t2759, __t2760);
  ergo_release_val(__t2759);
  ergo_release_val(__t2760);
  ErgoVal __t2761 = EV_NULLV;
  ergo_release_val(__t2761);
}

static void ergo_m_cogito_Toast_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t2762 = self; ergo_retain_val(__t2762);
  ErgoVal __t2763 = a0; ergo_retain_val(__t2763);
  ErgoVal __t2764 = a1; ergo_retain_val(__t2764);
  ErgoVal __t2765 = a2; ergo_retain_val(__t2765);
  ErgoVal __t2766 = a3; ergo_retain_val(__t2766);
  cogito_container_set_margins(__t2762, __t2763, __t2764, __t2765, __t2766);
  ergo_release_val(__t2762);
  ergo_release_val(__t2763);
  ergo_release_val(__t2764);
  ergo_release_val(__t2765);
  ergo_release_val(__t2766);
  ErgoVal __t2767 = EV_NULLV;
  ergo_release_val(__t2767);
}

static void ergo_m_cogito_Toast_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t2768 = self; ergo_retain_val(__t2768);
  ErgoVal __t2769 = a0; ergo_retain_val(__t2769);
  ErgoVal __t2770 = a1; ergo_retain_val(__t2770);
  ErgoVal __t2771 = a2; ergo_retain_val(__t2771);
  ErgoVal __t2772 = a3; ergo_retain_val(__t2772);
  cogito_container_set_padding(__t2768, __t2769, __t2770, __t2771, __t2772);
  ergo_release_val(__t2768);
  ergo_release_val(__t2769);
  ergo_release_val(__t2770);
  ergo_release_val(__t2771);
  ergo_release_val(__t2772);
  ErgoVal __t2773 = EV_NULLV;
  ergo_release_val(__t2773);
}

static void ergo_m_cogito_Toast_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2774 = self; ergo_retain_val(__t2774);
  ErgoVal __t2775 = a0; ergo_retain_val(__t2775);
  cogito_container_set_align(__t2774, __t2775);
  ergo_release_val(__t2774);
  ergo_release_val(__t2775);
  ErgoVal __t2776 = EV_NULLV;
  ergo_release_val(__t2776);
}

static void ergo_m_cogito_Toast_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2777 = self; ergo_retain_val(__t2777);
  ErgoVal __t2778 = a0; ergo_retain_val(__t2778);
  cogito_container_set_halign(__t2777, __t2778);
  ergo_release_val(__t2777);
  ergo_release_val(__t2778);
  ErgoVal __t2779 = EV_NULLV;
  ergo_release_val(__t2779);
}

static void ergo_m_cogito_Toast_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2780 = self; ergo_retain_val(__t2780);
  ErgoVal __t2781 = a0; ergo_retain_val(__t2781);
  cogito_container_set_valign(__t2780, __t2781);
  ergo_release_val(__t2780);
  ergo_release_val(__t2781);
  ErgoVal __t2782 = EV_NULLV;
  ergo_release_val(__t2782);
}

static void ergo_m_cogito_Toast_set_text(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2783 = self; ergo_retain_val(__t2783);
  ErgoVal __t2784 = a0; ergo_retain_val(__t2784);
  cogito_toast_set_text(__t2783, __t2784);
  ergo_release_val(__t2783);
  ergo_release_val(__t2784);
  ErgoVal __t2785 = EV_NULLV;
  ergo_release_val(__t2785);
}

static void ergo_m_cogito_Toast_on_click(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2786 = self; ergo_retain_val(__t2786);
  ErgoVal __t2787 = a0; ergo_retain_val(__t2787);
  cogito_toast_on_click(__t2786, __t2787);
  ergo_release_val(__t2786);
  ergo_release_val(__t2787);
  ErgoVal __t2788 = EV_NULLV;
  ergo_release_val(__t2788);
}

static void ergo_m_cogito_Toast_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2789 = self; ergo_retain_val(__t2789);
  ErgoVal __t2790 = a0; ergo_retain_val(__t2790);
  cogito_container_set_hexpand(__t2789, __t2790);
  ergo_release_val(__t2789);
  ergo_release_val(__t2790);
  ErgoVal __t2791 = EV_NULLV;
  ergo_release_val(__t2791);
}

static void ergo_m_cogito_Toast_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2792 = self; ergo_retain_val(__t2792);
  ErgoVal __t2793 = a0; ergo_retain_val(__t2793);
  cogito_container_set_vexpand(__t2792, __t2793);
  ergo_release_val(__t2792);
  ergo_release_val(__t2793);
  ErgoVal __t2794 = EV_NULLV;
  ergo_release_val(__t2794);
}

static void ergo_m_cogito_Toast_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2795 = self; ergo_retain_val(__t2795);
  ErgoVal __t2796 = a0; ergo_retain_val(__t2796);
  cogito_node_set_disabled(__t2795, __t2796);
  ergo_release_val(__t2795);
  ergo_release_val(__t2796);
  ErgoVal __t2797 = EV_NULLV;
  ergo_release_val(__t2797);
}

static void ergo_m_cogito_Toast_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2798 = self; ergo_retain_val(__t2798);
  ErgoVal __t2799 = a0; ergo_retain_val(__t2799);
  cogito_node_set_class(__t2798, __t2799);
  ergo_release_val(__t2798);
  ergo_release_val(__t2799);
  ErgoVal __t2800 = EV_NULLV;
  ergo_release_val(__t2800);
}

static void ergo_m_cogito_Toast_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2801 = self; ergo_retain_val(__t2801);
  ErgoVal __t2802 = a0; ergo_retain_val(__t2802);
  cogito_node_set_id(__t2801, __t2802);
  ergo_release_val(__t2801);
  ergo_release_val(__t2802);
  ErgoVal __t2803 = EV_NULLV;
  ergo_release_val(__t2803);
}

static void ergo_m_cogito_BottomToolbar_add(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2804 = self; ergo_retain_val(__t2804);
  ErgoVal __t2805 = a0; ergo_retain_val(__t2805);
  cogito_container_add(__t2804, __t2805);
  ergo_release_val(__t2804);
  ergo_release_val(__t2805);
  ErgoVal __t2806 = EV_NULLV;
  ergo_release_val(__t2806);
}

static void ergo_m_cogito_BottomToolbar_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2807 = self; ergo_retain_val(__t2807);
  ErgoVal __t2808 = a0; ergo_retain_val(__t2808);
  cogito_container_set_hexpand(__t2807, __t2808);
  ergo_release_val(__t2807);
  ergo_release_val(__t2808);
  ErgoVal __t2809 = EV_NULLV;
  ergo_release_val(__t2809);
}

static void ergo_m_cogito_BottomToolbar_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2810 = self; ergo_retain_val(__t2810);
  ErgoVal __t2811 = a0; ergo_retain_val(__t2811);
  cogito_container_set_vexpand(__t2810, __t2811);
  ergo_release_val(__t2810);
  ergo_release_val(__t2811);
  ErgoVal __t2812 = EV_NULLV;
  ergo_release_val(__t2812);
}

static void ergo_m_cogito_BottomToolbar_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2813 = self; ergo_retain_val(__t2813);
  ErgoVal __t2814 = a0; ergo_retain_val(__t2814);
  cogito_node_set_class(__t2813, __t2814);
  ergo_release_val(__t2813);
  ergo_release_val(__t2814);
  ErgoVal __t2815 = EV_NULLV;
  ergo_release_val(__t2815);
}

static void ergo_m_cogito_BottomToolbar_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2816 = self; ergo_retain_val(__t2816);
  ErgoVal __t2817 = a0; ergo_retain_val(__t2817);
  cogito_node_set_id(__t2816, __t2817);
  ergo_release_val(__t2816);
  ergo_release_val(__t2817);
  ErgoVal __t2818 = EV_NULLV;
  ergo_release_val(__t2818);
}

static void ergo_m_cogito_Chip_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t2819 = self; ergo_retain_val(__t2819);
  ErgoVal __t2820 = a0; ergo_retain_val(__t2820);
  ErgoVal __t2821 = a1; ergo_retain_val(__t2821);
  ErgoVal __t2822 = a2; ergo_retain_val(__t2822);
  ErgoVal __t2823 = a3; ergo_retain_val(__t2823);
  cogito_container_set_margins(__t2819, __t2820, __t2821, __t2822, __t2823);
  ergo_release_val(__t2819);
  ergo_release_val(__t2820);
  ergo_release_val(__t2821);
  ergo_release_val(__t2822);
  ergo_release_val(__t2823);
  ErgoVal __t2824 = EV_NULLV;
  ergo_release_val(__t2824);
}

static void ergo_m_cogito_Chip_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t2825 = self; ergo_retain_val(__t2825);
  ErgoVal __t2826 = a0; ergo_retain_val(__t2826);
  ErgoVal __t2827 = a1; ergo_retain_val(__t2827);
  ErgoVal __t2828 = a2; ergo_retain_val(__t2828);
  ErgoVal __t2829 = a3; ergo_retain_val(__t2829);
  cogito_container_set_padding(__t2825, __t2826, __t2827, __t2828, __t2829);
  ergo_release_val(__t2825);
  ergo_release_val(__t2826);
  ergo_release_val(__t2827);
  ergo_release_val(__t2828);
  ergo_release_val(__t2829);
  ErgoVal __t2830 = EV_NULLV;
  ergo_release_val(__t2830);
}

static void ergo_m_cogito_Chip_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2831 = self; ergo_retain_val(__t2831);
  ErgoVal __t2832 = a0; ergo_retain_val(__t2832);
  cogito_container_set_align(__t2831, __t2832);
  ergo_release_val(__t2831);
  ergo_release_val(__t2832);
  ErgoVal __t2833 = EV_NULLV;
  ergo_release_val(__t2833);
}

static void ergo_m_cogito_Chip_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2834 = self; ergo_retain_val(__t2834);
  ErgoVal __t2835 = a0; ergo_retain_val(__t2835);
  cogito_container_set_halign(__t2834, __t2835);
  ergo_release_val(__t2834);
  ergo_release_val(__t2835);
  ErgoVal __t2836 = EV_NULLV;
  ergo_release_val(__t2836);
}

static void ergo_m_cogito_Chip_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2837 = self; ergo_retain_val(__t2837);
  ErgoVal __t2838 = a0; ergo_retain_val(__t2838);
  cogito_container_set_valign(__t2837, __t2838);
  ergo_release_val(__t2837);
  ergo_release_val(__t2838);
  ErgoVal __t2839 = EV_NULLV;
  ergo_release_val(__t2839);
}

static void ergo_m_cogito_Chip_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2840 = self; ergo_retain_val(__t2840);
  ErgoVal __t2841 = a0; ergo_retain_val(__t2841);
  cogito_container_set_hexpand(__t2840, __t2841);
  ergo_release_val(__t2840);
  ergo_release_val(__t2841);
  ErgoVal __t2842 = EV_NULLV;
  ergo_release_val(__t2842);
}

static void ergo_m_cogito_Chip_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2843 = self; ergo_retain_val(__t2843);
  ErgoVal __t2844 = a0; ergo_retain_val(__t2844);
  cogito_container_set_vexpand(__t2843, __t2844);
  ergo_release_val(__t2843);
  ergo_release_val(__t2844);
  ErgoVal __t2845 = EV_NULLV;
  ergo_release_val(__t2845);
}

static void ergo_m_cogito_Chip_set_selected(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2846 = self; ergo_retain_val(__t2846);
  ErgoVal __t2847 = a0; ergo_retain_val(__t2847);
  cogito_chip_set_selected(__t2846, __t2847);
  ergo_release_val(__t2846);
  ergo_release_val(__t2847);
  ErgoVal __t2848 = EV_NULLV;
  ergo_release_val(__t2848);
}

static ErgoVal ergo_m_cogito_Chip_selected(ErgoVal self) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t2849 = self; ergo_retain_val(__t2849);
  ErgoVal __t2850 = cogito_chip_get_selected(__t2849);
  ergo_release_val(__t2849);
  ergo_move_into(&__ret, __t2850);
  return __ret;
  return __ret;
}

static void ergo_m_cogito_Chip_set_closable(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2851 = self; ergo_retain_val(__t2851);
  ErgoVal __t2852 = a0; ergo_retain_val(__t2852);
  cogito_chip_set_closable(__t2851, __t2852);
  ergo_release_val(__t2851);
  ergo_release_val(__t2852);
  ErgoVal __t2853 = EV_NULLV;
  ergo_release_val(__t2853);
}

static void ergo_m_cogito_Chip_on_click(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2854 = self; ergo_retain_val(__t2854);
  ErgoVal __t2855 = a0; ergo_retain_val(__t2855);
  cogito_chip_on_click(__t2854, __t2855);
  ergo_release_val(__t2854);
  ergo_release_val(__t2855);
  ErgoVal __t2856 = EV_NULLV;
  ergo_release_val(__t2856);
}

static void ergo_m_cogito_Chip_on_close(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2857 = self; ergo_retain_val(__t2857);
  ErgoVal __t2858 = a0; ergo_retain_val(__t2858);
  cogito_chip_on_close(__t2857, __t2858);
  ergo_release_val(__t2857);
  ergo_release_val(__t2858);
  ErgoVal __t2859 = EV_NULLV;
  ergo_release_val(__t2859);
}

static void ergo_m_cogito_Chip_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2860 = self; ergo_retain_val(__t2860);
  ErgoVal __t2861 = a0; ergo_retain_val(__t2861);
  cogito_node_set_disabled(__t2860, __t2861);
  ergo_release_val(__t2860);
  ergo_release_val(__t2861);
  ErgoVal __t2862 = EV_NULLV;
  ergo_release_val(__t2862);
}

static void ergo_m_cogito_Chip_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2863 = self; ergo_retain_val(__t2863);
  ErgoVal __t2864 = a0; ergo_retain_val(__t2864);
  cogito_node_set_class(__t2863, __t2864);
  ergo_release_val(__t2863);
  ergo_release_val(__t2864);
  ErgoVal __t2865 = EV_NULLV;
  ergo_release_val(__t2865);
}

static void ergo_m_cogito_Chip_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2866 = self; ergo_retain_val(__t2866);
  ErgoVal __t2867 = a0; ergo_retain_val(__t2867);
  cogito_node_set_id(__t2866, __t2867);
  ergo_release_val(__t2866);
  ergo_release_val(__t2867);
  ErgoVal __t2868 = EV_NULLV;
  ergo_release_val(__t2868);
}

static void ergo_m_cogito_FAB_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t2869 = self; ergo_retain_val(__t2869);
  ErgoVal __t2870 = a0; ergo_retain_val(__t2870);
  ErgoVal __t2871 = a1; ergo_retain_val(__t2871);
  ErgoVal __t2872 = a2; ergo_retain_val(__t2872);
  ErgoVal __t2873 = a3; ergo_retain_val(__t2873);
  cogito_container_set_margins(__t2869, __t2870, __t2871, __t2872, __t2873);
  ergo_release_val(__t2869);
  ergo_release_val(__t2870);
  ergo_release_val(__t2871);
  ergo_release_val(__t2872);
  ergo_release_val(__t2873);
  ErgoVal __t2874 = EV_NULLV;
  ergo_release_val(__t2874);
}

static void ergo_m_cogito_FAB_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t2875 = self; ergo_retain_val(__t2875);
  ErgoVal __t2876 = a0; ergo_retain_val(__t2876);
  ErgoVal __t2877 = a1; ergo_retain_val(__t2877);
  ErgoVal __t2878 = a2; ergo_retain_val(__t2878);
  ErgoVal __t2879 = a3; ergo_retain_val(__t2879);
  cogito_container_set_padding(__t2875, __t2876, __t2877, __t2878, __t2879);
  ergo_release_val(__t2875);
  ergo_release_val(__t2876);
  ergo_release_val(__t2877);
  ergo_release_val(__t2878);
  ergo_release_val(__t2879);
  ErgoVal __t2880 = EV_NULLV;
  ergo_release_val(__t2880);
}

static void ergo_m_cogito_FAB_set_align(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2881 = self; ergo_retain_val(__t2881);
  ErgoVal __t2882 = a0; ergo_retain_val(__t2882);
  cogito_container_set_align(__t2881, __t2882);
  ergo_release_val(__t2881);
  ergo_release_val(__t2882);
  ErgoVal __t2883 = EV_NULLV;
  ergo_release_val(__t2883);
}

static void ergo_m_cogito_FAB_set_halign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2884 = self; ergo_retain_val(__t2884);
  ErgoVal __t2885 = a0; ergo_retain_val(__t2885);
  cogito_container_set_halign(__t2884, __t2885);
  ergo_release_val(__t2884);
  ergo_release_val(__t2885);
  ErgoVal __t2886 = EV_NULLV;
  ergo_release_val(__t2886);
}

static void ergo_m_cogito_FAB_set_valign(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2887 = self; ergo_retain_val(__t2887);
  ErgoVal __t2888 = a0; ergo_retain_val(__t2888);
  cogito_container_set_valign(__t2887, __t2888);
  ergo_release_val(__t2887);
  ergo_release_val(__t2888);
  ErgoVal __t2889 = EV_NULLV;
  ergo_release_val(__t2889);
}

static void ergo_m_cogito_FAB_align_begin(ErgoVal self) {
  ErgoVal __t2890 = self; ergo_retain_val(__t2890);
  ErgoVal __t2891 = EV_INT(0);
  cogito_container_set_align(__t2890, __t2891);
  ergo_release_val(__t2890);
  ergo_release_val(__t2891);
  ErgoVal __t2892 = EV_NULLV;
  ergo_release_val(__t2892);
}

static void ergo_m_cogito_FAB_align_center(ErgoVal self) {
  ErgoVal __t2893 = self; ergo_retain_val(__t2893);
  ErgoVal __t2894 = EV_INT(1);
  cogito_container_set_align(__t2893, __t2894);
  ergo_release_val(__t2893);
  ergo_release_val(__t2894);
  ErgoVal __t2895 = EV_NULLV;
  ergo_release_val(__t2895);
}

static void ergo_m_cogito_FAB_align_end(ErgoVal self) {
  ErgoVal __t2896 = self; ergo_retain_val(__t2896);
  ErgoVal __t2897 = EV_INT(2);
  cogito_container_set_align(__t2896, __t2897);
  ergo_release_val(__t2896);
  ergo_release_val(__t2897);
  ErgoVal __t2898 = EV_NULLV;
  ergo_release_val(__t2898);
}

static void ergo_m_cogito_FAB_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2899 = self; ergo_retain_val(__t2899);
  ErgoVal __t2900 = a0; ergo_retain_val(__t2900);
  cogito_container_set_hexpand(__t2899, __t2900);
  ergo_release_val(__t2899);
  ergo_release_val(__t2900);
  ErgoVal __t2901 = EV_NULLV;
  ergo_release_val(__t2901);
}

static void ergo_m_cogito_FAB_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2902 = self; ergo_retain_val(__t2902);
  ErgoVal __t2903 = a0; ergo_retain_val(__t2903);
  cogito_container_set_vexpand(__t2902, __t2903);
  ergo_release_val(__t2902);
  ergo_release_val(__t2903);
  ErgoVal __t2904 = EV_NULLV;
  ergo_release_val(__t2904);
}

static void ergo_m_cogito_FAB_set_extended(ErgoVal self, ErgoVal a0, ErgoVal a1) {
  ErgoVal __t2905 = self; ergo_retain_val(__t2905);
  ErgoVal __t2906 = a0; ergo_retain_val(__t2906);
  ErgoVal __t2907 = a1; ergo_retain_val(__t2907);
  cogito_fab_set_extended(__t2905, __t2906, __t2907);
  ergo_release_val(__t2905);
  ergo_release_val(__t2906);
  ergo_release_val(__t2907);
  ErgoVal __t2908 = EV_NULLV;
  ergo_release_val(__t2908);
}

static void ergo_m_cogito_FAB_on_click(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2909 = self; ergo_retain_val(__t2909);
  ErgoVal __t2910 = a0; ergo_retain_val(__t2910);
  cogito_fab_on_click(__t2909, __t2910);
  ergo_release_val(__t2909);
  ergo_release_val(__t2910);
  ErgoVal __t2911 = EV_NULLV;
  ergo_release_val(__t2911);
}

static void ergo_m_cogito_FAB_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2912 = self; ergo_retain_val(__t2912);
  ErgoVal __t2913 = a0; ergo_retain_val(__t2913);
  cogito_node_set_disabled(__t2912, __t2913);
  ergo_release_val(__t2912);
  ergo_release_val(__t2913);
  ErgoVal __t2914 = EV_NULLV;
  ergo_release_val(__t2914);
}

static void ergo_m_cogito_FAB_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2915 = self; ergo_retain_val(__t2915);
  ErgoVal __t2916 = a0; ergo_retain_val(__t2916);
  cogito_node_set_class(__t2915, __t2916);
  ergo_release_val(__t2915);
  ergo_release_val(__t2916);
  ErgoVal __t2917 = EV_NULLV;
  ergo_release_val(__t2917);
}

static void ergo_m_cogito_FAB_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2918 = self; ergo_retain_val(__t2918);
  ErgoVal __t2919 = a0; ergo_retain_val(__t2919);
  cogito_node_set_id(__t2918, __t2919);
  ergo_release_val(__t2918);
  ergo_release_val(__t2919);
  ErgoVal __t2920 = EV_NULLV;
  ergo_release_val(__t2920);
}

static void ergo_m_cogito_NavRail_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t2921 = self; ergo_retain_val(__t2921);
  ErgoVal __t2922 = a0; ergo_retain_val(__t2922);
  ErgoVal __t2923 = a1; ergo_retain_val(__t2923);
  ErgoVal __t2924 = a2; ergo_retain_val(__t2924);
  ErgoVal __t2925 = a3; ergo_retain_val(__t2925);
  cogito_container_set_margins(__t2921, __t2922, __t2923, __t2924, __t2925);
  ergo_release_val(__t2921);
  ergo_release_val(__t2922);
  ergo_release_val(__t2923);
  ergo_release_val(__t2924);
  ergo_release_val(__t2925);
  ErgoVal __t2926 = EV_NULLV;
  ergo_release_val(__t2926);
}

static void ergo_m_cogito_NavRail_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t2927 = self; ergo_retain_val(__t2927);
  ErgoVal __t2928 = a0; ergo_retain_val(__t2928);
  ErgoVal __t2929 = a1; ergo_retain_val(__t2929);
  ErgoVal __t2930 = a2; ergo_retain_val(__t2930);
  ErgoVal __t2931 = a3; ergo_retain_val(__t2931);
  cogito_container_set_padding(__t2927, __t2928, __t2929, __t2930, __t2931);
  ergo_release_val(__t2927);
  ergo_release_val(__t2928);
  ergo_release_val(__t2929);
  ergo_release_val(__t2930);
  ergo_release_val(__t2931);
  ErgoVal __t2932 = EV_NULLV;
  ergo_release_val(__t2932);
}

static void ergo_m_cogito_NavRail_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2933 = self; ergo_retain_val(__t2933);
  ErgoVal __t2934 = a0; ergo_retain_val(__t2934);
  cogito_container_set_hexpand(__t2933, __t2934);
  ergo_release_val(__t2933);
  ergo_release_val(__t2934);
  ErgoVal __t2935 = EV_NULLV;
  ergo_release_val(__t2935);
}

static void ergo_m_cogito_NavRail_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2936 = self; ergo_retain_val(__t2936);
  ErgoVal __t2937 = a0; ergo_retain_val(__t2937);
  cogito_container_set_vexpand(__t2936, __t2937);
  ergo_release_val(__t2936);
  ergo_release_val(__t2937);
  ErgoVal __t2938 = EV_NULLV;
  ergo_release_val(__t2938);
}

static void ergo_m_cogito_NavRail_set_items(ErgoVal self, ErgoVal a0, ErgoVal a1) {
  ErgoVal __t2939 = self; ergo_retain_val(__t2939);
  ErgoVal __t2940 = a0; ergo_retain_val(__t2940);
  ErgoVal __t2941 = a1; ergo_retain_val(__t2941);
  cogito_nav_rail_set_items(__t2939, __t2940, __t2941);
  ergo_release_val(__t2939);
  ergo_release_val(__t2940);
  ergo_release_val(__t2941);
  ErgoVal __t2942 = EV_NULLV;
  ergo_release_val(__t2942);
}

static void ergo_m_cogito_NavRail_set_selected(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2943 = self; ergo_retain_val(__t2943);
  ErgoVal __t2944 = a0; ergo_retain_val(__t2944);
  cogito_nav_rail_set_selected(__t2943, __t2944);
  ergo_release_val(__t2943);
  ergo_release_val(__t2944);
  ErgoVal __t2945 = EV_NULLV;
  ergo_release_val(__t2945);
}

static ErgoVal ergo_m_cogito_NavRail_selected(ErgoVal self) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t2946 = self; ergo_retain_val(__t2946);
  ErgoVal __t2947 = cogito_nav_rail_get_selected(__t2946);
  ergo_release_val(__t2946);
  ergo_move_into(&__ret, __t2947);
  return __ret;
  return __ret;
}

static void ergo_m_cogito_NavRail_on_change(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2948 = self; ergo_retain_val(__t2948);
  ErgoVal __t2949 = a0; ergo_retain_val(__t2949);
  cogito_nav_rail_on_change(__t2948, __t2949);
  ergo_release_val(__t2948);
  ergo_release_val(__t2949);
  ErgoVal __t2950 = EV_NULLV;
  ergo_release_val(__t2950);
}

static void ergo_m_cogito_NavRail_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2951 = self; ergo_retain_val(__t2951);
  ErgoVal __t2952 = a0; ergo_retain_val(__t2952);
  cogito_node_set_disabled(__t2951, __t2952);
  ergo_release_val(__t2951);
  ergo_release_val(__t2952);
  ErgoVal __t2953 = EV_NULLV;
  ergo_release_val(__t2953);
}

static void ergo_m_cogito_NavRail_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2954 = self; ergo_retain_val(__t2954);
  ErgoVal __t2955 = a0; ergo_retain_val(__t2955);
  cogito_node_set_class(__t2954, __t2955);
  ergo_release_val(__t2954);
  ergo_release_val(__t2955);
  ErgoVal __t2956 = EV_NULLV;
  ergo_release_val(__t2956);
}

static void ergo_m_cogito_NavRail_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2957 = self; ergo_retain_val(__t2957);
  ErgoVal __t2958 = a0; ergo_retain_val(__t2958);
  cogito_node_set_id(__t2957, __t2958);
  ergo_release_val(__t2957);
  ergo_release_val(__t2958);
  ErgoVal __t2959 = EV_NULLV;
  ergo_release_val(__t2959);
}

static void ergo_m_cogito_BottomNav_set_margins(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t2960 = self; ergo_retain_val(__t2960);
  ErgoVal __t2961 = a0; ergo_retain_val(__t2961);
  ErgoVal __t2962 = a1; ergo_retain_val(__t2962);
  ErgoVal __t2963 = a2; ergo_retain_val(__t2963);
  ErgoVal __t2964 = a3; ergo_retain_val(__t2964);
  cogito_container_set_margins(__t2960, __t2961, __t2962, __t2963, __t2964);
  ergo_release_val(__t2960);
  ergo_release_val(__t2961);
  ergo_release_val(__t2962);
  ergo_release_val(__t2963);
  ergo_release_val(__t2964);
  ErgoVal __t2965 = EV_NULLV;
  ergo_release_val(__t2965);
}

static void ergo_m_cogito_BottomNav_set_padding(ErgoVal self, ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __t2966 = self; ergo_retain_val(__t2966);
  ErgoVal __t2967 = a0; ergo_retain_val(__t2967);
  ErgoVal __t2968 = a1; ergo_retain_val(__t2968);
  ErgoVal __t2969 = a2; ergo_retain_val(__t2969);
  ErgoVal __t2970 = a3; ergo_retain_val(__t2970);
  cogito_container_set_padding(__t2966, __t2967, __t2968, __t2969, __t2970);
  ergo_release_val(__t2966);
  ergo_release_val(__t2967);
  ergo_release_val(__t2968);
  ergo_release_val(__t2969);
  ergo_release_val(__t2970);
  ErgoVal __t2971 = EV_NULLV;
  ergo_release_val(__t2971);
}

static void ergo_m_cogito_BottomNav_set_hexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2972 = self; ergo_retain_val(__t2972);
  ErgoVal __t2973 = a0; ergo_retain_val(__t2973);
  cogito_container_set_hexpand(__t2972, __t2973);
  ergo_release_val(__t2972);
  ergo_release_val(__t2973);
  ErgoVal __t2974 = EV_NULLV;
  ergo_release_val(__t2974);
}

static void ergo_m_cogito_BottomNav_set_vexpand(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2975 = self; ergo_retain_val(__t2975);
  ErgoVal __t2976 = a0; ergo_retain_val(__t2976);
  cogito_container_set_vexpand(__t2975, __t2976);
  ergo_release_val(__t2975);
  ergo_release_val(__t2976);
  ErgoVal __t2977 = EV_NULLV;
  ergo_release_val(__t2977);
}

static void ergo_m_cogito_BottomNav_set_items(ErgoVal self, ErgoVal a0, ErgoVal a1) {
  ErgoVal __t2978 = self; ergo_retain_val(__t2978);
  ErgoVal __t2979 = a0; ergo_retain_val(__t2979);
  ErgoVal __t2980 = a1; ergo_retain_val(__t2980);
  cogito_bottom_nav_set_items(__t2978, __t2979, __t2980);
  ergo_release_val(__t2978);
  ergo_release_val(__t2979);
  ergo_release_val(__t2980);
  ErgoVal __t2981 = EV_NULLV;
  ergo_release_val(__t2981);
}

static void ergo_m_cogito_BottomNav_set_selected(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2982 = self; ergo_retain_val(__t2982);
  ErgoVal __t2983 = a0; ergo_retain_val(__t2983);
  cogito_bottom_nav_set_selected(__t2982, __t2983);
  ergo_release_val(__t2982);
  ergo_release_val(__t2983);
  ErgoVal __t2984 = EV_NULLV;
  ergo_release_val(__t2984);
}

static ErgoVal ergo_m_cogito_BottomNav_selected(ErgoVal self) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t2985 = self; ergo_retain_val(__t2985);
  ErgoVal __t2986 = cogito_bottom_nav_get_selected(__t2985);
  ergo_release_val(__t2985);
  ergo_move_into(&__ret, __t2986);
  return __ret;
  return __ret;
}

static void ergo_m_cogito_BottomNav_on_change(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2987 = self; ergo_retain_val(__t2987);
  ErgoVal __t2988 = a0; ergo_retain_val(__t2988);
  cogito_bottom_nav_on_change(__t2987, __t2988);
  ergo_release_val(__t2987);
  ergo_release_val(__t2988);
  ErgoVal __t2989 = EV_NULLV;
  ergo_release_val(__t2989);
}

static void ergo_m_cogito_BottomNav_set_disabled(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2990 = self; ergo_retain_val(__t2990);
  ErgoVal __t2991 = a0; ergo_retain_val(__t2991);
  cogito_node_set_disabled(__t2990, __t2991);
  ergo_release_val(__t2990);
  ergo_release_val(__t2991);
  ErgoVal __t2992 = EV_NULLV;
  ergo_release_val(__t2992);
}

static void ergo_m_cogito_BottomNav_set_class(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2993 = self; ergo_retain_val(__t2993);
  ErgoVal __t2994 = a0; ergo_retain_val(__t2994);
  cogito_node_set_class(__t2993, __t2994);
  ergo_release_val(__t2993);
  ergo_release_val(__t2994);
  ErgoVal __t2995 = EV_NULLV;
  ergo_release_val(__t2995);
}

static void ergo_m_cogito_BottomNav_set_id(ErgoVal self, ErgoVal a0) {
  ErgoVal __t2996 = self; ergo_retain_val(__t2996);
  ErgoVal __t2997 = a0; ergo_retain_val(__t2997);
  cogito_node_set_id(__t2996, __t2997);
  ergo_release_val(__t2996);
  ergo_release_val(__t2997);
  ErgoVal __t2998 = EV_NULLV;
  ergo_release_val(__t2998);
}

static ErgoVal ergo_m_cogito_State_get(ErgoVal self) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t2999 = self; ergo_retain_val(__t2999);
  ErgoVal __t3000 = cogito_state_get(__t2999);
  ergo_release_val(__t2999);
  ergo_move_into(&__ret, __t3000);
  return __ret;
  return __ret;
}

static void ergo_m_cogito_State_set(ErgoVal self, ErgoVal a0) {
  ErgoVal __t3001 = self; ergo_retain_val(__t3001);
  ErgoVal __t3002 = a0; ergo_retain_val(__t3002);
  cogito_state_set(__t3001, __t3002);
  ergo_release_val(__t3001);
  ergo_release_val(__t3002);
  ErgoVal __t3003 = EV_NULLV;
  ergo_release_val(__t3003);
}

static ErgoVal ergo_cogito_app(void) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3004 = cogito_app_new();
  ergo_move_into(&__ret, __t3004);
  return __ret;
  return __ret;
}

static void ergo_cogito_load_sum(ErgoVal a0) {
  ErgoVal __t3005 = a0; ergo_retain_val(__t3005);
  cogito_load_sum(__t3005);
  ergo_release_val(__t3005);
  ErgoVal __t3006 = EV_NULLV;
  ergo_release_val(__t3006);
}

static void ergo_cogito_set_script_dir(ErgoVal a0) {
  ErgoVal __t3007 = a0; ergo_retain_val(__t3007);
  ergo_cogito___cogito_set_script_dir(__t3007);
  ergo_release_val(__t3007);
  ErgoVal __t3008 = EV_NULLV;
  ergo_release_val(__t3008);
}

static ErgoVal ergo_cogito_open_url(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3009 = a0; ergo_retain_val(__t3009);
  ErgoVal __t3010 = cogito_open_url(__t3009);
  ergo_release_val(__t3009);
  ergo_move_into(&__ret, __t3010);
  return __ret;
  return __ret;
}

static void ergo_cogito_set_class(ErgoVal a0, ErgoVal a1) {
  ErgoVal __t3011 = a0; ergo_retain_val(__t3011);
  ErgoVal __t3012 = a1; ergo_retain_val(__t3012);
  cogito_node_set_class(__t3011, __t3012);
  ergo_release_val(__t3011);
  ergo_release_val(__t3012);
  ErgoVal __t3013 = EV_NULLV;
  ergo_release_val(__t3013);
}

static void ergo_cogito_set_a11y_label(ErgoVal a0, ErgoVal a1) {
  ErgoVal __t3014 = a0; ergo_retain_val(__t3014);
  ErgoVal __t3015 = a1; ergo_retain_val(__t3015);
  cogito_node_set_a11y_label(__t3014, __t3015);
  ergo_release_val(__t3014);
  ergo_release_val(__t3015);
  ErgoVal __t3016 = EV_NULLV;
  ergo_release_val(__t3016);
}

static void ergo_cogito_set_a11y_role(ErgoVal a0, ErgoVal a1) {
  ErgoVal __t3017 = a0; ergo_retain_val(__t3017);
  ErgoVal __t3018 = a1; ergo_retain_val(__t3018);
  cogito_node_set_a11y_role(__t3017, __t3018);
  ergo_release_val(__t3017);
  ergo_release_val(__t3018);
  ErgoVal __t3019 = EV_NULLV;
  ergo_release_val(__t3019);
}

static void ergo_cogito_set_tooltip(ErgoVal a0, ErgoVal a1) {
  ErgoVal __t3020 = a0; ergo_retain_val(__t3020);
  ErgoVal __t3021 = a1; ergo_retain_val(__t3021);
  cogito_node_set_tooltip_val(__t3020, __t3021);
  ergo_release_val(__t3020);
  ergo_release_val(__t3021);
  ErgoVal __t3022 = EV_NULLV;
  ergo_release_val(__t3022);
}

static void ergo_cogito_pointer_capture(ErgoVal a0) {
  ErgoVal __t3023 = a0; ergo_retain_val(__t3023);
  cogito_pointer_capture_set(__t3023);
  ergo_release_val(__t3023);
  ErgoVal __t3024 = EV_NULLV;
  ergo_release_val(__t3024);
}

static void ergo_cogito_pointer_release(void) {
  cogito_pointer_capture_clear();
  ErgoVal __t3025 = EV_NULLV;
  ergo_release_val(__t3025);
}

static ErgoVal ergo_cogito_window(void) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3026 = EV_STR(stdr_str_lit("Cogito"));
  ErgoVal __t3027 = EV_NULLV;
  {
    ErgoVal __parts214[1] = { __t3026 };
    ErgoStr* __s215 = stdr_str_from_parts(1, __parts214);
    __t3027 = EV_STR(__s215);
  }
  ergo_release_val(__t3026);
  ErgoVal __t3028 = EV_INT(360);
  ErgoVal __t3029 = EV_INT(296);
  ErgoVal __t3030 = cogito_window_new(__t3027, __t3028, __t3029);
  ergo_release_val(__t3027);
  ergo_release_val(__t3028);
  ergo_release_val(__t3029);
  ergo_move_into(&__ret, __t3030);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_window_title(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3031 = a0; ergo_retain_val(__t3031);
  ErgoVal __t3032 = EV_INT(360);
  ErgoVal __t3033 = EV_INT(296);
  ErgoVal __t3034 = cogito_window_new(__t3031, __t3032, __t3033);
  ergo_release_val(__t3031);
  ergo_release_val(__t3032);
  ergo_release_val(__t3033);
  ergo_move_into(&__ret, __t3034);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_window_size(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3035 = a0; ergo_retain_val(__t3035);
  ErgoVal __t3036 = a1; ergo_retain_val(__t3036);
  ErgoVal __t3037 = a2; ergo_retain_val(__t3037);
  ErgoVal __t3038 = cogito_window_new(__t3035, __t3036, __t3037);
  ergo_release_val(__t3035);
  ergo_release_val(__t3036);
  ergo_release_val(__t3037);
  ergo_move_into(&__ret, __t3038);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_about_window(ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3, ErgoVal a4) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal win__143 = EV_NULLV;
  ErgoVal __t3039 = EV_STR(stdr_str_lit("About"));
  ErgoVal __t3040 = EV_NULLV;
  {
    ErgoVal __parts216[1] = { __t3039 };
    ErgoStr* __s217 = stdr_str_from_parts(1, __parts216);
    __t3040 = EV_STR(__s217);
  }
  ergo_release_val(__t3039);
  ErgoVal __t3041 = EV_INT(420);
  ErgoVal __t3042 = EV_INT(420);
  ErgoVal __t3043 = ergo_cogito_window_size(__t3040, __t3041, __t3042);
  ergo_release_val(__t3040);
  ergo_release_val(__t3041);
  ergo_release_val(__t3042);
  ergo_move_into(&win__143, __t3043);
  ErgoVal __t3044 = win__143; ergo_retain_val(__t3044);
  ErgoVal __t3045 = EV_BOOL(false);
  ergo_m_cogito_Window_set_resizable(__t3044, __t3045);
  ergo_release_val(__t3044);
  ergo_release_val(__t3045);
  ErgoVal __t3046 = EV_NULLV;
  ergo_release_val(__t3046);
  ErgoVal root__144 = EV_NULLV;
  ErgoVal __t3047 = ergo_cogito_vstack();
  ergo_move_into(&root__144, __t3047);
  ErgoVal __t3048 = root__144; ergo_retain_val(__t3048);
  ErgoVal __t3049 = EV_INT(24);
  ErgoVal __t3050 = EV_INT(24);
  ErgoVal __t3051 = EV_INT(24);
  ErgoVal __t3052 = EV_INT(24);
  ergo_m_cogito_VStack_set_padding(__t3048, __t3049, __t3050, __t3051, __t3052);
  ergo_release_val(__t3048);
  ergo_release_val(__t3049);
  ergo_release_val(__t3050);
  ergo_release_val(__t3051);
  ergo_release_val(__t3052);
  ErgoVal __t3053 = EV_NULLV;
  ergo_release_val(__t3053);
  ErgoVal __t3054 = root__144; ergo_retain_val(__t3054);
  ErgoVal __t3055 = EV_INT(12);
  ergo_m_cogito_VStack_set_gap(__t3054, __t3055);
  ergo_release_val(__t3054);
  ergo_release_val(__t3055);
  ErgoVal __t3056 = EV_NULLV;
  ergo_release_val(__t3056);
  ErgoVal __t3057 = root__144; ergo_retain_val(__t3057);
  ergo_m_cogito_VStack_align_center(__t3057);
  ergo_release_val(__t3057);
  ErgoVal __t3058 = EV_NULLV;
  ergo_release_val(__t3058);
  ErgoVal __t3059 = root__144; ergo_retain_val(__t3059);
  ErgoVal __t3060 = EV_STR(stdr_str_lit("about-window"));
  ErgoVal __t3061 = EV_NULLV;
  {
    ErgoVal __parts218[1] = { __t3060 };
    ErgoStr* __s219 = stdr_str_from_parts(1, __parts218);
    __t3061 = EV_STR(__s219);
  }
  ergo_release_val(__t3060);
  ergo_cogito_set_class(__t3059, __t3061);
  ergo_release_val(__t3059);
  ergo_release_val(__t3061);
  ErgoVal __t3062 = EV_NULLV;
  ergo_release_val(__t3062);
  ErgoVal icon__145 = EV_NULLV;
  ErgoVal __t3063 = a0; ergo_retain_val(__t3063);
  ErgoVal __t3064 = ergo_cogito_image(__t3063);
  ergo_release_val(__t3063);
  ergo_move_into(&icon__145, __t3064);
  ErgoVal __t3065 = icon__145; ergo_retain_val(__t3065);
  ErgoVal __t3066 = EV_STR(stdr_str_lit("about-window-icon"));
  ErgoVal __t3067 = EV_NULLV;
  {
    ErgoVal __parts220[1] = { __t3066 };
    ErgoStr* __s221 = stdr_str_from_parts(1, __parts220);
    __t3067 = EV_STR(__s221);
  }
  ergo_release_val(__t3066);
  ergo_cogito_set_class(__t3065, __t3067);
  ergo_release_val(__t3065);
  ergo_release_val(__t3067);
  ErgoVal __t3068 = EV_NULLV;
  ergo_release_val(__t3068);
  ErgoVal __t3069 = root__144; ergo_retain_val(__t3069);
  ErgoVal __t3070 = icon__145; ergo_retain_val(__t3070);
  ergo_m_cogito_VStack_add(__t3069, __t3070);
  ergo_release_val(__t3069);
  ergo_release_val(__t3070);
  ErgoVal __t3071 = EV_NULLV;
  ergo_release_val(__t3071);
  ErgoVal name_label__146 = EV_NULLV;
  ErgoVal __t3072 = a1; ergo_retain_val(__t3072);
  ErgoVal __t3073 = ergo_cogito_label(__t3072);
  ergo_release_val(__t3072);
  ergo_move_into(&name_label__146, __t3073);
  ErgoVal __t3074 = name_label__146; ergo_retain_val(__t3074);
  ErgoVal __t3075 = EV_STR(stdr_str_lit("about-window-title"));
  ErgoVal __t3076 = EV_NULLV;
  {
    ErgoVal __parts222[1] = { __t3075 };
    ErgoStr* __s223 = stdr_str_from_parts(1, __parts222);
    __t3076 = EV_STR(__s223);
  }
  ergo_release_val(__t3075);
  ergo_cogito_set_class(__t3074, __t3076);
  ergo_release_val(__t3074);
  ergo_release_val(__t3076);
  ErgoVal __t3077 = EV_NULLV;
  ergo_release_val(__t3077);
  ErgoVal __t3078 = name_label__146; ergo_retain_val(__t3078);
  ErgoVal __t3079 = EV_INT(1);
  ergo_m_cogito_Label_set_text_align(__t3078, __t3079);
  ergo_release_val(__t3078);
  ergo_release_val(__t3079);
  ErgoVal __t3080 = EV_NULLV;
  ergo_release_val(__t3080);
  ErgoVal __t3081 = root__144; ergo_retain_val(__t3081);
  ErgoVal __t3082 = name_label__146; ergo_retain_val(__t3082);
  ergo_m_cogito_VStack_add(__t3081, __t3082);
  ergo_release_val(__t3081);
  ergo_release_val(__t3082);
  ErgoVal __t3083 = EV_NULLV;
  ergo_release_val(__t3083);
  ErgoVal license_label__147 = EV_NULLV;
  ErgoVal __t3084 = a2; ergo_retain_val(__t3084);
  ErgoVal __t3085 = ergo_cogito_label(__t3084);
  ergo_release_val(__t3084);
  ergo_move_into(&license_label__147, __t3085);
  ErgoVal __t3086 = license_label__147; ergo_retain_val(__t3086);
  ErgoVal __t3087 = EV_STR(stdr_str_lit("about-window-license"));
  ErgoVal __t3088 = EV_NULLV;
  {
    ErgoVal __parts224[1] = { __t3087 };
    ErgoStr* __s225 = stdr_str_from_parts(1, __parts224);
    __t3088 = EV_STR(__s225);
  }
  ergo_release_val(__t3087);
  ergo_cogito_set_class(__t3086, __t3088);
  ergo_release_val(__t3086);
  ergo_release_val(__t3088);
  ErgoVal __t3089 = EV_NULLV;
  ergo_release_val(__t3089);
  ErgoVal __t3090 = license_label__147; ergo_retain_val(__t3090);
  ErgoVal __t3091 = EV_INT(1);
  ergo_m_cogito_Label_set_text_align(__t3090, __t3091);
  ergo_release_val(__t3090);
  ergo_release_val(__t3091);
  ErgoVal __t3092 = EV_NULLV;
  ergo_release_val(__t3092);
  ErgoVal __t3093 = root__144; ergo_retain_val(__t3093);
  ErgoVal __t3094 = license_label__147; ergo_retain_val(__t3094);
  ergo_m_cogito_VStack_add(__t3093, __t3094);
  ergo_release_val(__t3093);
  ergo_release_val(__t3094);
  ErgoVal __t3095 = EV_NULLV;
  ergo_release_val(__t3095);
  ErgoVal actions__148 = EV_NULLV;
  ErgoVal __t3096 = ergo_cogito_hstack();
  ergo_move_into(&actions__148, __t3096);
  ErgoVal __t3097 = actions__148; ergo_retain_val(__t3097);
  ErgoVal __t3098 = EV_INT(10);
  ergo_m_cogito_HStack_set_gap(__t3097, __t3098);
  ergo_release_val(__t3097);
  ergo_release_val(__t3098);
  ErgoVal __t3099 = EV_NULLV;
  ergo_release_val(__t3099);
  ErgoVal __t3100 = actions__148; ergo_retain_val(__t3100);
  ergo_m_cogito_HStack_align_center(__t3100);
  ergo_release_val(__t3100);
  ErgoVal __t3101 = EV_NULLV;
  ergo_release_val(__t3101);
  ErgoVal __t3102 = actions__148; ergo_retain_val(__t3102);
  ErgoVal __t3103 = EV_STR(stdr_str_lit("about-window-actions"));
  ErgoVal __t3104 = EV_NULLV;
  {
    ErgoVal __parts226[1] = { __t3103 };
    ErgoStr* __s227 = stdr_str_from_parts(1, __parts226);
    __t3104 = EV_STR(__s227);
  }
  ergo_release_val(__t3103);
  ergo_cogito_set_class(__t3102, __t3104);
  ergo_release_val(__t3102);
  ergo_release_val(__t3104);
  ErgoVal __t3105 = EV_NULLV;
  ergo_release_val(__t3105);
  ErgoVal more_btn__149 = EV_NULLV;
  ErgoVal __t3106 = EV_STR(stdr_str_lit("More info"));
  ErgoVal __t3107 = EV_NULLV;
  {
    ErgoVal __parts228[1] = { __t3106 };
    ErgoStr* __s229 = stdr_str_from_parts(1, __parts228);
    __t3107 = EV_STR(__s229);
  }
  ergo_release_val(__t3106);
  ErgoVal __t3108 = ergo_cogito_button(__t3107);
  ergo_release_val(__t3107);
  ergo_move_into(&more_btn__149, __t3108);
  ErgoVal __t3109 = more_btn__149; ergo_retain_val(__t3109);
  ErgoVal __t3110 = EV_STR(stdr_str_lit("outlined"));
  ErgoVal __t3111 = EV_NULLV;
  {
    ErgoVal __parts230[1] = { __t3110 };
    ErgoStr* __s231 = stdr_str_from_parts(1, __parts230);
    __t3111 = EV_STR(__s231);
  }
  ergo_release_val(__t3110);
  ergo_cogito_set_class(__t3109, __t3111);
  ergo_release_val(__t3109);
  ergo_release_val(__t3111);
  ErgoVal __t3112 = EV_NULLV;
  ergo_release_val(__t3112);
  ErgoVal __t3113 = more_btn__149; ergo_retain_val(__t3113);
  ErgoVal* __env232 = (ErgoVal*)malloc(sizeof(ErgoVal) * 1);
  __env232[0] = a3; ergo_retain_val(__env232[0]);
  ErgoVal __t3114 = EV_FN(ergo_fn_new_with_env(ergo_lambda_14, 1, __env232, 1));
  ergo_m_cogito_Button_on_click(__t3113, __t3114);
  ergo_release_val(__t3113);
  ergo_release_val(__t3114);
  ErgoVal __t3115 = EV_NULLV;
  ergo_release_val(__t3115);
  ErgoVal __t3116 = actions__148; ergo_retain_val(__t3116);
  ErgoVal __t3117 = more_btn__149; ergo_retain_val(__t3117);
  ergo_m_cogito_HStack_add(__t3116, __t3117);
  ergo_release_val(__t3116);
  ergo_release_val(__t3117);
  ErgoVal __t3118 = EV_NULLV;
  ergo_release_val(__t3118);
  ErgoVal bug_btn__150 = EV_NULLV;
  ErgoVal __t3119 = EV_STR(stdr_str_lit("Report a Bug"));
  ErgoVal __t3120 = EV_NULLV;
  {
    ErgoVal __parts233[1] = { __t3119 };
    ErgoStr* __s234 = stdr_str_from_parts(1, __parts233);
    __t3120 = EV_STR(__s234);
  }
  ergo_release_val(__t3119);
  ErgoVal __t3121 = ergo_cogito_button(__t3120);
  ergo_release_val(__t3120);
  ergo_move_into(&bug_btn__150, __t3121);
  ErgoVal __t3122 = bug_btn__150; ergo_retain_val(__t3122);
  ErgoVal __t3123 = EV_STR(stdr_str_lit("outlined"));
  ErgoVal __t3124 = EV_NULLV;
  {
    ErgoVal __parts235[1] = { __t3123 };
    ErgoStr* __s236 = stdr_str_from_parts(1, __parts235);
    __t3124 = EV_STR(__s236);
  }
  ergo_release_val(__t3123);
  ergo_cogito_set_class(__t3122, __t3124);
  ergo_release_val(__t3122);
  ergo_release_val(__t3124);
  ErgoVal __t3125 = EV_NULLV;
  ergo_release_val(__t3125);
  ErgoVal __t3126 = bug_btn__150; ergo_retain_val(__t3126);
  ErgoVal* __env237 = (ErgoVal*)malloc(sizeof(ErgoVal) * 1);
  __env237[0] = a4; ergo_retain_val(__env237[0]);
  ErgoVal __t3127 = EV_FN(ergo_fn_new_with_env(ergo_lambda_15, 1, __env237, 1));
  ergo_m_cogito_Button_on_click(__t3126, __t3127);
  ergo_release_val(__t3126);
  ergo_release_val(__t3127);
  ErgoVal __t3128 = EV_NULLV;
  ergo_release_val(__t3128);
  ErgoVal __t3129 = actions__148; ergo_retain_val(__t3129);
  ErgoVal __t3130 = bug_btn__150; ergo_retain_val(__t3130);
  ergo_m_cogito_HStack_add(__t3129, __t3130);
  ergo_release_val(__t3129);
  ergo_release_val(__t3130);
  ErgoVal __t3131 = EV_NULLV;
  ergo_release_val(__t3131);
  ErgoVal __t3132 = root__144; ergo_retain_val(__t3132);
  ErgoVal __t3133 = actions__148; ergo_retain_val(__t3133);
  ergo_m_cogito_VStack_add(__t3132, __t3133);
  ergo_release_val(__t3132);
  ergo_release_val(__t3133);
  ErgoVal __t3134 = EV_NULLV;
  ergo_release_val(__t3134);
  ErgoVal __t3135 = win__143; ergo_retain_val(__t3135);
  ErgoVal __t3136 = root__144; ergo_retain_val(__t3136);
  ergo_m_cogito_Window_add(__t3135, __t3136);
  ergo_release_val(__t3135);
  ergo_release_val(__t3136);
  ErgoVal __t3137 = EV_NULLV;
  ergo_release_val(__t3137);
  ErgoVal __t3138 = win__143; ergo_retain_val(__t3138);
  ergo_move_into(&__ret, __t3138);
  return __ret;
  ergo_release_val(bug_btn__150);
  ergo_release_val(more_btn__149);
  ergo_release_val(actions__148);
  ergo_release_val(license_label__147);
  ergo_release_val(name_label__146);
  ergo_release_val(icon__145);
  ergo_release_val(root__144);
  ergo_release_val(win__143);
  return __ret;
}

static ErgoVal ergo_cogito_build(ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3139 = a0; ergo_retain_val(__t3139);
  ErgoVal __t3140 = a1; ergo_retain_val(__t3140);
  cogito_build(__t3139, __t3140);
  ergo_release_val(__t3139);
  ergo_release_val(__t3140);
  ErgoVal __t3141 = EV_NULLV;
  ergo_release_val(__t3141);
  ErgoVal __t3142 = a0; ergo_retain_val(__t3142);
  ergo_move_into(&__ret, __t3142);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_state(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3143 = a0; ergo_retain_val(__t3143);
  ErgoVal __t3144 = cogito_state_new(__t3143);
  ergo_release_val(__t3143);
  ergo_move_into(&__ret, __t3144);
  return __ret;
  return __ret;
}

static void ergo_cogito_set_id(ErgoVal a0, ErgoVal a1) {
  ErgoVal __t3145 = a0; ergo_retain_val(__t3145);
  ErgoVal __t3146 = a1; ergo_retain_val(__t3146);
  cogito_node_set_id(__t3145, __t3146);
  ergo_release_val(__t3145);
  ergo_release_val(__t3146);
  ErgoVal __t3147 = EV_NULLV;
  ergo_release_val(__t3147);
}

static ErgoVal ergo_cogito_vstack(void) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3148 = cogito_vstack_new();
  ergo_move_into(&__ret, __t3148);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_hstack(void) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3149 = cogito_hstack_new();
  ergo_move_into(&__ret, __t3149);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_zstack(void) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3150 = cogito_zstack_new();
  ergo_move_into(&__ret, __t3150);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_fixed(void) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3151 = cogito_fixed_new();
  ergo_move_into(&__ret, __t3151);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_scroller(void) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3152 = cogito_scroller_new();
  ergo_move_into(&__ret, __t3152);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_carousel(void) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3153 = cogito_carousel_new();
  ergo_move_into(&__ret, __t3153);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_carousel_item(void) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3154 = cogito_carousel_item_new();
  ergo_move_into(&__ret, __t3154);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_list(void) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3155 = cogito_list_new();
  ergo_move_into(&__ret, __t3155);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_grid(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3156 = a0; ergo_retain_val(__t3156);
  ErgoVal __t3157 = cogito_grid_new(__t3156);
  ergo_release_val(__t3156);
  ergo_move_into(&__ret, __t3157);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_tabs(void) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3158 = cogito_tabs_new();
  ergo_move_into(&__ret, __t3158);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_view_switcher(void) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3159 = cogito_view_switcher_new();
  ergo_move_into(&__ret, __t3159);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_progress(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3160 = a0; ergo_retain_val(__t3160);
  ErgoVal __t3161 = cogito_progress_new(__t3160);
  ergo_release_val(__t3160);
  ergo_move_into(&__ret, __t3161);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_divider(ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3162 = a0; ergo_retain_val(__t3162);
  ErgoVal __t3163 = a1; ergo_retain_val(__t3163);
  ErgoVal __t3164 = ergo_cogito___cogito_divider(__t3162, __t3163);
  ergo_release_val(__t3162);
  ergo_release_val(__t3163);
  ergo_move_into(&__ret, __t3164);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_toasts(void) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3165 = cogito_toasts_new();
  ergo_move_into(&__ret, __t3165);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_toast(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3166 = a0; ergo_retain_val(__t3166);
  ErgoVal __t3167 = cogito_toast_new(__t3166);
  ergo_release_val(__t3166);
  ergo_move_into(&__ret, __t3167);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_label(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3168 = a0; ergo_retain_val(__t3168);
  ErgoVal __t3169 = cogito_label_new(__t3168);
  ergo_release_val(__t3168);
  ergo_move_into(&__ret, __t3169);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_image(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3170 = a0; ergo_retain_val(__t3170);
  ErgoVal __t3171 = cogito_image_new(__t3170);
  ergo_release_val(__t3170);
  ergo_move_into(&__ret, __t3171);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_dialog(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3172 = a0; ergo_retain_val(__t3172);
  ErgoVal __t3173 = cogito_dialog_new(__t3172);
  ergo_release_val(__t3172);
  ergo_move_into(&__ret, __t3173);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_dialog_slot(void) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3174 = cogito_dialog_slot_new();
  ergo_move_into(&__ret, __t3174);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_button(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3175 = a0; ergo_retain_val(__t3175);
  ErgoVal __t3176 = cogito_button_new(__t3175);
  ergo_release_val(__t3175);
  ergo_move_into(&__ret, __t3176);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_iconbtn(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3177 = a0; ergo_retain_val(__t3177);
  ErgoVal __t3178 = cogito_iconbtn_new(__t3177);
  ergo_release_val(__t3177);
  ergo_move_into(&__ret, __t3178);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_appbar(ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3179 = a0; ergo_retain_val(__t3179);
  ErgoVal __t3180 = a1; ergo_retain_val(__t3180);
  ErgoVal __t3181 = cogito_appbar_new(__t3179, __t3180);
  ergo_release_val(__t3179);
  ergo_release_val(__t3180);
  ergo_move_into(&__ret, __t3181);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_checkbox(ErgoVal a0, ErgoVal a1) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3182 = a0; ergo_retain_val(__t3182);
  ErgoVal __t3183 = a1; ergo_retain_val(__t3183);
  ErgoVal __t3184 = cogito_checkbox_new(__t3182, __t3183);
  ergo_release_val(__t3182);
  ergo_release_val(__t3183);
  ergo_move_into(&__ret, __t3184);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_switch(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3185 = a0; ergo_retain_val(__t3185);
  ErgoVal __t3186 = cogito_switch_new(__t3185);
  ergo_release_val(__t3185);
  ergo_move_into(&__ret, __t3186);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_textfield(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3187 = a0; ergo_retain_val(__t3187);
  ErgoVal __t3188 = cogito_textfield_new(__t3187);
  ergo_release_val(__t3187);
  ergo_move_into(&__ret, __t3188);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_searchfield(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3189 = a0; ergo_retain_val(__t3189);
  ErgoVal __t3190 = cogito_searchfield_new(__t3189);
  ergo_release_val(__t3189);
  ergo_move_into(&__ret, __t3190);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_textview(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3191 = a0; ergo_retain_val(__t3191);
  ErgoVal __t3192 = cogito_textview_new(__t3191);
  ergo_release_val(__t3191);
  ergo_move_into(&__ret, __t3192);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_dropdown(void) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3193 = cogito_dropdown_new();
  ergo_move_into(&__ret, __t3193);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_datepicker(void) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3194 = cogito_datepicker_new();
  ergo_move_into(&__ret, __t3194);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_stepper(ErgoVal a0, ErgoVal a1, ErgoVal a2, ErgoVal a3) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3195 = a0; ergo_retain_val(__t3195);
  ErgoVal __t3196 = a1; ergo_retain_val(__t3196);
  ErgoVal __t3197 = a2; ergo_retain_val(__t3197);
  ErgoVal __t3198 = a3; ergo_retain_val(__t3198);
  ErgoVal __t3199 = cogito_stepper_new(__t3195, __t3196, __t3197, __t3198);
  ergo_release_val(__t3195);
  ergo_release_val(__t3196);
  ergo_release_val(__t3197);
  ergo_release_val(__t3198);
  ergo_move_into(&__ret, __t3199);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_slider(ErgoVal a0, ErgoVal a1, ErgoVal a2) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3200 = a0; ergo_retain_val(__t3200);
  ErgoVal __t3201 = a1; ergo_retain_val(__t3201);
  ErgoVal __t3202 = a2; ergo_retain_val(__t3202);
  ErgoVal __t3203 = cogito_slider_new(__t3200, __t3201, __t3202);
  ergo_release_val(__t3200);
  ergo_release_val(__t3201);
  ergo_release_val(__t3202);
  ergo_move_into(&__ret, __t3203);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_segmented(void) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3204 = cogito_segmented_new();
  ergo_move_into(&__ret, __t3204);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_treeview(void) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3205 = cogito_treeview_new();
  ergo_move_into(&__ret, __t3205);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_colorpicker(void) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3206 = cogito_colorpicker_new();
  ergo_move_into(&__ret, __t3206);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_bottom_toolbar(void) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3207 = cogito_toolbar_new();
  ergo_move_into(&__ret, __t3207);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_chip(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3208 = a0; ergo_retain_val(__t3208);
  ErgoVal __t3209 = cogito_chip_new(__t3208);
  ergo_release_val(__t3208);
  ergo_move_into(&__ret, __t3209);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_fab(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3210 = a0; ergo_retain_val(__t3210);
  ErgoVal __t3211 = cogito_fab_new(__t3210);
  ergo_release_val(__t3210);
  ergo_move_into(&__ret, __t3211);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_nav_rail(void) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3212 = cogito_nav_rail_new();
  ergo_move_into(&__ret, __t3212);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_bottom_nav(void) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3213 = cogito_bottom_nav_new();
  ergo_move_into(&__ret, __t3213);
  return __ret;
  return __ret;
}

static ErgoVal ergo_cogito_find_parent(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3214 = a0; ergo_retain_val(__t3214);
  ErgoVal __t3215 = cogito_find_parent(__t3214);
  ergo_release_val(__t3214);
  ergo_move_into(&__ret, __t3215);
  return __ret;
  return __ret;
}

static void ergo_cogito_dialog_slot_clear(ErgoVal a0) {
  ErgoVal __t3216 = a0; ergo_retain_val(__t3216);
  cogito_dialog_slot_clear(__t3216);
  ergo_release_val(__t3216);
  ErgoVal __t3217 = EV_NULLV;
  ergo_release_val(__t3217);
}

static ErgoVal ergo_cogito_find_children(ErgoVal a0) {
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3218 = a0; ergo_retain_val(__t3218);
  ErgoVal __t3219 = cogito_find_children(__t3218);
  ergo_release_val(__t3218);
  ergo_move_into(&__ret, __t3219);
  return __ret;
  return __ret;
}

// ---- entry ----
static void ergo_entry(void) {
  ergo_init_main();
  cogito_load_sum_inline("; ErgoCalc SUM theme\n@diagnostics: warn\n\n@primary: #8C56BF\n@primary-container: #D7C5E9\n@primary-hover: #F1EAF7\n@primary-active: #BEA0DB\n@bg: #f5f5f5\n@surface: #ffffff\n@text: #2D2D2D\n@muted: #6A6A6A\n@pink: #F1ACC1\n@pink-hover: #F8D8E2\n@pink-active: #EA80A0\n@dark-bg: #121212\n@dark-surface: #1c1c1c\n@dark-keypad: #1a1a1a\n@dark-text: #f2f2f7\n@dark-muted: #bbbbbb\n\n*\n  color: @text\n  font-size: 14\n\nwindow\n  background: @surface\n\nvstack\nhstack\nzstack\n  background: transparent\n  color: @text\n\nappbar\n  background: @bg\n  color: @text\n  min-height: 46\n  padding: 6 10\n\nappbar .iconbtn\n  background: @primary-container\n  color: @primary\n  border-radius: 21\n\nappbar .iconbtn:hover\n  background: @primary-hover\n\nappbar .iconbtn:active\n  background: @primary-active\n  border-radius: 10\n\nvstack.calc-display\n  border-radius: 12\n  min-height: 92\n  padding: 10 14\n\nlabel.calc-display-working\n  color: @text\n  background: transparent\n  font-size: 34\n  padding: 6 18\n  font-variant-numeric: tabular-nums\n\nlabel.calc-display-expression\n  color: @muted\n  background: transparent\n  font-size: 18\n  padding: 6 18\n  font-variant-numeric: tabular-nums\n\ngrid.calc-keypad\n  background: @surface\n  border-radius: 16\n  padding: 12 18 18\n  gap: 12\n\nbutton\n  background: @primary-container\n  color: @primary\n  border-radius: 10\n  min-height: 46\n  padding: 8 12\n\nlabel.calc-spacer\n  color: transparent\n\nbutton:hover\n  background: @primary-hover\n  color: @primary\n\nbutton:active\n  background: @primary-active\n  color: @primary\n  border-radius: 21\n\nbutton:focus\n  border: 2 solid @primary\n\nbutton.outlined\n  background: alpha(@primary, 0%)\n  color: @primary\n  border-radius: 10\n  border: 1 solid alpha(@primary, 35%)\n\nbutton.outlined:hover\n  background: @primary-hover\n  color: @primary\n  border-color: @primary-hover\n\nbutton.outlined:active\n  background: @primary-active\n  color: @primary\n  border-radius: 21\n\nbutton.text\n  background: @pink\n  color: @muted\n  border-radius: 10\n  padding: 8 12\n\nbutton.text:hover\n  background: @pink-hover\n  color: @muted\n\nbutton.text:active\n  background: @pink-active\n  color: @muted\n  border-radius: 21\n\nbutton.text:focus\n  border: 2 solid @primary\n\nbutton.calc-equals\n  background: @primary\n  color: @surface\n  border: 0 none #000\n\nbutton.calc-equals:hover\n  background: @primary-active\n  color: @surface\n\nbutton.calc-equals:active\n  background: mix(@primary, #000000, 18%)\n  color: @surface\n  border-radius: 21\n\nbutton.calc-equals:focus\n  border: 2 solid @surface\n\niconbtn:focus\n  border: 2 solid @primary\n\ndialog\n  background: @surface\n  color: @text\n  border-radius: 18\n  padding: 20\n\nlabel.about-window-title\n  color: @text\n\nlabel.about-window-license\n  color: @muted\n\nhstack.about-window-actions\n  background: transparent\n  gap: 10\n\ntooltip\n  background: @text\n  color: @bg\n  border-radius: 8\n  padding: 6 8\n\nvstack.conv-panel\n  background: @surface\n  padding: 18 18\n  border-radius: 16\n\nlabel.conv-label\n  color: @muted\n  font-size: 12\n  background: transparent\n\ntextfield.conv-input\ntextfield.conv-output\n  font-size: 22\n  border-radius: 10\n  padding: 10 12\n\niconbtn.conv-swap\n  background: @primary-container\n  color: @primary\n  border-radius: 21\n\niconbtn.conv-swap:hover\n  background: @primary-hover\n\niconbtn.conv-swap:active\n  background: @primary-active\n\n@when dark\n  window\n    background: @dark-bg\n  appbar\n    background: @dark-surface\n  grid.calc-keypad\n    background: @dark-keypad\n  dialog\n    background: @dark-surface\n    color: @dark-text\n  label.about-window-title\n    color: @dark-text\n  label.about-window-license\n    color: @dark-muted\n  label.calc-display-working\n    color: @dark-text\n  label.calc-display-expression\n    color: @dark-muted\n  vstack.conv-panel\n    background: @dark-keypad\n  label.conv-label\n    color: @dark-muted\n");
  ErgoVal __t3220 = EV_NULLV;
  ergo_release_val(__t3220);
  ErgoVal app__151 = EV_NULLV;
  ErgoVal __t3221 = ergo_cogito_app();
  ergo_move_into(&app__151, __t3221);
  ErgoVal __t3222 = app__151; ergo_retain_val(__t3222);
  ErgoVal __t3223 = EV_STR(stdr_str_lit("#8C56BF"));
  ErgoVal __t3224 = EV_NULLV;
  {
    ErgoVal __parts238[1] = { __t3223 };
    ErgoStr* __s239 = stdr_str_from_parts(1, __parts238);
    __t3224 = EV_STR(__s239);
  }
  ergo_release_val(__t3223);
  ErgoVal __t3225 = EV_BOOL(false);
  ergo_m_cogito_App_set_accent_color(__t3222, __t3224, __t3225);
  ergo_release_val(__t3222);
  ergo_release_val(__t3224);
  ergo_release_val(__t3225);
  ErgoVal __t3226 = EV_NULLV;
  ergo_release_val(__t3226);
  ErgoVal win__152 = EV_NULLV;
  ErgoVal __t3227 = EV_STR(stdr_str_lit("Ergo Calc"));
  ErgoVal __t3228 = EV_NULLV;
  {
    ErgoVal __parts240[1] = { __t3227 };
    ErgoStr* __s241 = stdr_str_from_parts(1, __parts240);
    __t3228 = EV_STR(__s241);
  }
  ergo_release_val(__t3227);
  ErgoVal __t3229 = EV_INT(360);
  ErgoVal __t3230 = EV_INT(470);
  ErgoVal __t3231 = ergo_cogito_window_size(__t3228, __t3229, __t3230);
  ergo_release_val(__t3228);
  ergo_release_val(__t3229);
  ergo_release_val(__t3230);
  ergo_move_into(&win__152, __t3231);
  ErgoVal __t3232 = win__152; ergo_retain_val(__t3232);
  ErgoVal __t3233 = EV_FN(ergo_fn_new(__fnwrap_main_build_ui, 1));
  ErgoVal __t3234 = ergo_m_cogito_Window_build(__t3232, __t3233);
  ergo_release_val(__t3232);
  ergo_release_val(__t3233);
  ergo_release_val(__t3234);
  ErgoVal __t3235 = win__152; ergo_retain_val(__t3235);
  ErgoVal __t3236 = EV_BOOL(false);
  ergo_m_cogito_Window_set_resizable(__t3235, __t3236);
  ergo_release_val(__t3235);
  ergo_release_val(__t3236);
  ErgoVal __t3237 = EV_NULLV;
  ergo_release_val(__t3237);
  ErgoVal __t3238 = app__151; ergo_retain_val(__t3238);
  ErgoVal __t3239 = EV_STR(stdr_str_lit("ergo.cogito.Calc"));
  ErgoVal __t3240 = EV_NULLV;
  {
    ErgoVal __parts242[1] = { __t3239 };
    ErgoStr* __s243 = stdr_str_from_parts(1, __parts242);
    __t3240 = EV_STR(__s243);
  }
  ergo_release_val(__t3239);
  ergo_m_cogito_App_set_appid(__t3238, __t3240);
  ergo_release_val(__t3238);
  ergo_release_val(__t3240);
  ErgoVal __t3241 = EV_NULLV;
  ergo_release_val(__t3241);
  ErgoVal __t3242 = app__151; ergo_retain_val(__t3242);
  ErgoVal __t3243 = EV_STR(stdr_str_lit("ErgoCalc"));
  ErgoVal __t3244 = EV_NULLV;
  {
    ErgoVal __parts244[1] = { __t3243 };
    ErgoStr* __s245 = stdr_str_from_parts(1, __parts244);
    __t3244 = EV_STR(__s245);
  }
  ergo_release_val(__t3243);
  ergo_m_cogito_App_set_app_name(__t3242, __t3244);
  ergo_release_val(__t3242);
  ergo_release_val(__t3244);
  ErgoVal __t3245 = EV_NULLV;
  ergo_release_val(__t3245);
  ErgoVal __t3246 = app__151; ergo_retain_val(__t3246);
  ErgoVal __t3247 = win__152; ergo_retain_val(__t3247);
  ergo_m_cogito_App_run(__t3246, __t3247);
  ergo_release_val(__t3246);
  ergo_release_val(__t3247);
  ErgoVal __t3248 = EV_NULLV;
  ergo_release_val(__t3248);
  ergo_release_val(win__152);
  ergo_release_val(app__151);
}

// ---- lambda defs ----
static ErgoVal ergo_lambda_1(void* env, int argc, ErgoVal* argv) {
  ErgoVal* __caps = (ErgoVal*)env;
  ErgoVal __cap0 = __caps[0];
  if (argc != 1) ergo_trap("lambda arity mismatch");
  ErgoVal arg0 = argv[0];
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3249 = EV_NULLV;
  {
    ErgoVal __t3250 = __cap0; ergo_retain_val(__t3250);
    ergo_main_input_digit(__t3250);
    ergo_release_val(__t3250);
    ErgoVal __t3251 = EV_NULLV;
    ergo_release_val(__t3251);
  }
  ergo_move_into(&__ret, __t3249);
  return __ret;
}

static ErgoVal ergo_lambda_2(void* env, int argc, ErgoVal* argv) {
  ErgoVal* __caps = (ErgoVal*)env;
  ErgoVal __cap0 = __caps[0];
  if (argc != 1) ergo_trap("lambda arity mismatch");
  ErgoVal arg0 = argv[0];
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3252 = EV_NULLV;
  {
    ErgoVal __t3253 = __cap0; ergo_retain_val(__t3253);
    ergo_main_choose_operator(__t3253);
    ergo_release_val(__t3253);
    ErgoVal __t3254 = EV_NULLV;
    ergo_release_val(__t3254);
  }
  ergo_move_into(&__ret, __t3252);
  return __ret;
}

static ErgoVal ergo_lambda_3(void* env, int argc, ErgoVal* argv) {
  (void)env;
  if (argc != 1) ergo_trap("lambda arity mismatch");
  ErgoVal arg0 = argv[0];
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3255 = EV_NULLV;
  {
    ergo_main_clear_all();
    ErgoVal __t3256 = EV_NULLV;
    ergo_release_val(__t3256);
  }
  ergo_move_into(&__ret, __t3255);
  return __ret;
}

static ErgoVal ergo_lambda_4(void* env, int argc, ErgoVal* argv) {
  (void)env;
  if (argc != 1) ergo_trap("lambda arity mismatch");
  ErgoVal arg0 = argv[0];
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3257 = EV_NULLV;
  {
    ergo_main_evaluate();
    ErgoVal __t3258 = EV_NULLV;
    ergo_release_val(__t3258);
  }
  ergo_move_into(&__ret, __t3257);
  return __ret;
}

static ErgoVal ergo_lambda_5(void* env, int argc, ErgoVal* argv) {
  (void)env;
  if (argc != 1) ergo_trap("lambda arity mismatch");
  ErgoVal arg0 = argv[0];
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3259 = EV_NULLV;
  {
    ErgoVal __t3260 = arg0; ergo_retain_val(__t3260);
    ErgoVal __t3261 = __t3260; ergo_retain_val(__t3261);
    ergo_move_into(&ergo_g_main_conv_category, __t3260);
    ergo_release_val(__t3261);
    ergo_main_refresh_conv_units();
    ErgoVal __t3262 = EV_NULLV;
    ergo_release_val(__t3262);
  }
  ergo_move_into(&__ret, __t3259);
  return __ret;
}

static ErgoVal ergo_lambda_6(void* env, int argc, ErgoVal* argv) {
  (void)env;
  if (argc != 1) ergo_trap("lambda arity mismatch");
  ErgoVal arg0 = argv[0];
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3263 = EV_NULLV;
  {
    ErgoVal __t3264 = arg0; ergo_retain_val(__t3264);
    ErgoVal __t3265 = __t3264; ergo_retain_val(__t3265);
    ergo_move_into(&ergo_g_main_conv_from_idx, __t3264);
    ergo_release_val(__t3265);
    ergo_main_do_convert();
    ErgoVal __t3266 = EV_NULLV;
    ergo_release_val(__t3266);
  }
  ergo_move_into(&__ret, __t3263);
  return __ret;
}

static ErgoVal ergo_lambda_7(void* env, int argc, ErgoVal* argv) {
  (void)env;
  if (argc != 1) ergo_trap("lambda arity mismatch");
  ErgoVal arg0 = argv[0];
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3267 = EV_NULLV;
  {
    ergo_main_do_convert();
    ErgoVal __t3268 = EV_NULLV;
    ergo_release_val(__t3268);
  }
  ergo_move_into(&__ret, __t3267);
  return __ret;
}

static ErgoVal ergo_lambda_8(void* env, int argc, ErgoVal* argv) {
  (void)env;
  if (argc != 1) ergo_trap("lambda arity mismatch");
  ErgoVal arg0 = argv[0];
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3269 = EV_NULLV;
  {
    ergo_main_swap_conv();
    ErgoVal __t3270 = EV_NULLV;
    ergo_release_val(__t3270);
  }
  ergo_move_into(&__ret, __t3269);
  return __ret;
}

static ErgoVal ergo_lambda_9(void* env, int argc, ErgoVal* argv) {
  (void)env;
  if (argc != 1) ergo_trap("lambda arity mismatch");
  ErgoVal arg0 = argv[0];
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3271 = EV_NULLV;
  {
    ErgoVal __t3272 = arg0; ergo_retain_val(__t3272);
    ErgoVal __t3273 = __t3272; ergo_retain_val(__t3273);
    ergo_move_into(&ergo_g_main_conv_to_idx, __t3272);
    ergo_release_val(__t3273);
    ergo_main_do_convert();
    ErgoVal __t3274 = EV_NULLV;
    ergo_release_val(__t3274);
  }
  ergo_move_into(&__ret, __t3271);
  return __ret;
}

static ErgoVal ergo_lambda_10(void* env, int argc, ErgoVal* argv) {
  ErgoVal* __caps = (ErgoVal*)env;
  ErgoVal __cap0 = __caps[0];
  if (argc != 1) ergo_trap("lambda arity mismatch");
  ErgoVal arg0 = argv[0];
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3275 = EV_NULLV;
  {
    ErgoVal __t3276 = ergo_g_main_ABOUT_MORE_INFO_URL; ergo_retain_val(__t3276);
    ErgoVal __t3277 = ergo_cogito_open_url(__t3276);
    ergo_release_val(__t3276);
    ergo_release_val(__t3277);
    ErgoVal __t3278 = __cap0; ergo_retain_val(__t3278);
    ergo_m_cogito_Dialog_close(__t3278);
    ergo_release_val(__t3278);
    ErgoVal __t3279 = EV_NULLV;
    ergo_release_val(__t3279);
  }
  ergo_move_into(&__ret, __t3275);
  return __ret;
}

static ErgoVal ergo_lambda_11(void* env, int argc, ErgoVal* argv) {
  ErgoVal* __caps = (ErgoVal*)env;
  ErgoVal __cap0 = __caps[0];
  if (argc != 1) ergo_trap("lambda arity mismatch");
  ErgoVal arg0 = argv[0];
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3280 = EV_NULLV;
  {
    ErgoVal __t3281 = ergo_g_main_ABOUT_REPORT_BUG_URL; ergo_retain_val(__t3281);
    ErgoVal __t3282 = ergo_cogito_open_url(__t3281);
    ergo_release_val(__t3281);
    ergo_release_val(__t3282);
    ErgoVal __t3283 = __cap0; ergo_retain_val(__t3283);
    ergo_m_cogito_Dialog_close(__t3283);
    ergo_release_val(__t3283);
    ErgoVal __t3284 = EV_NULLV;
    ergo_release_val(__t3284);
  }
  ergo_move_into(&__ret, __t3280);
  return __ret;
}

static ErgoVal ergo_lambda_12(void* env, int argc, ErgoVal* argv) {
  ErgoVal* __caps = (ErgoVal*)env;
  ErgoVal __cap0 = __caps[0];
  if (argc != 1) ergo_trap("lambda arity mismatch");
  ErgoVal arg0 = argv[0];
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3285 = EV_NULLV;
  {
    ErgoVal __t3286 = ergo_g_main_showing_converter; ergo_retain_val(__t3286);
    ErgoVal __t3287 = EV_BOOL(!ergo_as_bool(__t3286));
    ergo_release_val(__t3286);
    ErgoVal __t3288 = __t3287; ergo_retain_val(__t3288);
    ergo_move_into(&ergo_g_main_showing_converter, __t3287);
    ergo_release_val(__t3288);
    ErgoVal __t3289 = ergo_g_main_showing_converter; ergo_retain_val(__t3289);
    bool __b153 = ergo_as_bool(__t3289);
    ergo_release_val(__t3289);
    if (__b153) {
      ErgoVal __t3290 = __cap0; ergo_retain_val(__t3290);
      ErgoVal __t3291 = EV_STR(stdr_str_lit("converter"));
      ErgoVal __t3292 = EV_NULLV;
      {
        ErgoVal __parts246[1] = { __t3291 };
        ErgoStr* __s247 = stdr_str_from_parts(1, __parts246);
        __t3292 = EV_STR(__s247);
      }
      ergo_release_val(__t3291);
      ergo_m_cogito_ViewSwitcher_set_active(__t3290, __t3292);
      ergo_release_val(__t3290);
      ergo_release_val(__t3292);
      ErgoVal __t3293 = EV_NULLV;
      ergo_release_val(__t3293);
    } else {
      ErgoVal __t3294 = __cap0; ergo_retain_val(__t3294);
      ErgoVal __t3295 = EV_STR(stdr_str_lit("calculator"));
      ErgoVal __t3296 = EV_NULLV;
      {
        ErgoVal __parts248[1] = { __t3295 };
        ErgoStr* __s249 = stdr_str_from_parts(1, __parts248);
        __t3296 = EV_STR(__s249);
      }
      ergo_release_val(__t3295);
      ergo_m_cogito_ViewSwitcher_set_active(__t3294, __t3296);
      ergo_release_val(__t3294);
      ergo_release_val(__t3296);
      ErgoVal __t3297 = EV_NULLV;
      ergo_release_val(__t3297);
    }
  }
  ergo_move_into(&__ret, __t3285);
  return __ret;
}

static ErgoVal ergo_lambda_13(void* env, int argc, ErgoVal* argv) {
  ErgoVal* __caps = (ErgoVal*)env;
  ErgoVal __cap0 = __caps[0];
  if (argc != 1) ergo_trap("lambda arity mismatch");
  ErgoVal arg0 = argv[0];
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3298 = EV_NULLV;
  {
    ErgoVal __t3299 = __cap0; ergo_retain_val(__t3299);
    ergo_main_show_about_window(__t3299);
    ergo_release_val(__t3299);
    ErgoVal __t3300 = EV_NULLV;
    ergo_release_val(__t3300);
  }
  ergo_move_into(&__ret, __t3298);
  return __ret;
}

static ErgoVal ergo_lambda_14(void* env, int argc, ErgoVal* argv) {
  ErgoVal* __caps = (ErgoVal*)env;
  ErgoVal __cap0 = __caps[0];
  if (argc != 1) ergo_trap("lambda arity mismatch");
  ErgoVal arg0 = argv[0];
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3301 = EV_NULLV;
  {
    ErgoVal __t3302 = __cap0; ergo_retain_val(__t3302);
    ErgoVal __t3303 = ergo_cogito_open_url(__t3302);
    ergo_release_val(__t3302);
    ergo_release_val(__t3303);
  }
  ergo_move_into(&__ret, __t3301);
  return __ret;
}

static ErgoVal ergo_lambda_15(void* env, int argc, ErgoVal* argv) {
  ErgoVal* __caps = (ErgoVal*)env;
  ErgoVal __cap0 = __caps[0];
  if (argc != 1) ergo_trap("lambda arity mismatch");
  ErgoVal arg0 = argv[0];
  ErgoVal __ret = EV_NULLV;
  ErgoVal __t3304 = EV_NULLV;
  {
    ErgoVal __t3305 = __cap0; ergo_retain_val(__t3305);
    ErgoVal __t3306 = ergo_cogito_open_url(__t3305);
    ergo_release_val(__t3305);
    ergo_release_val(__t3306);
  }
  ergo_move_into(&__ret, __t3304);
  return __ret;
}


int main(void) {
  #ifdef __OBJC__
  @autoreleasepool {
    ergo_runtime_init();
    __cogito_set_script_dir("/Users/nayu/Documents/Projects/Ergo/extras/ergo-calc");
    ergo_entry();
  }
  #else
  ergo_runtime_init();
  ergo_entry();
  #endif
  return 0;
}
