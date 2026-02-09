// Cogito SDL3 Backend Implementation - Phase 3
// Uses SDL3 GPU API (Metal on macOS, DX12 on Windows, Vulkan on Linux)

#include "backend.h"
#include "csd.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// Forward Declarations
// ============================================================================

static void* sdl3_window_get_native_handle(CogitoWindow* window);
static SDL_HitTestResult sdl3_csd_hit_test_callback(SDL_Window* sdl_win, const SDL_Point* point, void* data);

// ============================================================================
// Internal Types
// ============================================================================

typedef struct CogitoSDL3Texture {
    SDL_GPUTexture* gpu_texture;
    int width;
    int height;
    int channels;
} CogitoSDL3Texture;

typedef struct CogitoSDL3Window {
    SDL_Window* sdl_window;
    int width, height;
    bool should_close;
    bool borderless;
    uint32_t window_id;
    CogitoCSDState csd_state;
    SDL_GPUSwapchainComposition swapchain_composition;
    SDL_GPUPresentMode present_mode;
} CogitoSDL3Window;

typedef struct CogitoSDL3Font {
    char* path;
    int size;
    TTF_Font* ttf_font;
    int ascent;
    int descent;
    int height;
} CogitoSDL3Font;

// GPU render state
typedef struct {
    SDL_GPUCommandBuffer* cmd_buf;
    SDL_GPURenderPass* render_pass;
    SDL_GPUTexture* swapchain_texture;
    SDL_GPUColorTargetInfo color_target;
    int window_width;
    int window_height;
} CogitoSDL3RenderState;

static CogitoSDL3RenderState g_render_state = {0};

// ============================================================================
// Global State
// ============================================================================

static bool sdl3_initialized = false;
static bool ttf_initialized = false;
static SDL_GPUDevice* global_gpu_device = NULL;
static CogitoWindowRegistry window_registry = {0};
static CogitoDebugFlags debug_flags = {0};

// Vertex buffer for quad rendering (max 1024 quads per frame)
#define MAX_VERTICES 4096
static SDL_GPUBuffer* vertex_buffer = NULL;
static float* vertex_data = NULL;
static int vertex_count = 0;

// Simple shader for colored primitives
// TODO: Implement GPU pipelines when shader system is ready
// static SDL_GPUGraphicsPipeline* color_pipeline = NULL;
// static SDL_GPUGraphicsPipeline* texture_pipeline = NULL;
// static SDL_GPUSampler* texture_sampler = NULL;

// Input state
static int mouse_x = 0, mouse_y = 0;
static bool mouse_buttons[3] = {false};
static bool mouse_buttons_pressed[3] = {false};
static bool mouse_buttons_released[3] = {false};
static float mouse_wheel = 0.0f;
static bool keys_down[512] = {false};
static bool keys_pressed[512] = {false};
static bool keys_released[512] = {false};
static int char_queue[16] = {0};
static int char_queue_head = 0, char_queue_tail = 0;
static double start_time = 0.0;

// ============================================================================
// Color Helpers
// ============================================================================

CogitoColor cogito_color_lerp(CogitoColor a, CogitoColor b, float t) {
    if (t <= 0.0f) return a;
    if (t >= 1.0f) return b;
    return (CogitoColor){
        (uint8_t)(a.r + (b.r - a.r) * t),
        (uint8_t)(a.g + (b.g - a.g) * t),
        (uint8_t)(a.b + (b.b - a.b) * t),
        (uint8_t)(a.a + (b.a - a.a) * t)
    };
}

CogitoColor cogito_color_blend(CogitoColor base, CogitoColor over) {
    float a = over.a / 255.0f;
    float ia = 1.0f - a;
    return (CogitoColor){
        (uint8_t)(base.r * ia + over.r * a),
        (uint8_t)(base.g * ia + over.g * a),
        (uint8_t)(base.b * ia + over.b * a),
        base.a
    };
}

CogitoColor cogito_color_apply_opacity(CogitoColor c, float opacity) {
    if (opacity >= 1.0f) return c;
    if (opacity <= 0.0f) return (CogitoColor){c.r, c.g, c.b, 0};
    c.a = (uint8_t)((float)c.a * opacity);
    return c;
}

float cogito_color_luma(CogitoColor c) {
    float r = c.r / 255.0f;
    float g = c.g / 255.0f;
    float b = c.b / 255.0f;
    return 0.2126f * r + 0.7152f * g + 0.0722f * b;
}

CogitoColor cogito_color_mix(CogitoColor a, CogitoColor b, float t) {
    return cogito_color_lerp(a, b, t);
}

CogitoColor cogito_color_on_color(CogitoColor bg) {
    return cogito_color_luma(bg) > 0.55f
        ? (CogitoColor){0, 0, 0, 255}
        : (CogitoColor){255, 255, 255, 255};
}

// ============================================================================
// Lifecycle
// ============================================================================

// Simple vertex shader (colored primitives) - TODO: implement shader system
// static const char* color_vertex_shader = 
//     "@vertex\n"
//     "fn main(@location(0) position: vec2<f32>, @location(1) color: vec4<f32>) -> @builtin(position) vec4<f32> {\n"
//     "    return vec4<f32>(position, 0.0, 1.0);\n"
//     "}\n";

// Simple fragment shader (colored primitives) - TODO: implement shader system
// static const char* color_fragment_shader = 
//     "@fragment\n"
//     "fn main() -> @location(0) vec4<f32> {\n"
//     "    return vec4<f32>(1.0, 1.0, 1.0, 1.0);\n"
//     "}\n";

// Helper: Create GPU buffer
static SDL_GPUBuffer* create_buffer(SDL_GPUDevice* device, uint32_t size, SDL_GPUBufferUsageFlags usage) {
    SDL_GPUBufferCreateInfo info = {
        .usage = usage,
        .size = size,
        .props = 0
    };
    return SDL_CreateGPUBuffer(device, &info);
}

// Helper: Upload data to GPU buffer
static void upload_buffer_data(SDL_GPUDevice* device, SDL_GPUBuffer* buffer, const void* data, uint32_t size) {
    SDL_GPUTransferBufferCreateInfo transfer_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = size,
        .props = 0
    };
    SDL_GPUTransferBuffer* transfer = SDL_CreateGPUTransferBuffer(device, &transfer_info);
    if (!transfer) return;
    
    void* map = SDL_MapGPUTransferBuffer(device, transfer, false);
    if (map) {
        memcpy(map, data, size);
        SDL_UnmapGPUTransferBuffer(device, transfer);
    }
    
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
    SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(cmd);
    
    SDL_GPUTransferBufferLocation src = { .transfer_buffer = transfer, .offset = 0 };
    SDL_GPUBufferRegion dst = { .buffer = buffer, .offset = 0, .size = size };
    SDL_UploadToGPUBuffer(pass, &src, &dst, false);
    
    SDL_EndGPUCopyPass(pass);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(device, transfer);
}

