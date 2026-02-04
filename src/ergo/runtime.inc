// ---- Ergo runtime (minimal) ----
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>
#if defined(_WIN32)
#include <io.h>
#else
int isatty(int);
int fileno(FILE*);
#endif

static int ergo_stdout_isatty = 0;

static bool cogito_debug_enabled(void);

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
  fprintf(stderr, "ergo trap: %s\n", msg);
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
    if (--f->ref == 0) free(f);
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
  return f;
}

static ErgoVal ergo_call(ErgoVal fval, int argc, ErgoVal* argv) {
  if (fval.tag != EVT_FN) ergo_trap("call expects function");
  ErgoFn* f = (ErgoFn*)fval.as.p;
  if (f->arity >= 0 && f->arity != argc) ergo_trap("arity mismatch");
  return f->fn(f->env, argc, argv);
}

// ---- Cogito GUI (raylib) ----

#include <raylib.h>
#if defined(__APPLE__)
#include <objc/objc.h>
#include <objc/message.h>
#include <objc/runtime.h>
#endif

typedef struct CogitoApp {
  ErgoObj base;
  void* native;
} CogitoApp;

typedef enum {
  COGITO_WINDOW,
  COGITO_VSTACK,
  COGITO_HSTACK,
  COGITO_BUTTON,
  COGITO_ICONBTN,
  COGITO_LABEL,
  COGITO_CHECKBOX,
  COGITO_SWITCH,
  COGITO_LIST,
  COGITO_GRID,
  COGITO_APPBAR,
  COGITO_KIND_COUNT
} CogitoKind;

typedef struct CogitoStyle {
  bool has_bg;
  bool has_text;
  bool has_border;
  bool has_border_width;
  bool has_radius;
  bool has_font_size;
  bool has_padding;
  bool has_padding_left;
  bool has_padding_top;
  bool has_padding_right;
  bool has_padding_bottom;
  bool has_margin;
  bool has_margin_left;
  bool has_margin_top;
  bool has_margin_right;
  bool has_margin_bottom;
  bool has_selection;
  Color bg;
  Color text;
  Color border;
  Color selection;
  int border_width;
  int radius;
  int font_size;
  int padding_left;
  int padding_top;
  int padding_right;
  int padding_bottom;
  int margin_left;
  int margin_top;
  int margin_right;
  int margin_bottom;
} CogitoStyle;

typedef struct CogitoTheme {
  CogitoStyle base;
  CogitoStyle per_kind[COGITO_KIND_COUNT];
} CogitoTheme;

typedef struct CogitoNode {
  ErgoObj base;
  CogitoKind kind;
  struct CogitoNode* parent;
  struct CogitoNode** children;
  size_t len;
  size_t cap;
  ErgoStr** menu_labels;
  ErgoFn** menu_handlers;
  size_t menu_len;
  size_t menu_cap;
  ErgoStr* text;
  ErgoStr* subtitle;
  ErgoFn* on_click;
  ErgoFn* on_change;
  ErgoFn* on_select;
  ErgoFn* on_activate;
  ErgoFn* builder;
  ErgoVal group;
  bool checked;
  int x;
  int y;
  int w;
  int h;
  int grid_cols;
  int selected;
  int margin_left;
  int margin_top;
  int margin_right;
  int margin_bottom;
  int padding_left;
  int padding_top;
  int padding_right;
  int padding_bottom;
  int align;
  bool auto_size;
  bool should_close;
  int appbar_btn_close_x;
  int appbar_btn_min_x;
  int appbar_btn_max_x;
  int appbar_btn_y;
  int appbar_btn_size;
  bool margin_set;
  bool padding_set;
  bool bg_set;
  bool text_color_set;
  bool border_color_set;
  bool border_width_set;
  bool radius_set;
  bool selection_set;
  Color bg;
  Color text_color;
  Color border_color;
  Color selection_color;
  int border_width;
  int border_radius;
} CogitoNode;

typedef struct CogitoState {
  ErgoObj base;
  ErgoVal value;
} CogitoState;

typedef struct CogitoMenu {
  bool open;
  CogitoNode* owner;
  int x;
  int y;
  int w;
  int h;
  int item_h;
} CogitoMenu;

static CogitoTheme cogito_theme;
static bool cogito_theme_initialized = false;
static bool cogito_raylib_ready = false;
static CogitoMenu cogito_menu = {0};
static Font cogito_font;
static bool cogito_font_ready = false;
static int cogito_font_size_value = 16;
static int cogito_font_loaded_size = 0;
static bool cogito_font_from_env = false;
static bool cogito_drag_active = false;
static Vector2 cogito_drag_start_mouse = {0};
static Vector2 cogito_drag_start_win = {0};
static bool cogito_drag_pending = false;
static Vector2 cogito_drag_pending_mouse = {0};
static double cogito_drag_last_move = 0.0;

#if defined(__APPLE__)
typedef unsigned long CogitoNSUInteger;

static id cogito_nsstring(const char* s) {
  id nsstring = (id)objc_getClass("NSString");
  SEL sel = sel_registerName("stringWithUTF8String:");
  return ((id (*)(id, SEL, const char*))objc_msgSend)(nsstring, sel, s ? s : "");
}

static void cogito_configure_macos_window(bool has_appbar) {
  if (!has_appbar) return;
  id nsw =
#if defined(__OBJC__)
      (__bridge id)GetWindowHandle();
#else
      (id)GetWindowHandle();
#endif
  if (!nsw) return;
  SEL sel_set_title = sel_registerName("setTitle:");
  ((void (*)(id, SEL, id))objc_msgSend)(nsw, sel_set_title, cogito_nsstring(""));
  SEL sel_set_title_visibility = sel_registerName("setTitleVisibility:");
  ((void (*)(id, SEL, CogitoNSUInteger))objc_msgSend)(nsw, sel_set_title_visibility, 1UL);
  SEL sel_set_transparent = sel_registerName("setTitlebarAppearsTransparent:");
  ((void (*)(id, SEL, BOOL))objc_msgSend)(nsw, sel_set_transparent, (BOOL)1);
  SEL sel_set_movable = sel_registerName("setMovableByWindowBackground:");
  ((void (*)(id, SEL, BOOL))objc_msgSend)(nsw, sel_set_movable, (BOOL)1);
  SEL sel_style_mask = sel_registerName("styleMask");
  CogitoNSUInteger mask = ((CogitoNSUInteger (*)(id, SEL))objc_msgSend)(nsw, sel_style_mask);
  mask |= (1UL << 15);
  SEL sel_set_style_mask = sel_registerName("setStyleMask:");
  ((void (*)(id, SEL, CogitoNSUInteger))objc_msgSend)(nsw, sel_set_style_mask, mask);
  SEL sel_standard_button = sel_registerName("standardWindowButton:");
  id close = ((id (*)(id, SEL, CogitoNSUInteger))objc_msgSend)(nsw, sel_standard_button, 0UL);
  id min = ((id (*)(id, SEL, CogitoNSUInteger))objc_msgSend)(nsw, sel_standard_button, 1UL);
  id zoom = ((id (*)(id, SEL, CogitoNSUInteger))objc_msgSend)(nsw, sel_standard_button, 2UL);
  SEL sel_set_hidden = sel_registerName("setHidden:");
  if (close) ((void (*)(id, SEL, BOOL))objc_msgSend)(close, sel_set_hidden, (BOOL)1);
  if (min) ((void (*)(id, SEL, BOOL))objc_msgSend)(min, sel_set_hidden, (BOOL)1);
  if (zoom) ((void (*)(id, SEL, BOOL))objc_msgSend)(zoom, sel_set_hidden, (BOOL)1);
}
#endif

static Color cogito_rgba(int r, int g, int b, int a) {
  if (r < 0) r = 0; if (r > 255) r = 255;
  if (g < 0) g = 0; if (g > 255) g = 255;
  if (b < 0) b = 0; if (b > 255) b = 255;
  if (a < 0) a = 0; if (a > 255) a = 255;
  Color c;
  c.r = (unsigned char)r;
  c.g = (unsigned char)g;
  c.b = (unsigned char)b;
  c.a = (unsigned char)a;
  return c;
}

static void cogito_style_clear(CogitoStyle* s) {
  memset(s, 0, sizeof(*s));
}

static void cogito_theme_set_defaults(void) {
  cogito_style_clear(&cogito_theme.base);
  for (int i = 0; i < COGITO_KIND_COUNT; i++) {
    cogito_style_clear(&cogito_theme.per_kind[i]);
  }
  cogito_theme.base.has_text = true;
  cogito_theme.base.text = cogito_rgba(30, 30, 30, 255);

  CogitoStyle* win = &cogito_theme.per_kind[COGITO_WINDOW];
  win->has_bg = true;
  win->bg = cogito_rgba(245, 245, 245, 255);

  CogitoStyle* btn = &cogito_theme.per_kind[COGITO_BUTTON];
  btn->has_bg = true;
  btn->bg = cogito_rgba(230, 230, 230, 255);
  btn->has_border = true;
  btn->border = cogito_rgba(180, 180, 180, 255);
  btn->has_border_width = true;
  btn->border_width = 1;
  btn->has_radius = true;
  btn->radius = 6;

  CogitoStyle* icon = &cogito_theme.per_kind[COGITO_ICONBTN];
  icon->has_bg = true;
  icon->bg = cogito_rgba(230, 230, 230, 255);
  icon->has_border = true;
  icon->border = cogito_rgba(180, 180, 180, 255);
  icon->has_border_width = true;
  icon->border_width = 1;
  icon->has_radius = true;
  icon->radius = 16;

  CogitoStyle* list = &cogito_theme.per_kind[COGITO_LIST];
  list->has_bg = true;
  list->bg = cogito_rgba(255, 255, 255, 255);
  list->has_border = true;
  list->border = cogito_rgba(200, 200, 200, 255);
  list->has_border_width = true;
  list->border_width = 1;
  list->has_radius = true;
  list->radius = 4;
  list->has_selection = true;
  list->selection = cogito_rgba(208, 220, 245, 255);

  CogitoStyle* grid = &cogito_theme.per_kind[COGITO_GRID];
  *grid = *list;

  CogitoStyle* appbar = &cogito_theme.per_kind[COGITO_APPBAR];
  appbar->has_bg = true;
  appbar->bg = cogito_rgba(238, 238, 238, 255);
  appbar->has_border = true;
  appbar->border = cogito_rgba(220, 220, 220, 255);
  appbar->has_border_width = true;
  appbar->border_width = 1;
}

static void cogito_theme_init(void) {
  if (!cogito_theme_initialized) {
    cogito_theme_initialized = true;
    cogito_theme_set_defaults();
  }
}

