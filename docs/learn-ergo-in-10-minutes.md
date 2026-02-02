# Learn Ergo in 10 Minutes

Welcome! This guide will introduce you to the basics of the Ergo programming language, focusing on syntax and core features. For more examples, see the `examples/` directory.

---

## Hello, World

```ergo
bring stdr;

entry () (( -- )) {
    writef(@"Hello, world!\n");
}
```
- `bring stdr;` imports the standard library.
- `entry () (( -- )) { ... }` defines the program entry point.

---

## Comments

```ergo
-- This is a comment
writef(@"Comments use --\n");
```

---

## Variables and Types

```ergo
let x = 42;
let y = @"Ergo";
let z = true;
```

---

## Functions

```ergo
fun add(a = num, b = num) (( num )) {
    a + b
}
let result = add(2, 3);
```

---

## Control Flow

### If/Else

```ergo
if x > 0 {
    writef("Positive\n");
} else {
    writef("Non-positive\n");
}
```

### For Loop

```ergo
for (let ?i = 0; i < 5; i = i + 1) {
    writef(@"{}\n", i);
}
```

---

## Arrays

```ergo
let arr = [1, 2, 3];
writef(@"First: {}\n", arr[0]);
```

---

## String Interpolation

```ergo
let name = @"Ergo";
write(@"Hello, $name!\n");
```

---

## Bringing Modules

```ergo
bring math;
let pi = math.PI;
```

Imports are **namespaced only**: you must use `module.member` to access them.
`bring utils.e;` is allowed, but module names come from the file name (no aliasing in v1).
`stdr` is a special case — its core functions are available unqualified when brought.
Local bindings can shadow module names, so avoid `let math = ...` if you need `math.*`.

---

## Entry Point

Every program must have an entry point:

```ergo
entry () (( -- )) {
    -- Your code here
}
```

---

## More Examples

See the `examples/` directory for more sample programs.

---

Happy hacking!
