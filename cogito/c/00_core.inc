// ---- Cogito GUI (raylib) ----

#include <raylib.h>
#if defined(__APPLE__)
#include <objc/objc.h>
#include <objc/message.h>
#include <objc/runtime.h>
#endif

static bool cogito_system_is_dark(void);
static bool cogito_system_accent(Color* out);
static void cogito_apply_theme(bool dark, Color accent);

static float cogito_luma(Color c) {
  float r = c.r / 255.0f;
  float g = c.g / 255.0f;
  float b = c.b / 255.0f;
  return 0.2126f * r + 0.7152f * g + 0.0722f * b;
}

static Color cogito_mix(Color a, Color b, float t) {
  Color c;
  c.r = (unsigned char)(a.r + (b.r - a.r) * t);
  c.g = (unsigned char)(a.g + (b.g - a.g) * t);
  c.b = (unsigned char)(a.b + (b.b - a.b) * t);
  c.a = (unsigned char)(a.a + (b.a - a.a) * t);
  return c;
}

static Color cogito_on_color(Color c) {
  return cogito_luma(c) > 0.55f
    ? (Color){0, 0, 0, 255}
    : (Color){255, 255, 255, 255};
}

typedef struct CogitoApp {
  ErgoObj base;
  void* native;
  ErgoStr* app_id;
  bool accent_override;
  Color accent_color;
  bool accent_set;
  bool system_dark;
  Color system_accent;
  bool system_accent_set;
} CogitoApp;

typedef enum {
  COGITO_WINDOW,
  COGITO_VSTACK,
  COGITO_HSTACK,
  COGITO_ZSTACK,
  COGITO_FIXED,
  COGITO_SCROLLER,
  COGITO_TEXTFIELD,
  COGITO_TEXTVIEW,
  COGITO_SEARCHFIELD,
  COGITO_DROPDOWN,
  COGITO_DATEPICKER,
  COGITO_STEPPER,
  COGITO_SLIDER,
  COGITO_TABS,
  COGITO_SEGMENTED,
  COGITO_VIEWSWITCHER,
  COGITO_PROGRESS,
  COGITO_TREEVIEW,
  COGITO_COLORPICKER,
  COGITO_TOASTS,
  COGITO_TOAST,
  COGITO_TOOLBAR,
  COGITO_TOOLTIP,
  COGITO_BUTTON,
  COGITO_ICONBTN,
  COGITO_IMAGE,
  COGITO_LABEL,
  COGITO_CHECKBOX,
  COGITO_SWITCH,
  COGITO_LIST,
  COGITO_GRID,
  COGITO_DIALOG,
  COGITO_DIALOG_SLOT,
  COGITO_APPBAR,
  COGITO_KIND_COUNT
} CogitoKind;

typedef struct CogitoStyle {
  bool has_bg;
  bool has_text;
  bool has_border;
  bool has_border_width;
  bool has_radius;
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
  Color bg;
  Color text;
  Color border;
  Color selection;
  Color track;
  Color track_on;
  Color knob;
  Color check;
  Color appbar_btn_close_color;
  Color appbar_btn_min_color;
  Color appbar_btn_max_color;
  Color appbar_btn_border_color;
  int border_width;
  int radius;
  int font_size;
  int font_weight;
  float letter_spacing;
  bool font_tabular;
  bool font_mono;
  int min_w;
  int min_h;
  int max_w;
  int max_h;
  int icon_size;
  Color icon_color;
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
} CogitoStyle;

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
} CogitoTheme;

