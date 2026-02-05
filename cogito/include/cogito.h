// Cogito C API (opaque handles, synchronous callbacks)
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CogitoApp cogito_app;
typedef struct CogitoNode cogito_node;
typedef struct CogitoNode cogito_window;

typedef void (*cogito_node_fn)(cogito_node* node, void* user);
typedef void (*cogito_index_fn)(cogito_node* node, int index, void* user);

typedef enum {
  COGITO_NODE_WINDOW = 1,
  COGITO_NODE_APPBAR,
  COGITO_NODE_VSTACK,
  COGITO_NODE_HSTACK,
  COGITO_NODE_ZSTACK,
  COGITO_NODE_FIXED,
  COGITO_NODE_SCROLLER,
  COGITO_NODE_LIST,
  COGITO_NODE_GRID,
  COGITO_NODE_LABEL,
  COGITO_NODE_BUTTON,
  COGITO_NODE_ICONBTN,
  COGITO_NODE_CHECKBOX,
  COGITO_NODE_SWITCH,
  COGITO_NODE_TEXTFIELD,
  COGITO_NODE_TEXTVIEW,
  COGITO_NODE_SEARCHFIELD,
  COGITO_NODE_DROPDOWN,
  COGITO_NODE_SLIDER,
  COGITO_NODE_TABS,
  COGITO_NODE_VIEW_SWITCHER,
  COGITO_NODE_PROGRESS,
  COGITO_NODE_DATEPICKER,
  COGITO_NODE_COLORPICKER,
  COGITO_NODE_STEPPER,
  COGITO_NODE_SEGMENTED,
  COGITO_NODE_TREEVIEW,
  COGITO_NODE_TOASTS,
  COGITO_NODE_TOAST,
  COGITO_NODE_BOTTOM_TOOLBAR,
  COGITO_NODE_DIALOG,
  COGITO_NODE_DIALOG_SLOT,
  COGITO_NODE_TOOLTIP,
  COGITO_NODE_IMAGE
} cogito_node_kind;

// App / window lifecycle
cogito_app* cogito_app_new(void);
void cogito_app_free(cogito_app* app);
void cogito_app_run(cogito_app* app, cogito_window* window);

void cogito_app_set_appid(cogito_app* app, const char* rdnn);
void cogito_app_set_accent_color(cogito_app* app, const char* hex, bool override_system);

cogito_window* cogito_window_new(const char* title, int w, int h);
void cogito_window_free(cogito_window* window);
void cogito_window_set_resizable(cogito_window* window, bool on);
void cogito_window_set_autosize(cogito_window* window, bool on);
void cogito_window_set_a11y_label(cogito_window* window, const char* label);
void cogito_window_set_builder(cogito_window* window, cogito_node_fn builder, void* user);
void cogito_rebuild_active_window(void);

// Node creation
cogito_node* cogito_node_new(cogito_node_kind kind);
cogito_node* cogito_grid_new_with_cols(int cols);
cogito_node* cogito_label_new(const char* text);
cogito_node* cogito_button_new(const char* text);
cogito_node* cogito_iconbtn_new(const char* text);
cogito_node* cogito_checkbox_new(const char* text, const char* group);
cogito_node* cogito_switch_new(const char* text);
cogito_node* cogito_textfield_new(const char* text);
cogito_node* cogito_textview_new(const char* text);
cogito_node* cogito_searchfield_new(const char* text);
cogito_node* cogito_dropdown_new(void);
cogito_node* cogito_slider_new(double min, double max, double value);
cogito_node* cogito_tabs_new(void);
cogito_node* cogito_view_switcher_new(void);
cogito_node* cogito_progress_new(double value);
cogito_node* cogito_datepicker_new(void);
cogito_node* cogito_colorpicker_new(void);
cogito_node* cogito_stepper_new(double min, double max, double value, double step);
cogito_node* cogito_segmented_new(void);
cogito_node* cogito_treeview_new(void);
cogito_node* cogito_toasts_new(void);
cogito_node* cogito_toast_new(const char* text);
cogito_node* cogito_bottom_toolbar_new(void);
cogito_node* cogito_dialog_new(const char* title);
cogito_node* cogito_dialog_slot_new(void);
cogito_node* cogito_appbar_new(const char* title, const char* subtitle);
cogito_node* cogito_image_new(const char* icon);

// Tree / layout
void cogito_node_add(cogito_node* parent, cogito_node* child);
void cogito_node_remove(cogito_node* parent, cogito_node* child);
void cogito_node_free(cogito_node* node);

void cogito_node_set_margins(cogito_node* node, int left, int top, int right, int bottom);
void cogito_node_set_padding(cogito_node* node, int left, int top, int right, int bottom);
void cogito_node_set_align(cogito_node* node, int align);
void cogito_node_set_halign(cogito_node* node, int align);
void cogito_node_set_valign(cogito_node* node, int align);
void cogito_node_set_id(cogito_node* node, const char* id);

// Common props
void cogito_node_set_text(cogito_node* node, const char* text);
const char* cogito_node_get_text(cogito_node* node);
void cogito_node_set_disabled(cogito_node* node, bool on);
void cogito_node_set_class(cogito_node* node, const char* cls);
void cogito_node_set_a11y_label(cogito_node* node, const char* label);
void cogito_node_set_a11y_role(cogito_node* node, const char* role);
void cogito_node_set_tooltip(cogito_node* node, const char* text);
void cogito_node_build(cogito_node* node, cogito_node_fn builder, void* user);
void cogito_pointer_capture(cogito_node* node);
void cogito_pointer_release(void);

