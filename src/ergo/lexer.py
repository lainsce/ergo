from typing import Any, List, Optional, Tuple

# Token kinds for the Ergo language
STMT_END = {
    "RBRACE",
    "SEMI",
    "RPAR",
    "RBRACK",
    "INT",
    "FLOAT",
    "IDENT",
    "STR",
    "KW_true",
    "KW_false",
    "KW_null",
}
KW = {
    "module",
    "bring",
    "fun",
    "entry",
    "class",
    "pub",
    "lock",
    "seal",
    "let",
    "const",
    "if",
    "else",
    "elif",
    "return",
    "true",
    "false",
    "null",
    "for",
    "match",
    "new",
    "in",
}


class Tok:
    kind: str
    text: str
    line: int
    col: int
    val: Any = None

    def __init__(self, kind: str, text: str, line: int, col: int, val: Any = None):
        self.kind = kind
        self.text = text
        self.line = line
        self.col = col
        self.val = val

    def __repr__(self):
        return (
            f"Tok({self.kind!r}, {self.text!r}, {self.line}, {self.col}, {self.val!r})"
        )


class LexErr(Exception):
    pass


class ParseErr(Exception):
    pass


def is_ident_start(ch: str) -> bool:
    return ch.isalpha() or ch == "_"


def is_ident_mid(ch: str) -> bool:
    return ch.isalnum() or ch == "_"


