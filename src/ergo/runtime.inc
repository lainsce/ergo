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
  COGITO_FIXED,
  COGITO_SCROLLER,
  COGITO_TEXTFIELD,
  COGITO_TEXTVIEW,
  COGITO_SEARCHFIELD,
  COGITO_DROPDOWN,
  COGITO_DATEPICKER,
  COGITO_STEPPER,
  COGITO_SLIDER,
  COGITO_TABS,
  COGITO_SEGMENTED,
  COGITO_VIEWSWITCHER,
  COGITO_PROGRESS,
  COGITO_TREEVIEW,
  COGITO_COLORPICKER,
  COGITO_TOASTS,
  COGITO_TOAST,
  COGITO_TOOLBAR,
  COGITO_TOOLTIP,
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
  bool has_icon_size;
  bool has_icon_color;
  bool has_transition_ms;
  bool has_transition_ease;
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
  int icon_size;
  Color icon_color;
  int transition_ms;
  int transition_ease;
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
  CogitoStyle per_kind_disabled[COGITO_KIND_COUNT];
  CogitoStyle label_class[4];
  CogitoStyle label_class_hover[4];
  CogitoStyle label_class_active[4];
  CogitoStyle label_class_checked[4];
  CogitoStyle label_class_disabled[4];
  CogitoStyle class_mono;
  CogitoStyle class_mono_hover;
  CogitoStyle class_mono_active;
  CogitoStyle class_mono_checked;
  CogitoStyle class_mono_disabled;
  CogitoStyle class_tabular;
  CogitoStyle class_tabular_hover;
  CogitoStyle class_tabular_active;
  CogitoStyle class_tabular_checked;
  CogitoStyle class_tabular_disabled;
  CogitoStyle menu;
  CogitoStyle menu_item;
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
  ErgoStr** tab_labels;
  ErgoStr** tab_ids;
  size_t tab_len;
  size_t tab_cap;
  ErgoStr* text;
  ErgoStr* subtitle;
  ErgoStr* icon;
  ErgoStr* tooltip;
  ErgoStr* view_id;
  ErgoStr* view_active_id;
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
  int grid_gap_x;
  int grid_gap_y;
  int grid_align_x;
  int grid_align_y;
  int grid_span_x;
  int grid_span_y;
  int fixed_x;
  int fixed_y;
  int scroll_x;
  int scroll_y;
  int scroll_content_w;
  int scroll_content_h;
  bool scroll_horz;
  bool scroll_vert;
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
  struct CogitoNode* view_switcher;
  bool dialog_open;
  bool resizable;
  bool fixed_pos_set;
  bool text_wrap;
  bool text_ellipsis;
  int text_align;
  bool disabled;
  double slider_min;
  double slider_max;
  double slider_value;
  bool slider_dragging;
  double stepper_min;
  double stepper_max;
  double stepper_value;
  double stepper_step;
  int date_year;
  int date_month;
  int date_day;
  double color_h;
  double color_c;
  double color_t;
  bool color_hex_editing;
  int caret;
  int sel_start;
  int sel_end;
  bool selecting;
  bool font_mono;
  bool font_tabular;
  ErgoStr* a11y_label;
  ErgoStr* a11y_role;
  bool anim_inited;
  double anim_start;
  int anim_ms;
  int anim_ease;
  Color anim_bg;
  Color anim_bg_from;
  Color anim_bg_target;
  Color anim_text;
  Color anim_text_from;
  Color anim_text_target;
  Color anim_border;
  Color anim_border_from;
  Color anim_border_target;
  Color anim_selection;
  Color anim_selection_from;
  Color anim_selection_target;
  double progress_value;
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
  int pad_top;
  int pad_bottom;
} CogitoMenu;

