# Cogito GUI Framework Specification

## Overview

Cogito is a declarative GUI framework for the Ergo programming language, providing a unified styling system and cross-platform windowing.

## Client-Side Decorations (CSD)

Cogito supports client-side window decorations for borderless windows, enabling custom title bars and window controls while maintaining native platform behavior for resizing and dragging.

### Hit-Test Regions

When a window has an AppBar and uses borderless mode (`FLAG_WINDOW_UNDECORATED`), the framework automatically configures hit-test regions:

| Region | Behavior | Priority |
|--------|----------|----------|
| 8px window edges | Resize handles (8 directions) | Highest |
| Window control buttons (close/min/max) | Clickable, non-draggable | High |
| AppBar title area | Draggable (moves window) | Normal |
| All other areas | Normal window content | Low |

### Environment Variables

- `COGITO_DEBUG_CSD=1` — Enable debug overlay showing hit-test regions:
  - **Blue**: Resize borders (8px)
  - **Green**: Draggable AppBar area
  - **Red**: Window control button areas

### Platform Behavior

**macOS**: Uses native `performWindowDragWithEvent:` for dragging. Traffic light buttons are hidden and replaced with custom-drawn controls in the AppBar.

**Linux/Windows**: Uses SDL3's `SDL_SetWindowHitTest()` callback system. The framework provides `cogito_window_hit_test()` which returns appropriate hit regions based on mouse position.

### API

```c
// Hit-test results
typedef enum {
  COGITO_HITTEST_NORMAL = 0,
  COGITO_HITTEST_DRAGGABLE,
  COGITO_HITTEST_RESIZE_TOPLEFT,
  COGITO_HITTEST_RESIZE_TOP,
  // ... (8 resize directions)
} CogitoHitTestResult;

// Set custom hit-test callback for a window
void cogito_window_set_hit_test(CogitoWindow* window, 
                                CogitoHitTestCallback callback, 
                                void* userdata);

// Enable debug overlay
void cogito_window_set_debug_overlay(CogitoWindow* window, bool enable);
```

## Build Configuration

### Dependencies

- **SDL3**: Core windowing and input (required)
- **SDL3_ttf**: Font rendering (optional, can use stb_truetype)
- **OpenGL 3.3+**: Rendering backend (or Metal on macOS)

### Meson Options

```meson
# Prefer SDL3 from pkg-config, fallback to CMake
dependency('sdl3', required: false) or dependency('SDL3', required: true)
```

## Runtime Behavior

### Window Creation

Windows with AppBars automatically use borderless mode on non-macOS platforms. The AppBar provides:

1. **Window controls**: Close (red), minimize (yellow), maximize (green) circles
2. **Draggable region**: Title area for moving the window
3. **Custom content**: Additional buttons via `cogito.appbar_add_button()`

### Event Flow

1. SDL3 polls events via `cogito_pump_events()`
2. Window-specific events routed by `SDL_WindowID`
3. Hit-test callback evaluated for mouse events on borderless windows
4. Cogito node tree processes input for widgets

## Migration from Raylib

When porting from raylib-based Cogito:

1. Replace `InitWindow()` / `CloseWindow()` with `cogito_window_create()` / `cogito_window_destroy()`
2. Replace `WindowShouldClose()` with event loop checking `COGITO_EVENT_WINDOW_CLOSE`
3. Replace `GetMousePosition()` with `COGITO_EVENT_MOUSE_MOVE` events
4. Replace `BeginDrawing()` / `EndDrawing()` with renderer begin/end calls
5. Enable CSD by adding AppBar to window (automatic on supported platforms)

## Debug Features

### Computed Style Dump

Cogito provides a debug utility to inspect the computed styles of any widget node, showing which SUM rules contributed each property.

#### Enabling Style Debug

Set the environment variable:
```bash
COGITO_DEBUG_STYLE=1 ./my_app
```

#### API Functions

```c
// Dump style for a single node
void cogito_style_dump(CogitoNode* node);

// Dump style tree recursively
void cogito_style_dump_tree(CogitoNode* root, int depth);

// Demo: dump a Button with current theme
void cogito_style_dump_button_demo(void);
```

#### Output Format

The style dump shows:
- **Property name**: The CSS-like property name
- **Computed value**: The final value after cascade
- **Origin**: Source of the value in brackets `[type:selector]`

Origin types:
- `base` - Base theme defaults
- `kind` - Per-widget kind styles
- `class` - Class-based styles (mono, tabular, outlined, text)
- `custom` - User-defined custom classes
- `state` - State variants (hover, active, checked, disabled)
- `inline` - Direct node settings

Example output:
```
=== Style Dump: button ===
Class: fancy
----------------------------------------
  background           = #007AFF            [kind:button]
  color                = #FFFFFF            [kind:button]
  border-radius        = 20                 [kind:button]
  border-width         = 0                  [base:*]
  padding              = 10 24 10 24        [kind:button]
  font-size            = 13                 [base:*]
  opacity              = 1.00               [default]
========================================
```

#### Usage Example