def lex(src: str) -> List[Tok]:
    toks: List[Tok] = []
    i = 0
    line = 1
    col = 1
    nest = 0  # (), [], {} nesting to suppress newline semis
    ret_depth = 0  # inside (( ... )) return spec?

    def emit(kind: str, text: str, val=None, line_num=None, c=None):
        toks.append(
            Tok(
                kind,
                text,
                line_num if line_num is not None else line,
                c if c is not None else col,
                val,
            )
        )

    def peek(k=0) -> str:
        return src[i + k] if i + k < len(src) else ""

    def adv(n=1):
        nonlocal i, line, col
        for _ in range(n):
            if i >= len(src):
                return
            ch = src[i]
            i += 1
            if ch == "\n":
                line += 1
                col = 1
            else:
                col += 1

    last_real_kind: Optional[str] = None  # includes SEMI
    last_sig_kind: Optional[str] = None  # ignores SEMI

    def set_last(kind: str):
        nonlocal last_real_kind, last_sig_kind
        last_real_kind = kind
        if kind != "SEMI":
            last_sig_kind = kind

    while i < len(src):
        ch = peek()
        two = ch + peek(1)

        # whitespace
        if ch in " \t\r":
            adv()
            continue

        # newline => maybe insert SEMI
        if ch == "\n":
            adv()
            if nest == 0 and last_sig_kind in STMT_END:
                emit("SEMI", ";", None, line - 1, 0)
                last_real_kind = "SEMI"
            continue

        # Return-spec markers:
        # - "(( ... ))" starts after a signature ")" (end of param list),
        #   possibly separated by whitespace/newlines (SEMI doesn't change last_sig_kind).
        # - " )) " is special only while inside ret_spec.
        if two == "((" and ret_depth == 0 and last_sig_kind == "RPAR":
            emit("((", "((")
            adv(2)
            ret_depth += 1
            set_last("((")
            continue

        if two == "))" and ret_depth > 0:
            emit("))", "))")
            adv(2)
            ret_depth = max(0, ret_depth - 1)
            set_last("))")
            continue

        # inside return spec, -- is a token (void marker)
        if two == "--" and ret_depth > 0:
            emit("--", "--")
            adv(2)
            set_last("--")
            continue

        # outside return spec, -- starts a comment
        if two == "--" and ret_depth == 0:
            adv(2)
            while i < len(src) and peek() != "\n":
                adv()
            continue

        # two-char operators
        if two in ("==", "!=", "<=", ">=", "&&", "||", "=>", "+=", "-=", "*=", "/="):
            emit(two, two)
            adv(2)
            set_last(two)
            continue

        # explicit semicolon
        if ch == ";":
            emit("SEMI", ";")
            adv()
            last_real_kind = "SEMI"
            continue

        # punctuation / operators
        if ch in "()[]{}.,:+-*/%!=<>|":
            kind_map = {
                "(": "LPAR",
                ")": "RPAR",
                "[": "LBRACK",
                "]": "RBRACK",
                "{": "LBRACE",
                "}": "RBRACE",
                ",": "COMMA",
                ".": "DOT",
                ":": "COLON",
                "+": "+",
                "-": "-",
                "*": "*",
                "/": "/",
                "%": "%",
                "!": "!",
                "=": "=",
                "<": "<",
                ">": ">",
                "|": "BAR",
            }
            kind = kind_map[ch]
            emit(kind, ch)
            adv()

            if ch in "([{":
                nest += 1
            elif ch in ")]}":
                nest = max(0, nest - 1)

            set_last(kind)
            continue

        # mut marker (declaration-only; parser enforces usage rules)
        if ch == "?":
            emit("QMARK", "?")
            adv()
            set_last("QMARK")
            continue

        # length operator
        if ch == "#":
            emit("#", "#")
            adv()
            set_last("#")
            continue

        # strings: escaping + interpolation @"..."
        if ch == "@" and peek(1) == '"':
            start_line, start_col = line, col
            adv(2)  # @"
            parts: List[Tuple[str, str]] = []  # ("text", s) or ("var", name)
            buf_interp: List[str] = []

            def flush_buf():
                nonlocal buf_interp
                if buf_interp:
                    parts.append(("text", "".join(buf_interp)))
                    buf_interp = []

            while i < len(src):
                c = peek()

                if c == '"':
                    adv()
                    flush_buf()
                    emit("STR", '@"..."', parts, start_line, start_col)
                    set_last("STR")
                    break

                if c == "\n":
                    raise LexErr(f"Unterminated string at {start_line}:{start_col}")

                # escapes
                if c == "\\":
                    adv()
                    e = peek()
                    if e == "n":
                        buf_interp.append("\n")
                        adv()
                    elif e == "t":
                        buf_interp.append("\t")
                        adv()
                    elif e == "r":
                        buf_interp.append("\r")
                        adv()
                    elif e == "\\":
                        buf_interp.append("\\")
                        adv()
                    elif e == '"':
                        buf_interp.append('"')
                        adv()
                    elif e == "$":
                        buf_interp.append("$")
                        adv()  # \$ -> literal $
                    elif e == "u" and peek(1) == "{":
                        adv(2)  # u{
                        hexbuf: List[str] = []
                        while i < len(src) and peek() != "}":
                            hexbuf.append(peek())
                            adv()
                        if peek() != "}":
                            raise LexErr(f"Bad \\u{{...}} escape at {line}:{col}")
                        adv()  # }
                        code = int("".join(hexbuf), 16)
                        buf_interp.append(chr(code))
                    else:
                        raise LexErr(f"Unknown escape \\{e} at {line}:{col}")
                    continue

                # interpolation: $name (identifier only). Literal $ is written as \$
                if c == "$":
                    # only interpolate if next char begins an identifier
                    if not is_ident_start(peek(1)):
                        buf_interp.append("$")
                        adv()
                        continue

                    flush_buf()
                    adv()  # consume '$'

                    namebuf: List[str] = []
                    while i < len(src) and is_ident_mid(peek()):
                        namebuf.append(peek())
                        adv()

                    name = "".join(namebuf)
                    parts.append(("var", name))
                    continue

                buf_interp.append(c)
                adv()

            continue

        # strings: raw "..."
        if ch == '"':
            start_line, start_col = line, col
            adv()  # opening "
            buf_raw: List[str] = []
            while i < len(src):
                c = peek()
                if c == '"':
                    adv()
                    s = "".join(buf_raw)
                    emit("STR", '"..."', [("text", s)], start_line, start_col)
                    set_last("STR")
                    break
                if c == "\n":
                    raise LexErr(f"Unterminated string at {start_line}:{start_col}")
                buf_raw.append(c)
                adv()
            continue

        # number (int / float)
        if ch.isdigit():
            start_line, start_col = line, col
            num: List[str] = []
            while i < len(src) and peek().isdigit():
                num.append(peek())
                adv()
            if peek() == "." and peek(1).isdigit():
                num.append(".")
                adv()
                while i < len(src) and peek().isdigit():
                    num.append(peek())
                    adv()
                s = "".join(num)
                emit("FLOAT", s, float(s), start_line, start_col)
                set_last("FLOAT")
            else:
                s = "".join(num)
                emit("INT", s, int(s), start_line, start_col)
                set_last("INT")
            continue

        # identifier / keyword (letters/_ only, no digits)
        if is_ident_start(ch):
            start_line, start_col = line, col
            buf: List[str] = []
            while i < len(src) and is_ident_mid(peek()):
                buf.append(peek())
                adv()
            word = "".join(buf)

            if word in KW:
                emit(f"KW_{word}", word)
                set_last(f"KW_{word}")
            else:
                emit("IDENT", word, word, start_line, start_col)
                set_last("IDENT")
            continue

        raise LexErr(f"Unexpected char '{ch}' at {line}:{col}")

    # final implicit ;
    if nest == 0 and last_sig_kind in STMT_END:
        emit("SEMI", ";", None, line, col)
        last_real_kind = "SEMI"

    # filter redundant SEMIs
    out: List[Tok] = []
    for t in toks:
        if t.kind == "SEMI" and out and out[-1].kind == "SEMI":
            continue
        out.append(t)

    return out
