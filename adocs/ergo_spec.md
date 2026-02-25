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

`cask`, `bring`, `fun`, `macro`, `entry`, `class`, `struct`, `enum`, `pub`, `lock`, `seal`, `def`, `let`, `const`, `if`, `elif`, `else`, `for`, `match`, `return`, `true`, `false`, `null`, `new`, `in`, `break`, `continue`

`cask` is an optional top-level declaration used to assert module identity.

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
   - Escapes are processed: `\n`, `\t`, `\r`, `\\`, `\"`, `\$`, and `\u{...}`.
2. Interpolated string: `@"text $name"`
   - Supports interpolation expressions beginning with `$`.
   - The interpolation parser accepts:
     - `$ident`
     - `$ident.member`
     - `$ident(args...)`
     - `$ident.member(args...)`
     - `$ident[index]`
     - and postfix combinations, e.g. `$obj.get_y()`, `$list.at(0).to_string()`
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
- Built-in cask names include `stdr`, `math`, and `cogito`.
- `bring name` resolves to `name.ergo` for user casks.
- `bring name.ergo` is accepted.
- `stdr` is required in non-stdlib casks.
- Legacy `.e` imports are rejected.

### 4.2 Module Naming

- Module names are derived from file basename (without `.ergo`).
- Optional declaration form:

```ergo
cask mymod
```

- If present, declared cask name must match the file basename.

### 4.3 Entry Constraints

- Exactly one `entry()` is required in the entry module.
- Imported modules cannot declare `entry()`.
- Current diagnostics refer to `init.ergo`; this is a toolchain convention reflected in error text.

## 5. Top-Level Declarations

Allowed top-level declarations:

- `fun` (optionally `pub`)
- `macro`
- `entry`
- nominal declarations: `class`, `struct`, `enum` (with optional `pub` / `lock` / `seal` prefixes)
- `def` (module global, optionally `pub`)
- `const` (module constants, general-purpose, optionally `pub`)

Macro declaration form:

```ergo
macro plussy(arg = num) (( num )) {
    this + arg
}
```

Current macro behavior:

- Macros are compile-time only and are removed during lowering.
- A macro body must lower to a single expression (single expr statement or `return expr`).
- Parameters are substituted by expression cloning (no runtime call overhead).
- Expansion depth is capped to avoid runaway recursion.
- `this` is reserved as the receiver binding inside macro bodies; macro parameters cannot be `this` / `?this`.

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

- Generic placeholders are explicit by naming convention:
  - unqualified type names matching `^[A-Z][A-Z0-9_]*$` are generic placeholders.
  - other unknown names are type errors (prevents typo-driven implicit generics).

### 6.5 Nullability

- Unifying a type with `null` produces a nullable form internally.
- Nullable values are restricted in many operations (member/index/call/arithmetic/comparison).
- Assignability is strict for nullability:
  - assigning/passing/returning `T | null` where `T` is required is a type error,
  - assigning `null` where non-null `T` is required is a type error,
  - assigning non-null `T` into `T | null` remains allowed.
- Null-coalescing operator is supported:
  - `a ?? b` evaluates to `a` when `a != null`, otherwise `b`.

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
- Methods can be declared `pub fun ...` for cross-cask visibility.

### 8.3 Nominal Type Syntax

```ergo
class Point {
    pub x = num
    y = num

    fun init(?this, x = num, y = num) (( -- )) {
        this.x = x
        this.y = y
    }
}
```

Class inheritance syntax (single base class):

