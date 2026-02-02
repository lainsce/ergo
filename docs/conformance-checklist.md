# Ergo Conformance Checklist (v1)

Each item is a test requirement. Use these to build a minimal conformance suite.  
Suggested file names are in parentheses.

## Numeric Semantics (`num`)
- [ ] Integer + integer yields integer value (e.g., `2 + 3 == 5`). (`num_int_ops.e`)
- [ ] Float literal forces float arithmetic (`1.0 + 2 == 3.0`). (`num_float_ops.e`)
- [ ] Division of integer literals performs integer division (`5 / 2 == 2`). (`num_div_int.e`)
- [ ] Division with a float operand performs float division (`5 / 2.0 == 2.5`). (`num_div_float.e`)
- [ ] Modulo with float operand traps at runtime (`5.0 % 2`). (`num_mod_float_trap.e`)
- [ ] Unary `-` preserves numeric value (`-(1)` and `-(1.5)`). (`num_unary_neg.e`)

## Truthiness
- [ ] `if 0 { ... } else { ... }` takes else branch. (`truthy_num_zero.e`)
- [ ] `if 1 { ... }` takes then branch. (`truthy_num_nonzero.e`)
- [ ] `if @"")` takes else branch; `if @"x"` takes then. (`truthy_string.e`)
- [ ] `if []` takes else branch; `if [1]` takes then. (`truthy_array.e`)
- [ ] `if null` takes else branch. (`truthy_null.e`)

## Nullability and Narrowing
- [ ] `x = null` unifies to nullable type. (`nullable_unify.e`)
- [ ] `if x != null { x.method() }` typechecks; `if x == null { ... } else { x.method() }` typechecks. (`nullable_narrow.e`)
- [ ] Member access/call/index on nullable value is a type error. (`nullable_errors.e`)

## Short‑Circuit Logic
- [ ] `false && (x = 1)` does not assign; `true || (x = 1)` does not assign. (`short_circuit.e`)
- [ ] `true && (x = 1)` assigns; `false || (x = 1)` assigns. (`short_circuit.e`)

## Indexing and Bounds
- [ ] `arr[-1]` and `arr[len]` return `null`. (`array_oob.e`)
- [ ] `arr[-1] = v` and `arr[len] = v` do nothing. (`array_oob_set.e`)
- [ ] `arr.remove(-1)` returns `null`. (`array_remove_oob.e`)
- [ ] `@"hi"[0] == @"h"` and `@"hi"[2] == @""`. (`string_index.e`)
- [ ] Tuple indexing requires literal and is bounds‑checked. (`tuple_index.e`)

## Move + Sealed Classes
- [ ] Sealed parameter requires `move(x)` or `null`. (`sealed_param.e`)
- [ ] `move(x)` requires `let ?x`. (`move_requires_mut.e`)
- [ ] `move(x)` invalid for non‑identifier expressions. (`move_invalid.e`)

## Modules and Imports
- [ ] Imported modules are namespaced only (`math.sin` required). (`module_namespace.e`)
- [ ] `stdr` prelude functions are available unqualified when brought. (`stdr_prelude.e`)
- [ ] Unknown `module.member` errors are clear. (`module_unknown.e`)

