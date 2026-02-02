from dataclasses import dataclass
import os
from typing import Any, Dict, List, Optional, Tuple, Union

from .ast import (
    ArrayLit,
    Assign,
    Binary,
    Block,
    BoolLit,
    Call,
    ClassDecl,
    ConstDecl,
    ConstStmt,
    EntryDecl,
    Expr,
    ExprStmt,
    FieldDecl,
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
    MatchArm,
    MatchExpr,
    Module,
    MoveExpr,
    NewExpr,
    NullLit,
    Param,
    Paren,
    PatBool,
    PatIdent,
    PatInt,
    PatNull,
    PatStr,
    PatWild,
    Program,
    RetSpec,
    ReturnStmt,
    Stmt,
    StrLit,
    Ternary,
    TupleLit,
    TypeArray,
    TypeName,
    Unary,
)

# ----------------------------
# Type System
# ----------------------------


class TypeErr(Exception):
    pass


@dataclass(frozen=True)
class Ty:
    tag: str
    name: str = ""
    elem: Optional["Ty"] = None
    items: Optional[Tuple["Ty", ...]] = None
    params: Optional[Tuple["Ty", ...]] = None
    ret: Optional["Ty"] = None


def T_prim(name: str) -> Ty:
    return Ty("prim", name=name)


def T_class(name: str) -> Ty:
    return Ty("class", name=name)


def T_array(elem: Ty) -> Ty:
    return Ty("array", elem=elem)


def T_tuple(items: List[Ty]) -> Ty:
    return Ty("tuple", items=tuple(items))


def T_void() -> Ty:
    return Ty("void")


def T_null() -> Ty:
    return Ty("null")


def T_mod(name: str) -> Ty:
    return Ty("mod", name=name)


def T_fn(params: List[Ty], ret: Ty) -> Ty:
    return Ty("fn", params=tuple(params), ret=ret)


def T_nullable(elem: Ty) -> Ty:
    return Ty("nullable", elem=elem)


PRIMS = {"bool", "string", "void", "num"}

NUMERIC = {"num"}


@dataclass(frozen=True)
class ConstVal:
    ty: Ty
    value: Any


# Module constants collected at build_global_env time.
MODULE_CONSTS: Dict[str, Dict[str, ConstVal]] = {}


def is_numeric_ty(t: Ty) -> bool:
    return t.tag == "prim" and t.name in NUMERIC


def numeric_result(a: Ty, b: Ty, op: str, where: str = "") -> Ty:
    if not (is_numeric_ty(a) and is_numeric_ty(b)):
        raise TypeErr(f"{where}: numeric op {op} expects numeric types")
    return T_prim("num")


def module_name_for_path(path: str) -> str:
    base = os.path.basename(path)
    return base[:-2] if base.endswith(".e") else base


def normalize_import_name(name: str) -> str:
    return name[:-2] if name.endswith(".e") else name


def _eval_const_expr(e: Expr) -> ConstVal:
    if isinstance(e, IntLit):
        return ConstVal(T_prim("num"), e.v)
    if isinstance(e, FloatLit):
        return ConstVal(T_prim("num"), e.v)
    if isinstance(e, BoolLit):
        return ConstVal(T_prim("bool"), e.v)
    if isinstance(e, NullLit):
        return ConstVal(T_null(), None)
    if isinstance(e, StrLit):
        parts: List[str] = []
        for kind, val in e.parts:
            if kind != "text":
                raise TypeErr("const string cannot interpolate")
            parts.append(val)
        return ConstVal(T_prim("string"), "".join(parts))
    if isinstance(e, Paren):
        return _eval_const_expr(e.x)
    if isinstance(e, Unary):
        cv = _eval_const_expr(e.x)
        if e.op == "-":
            if cv.ty.tag != "prim" or cv.ty.name != "num":
                raise TypeErr("const unary - expects numeric")
            return ConstVal(cv.ty, -cv.value)
        if e.op == "!":
            if cv.ty.tag != "prim" or cv.ty.name != "bool":
                raise TypeErr("const ! expects bool")
            return ConstVal(T_prim("bool"), not cv.value)
        raise TypeErr("unsupported const unary op")
    if isinstance(e, Binary) and e.op in ("+", "-", "*", "/", "%"):
        a = _eval_const_expr(e.a)
        b = _eval_const_expr(e.b)
        if (
            a.ty.tag != "prim"
            or b.ty.tag != "prim"
            or a.ty.name != "num"
            or b.ty.name != "num"
        ):
            raise TypeErr("const numeric op expects numeric literals")
        av = a.value
        bv = b.value
        if e.op == "%":
            if isinstance(av, float) or isinstance(bv, float):
                raise TypeErr("const % not supported for float")
            return ConstVal(T_prim("num"), int(av) % int(bv))
        if e.op == "+":
            return ConstVal(T_prim("num"), av + bv)
        if e.op == "-":
            return ConstVal(T_prim("num"), av - bv)
        if e.op == "*":
            return ConstVal(T_prim("num"), av * bv)
        if e.op == "/":
            if isinstance(av, float) or isinstance(bv, float):
                return ConstVal(T_prim("num"), av / bv)
            return ConstVal(T_prim("num"), int(av / bv))
    raise TypeErr("const expression must be a literal or simple numeric expression")


def is_null_ty(t: Ty) -> bool:
    return t.tag == "null"


def is_void_ty(t: Ty) -> bool:
    return t.tag == "void"


def is_nullable_ty(t: Ty) -> bool:
    return t.tag == "nullable"


def strip_nullable(t: Ty) -> Ty:
    if is_nullable_ty(t) and t.elem:
        return t.elem
    return t


