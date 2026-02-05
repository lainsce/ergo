# Ergo Conformance Checklist (v1)

Each item is a test requirement. Use these to build a minimal conformance suite.  
Suggested file names are in parentheses.

## Numeric Semantics (`num`)
- [ ] Integer + integer yields integer value (e.g., `2 + 3 == 5`). (`num_int_ops.ergo`)
- [ ] Float literal forces float arithmetic (`1.0 + 2 == 3.0`). (`num_float_ops.ergo`)
- [ ] Division of integer literals performs integer division (`5 / 2 == 2`). (`num_div_int.ergo`)
- [ ] Division with a float operand performs float division (`5 / 2.0 == 2.5`). (`num_div_float.ergo`)
- [ ] Modulo with float operand traps at runtime (`5.0 % 2`). (`num_mod_float_trap.ergo`)
- [ ] Unary `-` preserves numeric value (`-(1)` and `-(1.5)`). (`num_unary_neg.ergo`)

## Truthiness
- [ ] `if 0 { ... } else { ... }` takes else branch. (`truthy_num_zero.ergo`)
- [ ] `if 1 { ... }` takes then branch. (`truthy_num_nonzero.ergo`)
- [ ] `if @"")` takes else branch; `if @"x"` takes then. (`truthy_string.ergo`)
- [ ] `if []` takes else branch; `if [1]` takes then. (`truthy_array.ergo`)
- [ ] `if null` takes else branch. (`truthy_null.ergo`)

## Nullability and Narrowing
- [ ] `x = null` unifies to nullable type. (`nullable_unify.ergo`)
- [ ] `if x != null { x.method() }` typechecks; `if x == null { ... } else { x.method() }` typechecks. (`nullable_narrow.ergo`)
- [ ] Member access/call/index on nullable value is a type error. (`nullable_errors.ergo`)

## Short‑Circuit Logic
- [ ] `false && (x = 1)` does not assign; `true || (x = 1)` does not assign. (`short_circuit.ergo`)
- [ ] `true && (x = 1)` assigns; `false || (x = 1)` assigns. (`short_circuit.ergo`)

## Indexing and Bounds
- [ ] `arr[-1]` and `arr[len]` return `null`. (`array_oob.ergo`)
- [ ] `arr[-1] = v` and `arr[len] = v` do nothing. (`array_oob_set.ergo`)
- [ ] `arr.remove(-1)` returns `null`. (`array_remove_oob.ergo`)
- [ ] `@"hi"[0] == @"h"` and `@"hi"[2] == @""`. (`string_index.ergo`)
- [ ] Tuple indexing requires literal and is bounds‑checked. (`tuple_index.ergo`)

## Move + Sealed Classes
- [ ] Sealed parameter requires `move(x)` or `null`. (`sealed_param.ergo`)
- [ ] `move(x)` requires `let ?x`. (`move_requires_mut.ergo`)
- [ ] `move(x)` invalid for non‑identifier expressions. (`move_invalid.ergo`)

## Modules and Imports
- [ ] Imported modules are namespaced only (`math.sin` required). (`module_namespace.ergo`)
- [ ] `stdr` prelude functions are available unqualified when brought. (`stdr_prelude.ergo`)
- [ ] Unknown `module.member` errors are clear. (`module_unknown.ergo`)

## Globals (`def`)
- [ ] `def x = 1; entry { x }` works. (`def_basic.ergo`)
- [ ] `def ?x = 1; x = 2` is allowed. (`def_mut.ergo`)
- [ ] Assigning to immutable `def x` is a type error. (`def_immut_assign.ergo`)
- [ ] Global use before definition is an error. (`def_use_before.ergo`)

## Lambdas
- [ ] Bar form parses and runs: `|x = num| x + 1`. (`lambda_bar.ergo`)
- [ ] Arrow form parses and runs: `(x = num) => x + 1`. (`lambda_arrow.ergo`)
- [ ] Block arrow form parses and runs: `() => { 1 }`. (`lambda_block.ergo`)