static CogitoTheme cogito_theme;
static bool cogito_theme_initialized = false;
static bool cogito_raylib_ready = false;
static CogitoMenu cogito_menu = {0};
static Font cogito_font;
static bool cogito_font_ready = false;
static Font cogito_font_mono;
static bool cogito_font_mono_ready = false;
static int cogito_font_size_value = 16;
static int cogito_font_loaded_size = 0;
static bool cogito_font_from_env = false;
static bool cogito_css_default_loaded = false;
static int cogito_style_version = 0;
static CogitoNode* cogito_focused = NULL;
static CogitoNode* cogito_slider_active = NULL;
static CogitoNode* cogito_colorpicker_active = NULL;
static int cogito_colorpicker_channel = -1;
static CogitoNode* cogito_pointer_capture = NULL;
static CogitoNode* cogito_tooltip_hovered = NULL;
enum {
  COGITO_LABEL_CLASS_NONE = 0,
  COGITO_LABEL_CLASS_TITLE,
  COGITO_LABEL_CLASS_SUBTITLE,
  COGITO_LABEL_CLASS_BODY
};
enum {
  COGITO_STYLE_CLASS_NONE = 0,
  COGITO_STYLE_CLASS_MONO,
  COGITO_STYLE_CLASS_TABULAR
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

static void cogito_macos_set_window_a11y_label(const char* label) {
  id nsw =
#if defined(__OBJC__)
      (__bridge id)GetWindowHandle();
#else
      (id)GetWindowHandle();
#endif
  if (!nsw) return;
  SEL sel = sel_registerName("setAccessibilityLabel:");
  if (!sel) return;
  ((void (*)(id, SEL, id))objc_msgSend)(nsw, sel, cogito_nsstring(label ? label : ""));
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

static Font* cogito_font_for_node(const CogitoNode* n) {
  if (n && (n->font_mono || n->font_tabular) && cogito_font_mono_ready) return &cogito_font_mono;
  return &cogito_font;
}

static bool cogito_color_eq(Color a, Color b) {
  return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static Color cogito_color_lerp(Color a, Color b, double t) {
  if (t <= 0.0) return a;
  if (t >= 1.0) return b;
  Color out;
  out.r = (unsigned char)lround(a.r + (b.r - a.r) * t);
  out.g = (unsigned char)lround(a.g + (b.g - a.g) * t);
  out.b = (unsigned char)lround(a.b + (b.b - a.b) * t);
  out.a = (unsigned char)lround(a.a + (b.a - a.a) * t);
  return out;
}

static double cogito_ease(double t, int ease) {
  if (t <= 0.0) return 0.0;
  if (t >= 1.0) return 1.0;
  switch (ease) {
    case 2: return t * t; // ease-in
    case 3: { double u = 1.0 - t; return 1.0 - u * u; } // ease-out
    case 4: { // ease-in-out
      if (t < 0.5) return 2.0 * t * t;
      double u = 1.0 - t;
      return 1.0 - 2.0 * u * u;
    }
    case 1: // ease
      return t * t * (3.0 - 2.0 * t);
    default: return t; // linear
  }
}

static Color cogito_anim_color(CogitoNode* n, Color target, Color* cur, Color* from, Color* tgt, double now) {
  if (!n) return target;
  if (!n->anim_inited) {
    *cur = target;
    *from = target;
    *tgt = target;
    n->anim_inited = true;
    return target;
  }
  if (!cogito_color_eq(*tgt, target)) {
    *from = *cur;
    *tgt = target;
    n->anim_start = now;
  }
  if (n->anim_ms <= 0) {
    *cur = target;
    return target;
  }
  double t = (now - n->anim_start) * 1000.0 / (double)n->anim_ms;
  if (t >= 1.0) {
    *cur = target;
    return target;
  }
  double e = cogito_ease(t, n->anim_ease);
  *cur = cogito_color_lerp(*from, *tgt, e);
  return *cur;
}

static void cogito_anim_apply(CogitoNode* n, const CogitoStyle* s, Color* bg, Color* text, Color* border, Color* selection) {
  if (!n || !s) return;
  if (s->has_transition_ms) n->anim_ms = s->transition_ms;
  if (s->has_transition_ease) n->anim_ease = s->transition_ease;
  double now = GetTime();
  if (bg) *bg = cogito_anim_color(n, *bg, &n->anim_bg, &n->anim_bg_from, &n->anim_bg_target, now);
  if (text) *text = cogito_anim_color(n, *text, &n->anim_text, &n->anim_text_from, &n->anim_text_target, now);
  if (border) *border = cogito_anim_color(n, *border, &n->anim_border, &n->anim_border_from, &n->anim_border_target, now);
  if (selection) *selection = cogito_anim_color(n, *selection, &n->anim_selection, &n->anim_selection_from, &n->anim_selection_target, now);
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
    cogito_style_clear(&cogito_theme.per_kind_disabled[i]);
  }
  for (int i = 0; i < 4; i++) {
    cogito_style_clear(&cogito_theme.label_class[i]);
    cogito_style_clear(&cogito_theme.label_class_hover[i]);
    cogito_style_clear(&cogito_theme.label_class_active[i]);
    cogito_style_clear(&cogito_theme.label_class_checked[i]);
    cogito_style_clear(&cogito_theme.label_class_disabled[i]);
  }
  cogito_style_clear(&cogito_theme.class_mono);
  cogito_style_clear(&cogito_theme.class_mono_hover);
  cogito_style_clear(&cogito_theme.class_mono_active);
  cogito_style_clear(&cogito_theme.class_mono_checked);
  cogito_style_clear(&cogito_theme.class_mono_disabled);
  cogito_style_clear(&cogito_theme.class_tabular);
  cogito_style_clear(&cogito_theme.class_tabular_hover);
  cogito_style_clear(&cogito_theme.class_tabular_active);
  cogito_style_clear(&cogito_theme.class_tabular_checked);
  cogito_style_clear(&cogito_theme.class_tabular_disabled);
  cogito_style_clear(&cogito_theme.menu);
  cogito_style_clear(&cogito_theme.menu_item);
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
  if (over->has_icon_size) { out.has_icon_size = true; out.icon_size = over->icon_size; }
  if (over->has_icon_color) { out.has_icon_color = true; out.icon_color = over->icon_color; }
  if (over->has_transition_ms) { out.has_transition_ms = true; out.transition_ms = over->transition_ms; }
  if (over->has_transition_ease) { out.has_transition_ease = true; out.transition_ease = over->transition_ease; }
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
  if (n->font_mono) {
    s = cogito_style_merge(&s, &cogito_theme.class_mono);
  }
  if (n->font_tabular) {
    s = cogito_style_merge(&s, &cogito_theme.class_tabular);
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
  if (n->disabled) return;
  if (cogito_pointer_capture) {
    if (cogito_pointer_capture != n) return;
    *out_hover = true;
    *out_active = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    return;
  }
  int mx = (int)GetMousePosition().x;
  int my = (int)GetMousePosition().y;
  bool hover = cogito_hit_rect(mx, my, n->x, n->y, n->w, n->h);
  bool active = hover && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
  *out_hover = hover;
  *out_active = active;
}

static bool cogito_node_disabled(const CogitoNode* n);
static void cogito_toast_close_rect(CogitoNode* n, int* out_x, int* out_y, int* out_w, int* out_h);

static CogitoStyle cogito_theme_resolve_node_state(const CogitoNode* n, bool hover, bool active) {
  CogitoStyle s = cogito_theme_resolve_node(n);
  bool checked = (n && (n->kind == COGITO_CHECKBOX || n->kind == COGITO_SWITCH)) ? n->checked : false;
  if (n->kind == COGITO_LABEL && n->label_class > COGITO_LABEL_CLASS_NONE && n->label_class < 4) {
    if (checked) s = cogito_style_merge(&s, &cogito_theme.label_class_checked[n->label_class]);
    if (cogito_node_disabled(n)) {
      s = cogito_style_merge(&s, &cogito_theme.label_class_disabled[n->label_class]);
    }
    if (!cogito_node_disabled(n)) {
      if (active) s = cogito_style_merge(&s, &cogito_theme.label_class_active[n->label_class]);
      else if (hover) s = cogito_style_merge(&s, &cogito_theme.label_class_hover[n->label_class]);
    }
  }
  if (n->font_mono) {
    if (checked) s = cogito_style_merge(&s, &cogito_theme.class_mono_checked);
    if (cogito_node_disabled(n)) {
      s = cogito_style_merge(&s, &cogito_theme.class_mono_disabled);
    }
    if (!cogito_node_disabled(n)) {
      if (active) s = cogito_style_merge(&s, &cogito_theme.class_mono_active);
      else if (hover) s = cogito_style_merge(&s, &cogito_theme.class_mono_hover);
    }
  }
  if (n->font_tabular) {
    if (checked) s = cogito_style_merge(&s, &cogito_theme.class_tabular_checked);
    if (cogito_node_disabled(n)) {
      s = cogito_style_merge(&s, &cogito_theme.class_tabular_disabled);
    }
    if (!cogito_node_disabled(n)) {
      if (active) s = cogito_style_merge(&s, &cogito_theme.class_tabular_active);
      else if (hover) s = cogito_style_merge(&s, &cogito_theme.class_tabular_hover);
    }
  }
  if (checked) s = cogito_style_merge(&s, &cogito_theme.per_kind_checked[n->kind]);
  if (cogito_node_disabled(n)) {
    s = cogito_style_merge(&s, &cogito_theme.per_kind_disabled[n->kind]);
    return s;
  }
  if (active) s = cogito_style_merge(&s, &cogito_theme.per_kind_active[n->kind]);
  else if (hover) s = cogito_style_merge(&s, &cogito_theme.per_kind_hover[n->kind]);
  return s;
}

static void cogito_apply_style_to_node(CogitoNode* n) {
  if (!n) return;
  cogito_theme_init();
  CogitoStyle s = cogito_theme_resolve_node(n);
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

static bool cogito_node_disabled(const CogitoNode* n) {
  return n && n->disabled;
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

static void cogito_node_set_text(CogitoNode* n, ErgoStr* s);

static int cogito_text_width_size_n(const char* s, int len, int size);

static int cogito_text_width_size_font(Font* f, const char* s, int size) {
  if (!s) return 0;
  if (size <= 0) size = cogito_font_size();
  if (!cogito_raylib_ready || !cogito_font_ready || !f) {
    return MeasureText(s, size);
  }
  Vector2 v = MeasureTextEx(*f, s, (float)size, 1.0f);
  return (int)lroundf(v.x);
}

static int cogito_text_width_size_node(const CogitoNode* n, const char* s, int size) {
  Font* f = cogito_font_for_node(n);
  return cogito_text_width_size_font(f, s, size);
}

static int cogito_text_width_size_n_font(Font* f, const char* s, int len, int size) {
  if (!s || len <= 0) return 0;
  if (len > 1023) len = 1023;
  char buf[1024];
  memcpy(buf, s, (size_t)len);
  buf[len] = '\0';
  return cogito_text_width_size_font(f, buf, size);
}

static int cogito_text_width_size_n_node(const CogitoNode* n, const char* s, int len, int size) {
  Font* f = cogito_font_for_node(n);
  return cogito_text_width_size_n_font(f, s, len, size);
}

static void cogito_text_set_from_buf(CogitoNode* n, const char* buf, size_t len) {
  if (!n) return;
  ErgoStr* s = stdr_str_from_slice(buf, len);
  cogito_node_set_text(n, s);
  if (s) ergo_release_val(EV_STR(s));
}

static void cogito_text_delete_range(CogitoNode* n, size_t start, size_t end) {
  if (!n || !n->text) return;
  if (start > end) { size_t tmp = start; start = end; end = tmp; }
  size_t len = n->text->len;
  if (start > len) start = len;
  if (end > len) end = len;
  if (start == end) return;
  size_t new_len = len - (end - start);
  char* buf = (char*)malloc(new_len + 1);
  if (!buf) return;
  if (start > 0) memcpy(buf, n->text->data, start);
  if (end < len) memcpy(buf + start, n->text->data + end, len - end);
  buf[new_len] = '\0';
  cogito_text_set_from_buf(n, buf, new_len);
  free(buf);
}

static void cogito_text_append_codepoint(CogitoNode* n, int cp) {
  if (!n || cp <= 0) return;
  if (n->sel_start != n->sel_end) {
    size_t start = (size_t)n->sel_start;
    size_t end = (size_t)n->sel_end;
    cogito_text_delete_range(n, start, end);
    n->caret = (int)((start < end) ? start : end);
    n->sel_start = n->caret;
    n->sel_end = n->caret;
  }
  char utf8[5] = {0, 0, 0, 0, 0};
  int ulen = 0;
  if (cp <= 0x7F) {
    utf8[0] = (char)cp;
    ulen = 1;
  } else if (cp <= 0x7FF) {
    utf8[0] = (char)(0xC0 | ((cp >> 6) & 0x1F));
    utf8[1] = (char)(0x80 | (cp & 0x3F));
    ulen = 2;
  } else if (cp <= 0xFFFF) {
    utf8[0] = (char)(0xE0 | ((cp >> 12) & 0x0F));
    utf8[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
    utf8[2] = (char)(0x80 | (cp & 0x3F));
    ulen = 3;
  } else {
    utf8[0] = (char)(0xF0 | ((cp >> 18) & 0x07));
    utf8[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    utf8[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    utf8[3] = (char)(0x80 | (cp & 0x3F));
    ulen = 4;
  }
  size_t base_len = n->text ? n->text->len : 0;
  size_t insert_at = (size_t)n->caret;
  if (insert_at > base_len) insert_at = base_len;
  size_t out_len = base_len + (size_t)ulen;
  char* buf = (char*)malloc(out_len + 1);
  if (!buf) return;
  if (insert_at > 0) memcpy(buf, n->text->data, insert_at);
  memcpy(buf + insert_at, utf8, (size_t)ulen);
  if (base_len > insert_at) memcpy(buf + insert_at + (size_t)ulen, n->text->data + insert_at, base_len - insert_at);
  buf[out_len] = '\0';
  cogito_text_set_from_buf(n, buf, out_len);
  free(buf);
  n->caret = (int)(insert_at + (size_t)ulen);
  n->sel_start = n->caret;
  n->sel_end = n->caret;
}

static void cogito_text_backspace(CogitoNode* n) {
  if (!n || !n->text || n->text->len == 0) return;
  if (n->sel_start != n->sel_end) {
    size_t start = (size_t)n->sel_start;
    size_t end = (size_t)n->sel_end;
    cogito_text_delete_range(n, start, end);
    n->caret = (int)((start < end) ? start : end);
    n->sel_start = n->caret;
    n->sel_end = n->caret;
    return;
  }
  size_t len = n->text->len;
  size_t i = (size_t)n->caret;
  if (i == 0) return;
  i--;
  while (i > 0 && ((unsigned char)n->text->data[i] & 0xC0) == 0x80) {
    i--;
  }
  size_t start = i;
  size_t end = (size_t)n->caret;
  cogito_text_delete_range(n, start, end);
  n->caret = (int)start;
  n->sel_start = n->caret;
  n->sel_end = n->caret;
}

static int cogito_text_index_from_x(const CogitoNode* n, const char* text, int size, int target_x) {
  if (!text || target_x <= 0) return 0;
  int len = (int)strlen(text);
  int last = 0;
  for (int i = 1; i <= len; i++) {
    int w = cogito_text_width_size_n_node(n, text, i, size);
    if (w >= target_x) return i;
    last = i;
  }
  return last;
}

static int cogito_text_line_index_from_pos(const CogitoNode* n, const char* text, int size, int max_w, int line_y, int line_h, int x, int y) {
  if (!text) return 0;
  if (max_w <= 0) max_w = 1;
  int len = (int)strlen(text);
  int idx = 0;
  int cur_y = line_y;
  while (idx < len) {
    int line_start = idx;
    int line_len = 0;
    int last_space = -1;
    int last_fit = 0;
    for (int i = idx; i < len; i++) {
      char c = text[i];
      if (c == '\n') {
        line_len = i - idx;
        idx = i + 1;
        break;
      }
      int w = cogito_text_width_size_n_node(n, text + idx, i - idx + 1, size);
      if (c == ' ') last_space = i;
      if (w > max_w) {
        if (last_space >= idx) {
          line_len = last_space - idx;
          idx = last_space + 1;
        } else {
          line_len = i - idx;
          idx = i;
        }
        break;
      }
      last_fit = i;
      if (i == len - 1) {
        line_len = i - idx + 1;
        idx = len;
      }
    }
    if (line_len < 0) line_len = 0;
    if (y >= cur_y && y <= cur_y + line_h) {
      int local_x = x;
      if (local_x < 0) local_x = 0;
      int caret = cogito_text_index_from_x(n, text + line_start, size, local_x);
      if (caret > line_len) caret = line_len;
      return line_start + caret;
    }
    cur_y += line_h;
  }
  return len;
}

static int cogito_text_width_size_n(const char* s, int len, int size) {
  return cogito_text_width_size_n_font(&cogito_font, s, len, size);
}

static bool cogito_is_leap_year(int y) {
  return ((y % 4) == 0 && (y % 100) != 0) || (y % 400) == 0;
}

static int cogito_days_in_month(int y, int m) {
  static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (m < 1) m = 1;
  if (m > 12) m = 12;
  int d = days[m - 1];
  if (m == 2 && cogito_is_leap_year(y)) d = 29;
  return d;
}

static int cogito_weekday(int y, int m, int d) {
  struct tm t;
  memset(&t, 0, sizeof(t));
  t.tm_year = y - 1900;
  t.tm_mon = m - 1;
  t.tm_mday = d;
  mktime(&t);
  return t.tm_wday;
}

static void cogito_datepicker_ensure_date(CogitoNode* n) {
  if (!n || n->kind != COGITO_DATEPICKER) return;
  if (n->date_year <= 0 || n->date_month <= 0) {
    time_t now = time(NULL);
    struct tm* lt = localtime(&now);
    if (lt) {
      n->date_year = lt->tm_year + 1900;
      n->date_month = lt->tm_mon + 1;
      n->date_day = lt->tm_mday;
    } else {
      n->date_year = 2025;
      n->date_month = 1;
      n->date_day = 1;
    }
  }
  if (n->date_month < 1) n->date_month = 1;
  if (n->date_month > 12) n->date_month = 12;
  int dim = cogito_days_in_month(n->date_year, n->date_month);
  if (n->date_day < 1) n->date_day = 1;
  if (n->date_day > dim) n->date_day = dim;
}

static void cogito_datepicker_shift_month(CogitoNode* n, int delta) {
  if (!n) return;
  cogito_datepicker_ensure_date(n);
  int m = n->date_month + delta;
  int y = n->date_year;
  while (m < 1) { m += 12; y -= 1; }
  while (m > 12) { m -= 12; y += 1; }
  n->date_month = m;
  n->date_year = y;
  int dim = cogito_days_in_month(n->date_year, n->date_month);
  if (n->date_day > dim) n->date_day = dim;
}

static void cogito_datepicker_layout(CogitoNode* n, int* out_pad_l, int* out_pad_t, int* out_pad_r, int* out_pad_b,
                                     int* out_header_h, int* out_grid_x, int* out_grid_y, int* out_cell_w, int* out_cell_h) {
  CogitoStyle s = cogito_theme_resolve_node(n);
  int pad_l = s.has_padding_left ? s.padding_left : 10;
  int pad_t = s.has_padding_top ? s.padding_top : 10;
  int pad_r = s.has_padding_right ? s.padding_right : pad_l;
  int pad_b = s.has_padding_bottom ? s.padding_bottom : pad_t;
  int size = cogito_node_font_size(n);
  int th = cogito_text_height_size(size);
  int header_h = th + 8;
  int grid_x = n->x + pad_l;
  int grid_y = n->y + pad_t + header_h + 6;
  int grid_w = n->w - pad_l - pad_r;
  int grid_h = n->h - pad_t - pad_b - header_h - 6;
  int cell_w = grid_w / 7;
  int cell_h = grid_h / 6;
  if (cell_w < 1) cell_w = 1;
  if (cell_h < 1) cell_h = 1;
  if (out_pad_l) *out_pad_l = pad_l;
  if (out_pad_t) *out_pad_t = pad_t;
  if (out_pad_r) *out_pad_r = pad_r;
  if (out_pad_b) *out_pad_b = pad_b;
  if (out_header_h) *out_header_h = header_h;
  if (out_grid_x) *out_grid_x = grid_x;
  if (out_grid_y) *out_grid_y = grid_y;
  if (out_cell_w) *out_cell_w = cell_w;
  if (out_cell_h) *out_cell_h = cell_h;
}

static int cogito_datepicker_hit_day(CogitoNode* n, int mx, int my) {
  int grid_x = 0, grid_y = 0, cell_w = 0, cell_h = 0;
  cogito_datepicker_layout(n, NULL, NULL, NULL, NULL, NULL, &grid_x, &grid_y, &cell_w, &cell_h);
  int grid_w = cell_w * 7;
  int grid_h = cell_h * 6;
  if (!cogito_hit_rect(mx, my, grid_x, grid_y, grid_w, grid_h)) return -1;
  int col = (mx - grid_x) / cell_w;
  int row = (my - grid_y) / cell_h;
  if (col < 0 || col > 6 || row < 0 || row > 5) return -1;
  int first = cogito_weekday(n->date_year, n->date_month, 1);
  int idx = row * 7 + col;
  int day = idx - first + 1;
  int dim = cogito_days_in_month(n->date_year, n->date_month);
  if (day < 1 || day > dim) return -1;
  return day;
}

static int cogito_datepicker_hit_header(CogitoNode* n, int mx, int my) {
  int pad_l = 0, pad_t = 0, pad_r = 0, header_h = 0;
  cogito_datepicker_layout(n, &pad_l, &pad_t, &pad_r, NULL, &header_h, NULL, NULL, NULL, NULL);
  int left_x = n->x + pad_l;
  int right_x = n->x + n->w - pad_r - header_h;
  int y = n->y + pad_t;
  if (cogito_hit_rect(mx, my, left_x, y, header_h, header_h)) return -1;
  if (cogito_hit_rect(mx, my, right_x, y, header_h, header_h)) return 1;
  return 0;
}

typedef struct {
  double n;
  double aw;
  double nbb;
  double ncb;
  double c;
  double nc;
  double z;
  double fl;
  double rgb_d[3];
} CogitoCam16Viewing;

static bool cogito_cam16_inited = false;
static CogitoCam16Viewing cogito_cam16 = {0};
static const double cogito_pi = 3.14159265358979323846;

static double cogito_lstar_from_y(double y) {
  double y_n = y / 100.0;
  if (y_n <= 216.0 / 24389.0) return y_n * 903.3;
  return 116.0 * cbrt(y_n) - 16.0;
}

static double cogito_y_from_lstar(double lstar) {
  if (lstar <= 8.0) return (lstar / 903.3) * 100.0;
  double fy = (lstar + 16.0) / 116.0;
  return (fy * fy * fy) * 100.0;
}

static double cogito_srgb_to_linear(double c) {
  if (c <= 0.04045) return c / 12.92;
  return pow((c + 0.055) / 1.055, 2.4);
}

static double cogito_linear_to_srgb(double c) {
  if (c <= 0.0031308) return 12.92 * c;
  return 1.055 * pow(c, 1.0 / 2.4) - 0.055;
}

static void cogito_xyz_from_srgb(Color c, double* x, double* y, double* z) {
  double r = cogito_srgb_to_linear((double)c.r / 255.0);
  double g = cogito_srgb_to_linear((double)c.g / 255.0);
  double b = cogito_srgb_to_linear((double)c.b / 255.0);
  *x = 100.0 * (0.41233895 * r + 0.35762064 * g + 0.18051042 * b);
  *y = 100.0 * (0.2126 * r + 0.7152 * g + 0.0722 * b);
  *z = 100.0 * (0.01932141 * r + 0.11916382 * g + 0.95034478 * b);
}

static bool cogito_srgb_from_xyz(double x, double y, double z, Color* out) {
  double xr = x / 100.0;
  double yr = y / 100.0;
  double zr = z / 100.0;
  double r = 3.241377479 * xr + -1.537665240 * yr + -0.498853668 * zr;
  double g = -0.969145251 * xr + 1.875885345 * yr + 0.041565856 * zr;
  double b = 0.055620936 * xr + -0.203955245 * yr + 1.057179911 * zr;
  r = cogito_linear_to_srgb(r);
  g = cogito_linear_to_srgb(g);
  b = cogito_linear_to_srgb(b);
  if (r < 0.0 || r > 1.0 || g < 0.0 || g > 1.0 || b < 0.0 || b > 1.0) return false;
  out->r = (unsigned char)(r * 255.0 + 0.5);
  out->g = (unsigned char)(g * 255.0 + 0.5);
  out->b = (unsigned char)(b * 255.0 + 0.5);
  out->a = 255;
  return true;
}

static void cogito_cam16_init(void) {
  if (cogito_cam16_inited) return;
  cogito_cam16_inited = true;
  double la = 64.0;
  double yb = 20.0;
  double yw = 100.0;
  double f = 1.0;
  double c = 0.69;
  double nc = 1.0;
  double n = yb / yw;
  double z = 1.48 + sqrt(n);
  double nbb = 0.725 / pow(n, 0.2);
  double ncb = nbb;
  double k = 1.0 / (5.0 * la + 1.0);
  double k4 = k * k * k * k;
  double fl = 0.2 * k4 * 5.0 * la + 0.1 * pow(1.0 - k4, 2.0) * cbrt(5.0 * la);
  double d = f * (1.0 - (1.0 / 3.6) * exp((-la - 42.0) / 92.0));
  if (d > 1.0) d = 1.0;
  if (d < 0.0) d = 0.0;
  double xw = 95.047;
  double zw = 108.883;
  double r_w = 0.401288 * xw + 0.650173 * yw + -0.051461 * zw;
  double g_w = -0.250268 * xw + 1.204414 * yw + 0.045854 * zw;
  double b_w = -0.002079 * xw + 0.048952 * yw + 0.953127 * zw;
  double dr = d * yw / r_w + 1.0 - d;
  double dg = d * yw / g_w + 1.0 - d;
  double db = d * yw / b_w + 1.0 - d;
  double r_wc = dr * r_w;
  double g_wc = dg * g_w;
  double b_wc = db * b_w;
  double r_f = pow(fl * fabs(r_wc) / 100.0, 0.42);
  double g_f = pow(fl * fabs(g_wc) / 100.0, 0.42);
  double b_f = pow(fl * fabs(b_wc) / 100.0, 0.42);
  double r_a = (r_wc >= 0 ? 1.0 : -1.0) * (400.0 * r_f / (r_f + 27.13)) + 0.1;
  double g_a = (g_wc >= 0 ? 1.0 : -1.0) * (400.0 * g_f / (g_f + 27.13)) + 0.1;
  double b_a = (b_wc >= 0 ? 1.0 : -1.0) * (400.0 * b_f / (b_f + 27.13)) + 0.1;
  double aw = (2.0 * r_a + g_a + 0.05 * b_a - 0.305) * nbb;
  cogito_cam16.n = n;
  cogito_cam16.aw = aw;
  cogito_cam16.nbb = nbb;
  cogito_cam16.ncb = ncb;
  cogito_cam16.c = c;
  cogito_cam16.nc = nc;
  cogito_cam16.z = z;
  cogito_cam16.fl = fl;
  cogito_cam16.rgb_d[0] = dr;
  cogito_cam16.rgb_d[1] = dg;
  cogito_cam16.rgb_d[2] = db;
}

typedef struct {
  double h;
  double c;
} CogitoCam16;

static CogitoCam16 cogito_cam16_from_xyz(double x, double y, double z) {
  cogito_cam16_init();
  const CogitoCam16Viewing* vc = &cogito_cam16;
  double r = 0.401288 * x + 0.650173 * y + -0.051461 * z;
  double g = -0.250268 * x + 1.204414 * y + 0.045854 * z;
  double b = -0.002079 * x + 0.048952 * y + 0.953127 * z;
  r *= vc->rgb_d[0];
  g *= vc->rgb_d[1];
  b *= vc->rgb_d[2];
  double r_f = pow(vc->fl * fabs(r) / 100.0, 0.42);
  double g_f = pow(vc->fl * fabs(g) / 100.0, 0.42);
  double b_f = pow(vc->fl * fabs(b) / 100.0, 0.42);
  double r_a = (r >= 0 ? 1.0 : -1.0) * (400.0 * r_f / (r_f + 27.13)) + 0.1;
  double g_a = (g >= 0 ? 1.0 : -1.0) * (400.0 * g_f / (g_f + 27.13)) + 0.1;
  double b_a = (b >= 0 ? 1.0 : -1.0) * (400.0 * b_f / (b_f + 27.13)) + 0.1;
  double a = (11.0 * r_a - 12.0 * g_a + b_a) / 11.0;
  double bb = (r_a + g_a - 2.0 * b_a) / 9.0;
  double u = (20.0 * r_a + 20.0 * g_a + 21.0 * b_a) / 20.0;
  double h = atan2(bb, a) * (180.0 / cogito_pi);
  if (h < 0.0) h += 360.0;
  double h_rad = h * (cogito_pi / 180.0);
  double e_h = 0.25 * (cos(h_rad + 2.0) + 3.8);
  double A = (2.0 * r_a + g_a + 0.05 * b_a - 0.305) * vc->nbb;
  double J = 100.0 * pow(A / vc->aw, vc->c * vc->z);
  double t = (50000.0 / 13.0) * vc->nc * vc->ncb * e_h * sqrt(a * a + bb * bb) / (u + 0.305);
  double alpha = pow(t, 0.9) * pow(1.64 - pow(0.29, vc->n), 0.73);
  double C = alpha * sqrt(J / 100.0);
  CogitoCam16 cam = {h, C};
  return cam;
}

static void cogito_cam16_to_xyz(double J, double C, double h, double* x, double* y, double* z) {
  cogito_cam16_init();
  const CogitoCam16Viewing* vc = &cogito_cam16;
  double h_rad = h * (cogito_pi / 180.0);
  double e_h = 0.25 * (cos(h_rad + 2.0) + 3.8);
  double A = vc->aw * pow(J / 100.0, 1.0 / (vc->c * vc->z));
  double t = (C <= 0.0 || J <= 0.0) ? 0.0
    : pow(C / (sqrt(J / 100.0) * pow(1.64 - pow(0.29, vc->n), 0.73)), 1.0 / 0.9);
  double p1 = (50000.0 / 13.0) * vc->nc * vc->ncb * e_h / (t > 0.0 ? t : 1e-6);
  double p2 = A / vc->nbb + 0.305;
  double sin_h = sin(h_rad);
  double cos_h = cos(h_rad);
  double gamma = (p2 * 23.0) / (p1 * (23.0 * cos_h + 11.0 * sin_h) + 108.0 * sin_h);
  double a = gamma * cos_h;
  double bb = gamma * sin_h;
  double r_a = (460.0 * p2 + 451.0 * a + 288.0 * bb) / 1403.0;
  double g_a = (460.0 * p2 - 891.0 * a - 261.0 * bb) / 1403.0;
  double b_a = (460.0 * p2 - 220.0 * a - 6300.0 * bb) / 1403.0;
  double r_c = (r_a - 0.1);
  double g_c = (g_a - 0.1);
  double b_c = (b_a - 0.1);
  double r_c_abs = fabs(r_c);
  double g_c_abs = fabs(g_c);
  double b_c_abs = fabs(b_c);
  if (r_c_abs >= 399.999) r_c_abs = 399.999;
  if (g_c_abs >= 399.999) g_c_abs = 399.999;
  if (b_c_abs >= 399.999) b_c_abs = 399.999;
  double r = (r_c >= 0 ? 1.0 : -1.0) * 100.0 / vc->fl * pow((27.13 * r_c_abs) / (400.0 - r_c_abs), 1.0 / 0.42);
  double g = (g_c >= 0 ? 1.0 : -1.0) * 100.0 / vc->fl * pow((27.13 * g_c_abs) / (400.0 - g_c_abs), 1.0 / 0.42);
  double b = (b_c >= 0 ? 1.0 : -1.0) * 100.0 / vc->fl * pow((27.13 * b_c_abs) / (400.0 - b_c_abs), 1.0 / 0.42);
  r /= vc->rgb_d[0];
  g /= vc->rgb_d[1];
  b /= vc->rgb_d[2];
  *x = 1.86206786 * r + -1.01125463 * g + 0.14918677 * b;
  *y = 0.38752654 * r + 0.62144744 * g + -0.00897398 * b;
  *z = -0.01584150 * r + -0.03412294 * g + 1.04996444 * b;
}

static bool cogito_find_hct_color(double h, double c, double t, Color* out) {
  double low = 0.0;
  double high = 100.0;
  Color best = {0, 0, 0, 255};
  bool best_ok = false;
  double best_delta = 1e9;
  for (int i = 0; i < 24; i++) {
    double mid = (low + high) * 0.5;
    double x = 0.0, y = 0.0, z = 0.0;
    cogito_cam16_to_xyz(mid, c, h, &x, &y, &z);
    double lstar = cogito_lstar_from_y(y);
    Color rgb = {0, 0, 0, 255};
    bool in_gamut = cogito_srgb_from_xyz(x, y, z, &rgb);
    if (in_gamut) {
      double delta = fabs(lstar - t);
      if (delta < best_delta) {
        best = rgb;
        best_delta = delta;
        best_ok = true;
      }
    }
    if (lstar < t) low = mid;
    else high = mid;
  }
  if (!best_ok) return false;
  *out = best;
  return true;
}

static Color cogito_hct_to_rgb(double h, double c, double t) {
  if (t <= 0.0) t = 0.0;
  if (t >= 100.0) t = 100.0;
  if (c < 1e-4) {
    double y = cogito_y_from_lstar(t);
    Color gray = {0, 0, 0, 255};
    double x = y * 0.95047;
    double z = y * 1.08883;
    cogito_srgb_from_xyz(x, y, z, &gray);
    return gray;
  }
  double max_c = c;
  double min_c = 0.0;
  Color best = {0, 0, 0, 255};
  bool found = false;
  for (int i = 0; i < 24; i++) {
    double mid = (min_c + max_c) * 0.5;
    Color rgb = {0, 0, 0, 255};
    if (cogito_find_hct_color(h, mid, t, &rgb)) {
      best = rgb;
      found = true;
      min_c = mid;
    } else {
      max_c = mid;
    }
  }
  if (!found) {
    double y = cogito_y_from_lstar(t);
    Color gray = {0, 0, 0, 255};
    double x = y * 0.95047;
    double z = y * 1.08883;
    cogito_srgb_from_xyz(x, y, z, &gray);
    return gray;
  }
  return best;
}

static void cogito_rgb_to_hct(Color c, double* out_h, double* out_c, double* out_t) {
  double x = 0.0, y = 0.0, z = 0.0;
  cogito_xyz_from_srgb(c, &x, &y, &z);
  CogitoCam16 cam = cogito_cam16_from_xyz(x, y, z);
  double t = cogito_lstar_from_y(y);
  if (out_h) *out_h = cam.h;
  if (out_c) *out_c = cam.c;
  if (out_t) *out_t = t;
}

static void cogito_color_to_hex(Color c, char out[8]) {
  snprintf(out, 8, "#%02X%02X%02X", c.r, c.g, c.b);
}

static bool cogito_hex_to_color(const char* s, Color* out) {
  if (!s || !out) return false;
  if (s[0] == '#') s++;
  if (strlen(s) < 6) return false;
  unsigned int r = 0, g = 0, b = 0;
  if (sscanf(s, "%02x%02x%02x", &r, &g, &b) != 3) return false;
  *out = (Color){(unsigned char)r, (unsigned char)g, (unsigned char)b, 255};
  return true;
}

static void cogito_colorpicker_sync_hex(CogitoNode* n) {
  if (!n || n->kind != COGITO_COLORPICKER) return;
  Color c = cogito_hct_to_rgb(n->color_h, n->color_c, n->color_t);
  char buf[8];
  cogito_color_to_hex(c, buf);
  cogito_text_set_from_buf(n, buf, strlen(buf));
}

static void cogito_colorpicker_layout(CogitoNode* n, int* out_pad_l, int* out_pad_t, int* out_pad_r, int* out_pad_b,
                                      int* out_label_w, int* out_row_h, int* out_slider_h, int* out_gap,
                                      int* out_hex_x, int* out_hex_y, int* out_hex_w, int* out_hex_h) {
  CogitoStyle s = cogito_theme_resolve_node(n);
  int pad_l = s.has_padding_left ? s.padding_left : 10;
  int pad_t = s.has_padding_top ? s.padding_top : 10;
  int pad_r = s.has_padding_right ? s.padding_right : pad_l;
  int pad_b = s.has_padding_bottom ? s.padding_bottom : pad_t;
  int size = cogito_node_font_size(n);
  int row_h = size + 10;
  int slider_h = 6;
  int gap = 8;
  int label_w = 12;
  int hex_x = n->x + pad_l;
  int hex_y = n->y + pad_t + (row_h + gap) * 3;
  int hex_w = n->w - pad_l - pad_r;
  int hex_h = row_h;
  if (out_pad_l) *out_pad_l = pad_l;
  if (out_pad_t) *out_pad_t = pad_t;
  if (out_pad_r) *out_pad_r = pad_r;
  if (out_pad_b) *out_pad_b = pad_b;
  if (out_label_w) *out_label_w = label_w;
  if (out_row_h) *out_row_h = row_h;
  if (out_slider_h) *out_slider_h = slider_h;
  if (out_gap) *out_gap = gap;
  if (out_hex_x) *out_hex_x = hex_x;
  if (out_hex_y) *out_hex_y = hex_y;
  if (out_hex_w) *out_hex_w = hex_w;
  if (out_hex_h) *out_hex_h = hex_h;
}

static void cogito_colorpicker_slider_rect(CogitoNode* n, int idx, int* out_x, int* out_y, int* out_w, int* out_h) {
  int pad_l = 0, pad_t = 0, pad_r = 0, label_w = 0, row_h = 0, slider_h = 0, gap = 0;
  cogito_colorpicker_layout(n, &pad_l, &pad_t, &pad_r, NULL, &label_w, &row_h, &slider_h, &gap, NULL, NULL, NULL, NULL);
  int row_y = n->y + pad_t + idx * (row_h + gap);
  int x = n->x + pad_l + label_w + 6;
  int w = n->w - pad_l - pad_r - label_w - 6;
  int y = row_y + (row_h - slider_h) / 2;
  if (w < 1) w = 1;
  if (out_x) *out_x = x;
  if (out_y) *out_y = y;
  if (out_w) *out_w = w;
  if (out_h) *out_h = slider_h;
}

static bool cogito_colorpicker_hex_rect(CogitoNode* n, int* out_x, int* out_y, int* out_w, int* out_h) {
  int hx = 0, hy = 0, hw = 0, hh = 0;
  cogito_colorpicker_layout(n, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &hx, &hy, &hw, &hh);
  if (out_x) *out_x = hx;
  if (out_y) *out_y = hy;
  if (out_w) *out_w = hw;
  if (out_h) *out_h = hh;
  return true;
}

static int cogito_colorpicker_hit_slider(CogitoNode* n, int mx, int my) {
  for (int i = 0; i < 3; i++) {
    int sx = 0, sy = 0, sw = 0, sh = 0;
    cogito_colorpicker_slider_rect(n, i, &sx, &sy, &sw, &sh);
    if (cogito_hit_rect(mx, my, sx, sy - 6, sw, sh + 12)) return i;
  }
  return -1;
}

static bool cogito_colorpicker_set_from_mouse(CogitoNode* n, int channel, int mx) {
  int sx = 0, sy = 0, sw = 0, sh = 0;
  cogito_colorpicker_slider_rect(n, channel, &sx, &sy, &sw, &sh);
  double t = (sw > 0) ? ((double)(mx - sx) / (double)sw) : 0.0;
  if (t < 0.0) t = 0.0;
  if (t > 1.0) t = 1.0;
  double prev = 0.0;
  if (channel == 0) { prev = n->color_h; n->color_h = t * 360.0; }
  else if (channel == 1) { prev = n->color_c; n->color_c = t * 100.0; }
  else { prev = n->color_t; n->color_t = t * 100.0; }
  if (!n->color_hex_editing) {
    cogito_colorpicker_sync_hex(n);
  }
  return prev != (channel == 0 ? n->color_h : (channel == 1 ? n->color_c : n->color_t));
}

static int cogito_text_wrap_height(const char* text, int size, int max_w) {
  if (!text || max_w <= 0) return 0;
  if (size <= 0) size = cogito_font_size();
  int line_h = cogito_text_height_size(size);
  int lines = 0;
  const char* p = text;
  while (*p) {
    while (*p == '\n') { lines++; p++; }
    if (!*p) break;
    int line_len = 0;
    int line_w = 0;
    int last_space = -1;
    int last_space_w = 0;
    const char* start = p;
    while (*p && *p != '\n') {
      int w = cogito_text_width_size_n(start, line_len + 1, size);
      if (*p == ' ' || *p == '\t') {
        last_space = line_len;
        last_space_w = w;
      }
      if (w > max_w && line_len > 0) {
        if (last_space >= 0) {
          line_len = last_space;
          line_w = last_space_w;
          p = start + line_len + 1;
        } else {
          line_w = cogito_text_width_size_n(start, line_len, size);
        }
        break;
      }
      line_len++;
      line_w = w;
      p++;
    }
    if (*p == '\n') p++;
    (void)line_w;
    lines++;
  }
  if (lines == 0) lines = 1;
  return lines * line_h;
}

static void cogito_draw_text_size(int x, int y, const char* text, Color color, int size, bool bold);

static void cogito_draw_text_ellipsis(int x, int y, int w, const char* text, Color color, int size, bool bold, int align) {
  if (!text || w <= 0) return;
  if (size <= 0) size = cogito_font_size();
  int len = (int)strlen(text);
  int full_w = cogito_text_width_size(text, size);
  if (full_w <= w) {
    int tx = x;
    if (align == 1) tx = x + (w - full_w) / 2;
    else if (align == 2) tx = x + (w - full_w);
    cogito_draw_text_size(tx, y, text, color, size, bold);
    return;
  }
  const char* dots = "...";
  int dots_w = cogito_text_width_size(dots, size);
  int max_w = w - dots_w;
  if (max_w < 0) max_w = 0;
  int cut = len;
  for (int i = 0; i < len; i++) {
    int ww = cogito_text_width_size_n(text, i + 1, size);
    if (ww > max_w) { cut = i; break; }
  }
  if (cut < 0) cut = 0;
  char buf[1024];
  if (cut > 1000) cut = 1000;
  memcpy(buf, text, (size_t)cut);
  buf[cut] = '\0';
  int line_w = cogito_text_width_size(buf, size) + dots_w;
  int tx = x;
  if (align == 1) tx = x + (w - line_w) / 2;
  else if (align == 2) tx = x + (w - line_w);
  cogito_draw_text_size(tx, y, buf, color, size, bold);
  cogito_draw_text_size(tx + cogito_text_width_size(buf, size), y, dots, color, size, bold);
}

static int cogito_draw_text_wrapped(int x, int y, int w, const char* text, Color color, int size, bool bold, int align, bool draw) {
  if (!text || w <= 0) return 0;
  if (size <= 0) size = cogito_font_size();
  int line_h = cogito_text_height_size(size);
  int lines = 0;
  const char* p = text;
  while (*p) {
    while (*p == '\n') { if (draw) { lines++; } else { lines++; } p++; }
    if (!*p) break;
    int line_len = 0;
    int last_space = -1;
    int last_space_w = 0;
    const char* start = p;
    int line_w = 0;
    while (*p && *p != '\n') {
      int wcur = cogito_text_width_size_n(start, line_len + 1, size);
      if (*p == ' ' || *p == '\t') {
        last_space = line_len;
        last_space_w = wcur;
      }
      if (wcur > w && line_len > 0) {
        if (last_space >= 0) {
          line_len = last_space;
          line_w = last_space_w;
          p = start + line_len + 1;
        } else {
          line_w = cogito_text_width_size_n(start, line_len, size);
        }
        break;
      }
      line_len++;
      line_w = wcur;
      p++;
    }
    if (line_len > 0) {
      int tx = x;
      if (align == 1) tx = x + (w - line_w) / 2;
      else if (align == 2) tx = x + (w - line_w);
      if (draw) {
        char buf[1024];
        int n = line_len;
        if (n > 1000) n = 1000;
        memcpy(buf, start, (size_t)n);
        buf[n] = '\0';
        cogito_draw_text_size(tx, y + lines * line_h, buf, color, size, bold);
      }
    }
    if (*p == '\n') p++;
    lines++;
  }
  if (lines == 0) lines = 1;
  return lines * line_h;
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

static bool cogito_str_eq(const ErgoStr* a, const ErgoStr* b) {
  if (!a || !b) return false;
  if (a->len != b->len) return false;
  return memcmp(a->data, b->data, a->len) == 0;
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
  if (n->tooltip) {
    ergo_release_val(EV_STR(n->tooltip));
    n->tooltip = NULL;
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
  if (n->a11y_label) {
    ergo_release_val(EV_STR(n->a11y_label));
    n->a11y_label = NULL;
  }
  if (n->a11y_role) {
    ergo_release_val(EV_STR(n->a11y_role));
    n->a11y_role = NULL;
  }
  if (n->tab_labels) {
    for (size_t i = 0; i < n->tab_len; i++) {
      if (n->tab_labels[i]) ergo_release_val(EV_STR(n->tab_labels[i]));
      if (n->tab_ids && n->tab_ids[i]) ergo_release_val(EV_STR(n->tab_ids[i]));
    }
    free(n->tab_labels);
    free(n->tab_ids);
    n->tab_labels = NULL;
    n->tab_ids = NULL;
    n->tab_len = 0;
    n->tab_cap = 0;
  }
  if (n->view_id) {
    ergo_release_val(EV_STR(n->view_id));
    n->view_id = NULL;
  }
  if (n->view_active_id) {
    ergo_release_val(EV_STR(n->view_active_id));
    n->view_active_id = NULL;
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
  n->tooltip = NULL;
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
  n->grid_gap_x = 6;
  n->grid_gap_y = 6;
  n->grid_align_x = -1;
  n->grid_align_y = -1;
  n->grid_span_x = 1;
  n->grid_span_y = 1;
  n->fixed_x = 0;
  n->fixed_y = 0;
  n->scroll_x = 0;
  n->scroll_y = 0;
  n->scroll_content_w = 0;
  n->scroll_content_h = 0;
  n->scroll_horz = false;
  n->scroll_vert = true;
  n->selected = -1;
  n->should_close = false;
  n->menu_labels = NULL;
  n->menu_handlers = NULL;
  n->menu_len = 0;
  n->menu_cap = 0;
  n->tab_labels = NULL;
  n->tab_ids = NULL;
  n->tab_len = 0;
  n->tab_cap = 0;
  n->appbar_btn_close_x = 0;
  n->appbar_btn_min_x = 0;
  n->appbar_btn_max_x = 0;
  n->appbar_btn_y = 0;
  n->appbar_btn_size = 0;
  n->appbar_title_y = 0;
  n->appbar_subtitle_y = 0;
  n->appbar_controls[0] = 0;
  n->dialog = NULL;
  n->view_switcher = NULL;
  n->dialog_open = false;
  n->resizable = true;
  n->fixed_pos_set = false;
  n->text_wrap = false;
  n->text_ellipsis = false;
  n->text_align = 0;
  n->disabled = false;
  n->view_id = NULL;
  n->view_active_id = NULL;
  n->slider_min = 0.0;
  n->slider_max = 1.0;
  n->slider_value = 0.0;
  n->slider_dragging = false;
  n->stepper_min = 0.0;
  n->stepper_max = 100.0;
  n->stepper_value = 0.0;
  n->stepper_step = 1.0;
  n->date_year = 0;
  n->date_month = 0;
  n->date_day = 0;
  n->color_h = 0.0;
  n->color_c = 0.0;
  n->color_t = 0.0;
  n->color_hex_editing = false;
  n->caret = 0;
  n->sel_start = 0;
  n->sel_end = 0;
  n->selecting = false;
  n->font_mono = false;
  n->font_tabular = false;
  n->a11y_label = NULL;
  n->a11y_role = NULL;
  n->anim_inited = false;
  n->anim_start = 0.0;
  n->anim_ms = 0;
  n->anim_ease = 0;
  n->anim_bg = cogito_rgba(0, 0, 0, 0);
  n->anim_bg_from = n->anim_bg;
  n->anim_bg_target = n->anim_bg;
  n->anim_text = cogito_rgba(0, 0, 0, 0);
  n->anim_text_from = n->anim_text;
  n->anim_text_target = n->anim_text;
  n->anim_border = cogito_rgba(0, 0, 0, 0);
  n->anim_border_from = n->anim_border;
  n->anim_border_target = n->anim_border;
  n->anim_selection = cogito_rgba(0, 0, 0, 0);
  n->anim_selection_from = n->anim_selection;
  n->anim_selection_target = n->anim_selection;
  n->progress_value = 0.0;
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
  int len = n->text ? (int)n->text->len : 0;
  if (n->caret > len) n->caret = len;
  if (n->sel_start > len) n->sel_start = len;
  if (n->sel_end > len) n->sel_end = len;
}

static void cogito_node_set_subtitle(CogitoNode* n, ErgoStr* s) {
  if (!n) return;
  if (n->subtitle) ergo_release_val(EV_STR(n->subtitle));
  n->subtitle = s;
  if (s) ergo_retain_val(EV_STR(s));
}

static void cogito_node_set_tooltip(CogitoNode* n, ErgoStr* s) {
  if (!n) return;
  if (n->tooltip) ergo_release_val(EV_STR(n->tooltip));
  n->tooltip = s;
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
      if (n->text_wrap && n->max_w_set && n->max_w > 0) {
        w = n->max_w;
        h = cogito_text_wrap_height(n->text ? n->text->data : "", cogito_node_font_size(n), n->max_w);
      } else {
        w = text_w + 4;
        h = text_h;
      }
      break;
    case COGITO_CHECKBOX:
      w = text_w + 32;
      h = text_h + 4;
      break;
    case COGITO_SWITCH:
      w = text_w + 40;
      h = text_h + 4;
      break;
    case COGITO_TEXTFIELD:
      w = 160;
      h = text_h + 10;
      break;
    case COGITO_TEXTVIEW:
      w = 200;
      h = 120;
      break;
    case COGITO_SEARCHFIELD:
      w = 180;
      h = text_h + 10;
      break;
    case COGITO_DROPDOWN:
      w = 160;
      h = text_h + 10;
      break;
    case COGITO_DATEPICKER:
      w = 260;
      h = 220;
      break;
    case COGITO_STEPPER:
      w = 120;
      h = text_h + 10;
      break;
    case COGITO_SLIDER:
      w = 160;
      h = 24;
      break;
    case COGITO_TABS: {
      int size = cogito_node_font_size(n);
      int th = cogito_text_height_size(size);
      int pad_x = 12;
      int pad_y = 8;
      int gap = 6;
      int tw = 0;
      if (n->tab_len == 0) {
        w = 160;
        h = th + pad_y * 2;
        break;
      }
      for (size_t i = 0; i < n->tab_len; i++) {
        const char* t = n->tab_labels[i] ? n->tab_labels[i]->data : "";
        int lw = cogito_text_width_size(t, size);
        tw += lw + pad_x * 2;
        if (i + 1 < n->tab_len) tw += gap;
      }
      w = tw;
      h = th + pad_y * 2;
      break;
    }
    case COGITO_SEGMENTED:
      w = 200;
      h = text_h + 12;
      break;
    case COGITO_VIEWSWITCHER: {
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
    case COGITO_PROGRESS:
      w = 160;
      h = 10;
      break;
    case COGITO_TREEVIEW:
      w = 240;
      h = 200;
      break;
    case COGITO_COLORPICKER:
      w = 240;
      h = 200;
      break;
    case COGITO_TOASTS: {
      if (n->len == 0) { w = 0; h = 0; break; }
      int max_w = 0;
      int sum_h = 0;
      int gap = 6;
      for (size_t i = 0; i < n->len; i++) {
        int iw = 0;
        int ih = 0;
        cogito_intrinsic_size(n->children[i], &iw, &ih);
        int tw = iw + n->children[i]->margin_left + n->children[i]->margin_right;
        int th = ih + n->children[i]->margin_top + n->children[i]->margin_bottom;
        if (tw > max_w) max_w = tw;
        sum_h += th;
        if (i + 1 < n->len) sum_h += gap;
      }
      w = max_w;
      h = sum_h;
      break;
    }
    case COGITO_TOOLBAR:
      w = 0;
      h = text_h + 20;
      break;
    case COGITO_TOAST: {
      int size = cogito_node_font_size(n);
      int th = cogito_text_height_size(size);
      int pad_x = 12;
      int pad_y = 8;
      const char* t = n->text ? n->text->data : "";
      int tw = cogito_text_width_size(t, size);
      CogitoStyle s = cogito_theme_resolve_node(n);
      int icon = s.has_icon_size ? s.icon_size : 12;
      int close_box = icon + 8;
      w = tw + pad_x * 2 + close_box;
      h = th + pad_y * 2;
      break;
    }
    case COGITO_TOOLTIP:
      w = 0;
      h = 0;
      break;
    case COGITO_FIXED: {
      int max_w = 0;
      int max_h = 0;
      for (size_t i = 0; i < n->len; i++) {
        CogitoNode* c = n->children[i];
        int cw = 0;
        int ch = 0;
        cogito_intrinsic_size(c, &cw, &ch);
        int cx = c->fixed_pos_set ? c->fixed_x : 0;
        int cy = c->fixed_pos_set ? c->fixed_y : 0;
        int right = cx + cw + c->margin_left + c->margin_right;
        int bottom = cy + ch + c->margin_top + c->margin_bottom;
        if (right > max_w) max_w = right;
        if (bottom > max_h) max_h = bottom;
      }
      w = max_w + n->padding_left + n->padding_right;
      h = max_h + n->padding_top + n->padding_bottom;
      break;
    }
    case COGITO_SCROLLER:
      if (n->len > 0) {
        int cw = 0;
        int ch = 0;
        cogito_intrinsic_size(n->children[0], &cw, &ch);
        w = cw;
        h = ch;
      }
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
    int avail_w = n->w - n->padding_left - n->padding_right;
    if (avail_w < 0) avail_w = 0;
    if (c->kind == COGITO_LABEL && c->text_wrap) {
      int target_w = avail_w - c->margin_left - c->margin_right;
      if (target_w < 0) target_w = 0;
      cw = target_w;
      ch = cogito_text_wrap_height(c->text ? c->text->data : "", cogito_node_font_size(c), target_w);
      if (ch <= 0) ch = cogito_text_height_size(cogito_node_font_size(c));
    }
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
    int col_use = col;
    int row_use = row;
    if (c->kind == COGITO_TOASTS) {
      col_use = 2;
      row_use = 2;
    }
    if (col_use == 1) ox = (avail_w - cw) / 2;
    else if (col_use == 2) ox = avail_w - cw;
    if (row_use == 1) oy = (avail_h - ch) / 2;
    else if (row_use == 2) oy = avail_h - ch;
    if (ox < 0) ox = 0;
    if (oy < 0) oy = 0;
    cogito_layout_node(c, inner_x + c->margin_left + ox, inner_y + c->margin_top + oy, cw, ch);
  }
}

static CogitoNode* cogito_view_switcher_active_child(CogitoNode* n) {
  if (!n || n->len == 0) return NULL;
  if (n->view_active_id) {
    for (size_t i = 0; i < n->len; i++) {
      CogitoNode* c = n->children[i];
      if (c->view_id && cogito_str_eq(c->view_id, n->view_active_id)) return c;
    }
  }
  return n->children[0];
}

static void cogito_layout_view_switcher(CogitoNode* n, int x, int y) {
  int inner_x = x + n->padding_left;
  int inner_y = y + n->padding_top;
  int inner_w = n->w - n->padding_left - n->padding_right;
  int inner_h = n->h - n->padding_top - n->padding_bottom;
  if (inner_w < 0) inner_w = 0;
  if (inner_h < 0) inner_h = 0;
  CogitoNode* active = cogito_view_switcher_active_child(n);
  int align = n->align;
  if (align < 0 || align > 8) align = 0;
  int row = align / 3;
  int col = align % 3;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    if (c != active) {
      cogito_layout_node(c, inner_x, inner_y, 0, 0);
      continue;
    }
    int cw = 0;
    int ch = 0;
    int ox = 0;
    int oy = 0;
    int avail_w = inner_w - c->margin_left - c->margin_right;
    int avail_h = inner_h - c->margin_top - c->margin_bottom;
    if (avail_w < 0) avail_w = 0;
    if (avail_h < 0) avail_h = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    if (col == 1) ox = (avail_w - cw) / 2;
    else if (col == 2) ox = avail_w - cw;
    if (row == 1) oy = (avail_h - ch) / 2;
    else if (row == 2) oy = avail_h - ch;
    if (ox < 0) ox = 0;
    if (oy < 0) oy = 0;
    cogito_layout_node(c, inner_x + c->margin_left + ox, inner_y + c->margin_top + oy, cw, ch);
  }
}

static void cogito_layout_toasts(CogitoNode* n, int x, int y) {
  int inner_x = x + n->padding_left;
  int inner_y = y + n->padding_top;
  int inner_w = n->w - n->padding_left - n->padding_right;
  int inner_h = n->h - n->padding_top - n->padding_bottom;
  if (inner_w < 0) inner_w = 0;
  if (inner_h < 0) inner_h = 0;
  int gap = 6;
  int cy = inner_y + inner_h;
  for (size_t idx = n->len; idx-- > 0;) {
    CogitoNode* c = n->children[idx];
    int cw = 0;
    int ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    int cx = inner_x + inner_w - cw - c->margin_right;
    int cy_child = cy - ch - c->margin_bottom;
    if (cx < inner_x) cx = inner_x;
    if (cy_child < inner_y) cy_child = inner_y;
    cogito_layout_node(c, cx + c->margin_left, cy_child + c->margin_top, cw, ch);
    cy = cy_child - c->margin_top - gap;
  }
}

static void cogito_layout_fixed(CogitoNode* n, int x, int y) {
  int base_x = x + n->padding_left;
  int base_y = y + n->padding_top;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    int cw = 0;
    int ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    int cx = base_x + (c->fixed_pos_set ? c->fixed_x : 0);
    int cy = base_y + (c->fixed_pos_set ? c->fixed_y : 0);
    cogito_layout_node(c, cx + c->margin_left, cy + c->margin_top, cw, ch);
  }
}

static void cogito_layout_scroller(CogitoNode* n, int x, int y) {
  int inner_x = x + n->padding_left;
  int inner_y = y + n->padding_top;
  int inner_w = n->w - n->padding_left - n->padding_right;
  int inner_h = n->h - n->padding_top - n->padding_bottom;
  if (inner_w < 0) inner_w = 0;
  if (inner_h < 0) inner_h = 0;
  if (n->len == 0) return;
  CogitoNode* c = n->children[0];
  int cw = 0;
  int ch = 0;
  cogito_intrinsic_size(c, &cw, &ch);
  if (!n->scroll_horz) {
    cw = inner_w;
  } else if (cw < inner_w) {
    cw = inner_w;
  }
  if (!n->scroll_vert) {
    ch = inner_h;
  } else if (ch < inner_h) {
    ch = inner_h;
  }
  n->scroll_content_w = cw;
  n->scroll_content_h = ch;
  int max_x = cw - inner_w;
  int max_y = ch - inner_h;
  if (max_x < 0) max_x = 0;
  if (max_y < 0) max_y = 0;
  if (n->scroll_x < 0) n->scroll_x = 0;
  if (n->scroll_y < 0) n->scroll_y = 0;
  if (n->scroll_x > max_x) n->scroll_x = max_x;
  if (n->scroll_y > max_y) n->scroll_y = max_y;
  cogito_layout_node(c, inner_x - n->scroll_x, inner_y - n->scroll_y, cw, ch);
  for (size_t i = 1; i < n->len; i++) {
    cogito_layout_node(n->children[i], inner_x, inner_y, 0, 0);
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
    if (c->kind == COGITO_LABEL && c->text_wrap) {
      int target_w = cw - c->margin_left - c->margin_right;
      if (target_w < 0) target_w = 0;
      cw = target_w;
      ch = cogito_text_wrap_height(c->text ? c->text->data : "", cogito_node_font_size(c), target_w);
      if (ch <= 0) ch = cogito_text_height_size(cogito_node_font_size(c));
    }
    cogito_layout_node(c, x + n->padding_left + pad, cy, cw, ch);
    cy += ch + pad;
  }
}

static void cogito_layout_grid(CogitoNode* n, int x, int y) {
  int cols = n->grid_cols > 0 ? n->grid_cols : 1;
  int gap_x = n->grid_gap_x;
  int gap_y = n->grid_gap_y;
  int inner_w = n->w - n->padding_left - n->padding_right;
  if (inner_w < 0) inner_w = 0;
  int cell_w = cols > 0 ? (inner_w - (cols - 1) * gap_x) / cols : 0;
  if (cell_w < 0) cell_w = 0;
  size_t count = n->len;
  if (count == 0) return;
  int *rows = (int*)calloc(count, sizeof(int));
  int *cols_out = (int*)calloc(count, sizeof(int));
  int *span_x = (int*)calloc(count, sizeof(int));
  int *span_y = (int*)calloc(count, sizeof(int));
  if (!rows || !cols_out || !span_x || !span_y) {
    free(rows); free(cols_out); free(span_x); free(span_y);
    return;
  }
  int row_cap = 4;
  int row_count = 0;
  bool *occ = (bool*)calloc((size_t)row_cap * (size_t)cols, sizeof(bool));
  if (!occ) {
    free(rows); free(cols_out); free(span_x); free(span_y);
    return;
  }
  for (size_t i = 0; i < count; i++) {
    CogitoNode* c = n->children[i];
    int cs = c->grid_span_x > 0 ? c->grid_span_x : 1;
    int rs = c->grid_span_y > 0 ? c->grid_span_y : 1;
    if (cs > cols) cs = cols;
    int r = 0;
    int ccol = 0;
    bool placed = false;
    while (!placed) {
      if (r >= row_cap) {
        int new_cap = row_cap * 2;
        bool *next = (bool*)realloc(occ, (size_t)new_cap * (size_t)cols * sizeof(bool));
        if (!next) break;
        memset(next + (size_t)row_cap * (size_t)cols, 0, (size_t)(new_cap - row_cap) * (size_t)cols * sizeof(bool));
        occ = next;
        row_cap = new_cap;
      }
      for (ccol = 0; ccol + cs <= cols; ccol++) {
        bool ok = true;
        for (int rr = 0; rr < rs; rr++) {
          int row_idx = r + rr;
          if (row_idx >= row_cap) { ok = false; break; }
          for (int cc = 0; cc < cs; cc++) {
            if (occ[row_idx * cols + (ccol + cc)]) { ok = false; break; }
          }
          if (!ok) break;
        }
        if (ok) {
          for (int rr = 0; rr < rs; rr++) {
            int row_idx = r + rr;
            if (row_idx >= row_count) row_count = row_idx + 1;
            for (int cc = 0; cc < cs; cc++) {
              occ[row_idx * cols + (ccol + cc)] = true;
            }
          }
          placed = true;
          break;
        }
      }
      if (!placed) r++;
    }
    rows[i] = r;
    cols_out[i] = ccol;
    span_x[i] = cs;
    span_y[i] = rs;
  }
  int *row_heights = (int*)calloc((size_t)row_count, sizeof(int));
  if (!row_heights) {
    free(rows); free(cols_out); free(span_x); free(span_y); free(occ);
    return;
  }
  for (size_t i = 0; i < count; i++) {
    CogitoNode* c = n->children[i];
    int ch = 0;
    cogito_intrinsic_size(c, NULL, &ch);
    int rs = span_y[i] > 0 ? span_y[i] : 1;
    int per_row = rs > 0 ? (ch + rs - 1) / rs : ch;
    for (int rr = 0; rr < rs; rr++) {
      int row_idx = rows[i] + rr;
      if (row_idx >= row_count) break;
      if (per_row > row_heights[row_idx]) row_heights[row_idx] = per_row;
    }
  }
  int *row_y = (int*)calloc((size_t)row_count, sizeof(int));
  if (!row_y) {
    free(rows); free(cols_out); free(span_x); free(span_y); free(occ); free(row_heights);
    return;
  }
  int cy = y + n->padding_top;
  for (int r = 0; r < row_count; r++) {
    row_y[r] = cy;
    cy += row_heights[r] + gap_y;
  }
  int base_x = x + n->padding_left;
  for (size_t i = 0; i < count; i++) {
    CogitoNode* c = n->children[i];
    int r = rows[i];
    int ccol = cols_out[i];
    int cs = span_x[i] > 0 ? span_x[i] : 1;
    int rs = span_y[i] > 0 ? span_y[i] : 1;
    int cell_x = base_x + ccol * (cell_w + gap_x);
    int cell_y = row_y[r];
    int cell_w_span = cell_w * cs + gap_x * (cs - 1);
    int cell_h_span = 0;
    for (int rr = 0; rr < rs; rr++) {
      int row_idx = r + rr;
      if (row_idx >= row_count) break;
      cell_h_span += row_heights[row_idx];
      if (rr + 1 < rs) cell_h_span += gap_y;
    }
    int cw = 0;
    int ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    if (cw > cell_w_span) cw = cell_w_span;
    if (ch > cell_h_span) ch = cell_h_span;
    int ax = c->grid_align_x >= 0 ? c->grid_align_x : n->grid_align_x;
    int ay = c->grid_align_y >= 0 ? c->grid_align_y : n->grid_align_y;
    int ox = 0;
    int oy = 0;
    if (ax == 1) ox = (cell_w_span - cw) / 2;
    else if (ax == 2) ox = cell_w_span - cw;
    if (ay == 1) oy = (cell_h_span - ch) / 2;
    else if (ay == 2) oy = cell_h_span - ch;
    if (ox < 0) ox = 0;
    if (oy < 0) oy = 0;
    cogito_layout_node(c, cell_x + ox, cell_y + oy, cw, ch);
  }
  free(rows);
  free(cols_out);
  free(span_x);
  free(span_y);
  free(occ);
  free(row_heights);
  free(row_y);
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
    case COGITO_SEARCHFIELD:
    case COGITO_DATEPICKER:
    case COGITO_STEPPER:
    case COGITO_SEGMENTED:
    case COGITO_TREEVIEW:
    case COGITO_COLORPICKER:
    case COGITO_TOOLTIP:
      break;
    case COGITO_TOOLBAR:
      cogito_layout_hstack(n, x, y);
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
    case COGITO_VIEWSWITCHER:
      cogito_layout_view_switcher(n, x, y);
      break;
    case COGITO_FIXED:
      cogito_layout_fixed(n, x, y);
      break;
    case COGITO_SCROLLER:
      cogito_layout_scroller(n, x, y);
      break;
    case COGITO_LIST:
      cogito_layout_list(n, x, y);
      break;
    case COGITO_GRID:
      cogito_layout_grid(n, x, y);
      break;
    case COGITO_TOASTS:
      cogito_layout_toasts(n, x, y);
      break;
    case COGITO_WINDOW:
    case COGITO_BUTTON:
    case COGITO_ICONBTN:
    case COGITO_IMAGE:
    case COGITO_LABEL:
    case COGITO_CHECKBOX:
    case COGITO_SWITCH:
    case COGITO_TEXTFIELD:
    case COGITO_TEXTVIEW:
    case COGITO_DROPDOWN:
    case COGITO_SLIDER:
    case COGITO_TABS:
    case COGITO_PROGRESS:
    case COGITO_TOAST:
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
  int toolbar_h = 0;
  CogitoNode* appbar = NULL;
  CogitoNode* toolbar = NULL;
  for (size_t i = 0; i < win->len; i++) {
    if (win->children[i]->kind == COGITO_APPBAR) {
      appbar = win->children[i];
      break;
    }
  }
  for (size_t i = 0; i < win->len; i++) {
    if (win->children[i]->kind == COGITO_TOOLBAR) {
      toolbar = win->children[i];
      break;
    }
  }
  if (appbar) {
    cogito_intrinsic_size(appbar, NULL, &appbar_h);
    cogito_layout_node(appbar, 0, 0, win->w, appbar_h);
    cy += appbar_h + 6;
  }
  if (toolbar) {
    cogito_intrinsic_size(toolbar, NULL, &toolbar_h);
    int ty = win->h - toolbar_h;
    if (ty < 0) ty = 0;
    cogito_layout_node(toolbar, 0, ty, win->w, toolbar_h);
  }
  int appbar_gap = appbar ? 6 : 0;
  int toolbar_gap = toolbar ? 6 : 0;
  for (size_t i = 0; i < win->len; i++) {
    CogitoNode* c = win->children[i];
    if (c->kind == COGITO_APPBAR || c->kind == COGITO_TOOLBAR) continue;
    int cw = 0;
    int ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    int cw_total = cw + c->margin_left + c->margin_right;
    int ch_total = ch + c->margin_top + c->margin_bottom;
    if (c->kind == COGITO_VSTACK || c->kind == COGITO_HSTACK) {
      int avail_w = win->w - win->padding_left - win->padding_right;
      int avail_h = win->h - win->padding_top - win->padding_bottom;
      if (appbar) avail_h -= appbar_h + appbar_gap;
      if (toolbar) avail_h -= toolbar_h + toolbar_gap;
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
  int toolbar_h = 0;
  for (size_t i = 0; i < win->len; i++) {
    CogitoNode* c = win->children[i];
    if (c->kind == COGITO_APPBAR) {
      cogito_intrinsic_size(c, NULL, &appbar_h);
      int row_w = pad + win->padding_left + win->w;
      if (row_w > max_w) max_w = row_w;
      break;
    }
  }
  for (size_t i = 0; i < win->len; i++) {
    CogitoNode* c = win->children[i];
    if (c->kind == COGITO_TOOLBAR) {
      cogito_intrinsic_size(c, NULL, &toolbar_h);
      int row_w = pad + win->padding_left + win->w;
      if (row_w > max_w) max_w = row_w;
      break;
    }
  }
  if (appbar_h > 0) cy += appbar_h + 6;
  if (toolbar_h > 0) cy += toolbar_h + 6;
  for (size_t i = 0; i < win->len; i++) {
    CogitoNode* c = win->children[i];
    if (c->kind == COGITO_APPBAR || c->kind == COGITO_TOOLBAR) continue;
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

static void cogito_invoke_change(CogitoNode* n);

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
  if (n->kind == COGITO_DROPDOWN) {
    n->selected = (int)idx;
    cogito_invoke_change(n);
    return;
  }
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

static void cogito_toast_close_rect(CogitoNode* n, int* out_x, int* out_y, int* out_w, int* out_h) {
  if (!n || n->kind != COGITO_TOAST) return;
  CogitoStyle s = cogito_theme_resolve_node(n);
  int icon = s.has_icon_size ? s.icon_size : 12;
  if (icon < 8) icon = 8;
  int box = icon + 8;
  int pad_x = 12;
  int x = n->x + n->w - pad_x - box;
  int y = n->y + (n->h - box) / 2;
  if (out_x) *out_x = x;
  if (out_y) *out_y = y;
  if (out_w) *out_w = box;
  if (out_h) *out_h = box;
}

static int cogito_tabs_hit_index(CogitoNode* n, int x, int y) {
  if (!n || n->kind != COGITO_TABS) return -1;
  if (!cogito_hit_node(n, x, y)) return -1;
  int size = cogito_node_font_size(n);
  int th = cogito_text_height_size(size);
  int pad_x = 12;
  int pad_y = 8;
  int gap = 6;
  int tab_h = th + pad_y * 2;
  int ty = n->y + (n->h - tab_h) / 2;
  if (y < ty || y > ty + tab_h) return -1;
  int cx = n->x + n->padding_left;
  for (size_t i = 0; i < n->tab_len; i++) {
    const char* t = n->tab_labels[i] ? n->tab_labels[i]->data : "";
    int tw = cogito_text_width_size(t, size);
    int tab_w = tw + pad_x * 2;
    if (x >= cx && x <= cx + tab_w) return (int)i;
    cx += tab_w + gap;
  }
  return -1;
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
  if (n->kind == COGITO_VIEWSWITCHER) {
    CogitoNode* active = cogito_view_switcher_active_child(n);
    if (active) {
      CogitoNode* hit = cogito_find_list_grid_hit(active, x, y, out_idx);
      if (hit) return hit;
    }
  } else {
    for (size_t i = 0; i < n->len; i++) {
      CogitoNode* hit = cogito_find_list_grid_hit(n->children[i], x, y, out_idx);
      if (hit) return hit;
    }
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
  if (n->kind == COGITO_SCROLLER && !cogito_hit_rect(x, y, n->x, n->y, n->w, n->h)) return NULL;
  if (n->kind == COGITO_VIEWSWITCHER) {
    CogitoNode* active = cogito_view_switcher_active_child(n);
    if (active) {
      CogitoNode* child = cogito_find_clickable(active, x, y);
      if (child) return child;
    }
  } else {
    for (size_t i = 0; i < n->len; i++) {
      CogitoNode* child = cogito_find_clickable(n->children[i], x, y);
      if (child) return child;
    }
  }
  if (n->kind == COGITO_BUTTON || n->kind == COGITO_ICONBTN || n->kind == COGITO_CHECKBOX ||
      n->kind == COGITO_SWITCH || n->kind == COGITO_LIST || n->kind == COGITO_GRID ||
      n->kind == COGITO_TEXTFIELD || n->kind == COGITO_TEXTVIEW ||
      n->kind == COGITO_DROPDOWN || n->kind == COGITO_SLIDER ||
      n->kind == COGITO_TABS || n->kind == COGITO_TOAST ||
      n->kind == COGITO_DATEPICKER || n->kind == COGITO_COLORPICKER) {
    if (cogito_hit_node(n, x, y)) return n;
  }
  return NULL;
}

static CogitoNode* cogito_find_scroller(CogitoNode* n, int x, int y) {
  if (!n) return NULL;
  if (n->kind == COGITO_SCROLLER && !cogito_hit_rect(x, y, n->x, n->y, n->w, n->h)) return NULL;
  if (n->kind == COGITO_VIEWSWITCHER) {
    CogitoNode* active = cogito_view_switcher_active_child(n);
    if (active) {
      CogitoNode* child = cogito_find_scroller(active, x, y);
      if (child) return child;
    }
  } else {
    for (size_t i = 0; i < n->len; i++) {
      CogitoNode* child = cogito_find_scroller(n->children[i], x, y);
      if (child) return child;
    }
  }
  if (n->kind == COGITO_SCROLLER && cogito_hit_rect(x, y, n->x, n->y, n->w, n->h)) return n;
  return NULL;
}

static CogitoNode* cogito_find_text_input(CogitoNode* n, int x, int y) {
  if (!n) return NULL;
  if (n->kind == COGITO_VIEWSWITCHER) {
    CogitoNode* active = cogito_view_switcher_active_child(n);
    if (active) {
      CogitoNode* hit = cogito_find_text_input(active, x, y);
      if (hit) return hit;
    }
  } else {
    for (size_t i = 0; i < n->len; i++) {
      CogitoNode* hit = cogito_find_text_input(n->children[i], x, y);
      if (hit) return hit;
    }
  }
  if ((n->kind == COGITO_TEXTFIELD || n->kind == COGITO_TEXTVIEW) && cogito_hit_node(n, x, y)) return n;
  if (n->kind == COGITO_COLORPICKER) {
    int hx = 0, hy = 0, hw = 0, hh = 0;
    cogito_colorpicker_hex_rect(n, &hx, &hy, &hw, &hh);
    if (cogito_hit_rect(x, y, hx, hy, hw, hh)) return n;
  }
  return NULL;
}

static CogitoNode* cogito_find_tooltip_node(CogitoNode* n, int x, int y) {
  if (!n) return NULL;
  if (n->kind == COGITO_SCROLLER && !cogito_hit_rect(x, y, n->x, n->y, n->w, n->h)) return NULL;
  if (n->kind == COGITO_VIEWSWITCHER) {
    CogitoNode* active = cogito_view_switcher_active_child(n);
    if (active) {
      CogitoNode* hit = cogito_find_tooltip_node(active, x, y);
      if (hit) return hit;
    }
  } else {
    for (size_t i = 0; i < n->len; i++) {
      CogitoNode* hit = cogito_find_tooltip_node(n->children[i], x, y);
      if (hit) return hit;
    }
  }
  if (n->tooltip && n->tooltip->data && n->tooltip->data[0] && cogito_hit_node(n, x, y) && !n->disabled) {
    return n;
  }
  return NULL;
}

static bool cogito_node_focusable(const CogitoNode* n) {
  if (!n || n->disabled) return false;
  switch (n->kind) {
    case COGITO_TEXTFIELD:
    case COGITO_TEXTVIEW:
    case COGITO_DROPDOWN:
    case COGITO_SLIDER:
    case COGITO_BUTTON:
    case COGITO_ICONBTN:
    case COGITO_CHECKBOX:
    case COGITO_SWITCH:
    case COGITO_COLORPICKER:
    case COGITO_DATEPICKER:
      return true;
    default:
      return false;
  }
}

static CogitoNode* cogito_focus_next(CogitoNode* n, CogitoNode* current, bool* found_current) {
  if (!n) return NULL;
  if (n == current) *found_current = true;
  else if (*found_current && cogito_node_focusable(n)) return n;
  if (n->kind == COGITO_VIEWSWITCHER) {
    CogitoNode* active = cogito_view_switcher_active_child(n);
    if (active) {
      CogitoNode* hit = cogito_focus_next(active, current, found_current);
      if (hit) return hit;
    }
  } else {
    for (size_t i = 0; i < n->len; i++) {
      CogitoNode* hit = cogito_focus_next(n->children[i], current, found_current);
      if (hit) return hit;
    }
  }
  return NULL;
}

static CogitoNode* cogito_focus_first(CogitoNode* n) {
  if (!n) return NULL;
  if (cogito_node_focusable(n)) return n;
  if (n->kind == COGITO_VIEWSWITCHER) {
    CogitoNode* active = cogito_view_switcher_active_child(n);
    if (active) return cogito_focus_first(active);
  } else {
    for (size_t i = 0; i < n->len; i++) {
      CogitoNode* hit = cogito_focus_first(n->children[i]);
      if (hit) return hit;
    }
  }
  return NULL;
}

static void cogito_handle_list_select(CogitoNode* list, int idx, bool activate) {
  list->selected = idx;
  cogito_invoke_select(list, idx, activate);
}

static void cogito_container_remove_child(CogitoNode* parent, CogitoNode* child) {
  if (!parent || !child || !parent->children) return;
  size_t idx = parent->len;
  for (size_t i = 0; i < parent->len; i++) {
    if (parent->children[i] == child) { idx = i; break; }
  }
  if (idx >= parent->len) return;
  ergo_release_val(EV_OBJ(parent->children[idx]));
  for (size_t i = idx + 1; i < parent->len; i++) {
    parent->children[i - 1] = parent->children[i];
  }
  parent->len--;
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
  win->auto_size = true;
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

static CogitoNode* cogito_textfield_new_obj(ErgoStr* text) {
  CogitoNode* tf = cogito_node_new(COGITO_TEXTFIELD);
  cogito_node_set_text(tf, text);
  return tf;
}

static CogitoNode* cogito_textview_new_obj(ErgoStr* text) {
  CogitoNode* tv = cogito_node_new(COGITO_TEXTVIEW);
  cogito_node_set_text(tv, text);
  return tv;
}

static CogitoNode* cogito_searchfield_new_obj(ErgoStr* text) {
  CogitoNode* sf = cogito_node_new(COGITO_SEARCHFIELD);
  cogito_node_set_text(sf, text);
  return sf;
}

static CogitoNode* cogito_dropdown_new_obj(void) {
  CogitoNode* dd = cogito_node_new(COGITO_DROPDOWN);
  dd->selected = -1;
  return dd;
}

static CogitoNode* cogito_datepicker_new_obj(void) {
  CogitoNode* dp = cogito_node_new(COGITO_DATEPICKER);
  cogito_datepicker_ensure_date(dp);
  return dp;
}

static CogitoNode* cogito_stepper_new_obj(double min, double max, double value, double step) {
  CogitoNode* st = cogito_node_new(COGITO_STEPPER);
  st->stepper_min = min;
  st->stepper_max = max;
  if (st->stepper_max < st->stepper_min) {
    double tmp = st->stepper_min;
    st->stepper_min = st->stepper_max;
    st->stepper_max = tmp;
  }
  if (value < st->stepper_min) value = st->stepper_min;
  if (value > st->stepper_max) value = st->stepper_max;
  st->stepper_value = value;
  st->stepper_step = step;
  return st;
}

static CogitoNode* cogito_slider_new_obj(double min, double max, double value) {
  CogitoNode* sl = cogito_node_new(COGITO_SLIDER);
  sl->slider_min = min;
  sl->slider_max = max;
  if (sl->slider_max < sl->slider_min) {
    double tmp = sl->slider_min;
    sl->slider_min = sl->slider_max;
    sl->slider_max = tmp;
  }
  if (value < sl->slider_min) value = sl->slider_min;
  if (value > sl->slider_max) value = sl->slider_max;
  sl->slider_value = value;
  return sl;
}

static CogitoNode* cogito_tabs_new_obj(void) {
  CogitoNode* t = cogito_node_new(COGITO_TABS);
  t->selected = 0;
  return t;
}

static CogitoNode* cogito_segmented_new_obj(void) {
  CogitoNode* s = cogito_node_new(COGITO_SEGMENTED);
  s->selected = 0;
  return s;
}

static CogitoNode* cogito_view_switcher_new_obj(void) {
  return cogito_node_new(COGITO_VIEWSWITCHER);
}

static CogitoNode* cogito_progress_new_obj(double value) {
  CogitoNode* p = cogito_node_new(COGITO_PROGRESS);
  if (value < 0.0) value = 0.0;
  if (value > 1.0) value = 1.0;
  p->progress_value = value;
  return p;
}

static CogitoNode* cogito_treeview_new_obj(void) {
  return cogito_node_new(COGITO_TREEVIEW);
}

static CogitoNode* cogito_colorpicker_new_obj(void) {
  CogitoNode* cp = cogito_node_new(COGITO_COLORPICKER);
  cp->color_h = 200.0;
  cp->color_c = 20.0;
  cp->color_t = 90.0;
  cogito_colorpicker_sync_hex(cp);
  return cp;
}

static CogitoNode* cogito_toasts_new_obj(void) {
  return cogito_node_new(COGITO_TOASTS);
}

static CogitoNode* cogito_toast_new_obj(ErgoStr* text) {
  CogitoNode* t = cogito_node_new(COGITO_TOAST);
  t->text = text;
  if (t->text) ergo_retain_val(EV_STR(t->text));
  return t;
}

static CogitoNode* cogito_toolbar_new_obj(void) {
  return cogito_node_new(COGITO_TOOLBAR);
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

static CogitoNode* cogito_fixed_new_obj(void) {
  return cogito_node_new(COGITO_FIXED);
}

static CogitoNode* cogito_scroller_new_obj(void) {
  return cogito_node_new(COGITO_SCROLLER);
}

static CogitoNode* cogito_grid_new_obj(int cols) {
  CogitoNode* g = cogito_node_new(COGITO_GRID);
  g->grid_cols = cols > 0 ? cols : 1;
  g->grid_align_x = 0;
  g->grid_align_y = 0;
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

static ErgoVal cogito_textfield_new(ErgoVal text) {
  ErgoStr* ts = stdr_to_string(text);
  CogitoNode* tf = cogito_textfield_new_obj(ts);
  return EV_OBJ(tf);
}

static ErgoVal cogito_textview_new(ErgoVal text) {
  ErgoStr* ts = stdr_to_string(text);
  CogitoNode* tv = cogito_textview_new_obj(ts);
  return EV_OBJ(tv);
}

static ErgoVal cogito_searchfield_new(ErgoVal text) {
  ErgoStr* ts = stdr_to_string(text);
  CogitoNode* sf = cogito_searchfield_new_obj(ts);
  return EV_OBJ(sf);
}

static ErgoVal cogito_dropdown_new(void) {
  return EV_OBJ(cogito_dropdown_new_obj());
}

static ErgoVal cogito_datepicker_new(void) {
  return EV_OBJ(cogito_datepicker_new_obj());
}

static ErgoVal cogito_stepper_new(ErgoVal minv, ErgoVal maxv, ErgoVal valv, ErgoVal stepv) {
  double min = ergo_as_float(minv);
  double max = ergo_as_float(maxv);
  double val = ergo_as_float(valv);
  double step = ergo_as_float(stepv);
  return EV_OBJ(cogito_stepper_new_obj(min, max, val, step));
}

static ErgoVal cogito_slider_new(ErgoVal minv, ErgoVal maxv, ErgoVal valv) {
  double min = (double)ergo_as_int(minv);
  double max = (double)ergo_as_int(maxv);
  double val = (double)ergo_as_int(valv);
  return EV_OBJ(cogito_slider_new_obj(min, max, val));
}

static ErgoVal cogito_tabs_new(void) {
  return EV_OBJ(cogito_tabs_new_obj());
}

static ErgoVal cogito_segmented_new(void) {
  return EV_OBJ(cogito_segmented_new_obj());
}

static ErgoVal cogito_view_switcher_new(void) {
  return EV_OBJ(cogito_view_switcher_new_obj());
}

static ErgoVal cogito_progress_new(ErgoVal valv) {
  double v = ergo_as_float(valv);
  return EV_OBJ(cogito_progress_new_obj(v));
}

static ErgoVal cogito_treeview_new(void) {
  return EV_OBJ(cogito_treeview_new_obj());
}

static ErgoVal cogito_colorpicker_new(void) {
  return EV_OBJ(cogito_colorpicker_new_obj());
}

static ErgoVal cogito_toolbar_new(void) {
  return EV_OBJ(cogito_toolbar_new_obj());
}

static ErgoVal cogito_toasts_new(void) {
  return EV_OBJ(cogito_toasts_new_obj());
}

static ErgoVal cogito_toast_new(ErgoVal textv) {
  ErgoStr* s = stdr_to_string(textv);
  return EV_OBJ(cogito_toast_new_obj(s));
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

static ErgoVal cogito_fixed_new(void) {
  return EV_OBJ(cogito_fixed_new_obj());
}

static ErgoVal cogito_scroller_new(void) {
  return EV_OBJ(cogito_scroller_new_obj());
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

static void cogito_fixed_set_pos(ErgoVal fixedv, ErgoVal childv, ErgoVal xv, ErgoVal yv) {
  if (fixedv.tag != EVT_OBJ) ergo_trap("cogito.fixed_set_pos expects fixed");
  if (childv.tag != EVT_OBJ) ergo_trap("cogito.fixed_set_pos expects child");
  CogitoNode* child = (CogitoNode*)childv.as.p;
  child->fixed_x = (int)ergo_as_int(xv);
  child->fixed_y = (int)ergo_as_int(yv);
  child->fixed_pos_set = true;
  cogito_window_relayout(cogito_node_window(child));
}

static void cogito_scroller_set_axes(ErgoVal scv, ErgoVal horzv, ErgoVal vertv) {
  if (scv.tag != EVT_OBJ) ergo_trap("cogito.scroller_set_axes expects scroller");
  CogitoNode* sc = (CogitoNode*)scv.as.p;
  sc->scroll_horz = ergo_as_bool(horzv);
  sc->scroll_vert = ergo_as_bool(vertv);
  cogito_window_relayout(cogito_node_window(sc));
}

static void cogito_grid_set_gap(ErgoVal gridv, ErgoVal xv, ErgoVal yv) {
  if (gridv.tag != EVT_OBJ) ergo_trap("cogito.grid_set_gap expects grid");
  CogitoNode* g = (CogitoNode*)gridv.as.p;
  g->grid_gap_x = (int)ergo_as_int(xv);
  g->grid_gap_y = (int)ergo_as_int(yv);
  cogito_window_relayout(cogito_node_window(g));
}

static void cogito_grid_set_span(ErgoVal childv, ErgoVal colsv, ErgoVal rowsv) {
  if (childv.tag != EVT_OBJ) ergo_trap("cogito.grid_set_span expects child");
  CogitoNode* c = (CogitoNode*)childv.as.p;
  c->grid_span_x = (int)ergo_as_int(colsv);
  c->grid_span_y = (int)ergo_as_int(rowsv);
  if (c->grid_span_x < 1) c->grid_span_x = 1;
  if (c->grid_span_y < 1) c->grid_span_y = 1;
  cogito_window_relayout(cogito_node_window(c));
}

static void cogito_grid_set_align(ErgoVal childv, ErgoVal halignv, ErgoVal valignv) {
  if (childv.tag != EVT_OBJ) ergo_trap("cogito.grid_set_align expects child");
  CogitoNode* c = (CogitoNode*)childv.as.p;
  c->grid_align_x = (int)ergo_as_int(halignv);
  c->grid_align_y = (int)ergo_as_int(valignv);
  cogito_window_relayout(cogito_node_window(c));
}

static void cogito_label_set_wrap(ErgoVal labelv, ErgoVal onv) {
  if (labelv.tag != EVT_OBJ) ergo_trap("cogito.label_set_wrap expects label");
  CogitoNode* n = (CogitoNode*)labelv.as.p;
  n->text_wrap = ergo_as_bool(onv);
  cogito_window_relayout(cogito_node_window(n));
}

static void cogito_label_set_ellipsis(ErgoVal labelv, ErgoVal onv) {
  if (labelv.tag != EVT_OBJ) ergo_trap("cogito.label_set_ellipsis expects label");
  CogitoNode* n = (CogitoNode*)labelv.as.p;
  n->text_ellipsis = ergo_as_bool(onv);
  cogito_window_relayout(cogito_node_window(n));
}

static void cogito_label_set_align(ErgoVal labelv, ErgoVal alignv) {
  if (labelv.tag != EVT_OBJ) ergo_trap("cogito.label_set_align expects label");
  CogitoNode* n = (CogitoNode*)labelv.as.p;
  n->text_align = (int)ergo_as_int(alignv);
  if (n->text_align < 0) n->text_align = 0;
  if (n->text_align > 2) n->text_align = 2;
  cogito_window_relayout(cogito_node_window(n));
}

static void cogito_node_set_disabled(ErgoVal nodev, ErgoVal onv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.node_set_disabled expects object");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  n->disabled = ergo_as_bool(onv);
  if (n == cogito_focused && n->disabled) cogito_focused = NULL;
  cogito_window_relayout(cogito_node_window(n));
}

static void cogito_textfield_set_text(ErgoVal nodev, ErgoVal textv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.textfield_set_text expects textfield");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  ErgoStr* ts = stdr_to_string(textv);
  cogito_node_set_text(n, ts);
  cogito_window_relayout(cogito_node_window(n));
}

static ErgoVal cogito_textfield_get_text(ErgoVal nodev) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.textfield_get_text expects textfield");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  ErgoStr* s = n->text ? n->text : stdr_str_lit("");
  ergo_retain_val(EV_STR(s));
  return EV_STR(s);
}

static void cogito_textfield_on_change(ErgoVal nodev, ErgoVal handler) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.textfield_on_change expects textfield");
  if (handler.tag != EVT_FN) ergo_trap("cogito.textfield_on_change expects function");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->on_change) ergo_release_val(EV_FN(n->on_change));
  n->on_change = (ErgoFn*)handler.as.p;
  ergo_retain_val(EV_FN(n->on_change));
}

static void cogito_searchfield_set_text(ErgoVal nodev, ErgoVal textv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.searchfield_set_text expects searchfield");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->kind != COGITO_SEARCHFIELD) ergo_trap("cogito.searchfield_set_text expects searchfield");
  ErgoStr* ts = stdr_to_string(textv);
  cogito_node_set_text(n, ts);
  cogito_window_relayout(cogito_node_window(n));
}

static ErgoVal cogito_searchfield_get_text(ErgoVal nodev) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.searchfield_get_text expects searchfield");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->kind != COGITO_SEARCHFIELD) ergo_trap("cogito.searchfield_get_text expects searchfield");
  ErgoStr* s = n->text ? n->text : stdr_str_lit("");
  ergo_retain_val(EV_STR(s));
  return EV_STR(s);
}

static void cogito_searchfield_on_change(ErgoVal nodev, ErgoVal handler) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.searchfield_on_change expects searchfield");
  if (handler.tag != EVT_FN) ergo_trap("cogito.searchfield_on_change expects function");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->kind != COGITO_SEARCHFIELD) ergo_trap("cogito.searchfield_on_change expects searchfield");
  if (n->on_change) ergo_release_val(EV_FN(n->on_change));
  n->on_change = (ErgoFn*)handler.as.p;
  ergo_retain_val(EV_FN(n->on_change));
}

static void cogito_textview_set_text(ErgoVal nodev, ErgoVal textv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.textview_set_text expects textview");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  ErgoStr* ts = stdr_to_string(textv);
  cogito_node_set_text(n, ts);
  cogito_window_relayout(cogito_node_window(n));
}

static ErgoVal cogito_textview_get_text(ErgoVal nodev) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.textview_get_text expects textview");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  ErgoStr* s = n->text ? n->text : stdr_str_lit("");
  ergo_retain_val(EV_STR(s));
  return EV_STR(s);
}

static void cogito_textview_on_change(ErgoVal nodev, ErgoVal handler) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.textview_on_change expects textview");
  if (handler.tag != EVT_FN) ergo_trap("cogito.textview_on_change expects function");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->on_change) ergo_release_val(EV_FN(n->on_change));
  n->on_change = (ErgoFn*)handler.as.p;
  ergo_retain_val(EV_FN(n->on_change));
}

static void cogito_datepicker_on_change(ErgoVal nodev, ErgoVal handler) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.datepicker_on_change expects datepicker");
  if (handler.tag != EVT_FN) ergo_trap("cogito.datepicker_on_change expects function");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->kind != COGITO_DATEPICKER) ergo_trap("cogito.datepicker_on_change expects datepicker");
  if (n->on_change) ergo_release_val(EV_FN(n->on_change));
  n->on_change = (ErgoFn*)handler.as.p;
  ergo_retain_val(EV_FN(n->on_change));
}

static void cogito_dropdown_set_items(ErgoVal nodev, ErgoVal arrv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.dropdown_set_items expects dropdown");
  if (arrv.tag != EVT_ARR) ergo_trap("cogito.dropdown_set_items expects array");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
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
  ErgoArr* a = (ErgoArr*)arrv.as.p;
  if (!a || a->len == 0) { n->selected = -1; return; }
  n->menu_labels = (ErgoStr**)malloc(sizeof(ErgoStr*) * (size_t)a->len);
  n->menu_handlers = (ErgoFn**)malloc(sizeof(ErgoFn*) * (size_t)a->len);
  n->menu_len = (size_t)a->len;
  n->menu_cap = n->menu_len;
  for (size_t i = 0; i < n->menu_len; i++) {
    ErgoVal v = ergo_arr_get(a, (int64_t)i);
    ErgoStr* s = stdr_to_string(v);
    n->menu_labels[i] = s;
    n->menu_handlers[i] = NULL;
    if (s) ergo_retain_val(EV_STR(s));
  }
  if (n->selected < 0 && n->menu_len > 0) n->selected = 0;
  cogito_window_relayout(cogito_node_window(n));
}

static void cogito_dropdown_set_selected(ErgoVal nodev, ErgoVal idxv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.dropdown_set_selected expects dropdown");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  int idx = (int)ergo_as_int(idxv);
  if (idx < 0) idx = -1;
  if (n->menu_len > 0 && idx >= (int)n->menu_len) idx = (int)n->menu_len - 1;
  n->selected = idx;
  cogito_window_relayout(cogito_node_window(n));
}

static ErgoVal cogito_dropdown_get_selected(ErgoVal nodev) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.dropdown_get_selected expects dropdown");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  return EV_INT(n->selected);
}

static void cogito_dropdown_on_change(ErgoVal nodev, ErgoVal handler) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.dropdown_on_change expects dropdown");
  if (handler.tag != EVT_FN) ergo_trap("cogito.dropdown_on_change expects function");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->on_change) ergo_release_val(EV_FN(n->on_change));
  n->on_change = (ErgoFn*)handler.as.p;
  ergo_retain_val(EV_FN(n->on_change));
}

static void cogito_slider_set_value(ErgoVal nodev, ErgoVal valv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.slider_set_value expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  double v = (double)ergo_as_int(valv);
  if (v < n->slider_min) v = n->slider_min;
  if (v > n->slider_max) v = n->slider_max;
  n->slider_value = v;
  cogito_window_relayout(cogito_node_window(n));
}

static ErgoVal cogito_slider_get_value(ErgoVal nodev) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.slider_get_value expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  return EV_INT((int64_t)lround(n->slider_value));
}

static void cogito_slider_on_change(ErgoVal nodev, ErgoVal handler) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.slider_on_change expects slider");
  if (handler.tag != EVT_FN) ergo_trap("cogito.slider_on_change expects function");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->on_change) ergo_release_val(EV_FN(n->on_change));
  n->on_change = (ErgoFn*)handler.as.p;
  ergo_retain_val(EV_FN(n->on_change));
}

static void cogito_colorpicker_on_change(ErgoVal nodev, ErgoVal handler) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.colorpicker_on_change expects colorpicker");
  if (handler.tag != EVT_FN) ergo_trap("cogito.colorpicker_on_change expects function");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->kind != COGITO_COLORPICKER) ergo_trap("cogito.colorpicker_on_change expects colorpicker");
  if (n->on_change) ergo_release_val(EV_FN(n->on_change));
  n->on_change = (ErgoFn*)handler.as.p;
  ergo_retain_val(EV_FN(n->on_change));
}

