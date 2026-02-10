// ---- Cogito GUI (SDL3) ----

#include "backend.h"
#if defined(__APPLE__)
#include <objc/objc.h>
#include <objc/message.h>
#include <objc/runtime.h>
#endif

static bool sum_system_is_dark(void);
static bool sum_system_accent(CogitoColor* out);

static bool cogito_system_is_dark(void) {
  return sum_system_is_dark();
}

static bool cogito_system_accent(CogitoColor* out) {
  if (sum_system_accent(out)) return true;
  if (out) {
    *out = cogito_color(114, 222, 194, 255);
  }
  return false;
}

static void cogito_apply_theme(bool dark, CogitoColor accent);

// Animation Duration Constants (milliseconds)
#define COGITO_ANIM_DURATION_SHORT1 50   // Very fast (fab close, etc.)
#define COGITO_ANIM_DURATION_SHORT2 100  // Fast (switch toggle)
#define COGITO_ANIM_DURATION_SHORT3 150  // Quick (checkbox)
#define COGITO_ANIM_DURATION_SHORT4 200  // Standard (button state)
#define COGITO_ANIM_DURATION_MEDIUM1 250 // Slow (dropdown open)
#define COGITO_ANIM_DURATION_MEDIUM2 300 // Slower (dialog)
#define COGITO_ANIM_DURATION_MEDIUM3 350 // Medium (page transition)
#define COGITO_ANIM_DURATION_MEDIUM4 400
#define COGITO_ANIM_DURATION_LONG1 450
#define COGITO_ANIM_DURATION_LONG2 500
#define COGITO_ANIM_DURATION_LONG3 550
#define COGITO_ANIM_DURATION_LONG4 600
#define COGITO_ACTIVE_LATCH_MS 90      // Keep pressed visual briefly to avoid click flash

// Animation Easing Functions
#define COGITO_EASING_LINEAR 0
#define COGITO_EASING_STANDARD 1       // Standard easing (smooth start and end)
#define COGITO_EASING_IN 2             // Accelerate
#define COGITO_EASING_OUT 3            // Decelerate
#define COGITO_EASING_IN_OUT 4         // Accelerate then decelerate
#define COGITO_EASING_EMPHASIZED 5     // Spring with overshoot (in-back)
#define COGITO_EASING_EMPHASIZED_OUT 6 // Spring decelerate (out-back)

// Color functions now use backend.h implementations
#define cogito_luma cogito_color_luma
#define cogito_mix cogito_color_mix
#define cogito_on_color cogito_color_on_color

typedef struct CogitoApp {
  ErgoObj base;
  void* native;
  ErgoStr* app_id;
  bool accent_override;
  CogitoColor accent_color;
  bool accent_set;
  bool system_dark;
  CogitoColor system_accent;
  bool system_accent_set;
} CogitoApp;

typedef enum {
  COGITO_APPBAR,
  COGITO_BOTTOM_NAV,
  COGITO_BUTTON,
  COGITO_CHECKBOX,
  COGITO_CHIP,
  COGITO_COLORPICKER,
  COGITO_DATEPICKER,
  COGITO_DIALOG,
  COGITO_DIALOG_SLOT,
  COGITO_DROPDOWN,
  COGITO_FAB,
  COGITO_FIXED,
  COGITO_GRID,
  COGITO_HSTACK,
  COGITO_ICONBTN,
  COGITO_IMAGE,
  COGITO_LABEL,
  COGITO_LIST,
  COGITO_NAV_RAIL,
  COGITO_POPOVER,
  COGITO_PROGRESS,
  COGITO_SCROLLER,
  COGITO_SEARCHFIELD,
  COGITO_SEGMENTED,
  COGITO_SLIDER,
  COGITO_STEPPER,
  COGITO_SWITCH,
  COGITO_TABS,
  COGITO_TEXTFIELD,
  COGITO_TEXTVIEW,
  COGITO_TOAST,
  COGITO_TOASTS,
  COGITO_TOOLBAR,
  COGITO_TOOLTIP,
  COGITO_TREEVIEW,
  COGITO_VIEWSWITCHER,
  COGITO_VSTACK,
  COGITO_WINDOW,
  COGITO_ZSTACK,
  COGITO_KIND_COUNT
} CogitoKind;