static bool sdl3_init(void) {
    if (sdl3_initialized) return true;
    
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    
    SDL_GPUShaderFormat formats = SDL_GPU_SHADERFORMAT_SPIRV | 
                                  SDL_GPU_SHADERFORMAT_DXIL | 
                                  SDL_GPU_SHADERFORMAT_MSL;
    
    global_gpu_device = SDL_CreateGPUDevice(formats, true, NULL);
    if (!global_gpu_device) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateGPUDevice failed: %s", SDL_GetError());
        SDL_Quit();
        return false;
    }
    
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "SDL3 GPU driver: %s", 
                SDL_GetGPUDeviceDriver(global_gpu_device));
    
    // Initialize TTF
    if (!TTF_Init()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "TTF_Init failed: %s", SDL_GetError());
        SDL_DestroyGPUDevice(global_gpu_device);
        SDL_Quit();
        return false;
    }
    ttf_initialized = true;
    
    // Create vertex buffer
    vertex_buffer = create_buffer(global_gpu_device, MAX_VERTICES * 6 * sizeof(float) * 2, 
                                   SDL_GPU_BUFFERUSAGE_VERTEX);
    if (!vertex_buffer) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create vertex buffer");
        TTF_Quit();
        SDL_DestroyGPUDevice(global_gpu_device);
        SDL_Quit();
        return false;
    }
    
    vertex_data = (float*)malloc(MAX_VERTICES * 6 * sizeof(float) * 2);
    if (!vertex_data) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to allocate vertex data");
        SDL_ReleaseGPUBuffer(global_gpu_device, vertex_buffer);
        TTF_Quit();
        SDL_DestroyGPUDevice(global_gpu_device);
        SDL_Quit();
        return false;
    }
    
    // Parse debug flags
    cogito_debug_flags_parse(&debug_flags);
    if (debug_flags.debug_native) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Native handle debugging enabled");
    }
    
    // Initialize window registry
    cogito_window_registry_init(&window_registry);
    
    start_time = (double)SDL_GetTicks() / 1000.0;
    sdl3_initialized = true;
    return true;
}

static void sdl3_shutdown(void) {
    if (!sdl3_initialized) return;
    
    if (vertex_data) {
        free(vertex_data);
        vertex_data = NULL;
    }
    
    if (vertex_buffer && global_gpu_device) {
        SDL_ReleaseGPUBuffer(global_gpu_device, vertex_buffer);
        vertex_buffer = NULL;
    }
    
    if (global_gpu_device) {
        SDL_DestroyGPUDevice(global_gpu_device);
        global_gpu_device = NULL;
    }
    
    if (ttf_initialized) {
        TTF_Quit();
        ttf_initialized = false;
    }
    
    SDL_Quit();
    sdl3_initialized = false;
}

// ============================================================================
// Window Management
// ============================================================================

static CogitoWindow* sdl3_window_create(const char* title, int w, int h, bool resizable, bool borderless) {
    if (!sdl3_initialized) return NULL;
    
    CogitoSDL3Window* win = calloc(1, sizeof(CogitoSDL3Window));
    if (!win) return NULL;
    
    SDL_WindowFlags flags = SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (resizable) flags |= SDL_WINDOW_RESIZABLE;
    if (borderless) flags |= SDL_WINDOW_BORDERLESS;
    
    win->sdl_window = SDL_CreateWindow(title, w, h, flags);
    if (!win->sdl_window) {
        free(win);
        return NULL;
    }
    
    win->width = w;
    win->height = h;
    win->borderless = borderless;
    win->window_id = SDL_GetWindowID(win->sdl_window);
    
    if (!SDL_ClaimWindowForGPUDevice(global_gpu_device, win->sdl_window)) {
        SDL_DestroyWindow(win->sdl_window);
        free(win);
        return NULL;
    }
    
    // Initialize CSD for borderless windows with appbar
    cogito_csd_init(&win->csd_state, borderless);
    
    // Register in window registry
    cogito_window_registry_add(&window_registry, (CogitoWindow*)win);
    
    if (debug_flags.debug_native) {
        void* native = sdl3_window_get_native_handle((CogitoWindow*)win);
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Window %u native handle: %p", 
                    win->window_id, native);
    }
    
    return (CogitoWindow*)win;
}

static void sdl3_window_destroy(CogitoWindow* window) {
    CogitoSDL3Window* win = (CogitoSDL3Window*)window;
    if (!win) return;
    
    cogito_window_registry_remove(&window_registry, window);
    
    if (win->sdl_window) {
        SDL_ReleaseWindowFromGPUDevice(global_gpu_device, win->sdl_window);
        SDL_DestroyWindow(win->sdl_window);
    }
    
    free(win);
}

static void sdl3_window_set_size(CogitoWindow* window, int w, int h) {
    CogitoSDL3Window* win = (CogitoSDL3Window*)window;
    if (!win || !win->sdl_window) return;
    SDL_SetWindowSize(win->sdl_window, w, h);
    win->width = w;
    win->height = h;
}

static void sdl3_window_get_size(CogitoWindow* window, int* w, int* h) {
    CogitoSDL3Window* win = (CogitoSDL3Window*)window;
    if (!win) {
        if (w) *w = 0;
        if (h) *h = 0;
        return;
    }
    if (w) *w = win->width;
    if (h) *h = win->height;
}

static void sdl3_window_set_position(CogitoWindow* window, int x, int y) {
    CogitoSDL3Window* win = (CogitoSDL3Window*)window;
    if (!win || !win->sdl_window) return;
    SDL_SetWindowPosition(win->sdl_window, (float)x, (float)y);
}

static void sdl3_window_get_position(CogitoWindow* window, int* x, int* y) {
    CogitoSDL3Window* win = (CogitoSDL3Window*)window;
    if (!win || !win->sdl_window) {
        if (x) *x = 0;
        if (y) *y = 0;
        return;
    }
    int ix, iy;
    SDL_GetWindowPosition(win->sdl_window, &ix, &iy);
    if (x) *x = ix;
    if (y) *y = iy;
}