static CogitoStyle cogito_style_merge(const CogitoStyle* base, const CogitoStyle* over) {
  CogitoStyle out = *base;
  if (over->has_bg) { out.has_bg = true; out.bg = over->bg; }
  if (over->has_text) { out.has_text = true; out.text = over->text; }
  if (over->has_border) { out.has_border = true; out.border = over->border; }
  if (over->has_border_width) { out.has_border_width = true; out.border_width = over->border_width; }
  if (over->has_radius) { out.has_radius = true; out.radius = over->radius; }
  if (over->has_selection) { out.has_selection = true; out.selection = over->selection; }
  if (over->has_font_size) { out.has_font_size = true; out.font_size = over->font_size; }

  if (over->has_padding) {
    out.has_padding = true;
    out.padding_left = over->padding_left;
    out.padding_top = over->padding_top;
    out.padding_right = over->padding_right;
    out.padding_bottom = over->padding_bottom;
  }
  if (over->has_padding_left) { out.has_padding_left = true; out.padding_left = over->padding_left; }
  if (over->has_padding_top) { out.has_padding_top = true; out.padding_top = over->padding_top; }
  if (over->has_padding_right) { out.has_padding_right = true; out.padding_right = over->padding_right; }
  if (over->has_padding_bottom) { out.has_padding_bottom = true; out.padding_bottom = over->padding_bottom; }

  if (over->has_margin) {
    out.has_margin = true;
    out.margin_left = over->margin_left;
    out.margin_top = over->margin_top;
    out.margin_right = over->margin_right;
    out.margin_bottom = over->margin_bottom;
  }
  if (over->has_margin_left) { out.has_margin_left = true; out.margin_left = over->margin_left; }
  if (over->has_margin_top) { out.has_margin_top = true; out.margin_top = over->margin_top; }
  if (over->has_margin_right) { out.has_margin_right = true; out.margin_right = over->margin_right; }
  if (over->has_margin_bottom) { out.has_margin_bottom = true; out.margin_bottom = over->margin_bottom; }

  return out;
}

static CogitoStyle cogito_theme_resolve(CogitoKind kind) {
  return cogito_style_merge(&cogito_theme.base, &cogito_theme.per_kind[kind]);
}

static void cogito_apply_style_to_node(CogitoNode* n) {
  if (!n) return;
  cogito_theme_init();
  CogitoStyle s = cogito_theme_resolve(n->kind);
  if (s.has_bg) { n->bg = s.bg; n->bg_set = true; }
  else { n->bg_set = false; }
  if (s.has_text) { n->text_color = s.text; n->text_color_set = true; }
  else { n->text_color_set = false; }
  if (s.has_border) { n->border_color = s.border; n->border_color_set = true; }
  else { n->border_color_set = false; n->border_color = cogito_rgba(0, 0, 0, 0); }
  if (s.has_border_width) { n->border_width = s.border_width; n->border_width_set = true; }
  else { n->border_width = 0; n->border_width_set = false; }
  if (s.has_radius) { n->border_radius = s.radius; n->radius_set = true; }
  else { n->border_radius = 0; n->radius_set = false; }
  if (s.has_selection) { n->selection_color = s.selection; n->selection_set = true; }
  else { n->selection_set = false; }
  if (s.has_font_size && n->kind == COGITO_WINDOW) {
    int sz = s.font_size;
    if (sz > 0 && sz != cogito_font_size_value) {
      cogito_font_size_value = sz;
      if (cogito_raylib_ready && cogito_font_ready && cogito_font_from_env) {
        const char* font_path = getenv("COGITO_FONT");
        if (font_path && font_path[0] && cogito_font_loaded_size != sz) {
          UnloadFont(cogito_font);
          cogito_font = LoadFontEx(font_path, sz, NULL, 0);
          if (cogito_font.texture.id != 0) {
            cogito_font_ready = true;
            SetTextureFilter(cogito_font.texture, TEXTURE_FILTER_BILINEAR);
            cogito_font_loaded_size = sz;
          }
        }
      }
    }
  }

  if (!n->padding_set) {
    if (s.has_padding) {
      n->padding_left = s.padding_left;
      n->padding_top = s.padding_top;
      n->padding_right = s.padding_right;
      n->padding_bottom = s.padding_bottom;
    }
    if (s.has_padding_left) n->padding_left = s.padding_left;
    if (s.has_padding_top) n->padding_top = s.padding_top;
    if (s.has_padding_right) n->padding_right = s.padding_right;
    if (s.has_padding_bottom) n->padding_bottom = s.padding_bottom;
  }

  if (!n->margin_set) {
    if (s.has_margin) {
      n->margin_left = s.margin_left;
      n->margin_top = s.margin_top;
      n->margin_right = s.margin_right;
      n->margin_bottom = s.margin_bottom;
    }
    if (s.has_margin_left) n->margin_left = s.margin_left;
    if (s.has_margin_top) n->margin_top = s.margin_top;
    if (s.has_margin_right) n->margin_right = s.margin_right;
    if (s.has_margin_bottom) n->margin_bottom = s.margin_bottom;
  }
}

static void cogito_apply_style_tree(CogitoNode* n) {
  if (!n) return;
  cogito_apply_style_to_node(n);
  for (size_t i = 0; i < n->len; i++) {
    cogito_apply_style_tree(n->children[i]);
  }
}

static int cogito_font_size(void) {
  return cogito_font_size_value > 0 ? cogito_font_size_value : 16;
}

static int cogito_text_width(const char* s) {
  if (!s) return 0;
  if (!cogito_raylib_ready || !cogito_font_ready) return (int)strlen(s) * 8;
  Vector2 size = MeasureTextEx(cogito_font, s, (float)cogito_font_size(), 1.0f);
  return (int)lround(size.x);
}

static int cogito_text_height(void) {
  if (!cogito_raylib_ready || !cogito_font_ready) return 18;
  return cogito_font.baseSize + 2;
}

static bool cogito_debug_enabled(void) {
  const char* v = getenv("COGITO_DEBUG");
  return v && v[0] && v[0] != '0';
}

static bool cogito_val_equal(ErgoVal a, ErgoVal b) {
  if (a.tag != b.tag) return false;
  switch (a.tag) {
    case EVT_NULL: return true;
    case EVT_INT: return a.as.i == b.as.i;
    case EVT_FLOAT: return a.as.f == b.as.f;
    case EVT_BOOL: return a.as.b == b.as.b;
    case EVT_STR:
    case EVT_ARR:
    case EVT_OBJ:
    case EVT_FN:
      return a.as.p == b.as.p;
  }
  return false;
}

static void cogito_node_drop(ErgoObj* o) {
  CogitoNode* n = (CogitoNode*)o;
  if (n->text) {
    ergo_release_val(EV_STR(n->text));
    n->text = NULL;
  }
  if (n->subtitle) {
    ergo_release_val(EV_STR(n->subtitle));
    n->subtitle = NULL;
  }
  if (n->on_click) {
    ergo_release_val(EV_FN(n->on_click));
    n->on_click = NULL;
  }
  if (n->on_change) {
    ergo_release_val(EV_FN(n->on_change));
    n->on_change = NULL;
  }
  if (n->on_select) {
    ergo_release_val(EV_FN(n->on_select));
    n->on_select = NULL;
  }
  if (n->on_activate) {
    ergo_release_val(EV_FN(n->on_activate));
    n->on_activate = NULL;
  }
  if (n->builder) {
    ergo_release_val(EV_FN(n->builder));
    n->builder = NULL;
  }
  if (n->group.tag != EVT_NULL) {
    ergo_release_val(n->group);
    n->group = EV_NULLV;
  }
  if (n->menu_labels) {
    for (size_t i = 0; i < n->menu_len; i++) {
      if (n->menu_labels[i]) ergo_release_val(EV_STR(n->menu_labels[i]));
      if (n->menu_handlers[i]) ergo_release_val(EV_FN(n->menu_handlers[i]));
    }
    free(n->menu_labels);
    free(n->menu_handlers);
    n->menu_labels = NULL;
    n->menu_handlers = NULL;
    n->menu_len = 0;
    n->menu_cap = 0;
  }
  if (n->children) {
    for (size_t i = 0; i < n->len; i++) {
      ergo_release_val(EV_OBJ(n->children[i]));
    }
    free(n->children);
    n->children = NULL;
    n->len = 0;
    n->cap = 0;
  }
}

static CogitoNode* cogito_node_new(CogitoKind kind) {
  CogitoNode* n = (CogitoNode*)ergo_obj_new(sizeof(CogitoNode), cogito_node_drop);
  n->kind = kind;
  n->parent = NULL;
  n->children = NULL;
  n->len = 0;
  n->cap = 0;
  n->text = NULL;
  n->subtitle = NULL;
  n->on_click = NULL;
  n->on_change = NULL;
  n->on_select = NULL;
  n->on_activate = NULL;
  n->builder = NULL;
  n->group = EV_NULLV;
  n->checked = false;
  n->x = 0;
  n->y = 0;
  n->w = 0;
  n->h = 0;
  n->grid_cols = 1;
  n->selected = -1;
  n->should_close = false;
  n->menu_labels = NULL;
  n->menu_handlers = NULL;
  n->menu_len = 0;
  n->menu_cap = 0;
  n->appbar_btn_close_x = 0;
  n->appbar_btn_min_x = 0;
  n->appbar_btn_max_x = 0;
  n->appbar_btn_y = 0;
  n->appbar_btn_size = 0;
  n->margin_left = 0;
  n->margin_top = 0;
  n->margin_right = 0;
  n->margin_bottom = 0;
  n->padding_left = 0;
  n->padding_top = 0;
  n->padding_right = 0;
  n->padding_bottom = 0;
  n->align = 0;
  n->auto_size = false;
  n->margin_set = false;
  n->padding_set = false;
  n->bg_set = false;
  n->text_color_set = false;
  n->border_color_set = false;
  n->border_width_set = false;
  n->radius_set = false;
  n->selection_set = false;
  n->bg = cogito_rgba(0, 0, 0, 0);
  n->text_color = cogito_rgba(30, 30, 30, 255);
  n->border_color = cogito_rgba(0, 0, 0, 0);
  n->selection_color = cogito_rgba(208, 220, 245, 255);
  n->border_width = 0;
  n->border_radius = 0;
  cogito_apply_style_to_node(n);
  return n;
}

static void cogito_node_set_text(CogitoNode* n, ErgoStr* s) {
  if (!n) return;
  if (n->text) ergo_release_val(EV_STR(n->text));
  n->text = s;
  if (s) ergo_retain_val(EV_STR(s));
}

static void cogito_node_set_subtitle(CogitoNode* n, ErgoStr* s) {
  if (!n) return;
  if (n->subtitle) ergo_release_val(EV_STR(n->subtitle));
  n->subtitle = s;
  if (s) ergo_retain_val(EV_STR(s));
}

static void cogito_node_set_group(CogitoNode* n, ErgoVal group) {
  if (!n) return;
  if (n->group.tag != EVT_NULL) ergo_release_val(n->group);
  n->group = group;
  if (group.tag != EVT_NULL) ergo_retain_val(group);
}

static void cogito_children_add(CogitoNode* parent, CogitoNode* child) {
  if (!parent || !child) return;
  if (parent->len + 1 > parent->cap) {
    size_t next = parent->cap == 0 ? 4 : parent->cap * 2;
    parent->children = (CogitoNode**)realloc(parent->children, sizeof(CogitoNode*) * next);
    parent->cap = next;
  }
  parent->children[parent->len++] = child;
  child->parent = parent;
  ergo_retain_val(EV_OBJ(child));
}

