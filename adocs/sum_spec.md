# Sum (Style Unified Model) Styling Language — Minimal Spec v1

Sum is a minimal, script-friendly styling language for applying visual properties to a tree of UI nodes.

## 1. Units

### 1.1 `sp`
`sp` is the primary length unit.

- Definition: `1sp = 254/3 µm`
- Rendering model: device DPI is fixed at **300 DPI**
- Therefore: **`px = sp`**

Implementations MUST keep length values as floating-point pixels internally.

## 2. Source file format

- Text is UTF-8.
- Newlines separate logical lines.
- Indentation defines blocks. Indentation is either:
  - spaces only, or
  - tabs only,
  but MUST NOT be mixed within a file.
- Empty lines are allowed.
- Comments start with `;` and continue to end of line.
- File extension is .sum

## 3. Grammar (informal)

A file is a sequence of **rules**.

A **rule** is:

```
<selector-line>
  <declaration-line>
  <declaration-line>
  ...
```

A **selector-line** is a selector (see §4).

A **declaration-line** is:

```
<property-name> ":" <value>
```

Property names are lowercase ASCII words with `-` allowed.

## 4. Selectors

Sum supports a small subset of CSS-like selectors.

### 4.1 Supported selectors
- Universal: `*`
- Type: `button`
- Class: `.fancy`
- Type + class: `button.fancy`

### 4.2 Matching rules
A selector matches a node if:
- `*` always matches
- `type` matches node type name exactly
- `.class` matches if node has that class
- `type.class` matches if both match

### 4.3 Specificity
Specificity determines tie-breaking:

- `*` → (0)
- `type` → (1)
- `.class` → (10)
- `type.class` → (11)

Higher number wins. If equal specificity, the later rule in the file wins.

## 5. Cascade & inheritance

### 5.1 Cascade order (highest priority last)
1. Earlier rules
2. Later rules (same specificity)
3. Higher specificity

### 5.2 Inheritance
Only these properties inherit by default:
- `font`
- `color`
- `background`
- `border`
- `radius`
- `margin`
- `padding`
- `box-shadow`

## 6. Properties

This minimal spec defines these properties:

- `font`
- `color`
- `background`
- `border`
- `radius`
- `margin`
- `padding`
- `box-shadow`

Unknown properties MUST be ignored.

## 7. Value types

### 7.1 Colors
A color is:

- `#RGB`
- `#RRGGBB`
- `#RRGGBBAA`

Channels are hex. For `#RRGGBBAA`, `AA` is alpha (00–FF).

### 7.2 Lengths
A length is:

- `<number>sp` (preferred)

### 7.3 Identifiers
Keywords are lowercase ASCII identifiers (e.g., `solid`).

### 7.4 Strings
A string is double-quoted. Backslash escapes are implementation-defined (may be omitted in v0.1).

## 8. Property definitions

### 8.1 `font`
Groups: family, size, weight — in this order.

Syntax:
```
font: <family> <size> <weight>
```

- `<family>` is either:
  - a string: `"Inter"`
  - or an identifier without spaces: `Inter`
- `<size>` is a length
- `<weight>` is either:
  - integer 1–1000, or
  - `normal` (=400), `bold` (=700)

Example:
```
font: "Inter" 14sp 600
```

### 8.2 `color`
Syntax:
```
color: <color>
```

### 8.3 `background`
No images in v0.1.

Syntax:
```
background: <color>
```

### 8.4 `border`
Syntax:
```
border: <width> <style> <color>
```

- `<width>` is a length
- `<style>` is one of: `none`, `solid`, `dashed`, `dotted`
- `<color>` is a color

Border width rendering rule:
- If width is `0`, border is off.

### 8.5 `radius`
Controls corner radii.

Syntax:
```
radius: <r1>
radius: <r1> <r2>
radius: <r1> <r2> <r3>
radius: <r1> <r2> <r3> <r4>
```

Each `<rN>` is a length. Corner mapping (CSS-like):
- 1 value: all corners
- 2 values: TL/BR = r1, TR/BL = r2
- 3 values: TL = r1, TR/BL = r2, BR = r3
- 4 values: TL TR BR BL

### 8.6 `margin`
Syntax:
```
margin: <m1>
margin: <m1> <m2>
margin: <m1> <m2> <m3>
margin: <m1> <m2> <m3> <m4>
```

Mapping (CSS-like):
- 1: all
- 2: vertical = m1, horizontal = m2
- 3: top = m1, horizontal = m2, bottom = m3
- 4: top right bottom left

### 8.7 `padding`
Same syntax and mapping as `margin`.

### 8.8 `box-shadow`
A list of one or more shadow entries.

Syntax:
```
box-shadow: [ <shadow> ; <shadow> ; ... ]
```

Semicolons inside the bracket are optional line separators; newlines are allowed.

Each `<shadow>` is:
```
<dx> <dy> [<blur>] [<spread>] <color> [inset]
```

- `<dx>`, `<dy>`, `<blur>`, `<spread>` are lengths
- `<color>` is a color
- `inset` is an optional keyword
- Missing `<blur>` defaults to `0`
- Missing `<spread>` defaults to `0`

Example:
```
box-shadow: [
  0 0 0 1sp #0000002d
]
```

## 9. Example

```sum
; Base
*
  color: #222
  background: #fafafa
  font: "Inter" 14sp 400

button.fancy
  color: #fff
  background: #007AFF
  border: 1sp solid #0047cc
  radius: 99sp 0 99sp 0
  box-shadow: [
    0 0 0 1sp #0000002d
  ]
```

## 10. Error handling

- A rule with an invalid selector line MUST be ignored.
- A declaration with an invalid value MUST be ignored (other declarations in the same rule still apply).
- Unknown properties MUST be ignored.
- If a file mixes tabs and spaces for indentation, the implementation MAY reject the file or treat it as invalid.
