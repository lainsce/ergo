import os
from typing import Any, Dict, List, Optional, Tuple

from .ast import (
    ArrayLit,
    Assign,
    Binary,
    Block,
    BoolLit,
    Call,
    ClassDecl,
    ConstStmt,
    EntryDecl,
    ExprStmt,
    FunDecl,
    ForEachStmt,
    ForStmt,
    FloatLit,
    Ident,
    IfArm,
    IfStmt,
    Index,
    IntLit,
    LambdaExpr,
    LetStmt,
    Member,
    MatchExpr,
    MoveExpr,
    NewExpr,
    NullLit,
    Paren,
    PatBool,
    PatIdent,
    PatInt,
    PatNull,
    PatStr,
    PatWild,
    Program,
    ReturnStmt,
    StrLit,
    Ternary,
    TupleLit,
    Unary,
)
from .typechecker import (
    Binding,
    Ctx,
    Locals,
    MODULE_CONSTS,
    STDR_PRELUDE,
    Ty,
    TypeErr,
    T_class,
    T_prim,
    build_global_env,
    module_name_for_path,
    tc_expr,  # Import separately to avoid circular import issues
    ty_from_type,
)


# Utility functions for codegen
def c_escape(s: str) -> str:
    return (
        s.replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("\n", "\\n")
        .replace("\t", "\\t")
        .replace("\r", "\\r")
    )


def _mangle_mod(name: str) -> str:
    return "".join(ch if (ch.isalnum() or ch == "_") else "_" for ch in name)


def mangle_global(mod: str, name: str) -> str:
    return f"ergo_{_mangle_mod(mod)}_{name}"


def mangle_method(mod: str, cls: str, name: str) -> str:
    return f"ergo_m_{_mangle_mod(mod)}_{cls}_{name}"


def mangle_class(mod: str, name: str) -> str:
    return f"ergo_cls_{_mangle_mod(mod)}_{name}"


def split_qname(qname: str) -> Tuple[str, str]:
    if "." not in qname:
        return ("", qname)
    return qname.split(".", 1)


RUNTIME_C = r"""// ---- Ergo runtime (minimal) ----
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>

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

static void write(ErgoVal v) {
  ErgoStr* s = stdr_to_string(v);
  fwrite(s->data, 1, s->len, stdout);
  fflush(stdout);
  ergo_release_val(EV_STR(s));
}

static void writef(ErgoVal fmt, int argc, ErgoVal* argv) {
  if (fmt.tag != EVT_STR) ergo_trap("writef expects string");
  ErgoStr* s = (ErgoStr*)fmt.as.p;
  size_t i = 0;
  int argi = 0;
  while (i < s->len) {
    if (i + 1 < s->len && s->data[i] == '{' && s->data[i + 1] == '}') {
      if (argi < argc) {
        ErgoStr* ps = stdr_to_string(argv[argi++]);
        fwrite(ps->data, 1, ps->len, stdout);
        ergo_release_val(EV_STR(ps));
      }
      i += 2;
      continue;
    }
    fputc(s->data[i], stdout);
    i++;
  }
  fflush(stdout);
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
"""