def unify(a: Ty, b: Ty, where: str = "", subst: Optional[Dict[str, Ty]] = None) -> Ty:
    if subst is None:
        subst = {}

    if is_null_ty(a) and is_null_ty(b):
        return T_null()
    if is_null_ty(a):
        return b if is_nullable_ty(b) else T_nullable(b)
    if is_null_ty(b):
        return a if is_nullable_ty(a) else T_nullable(a)

    if is_nullable_ty(a) or is_nullable_ty(b):
        ua = strip_nullable(a)
        ub = strip_nullable(b)
        return T_nullable(unify(ua, ub, where, subst))

    if a.tag == "gen":
        if a.name in subst:
            return unify(subst[a.name], b, where, subst)
        subst[a.name] = b
        return b

    if b.tag == "gen":
        if b.name in subst:
            return unify(a, subst[b.name], where, subst)
        subst[b.name] = a
        return a

    if a.tag != b.tag:
        raise TypeErr(f"type mismatch{': ' + where if where else ''}: {a} vs {b}")

    if a.tag == "prim":
        if a.name == b.name:
            return a
        raise TypeErr(
            f"type mismatch{': ' + where if where else ''}: {a.name} vs {b.name}"
        )

    if a.tag == "class":
        if a.name != b.name:
            raise TypeErr(
                f"type mismatch{': ' + where if where else ''}: {a.name} vs {b.name}"
            )
        return a

    if a.tag == "array":
        assert a.elem and b.elem
        return T_array(unify(a.elem, b.elem, where, subst))

    if a.tag == "tuple":
        assert a.items and b.items
        if len(a.items) != len(b.items):
            raise TypeErr(f"tuple arity mismatch{': ' + where if where else ''}")
        return T_tuple([unify(x, y, where, subst) for x, y in zip(a.items, b.items)])

    if a.tag == "fn":
        assert a.params is not None and b.params is not None and a.ret and b.ret
        if len(a.params) != len(b.params):
            raise TypeErr(f"fn arity mismatch{': ' + where if where else ''}")
        params = [
            unify(x, y, where, subst) for x, y in zip(a.params, b.params)
        ]
        ret = unify(a.ret, b.ret, where, subst)
        return T_fn(params, ret)

    if a.tag == "void":
        return a

    if a.tag == "mod":
        if a.name != b.name:
            raise TypeErr(
                f"module mismatch{': ' + where if where else ''}: {a.name} vs {b.name}"
            )
        return a

    raise TypeErr(f"unsupported unify{': ' + where if where else ''}: {a} vs {b}")


def apply_subst(t: Ty, subst: Dict[str, Ty]) -> Ty:
    if t.tag == "gen" and t.name in subst:
        return apply_subst(subst[t.name], subst)
    if t.tag == "array" and t.elem:
        return T_array(apply_subst(t.elem, subst))
    if t.tag == "tuple" and t.items:
        return T_tuple([apply_subst(x, subst) for x in t.items])
    if t.tag == "fn" and t.params and t.ret:
        return T_fn([apply_subst(x, subst) for x in t.params], apply_subst(t.ret, subst))
    if t.tag == "nullable" and t.elem:
        return T_nullable(apply_subst(t.elem, subst))
    return t


def ensure_assignable(expected: Ty, actual: Ty, where: str = "") -> None:
    if is_null_ty(expected) or is_null_ty(actual):
        return
    if is_nullable_ty(expected) or is_nullable_ty(actual):
        ensure_assignable(strip_nullable(expected), strip_nullable(actual), where)
        return
    if expected.tag == "array" and actual.tag == "array":
        assert expected.elem and actual.elem
        ensure_assignable(expected.elem, actual.elem, where)
        return
    if expected.tag == "tuple" and actual.tag == "tuple":
        assert expected.items and actual.items
        if len(expected.items) != len(actual.items):
            raise TypeErr(f"tuple arity mismatch{': ' + where if where else ''}")
        for a, b in zip(expected.items, actual.items):
            ensure_assignable(a, b, where)
        return
    if expected.tag == "fn" and actual.tag == "fn":
        assert expected.params is not None and actual.params is not None
        if len(expected.params) != len(actual.params):
            raise TypeErr(f"fn arity mismatch{': ' + where if where else ''}")
        for ep, ap in zip(expected.params, actual.params):
            ensure_assignable(ep, ap, where)
        assert expected.ret and actual.ret
        ensure_assignable(expected.ret, actual.ret, where)
        return
    if expected.tag == "prim" and actual.tag == "prim":
        if expected.name != actual.name:
            raise TypeErr(
                f"type mismatch{': ' + where if where else ''}: {expected.name} vs {actual.name}"
            )


@dataclass
class FunSig:
    name: str
    module: str
    params: List[Ty]
    param_names: List[str]
    ret: Ty
    is_method: bool = False
    recv_mut: bool = False
    owner_class: Optional[str] = None
    module_path: str = ""


@dataclass
class ClassInfo:
    name: str
    module: str
    qname: str
    vis: str
    is_seal: bool
    module_path: str
    fields: Dict[str, Ty]
    methods: Dict[str, FunSig]


@dataclass
class GlobalEnv:
    classes: Dict[str, ClassInfo]
    funs: Dict[str, FunSig]
    entry: EntryDecl
    module_names: Dict[str, str]  # path -> module name
    module_imports: Dict[str, List[str]]


def _qualify_class_name(mod: str, name: str) -> str:
    return name if "." in name else f"{mod}.{name}"


def ty_from_type(
    tref: Any,
    known_classes: Dict[str, ClassInfo],
    ctx_mod: str,
    ctx_imports: List[str],
) -> Ty:
    if isinstance(tref, TypeArray):
        return T_array(ty_from_type(tref.elem, known_classes, ctx_mod, ctx_imports))
    if isinstance(tref, TypeName):
        n = tref.name
        if n == "str":
            n = "string"
        if n in ("int", "float", "char", "byte"):
            raise TypeErr(f"unknown type '{n}' (use num)")
        if n in PRIMS:
            return T_void() if n == "void" else T_prim(n)
        if "." in n:
            mod, _ = n.split(".", 1)
            if mod != ctx_mod and mod not in ctx_imports:
                raise TypeErr(f"unknown type '{n}'")
            qn = n
            if qn in known_classes:
                return T_class(qn)
            raise TypeErr(f"unknown type '{n}'")
        qn = _qualify_class_name(ctx_mod, n)
        if qn in known_classes:
            return T_class(qn)
        if all(ch.isalpha() or ch == "_" for ch in n):
            return Ty("gen", name=n)
        raise TypeErr(f"unknown type '{n}'")
    raise TypeErr("invalid type expression")