static void cogito_node_set_id(ErgoVal nodev, ErgoVal idv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.node_set_id expects object");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  ErgoStr* s = stdr_to_string(idv);
  if (n->view_id) ergo_release_val(EV_STR(n->view_id));
  n->view_id = s;
  if (n->view_id) ergo_retain_val(EV_STR(n->view_id));
  cogito_window_relayout(cogito_node_window(n));
}

static void cogito_view_switcher_set_active(ErgoVal vsv, ErgoVal idv) {
  if (vsv.tag != EVT_OBJ) ergo_trap("cogito.view_switcher_set_active expects view_switcher");
  CogitoNode* n = (CogitoNode*)vsv.as.p;
  if (n->kind != COGITO_VIEWSWITCHER) ergo_trap("cogito.view_switcher_set_active expects view_switcher");
  ErgoStr* s = stdr_to_string(idv);
  if (n->view_active_id) ergo_release_val(EV_STR(n->view_active_id));
  n->view_active_id = s;
  if (n->view_active_id) ergo_retain_val(EV_STR(n->view_active_id));
  cogito_window_relayout(cogito_node_window(n));
}

static void cogito_tabs_set_items(ErgoVal tabsv, ErgoVal arrv) {
  if (tabsv.tag != EVT_OBJ) ergo_trap("cogito.tabs_set_items expects tabs");
  if (arrv.tag != EVT_ARR) ergo_trap("cogito.tabs_set_items expects array");
  CogitoNode* n = (CogitoNode*)tabsv.as.p;
  if (n->tab_labels) {
    for (size_t i = 0; i < n->tab_len; i++) {
      if (n->tab_labels[i]) ergo_release_val(EV_STR(n->tab_labels[i]));
    }
    free(n->tab_labels);
    n->tab_labels = NULL;
  }
  if (n->tab_ids) {
    for (size_t i = 0; i < n->tab_len; i++) {
      if (n->tab_ids[i]) ergo_release_val(EV_STR(n->tab_ids[i]));
    }
    free(n->tab_ids);
    n->tab_ids = NULL;
  }
  n->tab_len = 0;
  n->tab_cap = 0;
  ErgoArr* a = (ErgoArr*)arrv.as.p;
  if (!a || a->len == 0) { n->selected = -1; return; }
  n->tab_labels = (ErgoStr**)malloc(sizeof(ErgoStr*) * (size_t)a->len);
  n->tab_len = (size_t)a->len;
  n->tab_cap = n->tab_len;
  for (size_t i = 0; i < n->tab_len; i++) {
    ErgoVal v = ergo_arr_get(a, (int64_t)i);
    ErgoStr* s = stdr_to_string(v);
    n->tab_labels[i] = s;
    if (s) ergo_retain_val(EV_STR(s));
  }
  if (n->selected < 0 && n->tab_len > 0) n->selected = 0;
  if (n->view_switcher && n->tab_ids && n->selected >= 0 && (size_t)n->selected < n->tab_len) {
    CogitoNode* vs = n->view_switcher;
    ErgoStr* id = n->tab_ids[n->selected];
    if (id) {
      if (vs->view_active_id) ergo_release_val(EV_STR(vs->view_active_id));
      vs->view_active_id = id;
      ergo_retain_val(EV_STR(vs->view_active_id));
    }
  }
  cogito_window_relayout(cogito_node_window(n));
}

