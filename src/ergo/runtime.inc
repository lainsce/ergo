// ---- Ergo runtime (minimal) ----
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
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

// ---- Cogito GUI (minimal cross-platform) ----

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__APPLE__)
#import <Cocoa/Cocoa.h>
@class CogitoTarget;
#else
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#endif

typedef struct CogitoApp {
  ErgoObj base;
#if defined(__APPLE__)
  __strong NSApplication* app;
#else
  void* native;
#endif
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
  COGITO_APPBAR
} CogitoKind;

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
#if !defined(__APPLE__)
  int drag_active;
  int drag_off_x;
  int drag_off_y;
#endif
#if defined(__APPLE__)
  __strong NSView* aux;
  __strong NSView* aux2;
  __strong NSView* aux3;
  __strong NSView* aux4;
#else
  void* aux;
  void* aux2;
  void* aux3;
  void* aux4;
#endif
#if defined(_WIN32)
  HWND hwnd;
  HWND native;
#elif defined(__APPLE__)
  __strong NSWindow* nswin;
  __strong NSView* native;
  __strong id target;
  __strong id delegate;
#else
  Display* dpy;
  Window win;
  GC gc;
  Atom wm_delete;
  void* native;
#endif
} CogitoNode;

typedef struct CogitoState {
  ErgoObj base;
  ErgoVal value;
} CogitoState;

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
#if !defined(__APPLE__)
  n->drag_active = 0;
  n->drag_off_x = 0;
  n->drag_off_y = 0;
#endif
#if defined(__APPLE__)
  n->aux = nil;
  n->aux2 = nil;
  n->aux3 = nil;
  n->aux4 = nil;
#else
  n->aux = NULL;
  n->aux2 = NULL;
  n->aux3 = NULL;
  n->aux4 = NULL;
#endif
  if (kind == COGITO_WINDOW) {
    n->padding_left = 0;
    n->padding_top = 0;
    n->padding_right = 0;
    n->padding_bottom = 0;
    n->auto_size = true;
  } else {
    n->padding_left = 6;
    n->padding_top = 6;
    n->padding_right = 6;
    n->padding_bottom = 6;
    n->auto_size = false;
  }
  n->align = -1;
#if defined(_WIN32)
  n->hwnd = NULL;
  n->native = NULL;
#elif defined(__APPLE__)
  n->nswin = nil;
  n->native = nil;
  n->target = nil;
  n->delegate = nil;
#else
  n->dpy = NULL;
  n->win = 0;
  n->gc = 0;
  n->wm_delete = 0;
  n->native = NULL;
#endif
  if (kind == COGITO_VSTACK) {
    n->margin_left = 18;
    n->margin_top = 0;
    n->margin_right = 18;
    n->margin_bottom = 0;
  }
  return n;
}

static void cogito_node_set_text(CogitoNode* n, ErgoStr* s) {
  if (n->text) ergo_release_val(EV_STR(n->text));
  n->text = s;
}

static void cogito_node_set_subtitle(CogitoNode* n, ErgoStr* s) {
  if (n->subtitle) ergo_release_val(EV_STR(n->subtitle));
  n->subtitle = s;
}

static void cogito_node_set_group(CogitoNode* n, ErgoVal group) {
  if (n->group.tag != EVT_NULL) ergo_release_val(n->group);
  n->group = group;
  if (group.tag != EVT_NULL) ergo_retain_val(group);
}

static void cogito_children_add(CogitoNode* parent, CogitoNode* child) {
  if (!parent || !child) return;
  if (parent->len == parent->cap) {
    size_t ncap = parent->cap ? parent->cap * 2 : 4;
    parent->children = (CogitoNode**)realloc(parent->children, ncap * sizeof(CogitoNode*));
    parent->cap = ncap;
  }
  parent->children[parent->len++] = child;
  child->parent = parent;
  ergo_retain_val(EV_OBJ(child));
}

static void cogito_node_add_menu(CogitoNode* n, ErgoStr* label, ErgoFn* handler) {
  if (!n) return;
  if (n->menu_len == n->menu_cap) {
    size_t ncap = n->menu_cap ? n->menu_cap * 2 : 4;
    n->menu_labels = (ErgoStr**)realloc(n->menu_labels, ncap * sizeof(ErgoStr*));
    n->menu_handlers = (ErgoFn**)realloc(n->menu_handlers, ncap * sizeof(ErgoFn*));
    n->menu_cap = ncap;
  }
  n->menu_labels[n->menu_len] = label;
  n->menu_handlers[n->menu_len] = handler;
  if (label) ergo_retain_val(EV_STR(label));
  if (handler) ergo_retain_val(EV_FN(handler));
  n->menu_len += 1;
}

static void cogito_intrinsic_size(CogitoNode* n, int* out_w, int* out_h) {
  int char_w = 8;
#if defined(__APPLE__)
  char_w = 7;
#endif
  int text_w = 0;
  if (n->text) text_w = (int)n->text->len * char_w;
  int w = 0;
  int h = 0;
  int pad = 6;
  switch (n->kind) {
    case COGITO_BUTTON:
      w = text_w + 24;
      h = 21;
      break;
    case COGITO_ICONBTN:
      w = 32;
      h = 32;
      break;
    case COGITO_LABEL:
      w = text_w + 4;
      h = 21;
      break;
    case COGITO_CHECKBOX:
      w = text_w + 32;
      h = 24;
      break;
    case COGITO_SWITCH:
#if defined(__APPLE__)
      w = text_w + 52;
      h = 24;
#else
      w = text_w + 32;
      h = 24;
#endif
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
      h = (has_title && has_sub && has_buttons) ? 100 : 40;
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
static void cogito_attach_node(CogitoNode* win, CogitoNode* n, int content_h);
#if defined(__APPLE__)
static void cogito_update_selection_visuals(CogitoNode* node);
#endif
static void cogito_window_relayout(CogitoNode* win);
#if defined(__APPLE__)
static void cogito_update_selection_visuals(CogitoNode* node);
#endif

static void cogito_layout_vstack(CogitoNode* n, int x, int y) {
  int pad = 6;
  int cy = y + n->margin_top + n->padding_top;
  int cx = x + n->margin_left + n->padding_left;
  int avail_w = n->w - n->margin_left - n->margin_right - n->padding_left - n->padding_right;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    int cw = 0;
    int ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    int cw_total = cw + c->margin_left + c->margin_right;
    int ch_total = ch + c->margin_top + c->margin_bottom;
    int align = (c->align >= 0) ? c->align : n->align;
    bool stretch = (c->align < 0 && n->align == 0);
    if (avail_w > 0) {
      if (align == 1) {
        int ox = (avail_w - cw_total) / 2;
        if (ox < 0) ox = 0;
        cogito_layout_node(c, cx + ox + c->margin_left, cy + c->margin_top, cw, ch);
      } else if (align == 2) {
        int ox = avail_w - cw_total;
        if (ox < 0) ox = 0;
        cogito_layout_node(c, cx + ox + c->margin_left, cy + c->margin_top, cw, ch);
      } else if (stretch) {
        cw = avail_w - c->margin_left - c->margin_right;
        if (cw < 0) cw = 0;
        cogito_layout_node(c, cx + c->margin_left, cy + c->margin_top, cw, ch);
      } else {
        cogito_layout_node(c, cx + c->margin_left, cy + c->margin_top, cw, ch);
      }
    } else {
      cogito_layout_node(c, cx + c->margin_left, cy + c->margin_top, cw, ch);
    }
    cy += ch_total + pad;
  }
}

static void cogito_layout_hstack(CogitoNode* n, int x, int y) {
  int pad = 6;
  int cx = x + n->margin_left + n->padding_left;
  int cy = y + n->margin_top + n->padding_top;
  int avail_h = n->h - n->margin_top - n->margin_bottom - n->padding_top - n->padding_bottom;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    int cw = 0;
    int ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    int cw_total = cw + c->margin_left + c->margin_right;
    int ch_total = ch + c->margin_top + c->margin_bottom;
    int align = (c->align >= 0) ? c->align : n->align;
    bool stretch = (c->align < 0 && n->align == 0);
    if (avail_h > 0) {
      if (align == 1) {
        int oy = (avail_h - ch_total) / 2;
        if (oy < 0) oy = 0;
        cogito_layout_node(c, cx + c->margin_left, cy + oy + c->margin_top, cw, ch);
      } else if (align == 2) {
        int oy = avail_h - ch_total;
        if (oy < 0) oy = 0;
        cogito_layout_node(c, cx + c->margin_left, cy + oy + c->margin_top, cw, ch);
      } else if (stretch) {
        ch = avail_h - c->margin_top - c->margin_bottom;
        if (ch < 0) ch = 0;
        cogito_layout_node(c, cx + c->margin_left, cy + c->margin_top, cw, ch);
      } else {
        cogito_layout_node(c, cx + c->margin_left, cy + c->margin_top, cw, ch);
      }
    } else {
      cogito_layout_node(c, cx + c->margin_left, cy + c->margin_top, cw, ch);
    }
    cx += cw_total + pad;
  }
}