def build_global_env(prog: Program) -> GlobalEnv:
    classes: Dict[str, ClassInfo] = {}
    funs: Dict[str, FunSig] = {}
    entry: Optional[EntryDecl] = None
    module_names: Dict[str, str] = {}
    module_imports: Dict[str, List[str]] = {}

    for m in prog.mods:
        module_names[m.path] = module_name_for_path(m.path)

    for m in prog.mods:
        mod_name = module_names[m.path]
        module_imports[mod_name] = [normalize_import_name(i.name) for i in m.imports]

    # module constants (stdr/math only)
    MODULE_CONSTS.clear()
    for m in prog.mods:
        mod_name = module_names[m.path]
        if mod_name not in ("stdr", "math"):
            for d in m.decls:
                if isinstance(d, ConstDecl):
                    raise TypeErr(
                        f"{m.path}: module-level consts are only supported in stdr/math"
                    )
            continue
        mod_consts = MODULE_CONSTS.setdefault(mod_name, {})
        for d in m.decls:
            if isinstance(d, ConstDecl):
                if d.name in mod_consts:
                    raise TypeErr(f"{m.path}: duplicate const '{d.name}'")
                mod_consts[d.name] = _eval_const_expr(d.expr)

    # class shells
    for m in prog.mods:
        mod_name = module_names[m.path]
        for d in m.decls:
            if isinstance(d, ClassDecl):
                qname = _qualify_class_name(mod_name, d.name)
                if qname in classes:
                    raise TypeErr(f"{m.path}: duplicate class '{d.name}'")
                classes[qname] = ClassInfo(
                    name=d.name,
                    module=mod_name,
                    qname=qname,
                    vis=d.vis,
                    is_seal=d.is_seal,
                    module_path=m.path,
                    fields={},
                    methods={},
                )

    # fill class fields + methods
    for m in prog.mods:
        mod_name = module_names[m.path]
        for d in m.decls:
            if isinstance(d, ClassDecl):
                qname = _qualify_class_name(mod_name, d.name)
                ci = classes[qname]
                for f in d.fields:
                    ci.fields[f.name] = ty_from_type(
                        f.typ, classes, mod_name, module_imports[mod_name]
                    )

                for md in d.methods:
                    if not md.params or not md.params[0].is_this:
                        raise TypeErr(
                            f"{m.path}: method '{md.name}' in class '{d.name}' must begin with this/?this"
                        )
                    recv_mut = md.params[0].is_mut

                    ret_ty = (
                        T_void()
                        if md.ret.is_void
                        else (
                            ty_from_type(
                                md.ret.types[0],
                                classes,
                                mod_name,
                                module_imports[mod_name],
                            )
                            if len(md.ret.types) == 1
                            else T_tuple(
                                [
                                    ty_from_type(
                                        x, classes, mod_name, module_imports[mod_name]
                                    )
                                    for x in md.ret.types
                                ]
                            )
                        )
                    )

                    param_types: List[Ty] = []
                    param_names: List[str] = []
                    for p in md.params[1:]:
                        if p.is_this:
                            raise TypeErr(f"{m.path}: only first param may be this")
                        assert p.typ is not None
                        param_types.append(
                            ty_from_type(
                                p.typ, classes, mod_name, module_imports[mod_name]
                            )
                        )
                        param_names.append(p.name)

                    sig = FunSig(
                        name=md.name,
                        module=mod_name,
                        params=param_types,
                        param_names=param_names,
                        ret=ret_ty,
                        is_method=True,
                        recv_mut=recv_mut,
                        owner_class=ci.qname,
                        module_path=m.path,
                    )

                    if md.name in ci.methods:
                        raise TypeErr(
                            f"{m.path}: duplicate method '{md.name}' in class '{d.name}'"
                        )
                    ci.methods[md.name] = sig

    # top-level funs + entry
    for m in prog.mods:
        mod_name = module_names[m.path]
        for d in m.decls:
            if isinstance(d, FunDecl):
                key = f"{mod_name}.{d.name}"
                if key in funs:
                    raise TypeErr(f"{m.path}: duplicate function '{d.name}'")
                if d.params and d.params[0].is_this:
                    raise TypeErr(
                        f"{m.path}: free function '{d.name}' cannot take this/?this"
                    )

                ret_ty = (
                    T_void()
                    if d.ret.is_void
                    else (
                        ty_from_type(
                            d.ret.types[0], classes, mod_name, module_imports[mod_name]
                        )
                        if len(d.ret.types) == 1
                        else T_tuple(
                            [
                                ty_from_type(
                                    x, classes, mod_name, module_imports[mod_name]
                                )
                                for x in d.ret.types
                            ]
                        )
                    )
                )

                ps: List[Ty] = []
                pnames: List[str] = []
                for p in d.params:
                    assert p.typ is not None
                    ps.append(
                        ty_from_type(
                            p.typ, classes, mod_name, module_imports[mod_name]
                        )
                    )
                    pnames.append(p.name)

                funs[key] = FunSig(
                    name=d.name,
                    module=mod_name,
                    params=ps,
                    param_names=pnames,
                    ret=ret_ty,
                    is_method=False,
                    module_path=m.path,
                )

            if isinstance(d, EntryDecl):
                entry = d

    if entry is None:
        raise TypeErr("missing entry() in init.e")

    return GlobalEnv(
        classes=classes,
        funs=funs,
        entry=entry,
        module_names=module_names,
        module_imports=module_imports,
    )


@dataclass
class Binding:
    ty: Ty
    is_mut: bool
    is_const: bool


class Locals:
    def __init__(self):
        self.scopes: List[Dict[str, Binding]] = [dict()]

    def push(self):
        self.scopes.append(dict())

    def pop(self):
        self.scopes.pop()

    def define(self, name: str, b: Binding):
        self.scopes[-1][name] = b

    def update(self, name: str, b: Binding):
        for s in reversed(self.scopes):
            if name in s:
                s[name] = b
                return
        self.define(name, b)

    def lookup(self, name: str) -> Optional[Binding]:
        for s in reversed(self.scopes):
            if name in s:
                return s[name]
        return None

    def clone(self) -> "Locals":
        c = Locals()
        c.scopes = [
            {k: Binding(ty=v.ty, is_mut=v.is_mut, is_const=v.is_const) for k, v in s.items()}
            for s in self.scopes
        ]
        return c


def is_mut_lvalue(e: Expr, loc: Locals) -> bool:
    from .ast import Ident, Index, Member

    if isinstance(e, Ident):
        b = loc.lookup(e.name)
        return bool(b and b.is_mut and not b.is_const)
    if isinstance(e, Member):
        return is_mut_lvalue(e.a, loc)
    if isinstance(e, Index):
        return is_mut_lvalue(e.a, loc)
    return False


@dataclass
class Ctx:
    module_path: str
    module_name: str
    imports: List[str]
    current_class: Optional[str] = None


STDR_PRELUDE = {"write", "writef", "readf", "len", "is_null", "str"}


def _fun_key(mod: str, name: str) -> str:
    return f"{mod}.{name}"


def _class_key(mod: str, name: str) -> str:
    return f"{mod}.{name}"


def _module_in_scope(name: str, ctx: Ctx, loc: Locals) -> bool:
    if loc.lookup(name):
        return False
    if name == ctx.module_name:
        return True
    return name in ctx.imports


# ----------------------------
# Expression Typechecking
# ----------------------------