```ergo
class Child : base.Parent {
    fun init(?this) (( -- )) { }
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

`enum` currently behaves as a nominal field container (same construction model as `struct`), not as tagged-sum variants.

### 8.4 Nominal Modifiers

- `pub class/struct/enum ...` is parsed.
- `lock class ...` enforces restricted field access (same file or own methods).
- `seal` is only valid on `class` declarations.
- `class Child : Base` is supported for class declarations.
- `seal` acts as a final class restriction for inheritance:
  - inheriting from a sealed class is a compile-time error.
- Inheritance currently enforces base-class validity and sealed-class rejection; it does not yet add broader polymorphic dispatch semantics.

### 8.5 Visibility (`pub`)

- `pub` controls cross-cask (cross-file module) access.
- Non-`pub` symbols are cask-private.
- Enforced entities:
  - nominal types (`class` / `struct` / `enum`),
  - top-level `fun`,
  - top-level `def`,
  - top-level `const`,
  - class fields,
  - class methods.

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

Element type:

- `for (x in [T])` binds `x: T`.
- `for (ch in some_string)` binds `ch: string` (single-character string slices).

### 9.4 Return

- `return` or `return expr`.
- For non-void functions (including methods and `entry` when non-void), all reachable paths must return.
- Path-complete return checking is conservative for loops:
  - loops are not treated as guaranteed-return constructs unless a guaranteed return is proven outside loop uncertainty.
  - if a loop may exit and execution can fall through, this is a type error.
- Void-return functions keep existing behavior (no path-complete return requirement).
- Diagnostics include a fallthrough-path reason (for example, missing `else` or a branch that can fall through).

### 9.5 Loop Control

- `break` exits the nearest loop.
- `continue` skips to the next iteration of the nearest loop.
- Using either outside a loop is a type error.

## 10. Expressions

### 10.1 Core Forms

- Literals and identifiers
- Unary: `!x`, `-x`, `#x`
- Binary: `+ - * / % == != < <= > >= && || ??`
- Assignment: `=`, `+=`, `-=`, `*=`, `/=`
- Call: `fn(args...)`
- Member: `a.b`
- Index: `a[i]`
- Object construction: `new Class(...)`
- Receiver macro call sugar: `x !plussy 2` (lowers to member call form `x.plussy(2)` for macro expansion)
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
- Typed array literal annotation: `[...]: [T]` (notably supports empty arrays: `[]: [num]`)
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

`match` is a general conditional expression/statement form over values and patterns; it is not enum-variant destructuring.

### 10.3 Unary `#`

- `#x` is lowered to `stdr.len(x)`.
- Valid for arrays and strings.

### 10.4 Lambda Capture

- Lambdas capture referenced local bindings.
- Captures are materialized in closure environment at lambda creation time.

### 10.5 `move(x)`

- `move(x)` is lowered into a move expression form.
- Enforced model:
  - only identifier operands are accepted,
  - target must be a mutable local binding (`let ?x = ...`),
  - globals, fields, index targets, temporaries, and immutable locals are rejected by typechecking.
- Source invalidation:
  - after `move(x)`, `x` is marked moved and direct use is a type error (`use of moved value`),
  - assigning to `x` reinitializes the binding and clears moved state.
- Interaction rules:
  - passing/returning/field-assignment are supported by using `move(local_ident)` as an expression,
  - closure/typecheck behavior follows normal local-binding analysis with moved-state checks.

### 10.6 Operator Precedence and Associativity

Parser precedence is explicit (lowest to highest):

1. Assignment: `=`, `+=`, `-=`, `*=`, `/=` (right-associative)
2. Null-coalescing: `??` (left-associative)
3. Logical OR: `||` (left-associative)
4. Logical AND: `&&` (left-associative)
5. Equality: `==`, `!=` (left-associative)
6. Relational: `<`, `<=`, `>`, `>=` (left-associative)
7. Additive: `+`, `-` (left-associative)
8. Multiplicative: `*`, `/`, `%` (left-associative)
9. Unary: `!`, unary `-`, `#` (prefix)
10. Postfix: call `()`, index `[]`, member `.`, macro-call sugar `!name` (highest)

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
- Lint mode can report non-`bool` conditions as warnings (or errors in strict lint mode).

### 11.2 Logical Operators

- `&&` and `||` are short-circuiting.
- Operand evaluation uses truthiness (same model as `if` / `for` conditions).
- Result type is `bool`.

### 11.3 Indexing Behavior

Typing rule (semantic model):

- For arrays, `a[i] : T | null` when `a : [T]` (out-of-bounds reads yield `null`).
- Practical handling patterns:
  - coalesce: `let v = a[i] ?? fallback`
  - null test: `if stdr.is_null(a[i]) { ... }`
  - explicit compare: `if a[i] != null { ... }`

- Arrays:
  - read out-of-bounds -> `null`
  - write out-of-bounds -> ignored
  - `remove(i)` out-of-bounds -> `null`
- Strings:
  - `s[i]` out-of-bounds -> `""`
- Tuples:
  - index must be integer literal and in range (compile-time checked)

### 11.4 Empty Array Literals

- `[]` without annotation is rejected because element type cannot be inferred.
- Use `[]: [T]` to declare element type.

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
