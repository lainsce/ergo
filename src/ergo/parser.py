from typing import Dict, List, Optional, Union

from .ast import (
    ArrayLit,
    Assign,
    Binary,
    Block,
    BoolLit,
    Call,
    ClassDecl,
    ConstStmt,
    Decl,
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
    Import,
    Index,
    IntLit,
    LambdaExpr,
    LetStmt,
    MatchArm,
    MatchExpr,
    Member,
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
    RetSpec,
    ReturnStmt,
    Stmt,
    StrLit,
    Ternary,
    TupleLit,
    Unary,
)
from .lexer import ParseErr, Tok


class Parser:
    def __init__(self, toks: List[Tok], path: str):
        self.toks = toks
        self.i = 0
        self.path = path

    def peek(self, k=0) -> Tok:
        return (
            self.toks[self.i + k]
            if self.i + k < len(self.toks)
            else Tok("EOF", "", -1, -1)
        )

    def at(self, kind: str) -> bool:
        return self.peek().kind == kind

    def eat(self, kind: str) -> Tok:
        t = self.peek()
        if t.kind != kind:
            raise ParseErr(
                f"{self.path}:{t.line}:{t.col}: expected {kind}, got {t.kind} ({t.text})"
            )
        self.i += 1
        return t

    def maybe(self, kind: str) -> Optional[Tok]:
        if self.at(kind):
            return self.eat(kind)
        return None

    def skip_semi(self):
        while self.at("SEMI"):
            self.eat("SEMI")

    def parse_module(self) -> Module:
        imports: List[Import] = []
        decls: List[Decl] = []
        self.skip_semi()
        while not self.at("EOF"):
            if self.at("KW_bring"):
                imports.append(self.parse_import())
            elif self.at("KW_entry"):
                decls.append(self.parse_entry())
            elif self.at("KW_fun"):
                decls.append(self.parse_fun())
            else:
                if (
                    self.at("KW_pub")
                    or self.at("KW_lock")
                    or self.at("KW_seal")
                    or self.at("KW_class")
                ):
                    decls.append(self.parse_class())
                else:
                    t = self.peek()
                    raise ParseErr(
                        f"{self.path}:{t.line}:{t.col}: unexpected token {t.kind} ({t.text})"
                    )
            self.skip_semi()
        return Module(path=self.path, imports=imports, decls=decls)

    def parse_import(self) -> Import:
        self.eat("KW_bring")
        t = self.eat("IDENT")
        name = t.val
        if self.maybe("DOT"):
            ext = self.eat("IDENT").val
            name = f"{name}.{ext}"
        return Import(name=name)

    def parse_ret_spec(self) -> RetSpec:
        self.eat("((")
        if self.at("--"):
            self.eat("--")
            self.eat("))")
            return RetSpec(is_void=True, types=[])
        types: List[str] = []
        types.append(self.parse_type())
        while self.at("SEMI") or self.at("COMMA"):
            self.eat(self.peek().kind)
            types.append(self.parse_type())
        self.eat("))")
        return RetSpec(is_void=False, types=types)

    def parse_type(self) -> str:
        return self.eat("IDENT").val

    def parse_params(self) -> List[Param]:
        ps: List[Param] = []
        if self.at("RPAR"):
            return ps
        while True:
            is_mut = self.maybe("QMARK") is not None
            name_tok = self.eat("IDENT")
            name = name_tok.val
            if name == "this" and not self.at("="):
                ps.append(Param(name="this", typ=None, is_mut=is_mut, is_this=True))
            else:
                self.eat("=")
                typ = self.parse_type()
                ps.append(Param(name=name, typ=typ, is_mut=is_mut, is_this=False))
            if not self.maybe("COMMA"):
                break
        return ps

    def parse_fun(self) -> FunDecl:
        self.eat("KW_fun")
        name = self.eat("IDENT").val
        self.eat("LPAR")
        params = self.parse_params()
        self.eat("RPAR")
        ret = self.parse_ret_spec()
        body = self.parse_block()
        return FunDecl(name=name, params=params, ret=ret, body=body)

    def parse_entry(self) -> EntryDecl:
        self.eat("KW_entry")
        self.eat("LPAR")
        self.eat("RPAR")
        ret = self.parse_ret_spec()
        body = self.parse_block()
        return EntryDecl(ret=ret, body=body)

    def parse_class(self) -> ClassDecl:
        vis = "priv"
        is_seal = False

        if self.at("KW_pub"):
            self.eat("KW_pub")
            vis = "pub"
        elif self.at("KW_lock"):
            self.eat("KW_lock")
            vis = "lock"

        if self.at("KW_seal"):
            self.eat("KW_seal")
            is_seal = True

        self.eat("KW_class")
        name = self.eat("IDENT").val
        self.eat("LBRACE")

        fields: List[FieldDecl] = []
        methods: List[FunDecl] = []

        self.skip_semi()
        while not self.at("RBRACE"):
            # allow `pub fun` inside classes only
            if self.at("KW_pub") and self.peek(1).kind == "KW_fun":
                self.eat("KW_pub")
                methods.append(self.parse_fun())
            elif self.at("KW_fun"):
                methods.append(self.parse_fun())
            else:
                fname = self.eat("IDENT").val
                self.eat("=")
                ftyp = self.parse_type()
                fields.append(FieldDecl(name=fname, typ=ftyp))
            self.skip_semi()

        self.eat("RBRACE")
        return ClassDecl(
            name=name, vis=vis, is_seal=is_seal, fields=fields, methods=methods
        )

    def parse_block(self) -> Block:
        self.eat("LBRACE")
        stmts: List[Stmt] = []
        self.skip_semi()
        while not self.at("RBRACE"):
            stmts.append(self.parse_stmt())
            self.skip_semi()
        self.eat("RBRACE")
        return Block(stmts=stmts)

    def parse_stmt(self) -> Stmt:
        if self.at("KW_let"):
            return self.parse_let()
        if self.at("KW_const"):
            return self.parse_const()
        if self.at("KW_if"):
            return self.parse_if()
        if self.at("KW_for"):
            return self.parse_for()
        if self.at("KW_return"):
            return self.parse_return()
        if self.at("LBRACE"):
            return self.parse_block()
        e = self.parse_expr(0)
        return ExprStmt(expr=e)

    def parse_let(self) -> LetStmt:
        self.eat("KW_let")
        is_mut = self.maybe("QMARK") is not None
        name = self.eat("IDENT").val
        self.eat("=")
        e = self.parse_expr(0)
        return LetStmt(name=name, is_mut=is_mut, expr=e)

    def parse_const(self) -> ConstStmt:
        self.eat("KW_const")
        name = self.eat("IDENT").val
        self.eat("=")
        e = self.parse_expr(0)
        return ConstStmt(name=name, expr=e)

    def parse_if(self) -> IfStmt:
        arms: List[IfArm] = []
        self.eat("KW_if")
        c = self.parse_if_cond()
        arms.append(IfArm(cond=c, body=self.parse_arm()))
        self.skip_semi()  # allow ; or newline after single-line arm

        while self.at("KW_elif"):
            self.eat("KW_elif")
            c2 = self.parse_if_cond()
            arms.append(IfArm(cond=c2, body=self.parse_arm()))
            self.skip_semi()

        if self.at("KW_else"):
            self.eat("KW_else")
            arms.append(IfArm(cond=None, body=self.parse_arm()))
            self.skip_semi()

        return IfStmt(arms=arms)

    def parse_if_cond(self) -> Expr:
        if self.at("LPAR"):
            self.eat("LPAR")
            c = self.parse_expr(0)
            self.eat("RPAR")
            return c
        return self.parse_expr(0)

    def parse_arm(self) -> Stmt:
        if self.at("COLON"):
            self.eat("COLON")
            return self.parse_stmt()
        return self.parse_block()

    def parse_return(self) -> ReturnStmt:
        self.eat("KW_return")
        if self.at("SEMI") or self.at("RBRACE"):
            return ReturnStmt(expr=None)
        e = self.parse_expr(0)
        return ReturnStmt(expr=e)

    def parse_for(self) -> Stmt:
        self.eat("KW_for")
        self.eat("LPAR")

        # foreach: for (name in expr)
        if self.at("IDENT") and self.peek(1).kind == "KW_in":
            name = self.eat("IDENT").val
            self.eat("KW_in")
            e = self.parse_expr(0)
            self.eat("RPAR")
            body = self.parse_arm()
            return ForEachStmt(name=name, expr=e, body=body)

        init: Optional[Stmt] = None
        if not self.at("SEMI"):
            if self.at("KW_let"):
                init = self.parse_let()
            elif self.at("KW_const"):
                init = self.parse_const()
            else:
                init_expr = self.parse_expr(0)
                init = ExprStmt(expr=init_expr)
        self.eat("SEMI")

        cond: Optional[Expr] = None
        if not self.at("SEMI"):
            cond = self.parse_expr(0)
        self.eat("SEMI")

        step: Optional[Expr] = None
        if not self.at("RPAR"):
            step = self.parse_expr(0)
        self.eat("RPAR")
        body = self.parse_arm()
        return ForStmt(init=init, cond=cond, step=step, body=body)

    PRECS: Dict[str, int] = {
        "=": 1,
        "||": 2,
        "&&": 3,
        "==": 4,
        "!=": 4,
        "<": 5,
        "<=": 5,
        ">": 5,
        ">=": 5,
        "+": 6,
        "-": 6,
        "*": 7,
        "/": 7,
        "%": 7,
    }

    def parse_expr(self, min_prec: int) -> Expr:
        x = self.parse_unary()
        while True:
            t = self.peek()
            if t.kind not in self.PRECS:
                break
            prec = self.PRECS[t.kind]
            if prec < min_prec:
                break
            op = t.kind
            self.eat(t.kind)
            next_min = prec + (0 if op == "=" else 1)
            rhs = self.parse_expr(next_min)
            if op == "=":
                x = Assign(target=x, value=rhs)
            else:
                x = Binary(op=op, a=x, b=rhs)
        return x

    def parse_unary(self) -> Expr:
        if self.at("#") or self.at("!") or self.at("-"):
            op = self.peek().kind
            self.eat(op)
            return Unary(op=op, x=self.parse_unary())
        return self.parse_postfix()

    def parse_postfix(self) -> Expr:
        x = self.parse_primary()
        while True:
            if self.at("LPAR"):
                args = self.parse_call_args()
                x = Call(fn=x, args=args)
                continue
            if self.at("LBRACK"):
                self.eat("LBRACK")
                idx = self.parse_expr(0)
                self.eat("RBRACK")
                x = Index(a=x, i=idx)
                continue
            if self.at("DOT"):
                self.eat("DOT")
                name = self.eat("IDENT").val
                x = Member(a=x, name=name)
                continue
            # Removed :: and Guard syntax; use only DOT for member/constant access
            break
        return x

    def parse_call_args(self) -> List[Expr]:
        self.eat("LPAR")
        args: List[Expr] = []
        if not self.at("RPAR"):
            args.append(self.parse_expr(0))
            while self.maybe("COMMA"):
                args.append(self.parse_expr(0))
        self.eat("RPAR")
        return args

    def parse_primary(self) -> Expr:
        t = self.peek()
        if t.kind == "INT":
            self.eat("INT")
            return IntLit(v=t.val)
        if t.kind == "FLOAT":
            self.eat("FLOAT")
            return FloatLit(v=t.val)
        if t.kind == "STR":
            self.eat("STR")
            return StrLit(parts=t.val)
        if t.kind == "KW_match":
            return self.parse_match()
        if t.kind == "KW_new":
            return self.parse_new()
        if t.kind == "BAR":
            return self.parse_lambda()
        if t.kind == "IDENT":
            self.eat("IDENT")
            return Ident(name=t.val)
        if t.kind == "KW_null":
            self.eat("KW_null")
            return NullLit()
        if t.kind == "KW_true":
            self.eat("KW_true")
            return BoolLit(v=True)
        if t.kind == "KW_false":
            self.eat("KW_false")
            return BoolLit(v=False)
        if t.kind == "LBRACK":
            return self.parse_array_lit()
        if t.kind == "LPAR":
            self.eat("LPAR")
            x = self.parse_expr(0)
            if self.at("COMMA"):
                items: List[Expr] = [x]
                while self.maybe("COMMA"):
                    items.append(self.parse_expr(0))
                self.eat("RPAR")
                return TupleLit(items=items)
            self.eat("RPAR")
            return Paren(x=x)
        raise ParseErr(
            f"{self.path}:{t.line}:{t.col}: bad expr token {t.kind} ({t.text})"
        )

    def parse_match(self) -> MatchExpr:
        self.eat("KW_match")
        scrut = self.parse_expr(0)

        arms: List[MatchArm] = []
        if self.at("COLON"):
            self.eat("COLON")
            arms.append(self.parse_match_arm())
            while self.maybe("COMMA"):
                arms.append(self.parse_match_arm())
            return MatchExpr(scrut=scrut, arms=arms)

        self.eat("LBRACE")
        self.skip_semi()
        while not self.at("RBRACE"):
            arms.append(self.parse_match_arm())
            self.skip_semi()
        self.eat("RBRACE")
        return MatchExpr(scrut=scrut, arms=arms)

    def parse_match_arm(self) -> MatchArm:
        pat = self.parse_pattern()
        self.eat("=>")
        expr = self.parse_expr(0)
        return MatchArm(pat=pat, expr=expr)

    def parse_pattern(self):
        t = self.peek()
        if t.kind == "INT":
            self.eat("INT")
            return PatInt(v=t.val)
        if t.kind == "STR":
            self.eat("STR")
            return PatStr(parts=t.val)
        if t.kind == "KW_true":
            self.eat("KW_true")
            return PatBool(v=True)
        if t.kind == "KW_false":
            self.eat("KW_false")
            return PatBool(v=False)
        if t.kind == "KW_null":
            self.eat("KW_null")
            return PatNull()
        if t.kind == "IDENT":
            name = self.eat("IDENT").val
            if name == "_":
                return PatWild()
            return PatIdent(name=name)
        raise ParseErr(
            f"{self.path}:{t.line}:{t.col}: bad pattern token {t.kind} ({t.text})"
        )

    def parse_lambda(self) -> LambdaExpr:
        self.eat("BAR")
        params: List[Param] = []
        if not self.at("BAR"):
            while True:
                is_mut = self.maybe("QMARK") is not None
                name = self.eat("IDENT").val
                typ: Optional[str] = None
                if self.maybe("="):
                    typ = self.parse_type()
                params.append(Param(name=name, typ=typ, is_mut=is_mut, is_this=False))
                if not self.maybe("COMMA"):
                    break
        self.eat("BAR")
        body = self.parse_expr(0)
        return LambdaExpr(params=params, body=body)

    def parse_new(self) -> NewExpr:
        self.eat("KW_new")
        name = self.eat("IDENT").val
        args: List[Expr] = []
        if self.at("LPAR"):
            args = self.parse_call_args()
        return NewExpr(name=name, args=args)

    def parse_array_lit(self) -> ArrayLit:
        self.eat("LBRACK")
        items: List[Expr] = []
        if not self.at("RBRACK"):
            items.append(self.parse_expr(0))
            while self.maybe("COMMA"):
                items.append(self.parse_expr(0))
        self.eat("RBRACK")
        return ArrayLit(items=items)