static void cogito_tabs_set_ids(ErgoVal tabsv, ErgoVal arrv) {
  if (tabsv.tag != EVT_OBJ) ergo_trap("cogito.tabs_set_ids expects tabs");
  if (arrv.tag != EVT_ARR) ergo_trap("cogito.tabs_set_ids expects array");
  CogitoNode* n = (CogitoNode*)tabsv.as.p;
  if (n->tab_ids) {
    for (size_t i = 0; i < n->tab_len; i++) {
      if (n->tab_ids[i]) ergo_release_val(EV_STR(n->tab_ids[i]));
    }
    free(n->tab_ids);
    n->tab_ids = NULL;
  }
  ErgoArr* a = (ErgoArr*)arrv.as.p;
  size_t len = a ? (size_t)a->len : 0;
  if (n->tab_len == 0) {
    n->tab_len = len;
    n->tab_cap = len;
    if (!n->tab_labels && len > 0) {
      n->tab_labels = (ErgoStr**)calloc(len, sizeof(ErgoStr*));
    }
  }
  if (n->tab_len == 0) return;
  n->tab_ids = (ErgoStr**)calloc(n->tab_len, sizeof(ErgoStr*));
  for (size_t i = 0; i < n->tab_len; i++) {
    if (i < len) {
      ErgoVal v = ergo_arr_get(a, (int64_t)i);
      ErgoStr* s = stdr_to_string(v);
      n->tab_ids[i] = s;
      if (s) ergo_retain_val(EV_STR(s));
    }
  }
  if (n->view_switcher && n->selected >= 0 && (size_t)n->selected < n->tab_len) {
    CogitoNode* vs = n->view_switcher;
    ErgoStr* id = n->tab_ids[n->selected];
    if (id) {
      if (vs->view_active_id) ergo_release_val(EV_STR(vs->view_active_id));
      vs->view_active_id = id;
      ergo_retain_val(EV_STR(vs->view_active_id));
    }
  }
  cogito_window_relayout(cogito_node_window(n));
}

