# Ergo Language Reference

This document provides a comprehensive reference for the Ergo programming language, covering its syntax, types, semantics, and standard features.

---

## Table of Contents

1. [Lexical Structure](#lexical-structure)
2. [Types](#types)
3. [Variables and Constants](#variables-and-constants)
4. [Functions](#functions)
5. [Control Flow](#control-flow)
6. [Expressions](#expressions)
7. [Statements](#statements)
8. [Modules and Imports](#modules-and-imports)
9. [Entry Point](#entry-point)
10. [Standard Library](#standard-library)
11. [Comments](#comments)
12. [Errors and Diagnostics](#errors-and-diagnostics)

---

## Lexical Structure

### Identifiers

- Start with a letter or underscore, followed by letters, digits, or underscores.
- Case-sensitive.

Examples:
```
foo
_bar
x1
```

### Keywords

Reserved words include:
```
module, bring, fun, entry, class, pub, lock, seal, def, let, const, if, elif, else, for, match, return, true, false, null, new, in
```

### Literals

- **Integer:** `42`, `-7`
- **Float:** `1.23`
- **Boolean:** `true`, `false`
- **String:** `"hello"`, `"foo\nbar"`
- **Interpolated string:** `@"Hello, $name!"` (identifier interpolation only)

### Operators

- Arithmetic: `+`, `-`, `*`, `/`, `%`
- Comparison: `==`, `!=`, `<`, `>`, `<=`, `>=`
- Logical: `&&`, `||`, `!`
- Assignment: `=`
- Indexing: `[expr]`
- Function call: `name(args...)`

---

## Types

### Primitive Types

- `bool` — Boolean value (`true` or `false`)
- `num` — numeric type (integer or float values)
- `string` — Unicode string
- `any` — dynamic type; can hold any value and is assignable to/from any type

### Numeric Semantics (`num`)

- `num` is the only numeric type. Integer and float literals both have type `num`.
- Use a decimal point to force floating arithmetic (e.g. `1.0`, `3.14`).
- `+`, `-`, `*` operate on `num` and preserve integer or float behavior based on operands.
- `/` performs integer division when both operands are integer literals/values; otherwise it performs floating division.
- `%` is only valid for integer numeric values. Using a floating operand (including literals like `5.0`) is a runtime error.
- Comparisons (`<`, `<=`, `>`, `>=`) compare numeric values.
- `len(...)` returns `num` but is always integer‑valued.

### Composite Types

- **Arrays:** `[T]` — array of type `T`
  - Example: `[num]` is an array of numbers.
- **Qualified types:** `mod.Type` — type defined in another module.

### Function Types

- Functions are declared with parameter and return types using a return spec.
  - Example: `fun add(a = num, b = num) (( num )) { ... }`
- Multiple return types are separated by `;` in the return spec:
  - Example: `(( num; string ))`

---

## Variables and Constants

### Local bindings

```
let x = 5;
let ?count = 0;
const greeting = @"Ergo";
```

- Types are inferred from the initializer.
- `let` is immutable by default; use `let ?name = ...` for mutability.
- `const` declares an immutable local binding.

### Module globals

Use `def` for module-level (global) bindings:

```
def version = @"1.0.0";
def ?counter = 0;
const BUILD = @"dev";
```

- `def` is only allowed at module scope.
- `def ?name = ...` declares a mutable global.
- Globals must be defined before they are used.

---

## Functions

### Declaration

```
fun name(param1 = Type1, param2 = Type2) (( ReturnType )) {
    -- function body
}
```

Example:
```
fun add(a = num, b = num) (( num )) {
    a + b
}
```

### Calling Functions

```
let result = add(2, 3);
```

---

## Control Flow

### If/Else

```
if condition {
    -- statements
} else {
    -- statements
}
```

### For Loop

```
for (init; cond; step) {
    -- statements
}
```

`for` can be used as a `while` by omitting init/step:

```
for (; cond; ) {
    -- statements
}
```

Foreach form:

```
for (item in collection) {
    -- statements
}
```

---

## Expressions

- Literals: `42`, `"foo"`, `true`
- Arithmetic: `a + b * 2`
- Logical: `x && y`, `!flag`
- Function call: `foo(1, 2)`
- Array indexing: `arr[0]`
- String interpolation: `@"Hello, $name!"`
- Match expression:
```
match x: 0 => @"zero", _ => @"other"
```
- Lambda expressions:
  - Bar form: `|x = num| x + 1`
  - Arrow form: `(x = num, y = num) => x + y`
  - Block form: `(x) => { x + 1 }`
- Lambdas are non-capturing (they can only use parameters and globals).
- Class construction: `new Foo()`

---

## Semantics

### Truthiness

Values used in conditionals (`if`, `for`, `&&`, `||`, `!`) are coerced to boolean:

- `null` is false
- `bool` is itself
- `num` is false when zero, true otherwise
- `string` is false when empty, true otherwise
- arrays are false when empty, true otherwise
- objects/functions are always true

### Nullability

- `null` can unify with any type and produces a nullable type.
- Accessing members, indexing, calling, or using logical ops on a nullable value is a type error.
- Simple null checks (`x == null` / `x != null`) narrow the type inside an `if`/`else`.

### Short‑Circuit Logic

`&&` and `||` are short‑circuiting:

- `a && b` evaluates `b` only if `a` is truthy.
- `a || b` evaluates `b` only if `a` is falsy.

### Indexing and Bounds

- Arrays: `arr[i]` returns `null` when `i` is out of bounds.
- Arrays: `arr[i] = v` ignores writes when `i` is out of bounds.
- Arrays: `arr.remove(i)` returns `null` when `i` is out of bounds.
- Strings: `s[i]` returns a one‑character string, or `""` if out of bounds.
- Tuples: `t[i]` requires `i` to be an integer literal and must be in range.

### Move and Sealed Classes

- A class declared `seal` can be used as a parameter type only with `move(x)` or `null`.
- `move(x)` requires `x` to be a mutable binding (`let ?x = ...`) and transfers ownership.

---

## Statements

- Variable declaration: `let x = 1;`, `const x = 1;`
- Assignment: `x = x + 1;`
- Function call: `writef("hi");`
- Control flow: `if`, `for`, `match`
- Return: `return expr;` (optional, can omit for last expression in function)

---

## Modules and Imports

### Bringing Modules

Use `bring` to import modules:

```
bring stdr;
bring math;
```

- Standard library modules: `stdr`, `math`, etc.
- User modules: You can bring another Ergo source file (e.g., `utils.ergo`) with `bring utils;` (omit the `.ergo` extension).
- `bring utils.ergo;` is also allowed; module names are derived from the file name (no aliasing in v1).
- Imported modules are **namespaced only**. Use `module.member` to access their members.
- Access module constants: `math.PI`, `utils.MY_CONST`
- Access module functions: `math.funcname(args)`, `utils.helper(x)`
- `stdr` is a special case: its core functions are available unqualified when brought *and* via `stdr.*`.

Name resolution notes:

- `bring` only introduces a module namespace. It does **not** import members into the local scope.
- Local bindings can shadow module names. If you write `let math = ...`, you can no longer access `math.*` from the imported module in that scope. Rename the local to use the module.

---

## Entry Point

Every program must define an entry point:

```
entry () (( -- )) {
    -- program body
}
```

- The entry function takes no arguments and returns nothing.
- Only one `entry` per program.

---

## Standard Library

### `stdr`

- `writef(fmt: string, ...): void` — Print formatted output.
- `readf(fmt: string, ...): (string, any)` — Print a prompt, read a line, and return `(line, parsed)` where `parsed` is a tuple of values parsed from `{}` placeholders (types inferred from the provided argument values, e.g. `0` or `0.0` for num, `@""` for string).
- `write(x: any): void` — Print a value.
- `len(x: any): num` — Length of array or string.
- `is_null(x: any): bool` — True if value is null.
- `str(x: any): string` — Convert a value to a string.

### `math`

- `math.PI: num` — The constant π.
- `math.sin(x: num): num` — Sine function.
- (Expand as needed.)

---

## Comments

- Single-line comments start with `--` and continue to the end of the line.

Example:
```
-- This is a comment
let x = 5; -- Inline comment
```

---

## Errors and Diagnostics

- Syntax errors, type errors, and undefined variables/functions are reported at compile time.
- Parse errors include file, line, and column. Type errors currently include the file path (line/column tracking for type errors is not yet implemented).

---

## Example Program

```
bring stdr;

fun fib(n = num) (( num )) {
    if n <= 1 {
        n
    } else {
        fib(n - 1) + fib(n - 2)
    }
}

entry () (( -- )) {
    for (let ?i = 0; i < 10; i = i + 1) {
        writef(@"fib({}): {}\n", i, fib(i));
    }
}
```

---

For more details and examples, see the `examples/` directory and the quickstart guide in `docs/learn-ergo-in-10-minutes.md`.

---

## Note on Library Usage

When using standard library modules, you must bring them explicitly with `bring modulename;`. Constants are accessed as `module.CONSTANT`, and functions as `module.function(args)`. For example:

```
bring math;
let circumference = 2 * math.PI * r;
let s = math.sin(angle);
```
