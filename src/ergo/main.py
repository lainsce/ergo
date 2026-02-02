#!/usr/bin/env python3
"""
Ergo Compiler CLI Entry Point

Usage:
    ergo <source.e> [--emit-c out.c]
    ergo run <source.e>

- Parses, typechecks, and lowers Ergo source files.
- By default, prints the lowered program as JSON.
- With --emit-c, emits C code to the specified file.
"""

import json
import os
import shutil
import subprocess
import sys
import tempfile
from dataclasses import asdict

from .ast import EntryDecl, Module, Program
from .codegen import emit_c
from .lexer import LexErr, ParseErr, lex
from .parser import Parser
from .typechecker import TypeErr, lower_program, typecheck_program


def load_project(entry_path: str) -> Program:
    entry_path = os.path.abspath(entry_path)
    root_dir = os.path.dirname(entry_path)
    visited = {}

    def load_file(path: str) -> Module:
        ap = os.path.abspath(path)
        if ap in visited:
            return visited[ap]
        with open(ap, "r", encoding="utf-8") as f:
            src = f.read()
        toks = lex(src)
        p = Parser(toks, ap)
        mod = p.parse_module()
        visited[ap] = mod

        stdlib_dir = os.path.join(os.path.dirname(__file__), "stdlib")
        is_stdlib = os.path.commonpath([ap, stdlib_dir]) == stdlib_dir

        # Enforce: every non-stdlib file must explicitly import stdr
        if not is_stdlib and not any(imp.name == "stdr" for imp in mod.imports):
            raise ParseErr(f"{ap}: missing required `bring stdr;`")

        stdlib_dir = os.path.join(os.path.dirname(__file__), "stdlib")
        for imp in mod.imports:
            if imp.name == "stdr":
                stdr_path = os.path.join(stdlib_dir, "stdr.e")
                if not os.path.isfile(stdr_path):
                    raise ParseErr(f"{ap}: stdr.e not found in stdlib")
                load_file(stdr_path)
                continue
            if imp.name == "math":
                math_path = os.path.join(stdlib_dir, "math.e")
                if not os.path.isfile(math_path):
                    raise ParseErr(f"{ap}: math.e not found in stdlib")
                load_file(math_path)
                continue
            # Allow bring of user modules with or without .e extension
            mod_filename = imp.name
            if not mod_filename.endswith(".e"):
                mod_filename += ".e"
            child = os.path.join(root_dir, mod_filename)
            if not os.path.isfile(child):
                raise ParseErr(
                    f"{ap}: bring expects stdr/math or a valid user module (file), got '{imp.name}'"
                )
            load_file(child)
        return mod

    init_mod = load_file(entry_path)

    # enforce entry rules: only init.e contains exactly one entry
    entries_in_init = [d for d in init_mod.decls if isinstance(d, EntryDecl)]
    if len(entries_in_init) != 1:
        raise ParseErr(f"{entry_path}: init.e must contain exactly one entry() decl")
    for path, mod in visited.items():
        if path == entry_path:
            continue
        if any(isinstance(d, EntryDecl) for d in mod.decls):
            raise ParseErr(f"{path}: entry() is only allowed in init.e")

    return Program(mods=list(visited.values()))


def main():
    if len(sys.argv) < 2:
        print("usage: ergo <source.e> [--emit-c out.c]", file=sys.stderr)
        print("       ergo run <source.e>", file=sys.stderr)
        sys.exit(2)

    mode_run = sys.argv[1] == "run"
    if mode_run:
        if len(sys.argv) < 3:
            print("error: run needs a source path", file=sys.stderr)
            sys.exit(2)
        entry = sys.argv[2]
        emit_path = None
    else:
        entry = sys.argv[1]
        emit_path = None
        if "--emit-c" in sys.argv:
            k = sys.argv.index("--emit-c")
            if k + 1 >= len(sys.argv):
                print("error: --emit-c needs a path", file=sys.stderr)
                sys.exit(2)
            emit_path = sys.argv[k + 1]

    try:
        prog = load_project(entry)
        prog = lower_program(prog)
        typecheck_program(prog)

        if mode_run:
            cc = os.environ.get("CC", "cc")
            if shutil.which(cc) is None:
                raise TypeErr("C compiler not found (set CC or install clang/gcc)")
            out_name = "run.exe" if os.name == "nt" else "run"
            out_path = os.path.join(os.getcwd(), out_name)
            with tempfile.TemporaryDirectory(prefix="ergo_run_") as td:
                c_path = os.path.join(td, "out.c")
                emit_c(prog, c_path)
                res = subprocess.run(
                    [cc, "-O3", "-std=c11", c_path, "-o", out_path], check=False
                )
                if res.returncode != 0:
                    sys.exit(res.returncode)
            res = subprocess.run([out_path], check=False)
            sys.exit(res.returncode)
        elif emit_path:
            emit_c(prog, emit_path)
            print(f"Wrote C: {emit_path}")
        else:
            print(json.dumps(asdict(prog), ensure_ascii=False, indent=2))

    except (LexErr, ParseErr, TypeErr) as e:
        print(f"error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