typedef enum {
  COGITO_BORDER_NONE = 0,
  COGITO_BORDER_SOLID = 1,
  COGITO_BORDER_DASHED = 2,
  COGITO_BORDER_DOTTED = 3
} CogitoBorderStyle;

typedef struct CogitoBoxShadow {
  float dx;
  float dy;
  float blur;
  float spread;
  CogitoColor color;
  bool inset;
} CogitoBoxShadow;

typedef struct CogitoStyle {
  bool has_bg;
  bool has_text;
  bool has_border;
  bool has_border_width;
  bool has_border_style;
  bool has_radius;
  bool has_radius_tl;
  bool has_radius_tr;
  bool has_radius_br;
  bool has_radius_bl;
  bool has_font_size;
  bool has_padding;
  bool has_padding_left;
  bool has_padding_top;
  bool has_padding_right;
  bool has_padding_bottom;
  bool has_margin;
  bool has_margin_left;
  bool has_margin_top;
  bool has_margin_right;
  bool has_margin_bottom;
  bool has_selection;
  bool has_track;
  bool has_track_on;
  bool has_knob;
  bool has_check;
  bool has_font_weight;
  bool has_letter_spacing;
  bool has_font_tabular;
  bool has_font_mono;
  bool has_font_family;
  bool has_box_shadow;
  bool has_min_w;
  bool has_min_h;
  bool has_max_w;
  bool has_max_h;
  bool has_icon_size;
  bool has_icon_color;
  bool has_transition_ms;
  bool has_transition_ease;
  bool has_appbar_btn_size;
  bool has_appbar_btn_gap;
  bool has_appbar_btn_pad_top;
  bool has_appbar_btn_pad_right;
  bool has_appbar_btn_close_color;
  bool has_appbar_btn_min_color;
  bool has_appbar_btn_max_color;
  bool has_appbar_btn_border_color;
  bool has_appbar_btn_border_width;
  bool has_shadow;
  bool has_opacity;
  bool has_track_height;
  CogitoColor bg;
  CogitoColor text;
  CogitoColor border;
  CogitoColor selection;
  CogitoColor track;
  CogitoColor track_on;
  CogitoColor knob;
  CogitoColor check;
  CogitoColor appbar_btn_close_color;
  CogitoColor appbar_btn_min_color;
  CogitoColor appbar_btn_max_color;
  CogitoColor appbar_btn_border_color;
  int border_width;
  CogitoBorderStyle border_style;
  int radius;
  int radius_tl;
  int radius_tr;
  int radius_br;
  int radius_bl;
  int font_size;
  int font_weight;
  float letter_spacing;
  bool font_tabular;
  bool font_mono;
  char font_family[64];
  CogitoBoxShadow box_shadow;
  int min_w;
  int min_h;
  int max_w;
  int max_h;
  int icon_size;
  CogitoColor icon_color;
  int transition_ms;
  int transition_ease;
  int appbar_btn_size;
  int appbar_btn_gap;
  int appbar_btn_pad_top;
  int appbar_btn_pad_right;
  int appbar_btn_border_width;
  int shadow_level;
  int padding_left;
  int padding_top;
  int padding_right;
  int padding_bottom;
  int margin_left;
  int margin_top;
  int margin_right;
  int margin_bottom;
  float opacity;
  int track_height;
} CogitoStyle;

#define COGITO_MAX_CUSTOM_CLASSES 32

typedef struct CogitoCustomClassStyle {
  char name[64];
  int kind;
  CogitoStyle style;
  CogitoStyle hover;
  CogitoStyle active;
  CogitoStyle checked;
  CogitoStyle disabled;
} CogitoCustomClassStyle;

