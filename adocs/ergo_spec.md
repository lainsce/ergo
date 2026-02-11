# Ergo Language Reference (Working Spec)

This document describes the language behavior currently implemented in this repository.
It is intended as an implementation-facing reference, not a future design document.

## 1. Status and Scope

- Covers lexer/parser/typechecker/codegen behavior currently shipped.
- Prioritizes what compiles and runs today.
- Notes implementation constraints where behavior is incomplete or transitional.

## 2. Lexical Structure

### 2.1 Identifiers

- Start: letter or `_`
- Continue: letter, digit, or `_`
- Case-sensitive.

### 2.2 Keywords

Current reserved words:

`module`, `bring`, `fun`, `entry`, `class`, `struct`, `enum`, `pub`, `lock`, `seal`, `def`, `let`, `const`, `if`, `elif`, `else`, `for`, `match`, `return`, `true`, `false`, `null`, `new`, `in`, `break`, `continue`

Note: `module` is currently reserved but not used as a top-level declaration.

### 2.3 Comments

- Line comments use `--` and continue to end of line.

### 2.4 Statement Terminators

- `;` is supported explicitly.
- Newlines also insert statement terminators automatically in statement-ending contexts.

## 3. Literals and Strings

### 3.1 Numeric Literals

- Integer: `42`
- Float: `3.14`
- Unary `-` is an operator, not part of the numeric token.

### 3.2 Other Literals

- `true`, `false`
- `null`

### 3.3 String Forms

1. Plain string: `"text"`
   - No interpolation.
   - Escapes are not processed in this form.
2. Interpolated string: `@"text $name"`
   - Supports `$ident` interpolation.
   - Supports escapes like `\n`, `\t`, `\r`, `\\`, `\"`, `\$`, and `\u{...}`.

## 4. Modules and Imports

### 4.1 Imports

Use `bring` at module top level:

```ergo
bring stdr
bring math
bring cogito
bring utils
bring utils.ergo
```

Current behavior:

- Imports are namespaced (`math.sin(...)`, `utils.fn(...)`).
- Built-in module names include `stdr`, `math`, and `cogito`.
- `bring name` resolves to `name.ergo` for user modules.
- `bring name.ergo` is accepted.
- `stdr` is required in non-stdlib modules.
- Legacy `.e` imports are rejected.

### 4.2 Module Naming

- Module names are derived from file basename (without `.ergo`).
- There is no explicit `module ...` declaration syntax in current parsing.

### 4.3 Entry Constraints

- Exactly one `entry()` is required in the entry module.
- Imported modules cannot declare `entry()`.
- Current diagnostics refer to `init.ergo`; this is a toolchain convention reflected in error text.

## 5. Top-Level Declarations

Allowed top-level declarations:

- `fun`
- `entry`
- nominal declarations: `class`, `struct`, `enum` (with optional `pub` / `lock` / `seal` prefixes)
- `def` (module global)
- `const` (currently enforced only for `stdr` / `math` modules)

## 6. Types

### 6.1 Primitive Types

- `num`
- `bool`
- `string`
- `any`
- `null` (literal/null type, participates in nullable unification)

### 6.2 Composite Types

- Arrays: `[T]`
- Tuples: expression tuples `(a, b, c)` and multi-return tuples
- Classes: `ClassName` or `mod.ClassName`

### 6.3 Function Signatures

Function return syntax:

- Void: `(( -- ))`
- Single return: `(( num ))`
- Multi return tuple: `(( num, string ))` or `(( num; string ))`

### 6.4 Generic Type Variables

- Unknown identifier-like type names in signatures are treated as generic placeholders and unified at call sites.

### 6.5 Nullability

- Unifying a type with `null` produces a nullable form internally.
- Nullable values are restricted in many operations (member/index/call/logical/arithmetic).

## 7. Variables and Mutability

- `let x = expr` creates immutable local bindings.
- `let ?x = expr` creates mutable local bindings.
- `const x = expr` creates immutable local constant bindings.
- `def x = expr` creates module global bindings.
- `def ?x = expr` creates mutable module globals.

Assignment requires mutable targets.

## 8. Functions, Methods, and Classes

### 8.1 Functions

```ergo
fun add(a = num, b = num) (( num )) {
    return a + b
}
```

### 8.2 Methods

- Methods are declared inside nominal types (`class` / `struct` / `enum`) with `fun`.
- First parameter must be `this` or `?this`.
- `this`/`?this` is implicit receiver syntax and does not use `= Type`.

### 8.3 Nominal Type Syntax

```ergo
class Point {
    x = num
    y = num

    fun init(?this, x = num, y = num) (( -- )) {
        this.x = x
        this.y = y
    }
}
```

Struct/enum declarations use `=[ ... ]` field lists:

```ergo
struct Color = [
    r = num
    g = num
    b = num
]

enum Result = [
    tag = string
    value = any
]
```