def tc_call(
    c: Call,
    ctx: Ctx,
    loc: Locals,
    classes: Dict[str, ClassInfo],
    funs: Dict[str, FunSig],
) -> Ty:
    # module-qualified calls: mod.fn(...)
    if isinstance(c.fn, Member) and isinstance(c.fn.a, Ident):
        mod = c.fn.a.name
        if _module_in_scope(mod, ctx, loc):
            name = c.fn.name
            key = _fun_key(mod, name)
            sig = funs.get(key)
            if sig is None:
                raise TypeErr(f"{ctx.module_path}: unknown {mod}.{name}")
            if len(c.args) != len(sig.params):
                raise TypeErr(
                    f"{ctx.module_path}: '{mod}.{name}' expects {len(sig.params)} args"
                )
            subst: Dict[str, Ty] = {}
            for a, pt in zip(c.args, sig.params):
                at = tc_expr(a, ctx, loc, classes, funs)
                ensure_assignable(pt, at, f"arg for {mod}.{name}")
                if pt.tag == "class" and classes[pt.name].is_seal:
                    if isinstance(a, NullLit):
                        pass
                    elif isinstance(a, MoveExpr):
                        if not isinstance(a.x, Ident):
                            raise TypeErr(
                                f"{ctx.module_path}: move(...) must wrap a variable name"
                            )
                        b = loc.lookup(a.x.name)
                        if not b or not b.is_mut:
                            raise TypeErr(
                                f"{ctx.module_path}: move({a.x.name}) requires mutable binding (let ?{a.x.name})"
                            )
                        unify(b.ty, pt, "move arg type", subst)
                    else:
                        raise TypeErr(
                            f"{ctx.module_path}: sealed param requires move(x) or null"
                        )
                else:
                    unify(pt, at, f"arg for {mod}.{name}", subst)
            return apply_subst(sig.ret, subst)

    # method call: obj.method(...)
    if isinstance(c.fn, Member):
        base = c.fn.a
        base_ty = tc_expr(base, ctx, loc, classes, funs)
        if is_nullable_ty(base_ty):
            raise TypeErr(f"{ctx.module_path}: call on nullable value")
        base_ty = strip_nullable(base_ty)
        mname = c.fn.name

        # array methods
        if base_ty.tag == "array" and base_ty.elem:
            if mname == "add":
                if len(c.args) != 1:
                    raise TypeErr(f"{ctx.module_path}: array.add expects 1 arg")
                if not is_mut_lvalue(base, loc):
                    raise TypeErr(
                        f"{ctx.module_path}: array.add requires mutable binding (let ?name)"
                    )
                ta = tc_expr(c.args[0], ctx, loc, classes, funs)
                ensure_assignable(base_ty.elem, ta, "array.add")
                unify(base_ty.elem, ta, "array.add")
                return T_void()

            if mname == "remove":
                if len(c.args) != 1:
                    raise TypeErr(f"{ctx.module_path}: array.remove expects 1 arg")
                if not is_mut_lvalue(base, loc):
                    raise TypeErr(
                        f"{ctx.module_path}: array.remove requires mutable binding (let ?name)"
                    )
                ti = tc_expr(c.args[0], ctx, loc, classes, funs)
                unify(ti, T_prim("num"), "array.remove index")
                return base_ty.elem

            raise TypeErr(f"{ctx.module_path}: unknown array method '{mname}'")

        # primitive methods
        if base_ty.tag == "prim" and base_ty.name in ("bool", "num"):
            if mname == "to_string":
                if len(c.args) != 0:
                    raise TypeErr(f"{ctx.module_path}: to_string takes no args")
                return T_prim("string")

        # class methods
        if base_ty.tag == "class":
            ci = classes[base_ty.name]
            if mname not in ci.methods:
                raise TypeErr(f"{ctx.module_path}: '{ci.name}' has no method '{mname}'")
            sig = ci.methods[mname]

            if sig.recv_mut and not is_mut_lvalue(base, loc):
                raise TypeErr(
                    f"{ctx.module_path}: method '{ci.name}.{mname}' requires mutable receiver (use let ?x)"
                )

            if len(c.args) != len(sig.params):
                raise TypeErr(
                    f"{ctx.module_path}: '{ci.name}.{mname}' expects {len(sig.params)} args"
                )

            subst: Dict[str, Ty] = {}
            for a, pt in zip(c.args, sig.params):
                at = tc_expr(a, ctx, loc, classes, funs)
                ensure_assignable(pt, at, f"arg for {ci.name}.{mname}")
                if pt.tag == "class" and classes[pt.name].is_seal:
                    if isinstance(a, NullLit):
                        pass
                    elif isinstance(a, MoveExpr):
                        if not isinstance(a.x, Ident):
                            raise TypeErr(
                                f"{ctx.module_path}: move(...) must wrap a variable name"
                            )
                        b = loc.lookup(a.x.name)
                        if not b or not b.is_mut:
                            raise TypeErr(
                                f"{ctx.module_path}: move({a.x.name}) requires mutable binding (let ?{a.x.name})"
                            )
                        unify(b.ty, pt, "move arg type", subst)
                    else:
                        raise TypeErr(
                            f"{ctx.module_path}: sealed param requires move(x) or null"
                        )
                else:
                    unify(pt, at, f"arg for {ci.name}.{mname}", subst)

            return apply_subst(sig.ret, subst)

        raise TypeErr(f"{ctx.module_path}: cannot call member on {base_ty}")

    # global function call: f(...)
    if isinstance(c.fn, Ident):
        fname = c.fn.name

        if loc.lookup(fname):
            fn_ty = tc_expr(c.fn, ctx, loc, classes, funs)
            if fn_ty.tag != "fn" or fn_ty.params is None or fn_ty.ret is None:
                raise TypeErr(f"{ctx.module_path}: unknown function '{fname}'")
            if len(c.args) != len(fn_ty.params):
                raise TypeErr(
                    f"{ctx.module_path}: call expects {len(fn_ty.params)} args"
                )
            subst: Dict[str, Ty] = {}
            for a, pt in zip(c.args, fn_ty.params):
                at = tc_expr(a, ctx, loc, classes, funs)
                unify(pt, at, "fn value call", subst)
            return apply_subst(fn_ty.ret, subst)

        if fname == "str":
            if len(c.args) != 1:
                raise TypeErr(f"{ctx.module_path}: str expects 1 arg")
            tc_expr(c.args[0], ctx, loc, classes, funs)
            return T_prim("string")

        sig = funs.get(_fun_key(ctx.module_name, fname))
        if sig is None and fname in STDR_PRELUDE and (
            ctx.module_name == "stdr" or "stdr" in ctx.imports
        ):
            sig = funs.get(_fun_key("stdr", fname))

        if sig is None:
            fn_ty = tc_expr(c.fn, ctx, loc, classes, funs)
            if fn_ty.tag != "fn" or fn_ty.params is None or fn_ty.ret is None:
                raise TypeErr(f"{ctx.module_path}: unknown function '{fname}'")
            if len(c.args) != len(fn_ty.params):
                raise TypeErr(
                    f"{ctx.module_path}: call expects {len(fn_ty.params)} args"
                )
            subst: Dict[str, Ty] = {}
            for a, pt in zip(c.args, fn_ty.params):
                at = tc_expr(a, ctx, loc, classes, funs)
                unify(pt, at, "fn value call", subst)
            return apply_subst(fn_ty.ret, subst)

        if len(c.args) != len(sig.params):
            raise TypeErr(
                f"{ctx.module_path}: '{fname}' expects {len(sig.params)} args"
            )

        subst: Dict[str, Ty] = {}
        for a, pt in zip(c.args, sig.params):
            at = tc_expr(a, ctx, loc, classes, funs)
            ensure_assignable(pt, at, f"arg for {fname}")
            if pt.tag == "class" and classes[pt.name].is_seal:
                if isinstance(a, NullLit):
                    pass
                elif isinstance(a, MoveExpr):
                    if not isinstance(a.x, Ident):
                        raise TypeErr(
                            f"{ctx.module_path}: move(...) must wrap a variable name"
                        )
                    b = loc.lookup(a.x.name)
                    if not b or not b.is_mut:
                        raise TypeErr(
                            f"{ctx.module_path}: move({a.x.name}) requires mutable binding (let ?{a.x.name})"
                        )
                    unify(b.ty, pt, "move arg type", subst)
                else:
                    raise TypeErr(
                        f"{ctx.module_path}: sealed param requires move(x) or null"
                    )
            else:
                unify(pt, at, f"arg for {fname}", subst)

        return apply_subst(sig.ret, subst)

    fn_ty = tc_expr(c.fn, ctx, loc, classes, funs)
    if fn_ty.tag != "fn" or fn_ty.params is None or fn_ty.ret is None:
        raise TypeErr(f"{ctx.module_path}: unsupported call form")

    if len(c.args) != len(fn_ty.params):
        raise TypeErr(
            f"{ctx.module_path}: call expects {len(fn_ty.params)} args"
        )

    subst: Dict[str, Ty] = {}
    for a, pt in zip(c.args, fn_ty.params):
        at = tc_expr(a, ctx, loc, classes, funs)
        unify(pt, at, "fn value call", subst)
    return apply_subst(fn_ty.ret, subst)