static void cogito_node_add_menu(CogitoNode* n, ErgoStr* label, ErgoFn* handler) {
  if (!n) return;
  if (n->menu_len + 1 > n->menu_cap) {
    size_t next = n->menu_cap == 0 ? 4 : n->menu_cap * 2;
    n->menu_labels = (ErgoStr**)realloc(n->menu_labels, sizeof(ErgoStr*) * next);
    n->menu_handlers = (ErgoFn**)realloc(n->menu_handlers, sizeof(ErgoFn*) * next);
    n->menu_cap = next;
  }
  n->menu_labels[n->menu_len] = label;
  n->menu_handlers[n->menu_len] = handler;
  if (label) ergo_retain_val(EV_STR(label));
  if (handler) ergo_retain_val(EV_FN(handler));
  n->menu_len++;
}

static void cogito_intrinsic_size(CogitoNode* n, int* out_w, int* out_h) {
  int text_w = 0;
  if (n->text) text_w = cogito_text_width(n->text->data);
  int text_h = cogito_text_height();
  int w = 0;
  int h = 0;
  int pad = 6;
  switch (n->kind) {
    case COGITO_BUTTON:
      w = text_w + 24;
      h = text_h + 6;
      break;
    case COGITO_ICONBTN:
      w = 32;
      h = 32;
      break;
    case COGITO_LABEL:
      w = text_w + 4;
      h = text_h;
      break;
    case COGITO_CHECKBOX:
      w = text_w + 32;
      h = text_h + 4;
      break;
    case COGITO_SWITCH:
      w = text_w + 40;
      h = text_h + 4;
      break;
    case COGITO_LIST:
    case COGITO_GRID:
      w = 160;
      h = 120;
      break;
    case COGITO_APPBAR: {
      bool has_title = n->text && n->text->len;
      bool has_sub = n->subtitle && n->subtitle->len;
      bool has_buttons = n->len > 0;
      h = (has_title && has_sub && has_buttons) ? 100 : 48;
      w = n->w;
      break;
    }
    case COGITO_VSTACK:
    case COGITO_HSTACK: {
      int cw = 0;
      int ch = 0;
      if (n->len == 0) { w = n->margin_left + n->margin_right; h = n->margin_top + n->margin_bottom; break; }
      for (size_t i = 0; i < n->len; i++) {
        int iw = 0;
        int ih = 0;
        cogito_intrinsic_size(n->children[i], &iw, &ih);
        if (n->kind == COGITO_VSTACK) {
          if (iw > cw) cw = iw;
          ch += ih;
          if (i + 1 < n->len) ch += pad;
        } else {
          cw += iw;
          if (i + 1 < n->len) cw += pad;
          if (ih > ch) ch = ih;
        }
      }
      w = cw + n->margin_left + n->margin_right;
      h = ch + n->margin_top + n->margin_bottom;
      break;
    }
    case COGITO_WINDOW:
      w = n->w;
      h = n->h;
      break;
  }
  w += n->padding_left + n->padding_right;
  h += n->padding_top + n->padding_bottom;
  if (out_w) *out_w = w;
  if (out_h) *out_h = h;
}

static void cogito_layout_node(CogitoNode* n, int x, int y, int w, int h);
static void cogito_window_relayout(CogitoNode* win);

static void cogito_layout_vstack(CogitoNode* n, int x, int y) {
  int pad = 6;
  int cy = y + n->padding_top;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    int cw = 0;
    int ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    int cx = x + n->padding_left;
    int ox = 0;
    if (n->align == 1) {
      ox = (n->w - cw) / 2;
    } else if (n->align == 2) {
      ox = n->w - cw - n->padding_right;
    }
    if (ox < 0) ox = 0;
    if (n->align == 1) {
      cogito_layout_node(c, cx + ox + c->margin_left, cy + c->margin_top, cw, ch);
    } else if (n->align == 2) {
      cogito_layout_node(c, cx + ox + c->margin_left, cy + c->margin_top, cw, ch);
    } else {
      cogito_layout_node(c, cx + c->margin_left, cy + c->margin_top, cw, ch);
    }
    cy += ch + c->margin_top + c->margin_bottom + pad;
  }
}

static void cogito_layout_hstack(CogitoNode* n, int x, int y) {
  int pad = 6;
  int cx = x + n->padding_left;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    int cw = 0;
    int ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    int cy = y + n->padding_top;
    int oy = 0;
    if (n->align == 1) {
      oy = (n->h - ch) / 2;
    } else if (n->align == 2) {
      oy = n->h - ch - n->padding_bottom;
    }
    if (oy < 0) oy = 0;
    if (n->align == 1) {
      cogito_layout_node(c, cx + c->margin_left, cy + oy + c->margin_top, cw, ch);
    } else if (n->align == 2) {
      cogito_layout_node(c, cx + c->margin_left, cy + oy + c->margin_top, cw, ch);
    } else {
      cogito_layout_node(c, cx + c->margin_left, cy + c->margin_top, cw, ch);
    }
    cx += cw + c->margin_left + c->margin_right + pad;
  }
}

static void cogito_layout_list(CogitoNode* n, int x, int y) {
  int pad = 6;
  int cy = y + n->padding_top;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    int cw = n->w - n->padding_left - n->padding_right;
    int ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    cogito_layout_node(c, x + n->padding_left + pad, cy, cw, ch);
    cy += ch + pad;
  }
}

static void cogito_layout_grid(CogitoNode* n, int x, int y) {
  int cols = n->grid_cols > 0 ? n->grid_cols : 1;
  int pad = 6;
  int cell_w = (n->w - n->padding_left - n->padding_right - (cols - 1) * pad) / cols;
  if (cell_w < 0) cell_w = 0;
  int row = 0;
  int col = 0;
  int cx = x + n->padding_left;
  int cy = y + n->padding_top;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* child = n->children[i];
    int cell_h = 0;
    cogito_intrinsic_size(child, NULL, &cell_h);
    cogito_layout_node(child, cx, cy, cell_w, cell_h);
    col++;
    if (col >= cols) {
      col = 0;
      row++;
      cx = x + n->padding_left;
      cy += cell_h + pad;
    } else {
      cx += cell_w + pad;
    }
  }
  (void)row;
}

static void cogito_layout_node(CogitoNode* n, int x, int y, int w, int h) {
  n->x = x;
  n->y = y;
  n->w = w;
  n->h = h;
  switch (n->kind) {
    case COGITO_APPBAR: {
      int pad_x = 16;
      int btn_size = 18;
      int btn_gap = 6;
      int buttons_y = y + 10;
      n->appbar_btn_size = btn_size;
      n->appbar_btn_y = buttons_y;
      n->appbar_btn_close_x = x + w - pad_x - btn_size;
      n->appbar_btn_min_x = n->appbar_btn_close_x - btn_size - btn_gap;
      n->appbar_btn_max_x = n->appbar_btn_min_x - btn_size - btn_gap;
      int icon_row_h = 0;
      for (size_t i = 0; i < n->len; i++) {
        int cw = 0;
        int ch = 0;
        cogito_intrinsic_size(n->children[i], &cw, &ch);
        if (ch > icon_row_h) icon_row_h = ch;
      }
      if (icon_row_h == 0) icon_row_h = btn_size;
      int cx = n->appbar_btn_max_x - btn_gap;
      int title_h = 0;
      int subtitle_h = 0;
      if (n->text && n->text->len) title_h = 24;
      if (n->subtitle && n->subtitle->len) subtitle_h = 18;
      for (size_t i = 0; i < n->len; i++) {
        CogitoNode* c = n->children[i];
        int cw = 0;
        int ch = 0;
        cogito_intrinsic_size(c, &cw, &ch);
        cx -= cw;
        int cy = buttons_y + (icon_row_h - ch) / 2;
        cogito_layout_node(c, cx, cy, cw, ch);
        cx -= btn_gap;
      }
      (void)title_h;
      (void)subtitle_h;
      break;
    }
    case COGITO_VSTACK:
      cogito_layout_vstack(n, x, y);
      break;
    case COGITO_HSTACK:
      cogito_layout_hstack(n, x, y);
      break;
    case COGITO_LIST:
      cogito_layout_list(n, x, y);
      break;
    case COGITO_GRID:
      cogito_layout_grid(n, x, y);
      break;
    case COGITO_WINDOW:
    case COGITO_BUTTON:
    case COGITO_ICONBTN:
    case COGITO_LABEL:
    case COGITO_CHECKBOX:
    case COGITO_SWITCH:
      break;
  }
}

static void cogito_layout_window(CogitoNode* win) {
  int pad = 12;
  win->x = 0;
  win->y = 0;
  int cx = pad + win->padding_left;
  int cy = pad + win->padding_top;
  int appbar_h = 0;
  CogitoNode* appbar = NULL;
  for (size_t i = 0; i < win->len; i++) {
    if (win->children[i]->kind == COGITO_APPBAR) {
      appbar = win->children[i];
      break;
    }
  }
  if (appbar) {
    cogito_intrinsic_size(appbar, NULL, &appbar_h);
    cogito_layout_node(appbar, 0, 0, win->w, appbar_h);
    cy += appbar_h;
  }
  for (size_t i = 0; i < win->len; i++) {
    CogitoNode* c = win->children[i];
    if (c->kind == COGITO_APPBAR) continue;
    int cw = 0;
    int ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    int cw_total = cw + c->margin_left + c->margin_right;
    int ch_total = ch + c->margin_top + c->margin_bottom;
    if (c->kind == COGITO_VSTACK || c->kind == COGITO_HSTACK) {
      int avail_w = win->w - win->padding_left - win->padding_right;
      int avail_h = win->h - win->padding_top - win->padding_bottom;
      if (appbar) avail_h -= appbar_h;
      if (avail_w < 0) avail_w = 0;
      if (avail_h < 0) avail_h = 0;
      cogito_layout_node(c, win->padding_left, win->padding_top + appbar_h, avail_w, avail_h);
      cy += avail_h + pad;
    } else {
      cogito_layout_node(c, cx + c->margin_left, cy + c->margin_top, cw, ch);
      cy += ch_total + pad;
    }
  }
}

static void cogito_window_desired_size(CogitoNode* win, int* out_w, int* out_h) {
  int pad = 12;
  int max_w = pad + win->padding_left;
  int cy = pad + win->padding_top;
  int appbar_h = 0;
  for (size_t i = 0; i < win->len; i++) {
    CogitoNode* c = win->children[i];
    if (c->kind == COGITO_APPBAR) {
      cogito_intrinsic_size(c, NULL, &appbar_h);
      int row_w = pad + win->padding_left + win->w;
      if (row_w > max_w) max_w = row_w;
      break;
    }
  }
  if (appbar_h > 0) cy += appbar_h;
  for (size_t i = 0; i < win->len; i++) {
    CogitoNode* c = win->children[i];
    if (c->kind == COGITO_APPBAR) continue;
    int cw = 0;
    int ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    int cw_total = cw + c->margin_left + c->margin_right;
    int ch_total = ch + c->margin_top + c->margin_bottom;
    int row_w = pad + win->padding_left + cw_total;
    if (row_w > max_w) max_w = row_w;
    cy += ch_total + pad;
  }
  int dw = max_w + win->padding_right + pad;
  int dh = cy + win->padding_bottom + pad;
  if (out_w) *out_w = dw;
  if (out_h) *out_h = dh;
}