typedef struct CogitoTheme {
  CogitoStyle base;
  CogitoStyle per_kind[COGITO_KIND_COUNT];
  CogitoStyle per_kind_hover[COGITO_KIND_COUNT];
  CogitoStyle per_kind_active[COGITO_KIND_COUNT];
  CogitoStyle per_kind_checked[COGITO_KIND_COUNT];
  CogitoStyle per_kind_disabled[COGITO_KIND_COUNT];
  CogitoStyle label_class[4];
  CogitoStyle label_class_hover[4];
  CogitoStyle label_class_active[4];
  CogitoStyle label_class_checked[4];
  CogitoStyle label_class_disabled[4];
  CogitoStyle class_mono;
  CogitoStyle class_mono_hover;
  CogitoStyle class_mono_active;
  CogitoStyle class_mono_checked;
  CogitoStyle class_mono_disabled;
  CogitoStyle class_tabular;
  CogitoStyle class_tabular_hover;
  CogitoStyle class_tabular_active;
  CogitoStyle class_tabular_checked;
  CogitoStyle class_tabular_disabled;
  CogitoStyle menu;
  CogitoStyle menu_item;
  CogitoStyle appbar_child_button;
  CogitoStyle appbar_child_button_hover;
  CogitoStyle appbar_child_button_active;
  CogitoStyle appbar_child_iconbtn;
  CogitoStyle appbar_child_iconbtn_hover;
  CogitoStyle appbar_child_iconbtn_active;
  CogitoStyle appbar_win_btn_all;
  CogitoStyle appbar_win_btn_hover_all;
  CogitoStyle appbar_win_btn_active_all;
  CogitoStyle appbar_win_btn[3];
  CogitoStyle appbar_win_btn_hover[3];
  CogitoStyle appbar_win_btn_active[3];
  CogitoStyle checkbox_check;
  CogitoStyle checkbox_check_hover;
  CogitoStyle checkbox_check_active;
  CogitoStyle checkbox_check_checked;
  CogitoStyle radio_check;
  CogitoStyle radio_check_hover;
  CogitoStyle radio_check_active;
  CogitoStyle radio_check_checked;
  CogitoStyle checkbox_box;
  CogitoStyle checkbox_box_hover;
  CogitoStyle checkbox_box_active;
  CogitoStyle checkbox_box_checked;
  CogitoStyle radio_box;
  CogitoStyle radio_box_hover;
  CogitoStyle radio_box_active;
  CogitoStyle radio_box_checked;
  CogitoStyle switch_track;
  CogitoStyle switch_track_hover;
  CogitoStyle switch_track_active;
  CogitoStyle switch_track_checked;
  CogitoStyle switch_knob;
  CogitoStyle switch_knob_hover;
  CogitoStyle switch_knob_active;
  CogitoStyle switch_knob_checked;
  CogitoStyle button_outlined;
  CogitoStyle button_outlined_hover;
  CogitoStyle button_outlined_active;
  CogitoStyle button_outlined_disabled;
  CogitoStyle button_text;
  CogitoStyle button_text_hover;
  CogitoStyle button_text_active;
  CogitoStyle button_text_disabled;
  CogitoCustomClassStyle custom_classes[COGITO_MAX_CUSTOM_CLASSES];
  int custom_class_count;
} CogitoTheme;

typedef struct CogitoFontCacheEntry {
  int size;
  CogitoFont* font;
  bool ready;
} CogitoFontCacheEntry;

