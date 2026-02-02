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
let, const, fun, entry, bring, if, elif, else, for, match, return, true, false, null, class, pub, lock, seal, new, in, bool, num, string
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

### Declaration

```
let x = 5;
let name = @"Ergo";
let arr = [1, 2, 3];
```

- Type is inferred from the initializer.
- Variables are immutable by default; use `let ?name = ...` for mutability.

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
- Lambda expression: `|x = num| x + 1`
- Lambdas are non-capturing in v0 (they can only use parameters and globals).
- Class construction: `new Foo()`

---

## Statements

- Variable declaration: `let x = 1;`
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
- User modules: You can bring another Ergo source file (e.g., `utils.e`) with `bring utils;` (omit the `.e` extension).
- Imported modules are **namespaced only**. Use `module.member` to access their members.
- Access module constants: `math.PI`, `utils.MY_CONST`
- Access module functions: `math.funcname(args)`, `utils.helper(x)`
- `stdr` is a special case: its core functions are available unqualified as language keywords *and* via `stdr.*` when brought.

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
- Error messages include file, line, and column information.

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