def tc_expr(
    e: Expr,
    ctx: Ctx,
    loc: Locals,
    classes: Dict[str, ClassInfo],
    funs: Dict[str, FunSig],
) -> Ty:
    if isinstance(e, IntLit):
        return T_prim("num")
    if isinstance(e, FloatLit):
        return T_prim("num")
    if isinstance(e, BoolLit):
        return T_prim("bool")
    if isinstance(e, NullLit):
        return T_null()
    if isinstance(e, StrLit):
        return T_prim("string")
    if isinstance(e, TupleLit):
        return T_tuple([tc_expr(x, ctx, loc, classes, funs) for x in e.items])

    if isinstance(e, Ident):
        b = loc.lookup(e.name)
        if b:
            return b.ty
        if _module_in_scope(e.name, ctx, loc):
            return T_mod(e.name)
        sig = funs.get(_fun_key(ctx.module_name, e.name))
        if sig is None and e.name in STDR_PRELUDE and (
            ctx.module_name == "stdr" or "stdr" in ctx.imports
        ):
            sig = funs.get(_fun_key("stdr", e.name))
        if sig:
            return T_fn(sig.params, sig.ret)
        raise TypeErr(f"{ctx.module_path}: unknown name '{e.name}'")

    if isinstance(e, ArrayLit):
        if not e.items:
            raise TypeErr(f"{ctx.module_path}: cannot infer type of empty array []")
        t0 = tc_expr(e.items[0], ctx, loc, classes, funs)
        for it in e.items[1:]:
            t0 = unify(t0, tc_expr(it, ctx, loc, classes, funs), "array literal")
        return T_array(t0)

    if isinstance(e, Unary):
        tx = tc_expr(e.x, ctx, loc, classes, funs)
        if e.op == "!":
            if is_nullable_ty(tx):
                raise TypeErr(f"{ctx.module_path}: ! on nullable value")
            unify(tx, T_prim("bool"), "!")
            return T_prim("bool")
        if e.op == "-":
            if is_nullable_ty(tx):
                raise TypeErr(f"{ctx.module_path}: unary - on nullable value")
            if not is_numeric_ty(strip_nullable(tx)):
                raise TypeErr(f"{ctx.module_path}: unary - expects numeric")
            return T_prim("num")
        if e.op == "#":
            if is_nullable_ty(tx):
                raise TypeErr(f"{ctx.module_path}: # on nullable value")
            if tx.tag == "array" or (tx.tag == "prim" and tx.name == "string"):
                return T_prim("num")
            raise TypeErr(f"{ctx.module_path}: # expects array or string")
        return tx

    if isinstance(e, Binary):
        ta = tc_expr(e.a, ctx, loc, classes, funs)
        tb = tc_expr(e.b, ctx, loc, classes, funs)

        if e.op in ("+", "-", "*", "/", "%"):
            if is_nullable_ty(ta) or is_nullable_ty(tb):
                raise TypeErr(f"{ctx.module_path}: numeric op on nullable value")
            return numeric_result(strip_nullable(ta), strip_nullable(tb), e.op, ctx.module_path)

        if e.op in ("<", "<=", ">", ">="):
            if is_nullable_ty(ta) or is_nullable_ty(tb):
                raise TypeErr(f"{ctx.module_path}: comparison on nullable value")
            if not (is_numeric_ty(strip_nullable(ta)) and is_numeric_ty(strip_nullable(tb))):
                raise TypeErr(f"{ctx.module_path}: comparison expects numeric types")
            return T_prim("bool")

        if e.op in ("&&", "||"):
            if is_nullable_ty(ta) or is_nullable_ty(tb):
                raise TypeErr(f"{ctx.module_path}: logical op on nullable value")
            unify(ta, T_prim("bool"), e.op)
            unify(tb, T_prim("bool"), e.op)
            return T_prim("bool")

        if e.op in ("==", "!="):
            unify(ta, tb, e.op)
            return T_prim("bool")

        raise TypeErr(f"{ctx.module_path}: unknown binary op {e.op}")

    if isinstance(e, Assign):
        if isinstance(e.target, Ident):
            b = loc.lookup(e.target.name)
            if not b:
                raise TypeErr(f"{ctx.module_path}: assign to unknown '{e.target.name}'")
            if b.is_const:
                raise TypeErr(
                    f"{ctx.module_path}: cannot assign to const '{e.target.name}'"
                )
            if not b.is_mut:
                raise TypeErr(
                    f"{ctx.module_path}: cannot assign to immutable '{e.target.name}' (use let ?{e.target.name})"
                )
            tv = tc_expr(e.value, ctx, loc, classes, funs)
            ensure_assignable(b.ty, tv, "assignment")
            new_ty = unify(b.ty, tv, "assignment")
            b.ty = new_ty
            return new_ty

        if isinstance(e.target, (Member, Index)):
            if not is_mut_lvalue(e.target.a, loc):
                raise TypeErr(
                    f"{ctx.module_path}: cannot mutate through immutable binding"
                )
            tt = tc_expr(e.target, ctx, loc, classes, funs)
            tv = tc_expr(e.value, ctx, loc, classes, funs)
            ensure_assignable(tt, tv, "assignment")
            return unify(tt, tv, "assignment")

        raise TypeErr(f"{ctx.module_path}: invalid assignment target")

    if isinstance(e, Member):
        ta = tc_expr(e.a, ctx, loc, classes, funs)
        if is_nullable_ty(ta):
            raise TypeErr(f"{ctx.module_path}: member access on nullable value")
        ta = strip_nullable(ta)

        if ta.tag == "mod":
            mod_consts = MODULE_CONSTS.get(ta.name, {})
            if e.name in mod_consts:
                return mod_consts[e.name].ty
            if _fun_key(ta.name, e.name) in funs:
                raise TypeErr(
                    f"{ctx.module_path}: module function '{ta.name}.{e.name}' must be called"
                )
            raise TypeErr(
                f"{ctx.module_path}: unknown module member '{ta.name}.{e.name}'"
            )

        if ta.tag == "class":
            ci = classes.get(ta.name)
            assert ci

            if ci.vis == "lock":
                in_same_file = ctx.module_path == ci.module_path
                in_own_method = ctx.current_class == ci.qname
                if not (in_same_file or in_own_method):
                    raise TypeErr(
                        f"{ctx.module_path}: cannot access field '{e.name}' of lock class '{ci.name}'"
                    )

            if e.name in ci.fields:
                return ci.fields[e.name]

            if e.name in ci.methods:
                raise TypeErr(
                    f"{ctx.module_path}: method '{e.name}' must be called (no method values in v0)"
                )
            raise TypeErr(
                f"{ctx.module_path}: unknown member '{e.name}' on '{ci.name}'"
            )

        raise TypeErr(f"{ctx.module_path}: member access on non-object")

    if isinstance(e, Index):
        ta = tc_expr(e.a, ctx, loc, classes, funs)
        ti = tc_expr(e.i, ctx, loc, classes, funs)
        unify(ti, T_prim("num"), "index")
        if is_nullable_ty(ta):
            raise TypeErr(f"{ctx.module_path}: indexing nullable value")
        ta = strip_nullable(ta)
        if ta.tag == "array" and ta.elem:
            return ta.elem
        if ta.tag == "tuple" and ta.items is not None:
            if isinstance(e.i, IntLit):
                idx = e.i.v
                if idx < 0 or idx >= len(ta.items):
                    raise TypeErr(f"{ctx.module_path}: tuple index out of range")
                return ta.items[idx]
            raise TypeErr(f"{ctx.module_path}: tuple index must be integer literal")
        if ta.tag == "prim" and ta.name == "string":
            return T_prim("string")
        raise TypeErr(f"{ctx.module_path}: indexing requires array or string")

    if isinstance(e, Ternary):
        tc = tc_expr(e.cond, ctx, loc, classes, funs)
        if is_void_ty(tc):
            raise TypeErr(f"{ctx.module_path}: ternary condition cannot be void")
        ta = tc_expr(e.a, ctx, loc, classes, funs)
        tb = tc_expr(e.b, ctx, loc, classes, funs)
        return unify(ta, tb, "ternary")

    if isinstance(e, MatchExpr):
        scrut_ty = tc_expr(e.scrut, ctx, loc, classes, funs)
        arm_ty: Optional[Ty] = None
        for arm in e.arms:
            loc.push()
            tc_pat(arm.pat, scrut_ty, ctx, loc, classes, funs)
            t = tc_expr(arm.expr, ctx, loc, classes, funs)
            loc.pop()
            arm_ty = t if arm_ty is None else unify(arm_ty, t, "match")
        if arm_ty is None:
            raise TypeErr(f"{ctx.module_path}: match requires at least one arm")
        return arm_ty

    if isinstance(e, LambdaExpr):
        lambda_loc = Locals()
        param_tys: List[Ty] = []
        gen_id = 0
        for p in e.params:
            if p.is_this:
                raise TypeErr(f"{ctx.module_path}: lambda params cannot be this")
            if p.typ is None:
                gen_id += 1
                ty = Ty("gen", name=f"_{p.name}_{gen_id}")
            else:
                ty = ty_from_type(p.typ, classes, ctx.module_name, ctx.imports)
            lambda_loc.define(p.name, Binding(ty=ty, is_mut=p.is_mut, is_const=False))
            param_tys.append(ty)
        body_ty = tc_expr(e.body, ctx, lambda_loc, classes, funs)
        return T_fn(param_tys, body_ty)

    if isinstance(e, NewExpr):
        if "." in e.name:
            mod, _ = e.name.split(".", 1)
            if mod != ctx.module_name and mod not in ctx.imports:
                raise TypeErr(f"{ctx.module_path}: unknown class '{e.name}'")
            qname = e.name
        else:
            qname = _qualify_class_name(ctx.module_name, e.name)
        if qname not in classes:
            raise TypeErr(f"{ctx.module_path}: unknown class '{e.name}'")
        ci = classes[qname]
        if "init" in ci.methods:
            sig = ci.methods["init"]
            if len(e.args) != len(sig.params):
                raise TypeErr(
                    f"{ctx.module_path}: '{ci.name}.init' expects {len(sig.params)} args"
                )
            subst: Dict[str, Ty] = {}
            for a, pt in zip(e.args, sig.params):
                at = tc_expr(a, ctx, loc, classes, funs)
                ensure_assignable(pt, at, f"arg for {ci.name}.init")
                unify(pt, at, f"arg for {ci.name}.init", subst)
            if not is_void_ty(sig.ret):
                raise TypeErr(f"{ctx.module_path}: '{ci.name}.init' must return void")
        elif e.args:
            raise TypeErr(f"{ctx.module_path}: class '{ci.name}' has no init method")
        return T_class(qname)

    if isinstance(e, MoveExpr):
        return tc_expr(e.x, ctx, loc, classes, funs)

    if isinstance(e, Call):
        return tc_call(e, ctx, loc, classes, funs)

    if isinstance(e, Paren):
        return tc_expr(e.x, ctx, loc, classes, funs)

    raise TypeErr(f"{ctx.module_path}: unhandled expr {type(e).__name__}")