static void cogito_layout_list(CogitoNode* n, int x, int y) {
  int pad = 4;
  int cy = y + pad;
  int maxw = n->w - pad * 2;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    int cw = maxw;
    int ch = 22;
    cogito_layout_node(c, x + pad, cy, cw, ch);
    cy += ch + pad;
  }
}

static void cogito_layout_grid(CogitoNode* n, int x, int y) {
  int pad = 4;
  int cols = n->grid_cols > 0 ? n->grid_cols : 1;
  int rows = (int)((n->len + (size_t)cols - 1) / (size_t)cols);
  int cell_w = (n->w - pad * (cols + 1)) / cols;
  int cell_h = 24;
  int idx = 0;
  for (int r = 0; r < rows; r++) {
    for (int c = 0; c < cols; c++) {
      if ((size_t)idx >= n->len) return;
      CogitoNode* child = n->children[idx++];
      int cx = x + pad + c * (cell_w + pad);
      int cy = y + pad + r * (cell_h + pad);
      cogito_layout_node(child, cx, cy, cell_w, cell_h);
    }
  }
}

static void cogito_layout_node(CogitoNode* n, int x, int y, int w, int h) {
  n->x = x;
  n->y = y;
  n->w = w;
  n->h = h;
  switch (n->kind) {
    case COGITO_APPBAR: {
      int pad_x = 18;
      int pad_y = 6;
      int btn_size = 24;
      int btn_gap = 12;
      int title_h = 24;
      int subtitle_h = 18;
      int close_x = x + w - 9 - btn_size;
      int close_y = y + 9;
      bool has_title = n->text && n->text->len;
      bool has_sub = n->subtitle && n->subtitle->len;
      int title_block_h = 0;
      if (has_title) title_block_h += title_h;
      if (has_sub) title_block_h += subtitle_h + (has_title ? 4 : 0);
      int buttons_y = y + pad_y + title_block_h + 6;
      n->appbar_btn_size = btn_size;
      n->appbar_btn_y = close_y;
      n->appbar_btn_close_x = close_x;
      n->appbar_btn_min_x = -1;
      n->appbar_btn_max_x = -1;
      int icon_row_h = 0;
      for (size_t i = 0; i < n->len; i++) {
        int cw = 0;
        int ch = 0;
        cogito_intrinsic_size(n->children[i], &cw, &ch);
        if (ch > icon_row_h) icon_row_h = ch;
      }
      if (icon_row_h == 0) icon_row_h = btn_size;
      int cx = x + w - pad_x;
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
#if defined(_WIN32)
  for (size_t i = 0; i < win->len; i++) {
    cogito_destroy_native(win->children[i]);
  }
  for (size_t i = 0; i < win->len; i++) {
    cogito_attach_node(win, win->children[i], win->h);
  }
  if (win->hwnd) InvalidateRect(win->hwnd, NULL, TRUE);
#elif defined(__APPLE__)
  if (win->nswin) {
    NSView* content = [win->nswin contentView];
    NSArray* subs = [content subviews];
    for (NSView* v in subs) {
      [v removeFromSuperview];
    }
    for (size_t i = 0; i < win->len; i++) {
      cogito_attach_node(win, win->children[i], win->h);
    }
    cogito_update_selection_visuals(win);
    [win->nswin display];
  }
#else
  if (win->dpy && win->win) {
    XClearWindow(win->dpy, win->win);
    cogito_draw_node(win->dpy, win->win, win->gc, win);
  }
#endif
}

static void cogito_invoke(ErgoFn* fn, int argc, ErgoVal* argv) {
  if (!fn) return;
  ErgoVal ret = fn->fn(fn->env, argc, argv);
  ergo_release_val(ret);
}

static void cogito_invoke_click(CogitoNode* n) {
  if (!n || !n->on_click) return;
  ErgoVal arg = EV_OBJ(n);
  ergo_retain_val(arg);
  cogito_invoke(n->on_click, 1, &arg);
  ergo_release_val(arg);
}

static void cogito_invoke_menu(CogitoNode* n, size_t idx) {
  if (!n) return;
  if (idx >= n->menu_len) return;
  ErgoFn* fn = n->menu_handlers[idx];
  if (!fn) return;
  ErgoVal arg = EV_OBJ(n);
  ergo_retain_val(arg);
  cogito_invoke(fn, 1, &arg);
  ergo_release_val(arg);
}

static void cogito_invoke_change(CogitoNode* n) {
  if (!n || !n->on_change) return;
  ErgoVal arg = EV_OBJ(n);
  ergo_retain_val(arg);
  cogito_invoke(n->on_change, 1, &arg);
  ergo_release_val(arg);
}
#if !defined(_WIN32) && !defined(__APPLE__)
static void cogito_show_menu_x11(CogitoNode* n, CogitoNode* win) {
  if (!n || !win || n->menu_len == 0) return;
  Display* dpy = win->dpy;
  Window root = DefaultRootWindow(dpy);
  int screen = DefaultScreen(dpy);
  int sx = n->x + n->w / 2;
  int sy = n->y + n->h;
  Window child;
  int rx, ry;
  XTranslateCoordinates(dpy, win->win, root, sx, sy, &rx, &ry, &child);
  int pad = 6;
  int item_h = 22;
  int maxw = 80;
  for (size_t i = 0; i < n->menu_len; i++) {
    int w = (int)(n->menu_labels[i] ? n->menu_labels[i]->len * 8 + 12 : 40);
    if (w > maxw) maxw = w;
  }
  int mw = maxw + pad * 2;
  int mh = (int)n->menu_len * item_h + pad * 2;
  XSetWindowAttributes attrs;
  attrs.override_redirect = True;
  attrs.background_pixel = WhitePixel(dpy, screen);
  Window popup = XCreateWindow(dpy, root, rx - mw / 2, ry, (unsigned)mw, (unsigned)mh, 1,
                               CopyFromParent, InputOutput, CopyFromParent,
                               CWOverrideRedirect | CWBackPixel, &attrs);
  XSelectInput(dpy, popup, ExposureMask | ButtonPressMask | ButtonReleaseMask);
  XMapRaised(dpy, popup);
  XEvent ev;
  bool done = false;
  while (!done) {
    XNextEvent(dpy, &ev);
    if (ev.type == Expose) {
      GC gc = DefaultGC(dpy, screen);
      XSetForeground(dpy, gc, 0xdddddd);
      XFillRectangle(dpy, popup, gc, 0, 0, (unsigned)mw, (unsigned)mh);
      XSetForeground(dpy, gc, BlackPixel(dpy, screen));
      for (size_t i = 0; i < n->menu_len; i++) {
        int iy = pad + (int)i * item_h;
        XDrawString(dpy, popup, gc, pad, iy + 16, n->menu_labels[i] ? n->menu_labels[i]->data : "", n->menu_labels[i] ? (int)n->menu_labels[i]->len : 0);
      }
    } else if (ev.type == ButtonRelease) {
      int mx = ev.xbutton.x;
      int my = ev.xbutton.y;
      if (mx >= 0 && mx <= mw && my >= 0 && my <= mh) {
        int idx = (my - pad) / item_h;
        if (idx >= 0 && (size_t)idx < n->menu_len) {
          cogito_invoke_menu(n, (size_t)idx);
        }
      }
      done = true;
    }
  }
  XDestroyWindow(dpy, popup);
}
#endif

#if defined(_WIN32)
static void cogito_show_menu_win(CogitoNode* n) {
  if (!n || n->menu_len == 0) return;
  HWND hwnd = (HWND)n->native;
  if (!hwnd) return;
  HMENU menu = CreatePopupMenu();
  const UINT base = 10000;
  for (size_t i = 0; i < n->menu_len; i++) {
    const char* title = n->menu_labels[i] ? n->menu_labels[i]->data : "";
    AppendMenu(menu, MF_STRING, base + (UINT)i, title);
  }
  RECT r;
  POINT pt = {0, 0};
  if (GetWindowRect(hwnd, &r)) {
    pt.x = (r.left + r.right) / 2;
    pt.y = r.bottom;
  }
  UINT cmd = TrackPopupMenu(menu, TPM_CENTERALIGN | TPM_TOPALIGN | TPM_RETURNCMD, pt.x, pt.y, 0, GetParent(hwnd), NULL);
  DestroyMenu(menu);
  if (cmd >= base) {
    size_t idx = cmd - base;
    cogito_invoke_menu(n, idx);
  }
}
#endif

static void cogito_invoke_select(CogitoNode* n, int idx, bool activate) {
  ErgoVal arg = EV_INT(idx);
  if (n->on_select) cogito_invoke(n->on_select, 1, &arg);
  if (activate && n->on_activate) cogito_invoke(n->on_activate, 1, &arg);
}

static void cogito_appbar_window_action(CogitoNode* win, int action) {
#if defined(_WIN32)
  if (!win || !win->hwnd) return;
  if (action == 0) {
    PostMessage(win->hwnd, WM_CLOSE, 0, 0);
  } else if (action == 1) {
    ShowWindow(win->hwnd, SW_MINIMIZE);
  } else if (action == 2) {
    ShowWindow(win->hwnd, SW_MAXIMIZE);
  }
#elif defined(__APPLE__)
  if (!win || !win->nswin) return;
  if (action == 0) {
    [win->nswin performClose:nil];
  } else if (action == 1) {
    [win->nswin miniaturize:nil];
  } else if (action == 2) {
    [win->nswin zoom:nil];
  }
#else
  if (!win || !win->dpy) return;
  if (action == 0) win->should_close = true;
#endif
}

#if defined(_WIN32)
static void cogito_window_apply_rounding(CogitoNode* win) {
  if (!win || !win->hwnd) return;
  CogitoNode* app = cogito_find_appbar(win);
  if (!app) return;
  HRGN rgn = CreateRoundRectRgn(0, 0, win->w + 1, win->h + 1, 32, 32);
  SetWindowRgn(win->hwnd, rgn, TRUE);
}
#endif

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
#if defined(_WIN32)
      if (root->native) SendMessage(root->native, BM_SETCHECK, BST_UNCHECKED, 0);
#elif defined(__APPLE__)
      if (root->native && [root->native respondsToSelector:@selector(setState:)]) {
        [(id)root->native setState:NSControlStateValueOff];
      }
#endif
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

static bool cogito_hit_node(CogitoNode* n, int x, int y) {
  return x >= n->x && y >= n->y && x <= n->x + n->w && y <= n->y + n->h;
}

static bool cogito_hit_rect(int x, int y, int rx, int ry, int rw, int rh) {
  return x >= rx && y >= ry && x <= rx + rw && y <= ry + rh;
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
  if (n->kind == COGITO_BUTTON || n->kind == COGITO_ICONBTN || n->kind == COGITO_CHECKBOX || n->kind == COGITO_SWITCH || n->kind == COGITO_LIST || n->kind == COGITO_GRID) {
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

static void cogito_destroy_native(CogitoNode* node) {
  if (!node) return;
#if defined(_WIN32)
  if (node->native) {
    DestroyWindow((HWND)node->native);
    node->native = NULL;
  }
  if (node->aux) {
    DestroyWindow((HWND)node->aux);
    node->aux = NULL;
  }
  if (node->aux2) {
    DestroyWindow((HWND)node->aux2);
    node->aux2 = NULL;
  }
  if (node->aux3) {
    DestroyWindow((HWND)node->aux3);
    node->aux3 = NULL;
  }
  if (node->aux4) {
    DestroyWindow((HWND)node->aux4);
    node->aux4 = NULL;
  }
#elif defined(__APPLE__)
  if (node->native) {
    NSView* view = (NSView*)node->native;
    [view removeFromSuperview];
    node->native = nil;
  }
  if (node->aux) {
    NSView* view = node->aux;
    [view removeFromSuperview];
    node->aux = nil;
  }
  if (node->aux2) {
    NSView* view = node->aux2;
    [view removeFromSuperview];
    node->aux2 = nil;
  }
  if (node->aux3) {
    NSView* view = node->aux3;
    [view removeFromSuperview];
    node->aux3 = nil;
  }
  if (node->aux4) {
    NSView* view = node->aux4;
    [view removeFromSuperview];
    node->aux4 = nil;
  }
#else
  (void)node;
#endif
  for (size_t i = 0; i < node->len; i++) {
    cogito_destroy_native(node->children[i]);
  }
}

static void cogito_window_rebuild(CogitoNode* win) {
  if (!win || !win->builder) return;
#if defined(_WIN32)
  for (size_t i = 0; i < win->len; i++) {
    cogito_destroy_native(win->children[i]);
  }
#elif defined(__APPLE__)
  if (win->nswin) {
    NSView* content = [win->nswin contentView];
    NSArray* subs = [content subviews];
    for (NSView* v in subs) {
      [v removeFromSuperview];
    }
  }
#endif
  cogito_clear_children(win);
  ErgoVal wv = EV_OBJ(win);
  ergo_retain_val(wv);
  ErgoVal ret = win->builder->fn(win->builder->env, 1, &wv);
  ergo_release_val(wv);
  ergo_release_val(ret);
  cogito_layout_window(win);
#if defined(_WIN32)
  for (size_t i = 0; i < win->len; i++) {
    cogito_attach_node(win, win->children[i], win->h);
  }
  if (win->hwnd) InvalidateRect(win->hwnd, NULL, TRUE);
#elif defined(__APPLE__)
  if (win->nswin) {
    for (size_t i = 0; i < win->len; i++) {
      cogito_attach_node(win, win->children[i], win->h);
    }
    cogito_update_selection_visuals(win);
    [win->nswin display];
  }
#else
  if (win->dpy && win->win) {
    XClearWindow(win->dpy, win->win);
    cogito_draw_node(win->dpy, win->win, win->gc, win);
  }
#endif
}

#if defined(__APPLE__)
@interface CogitoTarget : NSObject
@property (assign) void* node;
@end
@implementation CogitoTarget
- (void)onAction:(id)sender {
  (void)sender;
  CogitoNode* n = (CogitoNode*)self.node;
  if (!n) return;
  if (n->kind == COGITO_BUTTON) {
    if (n->menu_len > 0) {
      NSButton* btn = (NSButton*)n->native;
      if (!btn) return;
      NSMenu* menu = [[NSMenu alloc] initWithTitle:@""];
      for (size_t i = 0; i < n->menu_len; i++) {
        NSString* title = [NSString stringWithUTF8String:(n->menu_labels[i] ? n->menu_labels[i]->data : "")];
        NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:title action:@selector(onMenuItem:) keyEquivalent:@""];
        [item setTarget:self];
        [item setTag:(NSInteger)i];
        [menu addItem:item];
      }
      NSRect f = [btn frame];
      NSSize ms = [menu size];
      NSPoint p = NSMakePoint(NSMidX(f) - ms.width * 0.5, NSMinY(f) - 4.0);
      [menu popUpMenuPositioningItem:nil atLocation:p inView:[btn superview]];
    } else {
      cogito_invoke_click(n);
    }
  } else if (n->kind == COGITO_ICONBTN) {
    if (n->menu_len > 0) {
      NSButton* btn = (NSButton*)n->native;
      if (!btn) return;
      NSMenu* menu = [[NSMenu alloc] initWithTitle:@""];
      for (size_t i = 0; i < n->menu_len; i++) {
        NSString* title = [NSString stringWithUTF8String:(n->menu_labels[i] ? n->menu_labels[i]->data : "")];
        NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:title action:@selector(onMenuItem:) keyEquivalent:@""];
        [item setTarget:self];
        [item setTag:(NSInteger)i];
        [menu addItem:item];
      }
      NSRect f = [btn frame];
      NSSize ms = [menu size];
      NSPoint p = NSMakePoint(NSMidX(f) - ms.width * 0.5, NSMinY(f) - 4.0);
      [menu popUpMenuPositioningItem:nil atLocation:p inView:[btn superview]];
    } else {
      cogito_invoke_click(n);
    }
  } else if (n->kind == COGITO_CHECKBOX || n->kind == COGITO_SWITCH) {
    id ctrl = n->native;
    if (ctrl && [ctrl respondsToSelector:@selector(state)]) {
      n->checked = ([(id)ctrl state] == NSControlStateValueOn);
    }
    if (n->checked && n->group.tag != EVT_NULL) {
      CogitoNode* root = cogito_root(n);
      cogito_uncheck_group(root, n, n->group);
    }
    cogito_invoke_change(n);
  }
}

- (void)onMenuItem:(id)sender {
  CogitoNode* n = (CogitoNode*)self.node;
  if (!n) return;
  NSInteger idx = [sender tag];
  if (idx < 0 || (size_t)idx >= n->menu_len) return;
  cogito_invoke_menu(n, (size_t)idx);
}
@end

@interface CogitoWindowDelegate : NSObject<NSWindowDelegate>
@property (assign) void* win;
@end
@implementation CogitoWindowDelegate
- (void)windowWillClose:(NSNotification *)notification {
  (void)notification;
  [NSApp stop:nil];
  [NSApp terminate:nil];
}
- (void)windowDidResize:(NSNotification *)notification {
  (void)notification;
  CogitoNode* win = (CogitoNode*)self.win;
  if (!win || !win->nswin) return;
  NSRect r = [[win->nswin contentView] frame];
  win->w = (int)r.size.width;
  win->h = (int)r.size.height;
  cogito_window_relayout(win);
}
@end

@interface CogitoAppBarView : NSView
@property (assign) NSWindow* targetWindow;
@end
@implementation CogitoAppBarView
- (void)mouseDown:(NSEvent *)event {
  if (self.targetWindow) {
    [self.targetWindow performWindowDragWithEvent:event];
  }
}
@end

static void cogito_update_selection_visuals(CogitoNode* node) {
  if (!node) return;
  if ((node->kind == COGITO_LIST || node->kind == COGITO_GRID) && node->selected >= 0) {
    for (size_t i = 0; i < node->len; i++) {
      CogitoNode* child = node->children[i];
      if (child->kind == COGITO_LABEL && child->native) {
        NSTextField* tf = (NSTextField*)child->native;
        if ((int)i == node->selected) {
          if (!child->aux) {
            NSView* bg = [[NSView alloc] initWithFrame:[tf frame]];
            [bg setWantsLayer:YES];
            bg.layer.cornerRadius = 4.0;
            bg.layer.masksToBounds = YES;
            bg.layer.backgroundColor = [[[NSColor selectedControlColor] colorWithAlphaComponent:0.35] CGColor];
            [[tf superview] addSubview:bg positioned:NSWindowBelow relativeTo:tf];
            child->aux = bg;
          }
          NSRect r = [tf frame];
          r.origin.x -= 4;
          r.origin.y -= 2;
          r.size.width += 8;
          r.size.height += 4;
          [child->aux setFrame:r];
        } else {
          if (child->aux) {
            [child->aux removeFromSuperview];
            child->aux = nil;
          }
        }
      }
    }
  }
  for (size_t i = 0; i < node->len; i++) {
    cogito_update_selection_visuals(node->children[i]);
  }
}

static void cogito_update_appbar_hitrects(CogitoNode* win) {
  (void)win;
}
#endif

static CogitoApp* cogito_app_new_obj(void) {
  CogitoApp* app = (CogitoApp*)ergo_obj_new(sizeof(CogitoApp), NULL);
#if defined(__APPLE__)
  app->app = [NSApplication sharedApplication];
#else
  app->native = NULL;
#endif
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
  CogitoNode* win = NULL;
  if (sv.tag == EVT_OBJ) {
    (void)sv;
  }
  (void)win;
  // For now, rebuild the most recent running window if it has a builder.
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
#if defined(_WIN32)
  if (btn->native) SetWindowText(btn->native, ts->data);
#elif defined(__APPLE__)
  if (btn->native) {
    NSString* ns = [NSString stringWithUTF8String:ts->data];
    [(NSButton*)btn->native setTitle:ns];
  }
#endif
}

static void cogito_iconbtn_set_text(ErgoVal btnv, ErgoVal text) {
  if (btnv.tag != EVT_OBJ) ergo_trap("cogito.iconbtn_set_text expects button");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  ErgoStr* ts = stdr_to_string(text);
  cogito_node_set_text(btn, ts);
#if defined(_WIN32)
  if (btn->native) SetWindowText(btn->native, ts->data);
#elif defined(__APPLE__)
  if (btn->native) {
    NSString* ns = [NSString stringWithUTF8String:ts->data];
    [(NSButton*)btn->native setTitle:ns];
  }
#endif
}

static void cogito_checkbox_set_checked(ErgoVal cbv, ErgoVal checked) {
  if (cbv.tag != EVT_OBJ) ergo_trap("cogito.checkbox_set_checked expects checkbox");
  CogitoNode* cb = (CogitoNode*)cbv.as.p;
  cb->checked = ergo_as_bool(checked);
#if defined(_WIN32)
  if (cb->native) SendMessage(cb->native, BM_SETCHECK, cb->checked ? BST_CHECKED : BST_UNCHECKED, 0);
#elif defined(__APPLE__)
  if (cb->native && [cb->native respondsToSelector:@selector(setState:)]) {
    [(id)cb->native setState:(cb->checked ? NSControlStateValueOn : NSControlStateValueOff)];
  }
#endif
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
#if defined(_WIN32)
  if (sw->native) SendMessage(sw->native, BM_SETCHECK, sw->checked ? BST_CHECKED : BST_UNCHECKED, 0);
#elif defined(__APPLE__)
  if (sw->native && [sw->native respondsToSelector:@selector(setState:)]) {
    [(id)sw->native setState:(sw->checked ? NSControlStateValueOn : NSControlStateValueOff)];
  }
#endif
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
  ErgoFn* fn = NULL;
  if (handler.tag == EVT_FN) fn = (ErgoFn*)handler.as.p;
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

#if defined(_WIN32)
static LRESULT CALLBACK cogito_wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if (msg == WM_COMMAND) {
    if (HIWORD(wParam) == BN_CLICKED) {
      HWND hctrl = (HWND)lParam;
      CogitoNode* n = (CogitoNode*)GetWindowLongPtr(hctrl, GWLP_USERDATA);
      if (n) {
        if (n->kind == COGITO_BUTTON || n->kind == COGITO_ICONBTN) {
          if (n->menu_len > 0) {
            cogito_show_menu_win(n);
          } else {
            cogito_invoke_click(n);
          }
        } else if (n->kind == COGITO_CHECKBOX || n->kind == COGITO_SWITCH) {
          n->checked = SendMessage(hctrl, BM_GETCHECK, 0, 0) == BST_CHECKED;
          if (n->checked && n->group.tag != EVT_NULL) {
            CogitoNode* root = cogito_root(n);
            cogito_uncheck_group(root, n, n->group);
          }
          cogito_invoke_change(n);
        }
      } else {
        CogitoNode* win = (CogitoNode*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        int id = LOWORD(wParam);
        if (win && id == 9001) cogito_appbar_window_action(win, 0);
        if (win && id == 9002) cogito_appbar_window_action(win, 1);
        if (win && id == 9003) cogito_appbar_window_action(win, 2);
      }
    }
  } else if (msg == WM_SIZE) {
    CogitoNode* win = (CogitoNode*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (win) {
      win->w = (int)LOWORD(lParam);
      win->h = (int)HIWORD(lParam);
      cogito_window_relayout(win);
      cogito_window_apply_rounding(win);
    }
  } else if (msg == WM_DESTROY) {
    PostQuitMessage(0);
    return 0;
  } else if (msg == WM_NCHITTEST) {
    LRESULT hit = DefWindowProc(hwnd, msg, wParam, lParam);
    CogitoNode* win = (CogitoNode*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (!win) return hit;
    CogitoNode* app = cogito_find_appbar(win);
    if (!app) return hit;
    POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
    ScreenToClient(hwnd, &pt);
    int x = pt.x;
    int y = pt.y;
    if (cogito_hit_rect(x, y, app->x, app->y, app->w, app->h) &&
        !cogito_point_over_appbar_buttons(app, x, y)) {
      return HTCAPTION;
    }
    return hit;
  } else if (msg == WM_PAINT) {
    CogitoNode* win = (CogitoNode*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (win) {
      CogitoNode* app = cogito_find_appbar(win);
      if (app) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT r;
        r.left = app->x;
        r.top = app->y;
        r.right = app->x + app->w;
        r.bottom = app->y + app->h;
        HBRUSH brush = CreateSolidBrush(RGB(242, 242, 242));
        FillRect(hdc, &r, brush);
        DeleteObject(brush);
        EndPaint(hwnd, &ps);
        return 0;
      }
    }
  }
  return DefWindowProc(hwnd, msg, wParam, lParam);
}
#endif

#if !defined(_WIN32) && !defined(__APPLE__)
static void cogito_draw_text(Display* dpy, Window win, GC gc, int x, int y, const char* text) {
  if (!text) return;
  XDrawString(dpy, win, gc, x, y, text, (int)strlen(text));
}

static void cogito_draw_node(Display* dpy, Window win, GC gc, CogitoNode* n) {
  if (!n) return;
  int tx = n->x + 6;
  int ty = n->y + 16;
  switch (n->kind) {
    case COGITO_BUTTON:
      XDrawRectangle(dpy, win, gc, n->x, n->y, (unsigned)n->w, (unsigned)n->h);
      cogito_draw_text(dpy, win, gc, tx, ty, n->text ? n->text->data : "");
      break;
    case COGITO_ICONBTN:
      XDrawRectangle(dpy, win, gc, n->x, n->y, (unsigned)n->w, (unsigned)n->h);
      cogito_draw_text(dpy, win, gc, n->x + 8, n->y + 16, n->text ? n->text->data : "");
      break;
    case COGITO_LABEL:
      cogito_draw_text(dpy, win, gc, n->x, n->y + 14, n->text ? n->text->data : "");
      break;
    case COGITO_CHECKBOX:
    case COGITO_SWITCH: {
      if (n->kind == COGITO_CHECKBOX && n->group.tag != EVT_NULL) {
        /* radio */
        XDrawArc(dpy, win, gc, n->x, n->y, 16, 16, 0, 360 * 64);
        if (n->checked) {
          XFillArc(dpy, win, gc, n->x + 4, n->y + 4, 8, 8, 0, 360 * 64);
        }
      } else {
        /* checkbox */
        XDrawRectangle(dpy, win, gc, n->x, n->y, 16, 16);
        if (n->checked) {
          XDrawLine(dpy, win, gc, n->x + 3, n->y + 8, n->x + 7, n->y + 12);
          XDrawLine(dpy, win, gc, n->x + 7, n->y + 12, n->x + 13, n->y + 4);
        }
      }
      cogito_draw_text(dpy, win, gc, n->x + 22, n->y + 14, n->text ? n->text->data : "");
      break;
    }
    case COGITO_LIST:
    case COGITO_GRID:
      XDrawRectangle(dpy, win, gc, n->x, n->y, (unsigned)n->w, (unsigned)n->h);
      if (n->selected >= 0 && (size_t)n->selected < n->len) {
        CogitoNode* sel = n->children[n->selected];
        unsigned long old = BlackPixel(dpy, DefaultScreen(dpy));
        XSetForeground(dpy, gc, 0xdddddd);
        XFillRectangle(dpy, win, gc, sel->x, sel->y, (unsigned)sel->w, (unsigned)sel->h);
        XSetForeground(dpy, gc, old);
      }
      for (size_t i = 0; i < n->len; i++) {
        cogito_draw_node(dpy, win, gc, n->children[i]);
      }
      break;
    case COGITO_APPBAR: {
      unsigned long old = BlackPixel(dpy, DefaultScreen(dpy));
      XFillRectangle(dpy, win, gc, n->x, n->y, (unsigned)n->w, (unsigned)n->h);
      XSetForeground(dpy, gc, 0xffffff);
      XDrawString(dpy, win, gc, n->appbar_btn_close_x + 4, n->appbar_btn_y + 12, "x", 1);
      XSetForeground(dpy, gc, old);
      if (n->text) XDrawString(dpy, win, gc, n->x + 16, n->y + 22, n->text->data, (int)n->text->len);
      if (n->subtitle) XDrawString(dpy, win, gc, n->x + 16, n->y + 40, n->subtitle->data, (int)n->subtitle->len);
      for (size_t i = 0; i < n->len; i++) {
        cogito_draw_node(dpy, win, gc, n->children[i]);
      }
      break;
    }
    case COGITO_VSTACK:
    case COGITO_HSTACK:
    case COGITO_WINDOW:
    case COGITO_APPBAR:
      for (size_t i = 0; i < n->len; i++) {
        cogito_draw_node(dpy, win, gc, n->children[i]);
      }
      break;
  }
}
#endif

static void cogito_attach_node(CogitoNode* win, CogitoNode* n, int content_h) {
#if defined(_WIN32)
  (void)content_h;
  if (n->kind == COGITO_BUTTON || n->kind == COGITO_ICONBTN || n->kind == COGITO_CHECKBOX || n->kind == COGITO_SWITCH || n->kind == COGITO_LABEL) {
    DWORD style = WS_CHILD | WS_VISIBLE;
    const char* cls = "STATIC";
    if (n->kind == COGITO_BUTTON) {
      cls = "BUTTON";
      style |= BS_PUSHBUTTON;
    } else if (n->kind == COGITO_ICONBTN) {
      cls = "BUTTON";
      style |= BS_FLAT;
    } else if (n->kind == COGITO_CHECKBOX || n->kind == COGITO_SWITCH) {
      cls = "BUTTON";
      if (n->kind == COGITO_CHECKBOX && n->group.tag != EVT_NULL) {
        style |= BS_AUTORADIOBUTTON;
      } else {
        style |= BS_AUTOCHECKBOX;
      }
    }
    const char* title = n->text ? n->text->data : "";
    HWND h = CreateWindow(cls, title, style, n->x, n->y, n->w, n->h, win->hwnd, NULL, GetModuleHandle(NULL), NULL);
    SetWindowLongPtr(h, GWLP_USERDATA, (LONG_PTR)n);
    n->native = h;
    if (n->kind == COGITO_CHECKBOX || n->kind == COGITO_SWITCH) {
      SendMessage(h, BM_SETCHECK, n->checked ? BST_CHECKED : BST_UNCHECKED, 0);
    }
  } else if (n->kind == COGITO_APPBAR) {
    int by = n->y;
    int bs = n->appbar_btn_size;
    int byc = n->appbar_btn_y;
    HWND hclose = CreateWindow("BUTTON", "X", WS_CHILD | WS_VISIBLE | BS_FLAT, n->appbar_btn_close_x, byc, bs, bs, win->hwnd, (HMENU)(INT_PTR)9001, GetModuleHandle(NULL), NULL);
    n->native = hclose;
    int text_w = n->appbar_btn_close_x - (n->x + 16) - 12;
    if (text_w < 0) text_w = 0;
    if (n->text && n->text->len) {
      CreateWindow("STATIC", n->text->data, WS_CHILD | WS_VISIBLE, n->x + 16, by + 12, text_w, 20, win->hwnd, NULL, GetModuleHandle(NULL), NULL);
    }
    if (n->subtitle && n->subtitle->len) {
      CreateWindow("STATIC", n->subtitle->data, WS_CHILD | WS_VISIBLE, n->x + 16, by + 32, text_w, 20, win->hwnd, NULL, GetModuleHandle(NULL), NULL);
    }
  }
#elif defined(__APPLE__)
  if (!win->nswin) return;
  NSView* content = [win->nswin contentView];
  CGFloat by = (CGFloat)(content_h - n->y - n->h);
  if (n->kind == COGITO_BUTTON || n->kind == COGITO_ICONBTN || n->kind == COGITO_CHECKBOX || n->kind == COGITO_SWITCH) {
    NSString* nsTitle = [NSString stringWithUTF8String:(n->text ? n->text->data : "")];
    id ctrl = nil;
    bool is_native_switch = false;
    if (n->kind == COGITO_SWITCH) {
      Class swClass = NSClassFromString(@"NSSwitch");
      if (swClass) {
        id sw = [swClass new];
        ctrl = sw;
        is_native_switch = true;
      }
    }
    if (!ctrl) {
      NSButton* btn = [NSButton buttonWithTitle:nsTitle target:nil action:nil];
      if (n->kind == COGITO_CHECKBOX) {
        if (n->group.tag != EVT_NULL) {
          [btn setButtonType:NSButtonTypeRadio];
        } else {
          [btn setButtonType:NSButtonTypeSwitch];
        }
      } else if (n->kind == COGITO_SWITCH) {
        [btn setButtonType:NSButtonTypeSwitch];
      } else if (n->kind == COGITO_ICONBTN) {
        [btn setBordered:NO];
        [btn setWantsLayer:YES];
        btn.layer.cornerRadius = 21.0;
        btn.layer.backgroundColor = [[NSColor colorWithWhite:0.9 alpha:1.0] CGColor];
        [btn setFont:[NSFont systemFontOfSize:24.0]];
      }
      ctrl = btn;
    }
    CogitoTarget* target = [CogitoTarget new];
    target.node = n;
    if ([ctrl respondsToSelector:@selector(setTarget:)]) {
      [ctrl setTarget:target];
    }
    if ([ctrl respondsToSelector:@selector(setAction:)]) {
      [ctrl setAction:@selector(onAction:)];
    }
    if ((n->kind == COGITO_CHECKBOX || n->kind == COGITO_SWITCH) && [ctrl respondsToSelector:@selector(setState:)]) {
      [ctrl setState:(n->checked ? NSControlStateValueOn : NSControlStateValueOff)];
    }
    if (is_native_switch && [ctrl respondsToSelector:@selector(sizeToFit)]) {
      [ctrl sizeToFit];
      NSRect r = [ctrl frame];
      r.origin.x = n->x;
      r.origin.y = by + (n->h - r.size.height) * 0.5;
      [ctrl setFrame:r];
    } else {
      [ctrl setFrame:NSMakeRect(n->x, by, n->w, n->h)];
    }
    [content addSubview:ctrl];
    n->native = ctrl;
    n->target = target;
    if (is_native_switch && n->text && n->text->len) {
      CGFloat switch_w = [ctrl frame].size.width;
      int label_x = n->x + (int)switch_w + 6;
      NSDictionary* attrs = @{NSFontAttributeName:[NSFont systemFontOfSize:[NSFont systemFontSize]]};
      CGFloat label_w = [nsTitle sizeWithAttributes:attrs].width + 4.0;
      CGFloat label_h = [nsTitle sizeWithAttributes:attrs].height;
      if (label_h < 14.0) label_h = 14.0;
      CGFloat label_y = by + (n->h - label_h) * 0.5;
      NSTextField* tf = [[NSTextField alloc] initWithFrame:NSMakeRect(label_x, label_y, label_w, label_h)];
      [tf setStringValue:nsTitle];
      [tf setFont:[NSFont systemFontOfSize:[NSFont systemFontSize]]];
      [tf setBezeled:NO];
      [tf setDrawsBackground:NO];
      [tf setEditable:NO];
      [tf setSelectable:NO];
      [content addSubview:tf];
      n->aux = tf;
    }
  } else if (n->kind == COGITO_APPBAR) {
    NSWindow* nsw = win->nswin;
    NSButton* close = [nsw standardWindowButton:NSWindowCloseButton];
    NSButton* min = [nsw standardWindowButton:NSWindowMiniaturizeButton];
    NSButton* zoom = [nsw standardWindowButton:NSWindowZoomButton];
    [close setHidden:YES];
    [min setHidden:YES];
    [zoom setHidden:YES];

    CogitoAppBarView* bg = [[CogitoAppBarView alloc] initWithFrame:NSMakeRect(n->x, by, n->w, n->h)];
    [bg setWantsLayer:YES];
    bg.layer.backgroundColor = [[NSColor colorWithWhite:0.95 alpha:1.0] CGColor];
    bg.targetWindow = nsw;
    [content addSubview:bg];
    content.layer.backgroundColor = [[NSColor whiteColor] CGColor];
    [content setWantsLayer:YES];
    content.layer.cornerRadius = 16.0;
    content.layer.masksToBounds = YES;
    n->aux2 = bg;

    NSButton* cb = [NSButton buttonWithTitle:@"×" target:nsw action:@selector(performClose:)];
    [cb setBezelStyle:NSBezelStyleCircular];
    [cb setFrame:NSMakeRect(n->appbar_btn_close_x, by + n->h - n->appbar_btn_size - 9, n->appbar_btn_size, n->appbar_btn_size)];
    [cb setFont:[NSFont systemFontOfSize:14.0]];
    [content addSubview:cb];
    n->native = cb;

    CGFloat title_w = (CGFloat)(n->appbar_btn_close_x - (n->x + 16) - 12);
    if (title_w < 0.0) title_w = 0.0;
    if (n->text && n->text->len) {
      NSTextField* tf = [[NSTextField alloc] initWithFrame:NSMakeRect(n->x + 16, by + n->h - 55, title_w, 20)];
      [tf setStringValue:[NSString stringWithUTF8String:n->text->data]];
      [tf setFont:[NSFont boldSystemFontOfSize:18.0]];
      [tf setTextColor:[NSColor colorWithWhite:0.2 alpha:1.0]];
      [tf setBezeled:NO];
      [tf setDrawsBackground:NO];
      [tf setEditable:NO];
      [tf setSelectable:NO];
      [content addSubview:tf];
      n->aux3 = tf;
    }
    if (n->subtitle && n->subtitle->len) {
      NSTextField* tf2 = [[NSTextField alloc] initWithFrame:NSMakeRect(n->x + 16, by + n->h - 75, title_w, 18)];
      [tf2 setStringValue:[NSString stringWithUTF8String:n->subtitle->data]];
      [tf2 setFont:[NSFont systemFontOfSize:15.0]];
      [tf2 setTextColor:[NSColor colorWithWhite:0.35 alpha:1.0]];
      [tf2 setBezeled:NO];
      [tf2 setDrawsBackground:NO];
      [tf2 setEditable:NO];
      [tf2 setSelectable:NO];
      [content addSubview:tf2];
      n->aux4 = tf2;
    }
  } else if (n->kind == COGITO_LABEL) {
    NSString* nsTitle = [NSString stringWithUTF8String:(n->text ? n->text->data : "")];
    NSTextField* tf = [[NSTextField alloc] initWithFrame:NSMakeRect(n->x, by, n->w, n->h)];
    [tf setStringValue:nsTitle];
    [tf setFont:[NSFont systemFontOfSize:[NSFont systemFontSize]]];
    [tf setBezeled:NO];
    [tf setDrawsBackground:NO];
    [tf setEditable:NO];
    [tf setSelectable:NO];
    [tf sizeToFit];
    NSRect r = [tf frame];
    r.origin.x = n->x;
    r.origin.y = by + (n->h - r.size.height) * 0.5;
    r.size.width = n->w;
    [tf setFrame:r];
    [content addSubview:tf];
    n->native = tf;
  }
#else
  (void)win;
  (void)n;
  (void)content_h;
#endif
  for (size_t i = 0; i < n->len; i++) {
    cogito_attach_node(win, n->children[i], content_h);
  }
}

CogitoNode* cogito_active_window = NULL;

static void cogito_run(ErgoVal appv, ErgoVal winv) {
  (void)appv;
  if (winv.tag != EVT_OBJ) ergo_trap("cogito.run expects window");
  CogitoNode* win = (CogitoNode*)winv.as.p;
  cogito_active_window = win;
  if (win->auto_size) {
    int dw = 0;
    int dh = 0;
    cogito_window_desired_size(win, &dw, &dh);
    if (dw > win->w) win->w = dw;
    if (dh > win->h) win->h = dh;
  }
  cogito_layout_window(win);

#if defined(_WIN32)
  static bool registered = false;
  if (!registered) {
    WNDCLASS wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = cogito_wndproc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "CogitoWindow";
    RegisterClass(&wc);
    registered = true;
  }
  const char* title = win->text ? win->text->data : "Cogito";
  bool has_appbar = false;
  for (size_t i = 0; i < win->len; i++) {
    if (win->children[i]->kind == COGITO_APPBAR) { has_appbar = true; break; }
  }
  DWORD style = has_appbar ? (WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX) : WS_OVERLAPPEDWINDOW;
  win->hwnd = CreateWindowEx(0, "CogitoWindow", title, style,
                             CW_USEDEFAULT, CW_USEDEFAULT, win->w, win->h,
                             NULL, NULL, GetModuleHandle(NULL), NULL);
  SetWindowLongPtr(win->hwnd, GWLP_USERDATA, (LONG_PTR)win);
  ShowWindow(win->hwnd, SW_SHOW);
  UpdateWindow(win->hwnd);
  cogito_window_apply_rounding(win);
  for (size_t i = 0; i < win->len; i++) {
    cogito_attach_node(win, win->children[i], win->h);
  }
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
#elif defined(__APPLE__)
  @autoreleasepool {
    NSApplication* app = [NSApplication sharedApplication];
    if (cogito_debug_enabled()) {
      fprintf(stderr, "cogito: run mac start\n");
      fflush(stderr);
    }
    [app setActivationPolicy:NSApplicationActivationPolicyRegular];
    [app finishLaunching];
    NSRect rect = NSMakeRect(0, 0, win->w, win->h);
    bool has_appbar = false;
    for (size_t i = 0; i < win->len; i++) {
      if (win->children[i]->kind == COGITO_APPBAR) { has_appbar = true; break; }
    }
    NSUInteger style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
    if (has_appbar) {
      style |= NSWindowStyleMaskFullSizeContentView;
    }
    NSWindow* nsw = [[NSWindow alloc] initWithContentRect:rect styleMask:style backing:NSBackingStoreBuffered defer:NO];
    if (has_appbar) {
      [nsw setTitleVisibility:NSWindowTitleHidden];
      [nsw setTitlebarAppearsTransparent:YES];
      [nsw setMovableByWindowBackground:NO];
    }
    NSString* nsTitle = [NSString stringWithUTF8String:(win->text ? win->text->data : "Cogito")];
    [nsw setTitle:nsTitle];
    CogitoWindowDelegate* del = [CogitoWindowDelegate new];
    del.win = win;
    [nsw setDelegate:del];
    win->nswin = nsw;
    win->delegate = del;
    for (size_t i = 0; i < win->len; i++) {
      cogito_attach_node(win, win->children[i], win->h);
    }
    [nsw makeKeyAndOrderFront:nil];
    [nsw orderFrontRegardless];
    [nsw display];
    [app activateIgnoringOtherApps:YES];
    __block CogitoNode* win_ptr = win;
    [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskLeftMouseDown handler:^NSEvent* _Nullable(NSEvent* event) {
      if (!win_ptr || !win_ptr->nswin) return event;
      NSView* content = [win_ptr->nswin contentView];
      NSPoint p = [event locationInWindow];
      p = [content convertPoint:p fromView:nil];
      int mx = (int)p.x;
      int my = win_ptr->h - (int)p.y;
      for (size_t i = 0; i < win_ptr->len; i++) {
        CogitoNode* c = win_ptr->children[i];
        if (c->kind == COGITO_APPBAR) {
          int bx = mx;
          int by = my;
          if (bx >= c->appbar_btn_close_x && bx <= c->appbar_btn_close_x + c->appbar_btn_size &&
              by >= c->appbar_btn_y && by <= c->appbar_btn_y + c->appbar_btn_size) {
            cogito_appbar_window_action(win_ptr, 0);
          }
        }
      }
      int idx = -1;
      CogitoNode* list = cogito_find_list_grid_hit(win_ptr, mx, my, &idx);
      if (list && idx >= 0) {
        cogito_handle_list_select(list, idx, true);
        cogito_update_selection_visuals(win_ptr);
      }
      return event;
    }];
    if (cogito_debug_enabled()) {
      fprintf(stderr, "cogito: run mac loop\n");
      fflush(stderr);
    }
    [app run];
  }
#else
  win->dpy = XOpenDisplay(NULL);
  if (!win->dpy) ergo_trap("cogito: X11 display not available");
  int screen = DefaultScreen(win->dpy);
  win->win = XCreateSimpleWindow(win->dpy, RootWindow(win->dpy, screen), 100, 100, win->w, win->h, 1,
                                 BlackPixel(win->dpy, screen), WhitePixel(win->dpy, screen));
  win->gc = DefaultGC(win->dpy, screen);
  win->wm_delete = XInternAtom(win->dpy, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(win->dpy, win->win, &win->wm_delete, 1);
  XSelectInput(win->dpy, win->win, ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | StructureNotifyMask);
  if (win->text) XStoreName(win->dpy, win->win, win->text->data);
  XMapWindow(win->dpy, win->win);
  XEvent e;
  while (1) {
    XNextEvent(win->dpy, &e);
    if (e.type == Expose) {
      XClearWindow(win->dpy, win->win);
      cogito_draw_node(win->dpy, win->win, win->gc, win);
    } else if (e.type == ButtonPress) {
      int mx = e.xbutton.x;
      int my = e.xbutton.y;
      if (win->len > 0) {
        for (size_t i = 0; i < win->len; i++) {
          CogitoNode* c = win->children[i];
          if (c->kind == COGITO_APPBAR) {
            int bx = mx;
            int by = my;
            if (bx >= c->appbar_btn_close_x && bx <= c->appbar_btn_close_x + c->appbar_btn_size &&
                by >= c->appbar_btn_y && by <= c->appbar_btn_y + c->appbar_btn_size) {
              cogito_appbar_window_action(win, 0);
            }
          }
        }
      }
      CogitoNode* app = cogito_find_appbar(win);
      if (app && cogito_hit_rect(mx, my, app->x, app->y, app->w, app->h) &&
          !cogito_point_over_appbar_buttons(app, mx, my)) {
        win->drag_active = 1;
        win->drag_off_x = mx;
        win->drag_off_y = my;
      }
      CogitoNode* hit = cogito_find_clickable(win, mx, my);
      if (hit) {
        if (hit->kind == COGITO_BUTTON || hit->kind == COGITO_ICONBTN) {
          if (hit->menu_len > 0) {
            cogito_show_menu_x11(hit, win);
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
      XClearWindow(win->dpy, win->win);
      cogito_draw_node(win->dpy, win->win, win->gc, win);
      if (win->should_close) break;
    } else if (e.type == MotionNotify) {
      if (win->drag_active) {
        int nx = e.xmotion.x_root - win->drag_off_x;
        int ny = e.xmotion.y_root - win->drag_off_y;
        XMoveWindow(win->dpy, win->win, nx, ny);
      }
    } else if (e.type == ButtonRelease) {
      win->drag_active = 0;
    } else if (e.type == ConfigureNotify) {
      win->w = e.xconfigure.width;
      win->h = e.xconfigure.height;
      cogito_window_relayout(win);
    } else if (e.type == ClientMessage) {
      if ((Atom)e.xclient.data.l[0] == win->wm_delete) break;
    }
  }
#endif
}
