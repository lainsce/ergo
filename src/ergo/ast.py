# --- Program Structure ---
from dataclasses import dataclass
from typing import List, Optional, Tuple, Union


@dataclass
class Program:
    mods: List["Module"]


@dataclass
class Module:
    path: str
    imports: List["Import"]
    decls: List["Decl"]


@dataclass
class Import:
    name: str  # "stdr", "math", or "file.e"


Decl = Union["FunDecl", "EntryDecl", "ClassDecl", "ConstDecl"]

# --- Function and Class Declarations ---


TypeRef = Union["TypeName", "TypeArray"]


@dataclass
class TypeName:
    name: str


@dataclass
class TypeArray:
    elem: "TypeRef"


@dataclass
class RetSpec:
    is_void: bool
    types: List["TypeRef"]


@dataclass
class Param:
    name: str
    typ: Optional["TypeRef"]  # None for this/?this
    is_mut: bool
    is_this: bool


@dataclass
class FunDecl:
    name: str
    params: List[Param]
    ret: RetSpec
    body: "Block"


@dataclass
class ConstDecl:
    name: str
    expr: "Expr"


@dataclass
class EntryDecl:
    ret: RetSpec
    body: "Block"


@dataclass
class FieldDecl:
    name: str
    typ: "TypeRef"


@dataclass
class ClassDecl:
    name: str
    vis: str  # "pub" | "lock" | "priv"
    is_seal: bool
    fields: List[FieldDecl]
    methods: List["FunDecl"]


# --- Statements ---

Stmt = Union[
    "LetStmt",
    "ConstStmt",
    "IfStmt",
    "ForStmt",
    "ForEachStmt",
    "ReturnStmt",
    "ExprStmt",
    "Block",
]


@dataclass
class Block:
    stmts: List["Stmt"]


@dataclass
class LetStmt:
    name: str
    is_mut: bool
    expr: "Expr"


@dataclass
class ConstStmt:
    name: str
    expr: "Expr"


@dataclass
class IfArm:
    cond: Optional["Expr"]  # None for else
    body: Stmt  # block or single stmt


@dataclass
class IfStmt:
    arms: List[IfArm]


@dataclass
class ForStmt:
    init: Optional["Stmt"]
    cond: Optional["Expr"]
    step: Optional["Expr"]
    body: "Stmt"


@dataclass
class ForEachStmt:
    name: str
    expr: "Expr"
    body: "Stmt"


@dataclass
class ReturnStmt:
    expr: Optional["Expr"]


@dataclass
class ExprStmt:
    expr: "Expr"


# --- Expressions ---

Expr = Union[
    "IntLit",
    "FloatLit",
    "StrLit",
    "TupleLit",
    "Ident",
    "NullLit",
    "BoolLit",
    "ArrayLit",
    "Unary",
    "Binary",
    "Assign",
    "Call",
    "Index",
    "Member",
    "Paren",
    "MatchExpr",
    "LambdaExpr",
    "NewExpr",
    "Ternary",
    "MoveExpr",
]


@dataclass
class IntLit:
    v: int


@dataclass
class FloatLit:
    v: float


@dataclass
class StrLit:
    parts: List[Tuple[str, str]]  # ("text", ...) / ("var", ...)


@dataclass
class TupleLit:
    items: List["Expr"]


@dataclass
class Ident:
    name: str


@dataclass
class NullLit:
    pass


@dataclass
class BoolLit:
    v: bool


@dataclass
class ArrayLit:
    items: List["Expr"]


@dataclass
class Unary:
    op: str
    x: "Expr"


@dataclass
class Binary:
    op: str
    a: "Expr"
    b: "Expr"


@dataclass
class Assign:
    target: "Expr"
    value: "Expr"


@dataclass
class Call:
    fn: "Expr"
    args: List["Expr"]


@dataclass
class Index:
    a: "Expr"
    i: "Expr"


@dataclass
class Member:
    a: "Expr"
    name: str


@dataclass
class Paren:
    x: "Expr"


# Lowered forms


@dataclass
class Ternary:
    cond: "Expr"
    a: "Expr"
    b: "Expr"


@dataclass
class MatchArm:
    pat: "Pat"
    expr: "Expr"


@dataclass
class MatchExpr:
    scrut: "Expr"
    arms: List[MatchArm]


Pat = Union["PatWild", "PatIdent", "PatInt", "PatStr", "PatBool", "PatNull"]


@dataclass
class PatWild:
    pass


@dataclass
class PatIdent:
    name: str


@dataclass
class PatInt:
    v: int


@dataclass
class PatStr:
    parts: List[Tuple[str, str]]


@dataclass
class PatBool:
    v: bool


@dataclass
class PatNull:
    pass


@dataclass
class LambdaExpr:
    params: List[Param]
    body: "Expr"


@dataclass
class NewExpr:
    name: str
    args: List["Expr"]


@dataclass
class MoveExpr:
    x: "Expr"