typedef struct CogitoNode {
  ErgoObj base;
  CogitoKind kind;
  struct CogitoNode* parent;
  struct CogitoNode** children;
  size_t len;
  size_t cap;
  ErgoStr** menu_labels;
  ErgoFn** menu_handlers;
  size_t menu_len;
  size_t menu_cap;
  ErgoStr** tab_labels;
  ErgoStr** tab_ids;
  size_t tab_len;
  size_t tab_cap;
  ErgoStr* text;
  ErgoStr* subtitle;
  ErgoStr* icon;
  ErgoStr* tooltip;
  ErgoStr* action_text;
  ErgoStr* view_id;
  ErgoStr* view_active_id;
  ErgoFn* on_click;
  ErgoFn* on_change;
  ErgoFn* on_select;
  ErgoFn* on_activate;
  ErgoFn* builder;
  ErgoFn* on_action;
  ErgoVal group;
  bool checked;
  int x;
  int y;
  int w;
  int h;
  int grid_cols;
  int grid_gap_x;
  int grid_gap_y;
  int grid_align_x;
  int grid_align_y;
  int grid_span_x;
  int grid_span_y;
  int fixed_x;
  int fixed_y;
  int scroll_x;
  int scroll_y;
  int scroll_content_w;
  int scroll_content_h;
  bool scroll_horz;
  bool scroll_vert;
  int selected;
  int margin_left;
  int margin_top;
  int margin_right;
  int margin_bottom;
  int padding_left;
  int padding_top;
  int padding_right;
  int padding_bottom;
  int align;
  bool hexpand;
  bool vexpand;
  int gap;
  bool auto_size;
  bool should_close;
  int appbar_btn_close_x;
  int appbar_btn_min_x;
  int appbar_btn_max_x;
  int appbar_btn_y;
  int appbar_btn_size;
  int appbar_title_y;
  int appbar_subtitle_y;
  char appbar_controls[16];
  int action_button_x;
  int action_button_y;
  int action_button_w;
  int action_button_h;
  struct CogitoNode* dialog;
  struct CogitoNode* view_switcher;
  bool dialog_open;
  struct CogitoNode* popover;
  struct CogitoNode* popover_anchor;
  bool popover_open;
  bool resizable;
  bool fixed_pos_set;
  bool text_wrap;
  bool text_ellipsis;
  int text_align;
  bool disabled;
  double slider_min;
  double slider_max;
  double slider_value;
  bool slider_dragging;
  double stepper_min;
  double stepper_max;
  double stepper_value;
  double stepper_step;
  bool stepper_hit_minus;
  bool stepper_hit_plus;
  int date_year;
  int date_month;
  int date_day;
  double color_h;
  double color_c;
  double color_t;
  bool color_hex_editing;
  int caret;
  int sel_start;
  int sel_end;
  bool selecting;
  bool font_mono;
  bool font_tabular;
  ErgoStr* a11y_label;
  ErgoStr* a11y_role;
  ErgoStr* class_name;
  bool anim_inited;
  double anim_start;
  int anim_ms;
  int anim_ease;
  CogitoColor anim_bg;
  CogitoColor anim_bg_from;
  CogitoColor anim_bg_target;
  CogitoColor anim_text;
  CogitoColor anim_text_from;
  CogitoColor anim_text_target;
  CogitoColor anim_border;
  CogitoColor anim_border_from;
  CogitoColor anim_border_target;
  CogitoColor anim_selection;
  CogitoColor anim_selection_from;
  CogitoColor anim_selection_target;
  double anim_bg_start;
  double anim_text_start;
  double anim_border_start;
  double anim_selection_start;
  int anim_x;
  int anim_x_from;
  int anim_x_target;
  double anim_x_start;
  int anim_y;
  int anim_y_from;
  int anim_y_target;
  double anim_y_start;
  int anim_w;
  int anim_w_from;
  int anim_w_target;
  double anim_w_start;
  int anim_h;
  int anim_h_from;
  int anim_h_target;
  double anim_h_start;
  bool anim_layout_active;
  double active_latch_until;
  float opacity;
  float opacity_from;
  float opacity_target;
  double opacity_start;
  bool has_opacity;
  bool opacity_set;
  double progress_value;
  bool margin_set;
  bool padding_set;
  bool bg_set;
  bool text_color_set;
  bool border_color_set;
  bool border_width_set;
  bool radius_set;
  bool selection_set;
  bool track_set;
  bool track_on_set;
  bool knob_set;
  bool check_set;
  bool font_weight_set;
  bool letter_spacing_set;
  bool min_w_set;
  bool min_h_set;
  bool max_w_set;
  bool shadow_set;
  bool max_h_set;
  CogitoColor bg;
  CogitoColor text_color;
  CogitoColor border_color;
  CogitoColor selection_color;
  CogitoColor track_color;
  CogitoColor track_on_color;
  CogitoColor knob_color;
  CogitoColor check_color;
  int border_width;
  int border_radius;
  int label_class;
  int font_size;
  bool font_size_set;
  int font_weight;
  float letter_spacing;
  int min_w;
  int min_h;
  int max_w;
  int max_h;
  int shadow_level;
  CogitoBorderStyle border_style;
  bool border_style_set;
  int radius_tl;
  int radius_tr;
  int radius_br;
  int radius_bl;
  bool radius_tl_set;
  bool radius_tr_set;
  bool radius_br_set;
  bool radius_bl_set;
  char font_family[64];
  bool font_family_set;
  CogitoBoxShadow box_shadow;
  bool box_shadow_set;
  int track_height;
  bool track_height_set;
  bool button_outlined;
  bool button_text;
  bool chip_selected;
  bool chip_has_close;
  bool fab_extended;
  ErgoStr** nav_rail_labels;
  ErgoStr** nav_rail_icons;
  size_t nav_rail_len;
  size_t nav_rail_cap;
  int nav_rail_selected;
  ErgoStr** bottom_nav_labels;
  ErgoStr** bottom_nav_icons;
  size_t bottom_nav_len;
  size_t bottom_nav_cap;
  int bottom_nav_selected;
} CogitoNode;

