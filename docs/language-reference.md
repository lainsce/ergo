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
let, const, fun, entry, bring, if, elif, else, for, match, return, true, false, null, class, pub, lock, seal, new, in, int, float, bool, char, byte, string
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

- `int` — 64-bit signed integer
- `float` — double-precision floating point
- `bool` — Boolean value (`true` or `false`)
- `char` — Unicode scalar value
- `byte` — 0–255 integer
- `string` — Unicode string

### Composite Types

- **Arrays:** `[T]` — array of type `T`
  - Example: `[int]` is an array of integers.

### Function Types

- Functions are declared with parameter and return types using a return spec.
  - Example: `fun add(a = int, b = int) (( int )) { ... }`
- Multiple return types are separated by `;` in the return spec:
  - Example: `(( int; string ))`

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
fun add(a = int, b = int) (( int )) {
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
- Lambda expression: `|x = int| x + 1`
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
- Access module constants: `math.PI`, `utils.MY_CONST`
- Access module functions: `math.funcname(args)`, `utils.helper(x)`

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
- `readf(fmt: string, ...): void` — Read formatted input.
- `write(x: any): void` — Print a value.
- `len(x: any): int` — Length of array or string.
- `is_null(x: any): bool` — True if value is null.

### `math`

- `math.PI: float` — The constant π.
- `math.sin(x: float): float` — Sine function.
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

fun fib(n = int) (( int )) {
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