static void sdl3_window_set_title(CogitoWindow* window, const char* title) {
    CogitoSDL3Window* win = (CogitoSDL3Window*)window;
    if (!win || !win->sdl_window) return;
    SDL_SetWindowTitle(win->sdl_window, title);
}

static void sdl3_window_show(CogitoWindow* window) {
    CogitoSDL3Window* win = (CogitoSDL3Window*)window;
    if (!win || !win->sdl_window) return;
    SDL_ShowWindow(win->sdl_window);
}

static void sdl3_window_hide(CogitoWindow* window) {
    CogitoSDL3Window* win = (CogitoSDL3Window*)window;
    if (!win || !win->sdl_window) return;
    SDL_HideWindow(win->sdl_window);
}

static void* sdl3_window_get_native_handle(CogitoWindow* window) {
    CogitoSDL3Window* win = (CogitoSDL3Window*)window;
    if (!win || !win->sdl_window) return NULL;
    
#if defined(__APPLE__)
    return SDL_GetPointerProperty(SDL_GetWindowProperties(win->sdl_window), 
                                  SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, NULL);
#elif defined(_WIN32)
    return SDL_GetPointerProperty(SDL_GetWindowProperties(win->sdl_window),
                                  SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
#elif defined(__linux__)
    void* wayland = SDL_GetPointerProperty(SDL_GetWindowProperties(win->sdl_window),
                                           SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, NULL);
    if (wayland) return wayland;
    return (void*)(uintptr_t)SDL_GetNumberProperty(SDL_GetWindowProperties(win->sdl_window),
                                                     SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
#else
    return NULL;
#endif
}

static uint32_t sdl3_window_get_id(CogitoWindow* window) {
    CogitoSDL3Window* win = (CogitoSDL3Window*)window;
    if (!win) return 0;
    return win->window_id;
}

// ============================================================================
// Frame Rendering
// ============================================================================

static void sdl3_begin_frame(CogitoWindow* window) {
    CogitoSDL3Window* win = (CogitoSDL3Window*)window;
    if (!win) return;
    
    int w, h;
    SDL_GetWindowSize(win->sdl_window, &w, &h);
    if (w != win->width || h != win->height) {
        win->width = w;
        win->height = h;
    }
    
    // Acquire command buffer and swapchain texture
    g_render_state.cmd_buf = SDL_AcquireGPUCommandBuffer(global_gpu_device);
    if (!g_render_state.cmd_buf) return;
    
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(g_render_state.cmd_buf, win->sdl_window, 
                                                &g_render_state.swapchain_texture, 
                                                NULL, NULL)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to acquire swapchain texture: %s", 
                     SDL_GetError());
        SDL_CancelGPUCommandBuffer(g_render_state.cmd_buf);
        g_render_state.cmd_buf = NULL;
        return;
    }
    
    g_render_state.window_width = w;
    g_render_state.window_height = h;
    vertex_count = 0;
}

static void sdl3_end_frame(CogitoWindow* window) {
    (void)window;
    
    if (!g_render_state.render_pass) return;
    
    // End render pass if active
    SDL_EndGPURenderPass(g_render_state.render_pass);
    g_render_state.render_pass = NULL;
}

static void sdl3_present(CogitoWindow* window) {
    CogitoSDL3Window* win = (CogitoSDL3Window*)window;
    if (!win || !g_render_state.cmd_buf) return;
    
    // Submit command buffer
    SDL_SubmitGPUCommandBuffer(g_render_state.cmd_buf);
    g_render_state.cmd_buf = NULL;
    g_render_state.swapchain_texture = NULL;
}

static void sdl3_clear(CogitoColor color) {
    if (!g_render_state.swapchain_texture) return;
    
    // End any existing render pass
    if (g_render_state.render_pass) {
        SDL_EndGPURenderPass(g_render_state.render_pass);
        g_render_state.render_pass = NULL;
    }
    
    // Begin new render pass with clear
    SDL_GPUColorTargetInfo color_target = {
        .texture = g_render_state.swapchain_texture,
        .mip_level = 0,
        .layer_or_depth_plane = 0,
        .clear_color = { color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f },
        .load_op = SDL_GPU_LOADOP_CLEAR,
        .store_op = SDL_GPU_STOREOP_STORE,
        .cycle = false
    };
    
    SDL_GPUColorTargetInfo targets[1] = { color_target };
    g_render_state.render_pass = SDL_BeginGPURenderPass(g_render_state.cmd_buf, targets, 1, NULL);
}

// ============================================================================
// Event Loop
// ============================================================================

static void process_events(void) {
    // Reset per-frame state
    for (int i = 0; i < 3; i++) {
        mouse_buttons_pressed[i] = false;
        mouse_buttons_released[i] = false;
    }
    for (int i = 0; i < 512; i++) {
        keys_pressed[i] = false;
        keys_released[i] = false;
    }
    mouse_wheel = 0.0f;
    
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                // Mark all windows for close
                for (int i = 0; i < window_registry.count; i++) {
                    CogitoSDL3Window* win = (CogitoSDL3Window*)window_registry.windows[i];
                    if (win) win->should_close = true;
                }
                break;
                
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
                CogitoWindow* win = cogito_window_registry_get(&window_registry, event.window.windowID);
                if (win) {
                    ((CogitoSDL3Window*)win)->should_close = true;
                }
                break;
            }
            
            case SDL_EVENT_WINDOW_FOCUS_GAINED: {
                CogitoWindow* win = cogito_window_registry_get(&window_registry, event.window.windowID);
                if (win) {
                    cogito_window_registry_set_focused(&window_registry, win);
                }
                break;
            }
            
            case SDL_EVENT_MOUSE_MOTION:
                mouse_x = (int)event.motion.x;
                mouse_y = (int)event.motion.y;
                break;
                
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (event.button.button >= 1 && event.button.button <= 3) {
                    int btn = event.button.button - 1;
                    mouse_buttons[btn] = true;
                    mouse_buttons_pressed[btn] = true;
                }
                break;
                
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (event.button.button >= 1 && event.button.button <= 3) {
                    int btn = event.button.button - 1;
                    mouse_buttons[btn] = false;
                    mouse_buttons_released[btn] = true;
                }
                break;
                
            case SDL_EVENT_MOUSE_WHEEL:
                mouse_wheel = event.wheel.y;
                break;
                
            case SDL_EVENT_KEY_DOWN:
                if (event.key.scancode < 512) {
                    keys_down[event.key.scancode] = true;
                    keys_pressed[event.key.scancode] = true;
                }
                
                // Inspector toggle: Ctrl+Shift+I
                if (debug_flags.inspector || debug_flags.debug_csd) {
                    bool ctrl = keys_down[SDL_SCANCODE_LCTRL] || keys_down[SDL_SCANCODE_RCTRL];
                    bool shift = keys_down[SDL_SCANCODE_LSHIFT] || keys_down[SDL_SCANCODE_RSHIFT];
                    if (ctrl && shift && event.key.scancode == SDL_SCANCODE_I) {
                        // Toggle inspector for focused window
                        CogitoWindow* win = cogito_window_registry_get_focused(&window_registry);
                        if (win) {
                            CogitoSDL3Window* sdl_win = (CogitoSDL3Window*)win;
                            cogito_csd_set_debug_overlay(&sdl_win->csd_state, 
                                !sdl_win->csd_state.debug_overlay);
                            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, 
                                "CSD debug overlay toggled for window %u", sdl_win->window_id);
                        }
                    }
                }
                break;
                
            case SDL_EVENT_KEY_UP:
                if (event.key.scancode < 512) {
                    keys_down[event.key.scancode] = false;
                    keys_released[event.key.scancode] = true;
                }
                break;
                
            case SDL_EVENT_TEXT_INPUT: {
                const char* text = event.text.text;
                if (text) {
                    while (*text) {
                        unsigned char c = (unsigned char)*text;
                        int cp = 0, len = 0;
                        
                        if ((c & 0x80) == 0) { cp = c; len = 1; }
                        else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; len = 2; }
                        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; len = 3; }
                        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; len = 4; }
                        
                        for (int i = 1; i < len && text[i]; i++) {
                            cp = (cp << 6) | (text[i] & 0x3F);
                        }
                        
                        int next = (char_queue_tail + 1) % 16;
                        if (next != char_queue_head) {
                            char_queue[char_queue_tail] = cp;
                            char_queue_tail = next;
                        }
                        text += len;
                    }
                }
                break;
            }
        }
    }
}