```c
// In your app code (guarded by debug flag)
if (cogito_debug_style()) {
    CogitoNode* my_button = cogito_button("Click me");
    cogito_style_dump(my_button);
}
```

Or use the demo function to see a button's computed style under the current theme:
```c
cogito_style_dump_button_demo();
```

## SDL3 Backend and Ergo Integration

Cogito now uses SDL3 as its primary backend for windowing, input, and rendering. This provides better cross-platform support and modern GPU acceleration.

### Building with SDL3

#### Dependencies

- **SDL3** (required): Windowing and input
- **OpenGL 3.3+** (required): Rendering backend
- **FreeType2** (optional): Font rendering

#### Build Commands

```bash
# Configure with Meson
meson setup build --prefix=/usr

# Build
meson compile -C build

# Install (optional)
meson install -C build
```

#### Environment Variables

- `COGITO_DEBUG_CSD=1` — Show client-side decoration hit-test regions
- `COGITO_DEBUG_STYLE=1` — Enable computed style dump debugging
- `COGITO_FONT=/path/to/font.ttf` — Custom font path
- `COGITO_FONT_SIZE=14` — Base font size
- `COGITO_TARGET_FPS=60` — Frame rate limit (default: monitor refresh)

### Ergo Example: SDL3 Demo

The `ergo_sdl3_demo.ergo` example demonstrates:
- AppBar with window controls
- SUM-styled button with click counter
- Scroller with clipped content
- Clean quit on window close

```ergo
bring stdr
bring cogito

fun build_ui(win = cogito.Window) (( -- )) {
    let main_vstack = cogito.vstack()
    
    -- AppBar at top
    let appbar = cogito.appbar(@"Cogito SDL3", @"Backend Demo")
    main_vstack.add(appbar)
    
    -- Scroller for main content
    let scroller = cogito.scroller()
    scroller.set_vexpand(true)
    
    let content = cogito.vstack()
    content.set_gap(16)
    
    let btn = cogito.button(@"Click Me!")
    btn.set_class(@"primary")
    content.add(btn)
    
    scroller.add(content)
    main_vstack.add(scroller)
    win.add(main_vstack)
}

entry () (( -- )) {
    let app = cogito.app()
    let win = cogito.window_title(@"Cogito SDL3 Demo")
    win.build(build_ui)
    win.set_resizable(true)
    app.run(win)
}
```

### Running the Demo

```bash
# From the Ergo project root
ergo run cogito/examples/ergo_sdl3_demo.ergo

# Or compile first
ergo build cogito/examples/ergo_sdl3_demo.ergo -o sdl3_demo
./sdl3_demo
```

### API Stability

The public C API (`cogito.h`) and Ergo bindings remain stable. The SDL3 backend is an internal implementation detail—existing Ergo code continues to work without changes.

Key preserved APIs:
- `cogito.app()` / `cogito.app_run()`
- `cogito.window_title()` / `window.build()`
- All widget constructors (`cogito.button()`, `cogito.scroller()`, etc.)
- Event handlers (`on_click`, `on_change`, etc.)
- SUM theme loading (`cogito.load_sum_file()`)

## Widget Inspector

Cogito includes a built-in widget inspector for debugging UI hierarchies and inspecting computed styles. The inspector opens as a second SDL window when triggered.

### Enabling the Inspector

The inspector is only available in debug builds. Enable it by:

1. **Build with debug flag**: Compile with `-DCOGITO_DEBUG` or set `COGITO_DEBUG=1` in your build environment
2. **Environment variable**: Set `COGITO_INSPECTOR=1` at runtime
3. **Keyboard shortcut**: Press **Ctrl+Shift+I** in any Cogito window

### Inspector Layout

The inspector window is divided into two panels:

| Panel | Content |
|-------|---------|
| **Left** | Widget tree view with expand/collapse indicators |
| **Right** | Selected node details and computed style dump |

### Widget Tree View

- Shows complete hierarchy from root window to leaf widgets
- Indentation indicates nesting depth
- Expand/collapse triangles for containers
- Click any node to select it
- Displays: `kind.class` format (e.g., `button.primary`, `vstack`)

### Selected Node Details

The right panel displays:

| Section | Information |
|---------|-------------|
| **Identity** | Type, ID, class name |
| **Geometry** | Position (x, y), size (w, h) |
| **Clip Rect** | Current scissor/clip bounds |
| **Visibility** | visible/hidden, opacity value |
| **Input State** | hover, active, focused, disabled flags |
| **SUM Style** | Computed style dump with origin info |

### Style Dump Integration

The inspector automatically shows the SUM computed-style dump for the selected node, including:

- All resolved property values
- Origin tracking (base, kind, class, state, inline)
- Color values in hex format
- Geometry values in sp units

### Highlighting

When a widget is selected in the inspector:
- A **red outline** appears around the widget in the main window
- The outline updates as the widget moves/resizes
- Highlight is only visible when inspector is open

### Example Usage

```bash
# Build with debug enabled
meson setup build -DCOGITO_DEBUG=true
meson compile -C build

# Run with inspector enabled
COGITO_INSPECTOR=1 ./build/my_app

# Or toggle at runtime with Ctrl+Shift+I
```

