# Cogito SDL3 Migration Plan

## Phase 0 — Repo + Spec Mapping (COMPLETED)

### Spec Understanding
- **Source of truth**: `adocs/cogito_spec.md`
- **Key requirements**:
  - SDL3-based windowing/input/render layer (no raylib)
  - Linux: Wayland-first behavior
  - macOS: SDL3 GPU API (Metal backend)
  - Windows: DX12 via SDL3 GPU
  - CSD (Client-Side Decorations) with hit-testing
  - Multi-window support (up to 8 windows)
  - Debug flags: `COGITO_DEBUG_CSD`, `COGITO_DEBUG_STYLE`, `COGITO_DEBUG_NATIVE`, `COGITO_INSPECTOR`

### Raylib Symbol Usage Analysis (COMPLETED)

**Files with raylib dependencies (NOW FIXED):**
- `cogito/c/03_text.inc` - `LoadFontEx`, `UnloadFont`, `MeasureTextEx`, `Font` type
- `cogito/c/11_menu.inc` - `GetScreenWidth`, `GetScreenHeight`, `DrawRectangleRounded`, `DrawTextEx`, `DrawRectangleRec`, `DrawRectangleRoundedLines`, `DrawRectangleLinesEx`
- `cogito/c/12_draw.inc` - `GetMousePosition`
- `cogito/c/13_run.inc` - `InitWindow`, `SetWindowSize`, `WindowShouldClose`, `CloseWindow`, `BeginDrawing`, `EndDrawing`, `ClearBackground`, `GetMousePosition`, `IsMouseButtonDown`, `IsMouseButtonPressed`, `IsKeyDown`, `IsKeyPressed`, `GetCharPressed`, `GetMouseWheelMove`, `SetTargetFPS`, `GetFrameTime`, `MinimizeWindow`, `IsWindowMaximized`, `RestoreWindow`, `MaximizeWindow`

**Widget files converted (Color → CogitoColor, raylib → backend):**
- ✅ `cogito/c/widgets/draw/button.inc`
- ✅ `cogito/c/widgets/draw/segmented.inc`
- ✅ `cogito/c/widgets/draw/window.inc`
- ✅ `cogito/c/widgets/draw/treeview.inc`
- ✅ `cogito/c/widgets/draw/dropdown.inc`
- ✅ `cogito/c/widgets/draw/dialog.inc`
- ✅ `cogito/c/widgets/draw/toast.inc`
- ✅ `cogito/c/widgets/draw/textfield.inc`
- ✅ `cogito/c/widgets/draw/progress.inc`
- ✅ `cogito/c/widgets/draw/slider.inc`
- ✅ `cogito/c/widgets/draw/switch.inc`
- ✅ `cogito/c/widgets/draw/checkbox.inc`
- ✅ `cogito/c/widgets/draw/chip.inc`
- ✅ `cogito/c/widgets/draw/fab.inc`
- ✅ `cogito/c/widgets/draw/iconbtn.inc`
- ✅ `cogito/c/widgets/draw/label.inc`
- ✅ `cogito/c/widgets/draw/image.inc`
- ✅ `cogito/c/widgets/draw/tabs.inc`
- ✅ `cogito/c/widgets/draw/stepper.inc`
- ✅ `cogito/c/widgets/draw/searchfield.inc`
- ✅ `cogito/c/widgets/draw/bottom_nav.inc`
- ✅ `cogito/c/widgets/draw/nav_rail.inc`
- ✅ `cogito/c/widgets/draw/list.inc`
- ✅ `cogito/c/widgets/draw/scroller.inc`
- ✅ `cogito/c/widgets/draw/dialog_slot.inc`
- ✅ `cogito/c/widgets/draw/colorpicker.inc`
- ✅ `cogito/c/widgets/draw/datepicker.inc`
- ✅ `cogito/c/widgets/draw/grid.inc`
- ✅ `cogito/c/widgets/draw/textview.inc`
- ✅ `cogito/c/widgets/draw/appbar.inc`
- ✅ `cogito/c/widgets/interaction/appbar.inc`

**Core files converted:**
- ✅ `cogito/c/02_theme.inc` - Color → CogitoColor, mouse input → backend
- ✅ `cogito/c/06_colorpicker.inc` - Color → CogitoColor
- ✅ `cogito/c/10_sum.inc` - Color → CogitoColor, SetWindowSize → backend
- ✅ `cogito/c/12_draw.inc` - GetMousePosition → backend
- ✅ `cogito/c/13_run.inc` - Full SDL3 backend integration

### Backend Abstraction Layer (COMPLETED)

**Created files:**
- ✅ `cogito/c/backend/backend.h` - Backend interface definition
- ✅ `cogito/c/backend/backend_sdl3.c` - SDL3 implementation
- ✅ `cogito/c/backend/csd.h` - CSD hit-test definitions
- ✅ `cogito/c/backend/csd_sdl3.c` - CSD implementation

**Backend API includes:**
- Window management (create/destroy/show/hide/set_size/get_size)
- Frame rendering (begin_frame/end_frame/present/clear)
- Event loop (poll_events/window_should_close)
- Input (mouse position, buttons, keyboard, text input)
- Time (get_time/sleep)
- Drawing primitives (rect, line, circle)
- Text (font_load/unload, measure, draw)
- Textures (create/destroy/draw)
- CSD support (hit-test callbacks)

### Build System (COMPLETED)