static void sdl3_poll_events(void) {
    process_events();
}

static bool sdl3_window_should_close(CogitoWindow* window) {
    CogitoSDL3Window* win = (CogitoSDL3Window*)window;
    if (!win) return true;
    
    // Check if this is the last window
    if (win->should_close && window_registry.count <= 1) {
        return true;
    }
    
    return win->should_close;
}

// ============================================================================
// Input
// ============================================================================

static void sdl3_get_mouse_position(int* x, int* y) {
    if (x) *x = mouse_x;
    if (y) *y = mouse_y;
}

static void sdl3_get_mouse_position_in_window(CogitoWindow* window, int* x, int* y) {
    CogitoSDL3Window* win = (CogitoSDL3Window*)window;
    if (!win) {
        if (x) *x = 0;
        if (y) *y = 0;
        return;
    }
    int wx, wy;
    SDL_GetWindowPosition(win->sdl_window, &wx, &wy);
    if (x) *x = mouse_x - wx;
    if (y) *y = mouse_y - wy;
}

static bool sdl3_is_mouse_button_down(int button) {
    if (button < 0 || button >= 3) return false;
    return mouse_buttons[button];
}

static bool sdl3_is_mouse_button_pressed(int button) {
    if (button < 0 || button >= 3) return false;
    return mouse_buttons_pressed[button];
}

static bool sdl3_is_mouse_button_released(int button) {
    if (button < 0 || button >= 3) return false;
    return mouse_buttons_released[button];
}

static float sdl3_get_mouse_wheel_move(void) {
    return mouse_wheel;
}

static bool sdl3_is_key_down(int key) {
    if (key < 0 || key >= 512) return false;
    return keys_down[key];
}

static bool sdl3_is_key_pressed(int key) {
    if (key < 0 || key >= 512) return false;
    return keys_pressed[key];
}

static bool sdl3_is_key_released(int key) {
    if (key < 0 || key >= 512) return false;
    return keys_released[key];
}

static int sdl3_get_char_pressed(void) {
    if (char_queue_head == char_queue_tail) return 0;
    int cp = char_queue[char_queue_head];
    char_queue_head = (char_queue_head + 1) % 16;
    return cp;
}

// ============================================================================
// Time
// ============================================================================

static double sdl3_get_time(void) {
    return ((double)SDL_GetTicks() / 1000.0) - start_time;
}

static void sdl3_sleep(uint32_t ms) {
    SDL_Delay(ms);
}

// ============================================================================
// Drawing (GPU Implementation)
// ============================================================================

// Helper: Add quad vertices to buffer
static void add_quad(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4,
                     float r, float g, float b, float a) {
    if (vertex_count + 6 > MAX_VERTICES) return;
    
    float* v = &vertex_data[vertex_count * 8]; // 4 floats per vertex (x, y, r, g, b, a, u, v)
    
    // Triangle 1
    v[0] = x1; v[1] = y1; v[2] = r; v[3] = g; v[4] = b; v[5] = a; v[6] = 0; v[7] = 0;
    v[8] = x2; v[9] = y2; v[10] = r; v[11] = g; v[12] = b; v[13] = a; v[14] = 1; v[15] = 0;
    v[16] = x3; v[17] = y3; v[18] = r; v[19] = g; v[20] = b; v[21] = a; v[22] = 1; v[23] = 1;
    
    // Triangle 2
    v[24] = x1; v[25] = y1; v[26] = r; v[27] = g; v[28] = b; v[29] = a; v[30] = 0; v[31] = 0;
    v[32] = x3; v[33] = y3; v[34] = r; v[35] = g; v[36] = b; v[37] = a; v[38] = 1; v[39] = 1;
    v[40] = x4; v[41] = y4; v[42] = r; v[43] = g; v[44] = b; v[45] = a; v[46] = 0; v[47] = 1;
    
    vertex_count += 6;
}

// Helper: Flush vertex buffer to GPU - TODO: implement when shader pipeline ready
// static void flush_vertices(void) {
//     if (vertex_count == 0 || !g_render_state.cmd_buf) return;
//     
//     // Upload vertex data
//     upload_buffer_data(global_gpu_device, vertex_buffer, vertex_data, 
//                        vertex_count * 8 * sizeof(float));
//     
//     // TODO: Set up pipeline and draw
//     // For now, just clear the buffer
//     vertex_count = 0;
// }

