# Cogito GUI Framework Specification

This document defines the framework-level behavior of Cogito.
It focuses on the public programming model and runtime behavior, not build system or backend implementation details.

## 1. Scope

In scope:

- Application, window, and node model.
- Widget set and composition semantics.
- Event and callback behavior.
- Styling via SUM.
- Window integration features exposed by the public API.
- Debug and inspection features relevant to app developers.

Out of scope:

- Meson/dependency setup instructions.
- Renderer/backend internals.
- Platform-specific private implementation hooks.

## 2. Core Abstractions

Cogito exposes three primary opaque handles:

- `cogito_app`: application lifetime and global app metadata.
- `cogito_window`: top-level window/root node.
- `cogito_node`: widget or container in the UI tree.

The framework is declarative by tree construction: app code builds node trees and registers callbacks.

## 3. Lifecycle

### 3.1 App Lifecycle

Public entry points:

- `cogito_app_new()`
- `cogito_app_free()`
- `cogito_app_run(cogito_app* app, cogito_window* window)`

App metadata:

- `cogito_app_set_appid(...)`
- `cogito_app_set_app_name(...)`
- `cogito_app_set_accent_color(...)`
- `cogito_open_url(...)`

### 3.2 Window Lifecycle

Public entry points:

- `cogito_window_new(title, w, h)`
- `cogito_window_free(...)`
- `cogito_window_set_resizable(...)`
- `cogito_window_set_autosize(...)`
- `cogito_window_set_builder(...)`
- `cogito_rebuild_active_window()`

Current behavior: application runtime exits when the last open window closes.

## 4. Node Tree Model

### 4.1 Ownership and Structure

- Nodes form a rooted tree under a window.
- `cogito_node_add(parent, child)` appends child nodes.
- `cogito_node_remove(parent, child)` detaches child nodes.
- `cogito_node_free(node)` releases detached nodes.

Tree/query helpers:

- `cogito_node_get_parent(...)`
- `cogito_node_get_child_count(...)`
- `cogito_node_get_child(...)`
- `cogito_node_window(...)`

### 4.2 Widget Kinds

Current public kinds include:

- Window/container: `window`, `vstack`, `hstack`, `zstack`, `fixed`, `scroller`, `list`, `grid`.
- Text/content: `label`, `image`, `tooltip`.
- Button/container: `buttongroup`
- Actions/inputs: `button`, `iconbtn`, `fab`, `checkbox`, `switch`, `chip`, `textfield`, `textview`, `searchfield`, `dropdown`, `slider`, `stepper`.
- Navigation/surfaces: `tabs`, `view_switcher`, `appbar`, `bottom_toolbar`, `nav_rail`, `bottom_nav`, `dialog`, `dialog_slot`, `toast`, `toasts`, `treeview`, `progress`, `datepicker`, `colorpicker`.

Exact C enum values are defined in `cogito/src/cogito.h`.

## 5. Layout and Common Properties

Common node properties:

- Spacing: margins and padding.
- Alignment: horizontal/vertical and combined alignment.
- Expansion: horizontal/vertical expand flags.
- Gap for container spacing.
- Identity: node ID and class.
- Content/state: text, editable, disabled, tooltip, accessibility label/role.

Key APIs:

- `cogito_node_set_margins(...)`
- `cogito_node_set_padding(...)`
- `cogito_node_set_align(...)`
- `cogito_node_set_halign(...)`
- `cogito_node_set_valign(...)`
- `cogito_node_set_hexpand(...)`
- `cogito_node_set_vexpand(...)`
- `cogito_node_set_gap(...)`
- `cogito_node_set_id(...)`
- `cogito_node_set_class(...)`

## 6. Interaction Model

### 6.1 Callback Types

- Node callbacks: `cogito_node_fn(node, user)`.
- Indexed callbacks: `cogito_index_fn(node, index, user)`.

Common registration points:

- `cogito_node_on_click(...)`
- `cogito_node_on_change(...)`
- `cogito_node_on_select(...)`
- `cogito_node_on_activate(...)`

### 6.2 Pointer Capture

- `cogito_pointer_capture(node)` captures pointer interaction to a node.
- `cogito_pointer_release()` releases capture.

## 7. Widget-Specific APIs

Cogito provides helper APIs for richer widgets, for example:

- Item models and selection: dropdown/tabs/nav rail/bottom nav.
- Value getters/setters: slider/progress/stepper.
- State getters/setters: checkbox/switch/chip.
- Text setters/getters: textfield/textview/searchfield.
- Dialog management: dialog slot and window dialog helpers.
- Appbar controls: add buttons and control layout.

The authoritative API surface is `cogito/src/cogito.h`.

## 8. Styling and Theming (SUM)

### 8.1 Theme Source

SUM is the styling language used by Cogito.
Themes can be loaded from file or source text:

- `cogito_load_sum_file(path)`
- `cogito_load_sum_inline(src)`

### 8.2 Cascade Model (Framework-Level)

Computed style resolution combines:

- Framework defaults.
- Per-kind styles.
- Class/custom-class styles.
- State styles (`hover`, `active`, `checked`, `disabled`, `selection`).
- Direct inline values.

### 8.3 Style Debugging

Debug APIs:

- `cogito_debug_style()`
- `cogito_style_dump(node)`
- `cogito_style_dump_tree(root, depth)`
- `cogito_style_dump_button_demo()`

Environment:

- `COGITO_DEBUG_STYLE=1` enables style debug output.

## 9. Window Integration

### 9.1 Native Handle Access

Public API:

- `cogito_window_get_native_handle(window)`
- `cogito_window_has_native_handle(window)`

This is for integrations that require platform-native window access.

### 9.2 Hit Testing and Client-Side Decorations

Cogito exposes custom hit-test integration for borderless/custom-decorated windows:

- `cogito_window_set_hit_test(window, callback, user)`
- `cogito_window_set_debug_overlay(window, enable)`

Hit-test callback returns `cogito_hit_test_result` values including:

- normal content.
- draggable region.
- directional resize regions.
- close/min/max button regions.

Debug environment:

- `COGITO_DEBUG_CSD=1` enables CSD overlay diagnostics.

## 10. Inspector and Debug Flags

Cogito includes a runtime inspector for widget hierarchy/style debugging.
Current runtime controls:

- `COGITO_INSPECTOR=1` to start with inspector enabled.
- `Ctrl+Shift+I` to toggle inspector during runtime.

Native handle debug logging:

- `COGITO_DEBUG_NATIVE=1`

## 11. Multi-Window Behavior

Cogito runtime supports multiple windows with window-ID-based event routing.

Current behavior:

- Events are routed to the originating window.
- Keyboard focus is tracked per focused window.
- Window close is handled per window; application exits when all windows are closed.

Implementation note:

- Current backend default window registry limit is `COGITO_MAX_WINDOWS` (currently 8).

## 12. Compatibility Notes

- Public C API is declared in `cogito/src/cogito.h`.
- Ergo bindings are expected to preserve this model at the language level.
- Backend and renderer details are intentionally non-normative in this spec.