def tc_pat(
    pat: Any,
    scrut_ty: Ty,
    ctx: Ctx,
    loc: Locals,
    classes: Dict[str, ClassInfo],
    funs: Dict[str, FunSig],
) -> None:
    if isinstance(pat, PatWild):
        return
    if isinstance(pat, PatIdent):
        loc.define(pat.name, Binding(ty=scrut_ty, is_mut=False, is_const=False))
        return
    if isinstance(pat, PatInt):
        unify(scrut_ty, T_prim("num"), "match pattern")
        return
    if isinstance(pat, PatStr):
        unify(scrut_ty, T_prim("string"), "match pattern")
        return
    if isinstance(pat, PatBool):
        unify(scrut_ty, T_prim("bool"), "match pattern")
        return
    if isinstance(pat, PatNull):
        unify(scrut_ty, T_null(), "match pattern")
        return
    raise TypeErr(f"{ctx.module_path}: unsupported match pattern")


def _null_check_target(e: Expr) -> Optional[Tuple[str, bool]]:
    if not isinstance(e, Binary):
        return None
    if e.op not in ("==", "!="):
        return None
    if isinstance(e.a, Ident) and isinstance(e.b, NullLit):
        return (e.a.name, e.op == "!=")
    if isinstance(e.b, Ident) and isinstance(e.a, NullLit):
        return (e.b.name, e.op == "!=")
    return None