// Helper: Ensure render pass is active - TODO: implement when shader pipeline ready
// static void ensure_render_pass(void) {
//     if (g_render_state.render_pass) return;
//     if (!g_render_state.swapchain_texture) return;
//     
//     SDL_GPUColorTargetInfo color_target = {
//         .texture = g_render_state.swapchain_texture,
//         .mip_level = 0,
//         .layer_or_depth_plane = 0,
//         .clear_color = { 0, 0, 0, 0 },
//         .load_op = SDL_GPU_LOADOP_LOAD,
//         .store_op = SDL_GPU_STOREOP_STORE,
//         .cycle = false
//     };
//     
//     SDL_GPUColorTargetInfo targets[1] = { color_target };
//     g_render_state.render_pass = SDL_BeginGPURenderPass(g_render_state.cmd_buf, targets, 1, NULL);
// }

// Helper: Convert pixel coordinates to normalized device coordinates
static float px_to_ndc_x(int x, int width) {
    return (2.0f * x / width) - 1.0f;
}

static float px_to_ndc_y(int y, int height) {
    return 1.0f - (2.0f * y / height);
}

static void sdl3_draw_rect(int x, int y, int w, int h, CogitoColor color) {
    if (!g_render_state.swapchain_texture) return;
    
    float r = color.r / 255.0f;
    float g = color.g / 255.0f;
    float b = color.b / 255.0f;
    float a = color.a / 255.0f;
    
    float x1 = px_to_ndc_x(x, g_render_state.window_width);
    float y1 = px_to_ndc_y(y, g_render_state.window_height);
    float x2 = px_to_ndc_x(x + w, g_render_state.window_width);
    float y2 = px_to_ndc_y(y + h, g_render_state.window_height);
    
    add_quad(x1, y1, x2, y1, x2, y2, x1, y2, r, g, b, a);
}

static void sdl3_draw_rect_rounded(int x, int y, int w, int h, CogitoColor color, float roundness) {
    // For now, fall back to regular rect (TODO: implement proper rounded corners)
    (void)roundness;
    sdl3_draw_rect(x, y, w, h, color);
}

static void sdl3_draw_line(int x1, int y1, int x2, int y2, CogitoColor color, int thickness) {
    (void)thickness;
    if (!g_render_state.swapchain_texture) return;
    
    float r = color.r / 255.0f;
    float g = color.g / 255.0f;
    float b = color.b / 255.0f;
    float a = color.a / 255.0f;
    
    // Simple line as thin quad
    float dx = x2 - x1;
    float dy = y2 - y1;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.001f) return;
    
    float nx = -dy / len * 0.5f;
    float ny = dx / len * 0.5f;
    
    float x1_ndc = px_to_ndc_x(x1 + nx, g_render_state.window_width);
    float y1_ndc = px_to_ndc_y(y1 + ny, g_render_state.window_height);
    float x2_ndc = px_to_ndc_x(x2 + nx, g_render_state.window_width);
    float y2_ndc = px_to_ndc_y(y2 + ny, g_render_state.window_height);
    float x3_ndc = px_to_ndc_x(x2 - nx, g_render_state.window_width);
    float y3_ndc = px_to_ndc_y(y2 - ny, g_render_state.window_height);
    float x4_ndc = px_to_ndc_x(x1 - nx, g_render_state.window_width);
    float y4_ndc = px_to_ndc_y(y1 - ny, g_render_state.window_height);
    
    add_quad(x1_ndc, y1_ndc, x2_ndc, y2_ndc, x3_ndc, y3_ndc, x4_ndc, y4_ndc, r, g, b, a);
}

static void sdl3_draw_rect_lines(int x, int y, int w, int h, CogitoColor color, int thickness) {
    (void)thickness;
    // Draw 4 lines for the rectangle border
    sdl3_draw_line(x, y, x + w, y, color, 1);
    sdl3_draw_line(x + w, y, x + w, y + h, color, 1);
    sdl3_draw_line(x + w, y + h, x, y + h, color, 1);
    sdl3_draw_line(x, y + h, x, y, color, 1);
}

static void sdl3_draw_rect_rounded_lines(int x, int y, int w, int h, CogitoColor color, float roundness, int thickness) {
    (void)roundness; (void)thickness;
    // For now, fall back to regular rect lines
    sdl3_draw_rect_lines(x, y, w, h, color, 1);
}

static void sdl3_draw_circle(int x, int y, float radius, CogitoColor color) {
    if (!g_render_state.swapchain_texture) return;
    
    float r = color.r / 255.0f;
    float g = color.g / 255.0f;
    float b = color.b / 255.0f;
    float a = color.a / 255.0f;
    
    // Approximate circle with 16 triangles
    float cx = px_to_ndc_x(x, g_render_state.window_width);
    float cy = px_to_ndc_y(y, g_render_state.window_height);
    float rx = 2.0f * radius / g_render_state.window_width;
    float ry = 2.0f * radius / g_render_state.window_height;
    
    for (int i = 0; i < 16; i++) {
        float a1 = (float)i * 2.0f * M_PI / 16.0f;
        float a2 = (float)(i + 1) * 2.0f * M_PI / 16.0f;
        
        float x1 = cx + cosf(a1) * rx;
        float y1 = cy + sinf(a1) * ry;
        float x2 = cx + cosf(a2) * rx;
        float y2 = cy + sinf(a2) * ry;
        
        add_quad(cx, cy, x1, y1, x2, y2, cx, cy, r, g, b, a);
    }
}

static void sdl3_draw_circle_lines(int x, int y, float radius, CogitoColor color, int thickness) {
    (void)thickness;
    if (!g_render_state.swapchain_texture) return;
    
    float r = color.r / 255.0f;
    float g = color.g / 255.0f;
    float b = color.b / 255.0f;
    float a = color.a / 255.0f;
    
    // Draw circle outline with line segments
    float cx = px_to_ndc_x(x, g_render_state.window_width);
    float cy = px_to_ndc_y(y, g_render_state.window_height);
    float rx = 2.0f * radius / g_render_state.window_width;
    float ry = 2.0f * radius / g_render_state.window_height;
    
    float prev_x = cx + cosf(0) * rx;
    float prev_y = cy + sinf(0) * ry;
    
    for (int i = 1; i <= 32; i++) {
        float angle = (float)i * 2.0f * M_PI / 32.0f;
        float x2 = cx + cosf(angle) * rx;
        float y2 = cy + sinf(angle) * ry;
        
        // Draw line from prev to current
        float dx = x2 - prev_x;
        float dy = y2 - prev_y;
        float len = sqrtf(dx * dx + dy * dy);
        if (len > 0.001f) {
            float nx = -dy / len * 0.005f;
            float ny = dx / len * 0.005f;
            add_quad(prev_x + nx, prev_y + ny, x2 + nx, y2 + ny, 
                     x2 - nx, y2 - ny, prev_x - nx, prev_y - ny, r, g, b, a);
        }
        
        prev_x = x2;
        prev_y = y2;
    }
}