class CGen:
    def __init__(self, prog: Program):
        self.prog = prog
        self.lines: List[str] = []
        self.ind = 0
        self.tmp_id = 0
        self.var_id = 0
        self.arr_id = 0
        self.sym_id = 0
        self.lambda_id = 0

        env = build_global_env(prog)
        self.classes = env.classes
        self.funs = env.funs
        self.entry = env.entry
        self.module_names = env.module_names
        self.module_imports = env.module_imports

        self.class_decls: Dict[str, ClassDecl] = {}
        for m in prog.mods:
            mod_name = self.module_names[m.path]
            for d in m.decls:
                if isinstance(d, ClassDecl):
                    qname = f"{mod_name}.{d.name}"
                    self.class_decls[qname] = d

        self.lambda_names: Dict[int, str] = {}
        self.lambda_order: List[Tuple[LambdaExpr, str]] = []
        self.lambda_decls: List[str] = []
        self.lambda_defs: List[str] = []
        self.lambda_emitted: Dict[str, bool] = {}

        self.name_scopes: List[Dict[str, str]] = [dict()]
        self.scope_locals: List[List[str]] = [[]]
        self.ty_loc = Locals()

        self.fn_locals: List[str] = []
        self.current_module: str = ""
        self.current_imports: List[str] = []
        self.current_class: Optional[str] = None

    def w(self, s: str = ""):
        self.lines.append(("  " * self.ind) + s)

    def push_scope(self):
        self.name_scopes.append(dict())
        self.ty_loc.push()
        self.scope_locals.append([])

    def pop_scope(self) -> List[str]:
        locals_in_scope = self.scope_locals.pop()
        self.name_scopes.pop()
        self.ty_loc.pop()
        return locals_in_scope

    def cname_of(self, name: str) -> str:
        for sc in reversed(self.name_scopes):
            if name in sc:
                return sc[name]
        raise TypeErr(f"codegen: unknown local '{name}'")

    def define_local(self, name: str, ty: Ty, is_mut: bool, is_const: bool) -> str:
        self.var_id += 1
        c = f"{name}__{self.var_id}"
        self.name_scopes[-1][name] = c
        self.ty_loc.define(name, Binding(ty=ty, is_mut=is_mut, is_const=is_const))
        self.scope_locals[-1].append(c)
        return c

    def release_scope(self, locals_in_scope: List[str]):
        for c in reversed(locals_in_scope):
            self.w(f"ergo_release_val({c});")

    def new_tmp(self) -> str:
        self.tmp_id += 1
        return f"__t{self.tmp_id}"

    def new_sym(self, base: str) -> str:
        self.sym_id += 1
        return f"__{base}{self.sym_id}"

    def new_lambda_name(self) -> str:
        self.lambda_id += 1
        return f"ergo_lambda_{self.lambda_id}"

    def c_class_name(self, qname: str) -> str:
        mod, name = split_qname(qname)
        return f"ErgoObj_{_mangle_mod(mod)}_{name}" if mod else f"ErgoObj_{name}"

    def c_field_name(self, name: str) -> str:
        return f"f_{name}"

    def ctx_for(self, path: str) -> Ctx:
        mod = self.current_module or self.module_names.get(path, module_name_for_path(path))
        imports = self.current_imports or self.module_imports.get(mod, [])
        return Ctx(
            module_path=path,
            module_name=mod,
            imports=imports,
            current_class=self.current_class,
        )

    def module_in_scope(self, name: str) -> bool:
        if self.ty_loc.lookup(name):
            return False
        if name == self.current_module:
            return True
        return name in self.current_imports

    def w_raw(self, block: str):
        self.lines.extend(block.splitlines())

    def collect_lambdas(self):
        def visit_expr(e: Any, path: str):
            if isinstance(e, LambdaExpr):
                lid = id(e)
                if lid not in self.lambda_names:
                    name = self.new_lambda_name()
                    self.lambda_names[lid] = name
                    self.lambda_order.append((e, path))
                visit_expr(e.body, path)
                return

            if isinstance(e, Unary):
                visit_expr(e.x, path)
            elif isinstance(e, Binary):
                visit_expr(e.a, path)
                visit_expr(e.b, path)
            elif isinstance(e, Assign):
                visit_expr(e.target, path)
                visit_expr(e.value, path)
            elif isinstance(e, Call):
                visit_expr(e.fn, path)
                for a in e.args:
                    visit_expr(a, path)
            elif isinstance(e, Index):
                visit_expr(e.a, path)
                visit_expr(e.i, path)
            elif isinstance(e, Member):
                visit_expr(e.a, path)
            elif isinstance(e, Paren):
                visit_expr(e.x, path)
            elif isinstance(e, Ternary):
                visit_expr(e.cond, path)
                visit_expr(e.a, path)
                visit_expr(e.b, path)
            elif isinstance(e, MoveExpr):
                visit_expr(e.x, path)
            elif isinstance(e, ArrayLit):
                for it in e.items:
                    visit_expr(it, path)
            elif isinstance(e, TupleLit):
                for it in e.items:
                    visit_expr(it, path)
            elif isinstance(e, MatchExpr):
                visit_expr(e.scrut, path)
                for arm in e.arms:
                    visit_expr(arm.expr, path)
            elif isinstance(e, NewExpr):
                for a in e.args:
                    visit_expr(a, path)

        def visit_stmt(s: Any, path: str):
            if isinstance(s, LetStmt):
                visit_expr(s.expr, path)
            elif isinstance(s, ConstStmt):
                visit_expr(s.expr, path)
            elif isinstance(s, ExprStmt):
                visit_expr(s.expr, path)
            elif isinstance(s, ReturnStmt):
                if s.expr:
                    visit_expr(s.expr, path)
            elif isinstance(s, IfStmt):
                for arm in s.arms:
                    if arm.cond:
                        visit_expr(arm.cond, path)
                    visit_stmt(arm.body, path)
            elif isinstance(s, ForStmt):
                if s.init:
                    visit_stmt(s.init, path)
                if s.cond:
                    visit_expr(s.cond, path)
                if s.step:
                    visit_expr(s.step, path)
                visit_stmt(s.body, path)
            elif isinstance(s, ForEachStmt):
                visit_expr(s.expr, path)
                visit_stmt(s.body, path)
            elif isinstance(s, Block):
                for st in s.stmts:
                    visit_stmt(st, path)

        for m in self.prog.mods:
            for d in m.decls:
                if isinstance(d, FunDecl):
                    visit_stmt(d.body, m.path)
                elif isinstance(d, ClassDecl):
                    for md in d.methods:
                        visit_stmt(md.body, m.path)
                elif isinstance(d, EntryDecl):
                    visit_stmt(d.body, m.path)

    def emit_lambda_def(self, lam: LambdaExpr, path: str):
        lname = self.lambda_names.get(id(lam))
        if lname is None:
            lname = self.new_lambda_name()
            self.lambda_names[id(lam)] = lname
            self.lambda_order.append((lam, path))

        if self.lambda_emitted.get(lname):
            return lname

        self.lambda_emitted[lname] = True
        self.lambda_decls.append(
            f"static ErgoVal {lname}(void* env, int argc, ErgoVal* argv);"
        )

        saved_lines = self.lines
        saved_ind = self.ind
        saved_scopes = self.name_scopes
        saved_scope_locals = self.scope_locals
        saved_loc = self.ty_loc
        saved_fn_locals = self.fn_locals
        saved_module = self.current_module
        saved_imports = self.current_imports
        saved_class = self.current_class

        self.lines = []
        self.ind = 0
        self.name_scopes = [dict()]
        self.scope_locals = [[]]
        self.ty_loc = Locals()
        self.fn_locals = []
        self.current_module = self.module_names.get(path, module_name_for_path(path))
        self.current_imports = self.module_imports.get(self.current_module, [])
        self.current_class = None

        self.w(f"static ErgoVal {lname}(void* env, int argc, ErgoVal* argv) {{")
        self.ind += 1
        self.w("(void)env;")
        self.w(f"if (argc != {len(lam.params)}) ergo_trap(\"lambda arity mismatch\");")

        ctx = self.ctx_for(path)
        for i, p in enumerate(lam.params):
            cname = f"arg{i}"
            self.w(f"ErgoVal {cname} = argv[{i}];")
            self.name_scopes[-1][p.name] = cname
            if p.typ is not None:
                ty = ty_from_type(p.typ, self.classes, ctx.module_name, ctx.imports)
            else:
                ty = Ty("gen", name=f"_{p.name}_{i}")
            self.ty_loc.define(p.name, Binding(ty=ty, is_mut=p.is_mut, is_const=False))

        self.push_scope()
        self.w("ErgoVal __ret = EV_NULLV;")
        tmp, cleanup = self.gen_expr(path, lam.body)
        self.w(f"ergo_move_into(&__ret, {tmp});")
        for z in cleanup:
            if z != tmp:
                self.w(f"ergo_release_val({z});")
        self.release_scope(self.pop_scope())
        self.w("return __ret;")
        self.ind -= 1
        self.w("}")
        self.w("")

        self.lambda_defs.append("\n".join(self.lines))

        self.lines = saved_lines
        self.ind = saved_ind
        self.name_scopes = saved_scopes
        self.scope_locals = saved_scope_locals
        self.ty_loc = saved_loc
        self.fn_locals = saved_fn_locals
        self.current_module = saved_module
        self.current_imports = saved_imports
        self.current_class = saved_class

        return lname

    def gen(self) -> str:
        self.collect_lambdas()
        for lam, path in self.lambda_order:
            self.emit_lambda_def(lam, path)

        self.w(RUNTIME_C)

        self.w("// ---- class definitions ----")
        self.gen_class_defs()
        self.w("")

        if self.lambda_decls:
            self.w("// ---- lambda forward decls ----")
            for d in self.lambda_decls:
                self.w(d)
            self.w("")

        self.w("// ---- forward decls ----")
        for m in self.prog.mods:
            mod_name = self.module_names[m.path]
            for d in m.decls:
                if isinstance(d, ClassDecl):
                    for md in d.methods:
                        r = "void" if md.ret.is_void else "ErgoVal"
                        self.w(
                            f"static {r} {mangle_method(mod_name, d.name, md.name)}("
                            f"ErgoVal self{self.c_params(md.params[1:], leading_comma=True)});"
                        )
                if isinstance(d, FunDecl):
                    r = "void" if d.ret.is_void else "ErgoVal"
                    self.w(
                        f"static {r} {mangle_global(mod_name, d.name)}({self.c_params(d.params)});"
                    )
        self.w("static void ergo_entry(void);")
        self.w("")

        if self.lambda_defs:
            self.w("// ---- lambda defs ----")
            for block in self.lambda_defs:
                self.w_raw(block)
            self.w("")

        self.w("// ---- compiled functions ----")
        for m in self.prog.mods:
            mod_name = self.module_names[m.path]
            self.current_module = mod_name
            self.current_imports = self.module_imports.get(mod_name, [])
            self.current_class = None
            for d in m.decls:
                if isinstance(d, ClassDecl):
                    for md in d.methods:
                        self.gen_method(m.path, d, md)
                if isinstance(d, FunDecl):
                    self.gen_fun(m.path, d)

        self.w("// ---- entry ----")
        self.gen_entry()
        self.w("int main(void) {")
        self.ind += 1
        self.w("ergo_entry();")
        self.w("return 0;")
        self.ind -= 1
        self.w("}")

        return "\n".join(self.lines)

    def gen_class_defs(self):
        for qname, decl in self.class_decls.items():
            mod, name = split_qname(qname)
            cname = self.c_class_name(qname)
            drop_sym = f"ergo_drop_{_mangle_mod(mod)}_{name}" if mod else f"ergo_drop_{name}"
            self.w(f"typedef struct {cname} {{")
            self.ind += 1
            self.w("ErgoObj base;")
            for f in decl.fields:
                self.w(f"ErgoVal {self.c_field_name(f.name)};")
            self.ind -= 1
            self.w(f"}} {cname};")
            self.w(f"static void {drop_sym}(ErgoObj* o);")
            self.w("")

        for qname, decl in self.class_decls.items():
            mod, name = split_qname(qname)
            cname = self.c_class_name(qname)
            drop_sym = f"ergo_drop_{_mangle_mod(mod)}_{name}" if mod else f"ergo_drop_{name}"
            self.w(f"static void {drop_sym}(ErgoObj* o) {{")
            self.ind += 1
            self.w(f"{cname}* self = ({cname}*)o;")
            for f in decl.fields:
                self.w(f"ergo_release_val(self->{self.c_field_name(f.name)});")
            self.ind -= 1
            self.w("}")
            self.w("")

    def c_params(self, params: List[Any], leading_comma: bool = False) -> str:
        out = []
        for i, _p in enumerate(params):
            out.append(f"ErgoVal a{i}")
        if not out:
            return "" if leading_comma else "void"
        joined = ", ".join(out)
        return f", {joined}" if leading_comma else joined

    def gen_method(self, path: str, cls: ClassDecl, fn: FunDecl):
        self.fn_locals = []
        self.name_scopes = [dict()]
        self.scope_locals = [[]]
        self.ty_loc = Locals()
        qname = f"{self.current_module}.{cls.name}"
        self.current_class = qname

        # receiver
        self.name_scopes[-1]["this"] = "self"
        recv_mut = fn.params[0].is_mut if fn.params else False
        self.ty_loc.define(
            "this",
            Binding(
                ty=T_class(qname),
                is_mut=recv_mut,
                is_const=False,
            ),
        )

        # params after this
        for i, p in enumerate(fn.params[1:]):
            assert p.typ is not None
            ty = ty_from_type(p.typ, self.classes, self.current_module, self.current_imports)
            self.name_scopes[-1][p.name] = f"a{i}"
            self.ty_loc.define(p.name, Binding(ty=ty, is_mut=p.is_mut, is_const=False))

        ret_void = fn.ret.is_void
        r = "void" if ret_void else "ErgoVal"
        self.w(
            f"static {r} {mangle_method(self.current_module, cls.name, fn.name)}("
            f"ErgoVal self{self.c_params(fn.params[1:], leading_comma=True)}) {{"
        )
        self.ind += 1
        if not ret_void:
            self.w("ErgoVal __ret = EV_NULLV;")

        self.push_scope()
        self.gen_block(path, fn.body, ret_void)
        self.release_scope(self.pop_scope())

        if not ret_void:
            self.w("return __ret;")
        self.ind -= 1
        self.w("}")
        self.w("")
        self.current_class = None

    def gen_fun(self, path: str, fn: FunDecl):
        self.fn_locals = []
        self.name_scopes = [dict()]
        self.scope_locals = [[]]
        self.ty_loc = Locals()
        self.current_class = None

        for i, p in enumerate(fn.params):
            assert p.typ is not None
            ty = ty_from_type(p.typ, self.classes, self.current_module, self.current_imports)
            self.name_scopes[-1][p.name] = f"a{i}"
            self.ty_loc.define(p.name, Binding(ty=ty, is_mut=p.is_mut, is_const=False))

        ret_void = fn.ret.is_void
        r = "void" if ret_void else "ErgoVal"
        self.w(
            f"static {r} {mangle_global(self.current_module, fn.name)}({self.c_params(fn.params)}) {{"
        )
        self.ind += 1
        if not ret_void:
            self.w("ErgoVal __ret = EV_NULLV;")

        self.push_scope()
        self.gen_block(path, fn.body, ret_void)
        self.release_scope(self.pop_scope())

        if not ret_void:
            self.w("return __ret;")
        self.ind -= 1
        self.w("}")
        self.w("")

    def gen_entry(self):
        entry_decl: Optional[EntryDecl] = None
        entry_path: Optional[str] = None
        for m in self.prog.mods:
            for d in m.decls:
                if isinstance(d, EntryDecl):
                    entry_decl = d
                    entry_path = m.path
        if entry_decl is None:
            raise TypeErr("missing entry()")

        self.fn_locals = []
        self.name_scopes = [dict()]
        self.scope_locals = [[]]
        self.ty_loc = Locals()
        if entry_path:
            mod_name = self.module_names[entry_path]
            self.current_module = mod_name
            self.current_imports = self.module_imports.get(mod_name, [])
            self.current_class = None

        self.w("static void ergo_entry(void) {")
        self.ind += 1
        self.push_scope()
        self.gen_block(entry_path or "<init>", entry_decl.body, True)
        self.release_scope(self.pop_scope())
        self.ind -= 1
        self.w("}")
        self.w("")

    def gen_block(self, path: str, b: Block, ret_void: bool):
        for st in b.stmts:
            self.gen_stmt(path, st, ret_void)

    def gen_if_chain(self, path: str, arms: List[IfArm], idx: int, ret_void: bool):
        if idx >= len(arms):
            return

        arm = arms[idx]
        if arm.cond is None:
            if isinstance(arm.body, Block):
                self.gen_block(path, arm.body, ret_void)
            else:
                self.gen_stmt(path, arm.body, ret_void)
            return

        ctmp, cleanup = self.gen_expr(path, arm.cond)

        self.var_id += 1
        bname = f"__b{self.var_id}"
        self.w(f"bool {bname} = ergo_as_bool({ctmp});")
        self.w(f"ergo_release_val({ctmp});")
        for z in cleanup:
            if z != ctmp:
                self.w(f"ergo_release_val({z});")

        self.w(f"if ({bname}) {{")
        self.ind += 1
        self.push_scope()
        if isinstance(arm.body, Block):
            self.gen_block(path, arm.body, ret_void)
        else:
            self.gen_stmt(path, arm.body, ret_void)
        self.release_scope(self.pop_scope())
        self.ind -= 1

        if idx + 1 < len(arms):
            self.w("} else {")
            self.ind += 1
            self.gen_if_chain(path, arms, idx + 1, ret_void)
            self.ind -= 1
            self.w("}")
        else:
            self.w("}")

    def gen_stmt(self, path: str, s: Any, ret_void: bool):
        if isinstance(s, LetStmt):
            ty = tc_expr(
                s.expr, self.ctx_for(path), self.ty_loc, self.classes, self.funs
            )
            cvar = self.define_local(s.name, ty, s.is_mut, False)
            self.w(f"ErgoVal {cvar} = EV_NULLV;")
            tmp, cleanup = self.gen_expr(path, s.expr)
            self.w(f"ergo_move_into(&{cvar}, {tmp});")
            for t in cleanup:
                if t != tmp:
                    self.w(f"ergo_release_val({t});")
            return

        if isinstance(s, ConstStmt):
            ty = tc_expr(
                s.expr, self.ctx_for(path), self.ty_loc, self.classes, self.funs
            )
            cvar = self.define_local(s.name, ty, False, True)
            self.w(f"ErgoVal {cvar} = EV_NULLV;")
            tmp, cleanup = self.gen_expr(path, s.expr)
            self.w(f"ergo_move_into(&{cvar}, {tmp});")
            for t in cleanup:
                if t != tmp:
                    self.w(f"ergo_release_val({t});")
            return

        if isinstance(s, ExprStmt):
            tmp, cleanup = self.gen_expr(path, s.expr)
            self.w(f"ergo_release_val({tmp});")
            for t in cleanup:
                if t != tmp:
                    self.w(f"ergo_release_val({t});")
            return

        if isinstance(s, ReturnStmt):
            if ret_void:
                if s.expr is not None:
                    tmp, cleanup = self.gen_expr(path, s.expr)
                    self.w(f"ergo_release_val({tmp});")
                    for t in cleanup:
                        if t != tmp:
                            self.w(f"ergo_release_val({t});")
                self.w("return;")
            else:
                if s.expr is None:
                    self.w("__ret = EV_NULLV;")
                else:
                    tmp, cleanup = self.gen_expr(path, s.expr)
                    self.w(f"ergo_move_into(&__ret, {tmp});")
                    for t in cleanup:
                        if t != tmp:
                            self.w(f"ergo_release_val({t});")
                self.w("return __ret;")
            return

        if isinstance(s, IfStmt):
            self.gen_if_chain(path, s.arms, 0, ret_void)
            return

        if isinstance(s, ForStmt):
            if s.init is not None:
                self.gen_stmt(path, s.init, ret_void)

            self.w("for (;;) {")
            self.ind += 1

            if s.cond is not None:
                ct, cc = self.gen_expr(path, s.cond)
                self.var_id += 1
                bname = f"__b{self.var_id}"
                self.w(f"bool {bname} = ergo_as_bool({ct});")
                self.w(f"ergo_release_val({ct});")
                for z in cc:
                    if z != ct:
                        self.w(f"ergo_release_val({z});")
                self.w(f"if (!{bname}) {{")
                self.ind += 1
                self.w("break;")
                self.ind -= 1
                self.w("}")

            self.push_scope()
            if isinstance(s.body, Block):
                self.gen_block(path, s.body, ret_void)
            else:
                self.gen_stmt(path, s.body, ret_void)
            self.release_scope(self.pop_scope())

            if s.step is not None:
                st, sc = self.gen_expr(path, s.step)
                self.w(f"ergo_release_val({st});")
                for z in sc:
                    if z != st:
                        self.w(f"ergo_release_val({z});")

            self.ind -= 1
            self.w("}")
            return

        if isinstance(s, ForEachStmt):
            it, ic = self.gen_expr(path, s.expr)
            idx_name = self.new_sym("i")
            len_name = self.new_sym("len")

            # element binding
            elem_ty = tc_expr(
                s.expr, self.ctx_for(path), self.ty_loc, self.classes, self.funs
            )
            if elem_ty.tag == "array" and elem_ty.elem:
                ety = elem_ty.elem
                get_expr = f"ergo_arr_get((ErgoArr*){it}.as.p, {idx_name})"
            elif elem_ty.tag == "prim" and elem_ty.name == "string":
                ety = T_prim("string")
                get_expr = f"stdr_str_at({it}, {idx_name})"
            else:
                raise TypeErr(f"{path}: foreach expects array or string")

            self.push_scope()
            cvar = self.define_local(s.name, ety, False, False)
            self.w(f"ErgoVal {cvar} = EV_NULLV;")

            # precompute length; stdr_len handles arrays/strings
            self.w(f"int {len_name} = stdr_len({it});")
            self.w(f"for (int {idx_name} = 0; {idx_name} < {len_name}; {idx_name}++) {{")
            self.ind += 1
            self.push_scope()

            self.w(f"ErgoVal __e = {get_expr};")
            self.w(f"ergo_move_into(&{cvar}, __e);")

            if isinstance(s.body, Block):
                self.gen_block(path, s.body, ret_void)
            else:
                self.gen_stmt(path, s.body, ret_void)

            self.release_scope(self.pop_scope())
            self.ind -= 1
            self.w("}")

            self.release_scope(self.pop_scope())
            self.w(f"ergo_release_val({it});")
            for z in ic:
                if z != it:
                    self.w(f"ergo_release_val({z});")
            return

        if isinstance(s, Block):
            self.w("{")
            self.ind += 1
            self.push_scope()
            self.gen_block(path, s, ret_void)
            self.release_scope(self.pop_scope())
            self.ind -= 1
            self.w("}")
            return

        raise TypeErr(f"{path}: unhandled stmt {type(s).__name__}")

    def gen_expr(self, path: str, e: Any) -> Tuple[str, List[str]]:
        cleanup: List[str] = []

        if isinstance(e, IntLit):
            t = self.new_tmp()
            self.w(f"ErgoVal {t} = EV_INT({e.v});")
            cleanup.append(t)
            return t, cleanup

        if isinstance(e, FloatLit):
            t = self.new_tmp()
            self.w(f"ErgoVal {t} = EV_FLOAT({e.v});")
            cleanup.append(t)
            return t, cleanup

        if isinstance(e, BoolLit):
            t = self.new_tmp()
            self.w(f"ErgoVal {t} = EV_BOOL({'true' if e.v else 'false'});")
            cleanup.append(t)
            return t, cleanup

        if isinstance(e, NullLit):
            t = self.new_tmp()
            self.w(f"ErgoVal {t} = EV_NULLV;")
            cleanup.append(t)
            return t, cleanup

        if isinstance(e, StrLit):
            parts_tmps: List[str] = []
            for kind, val in e.parts:
                pt = self.new_tmp()
                if kind == "text":
                    self.w(f'ErgoVal {pt} = EV_STR(stdr_str_lit("{c_escape(val)}"));')
                else:
                    cname = self.cname_of(val)
                    self.w(f"ErgoVal {pt} = {cname}; ergo_retain_val({pt});")
                parts_tmps.append(pt)

            parts_name = self.new_sym("parts")
            s_name = self.new_sym("s")

            arr = self.new_tmp()
            self.w(f"ErgoVal {arr} = EV_NULLV;")
            self.w("{")
            self.ind += 1
            self.w(
                f"ErgoVal {parts_name}[{len(parts_tmps)}] = {{ "
                + ", ".join(parts_tmps)
                + " };"
            )
            self.w(
                f"ErgoStr* {s_name} = stdr_str_from_parts({len(parts_tmps)}, {parts_name});"
            )
            self.w(f"{arr} = EV_STR({s_name});")
            self.ind -= 1
            self.w("}")
            for pt in parts_tmps:
                self.w(f"ergo_release_val({pt});")

            cleanup.append(arr)
            return arr, cleanup

        if isinstance(e, TupleLit):
            self.arr_id += 1
            arrsym = f"__tup{self.arr_id}"
            t = self.new_tmp()
            self.w(f"ErgoArr* {arrsym} = stdr_arr_new({len(e.items)});")
            self.w(f"ErgoVal {t} = EV_ARR({arrsym});")
            for it in e.items:
                vt, vc = self.gen_expr(path, it)
                self.w(f"ergo_arr_add({arrsym}, {vt});")
                for z in vc:
                    if z != vt:
                        self.w(f"ergo_release_val({z});")
            cleanup.append(t)
            return t, cleanup

        if isinstance(e, Ident):
            t = self.new_tmp()
            cname = self.cname_of(e.name)
            self.w(f"ErgoVal {t} = {cname}; ergo_retain_val({t});")
            cleanup.append(t)
            return t, cleanup

        if isinstance(e, Member):
            base_ty = tc_expr(
                e.a, self.ctx_for(path), self.ty_loc, self.classes, self.funs
            )
            if base_ty.tag == "mod":
                mod_consts = MODULE_CONSTS.get(base_ty.name, {})
                if e.name in mod_consts:
                    cv = mod_consts[e.name]
                    t = self.new_tmp()
                    if cv.ty.tag == "prim" and cv.ty.name == "num":
                        if isinstance(cv.value, float):
                            self.w(f"ErgoVal {t} = EV_FLOAT({cv.value});")
                        else:
                            self.w(f"ErgoVal {t} = EV_INT({cv.value});")
                    elif cv.ty.tag == "prim" and cv.ty.name == "bool":
                        self.w(
                            f"ErgoVal {t} = EV_BOOL({'true' if cv.value else 'false'});"
                        )
                    elif cv.ty.tag == "prim" and cv.ty.name == "string":
                        self.w(
                            f'ErgoVal {t} = EV_STR(stdr_str_lit("{c_escape(str(cv.value))}"));'
                        )
                    elif cv.ty.tag == "null":
                        self.w(f"ErgoVal {t} = EV_NULLV;")
                    else:
                        raise TypeErr(
                            f"{path}: unsupported const type for '{base_ty.name}.{e.name}'"
                        )
                    cleanup.append(t)
                    return t, cleanup
                raise TypeErr(
                    f"{path}: unknown module member '{base_ty.name}.{e.name}'"
                )

            if base_ty.tag == "class":
                bt, bc = self.gen_expr(path, e.a)
                t = self.new_tmp()
                cname = self.c_class_name(base_ty.name)
                field = self.c_field_name(e.name)
                self.w(
                    f"ErgoVal {t} = (({cname}*){bt}.as.p)->{field}; ergo_retain_val({t});"
                )
                self.w(f"ergo_release_val({bt});")
                for z in bc:
                    if z != bt:
                        self.w(f"ergo_release_val({z});")
                cleanup.append(t)
                return t, cleanup

            raise TypeErr(f"{path}: member access not supported on this type")

        if isinstance(e, MoveExpr):
            if not isinstance(e.x, Ident):
                raise TypeErr(f"{path}: move(...) must be an identifier in v0 codegen")
            slot = self.cname_of(e.x.name)
            t = self.new_tmp()
            self.w(f"ErgoVal {t} = ergo_move(&{slot});")
            cleanup.append(t)
            return t, cleanup

        if isinstance(e, MatchExpr):
            scrut_ty = tc_expr(
                e.scrut, self.ctx_for(path), self.ty_loc, self.classes, self.funs
            )
            scrut, sc = self.gen_expr(path, e.scrut)
            t = self.new_tmp()
            matched = self.new_sym("matched")
            self.w(f"ErgoVal {t} = EV_NULLV;")
            self.w(f"bool {matched} = false;")

            for arm in e.arms:
                cond_name = self.new_sym("mc")
                bind_name: Optional[str] = None

                if isinstance(arm.pat, PatWild):
                    self.w(f"bool {cond_name} = true;")
                elif isinstance(arm.pat, PatIdent):
                    bind_name = arm.pat.name
                    self.w(f"bool {cond_name} = true;")
                else:
                    # build pattern value
                    if isinstance(arm.pat, PatInt):
                        pv = self.new_tmp()
                        self.w(f"ErgoVal {pv} = EV_INT({arm.pat.v});")
                    elif isinstance(arm.pat, PatBool):
                        pv = self.new_tmp()
                        self.w(
                            f"ErgoVal {pv} = EV_BOOL({'true' if arm.pat.v else 'false'});"
                        )
                    elif isinstance(arm.pat, PatNull):
                        pv = self.new_tmp()
                        self.w(f"ErgoVal {pv} = EV_NULLV;")
                    elif isinstance(arm.pat, PatStr):
                        pv, pc = self.gen_expr(path, StrLit(parts=arm.pat.parts))
                        for z in pc:
                            if z != pv:
                                self.w(f"ergo_release_val({z});")
                    else:
                        raise TypeErr(f"{path}: unsupported match pattern in codegen")

                    eqt = self.new_tmp()
                    self.w(f"ErgoVal {eqt} = ergo_eq({scrut}, {pv});")
                    self.w(f"bool {cond_name} = ergo_as_bool({eqt});")
                    self.w(f"ergo_release_val({eqt});")
                    self.w(f"ergo_release_val({pv});")

                self.w(f"if (!{matched} && {cond_name}) {{")
                self.ind += 1
                self.w(f"{matched} = true;")
                if bind_name is not None:
                    btmp = self.new_tmp()
                    self.w(f"ErgoVal {btmp} = {scrut}; ergo_retain_val({btmp});")
                    self.name_scopes.append({bind_name: btmp})
                    self.ty_loc.push()
                    self.ty_loc.define(
                        bind_name, Binding(ty=scrut_ty, is_mut=False, is_const=False)
                    )

                at, ac = self.gen_expr(path, arm.expr)
                self.w(f"ergo_move_into(&{t}, {at});")
                for z in ac:
                    if z != at:
                        self.w(f"ergo_release_val({z});")
                if bind_name is not None:
                    self.name_scopes.pop()
                    self.ty_loc.pop()
                    self.w(f"ergo_release_val({btmp});")
                self.ind -= 1
                self.w("}")

            self.w(f"ergo_release_val({scrut});")
            for z in sc:
                if z != scrut:
                    self.w(f"ergo_release_val({z});")
            cleanup.append(t)
            return t, cleanup

        if isinstance(e, LambdaExpr):
            lname = self.lambda_names.get(id(e))
            if lname is None:
                lname = self.emit_lambda_def(e, path)
            t = self.new_tmp()
            self.w(f"ErgoVal {t} = EV_FN(ergo_fn_new({lname}, {len(e.params)}));")
            cleanup.append(t)
            return t, cleanup

        if isinstance(e, NewExpr):
            if "." in e.name:
                mod, _ = e.name.split(".", 1)
                if mod != self.current_module and mod not in self.current_imports:
                    raise TypeErr(f"{path}: unknown class '{e.name}'")
                qname = e.name
            else:
                qname = f"{self.current_module}.{e.name}"
            if qname not in self.class_decls:
                raise TypeErr(f"{path}: unknown class '{e.name}'")
            decl = self.class_decls[qname]
            mod, cname_short = split_qname(qname)
            cname = self.c_class_name(qname)
            drop_sym = f"ergo_drop_{_mangle_mod(mod)}_{cname_short}" if mod else f"ergo_drop_{cname_short}"
            obj_name = self.new_sym("obj")
            self.w(
                f"{cname}* {obj_name} = ({cname}*)ergo_obj_new(sizeof({cname}), {drop_sym});"
            )
            for f in decl.fields:
                self.w(f"{obj_name}->{self.c_field_name(f.name)} = EV_NULLV;")
            t = self.new_tmp()
            self.w(f"ErgoVal {t} = EV_OBJ({obj_name});")

            if "init" in self.classes[qname].methods:
                arg_ts: List[str] = []
                all_cleanup: List[str] = []
                for a in e.args:
                    at, ac = self.gen_expr(path, a)
                    arg_ts.append(at)
                    all_cleanup.extend(ac)
                self.w(
                    f"{mangle_method(mod, cname_short, 'init')}("
                    + ", ".join([t] + arg_ts)
                    + ");"
                )
                for z in all_cleanup:
                    self.w(f"ergo_release_val({z});")

            cleanup.append(t)
            return t, cleanup

        if isinstance(e, Unary):
            xt, xc = self.gen_expr(path, e.x)
            t = self.new_tmp()
            if e.op == "!":
                self.w(f"ErgoVal {t} = EV_BOOL(!ergo_as_bool({xt}));")
            elif e.op == "-":
                xty = tc_expr(
                    e.x, self.ctx_for(path), self.ty_loc, self.classes, self.funs
                )
                if xty.tag == "prim" and xty.name == "num":
                    self.w(f"ErgoVal {t} = ergo_neg({xt});")
                else:
                    self.w(f"ErgoVal {t} = EV_INT(-ergo_as_int({xt}));")
            else:
                raise TypeErr(f"{path}: unary op {e.op} not supported in CGen")
            self.w(f"ergo_release_val({xt});")
            for z in xc:
                if z != xt:
                    self.w(f"ergo_release_val({z});")
            cleanup.append(t)
            return t, cleanup

        if isinstance(e, Binary) and e.op not in ("&&", "||"):
            at, ac = self.gen_expr(path, e.a)
            bt, bc = self.gen_expr(path, e.b)
            t = self.new_tmp()
            opmap = {
                "+": "ergo_add",
                "-": "ergo_sub",
                "*": "ergo_mul",
                "/": "ergo_div",
                "%": "ergo_mod",
                "==": "ergo_eq",
                "!=": "ergo_ne",
                "<": "ergo_lt",
                "<=": "ergo_le",
                ">": "ergo_gt",
                ">=": "ergo_ge",
            }
            if e.op not in opmap:
                raise TypeErr(f"{path}: binary op {e.op} not supported")
            self.w(f"ErgoVal {t} = {opmap[e.op]}({at}, {bt});")
            self.w(f"ergo_release_val({at});")
            self.w(f"ergo_release_val({bt});")
            for z in ac:
                if z != at:
                    self.w(f"ergo_release_val({z});")
            for z in bc:
                if z != bt:
                    self.w(f"ergo_release_val({z});")
            cleanup.append(t)
            return t, cleanup

        if isinstance(e, Binary) and e.op in ("&&", "||"):
            lt, lc = self.gen_expr(path, e.a)
            t = self.new_tmp()
            self.w(f"ErgoVal {t} = EV_BOOL(false);")
            if e.op == "&&":
                self.w(f"if (ergo_as_bool({lt})) {{")
                self.ind += 1
                rt, rc = self.gen_expr(path, e.b)
                self.w(f"ergo_move_into(&{t}, EV_BOOL(ergo_as_bool({rt})));")
                self.w(f"ergo_release_val({rt});")
                for z in rc:
                    if z != rt:
                        self.w(f"ergo_release_val({z});")
                self.ind -= 1
                self.w("} else {")
                self.ind += 1
                self.w(f"ergo_move_into(&{t}, EV_BOOL(false));")
                self.ind -= 1
                self.w("}")
            else:
                self.w(f"if (ergo_as_bool({lt})) {{")
                self.ind += 1
                self.w(f"ergo_move_into(&{t}, EV_BOOL(true));")
                self.ind -= 1
                self.w("} else {")
                self.ind += 1
                rt, rc = self.gen_expr(path, e.b)
                self.w(f"ergo_move_into(&{t}, EV_BOOL(ergo_as_bool({rt})));")
                self.w(f"ergo_release_val({rt});")
                for z in rc:
                    if z != rt:
                        self.w(f"ergo_release_val({z});")
                self.ind -= 1
                self.w("}")
            self.w(f"ergo_release_val({lt});")
            for z in lc:
                if z != lt:
                    self.w(f"ergo_release_val({z});")
            cleanup.append(t)
            return t, cleanup

        if isinstance(e, ArrayLit):
            self.arr_id += 1
            arrsym = f"__a{self.arr_id}"
            t = self.new_tmp()
            self.w(f"ErgoArr* {arrsym} = stdr_arr_new({len(e.items)});")
            self.w(f"ErgoVal {t} = EV_ARR({arrsym});")
            for it in e.items:
                vt, vc = self.gen_expr(path, it)
                self.w(f"ergo_arr_add({arrsym}, {vt});")
                for z in vc:
                    if z != vt:
                        self.w(f"ergo_release_val({z});")
            cleanup.append(t)
            return t, cleanup

        if isinstance(e, Index):
            base_ty = tc_expr(
                e.a, self.ctx_for(path), self.ty_loc, self.classes, self.funs
            )
            at, ac = self.gen_expr(path, e.a)
            it, ic = self.gen_expr(path, e.i)
            t = self.new_tmp()
            if base_ty.tag == "prim" and base_ty.name == "string":
                self.w(f"ErgoVal {t} = stdr_str_at({at}, ergo_as_int({it}));")
            else:
                self.w(
                    f"ErgoVal {t} = ergo_arr_get((ErgoArr*){at}.as.p, ergo_as_int({it}));"
                )
            self.w(f"ergo_release_val({at});")
            self.w(f"ergo_release_val({it});")
            for z in ac:
                if z != at:
                    self.w(f"ergo_release_val({z});")
            for z in ic:
                if z != it:
                    self.w(f"ergo_release_val({z});")
            cleanup.append(t)
            return t, cleanup

        if isinstance(e, Ternary):
            ct, cc = self.gen_expr(path, e.cond)
            t = self.new_tmp()
            self.w(f"ErgoVal {t} = EV_NULLV;")
            self.w(f"if (ergo_as_bool({ct})) {{")
            self.ind += 1
            at, ac = self.gen_expr(path, e.a)
            self.w(f"ergo_move_into(&{t}, {at});")
            for z in ac:
                if z != at:
                    self.w(f"ergo_release_val({z});")
            self.ind -= 1
            self.w("} else {")
            self.ind += 1
            bt, bc = self.gen_expr(path, e.b)
            self.w(f"ergo_move_into(&{t}, {bt});")
            for z in bc:
                if z != bt:
                    self.w(f"ergo_release_val({z});")
            self.ind -= 1
            self.w("}")
            self.w(f"ergo_release_val({ct});")
            for z in cc:
                if z != ct:
                    self.w(f"ergo_release_val({z});")
            cleanup.append(t)
            return t, cleanup

        if isinstance(e, Assign):
            vt, vc = self.gen_expr(path, e.value)
            tret = self.new_tmp()
            self.w(f"ErgoVal {tret} = {vt}; ergo_retain_val({tret});")

            if isinstance(e.target, Ident):
                slot = self.cname_of(e.target.name)
                self.w(f"ergo_move_into(&{slot}, {vt});")
            elif isinstance(e.target, Index):
                at, ac = self.gen_expr(path, e.target.a)
                it, ic = self.gen_expr(path, e.target.i)
                self.w(f"ergo_arr_set((ErgoArr*){at}.as.p, ergo_as_int({it}), {vt});")
                self.w(f"ergo_release_val({at});")
                self.w(f"ergo_release_val({it});")
                for z in ac:
                    if z != at:
                        self.w(f"ergo_release_val({z});")
                for z in ic:
                    if z != it:
                        self.w(f"ergo_release_val({z});")
            elif isinstance(e.target, Member):
                base_ty = tc_expr(
                    e.target.a,
                    self.ctx_for(path),
                    self.ty_loc,
                    self.classes,
                    self.funs,
                )
                if base_ty.tag != "class":
                    raise TypeErr(f"{path}: unsupported member assignment")
                at, ac = self.gen_expr(path, e.target.a)
                cname = self.c_class_name(base_ty.name)
                field = self.c_field_name(e.target.name)
                self.w(f"ergo_move_into(&(({cname}*){at}.as.p)->{field}, {vt});")
                self.w(f"ergo_release_val({at});")
                for z in ac:
                    if z != at:
                        self.w(f"ergo_release_val({z});")
            else:
                raise TypeErr(f"{path}: unsupported assignment target in v0 codegen")

            for z in vc:
                if z != vt:
                    self.w(f"ergo_release_val({z});")

            cleanup.append(tret)
            return tret, cleanup

        if isinstance(e, Call):
            # module-qualified calls: mod.fn(...)
            if isinstance(e.fn, Member) and isinstance(e.fn.a, Ident):
                mod = e.fn.a.name
                if self.module_in_scope(mod):
                    name = e.fn.name
                    key = f"{mod}.{name}"
                    sig = self.funs.get(key)
                    if sig is None:
                        raise TypeErr(f"{path}: unknown {mod}.{name}")
                    arg_ts: List[str] = []
                    all_cleanup: List[str] = []
                    for a in e.args:
                        at, ac = self.gen_expr(path, a)
                        arg_ts.append(at)
                        all_cleanup.extend(ac)

                    ret_void = sig.ret.tag == "void"
                    if ret_void:
                        self.w(
                            f"{mangle_global(mod, name)}(" + ", ".join(arg_ts) + ");"
                        )
                        for z in all_cleanup:
                            self.w(f"ergo_release_val({z});")
                        t = self.new_tmp()
                        self.w(f"ErgoVal {t} = EV_NULLV;")
                        cleanup.append(t)
                        return t, cleanup
                    t = self.new_tmp()
                    self.w(
                        f"ErgoVal {t} = {mangle_global(mod, name)}("
                        + ", ".join(arg_ts)
                        + ");"
                    )
                    for z in all_cleanup:
                        self.w(f"ergo_release_val({z});")
                    cleanup.append(t)
                    return t, cleanup

            # method calls: obj.method(...)
            if isinstance(e.fn, Member):
                base = e.fn.a
                m = e.fn.name

                # int/bool/string/... .to_string()
                if m == "to_string" and len(e.args) == 0:
                    bt, bc = self.gen_expr(path, base)
                    t = self.new_tmp()
                    self.w(f"ErgoVal {t} = EV_STR(stdr_to_string({bt}));")
                    self.w(f"ergo_release_val({bt});")
                    for z in bc:
                        if z != bt:
                            self.w(f"ergo_release_val({z});")
                    cleanup.append(t)
                    return t, cleanup

                # array.add(x)  (returns void)
                if m == "add" and len(e.args) == 1:
                    at, ac = self.gen_expr(path, base)
                    vt, vc = self.gen_expr(path, e.args[0])

                    # transfer ownership of vt into array (do NOT release vt)
                    self.w(f"ergo_arr_add((ErgoArr*){at}.as.p, {vt});")

                    # release the retained array temp
                    self.w(f"ergo_release_val({at});")
                    for z in ac:
                        if z != at:
                            self.w(f"ergo_release_val({z});")

                    # release arg temps except the moved value itself
                    for z in vc:
                        if z != vt:
                            self.w(f"ergo_release_val({z});")

                    t = self.new_tmp()
                    self.w(f"ErgoVal {t} = EV_NULLV;")
                    cleanup.append(t)
                    return t, cleanup

                # array.remove(i) (returns removed element)
                if m == "remove" and len(e.args) == 1:
                    at, ac = self.gen_expr(path, base)
                    it, ic = self.gen_expr(path, e.args[0])

                    t = self.new_tmp()
                    self.w(
                        f"ErgoVal {t} = ergo_arr_remove((ErgoArr*){at}.as.p, ergo_as_int({it}));"
                    )

                    self.w(f"ergo_release_val({at});")
                    self.w(f"ergo_release_val({it});")

                    for z in ac:
                        if z != at:
                            self.w(f"ergo_release_val({z});")
                    for z in ic:
                        if z != it:
                            self.w(f"ergo_release_val({z});")

                    cleanup.append(t)
                    return t, cleanup

                # class methods
                base_ty = tc_expr(
                    base, self.ctx_for(path), self.ty_loc, self.classes, self.funs
                )
                if base_ty.tag == "class":
                    ci = self.classes[base_ty.name]
                    if m not in ci.methods:
                        raise TypeErr(f"{path}: unknown method '{base_ty.name}.{m}'")
                    sig = ci.methods[m]
                    mod, cls_name = split_qname(ci.qname)

                    bt, bc = self.gen_expr(path, base)
                    arg_ts: List[str] = []
                    all_cleanup: List[str] = []
                    for a in e.args:
                        at, ac = self.gen_expr(path, a)
                        arg_ts.append(at)
                        all_cleanup.extend(ac)

                    ret_void = sig.ret.tag == "void"
                    if ret_void:
                        self.w(
                            f"{mangle_method(mod, cls_name, m)}("
                            + ", ".join([bt] + arg_ts)
                            + ");"
                        )
                        self.w(f"ergo_release_val({bt});")
                        for z in bc:
                            if z != bt:
                                self.w(f"ergo_release_val({z});")
                        for z in all_cleanup:
                            self.w(f"ergo_release_val({z});")
                        t = self.new_tmp()
                        self.w(f"ErgoVal {t} = EV_NULLV;")
                        cleanup.append(t)
                        return t, cleanup
                    else:
                        t = self.new_tmp()
                        self.w(
                            f"ErgoVal {t} = {mangle_method(mod, cls_name, m)}("
                            + ", ".join([bt] + arg_ts)
                            + ");"
                        )
                        self.w(f"ergo_release_val({bt});")
                        for z in bc:
                            if z != bt:
                                self.w(f"ergo_release_val({z});")
                        for z in all_cleanup:
                            self.w(f"ergo_release_val({z});")
                        cleanup.append(t)
                        return t, cleanup

                raise TypeErr(f"{path}: unknown member call '{m}'")

            # global prelude calls: internal stdr primitives
            if isinstance(e.fn, Ident):
                fname = e.fn.name
                if not self.ty_loc.lookup(fname):
                    if fname == "str":
                        if len(e.args) != 1:
                            raise TypeErr(f"{path}: str expects 1 arg")
                        at, ac = self.gen_expr(path, e.args[0])
                        t = self.new_tmp()
                        self.w(f"ErgoVal {t} = EV_STR(stdr_to_string({at}));")
                        self.w(f"ergo_release_val({at});")
                        for z in ac:
                            if z != at:
                                self.w(f"ergo_release_val({z});")
                        cleanup.append(t)
                        return t, cleanup

                    if fname == "__len":
                        at, ac = self.gen_expr(path, e.args[0])
                        t = self.new_tmp()
                        self.w(f"ErgoVal {t} = EV_INT(stdr_len({at}));")
                        self.w(f"ergo_release_val({at});")
                        for z in ac:
                            if z != at:
                                self.w(f"ergo_release_val({z});")
                        cleanup.append(t)
                        return t, cleanup

                    if fname == "__writef":
                        fmt_t, fc = self.gen_expr(path, e.args[0])
                        args_t, ac = self.gen_expr(path, e.args[1])
                        self.w(f"stdr_writef_args({fmt_t}, {args_t});")
                        self.w(f"ergo_release_val({fmt_t});")
                        self.w(f"ergo_release_val({args_t});")
                        for z in fc:
                            if z != fmt_t:
                                self.w(f"ergo_release_val({z});")
                        for z in ac:
                            if z != args_t:
                                self.w(f"ergo_release_val({z});")
                        t = self.new_tmp()
                        self.w(f"ErgoVal {t} = EV_NULLV;")
                        cleanup.append(t)
                        return t, cleanup

                    if fname == "__read_line":
                        t = self.new_tmp()
                        self.w(f"ErgoVal {t} = EV_STR(stdr_read_line());")
                        cleanup.append(t)
                        return t, cleanup

                    if fname == "__readf_parse":
                        fmt_t, fc = self.gen_expr(path, e.args[0])
                        line_t, lc = self.gen_expr(path, e.args[1])
                        args_t, ac = self.gen_expr(path, e.args[2])
                        t = self.new_tmp()
                        self.w(
                            f"ErgoVal {t} = stdr_readf_parse({fmt_t}, {line_t}, {args_t});"
                        )
                        self.w(f"ergo_release_val({fmt_t});")
                        self.w(f"ergo_release_val({line_t});")
                        self.w(f"ergo_release_val({args_t});")
                        for z in fc:
                            if z != fmt_t:
                                self.w(f"ergo_release_val({z});")
                        for z in lc:
                            if z != line_t:
                                self.w(f"ergo_release_val({z});")
                        for z in ac:
                            if z != args_t:
                                self.w(f"ergo_release_val({z});")
                        cleanup.append(t)
                        return t, cleanup

                    sig = self.funs.get(f"{self.current_module}.{fname}")
                    if sig is None and fname in STDR_PRELUDE and (
                        self.current_module == "stdr" or "stdr" in self.current_imports
                    ):
                        sig = self.funs.get(f"stdr.{fname}")
                    if sig is not None:
                        arg_ts: List[str] = []
                        all_cleanup: List[str] = []
                        for a in e.args:
                            at, ac = self.gen_expr(path, a)
                            arg_ts.append(at)
                            all_cleanup.extend(ac)

                        ret_void = sig.ret.tag == "void"
                        if ret_void:
                            self.w(
                                f"{mangle_global(sig.module, fname)}("
                                + ", ".join(arg_ts)
                                + ");"
                            )
                            for z in all_cleanup:
                                self.w(f"ergo_release_val({z});")
                            t = self.new_tmp()
                            self.w(f"ErgoVal {t} = EV_NULLV;")
                            cleanup.append(t)
                            return t, cleanup
                        t = self.new_tmp()
                        self.w(
                            f"ErgoVal {t} = {mangle_global(sig.module, fname)}("
                            + ", ".join(arg_ts)
                            + ");"
                        )
                        for z in all_cleanup:
                            self.w(f"ergo_release_val({z});")
                        cleanup.append(t)
                        return t, cleanup

            # function value call
            ft, fc = self.gen_expr(path, e.fn)
            arg_ts: List[str] = []
            all_cleanup: List[str] = []
            for a in e.args:
                at, ac = self.gen_expr(path, a)
                arg_ts.append(at)
                all_cleanup.extend(ac)

            t = self.new_tmp()
            self.w(f"ErgoVal {t} = EV_NULLV;")
            if arg_ts:
                argv_name = self.new_sym("argv")
                self.w("{")
                self.ind += 1
                self.w(
                    f"ErgoVal {argv_name}[{len(arg_ts)}] = {{ "
                    + ", ".join(arg_ts)
                    + " };"
                )
                self.w(f"{t} = ergo_call({ft}, {len(arg_ts)}, {argv_name});")
                self.ind -= 1
                self.w("}")
            else:
                self.w(f"{t} = ergo_call({ft}, 0, NULL);")

            self.w(f"ergo_release_val({ft});")
            for z in fc:
                if z != ft:
                    self.w(f"ergo_release_val({z});")
            for z in all_cleanup:
                self.w(f"ergo_release_val({z});")

            cleanup.append(t)
            return t, cleanup

        if isinstance(e, Paren):
            return self.gen_expr(path, e.x)

        raise TypeErr(f"{path}: unhandled expr {type(e).__name__} in CGen")


def emit_c(prog: Program, out_path: str) -> None:
    cg = CGen(prog)
    csrc = cg.gen()
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(csrc)