def tc_stmt(
    s: Stmt,
    ctx: Ctx,
    loc: Locals,
    classes: Dict[str, ClassInfo],
    funs: Dict[str, FunSig],
    ret_ty: Ty,
) -> None:
    if isinstance(s, LetStmt):
        t = tc_expr(s.expr, ctx, loc, classes, funs)
        loc.define(s.name, Binding(ty=t, is_mut=s.is_mut, is_const=False))
        return

    if isinstance(s, ConstStmt):
        t = tc_expr(s.expr, ctx, loc, classes, funs)
        loc.define(s.name, Binding(ty=t, is_mut=False, is_const=True))
        return

    if isinstance(s, ExprStmt):
        tc_expr(s.expr, ctx, loc, classes, funs)
        return

    if isinstance(s, ReturnStmt):
        if is_void_ty(ret_ty):
            if s.expr is not None:
                raise TypeErr(f"{ctx.module_path}: return value in void function")
            return
        if s.expr is None:
            raise TypeErr(f"{ctx.module_path}: missing return value")
        t = tc_expr(s.expr, ctx, loc, classes, funs)
        ensure_assignable(ret_ty, t, "return")
        unify(ret_ty, t, "return")
        return

    if isinstance(s, IfStmt):
        # Basic null-narrowing for simple if/else
        narrow = None
        if len(s.arms) == 2 and s.arms[0].cond is not None and s.arms[1].cond is None:
            narrow = _null_check_target(s.arms[0].cond)

        for i, arm in enumerate(s.arms):
            arm_loc = loc.clone()
            if arm.cond is not None:
                ct = tc_expr(arm.cond, ctx, arm_loc, classes, funs)
                if is_void_ty(ct):
                    raise TypeErr(f"{ctx.module_path}: if condition cannot be void")
                if narrow and i == 0:
                    name, not_null = narrow
                    b = arm_loc.lookup(name)
                    if b:
                        new_ty = strip_nullable(b.ty) if not_null else T_null()
                        arm_loc.update(
                            name, Binding(ty=new_ty, is_mut=b.is_mut, is_const=b.is_const)
                        )
            else:
                if narrow and i == 1:
                    name, not_null = narrow
                    b = arm_loc.lookup(name)
                    if b:
                        new_ty = T_null() if not_null else strip_nullable(b.ty)
                        arm_loc.update(
                            name, Binding(ty=new_ty, is_mut=b.is_mut, is_const=b.is_const)
                        )
            tc_stmt(arm.body, ctx, arm_loc, classes, funs, ret_ty)
        return

    if isinstance(s, ForStmt):
        loc.push()
        if s.init is not None:
            tc_stmt(s.init, ctx, loc, classes, funs, ret_ty)
        if s.cond is not None:
            ct = tc_expr(s.cond, ctx, loc, classes, funs)
            if is_void_ty(ct):
                raise TypeErr(f"{ctx.module_path}: for condition cannot be void")
        if s.step is not None:
            tc_expr(s.step, ctx, loc, classes, funs)
        tc_stmt(s.body, ctx, loc, classes, funs, ret_ty)
        loc.pop()
        return

    if isinstance(s, ForEachStmt):
        it = tc_expr(s.expr, ctx, loc, classes, funs)
        it = strip_nullable(it)
        if it.tag == "array" and it.elem:
            elem_ty = it.elem
        elif it.tag == "prim" and it.name == "string":
            elem_ty = T_prim("string")
        else:
            raise TypeErr(f"{ctx.module_path}: foreach expects array or string")
        loc.push()
        loc.define(s.name, Binding(ty=elem_ty, is_mut=False, is_const=False))
        tc_stmt(s.body, ctx, loc, classes, funs, ret_ty)
        loc.pop()
        return

    if isinstance(s, Block):
        loc.push()
        for st in s.stmts:
            tc_stmt(st, ctx, loc, classes, funs, ret_ty)
        loc.pop()
        return

    raise TypeErr(f"{ctx.module_path}: unhandled stmt {type(s).__name__}")


# ----------------------------
# Lowering and Typechecking Entry Points
# ----------------------------


def lower_expr(e: Expr) -> Expr:
    # Lower #x to stdr.len(x)
    if isinstance(e, Unary) and e.op == "#":
        return Call(fn=Member(a=Ident("stdr"), name="len"), args=[lower_expr(e.x)])

    # Lower stdr.writef/readf(...) to writef/readf(...)
    if (
        isinstance(e, Call)
        and isinstance(e.fn, Member)
        and isinstance(e.fn.a, Ident)
        and e.fn.a.name == "stdr"
        and e.fn.name in ("writef", "readf", "str")
    ):
        return lower_expr(Call(fn=Ident(e.fn.name), args=e.args))

    # Lower writef/readf varargs into writef/readf(fmt, (args...))
    if isinstance(e, Call) and isinstance(e.fn, Ident) and e.fn.name in (
        "writef",
        "readf",
    ):
        if not e.args:
            return e
        if (
            len(e.args) == 2
            and isinstance(e.args[1], TupleLit)
        ):
            return Call(
                fn=e.fn,
                args=[lower_expr(e.args[0]), lower_expr(e.args[1])],
            )
        fmt = lower_expr(e.args[0])
        rest = [lower_expr(a) for a in e.args[1:]]
        return Call(fn=e.fn, args=[fmt, TupleLit(items=rest)])

    # Lower move(x) to MoveExpr(x)
    if (
        isinstance(e, Call)
        and isinstance(e.fn, Ident)
        and e.fn.name == "move"
        and len(e.args) == 1
    ):
        return MoveExpr(x=lower_expr(e.args[0]))

    # Recurse
    if isinstance(e, Unary):
        return Unary(op=e.op, x=lower_expr(e.x))
    if isinstance(e, Binary):
        return Binary(op=e.op, a=lower_expr(e.a), b=lower_expr(e.b))
    if isinstance(e, Assign):
        return Assign(target=lower_expr(e.target), value=lower_expr(e.value))
    if isinstance(e, Call):
        return Call(fn=lower_expr(e.fn), args=[lower_expr(a) for a in e.args])
    if isinstance(e, Index):
        return Index(a=lower_expr(e.a), i=lower_expr(e.i))
    if isinstance(e, Member):
        return Member(a=lower_expr(e.a), name=e.name)
    if isinstance(e, Paren):
        return Paren(x=lower_expr(e.x))
    if isinstance(e, ArrayLit):
        return ArrayLit(items=[lower_expr(x) for x in e.items])
    if isinstance(e, Ternary):
        return Ternary(cond=lower_expr(e.cond), a=lower_expr(e.a), b=lower_expr(e.b))
    if isinstance(e, MatchExpr):
        return MatchExpr(
            scrut=lower_expr(e.scrut),
            arms=[MatchArm(pat=a.pat, expr=lower_expr(a.expr)) for a in e.arms],
        )
    if isinstance(e, LambdaExpr):
        return LambdaExpr(params=e.params, body=lower_expr(e.body))
    if isinstance(e, NewExpr):
        return NewExpr(name=e.name, args=[lower_expr(a) for a in e.args])
    if isinstance(e, TupleLit):
        return TupleLit(items=[lower_expr(x) for x in e.items])
    if isinstance(e, MoveExpr):
        return MoveExpr(x=lower_expr(e.x))

    return e