static void cogito_tabs_set_selected(ErgoVal tabsv, ErgoVal idxv) {
  if (tabsv.tag != EVT_OBJ) ergo_trap("cogito.tabs_set_selected expects tabs");
  CogitoNode* n = (CogitoNode*)tabsv.as.p;
  int idx = (int)ergo_as_int(idxv);
  if (idx < 0) idx = -1;
  if (n->tab_len > 0 && idx >= (int)n->tab_len) idx = (int)n->tab_len - 1;
  n->selected = idx;
  if (n->view_switcher && n->tab_ids && idx >= 0 && (size_t)idx < n->tab_len) {
    CogitoNode* vs = n->view_switcher;
    ErgoStr* id = n->tab_ids[idx];
    if (id) {
      if (vs->view_active_id) ergo_release_val(EV_STR(vs->view_active_id));
      vs->view_active_id = id;
      ergo_retain_val(EV_STR(vs->view_active_id));
    }
  }
  cogito_window_relayout(cogito_node_window(n));
}

static ErgoVal cogito_tabs_get_selected(ErgoVal tabsv) {
  if (tabsv.tag != EVT_OBJ) ergo_trap("cogito.tabs_get_selected expects tabs");
  CogitoNode* n = (CogitoNode*)tabsv.as.p;
  return EV_INT(n->selected);
}