static void cogito_window_relayout(CogitoNode* win) {
  if (!win) return;
  cogito_layout_window(win);
}

static void cogito_invoke(ErgoFn* fn, int argc, ErgoVal* argv) {
  if (!fn) return;
  fn->fn(fn->env, argc, argv);
}

static void cogito_invoke_click(CogitoNode* n) {
  if (!n || !n->on_click) return;
  ErgoVal arg = EV_OBJ(n);
  cogito_invoke(n->on_click, 1, &arg);
}

static void cogito_invoke_menu(CogitoNode* n, size_t idx) {
  if (!n || idx >= n->menu_len) return;
  ErgoFn* fn = n->menu_handlers[idx];
  if (!fn) return;
  ErgoVal arg = EV_OBJ(n);
  cogito_invoke(fn, 1, &arg);
}

static void cogito_invoke_change(CogitoNode* n) {
  if (!n || !n->on_change) return;
  ErgoVal arg = EV_OBJ(n);
  cogito_invoke(n->on_change, 1, &arg);
}

static void cogito_invoke_select(CogitoNode* n, int idx, bool activate) {
  ErgoVal arg = EV_INT(idx);
  if (n->on_select) cogito_invoke(n->on_select, 1, &arg);
  if (activate && n->on_activate) cogito_invoke(n->on_activate, 1, &arg);
}

static void cogito_appbar_window_action(CogitoNode* win, int action) {
  (void)win;
  if (action == 0) {
    if (win) win->should_close = true;
  } else if (action == 1) {
    MinimizeWindow();
  } else if (action == 2) {
    if (IsWindowMaximized()) {
      RestoreWindow();
    } else {
      MaximizeWindow();
    }
  }
}

static void cogito_state_drop(ErgoObj* o) {
  CogitoState* s = (CogitoState*)o;
  if (s->value.tag != EVT_NULL) {
    ergo_release_val(s->value);
    s->value = EV_NULLV;
  }
}

static CogitoState* cogito_state_new_obj(ErgoVal initial) {
  CogitoState* s = (CogitoState*)ergo_obj_new(sizeof(CogitoState), cogito_state_drop);
  s->value = initial;
  if (initial.tag != EVT_NULL) ergo_retain_val(initial);
  return s;
}

static void cogito_uncheck_group(CogitoNode* root, CogitoNode* except, ErgoVal group) {
  if (!root) return;
  if ((root->kind == COGITO_CHECKBOX || root->kind == COGITO_SWITCH) && root != except) {
    if (group.tag != EVT_NULL && cogito_val_equal(root->group, group)) {
      root->checked = false;
    }
  }
  for (size_t i = 0; i < root->len; i++) {
    cogito_uncheck_group(root->children[i], except, group);
  }
}

static CogitoNode* cogito_root(CogitoNode* n) {
  CogitoNode* cur = n;
  while (cur && cur->parent) cur = cur->parent;
  return cur;
}

static bool cogito_hit_rect(int x, int y, int rx, int ry, int rw, int rh) {
  return x >= rx && y >= ry && x <= rx + rw && y <= ry + rh;
}

static bool cogito_hit_node(CogitoNode* n, int x, int y) {
  return cogito_hit_rect(x, y, n->x, n->y, n->w, n->h);
}

static CogitoNode* cogito_find_appbar(CogitoNode* win) {
  if (!win) return NULL;
  for (size_t i = 0; i < win->len; i++) {
    if (win->children[i]->kind == COGITO_APPBAR) return win->children[i];
  }
  return NULL;
}

static bool cogito_point_over_appbar_buttons(CogitoNode* app, int x, int y) {
  if (!app) return false;
  if (cogito_hit_rect(x, y, app->appbar_btn_close_x, app->appbar_btn_y, app->appbar_btn_size, app->appbar_btn_size)) return true;
  if (cogito_hit_rect(x, y, app->appbar_btn_min_x, app->appbar_btn_y, app->appbar_btn_size, app->appbar_btn_size)) return true;
  if (cogito_hit_rect(x, y, app->appbar_btn_max_x, app->appbar_btn_y, app->appbar_btn_size, app->appbar_btn_size)) return true;
  for (size_t i = 0; i < app->len; i++) {
    CogitoNode* c = app->children[i];
    if (cogito_hit_rect(x, y, c->x, c->y, c->w, c->h)) return true;
  }
  return false;
}

static int cogito_list_hit_index(CogitoNode* list, int x, int y) {
  if (!cogito_hit_node(list, x, y)) return -1;
  for (size_t i = 0; i < list->len; i++) {
    CogitoNode* item = list->children[i];
    if (cogito_hit_node(item, x, y)) return (int)i;
  }
  return -1;
}

static CogitoNode* cogito_find_list_grid_hit(CogitoNode* n, int x, int y, int* out_idx) {
  if (!n) return NULL;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* hit = cogito_find_list_grid_hit(n->children[i], x, y, out_idx);
    if (hit) return hit;
  }
  if (n->kind == COGITO_LIST || n->kind == COGITO_GRID) {
    int idx = cogito_list_hit_index(n, x, y);
    if (idx >= 0) {
      if (out_idx) *out_idx = idx;
      return n;
    }
  }
  return NULL;
}

static CogitoNode* cogito_find_clickable(CogitoNode* n, int x, int y) {
  if (!n) return NULL;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* child = cogito_find_clickable(n->children[i], x, y);
    if (child) return child;
  }
  if (n->kind == COGITO_BUTTON || n->kind == COGITO_ICONBTN || n->kind == COGITO_CHECKBOX ||
      n->kind == COGITO_SWITCH || n->kind == COGITO_LIST || n->kind == COGITO_GRID) {
    if (cogito_hit_node(n, x, y)) return n;
  }
  return NULL;
}

static void cogito_handle_list_select(CogitoNode* list, int idx, bool activate) {
  list->selected = idx;
  cogito_invoke_select(list, idx, activate);
}

static void cogito_handle_click(CogitoNode* win, int x, int y, bool activate) {
  int idx = -1;
  CogitoNode* list = cogito_find_list_grid_hit(win, x, y, &idx);
  if (list && idx >= 0) {
    cogito_handle_list_select(list, idx, activate);
  }
}

static void cogito_clear_children(CogitoNode* node) {
  if (!node || !node->children) return;
  for (size_t i = 0; i < node->len; i++) {
    ergo_release_val(EV_OBJ(node->children[i]));
  }
  free(node->children);
  node->children = NULL;
  node->len = 0;
  node->cap = 0;
}

static void cogito_window_rebuild(CogitoNode* win) {
  if (!win || !win->builder) return;
  cogito_clear_children(win);
  ErgoVal wv = EV_OBJ(win);
  ergo_retain_val(wv);
  ErgoVal ret = win->builder->fn(win->builder->env, 1, &wv);
  ergo_release_val(wv);
  ergo_release_val(ret);
  cogito_apply_style_tree(win);
  if (win->auto_size) {
    int dw = 0;
    int dh = 0;
    cogito_window_desired_size(win, &dw, &dh);
    if (dw > 0 && dh > 0) {
      win->w = dw;
      win->h = dh;
      if (cogito_raylib_ready) SetWindowSize(dw, dh);
    }
  }
  cogito_layout_window(win);
}

static CogitoApp* cogito_app_new_obj(void) {
  CogitoApp* app = (CogitoApp*)ergo_obj_new(sizeof(CogitoApp), NULL);
  app->native = NULL;
  return app;
}

static CogitoNode* cogito_window_new_obj(ErgoStr* title, int w, int h) {
  CogitoNode* win = cogito_node_new(COGITO_WINDOW);
  cogito_node_set_text(win, title);
  win->w = w;
  win->h = h;
  return win;
}

static CogitoNode* cogito_button_new_obj(ErgoStr* label) {
  CogitoNode* btn = cogito_node_new(COGITO_BUTTON);
  cogito_node_set_text(btn, label);
  return btn;
}

static CogitoNode* cogito_label_new_obj(ErgoStr* label) {
  CogitoNode* lab = cogito_node_new(COGITO_LABEL);
  cogito_node_set_text(lab, label);
  return lab;
}

static CogitoNode* cogito_checkbox_new_obj(ErgoStr* label, ErgoVal group) {
  CogitoNode* cb = cogito_node_new(COGITO_CHECKBOX);
  cogito_node_set_text(cb, label);
  cogito_node_set_group(cb, group);
  return cb;
}

static CogitoNode* cogito_switch_new_obj(ErgoStr* label) {
  CogitoNode* sw = cogito_node_new(COGITO_SWITCH);
  cogito_node_set_text(sw, label);
  return sw;
}

static CogitoNode* cogito_iconbtn_new_obj(ErgoStr* label) {
  CogitoNode* btn = cogito_node_new(COGITO_ICONBTN);
  cogito_node_set_text(btn, label);
  return btn;
}

static CogitoNode* cogito_appbar_new_obj(ErgoStr* title, ErgoStr* subtitle) {
  CogitoNode* ab = cogito_node_new(COGITO_APPBAR);
  cogito_node_set_text(ab, title);
  cogito_node_set_subtitle(ab, subtitle);
  return ab;
}

static CogitoNode* cogito_vstack_new_obj(void) {
  return cogito_node_new(COGITO_VSTACK);
}

static CogitoNode* cogito_hstack_new_obj(void) {
  return cogito_node_new(COGITO_HSTACK);
}

static CogitoNode* cogito_list_new_obj(void) {
  return cogito_node_new(COGITO_LIST);
}

static CogitoNode* cogito_grid_new_obj(int cols) {
  CogitoNode* g = cogito_node_new(COGITO_GRID);
  g->grid_cols = cols > 0 ? cols : 1;
  return g;
}

static ErgoVal cogito_app_new(void) {
  if (cogito_debug_enabled()) {
    fprintf(stderr, "cogito: app_new\n");
    fflush(stderr);
  }
  return EV_OBJ(cogito_app_new_obj());
}

static ErgoVal cogito_window_new(ErgoVal title, ErgoVal w, ErgoVal h) {
  if (cogito_debug_enabled()) {
    fprintf(stderr, "cogito: window_new\n");
    fflush(stderr);
  }
  ErgoStr* ts = stdr_to_string(title);
  int wi = (int)ergo_as_int(w);
  int hi = (int)ergo_as_int(h);
  CogitoNode* win = cogito_window_new_obj(ts, wi, hi);
  return EV_OBJ(win);
}