// ============================================================================
// Text (SDL_ttf)
// ============================================================================

static CogitoFont* sdl3_font_load(const char* path, int size) {
    if (!path || !path[0] || size <= 0) return NULL;
    
    TTF_Font* ttf = TTF_OpenFont(path, size);
    if (!ttf) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "TTF_OpenFont failed: %s", SDL_GetError());
        return NULL;
    }
    
    CogitoSDL3Font* font = calloc(1, sizeof(CogitoSDL3Font));
    if (!font) {
        TTF_CloseFont(ttf);
        return NULL;
    }
    
    font->path = strdup(path);
    font->size = size;
    font->ttf_font = ttf;
    font->ascent = TTF_GetFontAscent(ttf);
    font->descent = TTF_GetFontDescent(ttf);
    font->height = TTF_GetFontHeight(ttf);
    
    return (CogitoFont*)font;
}

static void sdl3_font_unload(CogitoFont* font) {
    CogitoSDL3Font* f = (CogitoSDL3Font*)font;
    if (!f) return;
    if (f->ttf_font) TTF_CloseFont(f->ttf_font);
    free(f->path);
    free(f);
}

static void sdl3_font_get_metrics(CogitoFont* font, int* ascent, int* descent, int* height) {
    CogitoSDL3Font* f = (CogitoSDL3Font*)font;
    if (!f) {
        if (ascent) *ascent = 0;
        if (descent) *descent = 0;
        if (height) *height = 0;
        return;
    }
    if (ascent) *ascent = f->ascent;
    if (descent) *descent = f->descent;
    if (height) *height = f->height;
}

static int sdl3_text_measure_width(CogitoFont* font, const char* text, int size) {
    CogitoSDL3Font* f = (CogitoSDL3Font*)font;
    if (!f || !f->ttf_font || !text || !text[0]) return 0;
    
    int w, h;
    if (!TTF_GetStringSize(f->ttf_font, text, 0, &w, &h)) return 0;
    (void)h; (void)size;
    return w;
}

static int sdl3_text_measure_height(CogitoFont* font, int size) {
    CogitoSDL3Font* f = (CogitoSDL3Font*)font;
    if (!f || !f->ttf_font) return size > 0 ? size + 2 : 18;
    (void)size;
    return TTF_GetFontHeight(f->ttf_font);
}

static void sdl3_draw_text(CogitoFont* font, const char* text, int x, int y, int size, CogitoColor color) {
    CogitoSDL3Font* f = (CogitoSDL3Font*)font;
    if (!f || !f->ttf_font || !text || !text[0]) return;
    (void)size;
    
    // Render text to surface using SDL_ttf
    SDL_Color sdl_color = { color.r, color.g, color.b, color.a };
    SDL_Surface* surface = TTF_RenderText_Blended(f->ttf_font, text, 0, sdl_color);
    if (!surface) return;
    
    // Create texture from surface
    int tex_w = surface->w;
    int tex_h = surface->h;
    
    // Upload pixel data to GPU texture
    SDL_GPUTextureCreateInfo tex_info = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .width = (Uint32)tex_w,
        .height = (Uint32)tex_h,
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .props = 0
    };
    
    SDL_GPUTexture* gpu_tex = SDL_CreateGPUTexture(global_gpu_device, &tex_info);
    if (!gpu_tex) {
        SDL_DestroySurface(surface);
        return;
    }
    
    // Upload surface data to texture
    SDL_GPUTransferBufferCreateInfo transfer_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = (Uint32)(tex_w * tex_h * 4),
        .props = 0
    };
    SDL_GPUTransferBuffer* transfer = SDL_CreateGPUTransferBuffer(global_gpu_device, &transfer_info);
    if (!transfer) {
        SDL_ReleaseGPUTexture(global_gpu_device, gpu_tex);
        SDL_DestroySurface(surface);
        return;
    }
    
    void* map = SDL_MapGPUTransferBuffer(global_gpu_device, transfer, false);
    if (map) {
        memcpy(map, surface->pixels, tex_w * tex_h * 4);
        SDL_UnmapGPUTransferBuffer(global_gpu_device, transfer);
    }
    
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(global_gpu_device);
    SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(cmd);
    
    SDL_GPUTextureTransferInfo src = {
        .transfer_buffer = transfer,
        .offset = 0,
        .pixels_per_row = (Uint32)tex_w,
        .rows_per_layer = (Uint32)tex_h
    };
    SDL_GPUTextureRegion dst = {
        .texture = gpu_tex,
        .mip_level = 0,
        .layer = 0,
        .x = 0,
        .y = 0,
        .z = 0,
        .w = (Uint32)tex_w,
        .h = (Uint32)tex_h,
        .d = 1
    };
    SDL_UploadToGPUTexture(pass, &src, &dst, false);
    
    SDL_EndGPUCopyPass(pass);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(global_gpu_device, transfer);
    
    // Draw textured quad (simplified - just draw a colored rect for now)
    // TODO: Implement proper texture sampling in shader
    sdl3_draw_rect(x, y, tex_w, tex_h, color);
    
    // Cleanup
    SDL_ReleaseGPUTexture(global_gpu_device, gpu_tex);
    SDL_DestroySurface(surface);
}

// ============================================================================
// Textures
// ============================================================================