### Limitations

- Inspector is debug-build only (stripped from release builds)
- Maximum one inspector window per application
- Cannot inspect the inspector itself (no recursion)
- Selection highlight requires main window to be rendering

## Native Window Handle Access

Cogito provides access to platform-native window handles for advanced customization. This enables future features like frosted glass effects, custom window shapes, and platform-specific styling.

### Retrieving Native Handles

Each `CogitoWindow` stores a reference to its platform-native window handle:

| Platform | Handle Type | SDL Property |
|----------|-------------|--------------|
| **macOS** | `NSWindow*` | `SDL_PROP_WINDOW_COCOA_WINDOW_POINTER` |
| **Windows** | `HWND` | `SDL_PROP_WINDOW_WIN32_HWND_POINTER` |
| **Linux (X11)** | `Window` (unsigned long) | `SDL_PROP_WINDOW_X11_WINDOW_NUMBER` |
| **Linux (Wayland)** | `wl_surface*` | `SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER` |

### Public API

```c
// Get native window handle (returns NULL if unavailable)
void* cogito_window_get_native_handle(CogitoWindow* window);

// Check if handle is valid
bool cogito_window_has_native_handle(CogitoWindow* window);
```

### Debug Logging

Enable native handle debugging:

```bash
COGITO_DEBUG_NATIVE=1 ./my_app
```

Logs handle retrieval success/failure per window:
```
[COGITO_NATIVE] Platform initialized, native handle debugging enabled
[COGITO_NATIVE] retrieve: window=0x7f8b3c0, handle=0x7f8b5000 [SUCCESS]
[COGITO_NATIVE] destroy: window=0x7f8b3c0, handle=0x7f8b5000 [SUCCESS]
```

### Future Hook Attachment

The native handle seam enables future platform-specific effects:

| Feature | macOS Implementation | Windows Implementation |
|---------|-------------------|----------------------|
| **Frosted Glass** | `NSVisualEffectView` with `NSWindow` contentView | `DwmSetWindowAttribute` with `DWMWA_USE_HOST_BACKDROPBRUSH` |
| **Rounded Corners** | `NSWindow` `cornerRadius` property | `DwmSetWindowAttribute` with `DWMWA_WINDOW_CORNER_PREFERENCE` |
| **Window Shadow** | `NSWindow` `hasShadow` property | `CS_DROPSHADOW` class style |
| **Traffic Lights** | `NSWindow` `standardWindowButton:` | Custom title bar with `DwmExtendFrameIntoClientArea` |

Example future usage:
```c
// Platform-specific frosted glass effect
void cogito_window_set_frosted_glass(CogitoWindow* window, bool enable) {
    void* native = cogito_window_get_native_handle(window);
    if (!native) return;
    
#if defined(__APPLE__)
    // Attach NSVisualEffectView to NSWindow
    cogito_macos_set_frosted_glass(native, enable);
#elif defined(_WIN32)
    // Use DWM backdrop brush
    cogito_win32_set_frosted_glass(native, enable);
#endif
}
```

## Multi-Window Support

Cogito supports multiple simultaneous windows with independent content and proper event routing.

### Window Management

- **Maximum windows**: 8 simultaneous windows (configurable at compile time)
- **Close policy**: When the last window closes, the application exits
- **Focus tracking**: Only the focused window receives keyboard events
- **Mouse routing**: Mouse events are routed to the window under the cursor

### Event Routing by WindowID

All input events are routed to their target window using SDL's window ID:

| Event Type | Routing Behavior |
|------------|------------------|
| `WINDOW_CLOSE` | Sent to specific window that was closed |
| `WINDOW_RESIZE` | Triggers relayout + viewport update for that window only |
| `WINDOW_FOCUS` | Updates focused window for keyboard input |
| `MOUSE_*` | Routed to window containing the cursor |
| `KEY_*` | Only sent to window with keyboard focus |
| `TEXT_INPUT` | Only sent to focused window |

### Per-Window State

Each window maintains independent:
- **Renderer context**: Separate OpenGL context per window
- **Viewport/scissor**: Updated on resize, independent per window
- **Event queue**: 64-event ring buffer per window
- **Mouse position**: Tracked per-window for hover effects
- **Layout state**: Relayout triggered independently on resize

### Multi-Window Demo

The `multi_window_test.c` example demonstrates:
- Two windows with different sizes and positions
- Independent UI content (vstack vs hstack layouts)
- Separate click counters per window
- Focus tracking and keyboard routing
- Resize handling with independent relayout

```bash
# Build and run multi-window test
meson compile -C build
./build/multi_window_test
```

### Behavior Differences from Single-Window

1. **Menu/tooltip**: Only shown on focused window
2. **Dialogs**: Modal dialogs block input only on their parent window
3. **Drag operations**: Cannot drag between windows (each window is isolated)
4. **Clipboard**: Shared across all windows (system clipboard)

### Limitations

- Maximum 8 windows (increase `COGITO_MAX_WINDOWS` in `13_run.inc` if needed)
- No window-to-window drag-and-drop
- No child/parent window relationships (all windows are top-level)