static ErgoVal cogito_button_new(ErgoVal text) {
  if (cogito_debug_enabled()) {
    fprintf(stderr, "cogito: button_new\n");
    fflush(stderr);
  }
  ErgoStr* ts = stdr_to_string(text);
  CogitoNode* btn = cogito_button_new_obj(ts);
  return EV_OBJ(btn);
}

static ErgoVal cogito_label_new(ErgoVal text) {
  ErgoStr* ts = stdr_to_string(text);
  CogitoNode* lab = cogito_label_new_obj(ts);
  return EV_OBJ(lab);
}

static ErgoVal cogito_checkbox_new(ErgoVal text, ErgoVal group) {
  ErgoStr* ts = stdr_to_string(text);
  CogitoNode* cb = cogito_checkbox_new_obj(ts, group);
  return EV_OBJ(cb);
}

static ErgoVal cogito_switch_new(ErgoVal text) {
  ErgoStr* ts = stdr_to_string(text);
  CogitoNode* sw = cogito_switch_new_obj(ts);
  return EV_OBJ(sw);
}

static ErgoVal cogito_iconbtn_new(ErgoVal text) {
  ErgoStr* ts = stdr_to_string(text);
  CogitoNode* btn = cogito_iconbtn_new_obj(ts);
  return EV_OBJ(btn);
}

static ErgoVal cogito_appbar_new(ErgoVal title, ErgoVal subtitle) {
  ErgoStr* ts = stdr_to_string(title);
  ErgoStr* ss = stdr_to_string(subtitle);
  CogitoNode* ab = cogito_appbar_new_obj(ts, ss);
  return EV_OBJ(ab);
}

static ErgoVal cogito_vstack_new(void) {
  return EV_OBJ(cogito_vstack_new_obj());
}

static ErgoVal cogito_hstack_new(void) {
  return EV_OBJ(cogito_hstack_new_obj());
}

static ErgoVal cogito_list_new(void) {
  return EV_OBJ(cogito_list_new_obj());
}

static ErgoVal cogito_grid_new(ErgoVal cols) {
  int c = (int)ergo_as_int(cols);
  return EV_OBJ(cogito_grid_new_obj(c));
}

static void cogito_container_add(ErgoVal parentv, ErgoVal childv) {
  if (parentv.tag != EVT_OBJ || childv.tag != EVT_OBJ) ergo_trap("cogito.container_add expects objects");
  CogitoNode* parent = (CogitoNode*)parentv.as.p;
  CogitoNode* child = (CogitoNode*)childv.as.p;
  cogito_children_add(parent, child);
}

static void cogito_container_set_margins(ErgoVal nodev, ErgoVal left, ErgoVal top, ErgoVal right, ErgoVal bottom) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.container_set_margins expects object");
  CogitoNode* node = (CogitoNode*)nodev.as.p;
  node->margin_left = (int)ergo_as_int(left);
  node->margin_top = (int)ergo_as_int(top);
  node->margin_right = (int)ergo_as_int(right);
  node->margin_bottom = (int)ergo_as_int(bottom);
  node->margin_set = true;
}

static void cogito_container_set_align(ErgoVal nodev, ErgoVal alignv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.container_set_align expects object");
  CogitoNode* node = (CogitoNode*)nodev.as.p;
  node->align = (int)ergo_as_int(alignv);
}

static void cogito_container_set_padding(ErgoVal nodev, ErgoVal left, ErgoVal top, ErgoVal right, ErgoVal bottom) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.container_set_padding expects object");
  CogitoNode* node = (CogitoNode*)nodev.as.p;
  node->padding_left = (int)ergo_as_int(left);
  node->padding_top = (int)ergo_as_int(top);
  node->padding_right = (int)ergo_as_int(right);
  node->padding_bottom = (int)ergo_as_int(bottom);
  node->padding_set = true;
}

static void cogito_window_set_autosize(ErgoVal winv, ErgoVal onv) {
  if (winv.tag != EVT_OBJ) ergo_trap("cogito.window_set_autosize expects window");
  CogitoNode* win = (CogitoNode*)winv.as.p;
  win->auto_size = ergo_as_bool(onv);
}

static void cogito_build(ErgoVal nodev, ErgoVal builder) {
  if (builder.tag != EVT_FN) ergo_trap("cogito.build expects function");
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.build expects object");
  ErgoVal arg = nodev;
  ergo_retain_val(arg);
  ErgoVal ret = ergo_call(builder, 1, &arg);
  ergo_release_val(arg);
  ergo_release_val(ret);
}

static void cogito_window_set_builder(ErgoVal winv, ErgoVal builder) {
  if (winv.tag != EVT_OBJ) ergo_trap("cogito.window_set_builder expects window");
  if (builder.tag != EVT_FN) ergo_trap("cogito.window_set_builder expects function");
  CogitoNode* win = (CogitoNode*)winv.as.p;
  if (win->builder) ergo_release_val(EV_FN(win->builder));
  win->builder = (ErgoFn*)builder.as.p;
  ergo_retain_val(EV_FN(win->builder));
}

static ErgoVal cogito_state_new(ErgoVal initial) {
  CogitoState* s = cogito_state_new_obj(initial);
  return EV_OBJ(s);
}

static ErgoVal cogito_state_get(ErgoVal sv) {
  if (sv.tag != EVT_OBJ) ergo_trap("cogito.state_get expects state");
  CogitoState* s = (CogitoState*)sv.as.p;
  ErgoVal v = s->value;
  ergo_retain_val(v);
  return v;
}

static void cogito_state_set(ErgoVal sv, ErgoVal nv) {
  if (sv.tag != EVT_OBJ) ergo_trap("cogito.state_set expects state");
  CogitoState* s = (CogitoState*)sv.as.p;
  if (s->value.tag != EVT_NULL) ergo_release_val(s->value);
  s->value = nv;
  if (nv.tag != EVT_NULL) ergo_retain_val(nv);
  (void)sv;
  extern CogitoNode* cogito_active_window;
  if (cogito_active_window) {
    cogito_window_rebuild(cogito_active_window);
  }
}

static void cogito_button_set_text(ErgoVal btnv, ErgoVal text) {
  if (btnv.tag != EVT_OBJ) ergo_trap("cogito.button_set_text expects button");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  ErgoStr* ts = stdr_to_string(text);
  cogito_node_set_text(btn, ts);
}

static void cogito_iconbtn_set_text(ErgoVal btnv, ErgoVal text) {
  if (btnv.tag != EVT_OBJ) ergo_trap("cogito.iconbtn_set_text expects button");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  ErgoStr* ts = stdr_to_string(text);
  cogito_node_set_text(btn, ts);
}

static void cogito_checkbox_set_checked(ErgoVal cbv, ErgoVal checked) {
  if (cbv.tag != EVT_OBJ) ergo_trap("cogito.checkbox_set_checked expects checkbox");
  CogitoNode* cb = (CogitoNode*)cbv.as.p;
  cb->checked = ergo_as_bool(checked);
}

static ErgoVal cogito_checkbox_get_checked(ErgoVal cbv) {
  if (cbv.tag != EVT_OBJ) ergo_trap("cogito.checkbox_get_checked expects checkbox");
  CogitoNode* cb = (CogitoNode*)cbv.as.p;
  return EV_BOOL(cb->checked);
}

static void cogito_switch_set_checked(ErgoVal swv, ErgoVal checked) {
  if (swv.tag != EVT_OBJ) ergo_trap("cogito.switch_set_checked expects switch");
  CogitoNode* sw = (CogitoNode*)swv.as.p;
  sw->checked = ergo_as_bool(checked);
}

static ErgoVal cogito_switch_get_checked(ErgoVal swv) {
  if (swv.tag != EVT_OBJ) ergo_trap("cogito.switch_get_checked expects switch");
  CogitoNode* sw = (CogitoNode*)swv.as.p;
  return EV_BOOL(sw->checked);
}

static void cogito_checkbox_on_change(ErgoVal cbv, ErgoVal handler) {
  if (cbv.tag != EVT_OBJ) ergo_trap("cogito.checkbox_on_change expects checkbox");
  CogitoNode* cb = (CogitoNode*)cbv.as.p;
  if (cb->on_change) ergo_release_val(EV_FN(cb->on_change));
  cb->on_change = (ErgoFn*)handler.as.p;
  ergo_retain_val(EV_FN(cb->on_change));
}

static void cogito_switch_on_change(ErgoVal swv, ErgoVal handler) {
  if (swv.tag != EVT_OBJ) ergo_trap("cogito.switch_on_change expects switch");
  CogitoNode* sw = (CogitoNode*)swv.as.p;
  if (sw->on_change) ergo_release_val(EV_FN(sw->on_change));
  sw->on_change = (ErgoFn*)handler.as.p;
  ergo_retain_val(EV_FN(sw->on_change));
}

static void cogito_list_on_select(ErgoVal listv, ErgoVal handler) {
  if (listv.tag != EVT_OBJ) ergo_trap("cogito.list_on_select expects list");
  CogitoNode* list = (CogitoNode*)listv.as.p;
  if (list->on_select) ergo_release_val(EV_FN(list->on_select));
  list->on_select = (ErgoFn*)handler.as.p;
  ergo_retain_val(EV_FN(list->on_select));
}

static void cogito_list_on_activate(ErgoVal listv, ErgoVal handler) {
  if (listv.tag != EVT_OBJ) ergo_trap("cogito.list_on_activate expects list");
  CogitoNode* list = (CogitoNode*)listv.as.p;
  if (list->on_activate) ergo_release_val(EV_FN(list->on_activate));
  list->on_activate = (ErgoFn*)handler.as.p;
  ergo_retain_val(EV_FN(list->on_activate));
}

static void cogito_grid_on_select(ErgoVal gridv, ErgoVal handler) {
  if (gridv.tag != EVT_OBJ) ergo_trap("cogito.grid_on_select expects grid");
  CogitoNode* grid = (CogitoNode*)gridv.as.p;
  if (grid->on_select) ergo_release_val(EV_FN(grid->on_select));
  grid->on_select = (ErgoFn*)handler.as.p;
  ergo_retain_val(EV_FN(grid->on_select));
}

static void cogito_grid_on_activate(ErgoVal gridv, ErgoVal handler) {
  if (gridv.tag != EVT_OBJ) ergo_trap("cogito.grid_on_activate expects grid");
  CogitoNode* grid = (CogitoNode*)gridv.as.p;
  if (grid->on_activate) ergo_release_val(EV_FN(grid->on_activate));
  grid->on_activate = (ErgoFn*)handler.as.p;
  ergo_retain_val(EV_FN(grid->on_activate));
}

static void cogito_button_on_click(ErgoVal btnv, ErgoVal handler) {
  if (btnv.tag != EVT_OBJ) ergo_trap("cogito.button_on_click expects button");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (btn->on_click) ergo_release_val(EV_FN(btn->on_click));
  btn->on_click = (ErgoFn*)handler.as.p;
  ergo_retain_val(EV_FN(btn->on_click));
}

static void cogito_iconbtn_on_click(ErgoVal btnv, ErgoVal handler) {
  if (btnv.tag != EVT_OBJ) ergo_trap("cogito.iconbtn_on_click expects button");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (btn->on_click) ergo_release_val(EV_FN(btn->on_click));
  btn->on_click = (ErgoFn*)handler.as.p;
  ergo_retain_val(EV_FN(btn->on_click));
}