### 8.4 Nominal Modifiers

- `pub class/struct/enum ...` is parsed.
- `lock class ...` enforces restricted field access (same file or own methods).
- `seal class ...` is parsed and stored; extra semantic restrictions are currently limited.

## 9. Statements and Control Flow

### 9.1 Blocks

- `{ ... }` creates statement blocks and local scopes.

### 9.2 If / Elif / Else

Supported forms:

- Block form: `if cond { ... }`
- Paren condition: `if (cond) { ... }`
- Colon single-statement form: `if cond: stmt`

### 9.3 For

1. C-style loop:

```ergo
for (init; cond; step) { ... }
```

2. Foreach:

```ergo
for (item in expr) { ... }
```

Foreach currently supports iterating arrays and strings.

### 9.4 Return

- `return` or `return expr`.
- For non-void functions, explicit `return expr` is the reliable way to set return values.
- Without explicit return, generated code defaults return storage to `null`.

### 9.5 Loop Control

- `break` exits the nearest loop.
- `continue` skips to the next iteration of the nearest loop.
- Using either outside a loop is a type error.

## 10. Expressions

### 10.1 Core Forms

- Literals and identifiers
- Unary: `!x`, `-x`, `#x`
- Binary: `+ - * / % == != < <= > >= && ||`
- Assignment: `=`, `+=`, `-=`, `*=`, `/=`
- Call: `fn(args...)`
- Member: `a.b`
- Index: `a[i]`
- Object construction: `new Class(...)`
- Named-argument constructor form: `Class(field: value, ...)` (constructor shorthand)
- Match: `match x: pat => expr, ...` or block-arm form
- If-expression:
  - `if cond { expr } else { expr }`
  - `if cond: expr elif other: expr else: expr`
- Lambda:
  - `|x = num| x + 1`
  - `(x = num) => x + 1`
  - `(x) => { ... }`
- Array literal: `[a, b, c]`
- Tuple literal: `(a, b, c)`
- Paren grouping: `(expr)`

Notes:

- `if` is supported as both statement and expression.
- `if` expressions require an `else` branch.
- Braced `if` expression branches currently accept a single expression.

### 10.2 Match Patterns

Supported pattern kinds:

- `_` wildcard
- identifier bind
- int literal
- string literal
- bool literal
- `null`

### 10.3 Unary `#`

- `#x` is lowered to `stdr.len(x)`.
- Valid for arrays and strings.

### 10.4 Lambda Capture

- Lambdas capture referenced local bindings.
- Captures are materialized in closure environment at lambda creation time.

### 10.5 `move(x)`

- `move(x)` is lowered into a move expression form.
- Current backend behavior expects mutable local bindings for move targets.

## 11. Runtime/Type Semantics

### 11.1 Condition Evaluation

- `if`/`for` conditions are accepted if non-void.
- Runtime boolean conversion uses truthiness:
  - `null` false
  - `false` false
  - numeric zero false
  - empty string false
  - empty array false
  - others true

### 11.2 Logical Operators

- `&&` and `||` are short-circuiting.
- Typechecking currently expects boolean-typed operands for these operators.

### 11.3 Indexing Behavior

- Arrays:
  - read out-of-bounds -> `null`
  - write out-of-bounds -> ignored
  - `remove(i)` out-of-bounds -> `null`
- Strings:
  - `s[i]` out-of-bounds -> `""`
- Tuples:
  - index must be integer literal and in range (compile-time checked)

### 11.4 Empty Array Literals

- `[]` is rejected because element type cannot be inferred.

## 12. Standard Library Modules

### 12.1 `stdr`

Current core functions:

- `writef(fmt, ...)`
- `readf(fmt, ...hints)` -> `(string, any)`
- `write(x)`
- `len(x)` -> `num`
- `is_null(x)` -> `bool`
- `str(x)` -> `string`
- `read_text_file(path)` -> `any` (`null` on failure)
- `write_text_file(path, text)` -> `bool`
- `open_file_dialog(prompt, extension)` -> `any`
- `save_file_dialog(prompt, default_name, extension)` -> `any`

When `stdr` is brought, core prelude helpers are available unqualified and via `stdr.*`.

### 12.2 `math`

Includes constants and numeric helpers such as:

- constants: `PI`, `TAU`, `E`, ...
- functions: `sin`, `cos`, `tan`, `sqrt`, `abs`, `min`, `max`

## 13. Build-Time Include Directive

The loader supports source inclusion via comment directive:

```ergo
-- @include "relative/path.ergo"
```

This is processed before lexing/parsing.

## 14. Diagnostics

- Lexer/parser diagnostics include path/line/column.
- Typecheck diagnostics include module path and location for most semantic errors.
- Common enforced checks include:
  - unknown names/types/members
  - assignment mutability violations
  - arity mismatches
  - nullable misuse in restricted contexts
  - invalid `entry()` placement/count