static CogitoTexture* sdl3_texture_create(int w, int h, const uint8_t* data, int channels) {
    if (!global_gpu_device || w <= 0 || h <= 0 || !data) return NULL;
    
    CogitoSDL3Texture* tex = calloc(1, sizeof(CogitoSDL3Texture));
    if (!tex) return NULL;
    
    tex->width = w;
    tex->height = h;
    tex->channels = channels;
    
    // Determine format based on channels
    SDL_GPUTextureFormat format;
    switch (channels) {
        case 1: format = SDL_GPU_TEXTUREFORMAT_R8_UNORM; break;
        case 3: format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM; break; // Pad to RGBA
        case 4: format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM; break;
        default: format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM; break;
    }
    
    SDL_GPUTextureCreateInfo tex_info = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = format,
        .width = (Uint32)w,
        .height = (Uint32)h,
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .props = 0
    };
    
    tex->gpu_texture = SDL_CreateGPUTexture(global_gpu_device, &tex_info);
    if (!tex->gpu_texture) {
        free(tex);
        return NULL;
    }
    
    // Upload data
    int src_channels = channels;
    int dst_channels = (channels == 3) ? 4 : channels;
    int src_pitch = w * src_channels;
    int dst_pitch = w * dst_channels;
    
    uint8_t* upload_data = (uint8_t*)malloc(w * h * dst_channels);
    if (!upload_data) {
        SDL_ReleaseGPUTexture(global_gpu_device, tex->gpu_texture);
        free(tex);
        return NULL;
    }
    
    // Convert to RGBA if needed
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int src_idx = y * src_pitch + x * src_channels;
            int dst_idx = y * dst_pitch + x * dst_channels;
            
            if (channels == 1) {
                upload_data[dst_idx] = data[src_idx];
                upload_data[dst_idx + 1] = data[src_idx];
                upload_data[dst_idx + 2] = data[src_idx];
                upload_data[dst_idx + 3] = 255;
            } else if (channels == 3) {
                upload_data[dst_idx] = data[src_idx];
                upload_data[dst_idx + 1] = data[src_idx + 1];
                upload_data[dst_idx + 2] = data[src_idx + 2];
                upload_data[dst_idx + 3] = 255;
            } else {
                upload_data[dst_idx] = data[src_idx];
                upload_data[dst_idx + 1] = data[src_idx + 1];
                upload_data[dst_idx + 2] = data[src_idx + 2];
                upload_data[dst_idx + 3] = data[src_idx + 3];
            }
        }
    }
    
    // Upload to GPU
    SDL_GPUTransferBufferCreateInfo transfer_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = (Uint32)(w * h * dst_channels),
        .props = 0
    };
    SDL_GPUTransferBuffer* transfer = SDL_CreateGPUTransferBuffer(global_gpu_device, &transfer_info);
    if (!transfer) {
        free(upload_data);
        SDL_ReleaseGPUTexture(global_gpu_device, tex->gpu_texture);
        free(tex);
        return NULL;
    }
    
    void* map = SDL_MapGPUTransferBuffer(global_gpu_device, transfer, false);
    if (map) {
        memcpy(map, upload_data, w * h * dst_channels);
        SDL_UnmapGPUTransferBuffer(global_gpu_device, transfer);
    }
    free(upload_data);
    
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(global_gpu_device);
    SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(cmd);
    
    SDL_GPUTextureTransferInfo src_info = {
        .transfer_buffer = transfer,
        .offset = 0,
        .pixels_per_row = (Uint32)w,
        .rows_per_layer = (Uint32)h
    };
    SDL_GPUTextureRegion dst_region = {
        .texture = tex->gpu_texture,
        .mip_level = 0,
        .layer = 0,
        .x = 0,
        .y = 0,
        .z = 0,
        .w = (Uint32)w,
        .h = (Uint32)h,
        .d = 1
    };
    SDL_UploadToGPUTexture(pass, &src_info, &dst_region, false);
    
    SDL_EndGPUCopyPass(pass);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(global_gpu_device, transfer);
    
    return (CogitoTexture*)tex;
}

static void sdl3_texture_destroy(CogitoTexture* tex) {
    CogitoSDL3Texture* t = (CogitoSDL3Texture*)tex;
    if (!t) return;
    if (t->gpu_texture && global_gpu_device) {
        SDL_ReleaseGPUTexture(global_gpu_device, t->gpu_texture);
    }
    free(t);
}

static void sdl3_texture_get_size(CogitoTexture* tex, int* w, int* h) {
    CogitoSDL3Texture* t = (CogitoSDL3Texture*)tex;
    if (!t) {
        if (w) *w = 0;
        if (h) *h = 0;
        return;
    }
    if (w) *w = t->width;
    if (h) *h = t->height;
}

static void sdl3_draw_texture(CogitoTexture* tex, CogitoRect src, CogitoRect dst, CogitoColor tint) {
    (void)tex; (void)src; (void)dst; (void)tint;
    // TODO: Implement texture drawing with proper shader
}

static void sdl3_draw_texture_pro(CogitoTexture* tex, CogitoRect src, CogitoRect dst, CogitoVec2 origin, float rotation, CogitoColor tint) {
    (void)tex; (void)src; (void)dst; (void)origin; (void)rotation; (void)tint;
    // TODO: Implement rotated texture drawing
}

// ============================================================================
// Scissor/Blend
// ============================================================================

static void sdl3_begin_scissor(int x, int y, int w, int h) {
    (void)x; (void)y; (void)w; (void)h;
}

static void sdl3_end_scissor(void) {
}

static void sdl3_set_blend_mode(int mode) {
    (void)mode;
}

// ============================================================================
// CSD
// ============================================================================

static SDL_HitTestResult sdl3_csd_hit_test_callback(SDL_Window* sdl_win, const SDL_Point* point, void* data) {
    (void)sdl_win;
    CogitoSDL3Window* win = (CogitoSDL3Window*)data;
    if (!win || !point) return SDL_HITTEST_NORMAL;
    
    CogitoHitTestResult result = cogito_csd_hit_test(&win->csd_state, point->x, point->y, 
                                                      win->width, win->height);
    return cogito_csd_to_sdl_hit_test(result);
}

static void sdl3_window_set_hit_test_callback(CogitoWindow* window, 
                                              int (*callback)(CogitoWindow* win, int x, int y, void* user),
                                              void* user) {
    (void)callback; (void)user;
    CogitoSDL3Window* win = (CogitoSDL3Window*)window;
    if (!win || !win->sdl_window) return;
    
    // Use our internal CSD hit test
    SDL_SetWindowHitTest(win->sdl_window, sdl3_csd_hit_test_callback, win);
}

static void sdl3_window_set_borderless(CogitoWindow* window, bool borderless) {
    CogitoSDL3Window* win = (CogitoSDL3Window*)window;
    if (!win || !win->sdl_window) return;
    SDL_SetWindowBordered(win->sdl_window, !borderless);
    win->borderless = borderless;
    win->csd_state.enabled = borderless;
}

static bool sdl3_window_is_borderless(CogitoWindow* window) {
    CogitoSDL3Window* win = (CogitoSDL3Window*)window;
    if (!win) return false;
    return win->borderless;
}

// ============================================================================
// Debug
// ============================================================================

static void sdl3_set_debug_overlay(CogitoWindow* window, bool enable) {
    CogitoSDL3Window* win = (CogitoSDL3Window*)window;
    if (!win) return;
    win->csd_state.debug_overlay = enable;
}

// ============================================================================
// Window Registry Implementation
// ============================================================================