def lower_stmt(s: Stmt) -> Stmt:
    if isinstance(s, LetStmt):
        return LetStmt(name=s.name, is_mut=s.is_mut, expr=lower_expr(s.expr))
    if isinstance(s, ConstStmt):
        return ConstStmt(name=s.name, expr=lower_expr(s.expr))
    if isinstance(s, ReturnStmt):
        return ReturnStmt(expr=lower_expr(s.expr) if s.expr else None)
    if isinstance(s, ExprStmt):
        return ExprStmt(expr=lower_expr(s.expr))
    if isinstance(s, IfStmt):
        arms = []
        for a in s.arms:
            arms.append(
                IfArm(
                    cond=lower_expr(a.cond) if a.cond else None, body=lower_stmt(a.body)
                )
            )
        return IfStmt(arms=arms)
    if isinstance(s, ForStmt):
        return ForStmt(
            init=lower_stmt(s.init) if s.init else None,
            cond=lower_expr(s.cond) if s.cond else None,
            step=lower_expr(s.step) if s.step else None,
            body=lower_stmt(s.body),
        )
    if isinstance(s, ForEachStmt):
        return ForEachStmt(
            name=s.name, expr=lower_expr(s.expr), body=lower_stmt(s.body)
        )
    if isinstance(s, Block):
        return Block(stmts=[lower_stmt(x) for x in s.stmts])
    return s


def lower_decl(
    d: Union[FunDecl, EntryDecl, ClassDecl, ConstDecl],
) -> Union[FunDecl, EntryDecl, ClassDecl, ConstDecl]:
    if isinstance(d, FunDecl):
        fun_body = lower_stmt(d.body)
        if not isinstance(fun_body, Block):
            fun_body = Block(stmts=[fun_body])
        return FunDecl(name=d.name, params=d.params, ret=d.ret, body=fun_body)
    if isinstance(d, ConstDecl):
        return ConstDecl(name=d.name, expr=lower_expr(d.expr))
    if isinstance(d, EntryDecl):
        entry_body = lower_stmt(d.body)
        if not isinstance(entry_body, Block):
            entry_body = Block(stmts=[entry_body])
        return EntryDecl(ret=d.ret, body=entry_body)
    if isinstance(d, ClassDecl):
        return ClassDecl(
            name=d.name,
            vis=d.vis,
            is_seal=d.is_seal,
            fields=d.fields,
            methods=[lower_decl(m) for m in d.methods],  # type: ignore
        )
    return d


def lower_program(p: Program) -> Program:
    mods = []
    for m in p.mods:
        decls = [lower_decl(d) for d in m.decls]
        mods.append(Module(path=m.path, imports=m.imports, decls=decls))
    return Program(mods=mods)


def typecheck_program(prog: Program) -> None:
    env = build_global_env(prog)
    classes = env.classes
    funs = env.funs

    for m in prog.mods:
        mod_name = env.module_names[m.path]
        imports = env.module_imports.get(mod_name, [])
        for d in m.decls:
            if isinstance(d, FunDecl):
                loc = Locals()
                ctx = Ctx(module_path=m.path, module_name=mod_name, imports=imports)
                for p in d.params:
                    assert p.typ is not None
                    ty = ty_from_type(p.typ, classes, mod_name, imports)
                    loc.define(p.name, Binding(ty=ty, is_mut=p.is_mut, is_const=False))

                ret_ty = (
                    T_void()
                    if d.ret.is_void
                    else (
                        ty_from_type(d.ret.types[0], classes, mod_name, imports)
                        if len(d.ret.types) == 1
                        else T_tuple(
                            [ty_from_type(x, classes, mod_name, imports) for x in d.ret.types]
                        )
                    )
                )
                tc_stmt(d.body, ctx, loc, classes, funs, ret_ty)

            if isinstance(d, ClassDecl):
                qname = _qualify_class_name(mod_name, d.name)
                for md in d.methods:
                    loc = Locals()
                    ctx = Ctx(
                        module_path=m.path,
                        module_name=mod_name,
                        imports=imports,
                        current_class=qname,
                    )

                    if not md.params or not md.params[0].is_this:
                        raise TypeErr(
                            f"{m.path}: method '{md.name}' in class '{d.name}' must begin with this/?this"
                        )
                    recv_mut = md.params[0].is_mut
                    loc.define(
                        "this",
                        Binding(
                            ty=T_class(qname), is_mut=recv_mut, is_const=False
                        ),
                    )

                    for p in md.params[1:]:
                        assert p.typ is not None
                        ty = ty_from_type(p.typ, classes, mod_name, imports)
                        loc.define(
                            p.name, Binding(ty=ty, is_mut=p.is_mut, is_const=False)
                        )

                    ret_ty = (
                        T_void()
                        if md.ret.is_void
                        else (
                            ty_from_type(md.ret.types[0], classes, mod_name, imports)
                            if len(md.ret.types) == 1
                            else T_tuple(
                                [
                                    ty_from_type(x, classes, mod_name, imports)
                                    for x in md.ret.types
                                ]
                            )
                        )
                    )
                    tc_stmt(md.body, ctx, loc, classes, funs, ret_ty)

            if isinstance(d, EntryDecl):
                loc = Locals()
                ctx = Ctx(module_path=m.path, module_name=mod_name, imports=imports)
                if not d.ret.is_void:
                    raise TypeErr(f"{m.path}: entry() must return void")
                ret_ty = T_void()
                tc_stmt(d.body, ctx, loc, classes, funs, ret_ty)