typedef struct CogitoState {
  ErgoObj base;
  ErgoVal value;
} CogitoState;

typedef struct CogitoMenu {
  bool open;
  CogitoNode* owner;
  int x;
  int y;
  int w;
  int h;
  int item_h;
  int pad_x;
  int pad_y;
  int pad_top;
  int pad_bottom;
} CogitoMenu;

static CogitoTheme cogito_theme;
static bool cogito_theme_initialized = false;
static bool cogito_backend_ready = false;
static CogitoMenu cogito_menu = {0};
static CogitoFont* cogito_font = NULL;
static bool cogito_font_ready = false;
static CogitoFont* cogito_font_mono = NULL;
static bool cogito_font_mono_ready = false;
static bool cogito_font_mono_from_env = false;
static const char* cogito_font_mono_path_active = NULL;
static int cogito_font_mono_loaded_size = 0;
static CogitoFont* cogito_font_bold = NULL;
static bool cogito_font_bold_ready = false;
static bool cogito_font_bold_variable = false;
static CogitoFontCacheEntry cogito_font_cache[8];
static CogitoFontCacheEntry cogito_font_bold_cache[8];
static CogitoFontCacheEntry cogito_font_tabular_cache[8];
static int cogito_font_size_value = 16;
static int cogito_font_loaded_size = 0;
static int cogito_font_bold_loaded_size = 0;
static bool cogito_font_from_env = false;
static bool cogito_font_bold_from_env = false;
static bool cogito_sum_default_loaded = false;
static char* cogito_user_sum = NULL;
static int cogito_style_version = 0;
static CogitoNode* cogito_focused = NULL;
// Draw-context: forced text color for selected children (propagates down render tree)
static CogitoColor cogito_selection_text = {0};
static bool cogito_has_selection_text = false;
static bool cogito_dialog_slot_force_draw = false;
static bool cogito_toasts_force_draw = false;
static CogitoNode* cogito_slider_active = NULL;
static CogitoNode* cogito_colorpicker_active = NULL;
static int cogito_colorpicker_channel = -1;
static CogitoNode* cogito_pointer_capture = NULL;
static CogitoNode* cogito_tooltip_hovered = NULL;
static CogitoApp* cogito_active_app = NULL;
static CogitoNode* cogito_active_window = NULL;
static CogitoWindow* cogito_active_backend_window = NULL;
static bool cogito_theme_dark = false;
static CogitoColor cogito_theme_accent = {114, 222, 194, 255};
enum {
  COGITO_LABEL_CLASS_NONE = 0,
  COGITO_LABEL_CLASS_TITLE,
  COGITO_LABEL_CLASS_SUBTITLE,
  COGITO_LABEL_CLASS_BODY
};
enum {
  COGITO_STYLE_CLASS_NONE = 0,
  COGITO_STYLE_CLASS_MONO,
  COGITO_STYLE_CLASS_TABULAR,
  COGITO_STYLE_CLASS_OUTLINED,
  COGITO_STYLE_CLASS_TEXT
};
static bool cogito_drag_active = false;
static CogitoVec2 cogito_drag_start_mouse = {0};
static CogitoVec2 cogito_drag_start_win = {0};
static bool cogito_drag_pending = false;
static CogitoVec2 cogito_drag_pending_mouse = {0};
static double cogito_drag_last_move = 0.0;
static bool cogito_inspector_enabled = false;
static bool cogito_inspector_open = false;
static CogitoNode* cogito_inspector_selected = NULL;
#define COGITO_INSPECTOR_MAX_NODES 2048
static CogitoNode* cogito_inspector_nodes[COGITO_INSPECTOR_MAX_NODES];
static int cogito_inspector_depths[COGITO_INSPECTOR_MAX_NODES];
static int cogito_inspector_node_count = 0;