typedef struct CogitoFontCacheEntry {
  int size;
  Font font;
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
  ErgoStr* view_id;
  ErgoStr* view_active_id;
  ErgoFn* on_click;
  ErgoFn* on_change;
  ErgoFn* on_select;
  ErgoFn* on_activate;
  ErgoFn* builder;
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
  struct CogitoNode* dialog;
  struct CogitoNode* view_switcher;
  bool dialog_open;
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
  bool anim_inited;
  double anim_start;
  int anim_ms;
  int anim_ease;
  Color anim_bg;
  Color anim_bg_from;
  Color anim_bg_target;
  Color anim_text;
  Color anim_text_from;
  Color anim_text_target;
  Color anim_border;
  Color anim_border_from;
  Color anim_border_target;
  Color anim_selection;
  Color anim_selection_from;
  Color anim_selection_target;
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
  Color bg;
  Color text_color;
  Color border_color;
  Color selection_color;
  Color track_color;
  Color track_on_color;
  Color knob_color;
  Color check_color;
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
static bool cogito_raylib_ready = false;
static CogitoMenu cogito_menu = {0};
static Font cogito_font;
static bool cogito_font_ready = false;
static Font cogito_font_mono;
static bool cogito_font_mono_ready = false;
static bool cogito_font_mono_from_env = false;
static const char* cogito_font_mono_path_active = NULL;
static int cogito_font_mono_loaded_size = 0;
static Font cogito_font_bold;
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
static bool cogito_css_default_loaded = false;
static char* cogito_user_css = NULL;
static int cogito_style_version = 0;
static CogitoNode* cogito_focused = NULL;
static CogitoNode* cogito_slider_active = NULL;
static CogitoNode* cogito_colorpicker_active = NULL;
static int cogito_colorpicker_channel = -1;
static CogitoNode* cogito_pointer_capture = NULL;
static CogitoNode* cogito_tooltip_hovered = NULL;
static CogitoApp* cogito_active_app = NULL;
static bool cogito_theme_dark = false;
static Color cogito_theme_accent = {114, 222, 194, 255};
enum {
  COGITO_LABEL_CLASS_NONE = 0,
  COGITO_LABEL_CLASS_TITLE,
  COGITO_LABEL_CLASS_SUBTITLE,
  COGITO_LABEL_CLASS_BODY
};
enum {
  COGITO_STYLE_CLASS_NONE = 0,
  COGITO_STYLE_CLASS_MONO,
  COGITO_STYLE_CLASS_TABULAR
};
static bool cogito_drag_active = false;
static Vector2 cogito_drag_start_mouse = {0};
static Vector2 cogito_drag_start_win = {0};
static bool cogito_drag_pending = false;
static Vector2 cogito_drag_pending_mouse = {0};
static double cogito_drag_last_move = 0.0;

#if defined(__APPLE__)
typedef unsigned long CogitoNSUInteger;

static id cogito_nsstring(const char* s) {
  id nsstring = (id)objc_getClass("NSString");
  SEL sel = sel_registerName("stringWithUTF8String:");
  return ((id (*)(id, SEL, const char*))objc_msgSend)(nsstring, sel, s ? s : "");
}

static void cogito_configure_macos_window(bool has_appbar) {
  if (!has_appbar) return;
  id nsw =
#if defined(__OBJC__)
      (__bridge id)GetWindowHandle();
#else
      (id)GetWindowHandle();
#endif
  if (!nsw) return;
  SEL sel_set_title = sel_registerName("setTitle:");
  ((void (*)(id, SEL, id))objc_msgSend)(nsw, sel_set_title, cogito_nsstring(""));
  SEL sel_set_title_visibility = sel_registerName("setTitleVisibility:");
  ((void (*)(id, SEL, CogitoNSUInteger))objc_msgSend)(nsw, sel_set_title_visibility, 1UL);
  SEL sel_set_transparent = sel_registerName("setTitlebarAppearsTransparent:");
  ((void (*)(id, SEL, BOOL))objc_msgSend)(nsw, sel_set_transparent, (BOOL)1);
  SEL sel_set_movable = sel_registerName("setMovableByWindowBackground:");
  ((void (*)(id, SEL, BOOL))objc_msgSend)(nsw, sel_set_movable, (BOOL)1);
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
  id nsw =
#if defined(__OBJC__)
      (__bridge id)GetWindowHandle();
#else
      (id)GetWindowHandle();
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
  id nsw =
#if defined(__OBJC__)
      (__bridge id)GetWindowHandle();
#else
      (id)GetWindowHandle();
#endif
  if (!nsw) return;
  SEL sel = sel_registerName("setAccessibilityLabel:");
  if (!sel) return;
  ((void (*)(id, SEL, id))objc_msgSend)(nsw, sel, cogito_nsstring(label ? label : ""));
}
#endif

static Color cogito_rgba(int r, int g, int b, int a) {
  if (r < 0) r = 0; if (r > 255) r = 255;
  if (g < 0) g = 0; if (g > 255) g = 255;
  if (b < 0) b = 0; if (b > 255) b = 255;
  if (a < 0) a = 0; if (a > 255) a = 255;
  Color c;
  c.r = (unsigned char)r;
  c.g = (unsigned char)g;
  c.b = (unsigned char)b;
  c.a = (unsigned char)a;
  return c;
}

static Font* cogito_font_for_node(const CogitoNode* n) {
  if (n && n->font_tabular) {
    return &cogito_font;
  }
  if (n && n->font_mono && cogito_font_mono_ready) return &cogito_font_mono;
  return &cogito_font;
}

static bool cogito_color_eq(Color a, Color b) {
  return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static Color cogito_color_lerp(Color a, Color b, double t) {
  if (t <= 0.0) return a;
  if (t >= 1.0) return b;
  Color out;
  out.r = (unsigned char)lround(a.r + (b.r - a.r) * t);
  out.g = (unsigned char)lround(a.g + (b.g - a.g) * t);
  out.b = (unsigned char)lround(a.b + (b.b - a.b) * t);
  out.a = (unsigned char)lround(a.a + (b.a - a.a) * t);
  return out;
}

static double cogito_ease(double t, int ease) {
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
    default: return t; // linear
  }
}

static Color cogito_anim_color(CogitoNode* n, Color target, Color* cur, Color* from, Color* tgt, double now) {
  if (!n) return target;
  if (!n->anim_inited) {
    *cur = target;
    *from = target;
    *tgt = target;
    n->anim_inited = true;
    return target;
  }
  if (!cogito_color_eq(*tgt, target)) {
    *from = *cur;
    *tgt = target;
    n->anim_start = now;
  }
  if (n->anim_ms <= 0) {
    *cur = target;
    return target;
  }
  double t = (now - n->anim_start) * 1000.0 / (double)n->anim_ms;
  if (t >= 1.0) {
    *cur = target;
    return target;
  }
  double e = cogito_ease(t, n->anim_ease);
  *cur = cogito_color_lerp(*from, *tgt, e);
  return *cur;
}

static void cogito_anim_apply(CogitoNode* n, const CogitoStyle* s, Color* bg, Color* text, Color* border, Color* selection) {
  if (!n || !s) return;
  if (s->has_transition_ms) n->anim_ms = s->transition_ms;
  if (s->has_transition_ease) n->anim_ease = s->transition_ease;
  double now = GetTime();
  if (bg) *bg = cogito_anim_color(n, *bg, &n->anim_bg, &n->anim_bg_from, &n->anim_bg_target, now);
  if (text) *text = cogito_anim_color(n, *text, &n->anim_text, &n->anim_text_from, &n->anim_text_target, now);
  if (border) *border = cogito_anim_color(n, *border, &n->anim_border, &n->anim_border_from, &n->anim_border_target, now);
  if (selection) *selection = cogito_anim_color(n, *selection, &n->anim_selection, &n->anim_selection_from, &n->anim_selection_target, now);
}

static void cogito_style_clear(CogitoStyle* s) {
  memset(s, 0, sizeof(*s));
}

static void cogito_draw_rect(int x, int y, int w, int h, Color bg, int radius);
static bool cogito_hit_rect(int x, int y, int rx, int ry, int rw, int rh);
static CogitoNode* cogito_node_window(CogitoNode* n);
static bool cogito_file_exists(const char* path);
static int cogito_font_weight_normal(void);
static int cogito_font_weight_bold(void);
static bool cogito_load_font_file_ex(Font* out, const char* path, int size, int weight, bool* out_weight_applied);
static bool cogito_load_font_file(Font* out, const char* path, int size, int weight);
static bool cogito_load_font_file_tabular(Font* out, const char* path, int size, int weight);
static Font* cogito_font_for_size(Font* f, int size);
static Font* cogito_font_tabular_for(Font* f, int size);
static void cogito_font_cache_clear(void);

static void cogito_draw_shadow(int x, int y, int w, int h, int radius, int level) {
  if (level <= 0 || w <= 0 || h <= 0) return;
  if (level > 5) level = 5;
  int spread = 2 * level;
  int offset = level;
  int base_alpha = 20 + level * 12;
  if (base_alpha > 120) base_alpha = 120;
  int steps[3] = {spread, (spread * 2) / 3, spread / 3};
  int alphas[3] = {base_alpha, (base_alpha * 2) / 3, base_alpha / 3};
  for (int i = 0; i < 3; i++) {
    int s = steps[i];
    if (s <= 0) continue;
    int r = radius + s;
    Color c = cogito_rgba(0, 0, 0, alphas[i]);
    cogito_draw_rect(x - s, y - s + offset, w + s * 2, h + s * 2, c, r);
  }
}
