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
#include <sys/stat.h>
#if defined(__APPLE__)
#include <CoreGraphics/CoreGraphics.h>
#endif
#if defined(_WIN32)
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
  COGITO_ZSTACK,
  COGITO_BUTTON,
  COGITO_ICONBTN,
  COGITO_IMAGE,
  COGITO_LABEL,
  COGITO_CHECKBOX,
  COGITO_SWITCH,
  COGITO_LIST,
  COGITO_GRID,
  COGITO_DIALOG,
  COGITO_DIALOG_SLOT,
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
  bool has_track;
  bool has_track_on;
  bool has_knob;
  bool has_check;
  bool has_font_weight;
  bool has_min_w;
  bool has_min_h;
  bool has_max_w;
  bool has_max_h;
  bool has_appbar_btn_size;
  bool has_appbar_btn_gap;
  bool has_appbar_btn_pad_top;
  bool has_appbar_btn_pad_right;
  bool has_appbar_btn_close_color;
  bool has_appbar_btn_min_color;
  bool has_appbar_btn_max_color;
  bool has_appbar_btn_border_color;
  bool has_appbar_btn_border_width;
  bool has_shadow;
  Color bg;
  Color text;
  Color border;
  Color selection;
  Color track;
  Color track_on;
  Color knob;
  Color check;
  Color appbar_btn_close_color;
  Color appbar_btn_min_color;
  Color appbar_btn_max_color;
  Color appbar_btn_border_color;
  int border_width;
  int radius;
  int font_size;
  int font_weight;
  int min_w;
  int min_h;
  int max_w;
  int max_h;
  int appbar_btn_size;
  int appbar_btn_gap;
  int appbar_btn_pad_top;
  int appbar_btn_pad_right;
  int appbar_btn_border_width;
  int shadow_level;
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
  CogitoStyle per_kind_hover[COGITO_KIND_COUNT];
  CogitoStyle per_kind_active[COGITO_KIND_COUNT];
  CogitoStyle per_kind_checked[COGITO_KIND_COUNT];
  CogitoStyle label_class[4];
  CogitoStyle label_class_hover[4];
  CogitoStyle label_class_active[4];
  CogitoStyle label_class_checked[4];
  CogitoStyle menu;
  CogitoStyle appbar_child_button;
  CogitoStyle appbar_child_button_hover;
  CogitoStyle appbar_child_button_active;
  CogitoStyle appbar_child_iconbtn;
  CogitoStyle appbar_child_iconbtn_hover;
  CogitoStyle appbar_child_iconbtn_active;
  CogitoStyle appbar_win_btn_all;
  CogitoStyle appbar_win_btn_hover_all;
  CogitoStyle appbar_win_btn_active_all;
  CogitoStyle appbar_win_btn[3];
  CogitoStyle appbar_win_btn_hover[3];
  CogitoStyle appbar_win_btn_active[3];
  CogitoStyle checkbox_check;
  CogitoStyle checkbox_check_hover;
  CogitoStyle checkbox_check_active;
  CogitoStyle checkbox_check_checked;
  CogitoStyle radio_check;
  CogitoStyle radio_check_hover;
  CogitoStyle radio_check_active;
  CogitoStyle radio_check_checked;
  CogitoStyle checkbox_box;
  CogitoStyle checkbox_box_hover;
  CogitoStyle checkbox_box_active;
  CogitoStyle checkbox_box_checked;
  CogitoStyle radio_box;
  CogitoStyle radio_box_hover;
  CogitoStyle radio_box_active;
  CogitoStyle radio_box_checked;
  CogitoStyle switch_track;
  CogitoStyle switch_track_hover;
  CogitoStyle switch_track_active;
  CogitoStyle switch_track_checked;
  CogitoStyle switch_knob;
  CogitoStyle switch_knob_hover;
  CogitoStyle switch_knob_active;
  CogitoStyle switch_knob_checked;
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
  ErgoStr* icon;
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
  int appbar_title_y;
  int appbar_subtitle_y;
  char appbar_controls[16];
  struct CogitoNode* dialog;
  bool dialog_open;
  bool resizable;
  bool margin_set;
  bool padding_set;
  bool bg_set;
  bool text_color_set;
  bool border_color_set;
  bool border_width_set;
  bool radius_set;
  bool selection_set;
  bool track_set;
  bool track_on_set;
  bool knob_set;
  bool check_set;
  bool font_weight_set;
  bool min_w_set;
  bool min_h_set;
  bool max_w_set;
  bool shadow_set;
  bool max_h_set;
  Color bg;
  Color text_color;
  Color border_color;
  Color selection_color;
  Color track_color;
  Color track_on_color;
  Color knob_color;
  Color check_color;
  int border_width;
  int border_radius;
  int label_class;
  int font_size;
  bool font_size_set;
  int font_weight;
  int min_w;
  int min_h;
  int max_w;
  int max_h;
  int shadow_level;
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
  int pad_x;
  int pad_y;
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
static bool cogito_css_default_loaded = false;
static int cogito_style_version = 0;
enum {
  COGITO_LABEL_CLASS_NONE = 0,
  COGITO_LABEL_CLASS_TITLE,
  COGITO_LABEL_CLASS_SUBTITLE,
  COGITO_LABEL_CLASS_BODY
};
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