static void cogito_button_add_menu(ErgoVal btnv, ErgoVal label, ErgoVal handler) {
  if (btnv.tag != EVT_OBJ) ergo_trap("cogito.button_add_menu expects button");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  ErgoStr* ts = stdr_to_string(label);
  ErgoFn* fn = handler.tag == EVT_FN ? (ErgoFn*)handler.as.p : NULL;
  cogito_node_add_menu(btn, ts, fn);
}

static void cogito_iconbtn_add_menu(ErgoVal btnv, ErgoVal label, ErgoVal handler) {
  cogito_button_add_menu(btnv, label, handler);
}

static ErgoVal cogito_appbar_add_button(ErgoVal appv, ErgoVal text, ErgoVal handler) {
  if (appv.tag != EVT_OBJ) ergo_trap("cogito.appbar_add_button expects appbar");
  CogitoNode* ab = (CogitoNode*)appv.as.p;
  ErgoStr* ts = stdr_to_string(text);
  CogitoNode* btn = cogito_iconbtn_new_obj(ts);
  if (handler.tag == EVT_FN) {
    btn->on_click = (ErgoFn*)handler.as.p;
    ergo_retain_val(EV_FN(btn->on_click));
  }
  cogito_children_add(ab, btn);
  return EV_OBJ(btn);
}

static int cogito_css_hex_value(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
  if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
  return -1;
}

static void cogito_css_skip_ws(const char** p) {
  while (**p) {
    if (isspace((unsigned char)**p)) { (*p)++; continue; }
    if ((*p)[0] == '/' && (*p)[1] == '/') {
      (*p) += 2;
      while (**p && **p != '\n') (*p)++;
      continue;
    }
    if ((*p)[0] == '/' && (*p)[1] == '*') {
      (*p) += 2;
      while (**p && !((*p)[0] == '*' && (*p)[1] == '/')) (*p)++;
      if (**p) (*p) += 2;
      continue;
    }
    break;
  }
}

static bool cogito_css_read_ident(const char** p, char* out, size_t cap) {
  size_t len = 0;
  if (!isalpha((unsigned char)**p) && **p != '_' && **p != '-' && **p != '.' && **p != '#') return false;
  while (**p && (isalnum((unsigned char)**p) || **p == '_' || **p == '-' || **p == '.' || **p == '#')) {
    if (len + 1 < cap) out[len++] = **p;
    (*p)++;
  }
  out[len] = 0;
  return len > 0;
}

static bool cogito_css_read_number(const char** p, double* out) {
  char* end = NULL;
  double v = strtod(*p, &end);
  if (end == *p) return false;
  *p = end;
  if ((*p)[0] == 'p' && (*p)[1] == 'x') (*p) += 2;
  *out = v;
  return true;
}

static bool cogito_css_parse_hex_color(const char** p, Color* out) {
  if (**p != '#') return false;
  const char* s = *p + 1;
  int len = 0;
  while (cogito_css_hex_value(s[len]) >= 0) len++;
  if (len != 3 && len != 4 && len != 6 && len != 8) return false;
  int r = 0, g = 0, b = 0, a = 255;
  if (len == 3 || len == 4) {
    r = cogito_css_hex_value(s[0]) * 17;
    g = cogito_css_hex_value(s[1]) * 17;
    b = cogito_css_hex_value(s[2]) * 17;
    if (len == 4) a = cogito_css_hex_value(s[3]) * 17;
  } else if (len == 6 || len == 8) {
    r = cogito_css_hex_value(s[0]) * 16 + cogito_css_hex_value(s[1]);
    g = cogito_css_hex_value(s[2]) * 16 + cogito_css_hex_value(s[3]);
    b = cogito_css_hex_value(s[4]) * 16 + cogito_css_hex_value(s[5]);
    if (len == 8) a = cogito_css_hex_value(s[6]) * 16 + cogito_css_hex_value(s[7]);
  }
  *out = cogito_rgba(r, g, b, a);
  *p += 1 + len;
  return true;
}

static bool cogito_css_parse_named_color(const char* name, Color* out) {
  if (!name) return false;
  if (strcmp(name, "white") == 0) { *out = cogito_rgba(255, 255, 255, 255); return true; }
  if (strcmp(name, "black") == 0) { *out = cogito_rgba(0, 0, 0, 255); return true; }
  if (strcmp(name, "gray") == 0 || strcmp(name, "grey") == 0) { *out = cogito_rgba(128, 128, 128, 255); return true; }
  if (strcmp(name, "red") == 0) { *out = cogito_rgba(220, 60, 60, 255); return true; }
  if (strcmp(name, "green") == 0) { *out = cogito_rgba(60, 170, 90, 255); return true; }
  if (strcmp(name, "blue") == 0) { *out = cogito_rgba(70, 110, 200, 255); return true; }
  if (strcmp(name, "yellow") == 0) { *out = cogito_rgba(230, 210, 90, 255); return true; }
  if (strcmp(name, "orange") == 0) { *out = cogito_rgba(230, 150, 60, 255); return true; }
  if (strcmp(name, "purple") == 0) { *out = cogito_rgba(160, 100, 210, 255); return true; }
  return false;
}

static bool cogito_css_parse_rgb_color(const char** p, Color* out) {
  const char* s = *p;
  bool has_alpha = false;
  if (strncmp(s, "rgba", 4) == 0) {
    has_alpha = true;
    s += 4;
  } else if (strncmp(s, "rgb", 3) == 0) {
    s += 3;
  } else {
    return false;
  }
  cogito_css_skip_ws(&s);
  if (*s != '(') return false;
  s++;
  double vals[4] = {0, 0, 0, 1};
  int count = 0;
  while (*s && count < (has_alpha ? 4 : 3)) {
    cogito_css_skip_ws(&s);
    double v = 0;
    if (!cogito_css_read_number(&s, &v)) break;
    vals[count++] = v;
    cogito_css_skip_ws(&s);
    if (*s == ',') s++;
  }
  cogito_css_skip_ws(&s);
  if (*s != ')') return false;
  s++;
  if (count < (has_alpha ? 4 : 3)) return false;
  int r = (int)lround(vals[0]);
  int g = (int)lround(vals[1]);
  int b = (int)lround(vals[2]);
  int a = 255;
  if (has_alpha) {
    if (vals[3] <= 1.0) a = (int)lround(vals[3] * 255.0);
    else a = (int)lround(vals[3]);
  }
  *out = cogito_rgba(r, g, b, a);
  *p = s;
  return true;
}

static bool cogito_css_parse_color(const char** p, Color* out) {
  cogito_css_skip_ws(p);
  if (**p == '#') return cogito_css_parse_hex_color(p, out);
  if (strncmp(*p, "rgb", 3) == 0) return cogito_css_parse_rgb_color(p, out);
  char name[32];
  const char* s = *p;
  if (!cogito_css_read_ident(&s, name, sizeof(name))) return false;
  if (cogito_css_parse_named_color(name, out)) { *p = s; return true; }
  return false;
}

static int cogito_css_parse_ints(const char** p, int* out, int max) {
  int count = 0;
  while (count < max) {
    cogito_css_skip_ws(p);
    double v = 0;
    if (!cogito_css_read_number(p, &v)) break;
    out[count++] = (int)lround(v);
    cogito_css_skip_ws(p);
    if (**p == ',') (*p)++;
  }
  return count;
}

static void cogito_style_set_padding(CogitoStyle* s, int top, int right, int bottom, int left) {
  s->has_padding = true;
  s->padding_top = top;
  s->padding_right = right;
  s->padding_bottom = bottom;
  s->padding_left = left;
}

static void cogito_style_set_margin(CogitoStyle* s, int top, int right, int bottom, int left) {
  s->has_margin = true;
  s->margin_top = top;
  s->margin_right = right;
  s->margin_bottom = bottom;
  s->margin_left = left;
}

static void cogito_css_apply_property(CogitoStyle* style, const char* prop, const char** p) {
  if (strcmp(prop, "background") == 0 || strcmp(prop, "background-color") == 0) {
    Color c;
    if (cogito_css_parse_color(p, &c)) { style->has_bg = true; style->bg = c; }
    return;
  }
  if (strcmp(prop, "color") == 0 || strcmp(prop, "text-color") == 0) {
    Color c;
    if (cogito_css_parse_color(p, &c)) { style->has_text = true; style->text = c; }
    return;
  }
  if (strcmp(prop, "border-color") == 0) {
    Color c;
    if (cogito_css_parse_color(p, &c)) { style->has_border = true; style->border = c; }
    return;
  }
  if (strcmp(prop, "border-width") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_border_width = true; style->border_width = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "border-radius") == 0 || strcmp(prop, "radius") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_radius = true; style->radius = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "font-size") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_font_size = true; style->font_size = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "padding") == 0) {
    int vals[4] = {0, 0, 0, 0};
    int count = cogito_css_parse_ints(p, vals, 4);
    if (count == 1) {
      cogito_style_set_padding(style, vals[0], vals[0], vals[0], vals[0]);
    } else if (count == 2) {
      cogito_style_set_padding(style, vals[0], vals[1], vals[0], vals[1]);
    } else if (count == 3) {
      cogito_style_set_padding(style, vals[0], vals[1], vals[2], vals[1]);
    } else if (count >= 4) {
      cogito_style_set_padding(style, vals[0], vals[1], vals[2], vals[3]);
    }
    return;
  }
  if (strcmp(prop, "padding-left") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_padding_left = true; style->padding_left = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "padding-top") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_padding_top = true; style->padding_top = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "padding-right") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_padding_right = true; style->padding_right = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "padding-bottom") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_padding_bottom = true; style->padding_bottom = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "margin") == 0) {
    int vals[4] = {0, 0, 0, 0};
    int count = cogito_css_parse_ints(p, vals, 4);
    if (count == 1) {
      cogito_style_set_margin(style, vals[0], vals[0], vals[0], vals[0]);
    } else if (count == 2) {
      cogito_style_set_margin(style, vals[0], vals[1], vals[0], vals[1]);
    } else if (count == 3) {
      cogito_style_set_margin(style, vals[0], vals[1], vals[2], vals[1]);
    } else if (count >= 4) {
      cogito_style_set_margin(style, vals[0], vals[1], vals[2], vals[3]);
    }
    return;
  }
  if (strcmp(prop, "margin-left") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_margin_left = true; style->margin_left = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "margin-top") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_margin_top = true; style->margin_top = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "margin-right") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_margin_right = true; style->margin_right = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "margin-bottom") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_margin_bottom = true; style->margin_bottom = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "selection-color") == 0 || strcmp(prop, "selection-background") == 0) {
    Color c;
    if (cogito_css_parse_color(p, &c)) { style->has_selection = true; style->selection = c; }
    return;
  }
}