static void cogito_tabs_on_change(ErgoVal tabsv, ErgoVal handler) {
  if (tabsv.tag != EVT_OBJ) ergo_trap("cogito.tabs_on_change expects tabs");
  if (handler.tag != EVT_FN) ergo_trap("cogito.tabs_on_change expects function");
  CogitoNode* n = (CogitoNode*)tabsv.as.p;
  if (n->on_change) ergo_release_val(EV_FN(n->on_change));
  n->on_change = (ErgoFn*)handler.as.p;
  ergo_retain_val(EV_FN(n->on_change));
}

static void cogito_tabs_bind(ErgoVal tabsv, ErgoVal vsv) {
  if (tabsv.tag != EVT_OBJ) ergo_trap("cogito.tabs_bind expects tabs");
  if (vsv.tag != EVT_OBJ) ergo_trap("cogito.tabs_bind expects view_switcher");
  CogitoNode* t = (CogitoNode*)tabsv.as.p;
  CogitoNode* vs = (CogitoNode*)vsv.as.p;
  if (vs->kind != COGITO_VIEWSWITCHER) ergo_trap("cogito.tabs_bind expects view_switcher");
  t->view_switcher = vs;
  if (t->tab_ids && t->selected >= 0 && (size_t)t->selected < t->tab_len) {
    ErgoStr* id = t->tab_ids[t->selected];
    if (id) {
      if (vs->view_active_id) ergo_release_val(EV_STR(vs->view_active_id));
      vs->view_active_id = id;
      ergo_retain_val(EV_STR(vs->view_active_id));
    }
  }
}

static void cogito_progress_set_value(ErgoVal pv, ErgoVal valv) {
  if (pv.tag != EVT_OBJ) ergo_trap("cogito.progress_set_value expects progress");
  CogitoNode* p = (CogitoNode*)pv.as.p;
  double v = ergo_as_float(valv);
  if (v < 0.0) v = 0.0;
  if (v > 1.0) v = 1.0;
  p->progress_value = v;
  cogito_window_relayout(cogito_node_window(p));
}

static ErgoVal cogito_progress_get_value(ErgoVal pv) {
  if (pv.tag != EVT_OBJ) ergo_trap("cogito.progress_get_value expects progress");
  CogitoNode* p = (CogitoNode*)pv.as.p;
  return EV_FLOAT(p->progress_value);
}

static void cogito_toast_set_text(ErgoVal tv, ErgoVal textv) {
  if (tv.tag != EVT_OBJ) ergo_trap("cogito.toast_set_text expects toast");
  CogitoNode* n = (CogitoNode*)tv.as.p;
  ErgoStr* ts = stdr_to_string(textv);
  cogito_node_set_text(n, ts);
  cogito_window_relayout(cogito_node_window(n));
}

static void cogito_pointer_capture_set(ErgoVal nodev) {
  if (nodev.tag == EVT_NULL) {
    cogito_pointer_capture = NULL;
    return;
  }
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.pointer_capture expects node or null");
  cogito_pointer_capture = (CogitoNode*)nodev.as.p;
}

static void cogito_pointer_capture_clear(void) {
  cogito_pointer_capture = NULL;
}

static void cogito_node_set_class(ErgoVal nodev, ErgoVal classv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.node_set_class expects node");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  ErgoStr* cs = stdr_to_string(classv);
  const char* s = cs ? cs->data : "";
  n->font_mono = false;
  n->font_tabular = false;
  if (n->kind == COGITO_LABEL) n->label_class = COGITO_LABEL_CLASS_NONE;
  if (s && s[0]) {
    if (strcmp(s, "title") == 0) {
      n->label_class = COGITO_LABEL_CLASS_TITLE;
    } else if (strcmp(s, "subtitle") == 0 || strcmp(s, "sub-title") == 0) {
      n->label_class = COGITO_LABEL_CLASS_SUBTITLE;
    } else if (strcmp(s, "body") == 0) {
      n->label_class = COGITO_LABEL_CLASS_BODY;
    } else if (strcmp(s, "monospace") == 0 || strcmp(s, "mono") == 0) {
      n->font_mono = true;
    } else if (strcmp(s, "tabular") == 0 || strcmp(s, "tnum") == 0) {
      n->font_tabular = true;
      n->font_mono = true;
    }
  }
  if (cs) ergo_release_val(EV_STR(cs));
  cogito_apply_style_to_node(n);
  extern CogitoNode* cogito_active_window;
  if (cogito_active_window) {
    cogito_window_relayout(cogito_active_window);
  }
}

static void cogito_node_set_a11y_label(ErgoVal nodev, ErgoVal labelv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.node_set_a11y_label expects node");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->a11y_label) {
    ergo_release_val(EV_STR(n->a11y_label));
    n->a11y_label = NULL;
  }
  if (labelv.tag != EVT_NULL) {
    ErgoStr* ls = stdr_to_string(labelv);
    n->a11y_label = ls;
  }
#if defined(__APPLE__)
  if (n->kind == COGITO_WINDOW && cogito_raylib_ready) {
    const char* s = n->a11y_label ? n->a11y_label->data : "";
    cogito_macos_set_window_a11y_label(s);
  }
#endif
}

static void cogito_node_set_a11y_role(ErgoVal nodev, ErgoVal rolev) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.node_set_a11y_role expects node");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->a11y_role) {
    ergo_release_val(EV_STR(n->a11y_role));
    n->a11y_role = NULL;
  }
  if (rolev.tag != EVT_NULL) {
    ErgoStr* rs = stdr_to_string(rolev);
    n->a11y_role = rs;
  }
}

static void cogito_node_set_tooltip_val(ErgoVal nodev, ErgoVal textv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.node_set_tooltip expects node");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->tooltip) {
    ergo_release_val(EV_STR(n->tooltip));
    n->tooltip = NULL;
  }
  if (textv.tag != EVT_NULL) {
    ErgoStr* ts = stdr_to_string(textv);
    n->tooltip = ts;
  }
}