**Updated `cogito/meson.build`:**
- ✅ Removed raylib dependency
- ✅ Added SDL3 dependency (pkg-config + fallback)
- ✅ Added SDL3_ttf dependency
- ✅ Added backend source files
- ✅ Added backend include directory

---

## Phase 1 — SDL3 Core: Window + Event Loop + Frame Present (COMPLETED)

### Implementation Status
- ✅ SDL3 initialization with GPU device
- ✅ Window creation with borderless support
- ✅ Event loop with proper event routing
- ✅ Frame begin/end/present cycle
- ✅ Clear background rendering
- ✅ Window resize handling
- ✅ Drag/resize freeze protection via SDL event loop

### Files Modified
- ✅ `cogito/c/13_run.inc` - Main event loop using `cogito_backend`

---

## Phase 2 — Replace Drawing/Text/Input Used by Widgets (IN PROGRESS)

### Input (COMPLETED)
- ✅ Mouse position via `cogito_backend->get_mouse_position()`
- ✅ Mouse buttons via `cogito_backend->is_mouse_button_down/pressed/released()`
- ✅ Keyboard via `cogito_backend->is_key_down/pressed()`
- ✅ Text input via `cogito_backend->get_char_pressed()`
- ✅ Mouse wheel via `cogito_backend->get_mouse_wheel_move()`

### Drawing (PARTIALLY COMPLETE)
- ✅ `cogito_draw_rect()` - Backend call
- ✅ `cogito_draw_rect_lines()` - Backend call
- ✅ `cogito_draw_line()` - Backend call
- ✅ `cogito_draw_circle()` - Backend call
- ✅ `cogito_draw_circle_lines()` - Backend call
- ✅ `cogito_draw_shadow()` - Stub (needs implementation)
- ⏸️ `cogito_draw_texture()` - TODO (stub in backend)
- ⏸️ `cogito_draw_texture_pro()` - TODO (stub in backend)

### Text (IN PROGRESS)
- ✅ Font loading via `cogito_backend->font_load()`
- ✅ Font unloading via `cogito_backend->font_unload()`
- ✅ Text measurement via `cogito_backend->text_measure_width/height()`
- ⏸️ Text drawing via `cogito_backend->draw_text()` - Stub (needs GPU implementation)

### Remaining Widget Conversions
- ✅ All widget draw files converted to use `CogitoColor` and backend calls
- ✅ All widget interaction files reviewed (no raylib dependencies found)

---

## Phase 3 — Spec Features: CSD + Debug Flags + Multi-Window (IN PROGRESS)

### CSD Hit-Testing (IMPLEMENTED)
- ✅ `cogito/c/backend/csd.h` - Hit-test region definitions
- ✅ `cogito/c/backend/csd_sdl3.c` - SDL3 hit-test implementation
- ✅ `SDL_SetWindowHitTest()` integration
- ✅ 8px resize borders (8 directions)
- ✅ Window control buttons (close/min/max)
- ✅ AppBar title area (draggable)
- ✅ Debug overlay support

### Debug Flags (IMPLEMENTED)
- ✅ `COGITO_DEBUG_CSD=1` - Debug overlay for hit regions
- ✅ `COGITO_DEBUG_STYLE=1` - Style dump (structure)
- ✅ `COGITO_DEBUG_NATIVE=1` - Native handle info
- ✅ `COGITO_INSPECTOR=1` - Inspector toggle (Ctrl+Shift+I)

### Multi-Window Support (IMPLEMENTED)
- ✅ Window registry (`CogitoWindowRegistry`)
- ✅ Up to 8 windows (`COGITO_MAX_WINDOWS`)
- ✅ Event routing by `SDL_WindowID`
- ✅ Focus tracking
- ✅ Mouse routing to window under cursor
- ✅ App exit when last window closes

### Platform-Specific (IMPLEMENTED)
- ✅ macOS: Native window handle access
- ✅ Windows: Native window handle access
- ✅ Linux: Wayland + X11 native handle access
- ✅ macOS: Borderless disabled (uses native decorations)

---

## Verification Checklist

### Build Verification
```bash
# Clean build from scratch
meson setup build --wipe
meson compile -C build
```

### Raylib Removal Verification
```bash
# Should return NO matches
rg -n "raylib|InitWindow|BeginDrawing|DrawTextEx|MeasureTextEx" cogito/
```

### Current Status
- ✅ `cogito/meson.build` - No raylib dependency
- ✅ Backend abstraction layer implemented
- ✅ SDL3 integration complete
- ✅ All widget files use `CogitoColor`
- ✅ All widget files use backend calls
- ⏸️ GPU rendering implementation (stubs in place)
- ⏸️ Text rendering implementation (stubs in place)

### Next Steps for Full Completion
1. Implement GPU rendering pipeline in `backend_sdl3.c`
2. Implement text rendering using SDL_ttf + GPU textures
3. Implement texture loading and drawing
4. Test on all three platforms (Linux/Wayland, macOS, Windows)
5. Verify CSD hit-testing on each platform

---

## Summary

**COMPLETED:**
- Phase 0: Spec mapping and raylib dependency identification
- Phase 1: SDL3 core (window, event loop, frame present)
- Phase 2: Input system conversion, widget Color→CogitoColor conversion
- Phase 3: CSD hit-testing, debug flags, multi-window registry

**REMAINING:**
- GPU rendering implementation (currently stubs)
- Text rendering implementation (currently stubs)
- Full platform testing

The codebase is now **raylib-free** and uses the SDL3 backend abstraction layer. The drawing functions are stubbed and need GPU implementation for full visual rendering.