static bool cogito_css_selector_kind(const char* sel, CogitoKind* out_kind, bool* out_base) {
  if (!sel || !sel[0]) return false;
  const char* s = sel;
  if (*s == '.') s++;
  if (*s == '#') s++;
  if (strcmp(s, "*") == 0 || strcmp(s, "all") == 0 || strcmp(s, "base") == 0 || strcmp(s, "root") == 0) {
    if (out_base) *out_base = true;
    return true;
  }
  if (strcmp(s, "window") == 0) { if (out_kind) *out_kind = COGITO_WINDOW; return true; }
  if (strcmp(s, "vstack") == 0) { if (out_kind) *out_kind = COGITO_VSTACK; return true; }
  if (strcmp(s, "hstack") == 0) { if (out_kind) *out_kind = COGITO_HSTACK; return true; }
  if (strcmp(s, "button") == 0) { if (out_kind) *out_kind = COGITO_BUTTON; return true; }
  if (strcmp(s, "iconbtn") == 0 || strcmp(s, "icon-button") == 0) { if (out_kind) *out_kind = COGITO_ICONBTN; return true; }
  if (strcmp(s, "label") == 0) { if (out_kind) *out_kind = COGITO_LABEL; return true; }
  if (strcmp(s, "checkbox") == 0) { if (out_kind) *out_kind = COGITO_CHECKBOX; return true; }
  if (strcmp(s, "switch") == 0) { if (out_kind) *out_kind = COGITO_SWITCH; return true; }
  if (strcmp(s, "list") == 0) { if (out_kind) *out_kind = COGITO_LIST; return true; }
  if (strcmp(s, "grid") == 0) { if (out_kind) *out_kind = COGITO_GRID; return true; }
  if (strcmp(s, "appbar") == 0 || strcmp(s, "app-bar") == 0) { if (out_kind) *out_kind = COGITO_APPBAR; return true; }
  return false;
}

static void cogito_parse_css(const char* src) {
  const char* p = src;
  while (*p) {
    cogito_css_skip_ws(&p);
    if (!*p) break;
    char sel[64];
    if (*p == '*') {
      sel[0] = '*';
      sel[1] = 0;
      p++;
    } else if (!cogito_css_read_ident(&p, sel, sizeof(sel))) {
      p++;
      continue;
    }
    cogito_css_skip_ws(&p);
    if (*p != '{') {
      while (*p && *p != '{') p++;
      if (*p == '{') p++;
    } else {
      p++;
    }
    bool is_base = false;
    CogitoKind kind = COGITO_WINDOW;
    if (!cogito_css_selector_kind(sel, &kind, &is_base)) {
      int depth = 1;
      while (*p && depth > 0) {
        if (*p == '{') depth++;
        else if (*p == '}') depth--;
        p++;
      }
      continue;
    }
    CogitoStyle* style = is_base ? &cogito_theme.base : &cogito_theme.per_kind[kind];
    while (*p) {
      cogito_css_skip_ws(&p);
      if (*p == '}') { p++; break; }
      char prop[64];
      if (!cogito_css_read_ident(&p, prop, sizeof(prop))) { p++; continue; }
      cogito_css_skip_ws(&p);
      if (*p == ':') {
        p++;
      } else {
        while (*p && *p != ';' && *p != '}') p++;
        if (*p == ';') p++;
        continue;
      }
      const char* vp = p;
      cogito_css_apply_property(style, prop, &vp);
      while (*p && *p != ';' && *p != '}') p++;
      if (*p == ';') p++;
    }
  }
}

static char* cogito_read_file(const char* path, size_t* out_len) {
  if (!path || !path[0]) return NULL;
  FILE* f = fopen(path, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END);
  long len = ftell(f);
  if (len < 0) { fclose(f); return NULL; }
  fseek(f, 0, SEEK_SET);
  char* buf = (char*)malloc((size_t)len + 1);
  if (!buf) { fclose(f); return NULL; }
  size_t got = fread(buf, 1, (size_t)len, f);
  fclose(f);
  buf[got] = 0;
  if (out_len) *out_len = got;
  return buf;
}

static void cogito_load_css_file(const char* path) {
  size_t len = 0;
  char* src = cogito_read_file(path, &len);
  if (!src) {
    if (cogito_debug_enabled()) {
      fprintf(stderr, "cogito: css not found: %s\n", path ? path : "");
      fflush(stderr);
    }
    return;
  }
  cogito_theme_initialized = true;
  cogito_theme_set_defaults();
  cogito_parse_css(src);
  free(src);
  extern CogitoNode* cogito_active_window;
  if (cogito_active_window) {
    cogito_apply_style_tree(cogito_active_window);
    if (cogito_active_window->auto_size) {
      int dw = 0;
      int dh = 0;
      cogito_window_desired_size(cogito_active_window, &dw, &dh);
      if (dw > 0 && dh > 0) {
        cogito_active_window->w = dw;
        cogito_active_window->h = dh;
        if (cogito_raylib_ready) SetWindowSize(dw, dh);
      }
    }
    cogito_window_relayout(cogito_active_window);
  }
}

static void cogito_load_css(ErgoVal pathv) {
  ErgoStr* ps = stdr_to_string(pathv);
  if (ps && ps->data) {
    cogito_load_css_file(ps->data);
  }
  if (ps) ergo_release_val(EV_STR(ps));
}

static void cogito_menu_close(void) {
  cogito_menu.open = false;
  cogito_menu.owner = NULL;
}

static void cogito_menu_open_for(CogitoNode* n) {
  if (!n || n->menu_len == 0) return;
  int font_size = cogito_font_size();
  int max_w = 0;
  for (size_t i = 0; i < n->menu_len; i++) {
    const char* text = n->menu_labels[i] ? n->menu_labels[i]->data : "";
    int w = cogito_text_width(text);
    if (w > max_w) max_w = w;
  }
  int pad = 10;
  int item_h = font_size + 10;
  int w = max_w + pad * 2;
  int h = (int)n->menu_len * item_h;
  cogito_menu.open = true;
  cogito_menu.owner = n;
  cogito_menu.item_h = item_h;
  cogito_menu.w = w;
  cogito_menu.h = h;
  cogito_menu.x = n->x;
  cogito_menu.y = n->y + n->h + 4;
}

static int cogito_menu_hit(const CogitoMenu* menu, int x, int y) {
  if (!menu || !menu->open) return -1;
  if (x < menu->x || y < menu->y || x > menu->x + menu->w || y > menu->y + menu->h) return -1;
  int idx = (y - menu->y) / menu->item_h;
  if (idx < 0) return -1;
  if (!menu->owner) return -1;
  if ((size_t)idx >= menu->owner->menu_len) return -1;
  return idx;
}

static float cogito_roundness_for(int w, int h, int radius) {
  if (radius <= 0) return 0.0f;
  float min = (float)(w < h ? w : h);
  if (min <= 0.0f) return 0.0f;
  float roundness = (float)radius / (min * 0.5f);
  if (roundness > 1.0f) roundness = 1.0f;
  if (roundness < 0.0f) roundness = 0.0f;
  return roundness;
}

static void cogito_draw_rect(int x, int y, int w, int h, Color bg, int radius) {
  if (w <= 0 || h <= 0) return;
  Rectangle r = { (float)x, (float)y, (float)w, (float)h };
  float roundness = cogito_roundness_for(w, h, radius);
  if (roundness > 0.0f) DrawRectangleRounded(r, roundness, 8, bg);
  else DrawRectangleRec(r, bg);
}

static void cogito_draw_rect_lines(int x, int y, int w, int h, Color border, int radius, int thickness) {
  if (w <= 0 || h <= 0 || thickness <= 0) return;
  Rectangle r = { (float)x, (float)y, (float)w, (float)h };
  float roundness = cogito_roundness_for(w, h, radius);
  if (roundness > 0.0f) {
    DrawRectangleRoundedLines(r, roundness, 8, border);
  } else {
    DrawRectangleLinesEx(r, (float)thickness, border);
  }
}

static void cogito_draw_text(int x, int y, const char* text, Color color) {
  if (!text) return;
  if (cogito_font_ready) {
    DrawTextEx(cogito_font, text, (Vector2){(float)x, (float)y}, (float)cogito_font_size(), 1.0f, color);
  } else {
    DrawText(text, x, y, cogito_font_size(), color);
  }
}