// Callbacks
void cogito_node_on_click(cogito_node* node, cogito_node_fn fn, void* user);
void cogito_node_on_change(cogito_node* node, cogito_node_fn fn, void* user);
void cogito_node_on_select(cogito_node* node, cogito_index_fn fn, void* user);
void cogito_node_on_activate(cogito_node* node, cogito_index_fn fn, void* user);

// Widget-specific helpers
void cogito_dropdown_set_items(cogito_node* dropdown, const char** items, size_t count);
int cogito_dropdown_get_selected(cogito_node* dropdown);
void cogito_dropdown_set_selected(cogito_node* dropdown, int idx);

void cogito_tabs_set_items(cogito_node* tabs, const char** items, size_t count);
void cogito_tabs_set_ids(cogito_node* tabs, const char** ids, size_t count);
int cogito_tabs_get_selected(cogito_node* tabs);
void cogito_tabs_set_selected(cogito_node* tabs, int idx);
void cogito_tabs_bind(cogito_node* tabs, cogito_node* view_switcher);

double cogito_slider_get_value(cogito_node* slider);
void cogito_slider_set_value(cogito_node* slider, double value);

bool cogito_checkbox_get_checked(cogito_node* cb);
void cogito_checkbox_set_checked(cogito_node* cb, bool checked);

bool cogito_switch_get_checked(cogito_node* sw);
void cogito_switch_set_checked(cogito_node* sw, bool checked);

void cogito_textfield_set_text(cogito_node* tf, const char* text);
const char* cogito_textfield_get_text(cogito_node* tf);
void cogito_textview_set_text(cogito_node* tv, const char* text);
const char* cogito_textview_get_text(cogito_node* tv);
void cogito_searchfield_set_text(cogito_node* sf, const char* text);
const char* cogito_searchfield_get_text(cogito_node* sf);

void cogito_progress_set_value(cogito_node* prog, double value);
double cogito_progress_get_value(cogito_node* prog);

void cogito_stepper_set_value(cogito_node* stepper, double value);
double cogito_stepper_get_value(cogito_node* stepper);

// Theming
void cogito_load_css_file(const char* path);

#ifdef __cplusplus
}
#endif
// Label helpers
void cogito_label_set_wrap(cogito_node* label, bool on);
void cogito_label_set_ellipsis(cogito_node* label, bool on);
void cogito_label_set_align(cogito_node* label, int align);

// Image helpers
void cogito_image_set_icon(cogito_node* image, const char* icon);

// Appbar helpers
cogito_node* cogito_appbar_add_button(cogito_node* appbar, const char* icon, cogito_node_fn fn, void* user);
void cogito_appbar_set_controls(cogito_node* appbar, const char* layout);

// Dialog helpers
void cogito_dialog_slot_show(cogito_node* slot, cogito_node* dialog);
void cogito_dialog_slot_clear(cogito_node* slot);
void cogito_window_set_dialog(cogito_window* window, cogito_node* dialog);
void cogito_window_clear_dialog(cogito_window* window);

// Containers and layout helpers
void cogito_fixed_set_pos(cogito_node* fixed, cogito_node* child, int x, int y);
void cogito_scroller_set_axes(cogito_node* scroller, bool h, bool v);
void cogito_grid_set_gap(cogito_node* grid, int x, int y);
void cogito_grid_set_span(cogito_node* child, int col_span, int row_span);
void cogito_grid_set_align(cogito_node* child, int halign, int valign);

// Widget event helpers
void cogito_button_set_text(cogito_node* button, const char* text);
void cogito_button_add_menu(cogito_node* button, const char* label, cogito_node_fn fn, void* user);
void cogito_iconbtn_add_menu(cogito_node* button, const char* label, cogito_node_fn fn, void* user);

void cogito_checkbox_on_change(cogito_node* cb, cogito_node_fn fn, void* user);
void cogito_switch_on_change(cogito_node* sw, cogito_node_fn fn, void* user);

void cogito_textfield_on_change(cogito_node* tf, cogito_node_fn fn, void* user);
void cogito_textview_on_change(cogito_node* tv, cogito_node_fn fn, void* user);
void cogito_searchfield_on_change(cogito_node* sf, cogito_node_fn fn, void* user);

void cogito_dropdown_on_change(cogito_node* dropdown, cogito_node_fn fn, void* user);
void cogito_slider_on_change(cogito_node* slider, cogito_node_fn fn, void* user);
void cogito_tabs_on_change(cogito_node* tabs, cogito_node_fn fn, void* user);
void cogito_datepicker_on_change(cogito_node* datepicker, cogito_node_fn fn, void* user);
void cogito_colorpicker_on_change(cogito_node* colorpicker, cogito_node_fn fn, void* user);

void cogito_list_on_select(cogito_node* list, cogito_index_fn fn, void* user);
void cogito_list_on_activate(cogito_node* list, cogito_index_fn fn, void* user);
void cogito_grid_on_select(cogito_node* grid, cogito_index_fn fn, void* user);
void cogito_grid_on_activate(cogito_node* grid, cogito_index_fn fn, void* user);

void cogito_view_switcher_set_active(cogito_node* view_switcher, const char* id);

void cogito_toast_set_text(cogito_node* toast, const char* text);
void cogito_toast_on_click(cogito_node* toast, cogito_node_fn fn, void* user);

// Node/window helpers
cogito_window* cogito_node_window(cogito_node* node);
