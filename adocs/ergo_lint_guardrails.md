# Ergo Silent-Null Guardrails Lint

This lint pass adds diagnostics for common silent-null footguns while staying fully in-spec:

- no syntax changes
- no runtime/evaluation changes
- diagnostics only

Run it with:

- `ergo lint --mode warn <file.ergo>`
- `ergo lint --mode strict <file.ergo>`

Mode behavior:

- `warn`: emits warnings, exits 0
- `strict`: same diagnostics promoted to errors, exits non-zero if any are emitted

## Rules

1. Missing return coverage

- Applies to functions/methods with non-void return type.
- If control can reach the end of the function body without an explicit `return`, emits a diagnostic.
- Message includes the function name and a short branch-focused reason (for example, missing `else` or a fallthrough arm).

2. Indexing value may be null in non-null contexts

- Any indexing expression `a[i]` is treated as potentially null for lint purposes.
- Diagnostic is emitted when that value flows into a non-null context.

Non-null context (lint definition):

- return expression of a non-null function return type
- assignment into a known non-null target
- call argument for a parameter requiring non-null
- member/call receiver usage where null would crash

Accepted in-spec mitigation hints are emitted in diagnostics:

- `??` null-coalescing
- explicit null checks
- `match` handling

3. Implicit truthiness in conditions

- Emits when a non-`bool` expression is used directly as an `if`/`for` condition (including expression-form conditionals).
- Suggests explicit comparison or null check.

## Diagnostic format

Diagnostics print as:

`<warning|error>: path:line:startCol-line:endCol: message`

and include a one-line `hint` where useful.