static void cogito_label_set_class(ErgoVal labelv, ErgoVal classv) {
  if (labelv.tag != EVT_OBJ) ergo_trap("cogito.label_set_class expects label");
  cogito_node_set_class(labelv, classv);
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

static void cogito_toast_on_click(ErgoVal tv, ErgoVal handler) {
  if (tv.tag != EVT_OBJ) ergo_trap("cogito.toast_on_click expects toast");
  CogitoNode* t = (CogitoNode*)tv.as.p;
  if (t->on_click) ergo_release_val(EV_FN(t->on_click));
  t->on_click = (ErgoFn*)handler.as.p;
  ergo_retain_val(EV_FN(t->on_click));
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
  if (strcmp(prop, "icon-size") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_icon_size = true; style->icon_size = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "icon-color") == 0 || strcmp(prop, "icon-tint") == 0) {
    Color c;
    if (cogito_css_parse_color(p, &c)) { style->has_icon_color = true; style->icon_color = c; }
    return;
  }
  if (strcmp(prop, "transition") == 0) {
    double v = 0.0;
    if (cogito_css_read_number(p, &v)) {
      int ms = (int)lround(v);
      if (strstr(*p, "s")) {
        ms = (int)lround(v * 1000.0);
      }
      style->has_transition_ms = true;
      style->transition_ms = ms;
    }
    char ease[32] = {0};
    cogito_css_read_ident(p, ease, sizeof(ease));
    if (ease[0]) {
      style->has_transition_ease = true;
      if (strcmp(ease, "ease-in") == 0) style->transition_ease = 2;
      else if (strcmp(ease, "ease-out") == 0) style->transition_ease = 3;
      else if (strcmp(ease, "ease-in-out") == 0) style->transition_ease = 4;
      else if (strcmp(ease, "ease") == 0) style->transition_ease = 1;
      else style->transition_ease = 0;
    }
    return;
  }
  if (strcmp(prop, "transition-duration") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) {
      int ms = (int)lround(v);
      if (strstr(*p, "s")) ms = (int)lround(v * 1000.0);
      style->has_transition_ms = true;
      style->transition_ms = ms;
    }
    return;
  }
  if (strcmp(prop, "transition-easing") == 0 || strcmp(prop, "transition-timing-function") == 0) {
    char ease[32] = {0};
    cogito_css_read_ident(p, ease, sizeof(ease));
    if (ease[0]) {
      style->has_transition_ease = true;
      if (strcmp(ease, "ease-in") == 0) style->transition_ease = 2;
      else if (strcmp(ease, "ease-out") == 0) style->transition_ease = 3;
      else if (strcmp(ease, "ease-in-out") == 0) style->transition_ease = 4;
      else if (strcmp(ease, "ease") == 0) style->transition_ease = 1;
      else style->transition_ease = 0;
    }
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

static bool cogito_css_selector_kind(const char* sel, CogitoKind* out_kind, bool* out_base, int* out_label_class, bool* out_menu, int* out_class) {
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
    if (strcmp(class_part, "monospace") == 0 || strcmp(class_part, "mono") == 0) {
      if (out_class) *out_class = COGITO_STYLE_CLASS_MONO;
      return true;
    }
    if (strcmp(class_part, "tabular") == 0 || strcmp(class_part, "tnum") == 0) {
      if (out_class) *out_class = COGITO_STYLE_CLASS_TABULAR;
      return true;
    }
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
      if (strcmp(class_part, "zstack") == 0) { if (out_kind) *out_kind = COGITO_ZSTACK; return true; }
      if (strcmp(class_part, "fixed") == 0) { if (out_kind) *out_kind = COGITO_FIXED; return true; }
      if (strcmp(class_part, "scroller") == 0 || strcmp(class_part, "scroll") == 0) { if (out_kind) *out_kind = COGITO_SCROLLER; return true; }
      if (strcmp(class_part, "textfield") == 0 || strcmp(class_part, "text-field") == 0) { if (out_kind) *out_kind = COGITO_TEXTFIELD; return true; }
      if (strcmp(class_part, "searchfield") == 0 || strcmp(class_part, "search-field") == 0) { if (out_kind) *out_kind = COGITO_SEARCHFIELD; return true; }
      if (strcmp(class_part, "textview") == 0 || strcmp(class_part, "text-view") == 0) { if (out_kind) *out_kind = COGITO_TEXTVIEW; return true; }
      if (strcmp(class_part, "dropdown") == 0 || strcmp(class_part, "select") == 0) { if (out_kind) *out_kind = COGITO_DROPDOWN; return true; }
      if (strcmp(class_part, "date-picker") == 0 || strcmp(class_part, "datepicker") == 0) { if (out_kind) *out_kind = COGITO_DATEPICKER; return true; }
      if (strcmp(class_part, "stepper") == 0) { if (out_kind) *out_kind = COGITO_STEPPER; return true; }
      if (strcmp(class_part, "slider") == 0) { if (out_kind) *out_kind = COGITO_SLIDER; return true; }
      if (strcmp(class_part, "tabs") == 0) { if (out_kind) *out_kind = COGITO_TABS; return true; }
      if (strcmp(class_part, "segmented") == 0 || strcmp(class_part, "segmented-control") == 0) { if (out_kind) *out_kind = COGITO_SEGMENTED; return true; }
      if (strcmp(class_part, "view-switcher") == 0 || strcmp(class_part, "viewswitcher") == 0) { if (out_kind) *out_kind = COGITO_VIEWSWITCHER; return true; }
      if (strcmp(class_part, "progress") == 0) { if (out_kind) *out_kind = COGITO_PROGRESS; return true; }
      if (strcmp(class_part, "tree") == 0 || strcmp(class_part, "treeview") == 0) { if (out_kind) *out_kind = COGITO_TREEVIEW; return true; }
      if (strcmp(class_part, "color") == 0 || strcmp(class_part, "colorpicker") == 0 || strcmp(class_part, "color-picker") == 0) { if (out_kind) *out_kind = COGITO_COLORPICKER; return true; }
      if (strcmp(class_part, "toasts") == 0) { if (out_kind) *out_kind = COGITO_TOASTS; return true; }
      if (strcmp(class_part, "toast") == 0) { if (out_kind) *out_kind = COGITO_TOAST; return true; }
      if (strcmp(class_part, "tooltip") == 0) { if (out_kind) *out_kind = COGITO_TOOLTIP; return true; }
      if (strcmp(class_part, "toolbar") == 0 || strcmp(class_part, "bottom-toolbar") == 0) { if (out_kind) *out_kind = COGITO_TOOLBAR; return true; }
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
  if (strcmp(s, "fixed") == 0) { if (out_kind) *out_kind = COGITO_FIXED; return true; }
  if (strcmp(s, "scroller") == 0 || strcmp(s, "scroll") == 0) { if (out_kind) *out_kind = COGITO_SCROLLER; return true; }
  if (strcmp(s, "textfield") == 0 || strcmp(s, "text-field") == 0) { if (out_kind) *out_kind = COGITO_TEXTFIELD; return true; }
  if (strcmp(s, "searchfield") == 0 || strcmp(s, "search-field") == 0) { if (out_kind) *out_kind = COGITO_SEARCHFIELD; return true; }
  if (strcmp(s, "textview") == 0 || strcmp(s, "text-view") == 0) { if (out_kind) *out_kind = COGITO_TEXTVIEW; return true; }
  if (strcmp(s, "dropdown") == 0 || strcmp(s, "select") == 0) { if (out_kind) *out_kind = COGITO_DROPDOWN; return true; }
  if (strcmp(s, "date-picker") == 0 || strcmp(s, "datepicker") == 0) { if (out_kind) *out_kind = COGITO_DATEPICKER; return true; }
  if (strcmp(s, "stepper") == 0) { if (out_kind) *out_kind = COGITO_STEPPER; return true; }
  if (strcmp(s, "slider") == 0) { if (out_kind) *out_kind = COGITO_SLIDER; return true; }
  if (strcmp(s, "tabs") == 0) { if (out_kind) *out_kind = COGITO_TABS; return true; }
  if (strcmp(s, "segmented") == 0 || strcmp(s, "segmented-control") == 0) { if (out_kind) *out_kind = COGITO_SEGMENTED; return true; }
  if (strcmp(s, "view-switcher") == 0 || strcmp(s, "viewswitcher") == 0) { if (out_kind) *out_kind = COGITO_VIEWSWITCHER; return true; }
  if (strcmp(s, "progress") == 0) { if (out_kind) *out_kind = COGITO_PROGRESS; return true; }
  if (strcmp(s, "tree") == 0 || strcmp(s, "treeview") == 0) { if (out_kind) *out_kind = COGITO_TREEVIEW; return true; }
  if (strcmp(s, "color") == 0 || strcmp(s, "colorpicker") == 0 || strcmp(s, "color-picker") == 0) { if (out_kind) *out_kind = COGITO_COLORPICKER; return true; }
  if (strcmp(s, "toasts") == 0) { if (out_kind) *out_kind = COGITO_TOASTS; return true; }
  if (strcmp(s, "toast") == 0) { if (out_kind) *out_kind = COGITO_TOAST; return true; }
  if (strcmp(s, "tooltip") == 0) { if (out_kind) *out_kind = COGITO_TOOLTIP; return true; }
  if (strcmp(s, "toolbar") == 0 || strcmp(s, "bottom-toolbar") == 0) { if (out_kind) *out_kind = COGITO_TOOLBAR; return true; }
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
  if (!child || !child[0]) {
    bool is_base = false;
    bool is_menu = false;
    CogitoKind kind = COGITO_WINDOW;
    int label_class = COGITO_LABEL_CLASS_NONE;
    int class_kind = COGITO_STYLE_CLASS_NONE;
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
    if (cogito_css_selector_kind(sel, &kind, &is_base, &label_class, &is_menu, &class_kind)) {
      CogitoStyle* style = NULL;
      bool hover = strcmp(state, "hover") == 0;
      bool active = strcmp(state, "active") == 0;
      bool checked = strcmp(state, "checked") == 0;
      bool disabled = strcmp(state, "disabled") == 0;
      if (class_kind != COGITO_STYLE_CLASS_NONE) {
        if (class_kind == COGITO_STYLE_CLASS_MONO) {
          if (checked) style = &cogito_theme.class_mono_checked;
          else if (disabled) style = &cogito_theme.class_mono_disabled;
          else if (active) style = &cogito_theme.class_mono_active;
          else if (hover) style = &cogito_theme.class_mono_hover;
          else style = &cogito_theme.class_mono;
        } else if (class_kind == COGITO_STYLE_CLASS_TABULAR) {
          if (checked) style = &cogito_theme.class_tabular_checked;
          else if (disabled) style = &cogito_theme.class_tabular_disabled;
          else if (active) style = &cogito_theme.class_tabular_active;
          else if (hover) style = &cogito_theme.class_tabular_hover;
          else style = &cogito_theme.class_tabular;
        }
      } else if (label_class != COGITO_LABEL_CLASS_NONE) {
        if (checked) style = &cogito_theme.label_class_checked[label_class];
        else if (disabled) style = &cogito_theme.label_class_disabled[label_class];
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
        } else if (disabled) {
          style = &cogito_theme.per_kind_disabled[kind];
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
  if (strcmp(parent_base, "menu") == 0) {
    if (strcmp(child_base, "item") == 0) {
      if (*count < max) {
        styles[(*count)] = &cogito_theme.menu_item;
        if (flags && strcmp(child_state, "selection") == 0) flags[(*count)] = 1;
        (*count)++;
      }
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
  CogitoStyle* item_style = &cogito_theme.menu_item;
  int pad_x = item_style->has_padding_left ? item_style->padding_left
              : (cogito_theme.menu.has_padding_left ? cogito_theme.menu.padding_left : 10);
  int pad_y = item_style->has_padding_top ? item_style->padding_top
              : (cogito_theme.menu.has_padding_top ? cogito_theme.menu.padding_top : 6);
  int item_h = font_size + pad_y * 2;
  int min_h = item_style->has_min_h ? item_style->min_h
              : (cogito_theme.menu.has_min_h ? cogito_theme.menu.min_h : 0);
  if (min_h > item_h) item_h = min_h;
  int menu_pad_top = 8;
  int menu_pad_bottom = 8;
  int max_w = 0;
  for (size_t i = 0; i < n->menu_len; i++) {
    const char* text = n->menu_labels[i] ? n->menu_labels[i]->data : "";
    int w = cogito_text_width_size(text, font_size);
    if (w > max_w) max_w = w;
  }
  int w = max_w + pad_x * 2;
  int h = (int)n->menu_len * item_h + menu_pad_top + menu_pad_bottom;
  cogito_menu.open = true;
  cogito_menu.owner = n;
  cogito_menu.item_h = item_h;
  cogito_menu.pad_x = pad_x;
  cogito_menu.pad_y = pad_y;
  cogito_menu.pad_top = menu_pad_top;
  cogito_menu.pad_bottom = menu_pad_bottom;
  cogito_menu.w = w;
  cogito_menu.h = h;
  int x = n->x;
  int y = n->y + n->h + 4;
  int win_w = GetScreenWidth();
  int win_h = GetScreenHeight();
  if (w > win_w - 8) w = win_w - 8;
  if (h > win_h - 8) h = win_h - 8;
  if (x + w > win_w - 4) x = win_w - 4 - w;
  if (x < 4) x = 4;
  if (y + h > win_h - 4) {
    int above = n->y - h - 4;
    if (above >= 4) y = above;
    else y = win_h - 4 - h;
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
  int rel_y = y - (menu->y + menu->pad_top);
  if (rel_y < 0) return -1;
  int idx = rel_y / menu->item_h;
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

static void cogito_draw_text_size_font(Font* f, int x, int y, const char* text, Color color, int size, bool bold) {
  if (!text) return;
  if (size <= 0) size = cogito_font_size();
  if (cogito_font_ready && f) {
    DrawTextEx(*f, text, (Vector2){(float)x, (float)y}, (float)size, 1.0f, color);
    if (bold) {
      DrawTextEx(*f, text, (Vector2){(float)x + 1.0f, (float)y}, (float)size, 1.0f, color);
    }
  } else {
    DrawText(text, x, y, size, color);
    if (bold) DrawText(text, x + 1, y, size, color);
  }
}

static void cogito_draw_text_size(int x, int y, const char* text, Color color, int size, bool bold) {
  cogito_draw_text_size_font(&cogito_font, x, y, text, color, size, bold);
}

static void cogito_draw_node(CogitoNode* n) {
  if (!n) return;
  if (n->tooltip && n->tooltip->data && n->tooltip->data[0] && !n->disabled) {
    int mx = (int)GetMousePosition().x;
    int my = (int)GetMousePosition().y;
    if (cogito_hit_node(n, mx, my)) {
      cogito_tooltip_hovered = n;
    }
  }
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
        Color text = n->text_color;
        CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
        if (s.has_text) text = s.text;
        int align = n->text_align;
        if (n->text_wrap) {
          cogito_draw_text_wrapped(n->x, n->y, n->w, n->text->data, text, size, cogito_label_bold(n), align, true);
        } else if (n->text_ellipsis) {
          int th = cogito_text_height_size(size);
          int ty = n->y + (n->h - th) / 2;
          cogito_draw_text_ellipsis(n->x, ty, n->w, n->text->data, text, size, cogito_label_bold(n), align);
        } else {
          int tw = cogito_text_width_size_node(n, n->text->data, size);
          int th = cogito_text_height_size(size);
          int ty = n->y + (n->h - th) / 2;
          int tx = n->x;
          if (align == 1) tx = n->x + (n->w - tw) / 2;
          else if (align == 2) tx = n->x + (n->w - tw);
          cogito_draw_text_size_font(cogito_font_for_node(n), tx, ty, n->text->data, text, size, cogito_label_bold(n));
        }
      }
      break;
    }
    case COGITO_TEXTFIELD: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      Color bg = n->bg;
      Color border = n->border_color;
      int border_w = n->border_width;
      int radius = n->border_radius;
      if (s.has_bg) bg = s.bg;
      if (s.has_border) border = s.border;
      if (s.has_border_width) border_w = s.border_width;
      if (s.has_radius) radius = s.radius;
      if (n->bg_set || border_w > 0) {
        cogito_draw_rect(n->x, n->y, n->w, n->h, bg, radius);
        if (border_w > 0) {
          cogito_draw_rect_lines(n->x, n->y, n->w, n->h, border, radius, border_w);
        }
      }
      if (cogito_focused == n && !n->disabled) {
        Color focus = s.has_selection ? s.selection : (n->selection_set ? n->selection_color : cogito_rgba(120, 160, 220, 255));
        cogito_draw_rect_lines(n->x, n->y, n->w, n->h, focus, radius, border_w > 0 ? border_w : 2);
      }
      int pad = 8;
      int size = cogito_node_font_size(n);
      Color text = n->text_color;
      if (s.has_text) text = s.text;
      const char* t = n->text ? n->text->data : "";
      int th = cogito_text_height_size(size);
      int ty = n->y + (n->h - th) / 2;
      if (n->sel_start != n->sel_end) {
        int a = n->sel_start;
        int b = n->sel_end;
        if (a > b) { int tmp = a; a = b; b = tmp; }
        int sx = cogito_text_width_size_n_node(n, t, a, size);
        int ex = cogito_text_width_size_n_node(n, t, b, size);
        int x0 = n->x + pad + sx;
        int x1 = n->x + pad + ex;
        int max_x = n->x + n->w - pad;
        if (x0 < n->x + pad) x0 = n->x + pad;
        if (x1 > max_x) x1 = max_x;
        if (x1 > x0) {
          Color sel = s.has_selection ? s.selection : (n->selection_set ? n->selection_color : cogito_rgba(200, 220, 250, 255));
          cogito_draw_rect(x0, ty, x1 - x0, th, sel, 2);
        }
      }
      cogito_draw_text_ellipsis(n->x + pad, ty, n->w - pad * 2, t, text, size, cogito_node_bold(n), 0);
      if (cogito_focused == n && !n->disabled) {
        int caret = n->caret;
        if (caret < 0) caret = 0;
        int tw = cogito_text_width_size_n_node(n, t, caret, size);
        int cx = n->x + pad + tw;
        int max_x = n->x + n->w - pad;
        if (cx > max_x) cx = max_x;
        DrawLine(cx, ty, cx, ty + th, text);
      }
      break;
    }
    case COGITO_TEXTVIEW: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      Color bg = n->bg;
      Color border = n->border_color;
      int border_w = n->border_width;
      int radius = n->border_radius;
      if (s.has_bg) bg = s.bg;
      if (s.has_border) border = s.border;
      if (s.has_border_width) border_w = s.border_width;
      if (s.has_radius) radius = s.radius;
      if (n->bg_set || border_w > 0) {
        cogito_draw_rect(n->x, n->y, n->w, n->h, bg, radius);
        if (border_w > 0) {
          cogito_draw_rect_lines(n->x, n->y, n->w, n->h, border, radius, border_w);
        }
      }
      if (cogito_focused == n && !n->disabled) {
        Color focus = s.has_selection ? s.selection : (n->selection_set ? n->selection_color : cogito_rgba(120, 160, 220, 255));
        cogito_draw_rect_lines(n->x, n->y, n->w, n->h, focus, radius, border_w > 0 ? border_w : 2);
      }
      int pad = 8;
      int size = cogito_node_font_size(n);
      Color text = n->text_color;
      if (s.has_text) text = s.text;
      const char* t = n->text ? n->text->data : "";
      if (n->sel_start != n->sel_end) {
        int a = n->sel_start;
        int b = n->sel_end;
        if (a > b) { int tmp = a; a = b; b = tmp; }
        int max_w = n->w - pad * 2;
        if (max_w < 1) max_w = 1;
        int len = (int)strlen(t);
        int idx = 0;
        int line_h = cogito_text_height_size(size);
        int cy = n->y + pad;
        Color sel = s.has_selection ? s.selection : (n->selection_set ? n->selection_color : cogito_rgba(200, 220, 250, 255));
        while (idx < len) {
          int line_start = idx;
          int line_len = 0;
          int last_space = -1;
          for (int i = idx; i < len; i++) {
            char c = t[i];
            if (c == '\n') {
              line_len = i - idx;
              idx = i + 1;
              break;
            }
            int w = cogito_text_width_size_n_node(n, t + idx, i - idx + 1, size);
            if (c == ' ') last_space = i;
            if (w > max_w) {
              if (last_space >= idx) {
                line_len = last_space - idx;
                idx = last_space + 1;
              } else {
                line_len = i - idx;
                idx = i;
              }
              break;
            }
            if (i == len - 1) {
              line_len = i - idx + 1;
              idx = len;
            }
          }
          int line_end = line_start + line_len;
          if (b > line_start && a < line_end) {
            int sa = a > line_start ? a : line_start;
            int sb = b < line_end ? b : line_end;
            int sx = cogito_text_width_size_n_node(n, t + line_start, sa - line_start, size);
            int ex = cogito_text_width_size_n_node(n, t + line_start, sb - line_start, size);
            int x0 = n->x + pad + sx;
            int x1 = n->x + pad + ex;
            if (x1 > x0) {
              cogito_draw_rect(x0, cy, x1 - x0, line_h, sel, 2);
            }
          }
          cy += line_h;
        }
      }
      cogito_draw_text_wrapped(n->x + pad, n->y + pad, n->w - pad * 2, t, text, size, cogito_node_bold(n), 0, true);
      if (cogito_focused == n && !n->disabled) {
        int max_w = n->w - pad * 2;
        if (max_w < 1) max_w = 1;
        int len = (int)strlen(t);
        int idx = 0;
        int line_h = cogito_text_height_size(size);
        int cy = n->y + pad;
        int caret_idx = n->caret;
        if (caret_idx < 0) caret_idx = 0;
        if (caret_idx > len) caret_idx = len;
        while (idx <= len) {
          int line_start = idx;
          int line_len = 0;
          int last_space = -1;
          for (int i = idx; i < len; i++) {
            char c = t[i];
            if (c == '\n') {
              line_len = i - idx;
              idx = i + 1;
              break;
            }
            int w = cogito_text_width_size_n_node(n, t + idx, i - idx + 1, size);
            if (c == ' ') last_space = i;
            if (w > max_w) {
              if (last_space >= idx) {
                line_len = last_space - idx;
                idx = last_space + 1;
              } else {
                line_len = i - idx;
                idx = i;
              }
              break;
            }
            if (i == len - 1) {
              line_len = i - idx + 1;
              idx = len;
            }
          }
          int line_end = line_start + line_len;
          if (caret_idx >= line_start && caret_idx <= line_end) {
            int local = caret_idx - line_start;
            int cx = n->x + pad + cogito_text_width_size_n_node(n, t + line_start, local, size);
            int max_x = n->x + n->w - pad;
            if (cx > max_x) cx = max_x;
            DrawLine(cx, cy, cx, cy + line_h, text);
            break;
          }
          cy += line_h;
          if (idx >= len) break;
        }
      }
      break;
    }
    case COGITO_DROPDOWN: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      Color bg = n->bg;
      Color border = n->border_color;
      int border_w = n->border_width;
      int radius = n->border_radius;
      if (s.has_bg) bg = s.bg;
      if (s.has_border) border = s.border;
      if (s.has_border_width) border_w = s.border_width;
      if (s.has_radius) radius = s.radius;
      if (n->bg_set || border_w > 0) {
        cogito_draw_rect(n->x, n->y, n->w, n->h, bg, radius);
        if (border_w > 0) {
          cogito_draw_rect_lines(n->x, n->y, n->w, n->h, border, radius, border_w);
        }
      }
      int pad = 8;
      int size = cogito_node_font_size(n);
      Color text = n->text_color;
      if (s.has_text) text = s.text;
      const char* label = "";
      if (n->menu_len > 0 && n->selected >= 0 && (size_t)n->selected < n->menu_len) {
        label = n->menu_labels[n->selected] ? n->menu_labels[n->selected]->data : "";
      }
      int th = cogito_text_height_size(size);
      int ty = n->y + (n->h - th) / 2;
      int icon_size = s.has_icon_size ? s.icon_size : 12;
      if (icon_size < 8) icon_size = 8;
      int icon_box = icon_size + 4;
      int text_w = n->w - pad * 2 - icon_box;
      cogito_draw_text_ellipsis(n->x + pad, ty, text_w, label, text, size, cogito_node_bold(n), 0);
      int ix = n->x + n->w - pad - icon_box + (icon_box - icon_size) / 2;
      int iy = n->y + (n->h - icon_size) / 2;
      Texture2D tex = {0};
      int tw = 0;
      int th_i = 0;
      Color tint = s.has_icon_color ? s.icon_color : text;
      const char* drop_icon =
#if defined(__APPLE__)
          "sf:chevron.down";
#else
          "pan-down";
#endif
      if (cogito_icon_load_texture(drop_icon, icon_size, &tex, &tw, &th_i)) {
        float dw = (float)icon_size;
        float dh = (float)icon_size;
        if (tw > 0 && th_i > 0) {
          float sx = (float)icon_size / (float)tw;
          float sy = (float)icon_size / (float)th_i;
          float s = sx < sy ? sx : sy;
          dw = (float)tw * s;
          dh = (float)th_i * (s / 2);
        }
        float dx = ix + ((float)icon_size - dw) * 0.5f;
        float dy = iy + ((float)icon_size - dh) * 0.5f;
        DrawTexturePro(tex, (Rectangle){0, 0, (float)tw, (float)th_i},
                       (Rectangle){dx, dy, dw, dh},
                       (Vector2){0, 0}, 0.0f, tint);
      } else {
        int ax = n->x + n->w - pad - 8;
        int ay = n->y + n->h / 2 - 2;
        DrawTriangle((Vector2){(float)ax, (float)ay},
                     (Vector2){(float)ax + 8.0f, (float)ay},
                     (Vector2){(float)ax + 4.0f, (float)ay + 6.0f}, text);
      }
      break;
    }
    case COGITO_DATEPICKER: {
      cogito_datepicker_ensure_date(n);
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      Color bg = n->bg;
      Color border = n->border_color;
      int border_w = n->border_width;
      int radius = n->border_radius;
      if (s.has_bg) bg = s.bg;
      if (s.has_border) border = s.border;
      if (s.has_border_width) border_w = s.border_width;
      if (s.has_radius) radius = s.radius;
      if (n->bg_set || border_w > 0) {
        cogito_draw_rect(n->x, n->y, n->w, n->h, bg, radius);
        if (border_w > 0) {
          cogito_draw_rect_lines(n->x, n->y, n->w, n->h, border, radius, border_w);
        }
      }
      int pad_l = 0, pad_t = 0, pad_r = 0, pad_b = 0;
      int header_h = 0, grid_x = 0, grid_y = 0, cell_w = 0, cell_h = 0;
      cogito_datepicker_layout(n, &pad_l, &pad_t, &pad_r, &pad_b, &header_h, &grid_x, &grid_y, &cell_w, &cell_h);
      int size = cogito_node_font_size(n);
      Color text = s.has_text ? s.text : n->text_color;
      static const char* months[] = {"January","February","March","April","May","June","July","August","September","October","November","December"};
      const char* mname = (n->date_month >= 1 && n->date_month <= 12) ? months[n->date_month - 1] : "";
      char header[64];
      snprintf(header, sizeof(header), "%s %d", mname, n->date_year);
      int header_w = n->w - pad_l - pad_r;
      int tw = cogito_text_width_size(header, size);
      int tx = n->x + pad_l + (header_w - tw) / 2;
      int ty = n->y + pad_t + (header_h - cogito_text_height_size(size)) / 2;
      cogito_draw_text_size(tx, ty, header, text, size, false);
      int left_x = n->x + pad_l;
      int right_x = n->x + n->w - pad_r - header_h;
      int arrow_y = n->y + pad_t + (header_h - cogito_text_height_size(size)) / 2;
      cogito_draw_text_size(left_x + (header_h / 2) - 4, arrow_y, "<", text, size, false);
      cogito_draw_text_size(right_x + (header_h / 2) - 4, arrow_y, ">", text, size, false);
      int first = cogito_weekday(n->date_year, n->date_month, 1);
      int dim = cogito_days_in_month(n->date_year, n->date_month);
      Color sel = s.has_selection ? s.selection : (n->selection_set ? n->selection_color : cogito_rgba(114, 222, 194, 255));
      for (int day = 1; day <= dim; day++) {
        int idx = first + (day - 1);
        int row = idx / 7;
        int col = idx % 7;
        int cx = grid_x + col * cell_w;
        int cy = grid_y + row * cell_h;
        if (day == n->date_day) {
          int r = cell_h / 2;
          if (r < 2) r = 2;
          int inset = 2;
          cogito_draw_rect(cx + inset, cy + inset, cell_w - inset * 2, cell_h - inset * 2, sel, r);
        }
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", day);
        int dw = cogito_text_width_size(buf, size);
        int dh = cogito_text_height_size(size);
        int dx = cx + (cell_w - dw) / 2;
        int dy = cy + (cell_h - dh) / 2;
        cogito_draw_text_size(dx, dy, buf, text, size, false);
      }
      break;
    }
    case COGITO_SLIDER: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      Color track = s.has_bg ? s.bg : cogito_rgba(190, 190, 190, 255);
      Color knob = s.has_text ? s.text : cogito_rgba(80, 80, 80, 255);
      int pad = 8;
      int cy = n->y + n->h / 2;
      int x0 = n->x + pad;
      int x1 = n->x + n->w - pad;
      if (x1 < x0) x1 = x0;
      DrawLine(x0, cy, x1, cy, track);
      double t = 0.0;
      if (n->slider_max > n->slider_min) {
        t = (n->slider_value - n->slider_min) / (n->slider_max - n->slider_min);
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;
      }
      int kx = x0 + (int)((x1 - x0) * t);
      DrawCircle(kx, cy, 6, knob);
      break;
    }
    case COGITO_COLORPICKER: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      Color bg = n->bg;
      Color border = n->border_color;
      int border_w = n->border_width;
      int radius = n->border_radius;
      if (s.has_bg) bg = s.bg;
      if (s.has_border) border = s.border;
      if (s.has_border_width) border_w = s.border_width;
      if (s.has_radius) radius = s.radius;
      if (n->bg_set || border_w > 0) {
        cogito_draw_rect(n->x, n->y, n->w, n->h, bg, radius);
        if (border_w > 0) {
          cogito_draw_rect_lines(n->x, n->y, n->w, n->h, border, radius, border_w);
        }
      }
      if (!n->text || !n->text->data || !n->text->data[0] || !n->color_hex_editing) {
        cogito_colorpicker_sync_hex(n);
      }
      int label_w = 0, row_h = 0, slider_h = 0, gap = 0;
      int hex_x = 0, hex_y = 0, hex_w = 0, hex_h = 0;
      cogito_colorpicker_layout(n, NULL, NULL, NULL, NULL, &label_w, &row_h, &slider_h, &gap, &hex_x, &hex_y, &hex_w, &hex_h);
      Color text = s.has_text ? s.text : n->text_color;
      Color accent = s.has_selection ? s.selection : cogito_rgba(114, 222, 194, 255);
      int size = cogito_node_font_size(n);
      const char* labels[3] = {"H", "C", "T"};
      double values[3] = {n->color_h, n->color_c, n->color_t};
      double ranges[3] = {360.0, 100.0, 100.0};
      for (int i = 0; i < 3; i++) {
        int row_y = n->y + (s.has_padding_top ? s.padding_top : 10) + i * (row_h + gap);
        int lx = n->x + (s.has_padding_left ? s.padding_left : 10);
        int ly = row_y + (row_h - cogito_text_height_size(size)) / 2;
        cogito_draw_text_size(lx, ly, labels[i], text, size, false);
        int sx = 0, sy = 0, sw = 0, sh = 0;
        cogito_colorpicker_slider_rect(n, i, &sx, &sy, &sw, &sh);
        DrawLine(sx, sy + sh / 2, sx + sw, sy + sh / 2, cogito_rgba(190, 190, 190, 255));
        double t = (ranges[i] > 0.0) ? (values[i] / ranges[i]) : 0.0;
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;
        int kx = sx + (int)(sw * t);
        DrawCircle(kx, sy + sh / 2, 6, accent);
      }
      CogitoStyle hs = cogito_theme_resolve(COGITO_TEXTFIELD);
      Color hex_bg = hs.has_bg ? hs.bg : cogito_rgba(245, 245, 245, 255);
      Color hex_border = hs.has_border ? hs.border : cogito_rgba(0, 0, 0, 0);
      int hex_bw = hs.has_border_width ? hs.border_width : 0;
      int hex_radius = hs.has_radius ? hs.radius : 6;
      cogito_draw_rect(hex_x, hex_y, hex_w, hex_h, hex_bg, hex_radius);
      if (hex_bw > 0) {
        cogito_draw_rect_lines(hex_x, hex_y, hex_w, hex_h, hex_border, hex_radius, hex_bw);
      }
      int pad = 8;
      const char* hex = n->text ? n->text->data : "#000000";
      int th = cogito_text_height_size(size);
      int ty = hex_y + (hex_h - th) / 2;
      if (n->sel_start != n->sel_end) {
        int a = n->sel_start;
        int b = n->sel_end;
        if (a > b) { int tmp = a; a = b; b = tmp; }
        int sx = cogito_text_width_size_n_node(n, hex, a, size);
        int ex = cogito_text_width_size_n_node(n, hex, b, size);
        int x0 = hex_x + pad + sx;
        int x1 = hex_x + pad + ex;
        if (x1 > x0) {
          cogito_draw_rect(x0, ty, x1 - x0, th, accent, 2);
        }
      }
      cogito_draw_text_ellipsis(hex_x + pad, ty, hex_w - pad * 2, hex, text, size, false, 0);
      if (cogito_focused == n && n->color_hex_editing) {
        int caret = n->caret;
        if (caret < 0) caret = 0;
        int tw = cogito_text_width_size_n_node(n, hex, caret, size);
        int cx = hex_x + pad + tw;
        int max_x = hex_x + hex_w - pad;
        if (cx > max_x) cx = max_x;
        DrawLine(cx, ty, cx, ty + th, text);
      }
      break;
    }
    case COGITO_TABS: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      Color bg = n->bg;
      if (s.has_bg) bg = s.bg;
      if (n->bg_set || s.has_bg) {
        cogito_draw_rect(n->x, n->y, n->w, n->h, bg, n->border_radius);
      }
      Color text = n->text_color;
      if (s.has_text) text = s.text;
      Color sel = s.has_selection ? s.selection : cogito_rgba(220, 220, 220, 255);
      int size = cogito_node_font_size(n);
      int th = cogito_text_height_size(size);
      int pad_x = 12;
      int pad_y = 8;
      int gap = 6;
      int tab_h = th + pad_y * 2;
      int ty = n->y + (n->h - tab_h) / 2;
      int cx = n->x + n->padding_left;
      for (size_t i = 0; i < n->tab_len; i++) {
        const char* t = n->tab_labels[i] ? n->tab_labels[i]->data : "";
        int tw = cogito_text_width_size(t, size);
        int tab_w = tw + pad_x * 2;
        if (n->selected == (int)i) {
          int inset = 2;
          int sx = cx + inset;
          int sy = ty + inset;
          int sw = tab_w - inset * 2;
          int sh = tab_h - inset * 2;
          if (sw < 0) sw = 0;
          if (sh < 0) sh = 0;
          int base_radius = n->border_radius;
          int sel_radius = base_radius - inset;
          if (sel_radius < 0) sel_radius = 0;
          cogito_draw_rect(sx, sy, sw, sh, sel, sel_radius);
        }
        cogito_draw_text_size(cx + pad_x, ty + pad_y, t, text, size, cogito_node_bold(n));
        cx += tab_w + gap;
      }
      break;
    }
    case COGITO_PROGRESS: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      Color track = s.has_bg ? s.bg : (n->bg_set ? n->bg : cogito_rgba(210, 210, 210, 255));
      Color fill = s.has_selection ? s.selection : (s.has_text ? s.text : cogito_rgba(120, 160, 220, 255));
      int radius = s.has_radius ? s.radius : n->border_radius;
      double t = n->progress_value;
      if (t < 0.0) t = 0.0;
      if (t > 1.0) t = 1.0;
      cogito_draw_rect(n->x, n->y, n->w, n->h, track, radius);
      int fw = (int)(n->w * t);
      if (fw > 0) {
        cogito_draw_rect(n->x, n->y, fw, n->h, fill, radius);
      }
      break;
    }
    case COGITO_TOAST: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      if (s.has_bg) { n->bg = s.bg; n->bg_set = true; }
      if (s.has_border) { n->border_color = s.border; n->border_color_set = true; }
      if (s.has_border_width) { n->border_width = s.border_width; n->border_width_set = true; }
      if (s.has_radius) n->border_radius = s.radius;
      if (n->bg_set) {
        cogito_draw_rect(n->x, n->y, n->w, n->h, n->bg, n->border_radius);
        if (n->border_width_set && n->border_width > 0) {
          cogito_draw_rect_lines(n->x, n->y, n->w, n->h, n->border_color, n->border_radius, n->border_width);
        }
      }
      int size = cogito_node_font_size(n);
      Color text = s.has_text ? s.text : n->text_color;
      const char* t = n->text ? n->text->data : "";
      int th = cogito_text_height_size(size);
      int ty = n->y + (n->h - th) / 2;
      int pad_x = 12;
      int cx = 0;
      int cy = 0;
      int cw = 0;
      int ch = 0;
      cogito_toast_close_rect(n, &cx, &cy, &cw, &ch);
      int text_w = (cw > 0) ? (cx - (n->x + pad_x) - 6) : (n->w - pad_x * 2);
      if (text_w < 0) text_w = 0;
      cogito_draw_text_ellipsis(n->x + pad_x, ty, text_w, t, text, size, cogito_node_bold(n), 0);
      if (cw > 0 && ch > 0) {
        int icon_size = s.has_icon_size ? s.icon_size : 12;
        if (icon_size < 8) icon_size = 8;
        int ix = cx + (cw - icon_size) / 2;
        int iy = cy + (ch - icon_size) / 2;
        Texture2D tex = {0};
        int tw = 0;
        int th_i = 0;
        Color tint = s.has_icon_color ? s.icon_color : text;
        if (cogito_icon_load_texture("x", icon_size, &tex, &tw, &th_i)) {
          float dw = (float)icon_size;
          float dh = (float)icon_size;
          if (tw > 0 && th_i > 0) {
            float sx = (float)icon_size / (float)tw;
            float sy = (float)icon_size / (float)th_i;
            float s = sx < sy ? sx : sy;
            dw = (float)tw * s;
            dh = (float)th_i * s;
          }
          float dx = ix + ((float)icon_size - dw) * 0.5f;
          float dy = iy + ((float)icon_size - dh) * 0.5f;
          DrawTexturePro(tex, (Rectangle){0, 0, (float)tw, (float)th_i},
                         (Rectangle){dx, dy, dw, dh},
                         (Vector2){0, 0}, 0.0f, tint);
        } else {
          DrawLine(ix, iy, ix + icon_size, iy + icon_size, tint);
          DrawLine(ix + icon_size, iy, ix, iy + icon_size, tint);
        }
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
        int inset = 2;
        int sr = 0;
        if (sel->border_radius > 0) sr = sel->border_radius + inset;
        cogito_draw_rect(sel->x - inset, sel->y - inset, sel->w + inset * 2, sel->h + inset * 2, n->selection_color, sr);
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
    case COGITO_TOOLBAR: {
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
    case COGITO_VIEWSWITCHER: {
      CogitoNode* active = cogito_view_switcher_active_child(n);
      if (active) cogito_draw_node(active);
      break;
    }
    case COGITO_TOASTS:
    case COGITO_VSTACK:
    case COGITO_HSTACK:
    case COGITO_ZSTACK:
    case COGITO_FIXED:
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
    case COGITO_SCROLLER: {
      if (n->len > 0) {
        BeginScissorMode(n->x, n->y, n->w, n->h);
        for (size_t i = 0; i < n->len; i++) {
          cogito_draw_node(n->children[i]);
        }
        EndScissorMode();
      }
      break;
    }
    case COGITO_SEARCHFIELD:
    case COGITO_STEPPER:
    case COGITO_SEGMENTED:
    case COGITO_TREEVIEW:
    case COGITO_TOOLTIP:
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
  int item_radius = cogito_theme.menu_item.has_radius ? cogito_theme.menu_item.radius : 0;
  for (size_t i = 0; i < cogito_menu.owner->menu_len; i++) {
    int iy = y + cogito_menu.pad_top + (int)i * cogito_menu.item_h;
    if ((int)i == hover) {
      int sel_radius = item_radius;
      if (sel_radius < 0) sel_radius = 0;
      cogito_draw_rect(x, iy, w, cogito_menu.item_h, sel, sel_radius);
    }
    const char* text = cogito_menu.owner->menu_labels[i] ? cogito_menu.owner->menu_labels[i]->data : "";
    int th = cogito_text_height_size(font_size);
    int ty = iy + (cogito_menu.item_h - th) / 2;
    cogito_draw_text_size(x + cogito_menu.pad_x, ty, text, text_color, font_size, false);
  }
}

static void cogito_draw_tooltip(CogitoNode* win, CogitoNode* target) {
  if (!win || !target || !target->tooltip || !target->tooltip->data) return;
  CogitoStyle s = cogito_theme_resolve(COGITO_TOOLTIP);
  Color bg = s.has_bg ? s.bg : cogito_rgba(30, 30, 30, 240);
  Color text = s.has_text ? s.text : cogito_rgba(255, 255, 255, 255);
  Color border = s.has_border ? s.border : cogito_rgba(0, 0, 0, 0);
  int border_w = s.has_border_width ? s.border_width : 0;
  int radius = s.has_radius ? s.radius : 6;
  int font_size = s.has_font_size ? s.font_size : cogito_font_size();
  int pad_x = s.has_padding_left ? s.padding_left : 8;
  int pad_y = s.has_padding_top ? s.padding_top : 6;

  const char* t = target->tooltip->data;
  int tw = cogito_text_width_size(t, font_size);
  int th = cogito_text_height_size(font_size);
  int w = tw + pad_x * 2;
  int h = th + pad_y * 2;

  Vector2 mp = GetMousePosition();
  int x = (int)mp.x + 12;
  int y = (int)mp.y + 16;
  if (x < 4) x = 4;
  if (x + w > win->w - 4) x = win->w - 4 - w;
  if (y + h > win->h - 4) y = win->h - 4 - h;
  if (y < 4) y = 4;

  cogito_draw_rect(x, y, w, h, bg, radius);
  if (border_w > 0) {
    cogito_draw_rect_lines(x, y, w, h, border, radius, border_w);
  }
  cogito_draw_text_size(x + pad_x, y + pad_y, t, text, font_size, false);
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
  if (win->a11y_label) {
    cogito_macos_set_window_a11y_label(win->a11y_label->data);
  }
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
    float wheel = GetMouseWheelMove();
    bool click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if (cogito_pointer_capture && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
      cogito_pointer_capture = NULL;
    }
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
    if (wheel != 0.0f) {
      CogitoNode* slot = (win->dialog && win->dialog->kind == COGITO_DIALOG_SLOT && win->dialog->dialog_open && win->dialog->dialog)
        ? win->dialog
        : cogito_find_dialog_slot(win);
      CogitoNode* root = (slot && slot->dialog) ? slot->dialog : win;
      CogitoNode* scroller = cogito_find_scroller(root, mx, my);
      if (scroller) {
        int delta = (int)(wheel * 32.0f);
        bool horiz = scroller->scroll_horz && (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT) || !scroller->scroll_vert);
        if (horiz) scroller->scroll_x -= delta;
        else scroller->scroll_y -= delta;
        cogito_window_relayout(win);
      }
    }
    if (IsKeyPressed(KEY_TAB)) {
      CogitoNode* slot = (win->dialog && win->dialog->kind == COGITO_DIALOG_SLOT && win->dialog->dialog_open && win->dialog->dialog)
        ? win->dialog
        : cogito_find_dialog_slot(win);
      CogitoNode* root = (slot && slot->dialog) ? slot->dialog : win;
      bool found = false;
      CogitoNode* next = cogito_focus_next(root, cogito_focused, &found);
      if (!next) next = cogito_focus_first(root);
      cogito_focused = next;
    }
    if (cogito_focused && !cogito_focused->disabled &&
        (cogito_focused->kind == COGITO_TEXTFIELD || cogito_focused->kind == COGITO_TEXTVIEW ||
         (cogito_focused->kind == COGITO_COLORPICKER && cogito_focused->color_hex_editing))) {
      bool changed = false;
      int ch = GetCharPressed();
      while (ch > 0) {
        if (cogito_focused->kind == COGITO_TEXTFIELD && (ch == '\n' || ch == '\r')) {
          ch = GetCharPressed();
          continue;
        }
        if (cogito_focused->kind == COGITO_COLORPICKER) {
          char c = (char)ch;
          bool ok = (c == '#') || isxdigit((unsigned char)c);
          size_t len = cogito_focused->text ? cogito_focused->text->len : 0;
          size_t sel = 0;
          if (cogito_focused->sel_start != cogito_focused->sel_end) {
            int a = cogito_focused->sel_start;
            int b = cogito_focused->sel_end;
            if (a > b) { int tmp = a; a = b; b = tmp; }
            sel = (size_t)(b - a);
          }
          size_t new_len = len - sel + 1;
          if (ok && new_len <= 7) {
            if (c == '#' && cogito_focused->caret != 0) ok = false;
            if (ok) {
              cogito_text_append_codepoint(cogito_focused, ch);
              changed = true;
            }
          }
        } else {
          cogito_text_append_codepoint(cogito_focused, ch);
          changed = true;
        }
        ch = GetCharPressed();
      }
      if (IsKeyPressed(KEY_BACKSPACE)) {
        cogito_text_backspace(cogito_focused);
        changed = true;
      }
      if (changed) {
        if (cogito_focused->kind == COGITO_COLORPICKER) {
          Color col = {0};
          const char* hex = cogito_focused->text ? cogito_focused->text->data : "";
          if (cogito_hex_to_color(hex, &col)) {
            cogito_rgb_to_hct(col, &cogito_focused->color_h, &cogito_focused->color_c, &cogito_focused->color_t);
          }
        }
        cogito_invoke_change(cogito_focused);
        cogito_window_relayout(win);
      }
    }
    if (cogito_slider_active) {
      if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        CogitoNode* sld = cogito_slider_active;
        int pad = 8;
        int x0 = sld->x + pad;
        int x1 = sld->x + sld->w - pad;
        if (x1 < x0) x1 = x0;
        double t = (x1 > x0) ? ((double)(mx - x0) / (double)(x1 - x0)) : 0.0;
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;
        double v = sld->slider_min + t * (sld->slider_max - sld->slider_min);
        if (v != sld->slider_value) {
          sld->slider_value = v;
          cogito_invoke_change(sld);
          cogito_window_relayout(win);
        }
      } else {
        if (cogito_pointer_capture == cogito_slider_active) {
          cogito_pointer_capture = NULL;
        }
        cogito_slider_active = NULL;
      }
    }
    if (cogito_colorpicker_active) {
      if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        CogitoNode* cp = cogito_colorpicker_active;
        int chan = cogito_colorpicker_channel;
        if (chan >= 0) {
          if (cogito_colorpicker_set_from_mouse(cp, chan, mx)) {
            cogito_invoke_change(cp);
            cogito_window_relayout(win);
          }
        }
      } else {
        if (cogito_pointer_capture == cogito_colorpicker_active) {
          cogito_pointer_capture = NULL;
        }
        cogito_colorpicker_active = NULL;
        cogito_colorpicker_channel = -1;
      }
    }
    if (cogito_focused && !cogito_focused->disabled &&
        (cogito_focused->kind == COGITO_TEXTFIELD || cogito_focused->kind == COGITO_TEXTVIEW ||
         (cogito_focused->kind == COGITO_COLORPICKER && cogito_focused->color_hex_editing))) {
      if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && cogito_focused->selecting) {
        int pad = 8;
        int size = cogito_node_font_size(cogito_focused);
        const char* t = cogito_focused->text ? cogito_focused->text->data : "";
        if (cogito_focused->kind == COGITO_TEXTFIELD) {
          int rel_x = mx - (cogito_focused->x + pad);
          if (rel_x < 0) rel_x = 0;
          int idx = cogito_text_index_from_x(cogito_focused, t, size, rel_x);
          cogito_focused->sel_end = idx;
          cogito_focused->caret = idx;
        } else {
          if (cogito_focused->kind == COGITO_TEXTVIEW) {
            int rel_x = mx - (cogito_focused->x + pad);
            if (rel_x < 0) rel_x = 0;
            int line_h = cogito_text_height_size(size);
            int idx = cogito_text_line_index_from_pos(cogito_focused, t, size, cogito_focused->w - pad * 2, cogito_focused->y + pad, line_h, rel_x, my);
            cogito_focused->sel_end = idx;
            cogito_focused->caret = idx;
          } else if (cogito_focused->kind == COGITO_COLORPICKER) {
            int hx = 0, hy = 0, hw = 0, hh = 0;
            cogito_colorpicker_hex_rect(cogito_focused, &hx, &hy, &hw, &hh);
            int rel_x = mx - (hx + pad);
            if (rel_x < 0) rel_x = 0;
            int idx = cogito_text_index_from_x(cogito_focused, t, size, rel_x);
            cogito_focused->sel_end = idx;
            cogito_focused->caret = idx;
          }
        }
      }
      if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        cogito_focused->selecting = false;
      }
    }
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
        if (hit && hit->disabled) hit = NULL;
        CogitoNode* root = (slot && slot->dialog) ? slot->dialog : (win->dialog_open && win->dialog ? win->dialog : win);
        CogitoNode* focus_hit = cogito_find_text_input(root, mx, my);
        if (focus_hit && focus_hit->disabled) focus_hit = NULL;
        if (focus_hit) {
          if (cogito_focused && cogito_focused != focus_hit && cogito_focused->kind == COGITO_COLORPICKER) {
            cogito_focused->color_hex_editing = false;
          }
          cogito_focused = focus_hit;
          int pad = 8;
          int size = cogito_node_font_size(focus_hit);
          const char* t = focus_hit->text ? focus_hit->text->data : "";
          if (focus_hit->kind == COGITO_TEXTFIELD) {
          int rel_x = mx - (focus_hit->x + pad);
          if (rel_x < 0) rel_x = 0;
          focus_hit->caret = cogito_text_index_from_x(focus_hit, t, size, rel_x);
            focus_hit->sel_start = focus_hit->caret;
            focus_hit->sel_end = focus_hit->caret;
            focus_hit->selecting = true;
          } else if (focus_hit->kind == COGITO_TEXTVIEW) {
            int rel_x = mx - (focus_hit->x + pad);
            if (rel_x < 0) rel_x = 0;
          int line_h = cogito_text_height_size(size);
          focus_hit->caret = cogito_text_line_index_from_pos(focus_hit, t, size, focus_hit->w - pad * 2, focus_hit->y + pad, line_h, rel_x, my);
            focus_hit->sel_start = focus_hit->caret;
            focus_hit->sel_end = focus_hit->caret;
            focus_hit->selecting = true;
          } else if (focus_hit->kind == COGITO_COLORPICKER) {
            int hx = 0, hy = 0, hw = 0, hh = 0;
            cogito_colorpicker_hex_rect(focus_hit, &hx, &hy, &hw, &hh);
            focus_hit->color_hex_editing = cogito_hit_rect(mx, my, hx, hy, hw, hh);
            int rel_x = mx - (hx + pad);
            if (rel_x < 0) rel_x = 0;
            focus_hit->caret = cogito_text_index_from_x(focus_hit, t, size, rel_x);
            focus_hit->sel_start = focus_hit->caret;
            focus_hit->sel_end = focus_hit->caret;
            focus_hit->selecting = true;
          }
        } else if (hit && cogito_node_focusable(hit)) {
          if (cogito_focused && cogito_focused != hit && cogito_focused->kind == COGITO_COLORPICKER) {
            cogito_focused->color_hex_editing = false;
          }
          cogito_focused = hit;
        } else {
          if (cogito_focused && cogito_focused->kind == COGITO_COLORPICKER) {
            cogito_focused->color_hex_editing = false;
          }
          cogito_focused = NULL;
        }
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
          } else if (hit->kind == COGITO_DROPDOWN) {
            cogito_menu_open_for(hit);
          } else if (hit->kind == COGITO_SLIDER) {
            cogito_slider_active = hit;
            cogito_pointer_capture = hit;
          } else if (hit->kind == COGITO_TABS) {
            int idx = cogito_tabs_hit_index(hit, mx, my);
            if (idx >= 0) {
              hit->selected = idx;
              if (hit->view_switcher && hit->tab_ids && (size_t)idx < hit->tab_len) {
                CogitoNode* vs = hit->view_switcher;
                ErgoStr* id = hit->tab_ids[idx];
                if (id) {
                  if (vs->view_active_id) ergo_release_val(EV_STR(vs->view_active_id));
                  vs->view_active_id = id;
                  ergo_retain_val(EV_STR(vs->view_active_id));
                }
              }
              cogito_invoke_change(hit);
              cogito_window_relayout(win);
            }
          } else if (hit->kind == COGITO_DATEPICKER) {
            int header_hit = cogito_datepicker_hit_header(hit, mx, my);
            if (header_hit < 0) {
              cogito_datepicker_shift_month(hit, -1);
            } else if (header_hit > 0) {
              cogito_datepicker_shift_month(hit, 1);
            } else {
              int day = cogito_datepicker_hit_day(hit, mx, my);
              if (day > 0) hit->date_day = day;
            }
            cogito_invoke_change(hit);
            cogito_window_relayout(win);
          } else if (hit->kind == COGITO_COLORPICKER) {
            int chan = cogito_colorpicker_hit_slider(hit, mx, my);
            if (chan >= 0) {
              cogito_colorpicker_active = hit;
              cogito_colorpicker_channel = chan;
              cogito_pointer_capture = hit;
              if (cogito_colorpicker_set_from_mouse(hit, chan, mx)) {
                cogito_invoke_change(hit);
                cogito_window_relayout(win);
              }
            }
          } else if (hit->kind == COGITO_TOAST) {
            int cx = 0, cy = 0, cw = 0, ch = 0;
            cogito_toast_close_rect(hit, &cx, &cy, &cw, &ch);
            if (cw > 0 && ch > 0 && cogito_hit_rect(mx, my, cx, cy, cw, ch)) {
              CogitoNode* parent = hit->parent;
              if (parent) {
                cogito_container_remove_child(parent, hit);
                cogito_window_relayout(win);
              }
            } else {
              cogito_invoke_click(hit);
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
    cogito_tooltip_hovered = NULL;
    cogito_draw_node(win);
    if (cogito_menu.open) cogito_draw_menu();
    if (!cogito_menu.open) {
      if (cogito_tooltip_hovered) {
        cogito_draw_tooltip(win, cogito_tooltip_hovered);
      } else {
        CogitoNode* slot = (win->dialog && win->dialog->kind == COGITO_DIALOG_SLOT && win->dialog->dialog_open && win->dialog->dialog)
          ? win->dialog
          : cogito_find_dialog_slot(win);
        CogitoNode* root = (slot && slot->dialog) ? slot->dialog : win;
        CogitoNode* tip = cogito_find_tooltip_node(root, mx, my);
        if (tip) {
          cogito_draw_tooltip(win, tip);
        }
      }
    }
    EndDrawing();
  }
  CloseWindow();
  if (cogito_font_ready && cogito_font_from_env && cogito_font.texture.id != 0 && cogito_font.glyphCount > 0) {
    UnloadFont(cogito_font);
  }
  cogito_raylib_ready = false;
  cogito_menu_close();
}