static CogitoWindow* cogito_backend_window_for_node(CogitoNode* node) {
  if (!node || !cogito_active_window || !cogito_active_backend_window) return NULL;
  if (node == cogito_active_window) return cogito_active_backend_window;
  return NULL;
}

#if defined(__APPLE__)
typedef unsigned long CogitoNSUInteger;

static id cogito_nsstring(const char* s) {
  id nsstring = (id)objc_getClass("NSString");
  SEL sel = sel_registerName("stringWithUTF8String:");
  return ((id (*)(id, SEL, const char*))objc_msgSend)(nsstring, sel, s ? s : "");
}

static void cogito_configure_macos_window(bool has_appbar) {
  if (!has_appbar) return;
  // Get native window handle from backend
  if (!cogito_backend || !cogito_active_backend_window) return;
  id nsw =
#if defined(__OBJC__)
      (__bridge id)cogito_backend->window_get_native_handle(cogito_active_backend_window);
#else
      (id)cogito_backend->window_get_native_handle(cogito_active_backend_window);
#endif
  if (!nsw) return;
  SEL sel_set_title = sel_registerName("setTitle:");
  ((void (*)(id, SEL, id))objc_msgSend)(nsw, sel_set_title, cogito_nsstring(""));
  SEL sel_set_title_visibility = sel_registerName("setTitleVisibility:");
  ((void (*)(id, SEL, CogitoNSUInteger))objc_msgSend)(nsw, sel_set_title_visibility, 1UL);
  SEL sel_set_transparent = sel_registerName("setTitlebarAppearsTransparent:");
  ((void (*)(id, SEL, BOOL))objc_msgSend)(nsw, sel_set_transparent, (BOOL)1);
  SEL sel_set_movable = sel_registerName("setMovableByWindowBackground:");
  // Dragging should start only from Cogito appbar hit-testing, not full-window background.
  ((void (*)(id, SEL, BOOL))objc_msgSend)(nsw, sel_set_movable, (BOOL)0);
  SEL sel_style_mask = sel_registerName("styleMask");
  CogitoNSUInteger mask = ((CogitoNSUInteger (*)(id, SEL))objc_msgSend)(nsw, sel_style_mask);
  mask |= (1UL << 15);
  SEL sel_set_style_mask = sel_registerName("setStyleMask:");
  ((void (*)(id, SEL, CogitoNSUInteger))objc_msgSend)(nsw, sel_set_style_mask, mask);
  SEL sel_standard_button = sel_registerName("standardWindowButton:");
  id close = ((id (*)(id, SEL, CogitoNSUInteger))objc_msgSend)(nsw, sel_standard_button, 0UL);
  id min = ((id (*)(id, SEL, CogitoNSUInteger))objc_msgSend)(nsw, sel_standard_button, 1UL);
  id zoom = ((id (*)(id, SEL, CogitoNSUInteger))objc_msgSend)(nsw, sel_standard_button, 2UL);
  SEL sel_set_hidden = sel_registerName("setHidden:");
  if (close) ((void (*)(id, SEL, BOOL))objc_msgSend)(close, sel_set_hidden, (BOOL)1);
  if (min) ((void (*)(id, SEL, BOOL))objc_msgSend)(min, sel_set_hidden, (BOOL)1);
  if (zoom) ((void (*)(id, SEL, BOOL))objc_msgSend)(zoom, sel_set_hidden, (BOOL)1);
}

static void cogito_macos_drag_window(void) {
  if (!cogito_backend || !cogito_active_backend_window) return;
  id nsw =
#if defined(__OBJC__)
      (__bridge id)cogito_backend->window_get_native_handle(cogito_active_backend_window);
#else
      (id)cogito_backend->window_get_native_handle(cogito_active_backend_window);
#endif
  if (!nsw) return;
  Class nsapp = (Class)objc_getClass("NSApplication");
  if (!nsapp) return;
  SEL sel_shared = sel_registerName("sharedApplication");
  id app = ((id (*)(id, SEL))objc_msgSend)((id)nsapp, sel_shared);
  if (!app) return;
  SEL sel_event = sel_registerName("currentEvent");
  id event = ((id (*)(id, SEL))objc_msgSend)(app, sel_event);
  if (!event) return;
  SEL sel_drag = sel_registerName("performWindowDragWithEvent:");
  ((void (*)(id, SEL, id))objc_msgSend)(nsw, sel_drag, event);
}