static void cogito_macos_drag_window(void) {
  id nsw =
#if defined(__OBJC__)
      (__bridge id)GetWindowHandle();
#else
      (id)GetWindowHandle();
#endif
  if (!nsw) return;
  Class nsapp = (Class)objc_getClass("NSApplication");
  if (!nsapp) return;
  SEL sel_shared = sel_registerName("sharedApplication");
  id app = ((id (*)(id, SEL))objc_msgSend)((id)nsapp, sel_shared);
  if (!app) return;
  SEL sel_event = sel_registerName("currentEvent");
  id event = ((id (*)(id, SEL))objc_msgSend)(app, sel_event);
  if (!event) return;
  SEL sel_drag = sel_registerName("performWindowDragWithEvent:");
  ((void (*)(id, SEL, id))objc_msgSend)(nsw, sel_drag, event);
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

static void cogito_draw_rect(int x, int y, int w, int h, Color bg, int radius);
static bool cogito_hit_rect(int x, int y, int rx, int ry, int rw, int rh);
static CogitoNode* cogito_node_window(CogitoNode* n);

static void cogito_draw_shadow(int x, int y, int w, int h, int radius, int level) {
  if (level <= 0 || w <= 0 || h <= 0) return;
  if (level > 5) level = 5;
  int spread = 2 * level;
  int offset = level;
  int base_alpha = 20 + level * 12;
  if (base_alpha > 120) base_alpha = 120;
  int steps[3] = {spread, (spread * 2) / 3, spread / 3};
  int alphas[3] = {base_alpha, (base_alpha * 2) / 3, base_alpha / 3};
  for (int i = 0; i < 3; i++) {
    int s = steps[i];
    if (s <= 0) continue;
    int r = radius + s;
    Color c = cogito_rgba(0, 0, 0, alphas[i]);
    cogito_draw_rect(x - s, y - s + offset, w + s * 2, h + s * 2, c, r);
  }
}

typedef struct {
  char name[96];
  int size;
  int min_size;
  int max_size;
  int threshold;
  int type; /* 0 fixed, 1 scalable, 2 threshold */
} CogitoIconDir;

typedef struct {
  char base[PATH_MAX];
  char theme[64];
  CogitoIconDir dirs[128];
  int dir_count;
  char inherits[8][96];
  int inherit_count;
} CogitoIconTheme;

typedef struct {
  char name[128];
  char theme[64];
  int size;
  Texture2D tex;
  int w;
  int h;
  bool used;
} CogitoIconCacheEntry;

static CogitoIconCacheEntry cogito_icon_cache[128];
static int cogito_icon_cache_len = 0;
static char cogito_icon_theme_name[64] = {0};
static bool cogito_icon_theme_init = false;

static bool cogito_file_exists(const char* path) {
  struct stat st;
  return path && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static const char* cogito_default_font_path(void) {
#if defined(__APPLE__)
  const char* candidates[] = {
    "/System/Library/Fonts/SFNS.ttf",
    "/System/Library/Fonts/SFNSText.ttf",
    "/System/Library/Fonts/SFNSDisplay.ttf",
    "/System/Library/Fonts/Helvetica.ttc"
  };
#elif defined(_WIN32)
  const char* candidates[] = {
    "C:\\\\Windows\\\\Fonts\\\\segoeui.ttf"
  };
#else
  const char* candidates[] = {
    "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf"
  };
#endif
  for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
    if (cogito_file_exists(candidates[i])) return candidates[i];
  }
  return NULL;
}

static void cogito_trim(char* s) {
  if (!s) return;
  size_t len = strlen(s);
  size_t start = 0;
  while (start < len && isspace((unsigned char)s[start])) start++;
  size_t end = len;
  while (end > start && isspace((unsigned char)s[end - 1])) end--;
  if (start > 0) memmove(s, s + start, end - start);
  s[end - start] = 0;
}

static bool cogito_read_kv(const char* line, char* key, size_t key_cap, char* val, size_t val_cap) {
  const char* eq = strchr(line, '=');
  if (!eq) return false;
  size_t klen = (size_t)(eq - line);
  size_t vlen = strlen(eq + 1);
  if (klen >= key_cap) klen = key_cap - 1;
  if (vlen >= val_cap) vlen = val_cap - 1;
  strncpy(key, line, klen);
  key[klen] = 0;
  strncpy(val, eq + 1, vlen);
  val[vlen] = 0;
  cogito_trim(key);
  cogito_trim(val);
  return key[0] != 0;
}

static int cogito_icon_dir_index(CogitoIconTheme* theme, const char* name) {
  for (int i = 0; i < theme->dir_count; i++) {
    if (strcmp(theme->dirs[i].name, name) == 0) return i;
  }
  return -1;
}

static int cogito_parse_list(const char* src, char out[][96], int max) {
  int count = 0;
  const char* p = src;
  while (*p && count < max) {
    while (*p && (*p == ',' || isspace((unsigned char)*p))) p++;
    if (!*p) break;
    char buf[96];
    size_t i = 0;
    while (*p && *p != ',' && i + 1 < sizeof(buf)) {
      buf[i++] = *p++;
    }
    buf[i] = 0;
    cogito_trim(buf);
    if (buf[0]) {
      strncpy(out[count], buf, 95);
      out[count][95] = 0;
      count++;
    }
  }
  return count;
}

static int cogito_icon_base_dirs(char out[][PATH_MAX], int max) {
  int count = 0;
  const char* home = getenv("HOME");
  if (home && count < max) {
    snprintf(out[count++], PATH_MAX, "%s/.icons", home);
  }
  if (home && count < max) {
    snprintf(out[count++], PATH_MAX, "%s/.local/share/icons", home);
  }
  if (count < max) {
    snprintf(out[count++], PATH_MAX, "/opt/homebrew/share/icons");
  }
  const char* xdg = getenv("XDG_DATA_DIRS");
  if (!xdg || !*xdg) xdg = "/usr/local/share:/usr/share";
  const char* p = xdg;
  while (*p && count < max) {
    char buf[PATH_MAX];
    size_t i = 0;
    while (*p && *p != ':' && i + 1 < sizeof(buf)) buf[i++] = *p++;
    buf[i] = 0;
    if (*p == ':') p++;
    cogito_trim(buf);
    if (buf[0]) {
      snprintf(out[count++], PATH_MAX, "%s/icons", buf);
    }
  }
  return count;
}

static bool cogito_load_theme_file(const char* path, CogitoIconTheme* theme) {
  FILE* f = fopen(path, "rb");
  if (!f) return false;
  char line[512];
  char section[128] = {0};
  while (fgets(line, sizeof(line), f)) {
    char* s = line;
    while (*s && (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')) s++;
    if (*s == 0 || *s == '#' || *s == ';') continue;
    if (*s == '[') {
      char* end = strchr(s, ']');
      if (!end) continue;
      size_t len = (size_t)(end - (s + 1));
      if (len >= sizeof(section)) len = sizeof(section) - 1;
      strncpy(section, s + 1, len);
      section[len] = 0;
      cogito_trim(section);
      continue;
    }
    char key[96], val[384];
    if (!cogito_read_kv(s, key, sizeof(key), val, sizeof(val))) continue;
    if (strcmp(section, "Icon Theme") == 0) {
      if (strcmp(key, "Inherits") == 0) {
        theme->inherit_count = cogito_parse_list(val, theme->inherits, (int)(sizeof(theme->inherits) / sizeof(theme->inherits[0])));
      } else if (strcmp(key, "Directories") == 0) {
        char dirs[128][96];
        int n = cogito_parse_list(val, dirs, 128);
        for (int i = 0; i < n && theme->dir_count < 128; i++) {
          strncpy(theme->dirs[theme->dir_count].name, dirs[i], sizeof(theme->dirs[theme->dir_count].name) - 1);
          theme->dirs[theme->dir_count].name[sizeof(theme->dirs[theme->dir_count].name) - 1] = 0;
          theme->dirs[theme->dir_count].size = 0;
          theme->dirs[theme->dir_count].min_size = 0;
          theme->dirs[theme->dir_count].max_size = 0;
          theme->dirs[theme->dir_count].threshold = 2;
          theme->dirs[theme->dir_count].type = 0;
          theme->dir_count++;
        }
      }
    } else if (section[0]) {
      int idx = cogito_icon_dir_index(theme, section);
      if (idx >= 0) {
        CogitoIconDir* d = &theme->dirs[idx];
        if (strcmp(key, "Size") == 0) d->size = atoi(val);
        else if (strcmp(key, "MinSize") == 0) d->min_size = atoi(val);
        else if (strcmp(key, "MaxSize") == 0) d->max_size = atoi(val);
        else if (strcmp(key, "Threshold") == 0) d->threshold = atoi(val);
        else if (strcmp(key, "Type") == 0) {
          if (strcmp(val, "Scalable") == 0) d->type = 1;
          else if (strcmp(val, "Threshold") == 0) d->type = 2;
          else d->type = 0;
        }
      }
    }
  }
  fclose(f);
  return true;
}

static bool cogito_icon_theme_load(const char* theme_name, CogitoIconTheme* out) {
  if (!theme_name || !out) return false;
  memset(out, 0, sizeof(*out));
  strncpy(out->theme, theme_name, sizeof(out->theme) - 1);
  out->theme[sizeof(out->theme) - 1] = 0;
  char bases[16][PATH_MAX];
  int base_count = cogito_icon_base_dirs(bases, 16);
  for (int i = 0; i < base_count; i++) {
    char index_path[PATH_MAX];
    snprintf(index_path, sizeof(index_path), "%s/%s/index.theme", bases[i], theme_name);
    if (cogito_file_exists(index_path)) {
      snprintf(out->base, sizeof(out->base), "%s/%s", bases[i], theme_name);
      if (cogito_load_theme_file(index_path, out)) return true;
    }
  }
  return false;
}

static int cogito_icon_match_score(const CogitoIconDir* d, int size) {
  if (!d) return 100000;
  if (d->type == 1) {
    int minv = d->min_size ? d->min_size : d->size;
    int maxv = d->max_size ? d->max_size : d->size;
    if (size >= minv && size <= maxv) return 0;
    if (size < minv) return minv - size;
    return size - maxv;
  }
  if (d->type == 2) {
    int thr = d->threshold > 0 ? d->threshold : 2;
    int diff = abs(size - d->size);
    return diff <= thr ? 0 : diff - thr;
  }
  return abs(size - d->size);
}

static bool cogito_icon_find_in_theme(const char* theme_name, const char* icon, int size, char* out_path, size_t cap, bool* out_svg) {
  CogitoIconTheme theme;
  if (!cogito_icon_theme_load(theme_name, &theme)) return false;
  int best_score = 100000;
  bool best_svg = false;
  char best_path[PATH_MAX] = {0};
  for (int i = 0; i < theme.dir_count; i++) {
    CogitoIconDir* d = &theme.dirs[i];
    int score = cogito_icon_match_score(d, size);
    char svg_path[PATH_MAX];
    char png_path[PATH_MAX];
    snprintf(svg_path, sizeof(svg_path), "%s/%s/%s.svg", theme.base, d->name, icon);
    snprintf(png_path, sizeof(png_path), "%s/%s/%s.png", theme.base, d->name, icon);
    bool has_svg = cogito_file_exists(svg_path);
    bool has_png = cogito_file_exists(png_path);
    if (!has_svg && !has_png) continue;
    bool choose_svg = has_svg;
    if (score < best_score || (score == best_score && choose_svg && !best_svg)) {
      best_score = score;
      best_svg = choose_svg;
      strncpy(best_path, choose_svg ? svg_path : png_path, sizeof(best_path) - 1);
      best_path[sizeof(best_path) - 1] = 0;
    }
  }
  if (best_path[0]) {
    if (out_path && cap) {
      strncpy(out_path, best_path, cap - 1);
      out_path[cap - 1] = 0;
    }
    if (out_svg) *out_svg = best_svg;
    return true;
  }
  return false;
}

static bool cogito_icon_find_file(const char* theme_name, const char* icon, int size, char* out_path, size_t cap, bool* out_svg) {
  if (cogito_icon_find_in_theme(theme_name, icon, size, out_path, cap, out_svg)) return true;
  CogitoIconTheme theme;
  if (!cogito_icon_theme_load(theme_name, &theme)) return false;
  for (int i = 0; i < theme.inherit_count; i++) {
    if (cogito_icon_find_in_theme(theme.inherits[i], icon, size, out_path, cap, out_svg)) return true;
  }
  if (strcmp(theme_name, "hicolor") != 0) {
    if (cogito_icon_find_in_theme("hicolor", icon, size, out_path, cap, out_svg)) return true;
  }
  return false;
}

static const char* cogito_icon_theme_get(void) {
  if (!cogito_icon_theme_init) {
    const char* env = getenv("COGITO_ICON_THEME");
    if (env && *env) {
      strncpy(cogito_icon_theme_name, env, sizeof(cogito_icon_theme_name) - 1);
    } else {
      strncpy(cogito_icon_theme_name, "hicolor", sizeof(cogito_icon_theme_name) - 1);
    }
    cogito_icon_theme_name[sizeof(cogito_icon_theme_name) - 1] = 0;
    cogito_icon_theme_init = true;
  }
  return cogito_icon_theme_name;
}

static bool cogito_icon_is_sf(const char* name) {
  return name && strncmp(name, "sf:", 3) == 0;
}

#if defined(__APPLE__)
static bool cogito_sf_symbol_load(const char* symbol, int size, Texture2D* out, int* out_w, int* out_h) {
  if (!symbol || !symbol[0] || size <= 0) return false;
  id nsimage = (id)objc_getClass("NSImage");
  if (!nsimage) return false;
  SEL sel_image = sel_registerName("imageWithSystemSymbolName:accessibilityDescription:");
  id name = cogito_nsstring(symbol);
  id img = ((id (*)(id, SEL, id, id))objc_msgSend)(nsimage, sel_image, name, (id)0);
  if (!img) return false;
  // Avoid configuration calls that can abort for invalid weights/scales.
  SEL sel_cg = sel_registerName("CGImageForProposedRect:context:hints:");
  CGRect rect = CGRectMake(0, 0, (CGFloat)size, (CGFloat)size);
  CGImageRef cg = (CGImageRef)((void* (*)(id, SEL, CGRect*, id, id))objc_msgSend)(img, sel_cg, &rect, (id)0, (id)0);
  if (!cg) return false;
  size_t w = (size_t)CGRectGetWidth(rect);
  size_t h = (size_t)CGRectGetHeight(rect);
  if (w == 0 || h == 0) { w = (size_t)size; h = (size_t)size; }
  CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
  if (!cs) return false;
  size_t bpr = w * 4;
  CGContextRef ctx = CGBitmapContextCreate(NULL, w, h, 8, bpr, cs, kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
  if (!ctx) { CGColorSpaceRelease(cs); return false; }
  CGContextClearRect(ctx, CGRectMake(0, 0, (CGFloat)w, (CGFloat)h));
  CGContextDrawImage(ctx, CGRectMake(0, 0, (CGFloat)w, (CGFloat)h), cg);
  unsigned char* data = (unsigned char*)CGBitmapContextGetData(ctx);
  if (!data) { CGContextRelease(ctx); CGColorSpaceRelease(cs); return false; }
  size_t len = w * h * 4;
  unsigned char* copy = (unsigned char*)malloc(len);
  if (!copy) { CGContextRelease(ctx); CGColorSpaceRelease(cs); return false; }
  memcpy(copy, data, len);
  Image imgdata = {0};
  imgdata.data = copy;
  imgdata.width = (int)w;
  imgdata.height = (int)h;
  imgdata.mipmaps = 1;
  imgdata.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
  Texture2D tex = LoadTextureFromImage(imgdata);
  UnloadImage(imgdata);
  CGContextRelease(ctx);
  CGColorSpaceRelease(cs);
  if (tex.id == 0) return false;
  SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
  if (out) *out = tex;
  if (out_w) *out_w = (int)w;
  if (out_h) *out_h = (int)h;
  return true;
}
#endif

static bool cogito_icon_load_texture(const char* name, int size, Texture2D* out, int* out_w, int* out_h) {
  if (!name || !name[0] || size <= 0) return false;
  const char* theme = cogito_icon_is_sf(name) ? "sf" : cogito_icon_theme_get();
  for (int i = 0; i < cogito_icon_cache_len; i++) {
    CogitoIconCacheEntry* e = &cogito_icon_cache[i];
    if (e->used && e->size == size && strcmp(e->name, name) == 0 && strcmp(e->theme, theme) == 0) {
      if (out) *out = e->tex;
      if (out_w) *out_w = e->w;
      if (out_h) *out_h = e->h;
      return true;
    }
  }
  if (cogito_icon_is_sf(name)) {
#if defined(__APPLE__)
    Texture2D tex = {0};
    int tw = 0;
    int th = 0;
    if (cogito_sf_symbol_load(name + 3, size, &tex, &tw, &th)) {
      SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
      if (cogito_icon_cache_len < (int)(sizeof(cogito_icon_cache) / sizeof(cogito_icon_cache[0]))) {
        CogitoIconCacheEntry* e = &cogito_icon_cache[cogito_icon_cache_len++];
        memset(e, 0, sizeof(*e));
        strncpy(e->name, name, sizeof(e->name) - 1);
        strncpy(e->theme, theme, sizeof(e->theme) - 1);
        e->size = size;
        e->tex = tex;
        e->w = tw;
        e->h = th;
        e->used = true;
      }
      if (out) *out = tex;
      if (out_w) *out_w = tw;
      if (out_h) *out_h = th;
      return true;
    }
#endif
    return false;
  }
  char path[PATH_MAX];
  bool is_svg = false;
  if (!cogito_icon_find_file(theme, name, size, path, sizeof(path), &is_svg)) return false;
  Texture2D tex = {0};
  int tw = 0;
  int th = 0;
  (void)is_svg;
  Image img = LoadImage(path);
  if (img.data) {
    int maxdim = img.width > img.height ? img.width : img.height;
    if (size > 0 && maxdim > 0 && maxdim != size) {
      float scale = (float)size / (float)maxdim;
      int nw = (int)lroundf((float)img.width * scale);
      int nh = (int)lroundf((float)img.height * scale);
      if (nw < 1) nw = 1;
      if (nh < 1) nh = 1;
      ImageResize(&img, nw, nh);
    }
    tw = img.width;
    th = img.height;
    tex = LoadTextureFromImage(img);
    UnloadImage(img);
  }
  if (tex.id == 0) return false;
  if (cogito_icon_cache_len < (int)(sizeof(cogito_icon_cache) / sizeof(cogito_icon_cache[0]))) {
    CogitoIconCacheEntry* e = &cogito_icon_cache[cogito_icon_cache_len++];
    memset(e, 0, sizeof(*e));
    strncpy(e->name, name, sizeof(e->name) - 1);
    strncpy(e->theme, theme, sizeof(e->theme) - 1);
    e->size = size;
    e->tex = tex;
    e->w = tw;
    e->h = th;
    e->used = true;
  }
  if (out) *out = tex;
  if (out_w) *out_w = tw;
  if (out_h) *out_h = th;
  return true;
}

static void cogito_load_css_file(const char* path);

static void cogito_theme_set_defaults(void) {
  cogito_style_clear(&cogito_theme.base);
  for (int i = 0; i < COGITO_KIND_COUNT; i++) {
    cogito_style_clear(&cogito_theme.per_kind[i]);
    cogito_style_clear(&cogito_theme.per_kind_hover[i]);
    cogito_style_clear(&cogito_theme.per_kind_active[i]);
    cogito_style_clear(&cogito_theme.per_kind_checked[i]);
  }
  for (int i = 0; i < 4; i++) {
    cogito_style_clear(&cogito_theme.label_class[i]);
    cogito_style_clear(&cogito_theme.label_class_hover[i]);
    cogito_style_clear(&cogito_theme.label_class_active[i]);
    cogito_style_clear(&cogito_theme.label_class_checked[i]);
  }
  cogito_style_clear(&cogito_theme.menu);
  cogito_style_clear(&cogito_theme.appbar_child_button);
  cogito_style_clear(&cogito_theme.appbar_child_button_hover);
  cogito_style_clear(&cogito_theme.appbar_child_button_active);
  cogito_style_clear(&cogito_theme.appbar_child_iconbtn);
  cogito_style_clear(&cogito_theme.appbar_child_iconbtn_hover);
  cogito_style_clear(&cogito_theme.appbar_child_iconbtn_active);
  cogito_style_clear(&cogito_theme.appbar_win_btn_all);
  cogito_style_clear(&cogito_theme.appbar_win_btn_hover_all);
  cogito_style_clear(&cogito_theme.appbar_win_btn_active_all);
  for (int i = 0; i < 3; i++) {
    cogito_style_clear(&cogito_theme.appbar_win_btn[i]);
    cogito_style_clear(&cogito_theme.appbar_win_btn_hover[i]);
    cogito_style_clear(&cogito_theme.appbar_win_btn_active[i]);
  }
  cogito_style_clear(&cogito_theme.checkbox_check);
  cogito_style_clear(&cogito_theme.checkbox_check_hover);
  cogito_style_clear(&cogito_theme.checkbox_check_active);
  cogito_style_clear(&cogito_theme.checkbox_check_checked);
  cogito_style_clear(&cogito_theme.radio_check);
  cogito_style_clear(&cogito_theme.radio_check_hover);
  cogito_style_clear(&cogito_theme.radio_check_active);
  cogito_style_clear(&cogito_theme.radio_check_checked);
  cogito_style_clear(&cogito_theme.checkbox_box);
  cogito_style_clear(&cogito_theme.checkbox_box_hover);
  cogito_style_clear(&cogito_theme.checkbox_box_active);
  cogito_style_clear(&cogito_theme.checkbox_box_checked);
  cogito_style_clear(&cogito_theme.radio_box);
  cogito_style_clear(&cogito_theme.radio_box_hover);
  cogito_style_clear(&cogito_theme.radio_box_active);
  cogito_style_clear(&cogito_theme.radio_box_checked);
  cogito_style_clear(&cogito_theme.switch_track);
  cogito_style_clear(&cogito_theme.switch_track_hover);
  cogito_style_clear(&cogito_theme.switch_track_active);
  cogito_style_clear(&cogito_theme.switch_track_checked);
  cogito_style_clear(&cogito_theme.switch_knob);
  cogito_style_clear(&cogito_theme.switch_knob_hover);
  cogito_style_clear(&cogito_theme.switch_knob_active);
  cogito_style_clear(&cogito_theme.switch_knob_checked);
  cogito_theme.base.has_text = true;
  cogito_theme.base.text = cogito_rgba(30, 30, 30, 255);

  CogitoStyle* win = &cogito_theme.per_kind[COGITO_WINDOW];
  win->has_bg = true;
  win->bg = cogito_rgba(245, 245, 245, 255);

  CogitoStyle* btn = &cogito_theme.per_kind[COGITO_BUTTON];
  btn->has_bg = true;
  btn->bg = cogito_rgba(230, 230, 230, 255);
  btn->has_radius = true;
  btn->radius = 6;

  CogitoStyle* icon = &cogito_theme.per_kind[COGITO_ICONBTN];
  icon->has_bg = true;
  icon->bg = cogito_rgba(230, 230, 230, 255);
  icon->has_radius = true;
  icon->radius = 16;

  CogitoStyle* list = &cogito_theme.per_kind[COGITO_LIST];
  list->has_bg = true;
  list->bg = cogito_rgba(255, 255, 255, 255);
  list->has_radius = true;
  list->radius = 4;
  list->has_selection = true;
  list->selection = cogito_rgba(208, 220, 245, 255);

  CogitoStyle* grid = &cogito_theme.per_kind[COGITO_GRID];
  *grid = *list;

  CogitoStyle* appbar = &cogito_theme.per_kind[COGITO_APPBAR];
  appbar->has_bg = true;
  appbar->bg = cogito_rgba(238, 238, 238, 255);
}

static void cogito_theme_init(void) {
  if (!cogito_theme_initialized) {
    cogito_theme_initialized = true;
    cogito_theme_set_defaults();
    if (!cogito_css_default_loaded) {
      cogito_css_default_loaded = true;
      cogito_load_css_file("src/ergo/stdlib/cogito_default.css");
    }
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
  if (over->has_track) { out.has_track = true; out.track = over->track; }
  if (over->has_track_on) { out.has_track_on = true; out.track_on = over->track_on; }
  if (over->has_knob) { out.has_knob = true; out.knob = over->knob; }
  if (over->has_check) { out.has_check = true; out.check = over->check; }
  if (over->has_font_weight) { out.has_font_weight = true; out.font_weight = over->font_weight; }
  if (over->has_min_w) { out.has_min_w = true; out.min_w = over->min_w; }
  if (over->has_min_h) { out.has_min_h = true; out.min_h = over->min_h; }
  if (over->has_max_w) { out.has_max_w = true; out.max_w = over->max_w; }
  if (over->has_max_h) { out.has_max_h = true; out.max_h = over->max_h; }
  if (over->has_appbar_btn_size) { out.has_appbar_btn_size = true; out.appbar_btn_size = over->appbar_btn_size; }
  if (over->has_appbar_btn_gap) { out.has_appbar_btn_gap = true; out.appbar_btn_gap = over->appbar_btn_gap; }
  if (over->has_appbar_btn_pad_top) { out.has_appbar_btn_pad_top = true; out.appbar_btn_pad_top = over->appbar_btn_pad_top; }
  if (over->has_appbar_btn_pad_right) { out.has_appbar_btn_pad_right = true; out.appbar_btn_pad_right = over->appbar_btn_pad_right; }
  if (over->has_appbar_btn_close_color) { out.has_appbar_btn_close_color = true; out.appbar_btn_close_color = over->appbar_btn_close_color; }
  if (over->has_appbar_btn_min_color) { out.has_appbar_btn_min_color = true; out.appbar_btn_min_color = over->appbar_btn_min_color; }
  if (over->has_appbar_btn_max_color) { out.has_appbar_btn_max_color = true; out.appbar_btn_max_color = over->appbar_btn_max_color; }
  if (over->has_appbar_btn_border_color) { out.has_appbar_btn_border_color = true; out.appbar_btn_border_color = over->appbar_btn_border_color; }
  if (over->has_appbar_btn_border_width) { out.has_appbar_btn_border_width = true; out.appbar_btn_border_width = over->appbar_btn_border_width; }

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

static CogitoStyle cogito_theme_resolve_node(const CogitoNode* n) {
  CogitoStyle s = cogito_theme_resolve(n->kind);
  if (n->kind == COGITO_LABEL && n->label_class > COGITO_LABEL_CLASS_NONE && n->label_class < 4) {
    s = cogito_style_merge(&s, &cogito_theme.label_class[n->label_class]);
  }
  if (n->parent && n->parent->kind == COGITO_APPBAR) {
    if (n->kind == COGITO_BUTTON) {
      s = cogito_style_merge(&s, &cogito_theme.appbar_child_button);
    } else if (n->kind == COGITO_ICONBTN) {
      s = cogito_style_merge(&s, &cogito_theme.appbar_child_iconbtn);
    }
  }
  return s;
}

static CogitoStyle cogito_checkbox_check_style(bool is_radio, bool checked, bool hover, bool active) {
  CogitoStyle base = is_radio ? cogito_theme.radio_check : cogito_theme.checkbox_check;
  if (checked) {
    CogitoStyle over = is_radio ? cogito_theme.radio_check_checked : cogito_theme.checkbox_check_checked;
    base = cogito_style_merge(&base, &over);
  }
  if (active) {
    CogitoStyle over = is_radio ? cogito_theme.radio_check_active : cogito_theme.checkbox_check_active;
    return cogito_style_merge(&base, &over);
  }
  if (hover) {
    CogitoStyle over = is_radio ? cogito_theme.radio_check_hover : cogito_theme.checkbox_check_hover;
    return cogito_style_merge(&base, &over);
  }
  return base;
}

static CogitoStyle cogito_checkbox_box_style(bool is_radio, bool checked, bool hover, bool active) {
  CogitoStyle base = is_radio ? cogito_theme.radio_box : cogito_theme.checkbox_box;
  if (checked) {
    CogitoStyle over = is_radio ? cogito_theme.radio_box_checked : cogito_theme.checkbox_box_checked;
    base = cogito_style_merge(&base, &over);
  }
  if (active) {
    CogitoStyle over = is_radio ? cogito_theme.radio_box_active : cogito_theme.checkbox_box_active;
    return cogito_style_merge(&base, &over);
  }
  if (hover) {
    CogitoStyle over = is_radio ? cogito_theme.radio_box_hover : cogito_theme.checkbox_box_hover;
    return cogito_style_merge(&base, &over);
  }
  return base;
}

static CogitoStyle cogito_switch_track_style(bool checked, bool hover, bool active) {
  CogitoStyle base = cogito_theme.switch_track;
  if (checked) {
    base = cogito_style_merge(&base, &cogito_theme.switch_track_checked);
  }
  if (active) {
    return cogito_style_merge(&base, &cogito_theme.switch_track_active);
  }
  if (hover) {
    return cogito_style_merge(&base, &cogito_theme.switch_track_hover);
  }
  return base;
}

static CogitoStyle cogito_switch_knob_style(bool checked, bool hover, bool active) {
  CogitoStyle base = cogito_theme.switch_knob;
  if (checked) {
    base = cogito_style_merge(&base, &cogito_theme.switch_knob_checked);
  }
  if (active) {
    return cogito_style_merge(&base, &cogito_theme.switch_knob_active);
  }
  if (hover) {
    return cogito_style_merge(&base, &cogito_theme.switch_knob_hover);
  }
  return base;
}

static void cogito_node_hover_state(const CogitoNode* n, bool* out_hover, bool* out_active) {
  if (!out_hover || !out_active) return;
  *out_hover = false;
  *out_active = false;
  if (!n) return;
  int mx = (int)GetMousePosition().x;
  int my = (int)GetMousePosition().y;
  bool hover = cogito_hit_rect(mx, my, n->x, n->y, n->w, n->h);
  bool active = hover && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
  *out_hover = hover;
  *out_active = active;
}

static CogitoStyle cogito_theme_resolve_node_state(const CogitoNode* n, bool hover, bool active) {
  CogitoStyle s = cogito_theme_resolve_node(n);
  bool checked = (n && (n->kind == COGITO_CHECKBOX || n->kind == COGITO_SWITCH)) ? n->checked : false;
  if (n->kind == COGITO_LABEL && n->label_class > COGITO_LABEL_CLASS_NONE && n->label_class < 4) {
    if (checked) s = cogito_style_merge(&s, &cogito_theme.label_class_checked[n->label_class]);
    if (active) s = cogito_style_merge(&s, &cogito_theme.label_class_active[n->label_class]);
    else if (hover) s = cogito_style_merge(&s, &cogito_theme.label_class_hover[n->label_class]);
  }
  if (checked) s = cogito_style_merge(&s, &cogito_theme.per_kind_checked[n->kind]);
  if (active) s = cogito_style_merge(&s, &cogito_theme.per_kind_active[n->kind]);
  else if (hover) s = cogito_style_merge(&s, &cogito_theme.per_kind_hover[n->kind]);
  return s;
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
  if (s.has_track) { n->track_color = s.track; n->track_set = true; }
  else { n->track_set = false; }
  if (s.has_track_on) { n->track_on_color = s.track_on; n->track_on_set = true; }
  else { n->track_on_set = false; }
  if (s.has_knob) { n->knob_color = s.knob; n->knob_set = true; }
  else { n->knob_set = false; }
  if (s.has_check) { n->check_color = s.check; n->check_set = true; }
  else { n->check_set = false; }
  if (s.has_font_weight && (n->kind == COGITO_LABEL || n->kind == COGITO_BUTTON)) {
    n->font_weight = s.font_weight;
    n->font_weight_set = true;
  } else if (n->kind == COGITO_LABEL || n->kind == COGITO_BUTTON) {
    n->font_weight_set = false;
    n->font_weight = 400;
  }
  if (s.has_min_w) { n->min_w = s.min_w; n->min_w_set = true; }
  else { n->min_w_set = false; }
  if (s.has_min_h) { n->min_h = s.min_h; n->min_h_set = true; }
  else { n->min_h_set = false; }
  if (s.has_max_w) { n->max_w = s.max_w; n->max_w_set = true; }
  else { n->max_w_set = false; }
  if (s.has_max_h) { n->max_h = s.max_h; n->max_h_set = true; }
  else { n->max_h_set = false; }
  if (s.has_shadow) { n->shadow_level = s.shadow_level; n->shadow_set = true; }
  else { n->shadow_set = false; }
  if (s.has_font_size) {
    if (n->kind == COGITO_WINDOW) {
      int sz = s.font_size;
      if (sz > 0 && sz != cogito_font_size_value) {
        cogito_font_size_value = sz;
        if (cogito_raylib_ready && cogito_font_ready && cogito_font_from_env) {
          const char* font_path = cogito_font_path_active;
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
    } else if (n->kind == COGITO_LABEL || n->kind == COGITO_BUTTON) {
      n->font_size = s.font_size;
      n->font_size_set = true;
    }
  } else if (n->kind == COGITO_LABEL || n->kind == COGITO_BUTTON) {
    n->font_size_set = false;
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
    if (n->kind == COGITO_VSTACK && n->parent && n->parent->kind == COGITO_WINDOW) {
      bool has_pad = s.has_padding || s.has_padding_left || s.has_padding_top || s.has_padding_right || s.has_padding_bottom;
      if (!has_pad) {
        n->padding_left = 18;
        n->padding_right = 18;
        n->padding_top = 0;
        n->padding_bottom = 18;
      }
    }
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

static int cogito_label_font_size(const CogitoNode* n) {
  if (!n) return cogito_font_size();
  if (n->font_size_set && n->font_size > 0) return n->font_size;
  switch (n->label_class) {
    case COGITO_LABEL_CLASS_TITLE: return 32;
    case COGITO_LABEL_CLASS_SUBTITLE: return 26;
    case COGITO_LABEL_CLASS_BODY: return 16;
    default: return cogito_font_size();
  }
}

static bool cogito_label_bold(const CogitoNode* n);

static int cogito_node_font_size(const CogitoNode* n) {
  if (!n) return cogito_font_size();
  if (n->kind == COGITO_LABEL) return cogito_label_font_size(n);
  if (n->font_size_set && n->font_size > 0) return n->font_size;
  return cogito_font_size();
}

static bool cogito_node_bold(const CogitoNode* n) {
  if (!n) return false;
  if (n->kind == COGITO_LABEL) return cogito_label_bold(n);
  if (n->font_weight_set) return n->font_weight >= 600;
  return false;
}

static void cogito_apply_size_constraints(const CogitoNode* n, int* w, int* h) {
  if (!n || !w || !h) return;
  if (n->min_w_set && *w < n->min_w) *w = n->min_w;
  if (n->min_h_set && *h < n->min_h) *h = n->min_h;
  if (n->max_w_set && n->max_w > 0 && *w > n->max_w) *w = n->max_w;
  if (n->max_h_set && n->max_h > 0 && *h > n->max_h) *h = n->max_h;
}

static void cogito_appbar_button_metrics(int* size, int* gap, int* pad_top, int* pad_right) {
  CogitoStyle s = cogito_theme.appbar_win_btn_all;
  if (size) {
    if (s.has_min_w) *size = s.min_w;
    else if (s.has_min_h) *size = s.min_h;
    else *size = 12;
  }
  if (gap) *gap = 6;
  if (pad_top) *pad_top = 9;
  if (pad_right) *pad_right = 9;
}

static int cogito_appbar_ctrl_index(char c) {
  if (c == 'C') return 0;
  if (c == 'I') return 1;
  if (c == 'M') return 2;
  return -1;
}

static void cogito_appbar_controls_positions(CogitoNode* n, int x, int w, int btn_size, int gap, int pad, bool allow_max, int* out_close, int* out_min, int* out_max) {
  int pos[3] = {-1, -1, -1};
  bool seen[3] = {false, false, false};
  const char* s = n->appbar_controls[0] ? n->appbar_controls : "CIM|";
  const char* split = strchr(s, '|');
  size_t left_len = split ? (size_t)(split - s) : strlen(s);
  const char* right = split ? split + 1 : "";
  size_t right_len = strlen(right);

  int lx = x + pad;
  for (size_t i = 0; i < left_len; i++) {
    int idx = cogito_appbar_ctrl_index(s[i]);
    if (!allow_max && idx == 2) continue;
    if (idx < 0 || seen[idx]) continue;
    pos[idx] = lx;
    seen[idx] = true;
    lx += btn_size + gap;
  }

  int rx = x + w - pad - btn_size;
  for (size_t i = 0; i < right_len; i++) {
    char c = right[right_len - 1 - i];
    int idx = cogito_appbar_ctrl_index(c);
    if (!allow_max && idx == 2) continue;
    if (idx < 0 || seen[idx]) continue;
    pos[idx] = rx;
    seen[idx] = true;
    rx -= btn_size + gap;
  }

  if (out_close) *out_close = pos[0];
  if (out_min) *out_min = pos[1];
  if (out_max) *out_max = pos[2];
}

static int cogito_label_class_font_size(int cls, int fallback);
static int cogito_text_height_size(int size);
static void cogito_intrinsic_size(CogitoNode* n, int* out_w, int* out_h);

static void cogito_appbar_row_positions(CogitoNode* n, int* out_buttons_y, int* out_title_y, int* out_subtitle_y, int* out_child_y, int* out_appbar_h) {
  int btn_size = 12;
  int btn_gap = 6;
  int btn_pad_top = 9;
  int btn_pad_right = 9;
  cogito_appbar_button_metrics(&btn_size, &btn_gap, &btn_pad_top, &btn_pad_right);
  int title_h = 0;
  int subtitle_h = 0;
  if (n->text && n->text->len) {
    int tsz = cogito_label_class_font_size(COGITO_LABEL_CLASS_TITLE, 24);
    title_h = cogito_text_height_size(tsz);
  }
  if (n->subtitle && n->subtitle->len) {
    int ssz = cogito_label_class_font_size(COGITO_LABEL_CLASS_SUBTITLE, 18);
    subtitle_h = cogito_text_height_size(ssz);
  }
  int icon_row_h = 0;
  for (size_t i = 0; i < n->len; i++) {
    int cw = 0;
    int ch = 0;
    cogito_intrinsic_size(n->children[i], &cw, &ch);
    if (ch > icon_row_h) icon_row_h = ch;
  }
  if (icon_row_h == 0) icon_row_h = btn_size;
  int y0 = n->y + btn_pad_top;
  int y1 = y0 + btn_size + 18;
  int y2 = y1 + title_h + 6;
  int y3 = y2 + subtitle_h + 6;
  int h = (y3 - n->y) + icon_row_h + 12;
  if (out_buttons_y) *out_buttons_y = y0;
  if (out_title_y) *out_title_y = y1;
  if (out_subtitle_y) *out_subtitle_y = y2;
  if (out_child_y) *out_child_y = y3;
  if (out_appbar_h) *out_appbar_h = h;
}

static CogitoStyle cogito_appbar_win_btn_style(int kind, int state) {
  CogitoStyle s = cogito_style_merge(&cogito_theme.appbar_win_btn_all, &cogito_theme.appbar_win_btn[kind]);
  if (state == 1) {
    s = cogito_style_merge(&s, &cogito_theme.appbar_win_btn_hover_all);
    s = cogito_style_merge(&s, &cogito_theme.appbar_win_btn_hover[kind]);
  } else if (state == 2) {
    s = cogito_style_merge(&s, &cogito_theme.appbar_win_btn_active_all);
    s = cogito_style_merge(&s, &cogito_theme.appbar_win_btn_active[kind]);
  }
  return s;
}

static bool cogito_label_bold(const CogitoNode* n) {
  if (!n) return false;
  if (n->font_weight_set) return n->font_weight >= 600;
  return n->label_class == COGITO_LABEL_CLASS_TITLE;
}

static int cogito_label_class_font_size(int cls, int fallback) {
  cogito_theme_init();
  if (cls > COGITO_LABEL_CLASS_NONE && cls < 4) {
    CogitoStyle* s = &cogito_theme.label_class[cls];
    if (s->has_font_size && s->font_size > 0) return s->font_size;
  }
  return fallback;
}

static bool cogito_label_class_bold(int cls, bool fallback) {
  cogito_theme_init();
  if (cls > COGITO_LABEL_CLASS_NONE && cls < 4) {
    CogitoStyle* s = &cogito_theme.label_class[cls];
    if (s->has_font_weight) return s->font_weight >= 600;
  }
  return fallback;
}

static Color cogito_label_class_color(int cls, Color fallback) {
  cogito_theme_init();
  if (cls > COGITO_LABEL_CLASS_NONE && cls < 4) {
    CogitoStyle* s = &cogito_theme.label_class[cls];
    if (s->has_text) return s->text;
  }
  return fallback;
}

static int cogito_text_width(const char* s) {
  if (!s) return 0;
  if (!cogito_raylib_ready || !cogito_font_ready) return (int)strlen(s) * 8;
  Vector2 size = MeasureTextEx(cogito_font, s, (float)cogito_font_size(), 1.0f);
  return (int)lround(size.x);
}

static int cogito_text_width_size(const char* s, int size) {
  if (!s) return 0;
  if (size <= 0) size = cogito_font_size();
  if (!cogito_raylib_ready || !cogito_font_ready) return (int)strlen(s) * (size / 2);
  Vector2 sz = MeasureTextEx(cogito_font, s, (float)size, 1.0f);
  return (int)lround(sz.x);
}

static int cogito_text_height(void) {
  if (!cogito_raylib_ready || !cogito_font_ready) return 18;
  return cogito_font.baseSize + 2;
}

static int cogito_text_height_size(int size) {
  if (size <= 0) size = cogito_font_size();
  if (!cogito_raylib_ready || !cogito_font_ready) return size + 2;
  return size + 2;
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

static bool cogito_group_match(ErgoVal a, ErgoVal b) {
  if (a.tag != b.tag) return false;
  if (a.tag == EVT_STR) {
    ErgoStr* sa = (ErgoStr*)a.as.p;
    ErgoStr* sb = (ErgoStr*)b.as.p;
    if (!sa || !sb) return false;
    if (sa->len != sb->len) return false;
    return memcmp(sa->data, sb->data, sa->len) == 0;
  }
  return cogito_val_equal(a, b);
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
  if (n->icon) {
    ergo_release_val(EV_STR(n->icon));
    n->icon = NULL;
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
  if (n->dialog) {
    ergo_release_val(EV_OBJ(n->dialog));
    n->dialog = NULL;
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
  n->icon = NULL;
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
  n->appbar_title_y = 0;
  n->appbar_subtitle_y = 0;
  n->appbar_controls[0] = 0;
  n->dialog = NULL;
  n->dialog_open = false;
  n->resizable = true;
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
  n->track_set = false;
  n->track_on_set = false;
  n->knob_set = false;
  n->check_set = false;
  n->font_weight_set = false;
  n->min_w_set = false;
  n->min_h_set = false;
  n->max_w_set = false;
  n->max_h_set = false;
  n->shadow_set = false;
  n->bg = cogito_rgba(0, 0, 0, 0);
  n->text_color = cogito_rgba(30, 30, 30, 255);
  n->border_color = cogito_rgba(0, 0, 0, 0);
  n->selection_color = cogito_rgba(208, 220, 245, 255);
  n->track_color = cogito_rgba(170, 170, 170, 255);
  n->track_on_color = cogito_rgba(90, 160, 110, 255);
  n->knob_color = cogito_rgba(250, 250, 250, 255);
  n->check_color = cogito_rgba(30, 30, 30, 255);
  n->border_width = 0;
  n->border_radius = 0;
  n->label_class = COGITO_LABEL_CLASS_NONE;
  n->font_size = 0;
  n->font_size_set = false;
  n->font_weight = 400;
  n->min_w = 0;
  n->min_h = 0;
  n->max_w = 0;
  n->max_h = 0;
  n->shadow_level = 0;
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

static void cogito_node_set_icon(CogitoNode* n, ErgoStr* s) {
  if (!n) return;
  if (n->icon) ergo_release_val(EV_STR(n->icon));
  n->icon = s;
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
  int text_h = cogito_text_height();
  if (n->text) {
    int size = cogito_node_font_size(n);
    text_w = cogito_text_width_size(n->text->data, size);
    text_h = cogito_text_height_size(size);
  }
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
    case COGITO_IMAGE:
      w = 24;
      h = 24;
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
    case COGITO_ZSTACK: {
      if (n->len == 0) break;
      int max_w = 0;
      int max_h = 0;
      for (size_t i = 0; i < n->len; i++) {
        int iw = 0;
        int ih = 0;
        cogito_intrinsic_size(n->children[i], &iw, &ih);
        int tw = iw + n->children[i]->margin_left + n->children[i]->margin_right;
        int th = ih + n->children[i]->margin_top + n->children[i]->margin_bottom;
        if (tw > max_w) max_w = tw;
        if (th > max_h) max_h = th;
      }
      w = max_w;
      h = max_h;
      break;
    }
    case COGITO_LIST:
    case COGITO_GRID:
      w = 160;
      h = 120;
      break;
    case COGITO_DIALOG:
      w = 320;
      h = 200;
      if (n->len == 0) break;
      // Dialog behaves like VStack for intrinsic sizing
      {
        int cw = 0;
        int ch = 0;
        for (size_t i = 0; i < n->len; i++) {
          int iw = 0;
          int ih = 0;
          cogito_intrinsic_size(n->children[i], &iw, &ih);
          if (iw > cw) cw = iw;
          ch += ih;
          if (i + 1 < n->len) ch += pad;
        }
        w = cw;
        h = ch;
      }
      break;
    case COGITO_DIALOG_SLOT:
      w = n->w;
      h = n->h;
      break;
    case COGITO_APPBAR: {
      int appbar_h = 0;
      cogito_appbar_row_positions(n, NULL, NULL, NULL, NULL, &appbar_h);
      h = appbar_h > 0 ? appbar_h : 48;
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
    case COGITO_KIND_COUNT:
      break;
  }
  w += n->padding_left + n->padding_right;
  h += n->padding_top + n->padding_bottom;
  cogito_apply_size_constraints(n, &w, &h);
  if (out_w) *out_w = w;
  if (out_h) *out_h = h;
}

static void cogito_layout_node(CogitoNode* n, int x, int y, int w, int h);
static void cogito_window_relayout(CogitoNode* win);

static void cogito_layout_vstack(CogitoNode* n, int x, int y) {
  int pad = 6;
  int col = n->align % 3;
  int cy = y + n->padding_top;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    int cw = 0;
    int ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    int cx = x + n->padding_left;
    int ox = 0;
    if (col == 1) {
      ox = (n->w - cw) / 2;
    } else if (col == 2) {
      ox = n->w - cw - n->padding_right;
    }
    if (ox < 0) ox = 0;
    if (col == 1) {
      cogito_layout_node(c, cx + ox + c->margin_left, cy + c->margin_top, cw, ch);
    } else if (col == 2) {
      cogito_layout_node(c, cx + ox + c->margin_left, cy + c->margin_top, cw, ch);
    } else {
      cogito_layout_node(c, cx + c->margin_left, cy + c->margin_top, cw, ch);
    }
    cy += ch + c->margin_top + c->margin_bottom + pad;
  }
}

static void cogito_layout_zstack(CogitoNode* n, int x, int y) {
  int inner_x = x + n->padding_left;
  int inner_y = y + n->padding_top;
  int inner_w = n->w - n->padding_left - n->padding_right;
  int inner_h = n->h - n->padding_top - n->padding_bottom;
  if (inner_w < 0) inner_w = 0;
  if (inner_h < 0) inner_h = 0;
  int align = n->align;
  if (align < 0 || align > 8) align = 0;
  int row = align / 3;
  int col = align % 3;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    int cw = 0;
    int ch = 0;
    int ox = 0;
    int oy = 0;
    int avail_w = inner_w - c->margin_left - c->margin_right;
    int avail_h = inner_h - c->margin_top - c->margin_bottom;
    if (avail_w < 0) avail_w = 0;
    if (avail_h < 0) avail_h = 0;
    if (c->kind == COGITO_DIALOG_SLOT) {
      cw = avail_w;
      ch = avail_h;
    } else {
      cogito_intrinsic_size(c, &cw, &ch);
    }
    if (col == 1) ox = (avail_w - cw) / 2;
    else if (col == 2) ox = avail_w - cw;
    if (row == 1) oy = (avail_h - ch) / 2;
    else if (row == 2) oy = avail_h - ch;
    if (ox < 0) ox = 0;
    if (oy < 0) oy = 0;
    cogito_layout_node(c, inner_x + c->margin_left + ox, inner_y + c->margin_top + oy, cw, ch);
  }
}

static void cogito_layout_hstack(CogitoNode* n, int x, int y) {
  int pad = 6;
  int row = n->align / 3;
  int cx = x + n->padding_left;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    int cw = 0;
    int ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    int cy = y + n->padding_top;
    int oy = 0;
    if (row == 1) {
      oy = (n->h - ch) / 2;
    } else if (row == 2) {
      oy = n->h - ch - n->padding_bottom;
    }
    if (oy < 0) oy = 0;
    if (row == 1) {
      cogito_layout_node(c, cx + c->margin_left, cy + oy + c->margin_top, cw, ch);
    } else if (row == 2) {
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
  cogito_apply_size_constraints(n, &n->w, &n->h);
  switch (n->kind) {
    case COGITO_APPBAR: {
      int btn_size = 12;
      int btn_gap = 6;
      int btn_pad_top = 9;
      int btn_pad_right = 9;
      cogito_appbar_button_metrics(&btn_size, &btn_gap, &btn_pad_top, &btn_pad_right);
      int buttons_y = y + btn_pad_top;
      n->appbar_btn_size = btn_size;
      n->appbar_btn_y = buttons_y;
      CogitoNode* win = cogito_node_window(n);
      bool allow_max = !win || win->resizable;
      cogito_appbar_controls_positions(n, x, w, btn_size, btn_gap, btn_pad_right, allow_max,
                                       &n->appbar_btn_close_x, &n->appbar_btn_min_x, &n->appbar_btn_max_x);
      int title_y = 0;
      int subtitle_y = 0;
      int child_y = 0;
      int appbar_h = 0;
      cogito_appbar_row_positions(n, &buttons_y, &title_y, &subtitle_y, &child_y, &appbar_h);
      n->appbar_btn_y = buttons_y;
      n->appbar_title_y = title_y;
      n->appbar_subtitle_y = subtitle_y;
      int icon_row_h = appbar_h - (child_y - y) - 6;
      if (icon_row_h < btn_size) icon_row_h = btn_size;
      int cx = x + w - 12;
      for (size_t i = 0; i < n->len; i++) {
        CogitoNode* c = n->children[i];
        int cw = 0;
        int ch = 0;
        cogito_intrinsic_size(c, &cw, &ch);
        cx -= cw;
        int cy = child_y + (icon_row_h - ch) / 2;
        cogito_layout_node(c, cx, cy, cw, ch);
        cx -= btn_gap;
      }
      break;
    }
    case COGITO_DIALOG:
      cogito_layout_vstack(n, x, y);
      break;
    case COGITO_DIALOG_SLOT:
      if (n->dialog && n->dialog_open) {
        int dw = 0;
        int dh = 0;
        cogito_intrinsic_size(n->dialog, &dw, &dh);
        if (dw <= 0) dw = 320;
        if (dh <= 0) dh = 200;
        CogitoNode* win = cogito_node_window(n);
        int base_x = win ? 0 : x;
        int base_y = win ? 0 : y;
        int base_w = win ? win->w : w;
        int base_h = win ? win->h : h;
        int dx = base_x + (base_w - dw) / 2 + (int)(base_w * 0.10f);
        int dy = base_y + (base_h - dh) / 2;
        cogito_layout_node(n->dialog, dx, dy, dw, dh);
      }
      break;
    case COGITO_VSTACK:
      cogito_layout_vstack(n, x, y);
      break;
    case COGITO_HSTACK:
      cogito_layout_hstack(n, x, y);
      break;
    case COGITO_ZSTACK:
      cogito_layout_zstack(n, x, y);
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
    case COGITO_IMAGE:
    case COGITO_LABEL:
    case COGITO_CHECKBOX:
    case COGITO_SWITCH:
      break;
    case COGITO_KIND_COUNT:
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
    cy += appbar_h + 6;
  }
  int appbar_gap = appbar ? 6 : 0;
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
      if (appbar) avail_h -= appbar_h + appbar_gap;
      if (avail_w < 0) avail_w = 0;
      if (avail_h < 0) avail_h = 0;
      cogito_layout_node(c, win->padding_left, win->padding_top + appbar_h + appbar_gap, avail_w, avail_h);
      cy += avail_h + pad + appbar_gap;
    } else {
      cogito_layout_node(c, cx + c->margin_left, cy + c->margin_top, cw, ch);
      cy += ch_total + pad;
    }
  }
  if (win->dialog) {
    if (win->dialog->kind == COGITO_DIALOG_SLOT) {
      cogito_layout_node(win->dialog, 0, 0, win->w, win->h);
    } else if (win->dialog_open) {
      int dw = 0;
      int dh = 0;
      cogito_intrinsic_size(win->dialog, &dw, &dh);
      if (dw <= 0) dw = 320;
      if (dh <= 0) dh = 200;
      int dx = (win->w - dw) / 2;
      int dy = (win->h - dh) / 2;
      cogito_layout_node(win->dialog, dx, dy, dw, dh);
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
  if (appbar_h > 0) cy += appbar_h + 6;
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
  cogito_apply_size_constraints(win, &dw, &dh);
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
    if (group.tag != EVT_NULL && cogito_group_match(root->group, group)) {
      root->checked = false;
      root->bg_set = false;
      root->border_color_set = false;
      root->border_width_set = false;
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

static CogitoNode* cogito_node_window(CogitoNode* n) {
  CogitoNode* root = cogito_root(n);
  if (root && root->kind == COGITO_WINDOW) return root;
  return NULL;
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

static CogitoNode* cogito_find_dialog_slot(CogitoNode* n) {
  if (!n) return NULL;
  if (n->kind == COGITO_DIALOG_SLOT && n->dialog_open && n->dialog) return n;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* hit = cogito_find_dialog_slot(n->children[i]);
    if (hit) return hit;
  }
  return NULL;
}

static bool cogito_point_over_appbar_buttons(CogitoNode* app, int x, int y) {
  if (!app) return false;
  if (app->appbar_btn_close_x >= 0 &&
      cogito_hit_rect(x, y, app->appbar_btn_close_x, app->appbar_btn_y, app->appbar_btn_size, app->appbar_btn_size)) return true;
  if (app->appbar_btn_min_x >= 0 &&
      cogito_hit_rect(x, y, app->appbar_btn_min_x, app->appbar_btn_y, app->appbar_btn_size, app->appbar_btn_size)) return true;
  if (app->appbar_btn_max_x >= 0 &&
      cogito_hit_rect(x, y, app->appbar_btn_max_x, app->appbar_btn_y, app->appbar_btn_size, app->appbar_btn_size)) return true;
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

static CogitoNode* cogito_image_new_obj(ErgoStr* icon) {
  CogitoNode* img = cogito_node_new(COGITO_IMAGE);
  cogito_node_set_icon(img, icon);
  return img;
}

static CogitoNode* cogito_dialog_new_obj(ErgoStr* title) {
  CogitoNode* dlg = cogito_node_new(COGITO_DIALOG);
  cogito_node_set_text(dlg, title);
  return dlg;
}

static CogitoNode* cogito_dialog_slot_new_obj(void) {
  return cogito_node_new(COGITO_DIALOG_SLOT);
}

static CogitoNode* cogito_appbar_new_obj(ErgoStr* title, ErgoStr* subtitle) {
  CogitoNode* ab = cogito_node_new(COGITO_APPBAR);
  cogito_node_set_text(ab, title);
  cogito_node_set_subtitle(ab, subtitle);
  ab->appbar_btn_close_x = -1;
  ab->appbar_btn_min_x = -1;
  ab->appbar_btn_max_x = -1;
  strncpy(ab->appbar_controls, "CIM|", sizeof(ab->appbar_controls) - 1);
  ab->appbar_controls[sizeof(ab->appbar_controls) - 1] = 0;
  return ab;
}

static CogitoNode* cogito_vstack_new_obj(void) {
  return cogito_node_new(COGITO_VSTACK);
}

static CogitoNode* cogito_hstack_new_obj(void) {
  return cogito_node_new(COGITO_HSTACK);
}

static CogitoNode* cogito_zstack_new_obj(void) {
  return cogito_node_new(COGITO_ZSTACK);
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

static ErgoVal cogito_image_new(ErgoVal icon) {
  ErgoStr* is = stdr_to_string(icon);
  CogitoNode* img = cogito_image_new_obj(is);
  return EV_OBJ(img);
}

static ErgoVal cogito_dialog_new(ErgoVal title) {
  ErgoStr* ts = stdr_to_string(title);
  CogitoNode* dlg = cogito_dialog_new_obj(ts);
  return EV_OBJ(dlg);
}

static ErgoVal cogito_dialog_slot_new(void) {
  return EV_OBJ(cogito_dialog_slot_new_obj());
}

static void cogito_dialog_slot_show(ErgoVal slotv, ErgoVal dialogv);
static void cogito_dialog_slot_clear(ErgoVal slotv);

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

static ErgoVal cogito_zstack_new(void) {
  return EV_OBJ(cogito_zstack_new_obj());
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

static void cogito_container_set_halign(ErgoVal nodev, ErgoVal alignv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.container_set_halign expects object");
  CogitoNode* node = (CogitoNode*)nodev.as.p;
  int col = (int)ergo_as_int(alignv);
  if (col < 0) col = 0;
  if (col > 2) col = 2;
  int row = node->align / 3;
  node->align = row * 3 + col;
}

static void cogito_container_set_valign(ErgoVal nodev, ErgoVal alignv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.container_set_valign expects object");
  CogitoNode* node = (CogitoNode*)nodev.as.p;
  int row = (int)ergo_as_int(alignv);
  if (row < 0) row = 0;
  if (row > 2) row = 2;
  int col = node->align % 3;
  node->align = row * 3 + col;
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

static void cogito_label_set_class(ErgoVal labelv, ErgoVal classv) {
  if (labelv.tag != EVT_OBJ) ergo_trap("cogito.label_set_class expects label");
  CogitoNode* label = (CogitoNode*)labelv.as.p;
  ErgoStr* cs = stdr_to_string(classv);
  const char* s = cs ? cs->data : "";
  int cls = COGITO_LABEL_CLASS_NONE;
  if (s) {
    if (strcmp(s, "title") == 0) cls = COGITO_LABEL_CLASS_TITLE;
    else if (strcmp(s, "subtitle") == 0 || strcmp(s, "sub-title") == 0) cls = COGITO_LABEL_CLASS_SUBTITLE;
    else if (strcmp(s, "body") == 0) cls = COGITO_LABEL_CLASS_BODY;
  }
  label->label_class = cls;
  cogito_apply_style_to_node(label);
  extern CogitoNode* cogito_active_window;
  if (cogito_active_window) {
    cogito_window_relayout(cogito_active_window);
  }
}

static void cogito_window_set_autosize(ErgoVal winv, ErgoVal onv) {
  if (winv.tag != EVT_OBJ) ergo_trap("cogito.window_set_autosize expects window");
  CogitoNode* win = (CogitoNode*)winv.as.p;
  win->auto_size = ergo_as_bool(onv);
}

static void cogito_window_set_resizable(ErgoVal winv, ErgoVal onv) {
  if (winv.tag != EVT_OBJ) ergo_trap("cogito.window_set_resizable expects window");
  CogitoNode* win = (CogitoNode*)winv.as.p;
  win->resizable = ergo_as_bool(onv);
  if (cogito_raylib_ready) {
    if (win->resizable) SetWindowState(FLAG_WINDOW_RESIZABLE);
    else ClearWindowState(FLAG_WINDOW_RESIZABLE);
  }
  cogito_window_relayout(win);
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

static void cogito_image_set_icon(ErgoVal imgv, ErgoVal icon) {
  if (imgv.tag != EVT_OBJ) ergo_trap("cogito.image_set_icon expects image");
  CogitoNode* img = (CogitoNode*)imgv.as.p;
  ErgoStr* is = stdr_to_string(icon);
  cogito_node_set_icon(img, is);
}

static void cogito_window_set_dialog(ErgoVal winv, ErgoVal dialogv) {
  if (winv.tag != EVT_OBJ) ergo_trap("cogito.window_set_dialog expects window");
  if (dialogv.tag != EVT_OBJ) ergo_trap("cogito.window_set_dialog expects dialog");
  CogitoNode* win = (CogitoNode*)winv.as.p;
  CogitoNode* dlg = (CogitoNode*)dialogv.as.p;
  if (!win->dialog || win->dialog->kind != COGITO_DIALOG_SLOT) {
    if (win->dialog) {
      ergo_release_val(EV_OBJ(win->dialog));
      win->dialog = NULL;
    }
    win->dialog = cogito_dialog_slot_new_obj();
    win->dialog->parent = win;
    ergo_retain_val(EV_OBJ(win->dialog));
  }
  cogito_dialog_slot_show(EV_OBJ(win->dialog), dialogv);
  win->dialog_open = true;
}

static void cogito_window_clear_dialog(ErgoVal winv) {
  if (winv.tag != EVT_OBJ) ergo_trap("cogito.window_clear_dialog expects window");
  CogitoNode* win = (CogitoNode*)winv.as.p;
  if (win->dialog && win->dialog->kind == COGITO_DIALOG_SLOT) {
    cogito_dialog_slot_clear(EV_OBJ(win->dialog));
  }
  win->dialog_open = false;
}

static void cogito_dialog_slot_show(ErgoVal slotv, ErgoVal dialogv) {
  if (slotv.tag != EVT_OBJ) ergo_trap("cogito.dialog_slot_show expects dialog slot");
  if (dialogv.tag != EVT_OBJ) ergo_trap("cogito.dialog_slot_show expects dialog");
  CogitoNode* slot = (CogitoNode*)slotv.as.p;
  CogitoNode* dlg = (CogitoNode*)dialogv.as.p;
  if (slot->dialog) ergo_release_val(EV_OBJ(slot->dialog));
  slot->dialog = dlg;
  slot->dialog_open = true;
  dlg->parent = slot;
  ergo_retain_val(EV_OBJ(dlg));
  cogito_apply_style_tree(dlg);
  cogito_window_relayout(cogito_node_window(slot));
}

static void cogito_dialog_slot_clear(ErgoVal slotv) {
  if (slotv.tag != EVT_OBJ) ergo_trap("cogito.dialog_slot_clear expects dialog slot");
  CogitoNode* slot = (CogitoNode*)slotv.as.p;
  slot->dialog_open = false;
  cogito_window_relayout(cogito_node_window(slot));
}

static ErgoVal cogito_node_window_val(ErgoVal nodev) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.node_window expects object");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  CogitoNode* win = cogito_node_window(n);
  if (win) return EV_OBJ(win);
  return EV_NULLV;
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

static void cogito_appbar_set_controls(ErgoVal appv, ErgoVal layout) {
  if (appv.tag != EVT_OBJ) ergo_trap("cogito.appbar_set_controls expects appbar");
  if (layout.tag != EVT_STR) ergo_trap("cogito.appbar_set_controls expects string");
  CogitoNode* ab = (CogitoNode*)appv.as.p;
  ErgoStr* ls = (ErgoStr*)layout.as.p;
  const char* s = ls ? ls->data : "";
  char buf[16];
  size_t j = 0;
  bool has_split = false;
  for (size_t i = 0; s[i] && j + 1 < sizeof(buf); i++) {
    char c = s[i];
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    if (c == 'C' || c == 'M' || c == 'I') {
      buf[j++] = c;
    } else if (c == '|') {
      if (!has_split) {
        buf[j++] = c;
        has_split = true;
      }
    }
  }
  buf[j] = 0;
  if (j == 0) {
    strncpy(ab->appbar_controls, "CIM|", sizeof(ab->appbar_controls) - 1);
    ab->appbar_controls[sizeof(ab->appbar_controls) - 1] = 0;
  } else {
    strncpy(ab->appbar_controls, buf, sizeof(ab->appbar_controls) - 1);
    ab->appbar_controls[sizeof(ab->appbar_controls) - 1] = 0;
  }
  CogitoNode* win = cogito_root(ab);
  if (win && win->kind == COGITO_WINDOW) {
    cogito_window_relayout(win);
  }
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

static bool cogito_css_read_selector(const char** p, char* out, size_t cap) {
  size_t len = 0;
  if (!isalpha((unsigned char)**p) && **p != '_' && **p != '-' && **p != '.' && **p != '#' && **p != '*') return false;
  while (**p && (isalnum((unsigned char)**p) || **p == '_' || **p == '-' || **p == '.' || **p == '#' || **p == ':' || **p == '*')) {
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

static void cogito_css_apply_property(CogitoStyle* style, const char* prop, const char** p, bool selection_mode) {
  if (selection_mode) {
    if (strcmp(prop, "background") == 0 || strcmp(prop, "background-color") == 0 ||
        strcmp(prop, "selection-color") == 0 || strcmp(prop, "selection-background") == 0 ||
        strcmp(prop, "highlight-color") == 0) {
      Color c;
      if (cogito_css_parse_color(p, &c)) { style->has_selection = true; style->selection = c; }
      return;
    }
  } else if (strcmp(prop, "selection-color") == 0 || strcmp(prop, "selection-background") == 0 || strcmp(prop, "highlight-color") == 0) {
    return;
  }
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
  if (strcmp(prop, "min-width") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_min_w = true; style->min_w = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "min-height") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_min_h = true; style->min_h = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "max-width") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_max_w = true; style->max_w = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "max-height") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_max_h = true; style->max_h = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "appbar-btn-size") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_appbar_btn_size = true; style->appbar_btn_size = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "appbar-btn-gap") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_appbar_btn_gap = true; style->appbar_btn_gap = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "appbar-btn-top") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_appbar_btn_pad_top = true; style->appbar_btn_pad_top = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "appbar-btn-right") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_appbar_btn_pad_right = true; style->appbar_btn_pad_right = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "appbar-btn-close-color") == 0) {
    Color c;
    if (cogito_css_parse_color(p, &c)) { style->has_appbar_btn_close_color = true; style->appbar_btn_close_color = c; }
    return;
  }
  if (strcmp(prop, "appbar-btn-min-color") == 0) {
    Color c;
    if (cogito_css_parse_color(p, &c)) { style->has_appbar_btn_min_color = true; style->appbar_btn_min_color = c; }
    return;
  }
  if (strcmp(prop, "appbar-btn-max-color") == 0) {
    Color c;
    if (cogito_css_parse_color(p, &c)) { style->has_appbar_btn_max_color = true; style->appbar_btn_max_color = c; }
    return;
  }
  if (strcmp(prop, "appbar-btn-border-color") == 0) {
    Color c;
    if (cogito_css_parse_color(p, &c)) { style->has_appbar_btn_border_color = true; style->appbar_btn_border_color = c; }
    return;
  }
  if (strcmp(prop, "appbar-btn-border-width") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_appbar_btn_border_width = true; style->appbar_btn_border_width = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "font-weight") == 0) {
    double v = 0;
    const char* s = *p;
    char ident[32];
    if (cogito_css_read_ident(&s, ident, sizeof(ident))) {
      if (strcmp(ident, "bold") == 0) { style->has_font_weight = true; style->font_weight = 700; *p = s; return; }
      if (strcmp(ident, "normal") == 0) { style->has_font_weight = true; style->font_weight = 400; *p = s; return; }
    }
    if (cogito_css_read_number(p, &v)) { style->has_font_weight = true; style->font_weight = (int)lround(v); }
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
  if (strcmp(prop, "item-padding") == 0 || strcmp(prop, "menu-item-padding") == 0) {
    int vals[4] = {0, 0, 0, 0};
    int count = cogito_css_parse_ints(p, vals, 4);
    if (count == 1) {
      style->has_padding = true;
      style->padding_left = vals[0];
      style->padding_top = vals[0];
      style->padding_right = vals[0];
      style->padding_bottom = vals[0];
    } else if (count == 2) {
      style->has_padding = true;
      style->padding_top = vals[0];
      style->padding_bottom = vals[0];
      style->padding_left = vals[1];
      style->padding_right = vals[1];
    } else if (count == 3) {
      style->has_padding = true;
      style->padding_top = vals[0];
      style->padding_left = vals[1];
      style->padding_right = vals[1];
      style->padding_bottom = vals[2];
    } else if (count >= 4) {
      style->has_padding = true;
      style->padding_top = vals[0];
      style->padding_right = vals[1];
      style->padding_bottom = vals[2];
      style->padding_left = vals[3];
    }
    return;
  }
  if (strcmp(prop, "item-height") == 0 || strcmp(prop, "menu-item-height") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_margin_top = true; style->margin_top = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "box-shadow") == 0 || strcmp(prop, "elevation") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) {
      int level = (int)lround(v);
      if (level < 0) level = 0;
      if (level > 5) level = 5;
      style->has_shadow = true;
      style->shadow_level = level;
    }
    return;
  }
  if (strcmp(prop, "track-color") == 0 || strcmp(prop, "track") == 0) {
    Color c;
    if (cogito_css_parse_color(p, &c)) { style->has_track = true; style->track = c; }
    return;
  }
  if (strcmp(prop, "knob-color") == 0 || strcmp(prop, "knob") == 0) {
    Color c;
    if (cogito_css_parse_color(p, &c)) { style->has_knob = true; style->knob = c; }
    return;
  }
  if (strcmp(prop, "knob-width") == 0 || strcmp(prop, "knob-w") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_min_w = true; style->min_w = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "knob-height") == 0 || strcmp(prop, "knob-h") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_min_h = true; style->min_h = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "check-color") == 0 || strcmp(prop, "check") == 0) {
    Color c;
    if (cogito_css_parse_color(p, &c)) { style->has_check = true; style->check = c; }
    return;
  }
}

static int cogito_label_class_from_name(const char* s) {
  if (!s || !s[0]) return COGITO_LABEL_CLASS_NONE;
  if (strcmp(s, "title") == 0) return COGITO_LABEL_CLASS_TITLE;
  if (strcmp(s, "subtitle") == 0 || strcmp(s, "sub-title") == 0) return COGITO_LABEL_CLASS_SUBTITLE;
  if (strcmp(s, "body") == 0) return COGITO_LABEL_CLASS_BODY;
  return COGITO_LABEL_CLASS_NONE;
}

static bool cogito_css_selector_kind(const char* sel, CogitoKind* out_kind, bool* out_base, int* out_label_class, bool* out_menu) {
  if (!sel || !sel[0]) return false;
  char buf[64];
  size_t n = strlen(sel);
  if (n >= sizeof(buf)) n = sizeof(buf) - 1;
  memcpy(buf, sel, n);
  buf[n] = 0;
  char* class_part = NULL;
  if (buf[0] == '.') {
    class_part = buf + 1;
    buf[0] = 0;
  } else {
    char* dot = strchr(buf, '.');
    if (dot) {
      *dot = 0;
      class_part = dot + 1;
    }
  }
  const char* s = buf;
  if (*s == '#') s++;
  if (strcmp(s, "*") == 0 || strcmp(s, "all") == 0 || strcmp(s, "base") == 0 || strcmp(s, "root") == 0) {
    if (out_base) *out_base = true;
    return true;
  }
  if (strcmp(s, "menu") == 0) {
    if (out_menu) *out_menu = true;
    return true;
  }
  if (class_part) {
    int cls = cogito_label_class_from_name(class_part);
    if (cls != COGITO_LABEL_CLASS_NONE) {
      if (s[0] == 0 || strcmp(s, "label") == 0) {
        if (out_label_class) *out_label_class = cls;
        if (out_kind) *out_kind = COGITO_LABEL;
        return true;
      }
    }
    if (s[0] == 0) {
      if (strcmp(class_part, "window") == 0) { if (out_kind) *out_kind = COGITO_WINDOW; return true; }
      if (strcmp(class_part, "vstack") == 0) { if (out_kind) *out_kind = COGITO_VSTACK; return true; }
      if (strcmp(class_part, "hstack") == 0) { if (out_kind) *out_kind = COGITO_HSTACK; return true; }
      if (strcmp(class_part, "button") == 0) { if (out_kind) *out_kind = COGITO_BUTTON; return true; }
      if (strcmp(class_part, "iconbtn") == 0 || strcmp(class_part, "icon-button") == 0) { if (out_kind) *out_kind = COGITO_ICONBTN; return true; }
      if (strcmp(class_part, "image") == 0 || strcmp(class_part, "img") == 0) { if (out_kind) *out_kind = COGITO_IMAGE; return true; }
      if (strcmp(class_part, "label") == 0) { if (out_kind) *out_kind = COGITO_LABEL; return true; }
      if (strcmp(class_part, "checkbox") == 0 || strcmp(class_part, "radio") == 0) { if (out_kind) *out_kind = COGITO_CHECKBOX; return true; }
      if (strcmp(class_part, "switch") == 0) { if (out_kind) *out_kind = COGITO_SWITCH; return true; }
      if (strcmp(class_part, "list") == 0) { if (out_kind) *out_kind = COGITO_LIST; return true; }
      if (strcmp(class_part, "grid") == 0) { if (out_kind) *out_kind = COGITO_GRID; return true; }
      if (strcmp(class_part, "appbar") == 0 || strcmp(class_part, "app-bar") == 0) { if (out_kind) *out_kind = COGITO_APPBAR; return true; }
      if (strcmp(class_part, "menu") == 0) { if (out_menu) *out_menu = true; return true; }
    }
    if (strcmp(class_part, "checkbox") == 0 || strcmp(class_part, "radio") == 0) {
      if (s[0] == 0 || strcmp(s, "checkbox") == 0) {
        if (out_kind) *out_kind = COGITO_CHECKBOX;
        return true;
      }
    }
    if (strcmp(class_part, "switch") == 0) {
      if (s[0] == 0 || strcmp(s, "switch") == 0) {
        if (out_kind) *out_kind = COGITO_SWITCH;
        return true;
      }
    }
  }
  if (strcmp(s, "window") == 0) { if (out_kind) *out_kind = COGITO_WINDOW; return true; }
  if (strcmp(s, "vstack") == 0) { if (out_kind) *out_kind = COGITO_VSTACK; return true; }
  if (strcmp(s, "hstack") == 0) { if (out_kind) *out_kind = COGITO_HSTACK; return true; }
  if (strcmp(s, "zstack") == 0) { if (out_kind) *out_kind = COGITO_ZSTACK; return true; }
  if (strcmp(s, "button") == 0) { if (out_kind) *out_kind = COGITO_BUTTON; return true; }
  if (strcmp(s, "iconbtn") == 0 || strcmp(s, "icon-button") == 0) { if (out_kind) *out_kind = COGITO_ICONBTN; return true; }
  if (strcmp(s, "image") == 0 || strcmp(s, "img") == 0) { if (out_kind) *out_kind = COGITO_IMAGE; return true; }
  if (strcmp(s, "label") == 0) { if (out_kind) *out_kind = COGITO_LABEL; return true; }
  if (strcmp(s, "checkbox") == 0) { if (out_kind) *out_kind = COGITO_CHECKBOX; return true; }
  if (strcmp(s, "switch") == 0) { if (out_kind) *out_kind = COGITO_SWITCH; return true; }
  if (strcmp(s, "list") == 0) { if (out_kind) *out_kind = COGITO_LIST; return true; }
  if (strcmp(s, "grid") == 0) { if (out_kind) *out_kind = COGITO_GRID; return true; }
  if (strcmp(s, "dialog") == 0) { if (out_kind) *out_kind = COGITO_DIALOG; return true; }
  if (strcmp(s, "dialog-slot") == 0 || strcmp(s, "dialogslot") == 0) { if (out_kind) *out_kind = COGITO_DIALOG_SLOT; return true; }
  if (strcmp(s, "appbar") == 0 || strcmp(s, "app-bar") == 0) { if (out_kind) *out_kind = COGITO_APPBAR; return true; }
  return false;
}

static bool cogito_css_parse_token(const char* tok, char* base, size_t base_cap, char* state, size_t state_cap) {
  if (!tok || !tok[0]) return false;
  while (*tok == '.' || *tok == '#') tok++;
  if (!*tok) return false;
  char buf[64];
  size_t len = strlen(tok);
  if (len >= sizeof(buf)) len = sizeof(buf) - 1;
  memcpy(buf, tok, len);
  buf[len] = 0;
  char* state_ptr = NULL;
  char* dot = strchr(buf, '.');
  char* colon = strchr(buf, ':');
  if (dot && colon) state_ptr = dot < colon ? dot : colon;
  else if (dot) state_ptr = dot;
  else if (colon) state_ptr = colon;
  if (state_ptr) {
    *state_ptr = 0;
    state_ptr++;
  }
  if (base && base_cap) {
    strncpy(base, buf, base_cap - 1);
    base[base_cap - 1] = 0;
  }
  if (state && state_cap) {
    state[0] = 0;
    if (state_ptr && state_ptr[0]) {
      strncpy(state, state_ptr, state_cap - 1);
      state[state_cap - 1] = 0;
    }
  }
  return true;
}

static bool cogito_css_selector_has_class(const char* sel, const char* name) {
  if (!sel || !name || !name[0]) return false;
  const char* p = sel;
  while (*p) {
    if (*p == '.') {
      p++;
      const char* start = p;
      while (*p && *p != '.' && *p != ':' && *p != '#' && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r' && *p != ',') p++;
      size_t len = (size_t)(p - start);
      if (len > 0 && strlen(name) == len && strncmp(start, name, len) == 0) return true;
    } else {
      p++;
    }
  }
  return false;
}

static void cogito_css_add_styles(const char* parent, const char* child, CogitoStyle** styles, int* flags, int* count, int max) {
  if (!count || !styles || max <= 0) return;
  if (flags) {
    for (int i = 0; i < max; i++) flags[i] = 0;
  }
  if (!child || !child[0]) {
    bool is_base = false;
    bool is_menu = false;
    CogitoKind kind = COGITO_WINDOW;
    int label_class = COGITO_LABEL_CLASS_NONE;
    char buf[64];
    char state[16] = {0};
    const char* sel = parent;
    if (parent) {
      size_t n = strlen(parent);
      if (n >= sizeof(buf)) n = sizeof(buf) - 1;
      memcpy(buf, parent, n);
      buf[n] = 0;
      char* colon = strchr(buf, ':');
      if (colon) {
        *colon = 0;
        strncpy(state, colon + 1, sizeof(state) - 1);
        state[sizeof(state) - 1] = 0;
        cogito_trim(state);
      }
      sel = buf;
    }
    if (cogito_css_selector_kind(sel, &kind, &is_base, &label_class, &is_menu)) {
      CogitoStyle* style = NULL;
      bool hover = strcmp(state, "hover") == 0;
      bool active = strcmp(state, "active") == 0;
      bool checked = strcmp(state, "checked") == 0;
      if (label_class != COGITO_LABEL_CLASS_NONE) {
        if (checked) style = &cogito_theme.label_class_checked[label_class];
        else if (active) style = &cogito_theme.label_class_active[label_class];
        else if (hover) style = &cogito_theme.label_class_hover[label_class];
        else style = &cogito_theme.label_class[label_class];
      } else if (is_menu) {
        style = &cogito_theme.menu;
      } else {
        if (is_base) {
          style = &cogito_theme.base;
        } else if (checked) {
          style = &cogito_theme.per_kind_checked[kind];
        } else if (active) {
          style = &cogito_theme.per_kind_active[kind];
        } else if (hover) {
          style = &cogito_theme.per_kind_hover[kind];
        } else {
          style = &cogito_theme.per_kind[kind];
        }
      }
      if (style && *count < max) {
        styles[(*count)] = style;
        if (flags && strcmp(state, "selection") == 0) flags[(*count)] = 1;
        (*count)++;
      }
    }
    return;
  }
  char parent_base[32] = {0};
  char child_base[32] = {0};
  char child_state[16] = {0};
  char parent_state[16] = {0};
  if (!cogito_css_parse_token(parent, parent_base, sizeof(parent_base), parent_state, sizeof(parent_state))) return;
  if (!cogito_css_parse_token(child, child_base, sizeof(child_base), child_state, sizeof(child_state))) return;
  bool hover = strcmp(child_state, "hover") == 0;
  bool active = strcmp(child_state, "active") == 0;
  bool checked_state = strcmp(parent_state, "checked") == 0;
  bool parent_radio = strcmp(parent_base, "radio") == 0 || cogito_css_selector_has_class(parent, "radio");
  bool parent_checkbox = strcmp(parent_base, "checkbox") == 0 || cogito_css_selector_has_class(parent, "checkbox");
  bool parent_switch = strcmp(parent_base, "switch") == 0 || cogito_css_selector_has_class(parent, "switch");
  if ((parent_checkbox || parent_radio) && strcmp(child_base, "check") == 0) {
    CogitoStyle* style = NULL;
    if (parent_radio) {
      if (checked_state) style = &cogito_theme.radio_check_checked;
      else if (hover) style = &cogito_theme.radio_check_hover;
      else if (active) style = &cogito_theme.radio_check_active;
      else style = &cogito_theme.radio_check;
    } else {
      if (checked_state) style = &cogito_theme.checkbox_check_checked;
      else if (hover) style = &cogito_theme.checkbox_check_hover;
      else if (active) style = &cogito_theme.checkbox_check_active;
      else style = &cogito_theme.checkbox_check;
    }
    if (*count < max) {
      styles[(*count)] = style;
      if (flags && strcmp(child_state, "selection") == 0) flags[(*count)] = 1;
      (*count)++;
    }
    return;
  }
  if ((parent_checkbox || parent_radio) && strcmp(child_base, "box") == 0) {
    CogitoStyle* style = NULL;
    if (parent_radio) {
      if (checked_state) style = &cogito_theme.radio_box_checked;
      else if (hover) style = &cogito_theme.radio_box_hover;
      else if (active) style = &cogito_theme.radio_box_active;
      else style = &cogito_theme.radio_box;
    } else {
      if (checked_state) style = &cogito_theme.checkbox_box_checked;
      else if (hover) style = &cogito_theme.checkbox_box_hover;
      else if (active) style = &cogito_theme.checkbox_box_active;
      else style = &cogito_theme.checkbox_box;
    }
    if (*count < max) {
      styles[(*count)] = style;
      if (flags && strcmp(child_state, "selection") == 0) flags[(*count)] = 1;
      (*count)++;
    }
    return;
  }
  if (parent_switch && strcmp(child_base, "track") == 0) {
    CogitoStyle* style = NULL;
    if (checked_state) style = &cogito_theme.switch_track_checked;
    else if (hover) style = &cogito_theme.switch_track_hover;
    else if (active) style = &cogito_theme.switch_track_active;
    else style = &cogito_theme.switch_track;
    if (*count < max) {
      styles[(*count)] = style;
      if (flags && strcmp(child_state, "selection") == 0) flags[(*count)] = 1;
      (*count)++;
    }
    return;
  }
  if (parent_switch && strcmp(child_base, "knob") == 0) {
    CogitoStyle* style = NULL;
    if (checked_state) style = &cogito_theme.switch_knob_checked;
    else if (hover) style = &cogito_theme.switch_knob_hover;
    else if (active) style = &cogito_theme.switch_knob_active;
    else style = &cogito_theme.switch_knob;
    if (*count < max) {
      styles[(*count)] = style;
      if (flags && strcmp(child_state, "selection") == 0) flags[(*count)] = 1;
      (*count)++;
    }
    return;
  }
  if (strcmp(parent_base, "appbar") != 0) return;

  if (strcmp(child_base, "button") == 0) {
    CogitoStyle* style = NULL;
    if (hover) style = &cogito_theme.appbar_child_button_hover;
    else if (active) style = &cogito_theme.appbar_child_button_active;
    else style = &cogito_theme.appbar_child_button;
    if (*count < max) {
      styles[(*count)] = style;
      if (flags && strcmp(child_state, "selection") == 0) flags[(*count)] = 1;
      (*count)++;
    }
    return;
  }
  if (strcmp(child_base, "iconbtn") == 0 || strcmp(child_base, "icon-button") == 0) {
    CogitoStyle* style = NULL;
    if (hover) style = &cogito_theme.appbar_child_iconbtn_hover;
    else if (active) style = &cogito_theme.appbar_child_iconbtn_active;
    else style = &cogito_theme.appbar_child_iconbtn;
    if (*count < max) {
      styles[(*count)] = style;
      if (flags && strcmp(child_state, "selection") == 0) flags[(*count)] = 1;
      (*count)++;
    }
    return;
  }
  CogitoStyle* all = NULL;
  if (hover) all = &cogito_theme.appbar_win_btn_hover_all;
  else if (active) all = &cogito_theme.appbar_win_btn_active_all;
  else all = &cogito_theme.appbar_win_btn_all;

  if (strcmp(child_base, "window-button") == 0) {
    if (*count < max) {
      styles[(*count)] = all;
      if (flags && strcmp(child_state, "selection") == 0) flags[(*count)] = 1;
      (*count)++;
    }
    return;
  }
  int idx = -1;
  if (strcmp(child_base, "window-close") == 0) idx = 0;
  else if (strcmp(child_base, "window-min") == 0) idx = 1;
  else if (strcmp(child_base, "window-max") == 0) idx = 2;
  if (idx >= 0) {
    CogitoStyle* style = NULL;
    if (hover) style = &cogito_theme.appbar_win_btn_hover[idx];
    else if (active) style = &cogito_theme.appbar_win_btn_active[idx];
    else style = &cogito_theme.appbar_win_btn[idx];
    if (*count < max) {
      styles[(*count)] = style;
      if (flags && strcmp(child_state, "selection") == 0) flags[(*count)] = 1;
      (*count)++;
    }
    return;
  }
}

static void cogito_parse_css(const char* src) {
  const char* p = src;
  cogito_style_version++;
  while (*p) {
    cogito_css_skip_ws(&p);
    if (!*p) break;
    CogitoStyle* styles[16];
    int style_flags[16] = {0};
    int style_count = 0;
    while (*p) {
      char parent[64] = {0};
      char child[64] = {0};
      if (!cogito_css_read_selector(&p, parent, sizeof(parent))) {
        p++;
        break;
      }
      cogito_css_skip_ws(&p);
      if (*p && *p != '{' && *p != ',') {
        if (!cogito_css_read_selector(&p, child, sizeof(child))) {
          child[0] = 0;
        }
      }
      cogito_css_add_styles(parent, child[0] ? child : NULL, styles, style_flags, &style_count, (int)(sizeof(styles) / sizeof(styles[0])));
      cogito_css_skip_ws(&p);
      if (*p == ',') {
        p++;
        cogito_css_skip_ws(&p);
        continue;
      }
      break;
    }
    cogito_css_skip_ws(&p);
    if (*p != '{') {
      while (*p && *p != '{') p++;
      if (*p == '{') p++;
    } else {
      p++;
    }
    if (style_count == 0) {
      int depth = 1;
      while (*p && depth > 0) {
        if (*p == '{') depth++;
        else if (*p == '}') depth--;
        p++;
      }
      continue;
    }
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
      for (int i = 0; i < style_count; i++) {
        const char* v = vp;
        cogito_css_apply_property(styles[i], prop, &v, style_flags[i] != 0);
      }
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
  if (cogito_theme.menu.has_font_size && cogito_theme.menu.font_size > 0) {
    font_size = cogito_theme.menu.font_size;
  }
  int pad_x = cogito_theme.menu.has_padding_left ? cogito_theme.menu.padding_left : 10;
  int pad_y = cogito_theme.menu.has_padding_top ? cogito_theme.menu.padding_top : 6;
  int item_h = cogito_theme.menu.has_margin_top ? cogito_theme.menu.margin_top : (font_size + pad_y * 2);
  int max_w = 0;
  for (size_t i = 0; i < n->menu_len; i++) {
    const char* text = n->menu_labels[i] ? n->menu_labels[i]->data : "";
    int w = cogito_text_width_size(text, font_size);
    if (w > max_w) max_w = w;
  }
  int w = max_w + pad_x * 2;
  int h = (int)n->menu_len * item_h;
  cogito_menu.open = true;
  cogito_menu.owner = n;
  cogito_menu.item_h = item_h;
  cogito_menu.pad_x = pad_x;
  cogito_menu.pad_y = pad_y;
  cogito_menu.w = w;
  cogito_menu.h = h;
  int x = n->x;
  int y = n->y + n->h + 4;
  int sw = GetScreenWidth();
  int sh = GetScreenHeight();
  if (w > sw - 8) w = sw - 8;
  if (h > sh - 8) h = sh - 8;
  if (x + w > sw - 4) x = sw - 4 - w;
  if (x < 4) x = 4;
  if (y + h > sh - 4) {
    int above = n->y - h - 4;
    if (above >= 4) y = above;
    else y = sh - 4 - h;
  }
  if (y < 4) y = 4;
  cogito_menu.w = w;
  cogito_menu.h = h;
  cogito_menu.x = x;
  cogito_menu.y = y;
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

static Color cogito_blend(Color base, Color over) {
  float a = over.a / 255.0f;
  float ia = 1.0f - a;
  Color out;
  out.r = (unsigned char)(base.r * ia + over.r * a);
  out.g = (unsigned char)(base.g * ia + over.g * a);
  out.b = (unsigned char)(base.b * ia + over.b * a);
  out.a = base.a;
  return out;
}

static void cogito_draw_text(int x, int y, const char* text, Color color) {
  if (!text) return;
  if (cogito_font_ready) {
    DrawTextEx(cogito_font, text, (Vector2){(float)x, (float)y}, (float)cogito_font_size(), 1.0f, color);
  } else {
    DrawText(text, x, y, cogito_font_size(), color);
  }
}

static void cogito_draw_text_size(int x, int y, const char* text, Color color, int size, bool bold) {
  if (!text) return;
  if (size <= 0) size = cogito_font_size();
  if (cogito_font_ready) {
    DrawTextEx(cogito_font, text, (Vector2){(float)x, (float)y}, (float)size, 1.0f, color);
    if (bold) {
      DrawTextEx(cogito_font, text, (Vector2){(float)x + 1.0f, (float)y}, (float)size, 1.0f, color);
    }
  } else {
    DrawText(text, x, y, size, color);
    if (bold) DrawText(text, x + 1, y, size, color);
  }
}

static void cogito_draw_node(CogitoNode* n) {
  if (!n) return;
  switch (n->kind) {
    case COGITO_BUTTON: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      if (n->parent && n->parent->kind == COGITO_APPBAR) {
        int mx = (int)GetMousePosition().x;
        int my = (int)GetMousePosition().y;
        bool over = cogito_hit_rect(mx, my, n->x, n->y, n->w, n->h);
        bool down = over && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        if (down) {
          s = cogito_style_merge(&s, &cogito_theme.appbar_child_button_active);
        } else if (over) {
          s = cogito_style_merge(&s, &cogito_theme.appbar_child_button_hover);
        }
      }
      Color bg = n->bg;
      Color border = n->border_color;
      int border_w = n->border_width;
      int radius = n->border_radius;
      if (s.has_bg) bg = s.bg;
      if (s.has_border) border = s.border;
      if (s.has_border_width) border_w = s.border_width;
      if (s.has_radius) radius = s.radius;
      if (n->bg_set || border_w > 0) {
        if (n->shadow_set && n->shadow_level > 0) {
          cogito_draw_shadow(n->x, n->y, n->w, n->h, radius, n->shadow_level);
        }
        cogito_draw_rect(n->x, n->y, n->w, n->h, bg, radius);
        if (border_w > 0) {
          cogito_draw_rect_lines(n->x, n->y, n->w, n->h, border, radius, border_w);
        }
      }
      if (n->text) {
        int size = cogito_node_font_size(n);
        int tw = cogito_text_width_size(n->text->data, size);
        int th = cogito_text_height_size(size);
        int tx = n->x + (n->w - tw) / 2;
        int ty = n->y + (n->h - th) / 2;
        Color text = n->text_color;
        if (s.has_text) text = s.text;
        cogito_draw_text_size(tx, ty, n->text->data, text, size, cogito_node_bold(n));
      }
      break;
    }
    case COGITO_ICONBTN: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      if (n->parent && n->parent->kind == COGITO_APPBAR) {
        int mx = (int)GetMousePosition().x;
        int my = (int)GetMousePosition().y;
        bool over = cogito_hit_rect(mx, my, n->x, n->y, n->w, n->h);
        bool down = over && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        if (down) {
          s = cogito_style_merge(&s, &cogito_theme.appbar_child_iconbtn_active);
        } else if (over) {
          s = cogito_style_merge(&s, &cogito_theme.appbar_child_iconbtn_hover);
        }
      }
      Color bg = n->bg;
      Color border = n->border_color;
      int border_w = n->border_width;
      int radius = n->border_radius;
      if (s.has_bg) bg = s.bg;
      if (n->parent && n->parent->kind == COGITO_APPBAR) {
        int mx = (int)GetMousePosition().x;
        int my = (int)GetMousePosition().y;
        bool over = cogito_hit_rect(mx, my, n->x, n->y, n->w, n->h);
        bool down = over && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        if (over || down) {
          CogitoStyle overlay = down ? cogito_theme.appbar_child_iconbtn_active : cogito_theme.appbar_child_iconbtn_hover;
          if (overlay.has_bg) {
            bg = cogito_blend(bg, overlay.bg);
          }
        }
      }
      if (s.has_border) border = s.border;
      if (s.has_border_width) border_w = s.border_width;
      if (s.has_radius) radius = s.radius;
      if (n->bg_set || border_w > 0) {
        if (n->shadow_set && n->shadow_level > 0) {
          cogito_draw_shadow(n->x, n->y, n->w, n->h, radius, n->shadow_level);
        }
        cogito_draw_rect(n->x, n->y, n->w, n->h, bg, radius);
        if (border_w > 0) {
          cogito_draw_rect_lines(n->x, n->y, n->w, n->h, border, radius, border_w);
        }
      }
      if (n->text) {
        int btn_min = n->w < n->h ? n->w : n->h;
        int target = 24;
        if (btn_min > 0 && btn_min < target) target = btn_min;
        if (target < 8) target = 8;
        Texture2D tex = {0};
        int tw = 0;
        int th = 0;
        if (cogito_icon_load_texture(n->text->data, target, &tex, &tw, &th)) {
          float scale = 1.0f;
          if (tw > 0 && th > 0) {
            float sx = (float)n->w / (float)tw;
            float sy = (float)n->h / (float)th;
            scale = sx < sy ? sx : sy;
            if (scale > 1.0f) scale = 1.0f;
            if (scale <= 0.0f) scale = 1.0f;
          }
          float dw = tw * scale;
          float dh = th * scale;
          float dx = n->x + (n->w - dw) * 0.5f;
          float dy = n->y + (n->h - dh) * 0.5f;
          DrawTexturePro(tex, (Rectangle){0, 0, (float)tw, (float)th},
                         (Rectangle){dx, dy, dw, dh}, (Vector2){0, 0}, 0.0f, WHITE);
        } else {
          int text_w = cogito_text_width(n->text->data);
          int text_h = cogito_text_height();
          int tx = n->x + (n->w - text_w) / 2;
          int ty = n->y + (n->h - text_h) / 2;
          Color text = n->text_color;
          if (s.has_text) text = s.text;
          cogito_draw_text(tx, ty, n->text->data, text);
        }
      }
      break;
    }
    case COGITO_IMAGE: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      if (s.has_bg) { n->bg = s.bg; n->bg_set = true; }
      else { n->bg_set = false; }
      if (s.has_border) { n->border_color = s.border; n->border_color_set = true; }
      else { n->border_color_set = false; }
      if (s.has_border_width) { n->border_width = s.border_width; n->border_width_set = true; }
      else { n->border_width_set = false; }
      if (s.has_radius) n->border_radius = s.radius;
      if (n->bg_set || n->border_width > 0) {
        if (n->shadow_set && n->shadow_level > 0) {
          cogito_draw_shadow(n->x, n->y, n->w, n->h, n->border_radius, n->shadow_level);
        }
        cogito_draw_rect(n->x, n->y, n->w, n->h, n->bg, n->border_radius);
        if (n->border_width > 0) {
          cogito_draw_rect_lines(n->x, n->y, n->w, n->h, n->border_color, n->border_radius, n->border_width);
        }
      }
      if (n->icon && n->icon->data) {
        int target = n->w < n->h ? n->w : n->h;
        if (target <= 0) target = 16;
        Texture2D tex = {0};
        int tw = 0;
        int th = 0;
        if (cogito_icon_load_texture(n->icon->data, target, &tex, &tw, &th)) {
          float scale = 1.0f;
          if (tw > 0 && th > 0) {
            float sx = (float)n->w / (float)tw;
            float sy = (float)n->h / (float)th;
            scale = sx < sy ? sx : sy;
            if (scale <= 0.0f) scale = 1.0f;
          }
          float dw = tw * scale;
          float dh = th * scale;
          float dx = n->x + (n->w - dw) * 0.5f;
          float dy = n->y + (n->h - dh) * 0.5f;
          DrawTexturePro(tex, (Rectangle){0, 0, (float)tw, (float)th},
                         (Rectangle){dx, dy, dw, dh}, (Vector2){0, 0}, 0.0f, WHITE);
        }
      }
      break;
    }
    case COGITO_LABEL: {
      if (n->text) {
        bool hover = false;
        bool active = false;
        cogito_node_hover_state(n, &hover, &active);
        int size = cogito_label_font_size(n);
        int th = cogito_text_height_size(size);
        int ty = n->y + (n->h - th) / 2;
        Color text = n->text_color;
        CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
        if (s.has_text) text = s.text;
        cogito_draw_text_size(n->x, ty, n->text->data, text, size, cogito_label_bold(n));
      }
      break;
    }
    case COGITO_CHECKBOX: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      int text_w = 0;
      if (n->text) text_w = cogito_text_width(n->text->data);
      int max_box = n->h > 0 ? n->h : 16;
      int avail = n->w - (n->text ? (text_w + 6) : 0);
      if (avail < 12) avail = 12;
      int box = max_box;
      if (box > avail) box = avail;
      if (box < 12) box = 12;
      int cy = n->y + (n->h - box) / 2;
      bool over = hover;
      bool down = active;
      bool is_radio = (n->group.tag != EVT_NULL);
      CogitoStyle check_style = cogito_checkbox_check_style(is_radio, n->checked, over, down);
      CogitoStyle box_style = cogito_checkbox_box_style(is_radio, n->checked, over, down);
      Color check = n->text_color;
      if (check_style.has_text) check = check_style.text;
      else if (n->check_set) check = n->check_color;
      bool has_border = n->border_width_set || n->border_color_set;
      if (s.has_bg) { n->bg = s.bg; n->bg_set = true; }
      if (s.has_border) { n->border_color = s.border; n->border_color_set = true; }
      if (s.has_border_width) { n->border_width = s.border_width; n->border_width_set = true; }
      if (is_radio) {
        if (n->bg_set) {
          DrawCircle(n->x + box / 2, cy + box / 2, box / 2, n->bg);
        }
        if (box_style.has_bg) {
          DrawCircle(n->x + box / 2, cy + box / 2, box / 2, box_style.bg);
        }
        if (has_border) {
          DrawCircleLines(n->x + box / 2, cy + box / 2, box / 2, n->border_color.a ? n->border_color : n->text_color);
        }
        if (n->checked) {
          int r = 4;
          int maxr = box / 2;
          if (r > maxr) r = maxr;
          DrawCircle(n->x + box / 2, cy + box / 2, r, check);
        }
      } else {
        int box_radius = box_style.has_radius ? box_style.radius : 4;
        if (n->bg_set) {
          cogito_draw_rect(n->x, cy, box, box, n->bg, box_radius);
        }
        if (box_style.has_bg) {
          cogito_draw_rect(n->x, cy, box, box, box_style.bg, box_radius);
        }
        if (has_border) {
          cogito_draw_rect_lines(n->x, cy, box, box, n->border_color.a ? n->border_color : n->text_color, box_radius, 1);
        }
        if (n->checked) {
          int mark = 16;
          if (mark > box) mark = box;
          int x0 = n->x + (box - mark) / 2;
          int y0 = cy + (box - mark) / 2;
          int x1 = x0 + (mark * 7) / 16;
          int y1 = y0 + (mark * 12) / 16;
          int x2 = x0 + (mark * 13) / 16;
          int y2 = y0 + (mark * 4) / 16;
          int sx = x0 + (mark * 3) / 16;
          int sy = y0 + (mark * 8) / 16;
          DrawLineEx((Vector2){(float)sx, (float)sy}, (Vector2){(float)x1, (float)y1}, 2.0f, check);
          DrawLineEx((Vector2){(float)x1, (float)y1}, (Vector2){(float)x2, (float)y2}, 2.0f, check);
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
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      int text_w = 0;
      if (n->text) text_w = cogito_text_width(n->text->data);
      int track_w = n->w - (n->text ? (text_w + 8) : 0);
      if (track_w < 24) track_w = 24;
      if (n->min_w_set && track_w < n->min_w) track_w = n->min_w;
      if (n->max_w_set && n->max_w > 0 && track_w > n->max_w) track_w = n->max_w;
      int h = n->h > 0 ? n->h : 18;
      if (n->min_h_set && h < n->min_h) h = n->min_h;
      if (n->max_h_set && n->max_h > 0 && h > n->max_h) h = n->max_h;
      if (h > track_w / 2) h = track_w / 2;
      if (h < 14) h = 14;
      int w = track_w;
      int cy = n->y + (n->h - h) / 2;
      int cx = n->x;
      bool over = hover;
      bool down = active;
      CogitoStyle track_s = cogito_switch_track_style(n->checked, over, down);
      CogitoStyle knob_s = cogito_switch_knob_style(n->checked, over, down);
      Color track_off = n->track_set ? n->track_color : cogito_rgba(170, 170, 170, 255);
      Color track_on = n->track_on_set ? n->track_on_color : cogito_rgba(90, 160, 110, 255);
      if (track_s.has_bg) {
        track_off = track_s.bg;
        track_on = track_s.bg;
      }
      if (track_s.has_track_on) track_on = track_s.track_on;
      Color track = n->checked ? track_on : track_off;
      cogito_draw_rect(cx, cy, w, h, track, h / 2);
      int knob_w = h - 4;
      int knob_h = h - 4;
      if (knob_s.has_min_w && knob_w < knob_s.min_w) knob_w = knob_s.min_w;
      if (knob_s.has_max_w && knob_s.max_w > 0 && knob_w > knob_s.max_w) knob_w = knob_s.max_w;
      if (knob_s.has_min_h && knob_h < knob_s.min_h) knob_h = knob_s.min_h;
      if (knob_s.has_max_h && knob_s.max_h > 0 && knob_h > knob_s.max_h) knob_h = knob_s.max_h;
      if (knob_w < 4) knob_w = 4;
      if (knob_h < 4) knob_h = 4;
      if (knob_w > w - 4) knob_w = w - 4;
      if (knob_h > h - 2) knob_h = h - 2;
      int knob = knob_w;
      int kx = n->checked ? (cx + w - knob - 2) : (cx + 2);
      int ky = cy + (h - knob_h) / 2;
      Color knob_col = n->knob_set ? n->knob_color : cogito_rgba(250, 250, 250, 255);
      if (knob_s.has_bg) knob_col = knob_s.bg;
      else if (knob_s.has_knob) knob_col = knob_s.knob;
      cogito_draw_rect(kx, ky, knob_w, knob_h, knob_col, knob_h / 2);
      if (n->text) {
        int th = cogito_text_height();
        int ty = n->y + (n->h - th) / 2;
        cogito_draw_text(n->x + w + 8, ty, n->text->data, n->text_color);
      }
      break;
    }
    case COGITO_LIST:
    case COGITO_GRID: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      if (s.has_bg) { n->bg = s.bg; n->bg_set = true; }
      if (s.has_border) { n->border_color = s.border; n->border_color_set = true; }
      if (s.has_border_width) { n->border_width = s.border_width; n->border_width_set = true; }
      if (s.has_radius) n->border_radius = s.radius;
      if (n->bg_set || n->border_width > 0) {
        if (n->shadow_set && n->shadow_level > 0) {
          cogito_draw_shadow(n->x, n->y, n->w, n->h, n->border_radius, n->shadow_level);
        }
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
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      if (s.has_bg) { n->bg = s.bg; n->bg_set = true; }
      if (s.has_border) { n->border_color = s.border; n->border_color_set = true; }
      if (s.has_border_width) { n->border_width = s.border_width; n->border_width_set = true; }
      if (s.has_radius) n->border_radius = s.radius;
      if (n->bg_set || n->border_width > 0) {
        if (n->shadow_set && n->shadow_level > 0) {
          cogito_draw_shadow(n->x, n->y, n->w, n->h, n->border_radius, n->shadow_level);
        }
        cogito_draw_rect(n->x, n->y, n->w, n->h, n->bg, n->border_radius);
        if (n->border_width > 0) {
          cogito_draw_rect_lines(n->x, n->y, n->w, n->h, n->border_color, n->border_radius, n->border_width);
        }
      }
      int btn = n->appbar_btn_size;
      int by = n->appbar_btn_y;
      int cx = n->appbar_btn_close_x;
      int r = btn / 2;
      Vector2 mp = GetMousePosition();
      int mx = (int)mp.x;
      int my = (int)mp.y;
      bool down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
      bool over_close = (n->appbar_btn_close_x >= 0) && cogito_hit_rect(mx, my, cx, by, btn, btn);
      bool over_min = (n->appbar_btn_min_x >= 0) && cogito_hit_rect(mx, my, n->appbar_btn_min_x, by, btn, btn);
      bool over_max = (n->appbar_btn_max_x >= 0) && cogito_hit_rect(mx, my, n->appbar_btn_max_x, by, btn, btn);
      int state_close = over_close ? (down ? 2 : 1) : 0;
      int state_min = over_min ? (down ? 2 : 1) : 0;
      int state_max = over_max ? (down ? 2 : 1) : 0;
      CogitoStyle close_s = cogito_appbar_win_btn_style(0, state_close);
      CogitoStyle min_s = cogito_appbar_win_btn_style(1, state_min);
      CogitoStyle max_s = cogito_appbar_win_btn_style(2, state_max);
      Color close_bg = close_s.has_bg ? close_s.bg : cogito_rgba(255, 95, 87, 255);
      Color min_bg = min_s.has_bg ? min_s.bg : cogito_rgba(254, 188, 46, 255);
      Color max_bg = max_s.has_bg ? max_s.bg : cogito_rgba(40, 200, 64, 255);
      if (n->appbar_btn_close_x >= 0) DrawCircle(cx + r, by + r, r, close_bg);
      if (n->appbar_btn_min_x >= 0) DrawCircle(n->appbar_btn_min_x + r, by + r, r, min_bg);
      if (n->appbar_btn_max_x >= 0) DrawCircle(n->appbar_btn_max_x + r, by + r, r, max_bg);
      if (n->appbar_btn_close_x >= 0 && (close_s.has_border || close_s.has_border_width)) {
        int bw = close_s.has_border_width ? close_s.border_width : 1;
        Color bc = close_s.has_border ? close_s.border : cogito_rgba(0, 0, 0, 40);
        if (bw > 0) DrawCircleLines(cx + r, by + r, r, bc);
      }
      if (n->appbar_btn_min_x >= 0 && (min_s.has_border || min_s.has_border_width)) {
        int bw = min_s.has_border_width ? min_s.border_width : 1;
        Color bc = min_s.has_border ? min_s.border : cogito_rgba(0, 0, 0, 40);
        if (bw > 0) DrawCircleLines(n->appbar_btn_min_x + r, by + r, r, bc);
      }
      if (n->appbar_btn_max_x >= 0 && (max_s.has_border || max_s.has_border_width)) {
        int bw = max_s.has_border_width ? max_s.border_width : 1;
        Color bc = max_s.has_border ? max_s.border : cogito_rgba(0, 0, 0, 40);
        if (bw > 0) DrawCircleLines(n->appbar_btn_max_x + r, by + r, r, bc);
      }
      if (n->text) {
        int tsz = cogito_label_class_font_size(COGITO_LABEL_CLASS_TITLE, 24);
        int ty = n->appbar_title_y ? n->appbar_title_y : (n->y + 12);
        Color tcol = cogito_label_class_color(COGITO_LABEL_CLASS_TITLE, n->text_color);
        bool tbold = cogito_label_class_bold(COGITO_LABEL_CLASS_TITLE, true);
        cogito_draw_text_size(n->x + 16, ty, n->text->data, tcol, tsz, tbold);
      }
      if (n->subtitle) {
        int tsz = cogito_label_class_font_size(COGITO_LABEL_CLASS_TITLE, 24);
        int t_h = cogito_text_height_size(tsz);
        int ssz = cogito_label_class_font_size(COGITO_LABEL_CLASS_SUBTITLE, 18);
        int sy = n->appbar_subtitle_y ? n->appbar_subtitle_y : (n->y + 12 + t_h + 4);
        Color scol = cogito_label_class_color(COGITO_LABEL_CLASS_SUBTITLE, n->text_color);
        bool sbold = cogito_label_class_bold(COGITO_LABEL_CLASS_SUBTITLE, false);
        cogito_draw_text_size(n->x + 16, sy, n->subtitle->data, scol, ssz, sbold);
      }
      for (size_t i = 0; i < n->len; i++) {
        cogito_draw_node(n->children[i]);
      }
      break;
    }
    case COGITO_DIALOG: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      if (s.has_bg) { n->bg = s.bg; n->bg_set = true; }
      if (s.has_border) { n->border_color = s.border; n->border_color_set = true; }
      if (s.has_border_width) { n->border_width = s.border_width; n->border_width_set = true; }
      if (s.has_radius) n->border_radius = s.radius;
      if (s.has_shadow) { n->shadow_level = s.shadow_level; n->shadow_set = true; }
      if (n->bg_set || n->border_width > 0) {
        if (n->shadow_set && n->shadow_level > 0) {
          cogito_draw_shadow(n->x, n->y, n->w, n->h, n->border_radius, n->shadow_level);
        }
        cogito_draw_rect(n->x, n->y, n->w, n->h, n->bg, n->border_radius);
        if (n->border_width > 0) {
          cogito_draw_rect_lines(n->x, n->y, n->w, n->h, n->border_color, n->border_radius, n->border_width);
        }
      }
      for (size_t i = 0; i < n->len; i++) {
        cogito_draw_node(n->children[i]);
      }
      break;
    }
    case COGITO_DIALOG_SLOT:
      if (n->dialog && n->dialog_open) {
        CogitoNode* win = cogito_node_window(n);
        if (win) {
          DrawRectangle(0, 0, win->w, win->h, cogito_rgba(0, 0, 0, 82));
        } else {
          DrawRectangle(n->x, n->y, n->w, n->h, cogito_rgba(0, 0, 0, 82));
        }
        cogito_draw_node(n->dialog);
      }
      break;
    case COGITO_VSTACK:
    case COGITO_HSTACK:
    case COGITO_ZSTACK:
    case COGITO_WINDOW:
      for (size_t i = 0; i < n->len; i++) {
        cogito_draw_node(n->children[i]);
      }
      if (n->kind == COGITO_WINDOW && n->dialog) {
        if (n->dialog->kind == COGITO_DIALOG_SLOT) {
          cogito_draw_node(n->dialog);
        } else if (n->dialog_open) {
          DrawRectangle(0, 0, n->w, n->h, cogito_rgba(0, 0, 0, 82));
          cogito_draw_node(n->dialog);
        }
      }
      break;
    case COGITO_KIND_COUNT:
      break;
  }
}

static void cogito_draw_menu(void) {
  if (!cogito_menu.open || !cogito_menu.owner) return;
  Color bg = cogito_theme.menu.has_bg ? cogito_theme.menu.bg : cogito_rgba(250, 250, 250, 255);
  Color border = cogito_theme.menu.has_border ? cogito_theme.menu.border : cogito_rgba(180, 180, 180, 255);
  Color text_color = cogito_theme.menu.has_text ? cogito_theme.menu.text : cogito_rgba(30, 30, 30, 255);
  Color sel = cogito_theme.menu.has_selection ? cogito_theme.menu.selection : cogito_rgba(220, 230, 250, 255);
  int radius = cogito_theme.menu.has_radius ? cogito_theme.menu.radius : 4;
  bool has_border = cogito_theme.menu.has_border || cogito_theme.menu.has_border_width;
  int border_w = cogito_theme.menu.has_border_width ? cogito_theme.menu.border_width : 1;
  int shadow_level = cogito_theme.menu.has_shadow ? cogito_theme.menu.shadow_level : 0;
  int font_size = cogito_font_size();
  if (cogito_theme.menu.has_font_size && cogito_theme.menu.font_size > 0) {
    font_size = cogito_theme.menu.font_size;
  }
  int x = cogito_menu.x;
  int y = cogito_menu.y;
  int w = cogito_menu.w;
  int h = cogito_menu.h;
  if (shadow_level > 0) {
    cogito_draw_shadow(x, y, w, h, radius, shadow_level);
  }
  cogito_draw_rect(x, y, w, h, bg, radius);
  if (has_border && border_w > 0) {
    cogito_draw_rect_lines(x, y, w, h, border, radius, border_w);
  }
  int mx = (int)GetMousePosition().x;
  int my = (int)GetMousePosition().y;
  int hover = cogito_menu_hit(&cogito_menu, mx, my);
  for (size_t i = 0; i < cogito_menu.owner->menu_len; i++) {
    int iy = y + (int)i * cogito_menu.item_h;
    if ((int)i == hover) {
      cogito_draw_rect(x + 2, iy + 2, w - 4, cogito_menu.item_h - 4, sel, 3);
    }
    const char* text = cogito_menu.owner->menu_labels[i] ? cogito_menu.owner->menu_labels[i]->data : "";
    int th = cogito_text_height_size(font_size);
    int ty = iy + (cogito_menu.item_h - th) / 2;
    cogito_draw_text_size(x + cogito_menu.pad_x, ty, text, text_color, font_size, false);
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
  if (win->resizable) SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
  else SetConfigFlags(FLAG_MSAA_4X_HINT);
#else
  if (appbar) {
    if (win->resizable) SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_UNDECORATED | FLAG_WINDOW_RESIZABLE);
    else SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_UNDECORATED);
  } else {
    if (win->resizable) SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    else SetConfigFlags(FLAG_MSAA_4X_HINT);
  }
#endif
  InitWindow(win->w, win->h, title);
  cogito_raylib_ready = true;
#if defined(__APPLE__)
  cogito_configure_macos_window(appbar != NULL);
#endif
  const char* font_path = getenv("COGITO_FONT");
  if (!font_path || !font_path[0]) {
    font_path = cogito_default_font_path();
  }
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
      cogito_font_path_active = font_path;
    }
  }
  if (!cogito_font_ready) {
    cogito_font = GetFontDefault();
    cogito_font_ready = true;
    SetTextureFilter(cogito_font.texture, TEXTURE_FILTER_BILINEAR);
    cogito_font_loaded_size = cogito_font_size();
    cogito_font_from_env = false;
    cogito_font_path_active = NULL;
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
          if (appbar->appbar_btn_close_x >= 0 &&
              cogito_hit_rect(mx, my, appbar->appbar_btn_close_x, appbar->appbar_btn_y, appbar->appbar_btn_size, appbar->appbar_btn_size)) {
            cogito_appbar_window_action(win, 0);
          } else if (appbar->appbar_btn_min_x >= 0 &&
                     cogito_hit_rect(mx, my, appbar->appbar_btn_min_x, appbar->appbar_btn_y, appbar->appbar_btn_size, appbar->appbar_btn_size)) {
            cogito_appbar_window_action(win, 1);
          } else if (appbar->appbar_btn_max_x >= 0 &&
                     cogito_hit_rect(mx, my, appbar->appbar_btn_max_x, appbar->appbar_btn_y, appbar->appbar_btn_size, appbar->appbar_btn_size)) {
            cogito_appbar_window_action(win, 2);
          }
          else if (cogito_hit_rect(mx, my, appbar->x, appbar->y, appbar->w, appbar->h) &&
                   !cogito_point_over_appbar_buttons(appbar, mx, my)) {
#if defined(__APPLE__)
            cogito_macos_drag_window();
#else
            cogito_drag_pending = true;
            cogito_drag_pending_mouse = mp;
            cogito_drag_last_move = 0.0;
#endif
          }
        }
        CogitoNode* slot = (win->dialog && win->dialog->kind == COGITO_DIALOG_SLOT && win->dialog->dialog_open && win->dialog->dialog)
          ? win->dialog
          : cogito_find_dialog_slot(win);
        CogitoNode* hit = (slot && slot->dialog) ? cogito_find_clickable(slot->dialog, mx, my)
                                                 : (win->dialog_open && win->dialog ? cogito_find_clickable(win->dialog, mx, my)
                                                                                   : cogito_find_clickable(win, mx, my));
        if (hit) {
          if (hit->kind == COGITO_BUTTON || hit->kind == COGITO_ICONBTN) {
            if (hit->menu_len > 0) {
              cogito_menu_open_for(hit);
            } else {
              cogito_invoke_click(hit);
            }
          } else if (hit->kind == COGITO_CHECKBOX || hit->kind == COGITO_SWITCH) {
            bool prev = hit->checked;
            if (hit->kind == COGITO_CHECKBOX && hit->group.tag != EVT_NULL) {
              hit->checked = true;
              cogito_uncheck_group(win, hit, hit->group);
            } else {
              hit->checked = !hit->checked;
            }
            if (hit->checked && hit->group.tag != EVT_NULL) {
              cogito_uncheck_group(win, hit, hit->group);
            }
            if (!hit->checked) {
              hit->bg_set = false;
              hit->border_color_set = false;
              hit->border_width_set = false;
            }
            if (hit->checked != prev) {
              cogito_invoke_change(hit);
            }
          }
        }
        if (slot && slot->dialog) {
          cogito_handle_click(slot->dialog, mx, my, true);
        } else if (win->dialog_open && win->dialog) {
          cogito_handle_click(win->dialog, mx, my, true);
        } else {
          cogito_handle_click(win, mx, my, true);
        }
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