static void cogito_draw_node(CogitoNode* n) {
  if (!n) return;
  switch (n->kind) {
    case COGITO_BUTTON: {
      if (n->bg_set || n->border_width > 0) {
        cogito_draw_rect(n->x, n->y, n->w, n->h, n->bg, n->border_radius);
        if (n->border_width > 0) {
          cogito_draw_rect_lines(n->x, n->y, n->w, n->h, n->border_color, n->border_radius, n->border_width);
        }
      }
      if (n->text) {
        int tw = cogito_text_width(n->text->data);
        int th = cogito_text_height();
        int tx = n->x + (n->w - tw) / 2;
        int ty = n->y + (n->h - th) / 2;
        cogito_draw_text(tx, ty, n->text->data, n->text_color);
      }
      break;
    }
    case COGITO_ICONBTN: {
      if (n->bg_set || n->border_width > 0) {
        cogito_draw_rect(n->x, n->y, n->w, n->h, n->bg, n->border_radius);
        if (n->border_width > 0) {
          cogito_draw_rect_lines(n->x, n->y, n->w, n->h, n->border_color, n->border_radius, n->border_width);
        }
      }
      if (n->text) {
        int tw = cogito_text_width(n->text->data);
        int th = cogito_text_height();
        int tx = n->x + (n->w - tw) / 2;
        int ty = n->y + (n->h - th) / 2;
        cogito_draw_text(tx, ty, n->text->data, n->text_color);
      }
      break;
    }
    case COGITO_LABEL: {
      if (n->text) {
        int th = cogito_text_height();
        int ty = n->y + (n->h - th) / 2;
        cogito_draw_text(n->x, ty, n->text->data, n->text_color);
      }
      break;
    }
    case COGITO_CHECKBOX: {
      int box = 16;
      int cy = n->y + (n->h - box) / 2;
      if (n->group.tag != EVT_NULL) {
        DrawCircleLines(n->x + box / 2, cy + box / 2, box / 2, n->border_color.a ? n->border_color : n->text_color);
        if (n->checked) {
          DrawCircle(n->x + box / 2, cy + box / 2, box / 2 - 4, n->text_color);
        }
      } else {
        DrawRectangleLines(n->x, cy, box, box, n->border_color.a ? n->border_color : n->text_color);
        if (n->checked) {
          DrawLine(n->x + 3, cy + 8, n->x + 7, cy + 12, n->text_color);
          DrawLine(n->x + 7, cy + 12, n->x + 13, cy + 4, n->text_color);
        }
      }
      if (n->text) {
        int th = cogito_text_height();
        int ty = n->y + (n->h - th) / 2;
        cogito_draw_text(n->x + box + 6, ty, n->text->data, n->text_color);
      }
      break;
    }
    case COGITO_SWITCH: {
      int h = 18;
      int w = 32;
      int cy = n->y + (n->h - h) / 2;
      int cx = n->x;
      Color track = n->checked ? cogito_rgba(90, 160, 110, 255) : cogito_rgba(170, 170, 170, 255);
      cogito_draw_rect(cx, cy, w, h, track, h / 2);
      int knob = h - 4;
      int kx = n->checked ? (cx + w - knob - 2) : (cx + 2);
      cogito_draw_rect(kx, cy + 2, knob, knob, cogito_rgba(250, 250, 250, 255), knob / 2);
      if (n->text) {
        int th = cogito_text_height();
        int ty = n->y + (n->h - th) / 2;
        cogito_draw_text(n->x + w + 8, ty, n->text->data, n->text_color);
      }
      break;
    }
    case COGITO_LIST:
    case COGITO_GRID: {
      if (n->bg_set || n->border_width > 0) {
        cogito_draw_rect(n->x, n->y, n->w, n->h, n->bg, n->border_radius);
        if (n->border_width > 0) {
          cogito_draw_rect_lines(n->x, n->y, n->w, n->h, n->border_color, n->border_radius, n->border_width);
        }
      }
      if (n->selected >= 0 && (size_t)n->selected < n->len) {
        CogitoNode* sel = n->children[n->selected];
        cogito_draw_rect(sel->x - 2, sel->y - 2, sel->w + 4, sel->h + 4, n->selection_color, 4);
      }
      for (size_t i = 0; i < n->len; i++) {
        cogito_draw_node(n->children[i]);
      }
      break;
    }
    case COGITO_APPBAR: {
      if (n->bg_set || n->border_width > 0) {
        cogito_draw_rect(n->x, n->y, n->w, n->h, n->bg, n->border_radius);
        if (n->border_width > 0) {
          cogito_draw_rect_lines(n->x, n->y, n->w, n->h, n->border_color, n->border_radius, n->border_width);
        }
      }
      int btn = n->appbar_btn_size;
      int by = n->appbar_btn_y;
      int cx = n->appbar_btn_close_x;
      DrawRectangleLines(cx, by, btn, btn, n->text_color);
      cogito_draw_text(cx + 5, by + 1, "x", n->text_color);
      DrawRectangleLines(n->appbar_btn_min_x, by, btn, btn, n->text_color);
      DrawRectangle(n->appbar_btn_min_x + 4, by + btn / 2, btn - 8, 2, n->text_color);
      DrawRectangleLines(n->appbar_btn_max_x, by, btn, btn, n->text_color);
      DrawRectangleLines(n->appbar_btn_max_x + 4, by + 4, btn - 8, btn - 8, n->text_color);
      if (n->text) {
        cogito_draw_text(n->x + 16, n->y + 12, n->text->data, n->text_color);
      }
      if (n->subtitle) {
        cogito_draw_text(n->x + 16, n->y + 32, n->subtitle->data, n->text_color);
      }
      for (size_t i = 0; i < n->len; i++) {
        cogito_draw_node(n->children[i]);
      }
      break;
    }
    case COGITO_VSTACK:
    case COGITO_HSTACK:
    case COGITO_WINDOW:
      for (size_t i = 0; i < n->len; i++) {
        cogito_draw_node(n->children[i]);
      }
      break;
  }
}

static void cogito_draw_menu(void) {
  if (!cogito_menu.open || !cogito_menu.owner) return;
  Color bg = cogito_rgba(250, 250, 250, 255);
  Color border = cogito_rgba(180, 180, 180, 255);
  int x = cogito_menu.x;
  int y = cogito_menu.y;
  int w = cogito_menu.w;
  int h = cogito_menu.h;
  cogito_draw_rect(x, y, w, h, bg, 4);
  cogito_draw_rect_lines(x, y, w, h, border, 4, 1);
  int mx = (int)GetMousePosition().x;
  int my = (int)GetMousePosition().y;
  int hover = cogito_menu_hit(&cogito_menu, mx, my);
  for (size_t i = 0; i < cogito_menu.owner->menu_len; i++) {
    int iy = y + (int)i * cogito_menu.item_h;
    if ((int)i == hover) {
      cogito_draw_rect(x + 2, iy + 2, w - 4, cogito_menu.item_h - 4, cogito_rgba(220, 230, 250, 255), 3);
    }
    const char* text = cogito_menu.owner->menu_labels[i] ? cogito_menu.owner->menu_labels[i]->data : "";
    cogito_draw_text(x + 10, iy + 6, text, cogito_rgba(30, 30, 30, 255));
  }
}

CogitoNode* cogito_active_window = NULL;

static void cogito_run(ErgoVal appv, ErgoVal winv) {
  (void)appv;
  if (winv.tag != EVT_OBJ) ergo_trap("cogito.run expects window");
  CogitoNode* win = (CogitoNode*)winv.as.p;
  cogito_active_window = win;
  cogito_apply_style_tree(win);
  if (win->auto_size) {
    int dw = 0;
    int dh = 0;
    cogito_window_desired_size(win, &dw, &dh);
    if (dw > 0 && dh > 0) {
      win->w = dw;
      win->h = dh;
    }
  }
  const char* title = win->text ? win->text->data : "Cogito";
  CogitoNode* appbar = cogito_find_appbar(win);
#if defined(__APPLE__)
  SetConfigFlags(FLAG_MSAA_4X_HINT);
#else
  if (appbar) {
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_UNDECORATED);
  } else {
    SetConfigFlags(FLAG_MSAA_4X_HINT);
  }
#endif
  InitWindow(win->w, win->h, title);
  cogito_raylib_ready = true;
#if defined(__APPLE__)
  cogito_configure_macos_window(appbar != NULL);
#endif
  const char* font_path = getenv("COGITO_FONT");
  const char* font_size_env = getenv("COGITO_FONT_SIZE");
  if (font_size_env && font_size_env[0]) {
    int sz = atoi(font_size_env);
    if (sz > 6) cogito_font_size_value = sz;
  }
  if (font_path && font_path[0]) {
    cogito_font = LoadFontEx(font_path, cogito_font_size(), NULL, 0);
    if (cogito_font.texture.id != 0) {
      cogito_font_ready = true;
      SetTextureFilter(cogito_font.texture, TEXTURE_FILTER_BILINEAR);
      cogito_font_loaded_size = cogito_font_size();
      cogito_font_from_env = true;
    }
  }
  if (!cogito_font_ready) {
    cogito_font = GetFontDefault();
    cogito_font_ready = true;
    SetTextureFilter(cogito_font.texture, TEXTURE_FILTER_BILINEAR);
    cogito_font_loaded_size = cogito_font_size();
    cogito_font_from_env = false;
  }
  SetTargetFPS(60);
  if (win->auto_size) {
    int dw = 0;
    int dh = 0;
    cogito_window_desired_size(win, &dw, &dh);
    if (dw > 0 && dh > 0) {
      win->w = dw;
      win->h = dh;
      SetWindowSize(dw, dh);
    }
  }
  cogito_layout_window(win);

  while (!WindowShouldClose() && !win->should_close) {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    if (sw != win->w || sh != win->h) {
      win->w = sw;
      win->h = sh;
      cogito_window_relayout(win);
    }
    Vector2 mp = GetMousePosition();
    int mx = (int)mp.x;
    int my = (int)mp.y;
    bool click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if (IsKeyPressed(KEY_ESCAPE) && cogito_menu.open) {
      cogito_menu_close();
    }
#if !defined(__APPLE__)
    if (cogito_drag_pending) {
      if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        Vector2 dp = { mp.x - cogito_drag_pending_mouse.x, mp.y - cogito_drag_pending_mouse.y };
        if (fabsf(dp.x) > 2.0f || fabsf(dp.y) > 2.0f) {
          cogito_drag_active = true;
          cogito_drag_pending = false;
          cogito_drag_start_mouse = cogito_drag_pending_mouse;
          cogito_drag_start_win = GetWindowPosition();
        }
      } else {
        cogito_drag_pending = false;
      }
    }
    if (cogito_drag_active) {
      if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        Vector2 delta = { mp.x - cogito_drag_start_mouse.x, mp.y - cogito_drag_start_mouse.y };
        double now = GetTime();
        if (now - cogito_drag_last_move > 0.016) {
          SetWindowPosition((int)(cogito_drag_start_win.x + delta.x), (int)(cogito_drag_start_win.y + delta.y));
          cogito_drag_last_move = now;
        }
      } else {
        cogito_drag_active = false;
      }
    }
#endif
    if (click) {
      if (cogito_menu.open) {
        int idx = cogito_menu_hit(&cogito_menu, mx, my);
        if (idx >= 0 && cogito_menu.owner) {
          cogito_invoke_menu(cogito_menu.owner, (size_t)idx);
        }
        cogito_menu_close();
      } else {
        if (appbar) {
          if (cogito_hit_rect(mx, my, appbar->appbar_btn_close_x, appbar->appbar_btn_y, appbar->appbar_btn_size, appbar->appbar_btn_size)) {
            cogito_appbar_window_action(win, 0);
          } else if (cogito_hit_rect(mx, my, appbar->appbar_btn_min_x, appbar->appbar_btn_y, appbar->appbar_btn_size, appbar->appbar_btn_size)) {
            cogito_appbar_window_action(win, 1);
          } else if (cogito_hit_rect(mx, my, appbar->appbar_btn_max_x, appbar->appbar_btn_y, appbar->appbar_btn_size, appbar->appbar_btn_size)) {
            cogito_appbar_window_action(win, 2);
          }
#if !defined(__APPLE__)
          else if (cogito_hit_rect(mx, my, appbar->x, appbar->y, appbar->w, appbar->h) &&
                   !cogito_point_over_appbar_buttons(appbar, mx, my)) {
            cogito_drag_pending = true;
            cogito_drag_pending_mouse = mp;
            cogito_drag_last_move = 0.0;
          }
#endif
        }
        CogitoNode* hit = cogito_find_clickable(win, mx, my);
        if (hit) {
          if (hit->kind == COGITO_BUTTON || hit->kind == COGITO_ICONBTN) {
            if (hit->menu_len > 0) {
              cogito_menu_open_for(hit);
            } else {
              cogito_invoke_click(hit);
            }
          } else if (hit->kind == COGITO_CHECKBOX || hit->kind == COGITO_SWITCH) {
            hit->checked = !hit->checked;
            if (hit->checked && hit->group.tag != EVT_NULL) {
              cogito_uncheck_group(win, hit, hit->group);
            }
            cogito_invoke_change(hit);
          }
        }
        cogito_handle_click(win, mx, my, true);
      }
    }

    if (cogito_drag_active) {
      BeginDrawing();
      EndDrawing();
      continue;
    }

    BeginDrawing();
    Color win_bg = win->bg_set ? win->bg : cogito_rgba(245, 245, 245, 255);
    ClearBackground(win_bg);
    cogito_draw_node(win);
    if (cogito_menu.open) cogito_draw_menu();
    EndDrawing();
  }
  CloseWindow();
  if (cogito_font_ready && cogito_font_from_env && cogito_font.texture.id != 0 && cogito_font.glyphCount > 0) {
    UnloadFont(cogito_font);
  }
  cogito_raylib_ready = false;
  cogito_menu_close();
}