void cogito_window_registry_init(CogitoWindowRegistry* registry) {
    if (!registry) return;
    memset(registry, 0, sizeof(CogitoWindowRegistry));
    registry->count = 0;
    registry->focused_index = -1;
}

bool cogito_window_registry_add(CogitoWindowRegistry* registry, CogitoWindow* window) {
    if (!registry || !window) return false;
    if (registry->count >= COGITO_MAX_WINDOWS) return false;
    
    CogitoSDL3Window* win = (CogitoSDL3Window*)window;
    registry->windows[registry->count] = window;
    registry->window_ids[registry->count] = win->window_id;
    registry->count++;
    return true;
}

void cogito_window_registry_remove(CogitoWindowRegistry* registry, CogitoWindow* window) {
    if (!registry || !window) return;
    
    for (int i = 0; i < registry->count; i++) {
        if (registry->windows[i] == window) {
            // Shift remaining windows down
            for (int j = i; j < registry->count - 1; j++) {
                registry->windows[j] = registry->windows[j + 1];
                registry->window_ids[j] = registry->window_ids[j + 1];
            }
            registry->count--;
            if (registry->focused_index >= registry->count) {
                registry->focused_index = registry->count > 0 ? 0 : -1;
            }
            return;
        }
    }
}

CogitoWindow* cogito_window_registry_get(CogitoWindowRegistry* registry, uint32_t window_id) {
    if (!registry) return NULL;
    
    for (int i = 0; i < registry->count; i++) {
        if (registry->window_ids[i] == window_id) {
            return registry->windows[i];
        }
    }
    return NULL;
}

void cogito_window_registry_set_focused(CogitoWindowRegistry* registry, CogitoWindow* window) {
    if (!registry) return;
    
    for (int i = 0; i < registry->count; i++) {
        if (registry->windows[i] == window) {
            registry->focused_index = i;
            return;
        }
    }
}

CogitoWindow* cogito_window_registry_get_focused(CogitoWindowRegistry* registry) {
    if (!registry || registry->focused_index < 0 || registry->focused_index >= registry->count) {
        return NULL;
    }
    return registry->windows[registry->focused_index];
}

// ============================================================================
// Debug Flags Implementation
// ============================================================================

void cogito_debug_flags_parse(CogitoDebugFlags* flags) {
    if (!flags) return;
    
    memset(flags, 0, sizeof(CogitoDebugFlags));
    
    const char* env_csd = getenv("COGITO_DEBUG_CSD");
    if (env_csd && env_csd[0] && env_csd[0] != '0') {
        flags->debug_csd = true;
    }
    
    const char* env_style = getenv("COGITO_DEBUG_STYLE");
    if (env_style && env_style[0] && env_style[0] != '0') {
        flags->debug_style = true;
    }
    
    const char* env_native = getenv("COGITO_DEBUG_NATIVE");
    if (env_native && env_native[0] && env_native[0] != '0') {
        flags->debug_native = true;
    }
    
    const char* env_inspector = getenv("COGITO_INSPECTOR");
    if (env_inspector && env_inspector[0] && env_inspector[0] != '0') {
        flags->inspector = true;
    }
}

// ============================================================================
// Backend Instance
// ============================================================================

CogitoBackend* cogito_backend = NULL;

static CogitoBackend sdl3_backend = {
    .init = sdl3_init,
    .shutdown = sdl3_shutdown,
    .window_create = sdl3_window_create,
    .window_destroy = sdl3_window_destroy,
    .window_set_size = sdl3_window_set_size,
    .window_get_size = sdl3_window_get_size,
    .window_set_position = sdl3_window_set_position,
    .window_get_position = sdl3_window_get_position,
    .window_set_title = sdl3_window_set_title,
    .window_show = sdl3_window_show,
    .window_hide = sdl3_window_hide,
    .window_get_native_handle = sdl3_window_get_native_handle,
    .window_get_id = sdl3_window_get_id,
    .begin_frame = sdl3_begin_frame,
    .end_frame = sdl3_end_frame,
    .present = sdl3_present,
    .clear = sdl3_clear,
    .poll_events = sdl3_poll_events,
    .window_should_close = sdl3_window_should_close,
    .get_mouse_position = sdl3_get_mouse_position,
    .get_mouse_position_in_window = sdl3_get_mouse_position_in_window,
    .is_mouse_button_down = sdl3_is_mouse_button_down,
    .is_mouse_button_pressed = sdl3_is_mouse_button_pressed,
    .is_mouse_button_released = sdl3_is_mouse_button_released,
    .get_mouse_wheel_move = sdl3_get_mouse_wheel_move,
    .is_key_down = sdl3_is_key_down,
    .is_key_pressed = sdl3_is_key_pressed,
    .is_key_released = sdl3_is_key_released,
    .get_char_pressed = sdl3_get_char_pressed,
    .get_time = sdl3_get_time,
    .sleep = sdl3_sleep,
    .draw_rect = sdl3_draw_rect,
    .draw_rect_rounded = sdl3_draw_rect_rounded,
    .draw_rect_lines = sdl3_draw_rect_lines,
    .draw_rect_rounded_lines = sdl3_draw_rect_rounded_lines,
    .draw_line = sdl3_draw_line,
    .draw_circle = sdl3_draw_circle,
    .draw_circle_lines = sdl3_draw_circle_lines,
    .font_load = sdl3_font_load,
    .font_unload = sdl3_font_unload,
    .font_get_metrics = sdl3_font_get_metrics,
    .text_measure_width = sdl3_text_measure_width,
    .text_measure_height = sdl3_text_measure_height,
    .draw_text = sdl3_draw_text,
    .texture_create = sdl3_texture_create,
    .texture_destroy = sdl3_texture_destroy,
    .texture_get_size = sdl3_texture_get_size,
    .draw_texture = sdl3_draw_texture,
    .draw_texture_pro = sdl3_draw_texture_pro,
    .begin_scissor = sdl3_begin_scissor,
    .end_scissor = sdl3_end_scissor,
    .set_blend_mode = sdl3_set_blend_mode,
    .window_set_hit_test_callback = sdl3_window_set_hit_test_callback,
    .window_set_borderless = sdl3_window_set_borderless,
    .window_is_borderless = sdl3_window_is_borderless,
    .set_debug_overlay = sdl3_set_debug_overlay,
};

bool cogito_backend_sdl3_init(void) {
    cogito_backend = &sdl3_backend;
    return sdl3_init();
}

CogitoBackend* cogito_backend_sdl3_get(void) {
    return &sdl3_backend;
}