static void cogito_macos_set_window_a11y_label(const char* label) {
  if (!cogito_backend || !cogito_active_backend_window) return;
  id nsw =
#if defined(__OBJC__)
      (__bridge id)cogito_backend->window_get_native_handle(cogito_active_backend_window);
#else
      (id)cogito_backend->window_get_native_handle(cogito_active_backend_window);
#endif
  if (!nsw) return;
  SEL sel = sel_registerName("setAccessibilityLabel:");
  if (!sel) return;
  ((void (*)(id, SEL, id))objc_msgSend)(nsw, sel, cogito_nsstring(label ? label : ""));
}
#endif

// Use cogito_color from backend.h instead
#define cogito_rgba cogito_color

static CogitoFont* cogito_font_for_node(const CogitoNode* n) {
  if (n && n->font_tabular) {
    return cogito_font;
  }
  if (n && n->font_mono && cogito_font_mono_ready) return cogito_font_mono;
  return cogito_font;
}

// Use backend.h implementations
#define cogito_color_eq(a, b) ((a).r == (b).r && (a).g == (b).g && (a).b == (b).b && (a).a == (b).a)

// Use backend.h implementation
#define cogito_ease cogito_backend_ease

static double cogito_backend_ease(double t, int ease) {
  if (t <= 0.0) return 0.0;
  if (t >= 1.0) return 1.0;
  switch (ease) {
    case 2: return t * t; // ease-in
    case 3: { double u = 1.0 - t; return 1.0 - u * u; } // ease-out
    case 4: { // ease-in-out
      if (t < 0.5) return 2.0 * t * t;
      double u = 1.0 - t;
      return 1.0 - 2.0 * u * u;
    }
    case 1: // ease
      return t * t * (3.0 - 2.0 * t);
    case 5: { // spring (ease-in-back)
      double s = 1.70158;
      return t * t * ((s + 1) * t - s);
    }
    case 6: { // spring-out (ease-out-back)
      double u = t - 1;
      double s = 1.70158;
      return u * u * ((s + 1) * u + s) + 1;
    }
    default: return t; // linear
  }
}

static CogitoColor cogito_anim_color(CogitoNode* n, CogitoColor target, CogitoColor* cur, CogitoColor* from, CogitoColor* tgt, double* start, double now) {
  if (!n) return target;
  if (!n->anim_inited) {
    *cur = target;
    *from = target;
    *tgt = target;
    if (start) *start = now;
    n->anim_inited = true;
    return target;
  }
  bool changed = !(tgt->r == target.r && tgt->g == target.g && tgt->b == target.b && tgt->a == target.a);
  if (changed) {
    *from = *cur;
    *tgt = target;
    if (start) *start = now;
  }
  if (n->anim_ms <= 0) {
    *cur = target;
    if (start) *start = now;
    return target;
  }
  if (!changed && cur->r == target.r && cur->g == target.g && cur->b == target.b && cur->a == target.a) {
    return *cur;
  }
  double t0 = start ? *start : n->anim_start;
  double t = (now - t0) * 1000.0 / (double)n->anim_ms;
  if (t >= 1.0) {
    *cur = target;
    return target;
  }
  double e = cogito_ease(t, n->anim_ease);
  *cur = cogito_color_lerp(*from, *tgt, e);
  return *cur;
}

static int cogito_anim_int(CogitoNode* n, int target, int* cur, int* from, int* tgt, double* start, double now) {
  if (!n) return target;
  if (!n->anim_inited) {
    *cur = target;
    *from = target;
    *tgt = target;
    if (start) *start = now;
    n->anim_inited = true;
    return target;
  }
  bool changed = (*tgt != target);
  if (changed) {
    *from = *cur;
    *tgt = target;
    if (start) *start = now;
  }
  if (n->anim_ms <= 0) {
    *cur = target;
    if (start) *start = now;
    return target;
  }
  if (!changed && *cur == target) {
    return *cur;
  }
  double t0 = start ? *start : n->anim_start;
  double t = (now - t0) * 1000.0 / (double)n->anim_ms;
  if (t >= 1.0) {
    *cur = target;
    return target;
  }
  double e = cogito_ease(t, n->anim_ease);
  *cur = (int)(*from + e * (*tgt - *from));
  return *cur;
}

// Use backend.h implementation
#define cogito_apply_opacity cogito_color_apply_opacity

static float cogito_anim_float(CogitoNode* n, float target, float* cur, float* from, float* tgt, double* start, double now) {
  if (!n) return target;
  if (!n->anim_inited) {
    *cur = target;
    *from = target;
    *tgt = target;
    if (start) *start = now;
    n->anim_inited = true;
    return target;
  }
  bool changed = (*tgt != target);
  if (changed) {
    *from = *cur;
    *tgt = target;
    if (start) *start = now;
  }
  if (n->anim_ms <= 0) {
    *cur = target;
    if (start) *start = now;
    return target;
  }
  if (!changed && *cur == target) {
    return *cur;
  }
  double t0 = start ? *start : n->anim_start;
  double t = (now - t0) * 1000.0 / (double)n->anim_ms;
  if (t >= 1.0) {
    *cur = target;
    return target;
  }
  double e = cogito_ease(t, n->anim_ease);
  *cur = *from + (float)e * (*tgt - *from);
  return *cur;
}

// Apply opacity to a color by multiplying its alpha channel
// Use backend.h implementation
#define cogito_apply_opacity cogito_color_apply_opacity

static void cogito_anim_apply(CogitoNode* n, const CogitoStyle* s, CogitoColor* bg, CogitoColor* text, CogitoColor* border, CogitoColor* selection) {
  if (!n || !s) return;
  if (s->has_transition_ms) n->anim_ms = s->transition_ms;
  if (s->has_transition_ease) n->anim_ease = s->transition_ease;
  double now = cogito_backend->get_time();
  // Animate opacity if set
  float opacity = n->opacity;
  if (n->opacity_set) {
    opacity = cogito_anim_float(n, n->opacity, &n->opacity, &n->opacity_from, &n->opacity_target, &n->opacity_start, now);
  }
  if (bg) {
    *bg = cogito_anim_color(n, *bg, &n->anim_bg, &n->anim_bg_from, &n->anim_bg_target, &n->anim_bg_start, now);
    *bg = cogito_apply_opacity(*bg, opacity);
  }
  if (text) {
    *text = cogito_anim_color(n, *text, &n->anim_text, &n->anim_text_from, &n->anim_text_target, &n->anim_text_start, now);
    *text = cogito_apply_opacity(*text, opacity);
  }
  if (border) {
    *border = cogito_anim_color(n, *border, &n->anim_border, &n->anim_border_from, &n->anim_border_target, &n->anim_border_start, now);
    *border = cogito_apply_opacity(*border, opacity);
  }
  if (selection) {
    *selection = cogito_anim_color(n, *selection, &n->anim_selection, &n->anim_selection_from, &n->anim_selection_target, &n->anim_selection_start, now);
    *selection = cogito_apply_opacity(*selection, opacity);
  }
}

static void cogito_style_clear(CogitoStyle* s) {
  memset(s, 0, sizeof(*s));
}

static void cogito_draw_rect(int x, int y, int w, int h, CogitoColor bg, int radius);
static bool cogito_hit_rect(int x, int y, int rx, int ry, int rw, int rh);
static CogitoNode* cogito_node_window(CogitoNode* n);
static bool cogito_file_exists(const char* path);
static int cogito_font_weight_normal(void);
static int cogito_font_weight_bold(void);
static bool cogito_load_font_file_ex(CogitoFont** out, const char* path, int size, int weight, bool* out_weight_applied);
static bool cogito_load_font_file(CogitoFont** out, const char* path, int size, int weight);
static bool cogito_load_font_file_tabular(CogitoFont** out, const char* path, int size, int weight);
static CogitoFont* cogito_font_for_size(CogitoFont* f, int size);
static CogitoFont* cogito_font_tabular_for(CogitoFont* f, int size);
static void cogito_font_cache_clear(void);
