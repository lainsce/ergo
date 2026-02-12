// Cogito shared library (C API + internal engine).
#include "ergo_compat.h"

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include "cogito.h"

#if defined(__APPLE__)
#include <CoreGraphics/CoreGraphics.h>
#endif

static const char* cogito_font_path_active = NULL;
static const char* cogito_font_bold_path_active = NULL;

#define ergo_obj_new cogito_compat_obj_new
#define ergo_retain_val cogito_compat_retain_val
#define ergo_release_val cogito_compat_release_val
#define ergo_trap cogito_compat_trap
#define ergo_as_int cogito_compat_as_int
#define ergo_as_float cogito_compat_as_float
#define ergo_as_bool cogito_compat_as_bool
#define ergo_call cogito_compat_call
#define stdr_str_from_slice cogito_compat_str_from_slice
#define stdr_str_lit cogito_compat_str_lit
#define stdr_to_string cogito_compat_to_string
#define ergo_arr_new cogito_compat_arr_new
#define ergo_arr_set cogito_compat_arr_set
#define ergo_arr_get cogito_compat_arr_get

// Rename internal Ergo-facing symbols to avoid conflicts with public C API.
#define cogito_app_new cogito_app_new_ergo
#define cogito_app_set_accent_color cogito_app_set_accent_color_ergo
#define cogito_app_set_app_name cogito_app_set_app_name_ergo
#define cogito_app_set_appid cogito_app_set_appid_ergo
#define cogito_appbar_add_button cogito_appbar_add_button_ergo
#define cogito_appbar_new cogito_appbar_new_ergo
#define cogito_appbar_set_controls cogito_appbar_set_controls_ergo
#define cogito_appbar_set_subtitle cogito_appbar_set_subtitle_ergo
#define cogito_appbar_set_title cogito_appbar_set_title_ergo
#define cogito_bottom_nav_get_selected cogito_bottom_nav_get_selected_ergo
#define cogito_bottom_nav_new cogito_bottom_nav_new_ergo
#define cogito_bottom_nav_on_change cogito_bottom_nav_on_change_ergo
#define cogito_bottom_nav_set_items cogito_bottom_nav_set_items_ergo
#define cogito_bottom_nav_set_selected cogito_bottom_nav_set_selected_ergo
#define cogito_build cogito_build_ergo
#define cogito_button_add_menu cogito_button_add_menu_ergo
#define cogito_button_new cogito_button_new_ergo
#define cogito_button_set_text cogito_button_set_text_ergo
#define cogito_carousel_new cogito_carousel_new_ergo
#define cogito_carousel_get_active_index cogito_carousel_get_active_index_ergo
#define cogito_carousel_set_active_index cogito_carousel_set_active_index_ergo
#define cogito_checkbox_get_checked cogito_checkbox_get_checked_ergo
#define cogito_checkbox_new cogito_checkbox_new_ergo
#define cogito_checkbox_on_change cogito_checkbox_on_change_ergo
#define cogito_checkbox_set_checked cogito_checkbox_set_checked_ergo
#define cogito_chip_get_selected cogito_chip_get_selected_ergo
#define cogito_chip_new cogito_chip_new_ergo
#define cogito_chip_on_close cogito_chip_on_close_ergo
#define cogito_chip_set_closable cogito_chip_set_closable_ergo
#define cogito_chip_set_selected cogito_chip_set_selected_ergo
#define cogito_colorpicker_new cogito_colorpicker_new_ergo
#define cogito_colorpicker_on_change cogito_colorpicker_on_change_ergo
#define cogito_datepicker_new cogito_datepicker_new_ergo
#define cogito_datepicker_on_change cogito_datepicker_on_change_ergo
#define cogito_dialog_close cogito_dialog_close_ergo
#define cogito_dialog_new cogito_dialog_new_ergo
#define cogito_dialog_remove cogito_dialog_remove_ergo
#define cogito_dialog_slot_clear cogito_dialog_slot_clear_ergo
#define cogito_dialog_slot_new cogito_dialog_slot_new_ergo
#define cogito_dialog_slot_show cogito_dialog_slot_show_ergo
#define cogito_divider_new cogito_divider_new_ergo
#define cogito_dropdown_get_selected cogito_dropdown_get_selected_ergo
#define cogito_dropdown_new cogito_dropdown_new_ergo
#define cogito_dropdown_on_change cogito_dropdown_on_change_ergo
#define cogito_dropdown_set_items cogito_dropdown_set_items_ergo
#define cogito_dropdown_set_selected cogito_dropdown_set_selected_ergo
#define cogito_fab_new cogito_fab_new_ergo
#define cogito_fab_on_click cogito_fab_on_click_ergo
#define cogito_fab_set_extended cogito_fab_set_extended_ergo
#define cogito_fixed_new cogito_fixed_new_ergo
#define cogito_fixed_set_pos cogito_fixed_set_pos_ergo
#define cogito_grid_new cogito_grid_new_ergo
#define cogito_grid_new_with_cols cogito_grid_new_ergo
#define cogito_grid_on_activate cogito_grid_on_activate_ergo
#define cogito_grid_on_select cogito_grid_on_select_ergo
#define cogito_grid_set_align cogito_grid_set_align_ergo
#define cogito_grid_set_gap cogito_grid_set_gap_ergo
#define cogito_grid_set_span cogito_grid_set_span_ergo
#define cogito_hstack_new cogito_hstack_new_ergo
#define cogito_iconbtn_add_menu cogito_iconbtn_add_menu_ergo
#define cogito_iconbtn_new cogito_iconbtn_new_ergo
#define cogito_image_new cogito_image_new_ergo
#define cogito_image_set_icon cogito_image_set_icon_ergo
#define cogito_label_new cogito_label_new_ergo
#define cogito_label_set_align cogito_label_set_align_ergo
#define cogito_label_set_class cogito_label_set_class_ergo
#define cogito_label_set_ellipsis cogito_label_set_ellipsis_ergo
#define cogito_label_set_text cogito_label_set_text_ergo
#define cogito_label_set_wrap cogito_label_set_wrap_ergo
#define cogito_list_new cogito_list_new_ergo
#define cogito_list_on_activate cogito_list_on_activate_ergo
#define cogito_list_on_select cogito_list_on_select_ergo
#define cogito_load_sum cogito_load_sum_ergo
#define cogito_load_sum_file cogito_load_sum_file_ergo
#define cogito_nav_rail_get_selected cogito_nav_rail_get_selected_ergo
#define cogito_nav_rail_new cogito_nav_rail_new_ergo
#define cogito_nav_rail_on_change cogito_nav_rail_on_change_ergo
#define cogito_nav_rail_set_items cogito_nav_rail_set_items_ergo
#define cogito_nav_rail_set_selected cogito_nav_rail_set_selected_ergo
#define cogito_node_get_editable cogito_node_get_editable_ergo
#define cogito_node_new cogito_node_new_ergo
#define cogito_node_parent cogito_node_parent_ergo
#define cogito_node_set_a11y_label cogito_node_set_a11y_label_ergo
#define cogito_node_set_a11y_role cogito_node_set_a11y_role_ergo
#define cogito_node_set_class cogito_node_set_class_ergo
#define cogito_node_set_disabled cogito_node_set_disabled_ergo
#define cogito_node_set_editable cogito_node_set_editable_ergo
#define cogito_node_set_id cogito_node_set_id_ergo
#define cogito_node_set_text cogito_node_set_text_ergo
#define cogito_node_set_tooltip cogito_node_set_tooltip_ergo
#define cogito_node_window cogito_node_window_internal
#define cogito_node_window_val cogito_node_window_val_ergo
#define cogito_pointer_capture cogito_pointer_capture_node
#define cogito_pointer_capture_clear cogito_pointer_capture_clear_ergo
#define cogito_pointer_capture_set cogito_pointer_capture_set_ergo
#define cogito_progress_get_value cogito_progress_get_value_ergo
#define cogito_progress_new cogito_progress_new_ergo
#define cogito_progress_set_value cogito_progress_set_value_ergo
#define cogito_run cogito_run_ergo
#define cogito_scroller_new cogito_scroller_new_ergo
#define cogito_scroller_set_axes cogito_scroller_set_axes_ergo
#define cogito_carousel_new cogito_carousel_new_ergo
#define cogito_carousel_set_active_index cogito_carousel_set_active_index_ergo
#define cogito_carousel_get_active_index cogito_carousel_get_active_index_ergo
#define cogito_searchfield_get_text cogito_searchfield_get_text_ergo
#define cogito_searchfield_new cogito_searchfield_new_ergo
#define cogito_searchfield_on_change cogito_searchfield_on_change_ergo
#define cogito_searchfield_set_text cogito_searchfield_set_text_ergo
#define cogito_segmented_new cogito_segmented_new_ergo
#define cogito_segmented_on_select cogito_segmented_on_select_ergo
#define cogito_slider_get_value cogito_slider_get_value_ergo
#define cogito_slider_new cogito_slider_new_ergo
#define cogito_slider_on_change cogito_slider_on_change_ergo
#define cogito_slider_set_value cogito_slider_set_value_ergo
#define cogito_stepper_get_value cogito_stepper_get_value_ergo
#define cogito_stepper_new cogito_stepper_new_ergo
#define cogito_stepper_on_change cogito_stepper_on_change_ergo
#define cogito_stepper_set_value cogito_stepper_set_value_ergo
#define cogito_switch_get_checked cogito_switch_get_checked_ergo
#define cogito_switch_new cogito_switch_new_ergo
#define cogito_switch_on_change cogito_switch_on_change_ergo
#define cogito_switch_set_checked cogito_switch_set_checked_ergo
#define cogito_tabs_bind cogito_tabs_bind_ergo
#define cogito_tabs_get_selected cogito_tabs_get_selected_ergo
#define cogito_tabs_new cogito_tabs_new_ergo
#define cogito_tabs_on_change cogito_tabs_on_change_ergo
#define cogito_tabs_set_ids cogito_tabs_set_ids_ergo
#define cogito_tabs_set_items cogito_tabs_set_items_ergo
#define cogito_tabs_set_selected cogito_tabs_set_selected_ergo
#define cogito_textfield_get_text cogito_textfield_get_text_ergo
#define cogito_textfield_new cogito_textfield_new_ergo
#define cogito_textfield_on_change cogito_textfield_on_change_ergo
#define cogito_textfield_set_text cogito_textfield_set_text_ergo
#define cogito_textview_get_text cogito_textview_get_text_ergo
#define cogito_textview_new cogito_textview_new_ergo
#define cogito_textview_on_change cogito_textview_on_change_ergo
#define cogito_textview_set_text cogito_textview_set_text_ergo
#define cogito_toast_new cogito_toast_new_ergo
#define cogito_toast_on_click cogito_toast_on_click_ergo
#define cogito_toast_set_action cogito_toast_set_action_ergo
#define cogito_toast_set_text cogito_toast_set_text_ergo
#define cogito_toasts_new cogito_toasts_new_ergo
#define cogito_toolbar_new cogito_bottom_toolbar_new_ergo
#define cogito_treeview_new cogito_treeview_new_ergo
#define cogito_view_switcher_new cogito_view_switcher_new_ergo
#define cogito_view_switcher_set_active cogito_view_switcher_set_active_ergo
#define cogito_vstack_new cogito_vstack_new_ergo
#define cogito_window_clear_dialog cogito_window_clear_dialog_ergo
#define cogito_window_new cogito_window_new_ergo
#define cogito_window_set_a11y_label cogito_window_set_a11y_label_ergo
#define cogito_window_set_autosize cogito_window_set_autosize_ergo
#define cogito_window_set_builder cogito_window_set_builder_ergo
#define cogito_window_set_dialog cogito_window_set_dialog_ergo
#define cogito_window_set_resizable cogito_window_set_resizable_ergo
#define cogito_zstack_new cogito_zstack_new_ergo

// Internal engine (same order as previous runtime include).
#include "../c/00_core.inc"
#include "../c/01_icons.inc"
#include "../c/02_theme.inc"
#include "../c/03_text.inc"
#include "../c/04_datepicker.inc"
#include "../c/05_cam16.inc"
#include "../c/06_colorpicker.inc"
#include "../c/07_nodes.inc"
#include "../c/08_layout.inc"
#include "../c/09_interaction.inc"
#include "../c/10_sum.inc"
#include "../c/11_sum_cogito.inc"
#include "../c/12_menu.inc"
#include "../c/13_draw.inc"
#include "../c/14_run.inc"

// Restore public names.
#undef cogito_app_new
#undef cogito_app_set_accent_color
#undef cogito_app_set_app_name
#undef cogito_app_set_appid
#undef cogito_appbar_add_button
#undef cogito_appbar_new
#undef cogito_appbar_set_controls
#undef cogito_appbar_set_subtitle
#undef cogito_appbar_set_title
#undef cogito_bottom_nav_get_selected
#undef cogito_bottom_nav_new
#undef cogito_bottom_nav_on_change
#undef cogito_bottom_nav_set_items
#undef cogito_bottom_nav_set_selected
#undef cogito_build
#undef cogito_button_add_menu
#undef cogito_button_new
#undef cogito_button_set_text
#undef cogito_carousel_new
#undef cogito_carousel_get_active_index
#undef cogito_carousel_set_active_index
#undef cogito_checkbox_get_checked
#undef cogito_checkbox_new
#undef cogito_checkbox_on_change
#undef cogito_checkbox_set_checked
#undef cogito_chip_get_selected
#undef cogito_chip_new
#undef cogito_chip_on_close
#undef cogito_chip_set_closable
#undef cogito_chip_set_selected
#undef cogito_colorpicker_new
#undef cogito_colorpicker_on_change
#undef cogito_datepicker_new
#undef cogito_datepicker_on_change
#undef cogito_dialog_close
#undef cogito_dialog_new
#undef cogito_dialog_remove
#undef cogito_dialog_slot_clear
#undef cogito_dialog_slot_new
#undef cogito_dialog_slot_show
#undef cogito_divider_new
#undef cogito_dropdown_get_selected
#undef cogito_dropdown_new
#undef cogito_dropdown_on_change
#undef cogito_dropdown_set_items
#undef cogito_dropdown_set_selected
#undef cogito_fab_new
#undef cogito_fab_on_click
#undef cogito_fab_set_extended
#undef cogito_fixed_new
#undef cogito_fixed_set_pos
#undef cogito_grid_new
#undef cogito_grid_new_with_cols
#undef cogito_grid_on_activate
#undef cogito_grid_on_select
#undef cogito_grid_set_align
#undef cogito_grid_set_gap
#undef cogito_grid_set_span
#undef cogito_hstack_new
#undef cogito_iconbtn_add_menu
#undef cogito_iconbtn_new
#undef cogito_image_new
#undef cogito_image_set_icon
#undef cogito_label_new
#undef cogito_label_set_align
#undef cogito_label_set_class
#undef cogito_label_set_ellipsis
#undef cogito_label_set_text
#undef cogito_label_set_wrap
#undef cogito_list_new
#undef cogito_list_on_activate
#undef cogito_list_on_select
#undef cogito_load_sum
#undef cogito_load_sum_file
#undef cogito_nav_rail_get_selected
#undef cogito_nav_rail_new
#undef cogito_nav_rail_on_change
#undef cogito_nav_rail_set_items
#undef cogito_nav_rail_set_selected
#undef cogito_node_get_editable
#undef cogito_node_new
#undef cogito_node_parent
#undef cogito_node_set_a11y_label
#undef cogito_node_set_a11y_role
#undef cogito_node_set_class
#undef cogito_node_set_disabled
#undef cogito_node_set_editable
#undef cogito_node_set_id
#undef cogito_node_set_text
#undef cogito_node_set_tooltip
#undef cogito_node_window
#undef cogito_node_window_val
#undef cogito_pointer_capture
#undef cogito_pointer_capture_clear
#undef cogito_pointer_capture_set
#undef cogito_progress_get_value
#undef cogito_progress_new
#undef cogito_progress_set_value
#undef cogito_run
#undef cogito_scroller_new
#undef cogito_scroller_set_axes
#undef cogito_searchfield_get_text
#undef cogito_searchfield_new
#undef cogito_searchfield_on_change
#undef cogito_searchfield_set_text
#undef cogito_segmented_new
#undef cogito_segmented_on_select
#undef cogito_slider_get_value
#undef cogito_slider_new
#undef cogito_slider_on_change
#undef cogito_slider_set_value
#undef cogito_stepper_get_value
#undef cogito_stepper_new
#undef cogito_stepper_on_change
#undef cogito_stepper_set_value
#undef cogito_switch_get_checked
#undef cogito_switch_new
#undef cogito_switch_on_change
#undef cogito_switch_set_checked
#undef cogito_tabs_bind
#undef cogito_tabs_get_selected
#undef cogito_tabs_new
#undef cogito_tabs_on_change
#undef cogito_tabs_set_ids
#undef cogito_tabs_set_items
#undef cogito_tabs_set_selected
#undef cogito_textfield_get_text
#undef cogito_textfield_new
#undef cogito_textfield_on_change
#undef cogito_textfield_set_text
#undef cogito_textview_get_text
#undef cogito_textview_new
#undef cogito_textview_on_change
#undef cogito_textview_set_text
#undef cogito_toast_new
#undef cogito_toast_on_click
#undef cogito_toast_set_action
#undef cogito_toast_set_text
#undef cogito_toasts_new
#undef cogito_toolbar_new
#undef cogito_treeview_new
#undef cogito_view_switcher_new
#undef cogito_view_switcher_set_active
#undef cogito_vstack_new
#undef cogito_window_clear_dialog
#undef cogito_window_new
#undef cogito_window_set_a11y_label
#undef cogito_window_set_autosize
#undef cogito_window_set_builder
#undef cogito_window_set_dialog
#undef cogito_window_set_resizable
#undef cogito_zstack_new

// Public C API implementations for node hierarchy (use internal functions)
cogito_node* cogito_node_get_parent(cogito_node* node) {
  return (cogito_node*)cogito_node_get_parent_internal((CogitoNode*)node);
}

size_t cogito_node_get_child_count(cogito_node* node) {
  return cogito_node_get_child_count_internal((CogitoNode*)node);
}

cogito_node* cogito_node_get_child(cogito_node* node, size_t index) {
  return (cogito_node*)cogito_node_get_child_internal((CogitoNode*)node, index);
}

static cogito_node* cogito_from_val(ErgoVal v);

typedef struct CogitoCbNode {
  cogito_node_fn fn;
  void* user;
} CogitoCbNode;

typedef struct CogitoCbIndex {
  cogito_index_fn fn;
  void* user;
  CogitoNode* node;
} CogitoCbIndex;

typedef struct CogitoHitTestAdapter {
  cogito_window* window;
  cogito_hit_test_fn fn;
  void* user;
} CogitoHitTestAdapter;

static CogitoHitTestAdapter* cogito_hit_test_adapter = NULL;

static ErgoFn* cogito_make_fn(ErgoFnImpl impl, void* env) {
  ErgoFn* fn = (ErgoFn*)malloc(sizeof(ErgoFn));
  if (!fn) return NULL;
  fn->ref = 1;
  fn->arity = -1;
  fn->env = env;
  fn->fn = impl;
  return fn;
}

static ErgoVal cogito_cb_node(void* env, int argc, ErgoVal* argv) {
  (void)argc;
  CogitoCbNode* cb = (CogitoCbNode*)env;
  if (!cb || !cb->fn) return EV_NULLV;
  CogitoNode* n = NULL;
  if (argv && argv[0].tag == EVT_OBJ) n = (CogitoNode*)argv[0].as.p;
  cb->fn((cogito_node*)n, cb->user);
  return EV_NULLV;
}

static ErgoVal cogito_cb_index(void* env, int argc, ErgoVal* argv) {
  CogitoCbIndex* cb = (CogitoCbIndex*)env;
  if (!cb || !cb->fn) return EV_NULLV;
  int idx = (argc > 0) ? (int)ergo_as_int(argv[0]) : -1;
  cb->fn((cogito_node*)cb->node, idx, cb->user);
  return EV_NULLV;
}

static int cogito_hit_test_bridge(CogitoWindow* window, int x, int y, void* user) {
  (void)window;
  CogitoHitTestAdapter* a = (CogitoHitTestAdapter*)user;
  if (!a || !a->fn) return (int)COGITO_WINDOW_HITTEST_NORMAL;
  return (int)a->fn(a->window, x, y, a->user);
}

static ErgoVal cogito_val_from_cstr(const char* s) {
  if (!s) return EV_NULLV;
  ErgoStr* es = stdr_str_from_slice(s, strlen(s));
  return EV_STR(es);
}

static void cogito_set_fn(ErgoFn** slot, ErgoFn* fn) {
  if (*slot) ergo_release_val(EV_FN(*slot));
  *slot = fn;
  if (fn) ergo_retain_val(EV_FN(fn));
}

cogito_app* cogito_app_new(void) {
  ErgoVal v = cogito_app_new_ergo();
  return (cogito_app*)v.as.p;
}

void cogito_app_free(cogito_app* app) {
  if (app) ergo_release_val(EV_OBJ(app));
}

void cogito_app_run(cogito_app* app, cogito_window* window) {
  if (!app || !window) return;
  cogito_run_ergo(EV_OBJ(app), EV_OBJ(window));
}

void cogito_app_set_appid(cogito_app* app, const char* rdnn) {
  if (!app) return;
  ErgoVal idv = cogito_val_from_cstr(rdnn);
  cogito_app_set_appid_ergo(EV_OBJ(app), idv);
  if (idv.tag == EVT_STR) ergo_release_val(idv);
}

void cogito_app_set_app_name(cogito_app* app, const char* name) {
  if (!app) return;
  ErgoVal nv = cogito_val_from_cstr(name);
  cogito_app_set_app_name_ergo(EV_OBJ(app), nv);
  if (nv.tag == EVT_STR) ergo_release_val(nv);
}

void cogito_app_set_accent_color(cogito_app* app, const char* hex, bool follow_system) {
  if (!app) return;
  ErgoVal hv = cogito_val_from_cstr(hex);
  cogito_app_set_accent_color_ergo(EV_OBJ(app), hv, EV_BOOL(follow_system));
  if (hv.tag == EVT_STR) ergo_release_val(hv);
}

bool cogito_open_url(const char* url) {
  if (!url || !url[0]) return false;
  if (!cogito_backend || !cogito_backend->open_url) return false;
  return cogito_backend->open_url(url);
}

cogito_window* cogito_window_new(const char* title, int w, int h) {
  ErgoVal tv = cogito_val_from_cstr(title);
  ErgoVal v = cogito_window_new_ergo(tv, EV_INT(w), EV_INT(h));
  if (tv.tag == EVT_STR) ergo_release_val(tv);
  return (cogito_window*)v.as.p;
}

void cogito_window_free(cogito_window* window) {
  if (window) ergo_release_val(EV_OBJ(window));
}

void cogito_window_set_resizable(cogito_window* window, bool on) {
  if (!window) return;
  cogito_window_set_resizable_ergo(EV_OBJ(window), EV_BOOL(on));
}

void cogito_window_set_autosize(cogito_window* window, bool on) {
  if (!window) return;
  cogito_window_set_autosize_ergo(EV_OBJ(window), EV_BOOL(on));
}

void cogito_window_set_a11y_label(cogito_window* window, const char* label) {
  if (!window) return;
  ErgoVal lv = cogito_val_from_cstr(label);
  cogito_node_set_a11y_label_ergo(EV_OBJ(window), lv);
  if (lv.tag == EVT_STR) ergo_release_val(lv);
}

void cogito_window_set_builder(cogito_window* window, cogito_node_fn builder, void* user) {
  if (!window || !builder) return;
  CogitoCbNode* env = (CogitoCbNode*)calloc(1, sizeof(*env));
  env->fn = builder;
  env->user = user;
  ErgoFn* wrap = cogito_make_fn(cogito_cb_node, env);
  cogito_window_set_builder_ergo(EV_OBJ(window), EV_FN(wrap));
  ergo_release_val(EV_FN(wrap));
}

void* cogito_window_get_native_handle(cogito_window* window) {
  if (!window || !cogito_backend || !cogito_backend->window_get_native_handle) return NULL;
  CogitoWindow* backend_window = cogito_backend_window_for_node((CogitoNode*)window);
  if (!backend_window) return NULL;
  return cogito_backend->window_get_native_handle(backend_window);
}

bool cogito_window_has_native_handle(cogito_window* window) {
  return cogito_window_get_native_handle(window) != NULL;
}

void cogito_window_set_hit_test(cogito_window* window, cogito_hit_test_fn callback, void* user) {
  if (!window || !cogito_backend || !cogito_backend->window_set_hit_test_callback) return;
  CogitoWindow* backend_window = cogito_backend_window_for_node((CogitoNode*)window);
  if (!backend_window) return;
  if (!callback) {
    if (cogito_hit_test_adapter) {
      free(cogito_hit_test_adapter);
      cogito_hit_test_adapter = NULL;
    }
    cogito_backend->window_set_hit_test_callback(backend_window, NULL, NULL);
    return;
  }
  if (cogito_hit_test_adapter) {
    free(cogito_hit_test_adapter);
  }
  cogito_hit_test_adapter = (CogitoHitTestAdapter*)calloc(1, sizeof(*cogito_hit_test_adapter));
  if (!cogito_hit_test_adapter) return;
  cogito_hit_test_adapter->window = window;
  cogito_hit_test_adapter->fn = callback;
  cogito_hit_test_adapter->user = user;
  cogito_backend->window_set_hit_test_callback(backend_window, cogito_hit_test_bridge, cogito_hit_test_adapter);
}

void cogito_hit_test_cleanup(void) {
  if (cogito_hit_test_adapter) {
    free(cogito_hit_test_adapter);
    cogito_hit_test_adapter = NULL;
  }
}

void cogito_window_set_debug_overlay(cogito_window* window, bool enable) {
  if (!window || !cogito_backend || !cogito_backend->set_debug_overlay) return;
  CogitoWindow* backend_window = cogito_backend_window_for_node((CogitoNode*)window);
  if (!backend_window) return;
  cogito_backend->set_debug_overlay(backend_window, enable);
}

void cogito_rebuild_active_window(void) {
  extern CogitoNode* cogito_active_window;
  if (cogito_active_window) {
    cogito_window_rebuild(cogito_active_window);
  }
}

static CogitoKind cogito_kind_from_public(cogito_node_kind kind) {
  switch (kind) {
    case COGITO_NODE_WINDOW: return COGITO_WINDOW;
    case COGITO_NODE_APPBAR: return COGITO_APPBAR;
    case COGITO_NODE_VSTACK: return COGITO_VSTACK;
    case COGITO_NODE_HSTACK: return COGITO_HSTACK;
    case COGITO_NODE_ZSTACK: return COGITO_ZSTACK;
    case COGITO_NODE_FIXED: return COGITO_FIXED;
    case COGITO_NODE_SCROLLER: return COGITO_SCROLLER;
    case COGITO_NODE_LIST: return COGITO_LIST;
    case COGITO_NODE_GRID: return COGITO_GRID;
    case COGITO_NODE_LABEL: return COGITO_LABEL;
    case COGITO_NODE_BUTTON: return COGITO_BUTTON;
    case COGITO_NODE_ICONBTN: return COGITO_ICONBTN;
    case COGITO_NODE_CHECKBOX: return COGITO_CHECKBOX;
    case COGITO_NODE_SWITCH: return COGITO_SWITCH;
    case COGITO_NODE_TEXTFIELD: return COGITO_TEXTFIELD;
    case COGITO_NODE_TEXTVIEW: return COGITO_TEXTVIEW;
    case COGITO_NODE_SEARCHFIELD: return COGITO_SEARCHFIELD;
    case COGITO_NODE_DROPDOWN: return COGITO_DROPDOWN;
    case COGITO_NODE_SLIDER: return COGITO_SLIDER;
    case COGITO_NODE_TABS: return COGITO_TABS;
    case COGITO_NODE_VIEW_SWITCHER: return COGITO_VIEWSWITCHER;
    case COGITO_NODE_PROGRESS: return COGITO_PROGRESS;
    case COGITO_NODE_DATEPICKER: return COGITO_DATEPICKER;
    case COGITO_NODE_COLORPICKER: return COGITO_COLORPICKER;
    case COGITO_NODE_STEPPER: return COGITO_STEPPER;
    case COGITO_NODE_SEGMENTED: return COGITO_SEGMENTED;
    case COGITO_NODE_TREEVIEW: return COGITO_TREEVIEW;
    case COGITO_NODE_TOASTS: return COGITO_TOASTS;
    case COGITO_NODE_TOAST: return COGITO_TOAST;
    case COGITO_NODE_BOTTOM_TOOLBAR: return COGITO_TOOLBAR;
    case COGITO_NODE_CAROUSEL: return COGITO_CAROUSEL;
    case COGITO_NODE_DIALOG: return COGITO_DIALOG;
    case COGITO_NODE_DIALOG_SLOT: return COGITO_DIALOG_SLOT;
    case COGITO_NODE_TOOLTIP: return COGITO_TOOLTIP;
    case COGITO_NODE_IMAGE: return COGITO_IMAGE;
    default: return COGITO_KIND_COUNT;
  }
}

cogito_node* cogito_node_new(cogito_node_kind kind) {
  CogitoKind k = cogito_kind_from_public(kind);
  if (k == COGITO_KIND_COUNT) return NULL;
  CogitoNode* n = cogito_node_new_ergo(k);
  return (cogito_node*)n;
}

cogito_node* cogito_grid_new_with_cols(int cols) {
  return cogito_from_val(cogito_grid_new_ergo(EV_INT(cols)));
}

static cogito_node* cogito_from_val(ErgoVal v) {
  return v.tag == EVT_OBJ ? (cogito_node*)v.as.p : NULL;
}

cogito_node* cogito_label_new(const char* text) {
  ErgoVal tv = cogito_val_from_cstr(text);
  ErgoVal v = cogito_label_new_ergo(tv);
  if (tv.tag == EVT_STR) ergo_release_val(tv);
  return cogito_from_val(v);
}

cogito_node* cogito_button_new(const char* text) {
  ErgoVal tv = cogito_val_from_cstr(text);
  ErgoVal v = cogito_button_new_ergo(tv);
  if (tv.tag == EVT_STR) ergo_release_val(tv);
  return cogito_from_val(v);
}

cogito_node* cogito_carousel_new(void) {
  ErgoVal v = cogito_carousel_new_ergo();
  return cogito_from_val(v);
}

int cogito_carousel_get_active_index(cogito_node* node) {
  if (!node) return 0;
  ErgoVal nv = EV_OBJ((void*)node);
  ErgoVal v = cogito_carousel_get_active_index_ergo(nv);
  return (int)ergo_as_int(v);
}

void cogito_carousel_set_active_index(cogito_node* node, int index) {
  if (!node) return;
  ErgoVal nv = EV_OBJ((void*)node);
  ErgoVal iv = EV_INT((int64_t)index);
  cogito_carousel_set_active_index_ergo(nv, iv);
}


cogito_node* cogito_iconbtn_new(const char* text) {
  ErgoVal tv = cogito_val_from_cstr(text);
  ErgoVal v = cogito_iconbtn_new_ergo(tv);
  if (tv.tag == EVT_STR) ergo_release_val(tv);
  return cogito_from_val(v);
}

cogito_node* cogito_checkbox_new(const char* text, const char* group) {
  ErgoVal tv = cogito_val_from_cstr(text);
  ErgoVal gv = group ? cogito_val_from_cstr(group) : EV_NULLV;
  ErgoVal v = cogito_checkbox_new_ergo(tv, gv);
  if (tv.tag == EVT_STR) ergo_release_val(tv);
  if (gv.tag == EVT_STR) ergo_release_val(gv);
  return cogito_from_val(v);
}

cogito_node* cogito_switch_new(const char* text) {
  ErgoVal tv = cogito_val_from_cstr(text);
  ErgoVal v = cogito_switch_new_ergo(tv);
  if (tv.tag == EVT_STR) ergo_release_val(tv);
  return cogito_from_val(v);
}

cogito_node* cogito_textfield_new(const char* text) {
  ErgoVal tv = cogito_val_from_cstr(text);
  ErgoVal v = cogito_textfield_new_ergo(tv);
  if (tv.tag == EVT_STR) ergo_release_val(tv);
  return cogito_from_val(v);
}

cogito_node* cogito_textview_new(const char* text) {
  ErgoVal tv = cogito_val_from_cstr(text);
  ErgoVal v = cogito_textview_new_ergo(tv);
  if (tv.tag == EVT_STR) ergo_release_val(tv);
  return cogito_from_val(v);
}

cogito_node* cogito_searchfield_new(const char* text) {
  ErgoVal tv = cogito_val_from_cstr(text);
  ErgoVal v = cogito_searchfield_new_ergo(tv);
  if (tv.tag == EVT_STR) ergo_release_val(tv);
  return cogito_from_val(v);
}

cogito_node* cogito_dropdown_new(void) { return cogito_from_val(cogito_dropdown_new_ergo()); }
cogito_node* cogito_slider_new(double min, double max, double value) {
  return cogito_from_val(cogito_slider_new_ergo(EV_FLOAT(min), EV_FLOAT(max), EV_FLOAT(value)));
}
cogito_node* cogito_tabs_new(void) { return cogito_from_val(cogito_tabs_new_ergo()); }
cogito_node* cogito_view_switcher_new(void) { return cogito_from_val(cogito_view_switcher_new_ergo()); }
cogito_node* cogito_progress_new(double value) { return cogito_from_val(cogito_progress_new_ergo(EV_FLOAT(value))); }
cogito_node* cogito_divider_new(const char* orientation, bool is_inset) {
  ErgoVal ov = orientation ? cogito_val_from_cstr(orientation) : EV_NULLV;
  ErgoVal v = cogito_divider_new_ergo(ov, EV_BOOL(is_inset));
  if (ov.tag == EVT_STR) ergo_release_val(ov);
  return cogito_from_val(v);
}
cogito_node* cogito_datepicker_new(void) { return cogito_from_val(cogito_datepicker_new_ergo()); }
cogito_node* cogito_colorpicker_new(void) { return cogito_from_val(cogito_colorpicker_new_ergo()); }
cogito_node* cogito_stepper_new(double min, double max, double value, double step) {
  return cogito_from_val(cogito_stepper_new_ergo(EV_FLOAT(min), EV_FLOAT(max), EV_FLOAT(value), EV_FLOAT(step)));
}
cogito_node* cogito_segmented_new(void) { return cogito_from_val(cogito_segmented_new_ergo()); }
cogito_node* cogito_treeview_new(void) { return cogito_from_val(cogito_treeview_new_ergo()); }
cogito_node* cogito_toasts_new(void) { return cogito_from_val(cogito_toasts_new_ergo()); }
cogito_node* cogito_toast_new(const char* text) {
  ErgoVal tv = cogito_val_from_cstr(text);
  ErgoVal v = cogito_toast_new_ergo(tv);
  if (tv.tag == EVT_STR) ergo_release_val(tv);
  return cogito_from_val(v);
}
cogito_node* cogito_bottom_toolbar_new(void) { return cogito_from_val(cogito_bottom_toolbar_new_ergo()); }
cogito_node* cogito_dialog_new(const char* title) {
  ErgoVal tv = cogito_val_from_cstr(title);
  ErgoVal v = cogito_dialog_new_ergo(tv);
  if (tv.tag == EVT_STR) ergo_release_val(tv);
  return cogito_from_val(v);
}
cogito_node* cogito_dialog_slot_new(void) { return cogito_from_val(cogito_dialog_slot_new_ergo()); }
cogito_node* cogito_appbar_new(const char* title, const char* subtitle) {
  ErgoVal tv = cogito_val_from_cstr(title);
  ErgoVal sv = cogito_val_from_cstr(subtitle);
  ErgoVal v = cogito_appbar_new_ergo(tv, sv);
  if (tv.tag == EVT_STR) ergo_release_val(tv);
  if (sv.tag == EVT_STR) ergo_release_val(sv);
  return cogito_from_val(v);
}
cogito_node* cogito_image_new(const char* icon) {
  ErgoVal iv = cogito_val_from_cstr(icon);
  ErgoVal v = cogito_image_new_ergo(iv);
  if (iv.tag == EVT_STR) ergo_release_val(iv);
  return cogito_from_val(v);
}

void cogito_node_add(cogito_node* parent, cogito_node* child) {
  if (!parent || !child) return;
  cogito_container_add(EV_OBJ(parent), EV_OBJ(child));
}

void cogito_node_remove(cogito_node* parent, cogito_node* child) {
  if (!parent || !child) return;
  cogito_container_remove_child((CogitoNode*)parent, (CogitoNode*)child);
}

void cogito_node_free(cogito_node* node) {
  if (node) ergo_release_val(EV_OBJ(node));
}

void cogito_node_set_margins(cogito_node* node, int left, int top, int right, int bottom) {
  if (!node) return;
  cogito_container_set_margins(EV_OBJ(node), EV_INT(left), EV_INT(top), EV_INT(right), EV_INT(bottom));
}

void cogito_node_set_padding(cogito_node* node, int left, int top, int right, int bottom) {
  if (!node) return;
  cogito_container_set_padding(EV_OBJ(node), EV_INT(left), EV_INT(top), EV_INT(right), EV_INT(bottom));
}

void cogito_node_set_align(cogito_node* node, int align) {
  if (!node) return;
  cogito_container_set_align(EV_OBJ(node), EV_INT(align));
}

void cogito_node_set_halign(cogito_node* node, int align) {
  if (!node) return;
  cogito_container_set_halign(EV_OBJ(node), EV_INT(align));
}

void cogito_node_set_valign(cogito_node* node, int align) {
  if (!node) return;
  cogito_container_set_valign(EV_OBJ(node), EV_INT(align));
}

void cogito_node_set_hexpand(cogito_node* node, bool expand) {
  cogito_container_set_hexpand(EV_OBJ(node), EV_BOOL(expand));
}

void cogito_node_set_vexpand(cogito_node* node, bool expand) {
  cogito_container_set_vexpand(EV_OBJ(node), EV_BOOL(expand));
}

void cogito_node_set_gap(cogito_node* node, int gap) {
  if (!node) return;
  cogito_container_set_gap(EV_OBJ(node), EV_INT(gap));
}

void cogito_node_set_id(cogito_node* node, const char* id) {
  if (!node) return;
  ErgoVal iv = cogito_val_from_cstr(id);
  cogito_node_set_id_ergo(EV_OBJ(node), iv);
  if (iv.tag == EVT_STR) ergo_release_val(iv);
}

void cogito_node_set_text(cogito_node* node, const char* text) {
  if (!node) return;
  ErgoStr* s = stdr_str_lit(text ? text : "");
  cogito_node_set_text_ergo((CogitoNode*)node, s);
  if (s) ergo_release_val(EV_STR(s));
}

const char* cogito_node_get_text(cogito_node* node) {
  if (!node) return NULL;
  CogitoNode* n = (CogitoNode*)node;
  return n->text ? n->text->data : NULL;
}

cogito_node* cogito_vstack_new(void) {
  return cogito_from_val(cogito_vstack_new_ergo());
}

cogito_node* cogito_hstack_new(void) {
  return cogito_from_val(cogito_hstack_new_ergo());
}

cogito_node* cogito_zstack_new(void) {
  return cogito_from_val(cogito_zstack_new_ergo());
}

cogito_node* cogito_fixed_new(void) {
  return cogito_from_val(cogito_fixed_new_ergo());
}

cogito_node* cogito_scroller_new(void) {
  return cogito_from_val(cogito_scroller_new_ergo());
}

cogito_node* cogito_list_new(void) {
  return cogito_from_val(cogito_list_new_ergo());
}

void cogito_label_set_class(cogito_node* label, const char* cls) {
  if (!label) return;
  ErgoVal cv = cogito_val_from_cstr(cls);
  cogito_label_set_class_ergo(EV_OBJ(label), cv);
  if (cv.tag == EVT_STR) ergo_release_val(cv);
}

void cogito_label_set_text(cogito_node* label, const char* text) {
  if (!label) return;
  ErgoVal tv = cogito_val_from_cstr(text);
  cogito_label_set_text_ergo(EV_OBJ(label), tv);
  if (tv.tag == EVT_STR) ergo_release_val(tv);
}

void cogito_load_sum(ErgoVal pathv) {
  cogito_load_sum_ergo(pathv);
}

void cogito_node_set_disabled(cogito_node* node, bool on) {
  if (!node) return;
  cogito_node_set_disabled_ergo(EV_OBJ(node), EV_BOOL(on));
}

void cogito_node_set_editable(cogito_node* node, bool on) {
  if (!node) return;
  cogito_node_set_editable_ergo(EV_OBJ(node), EV_BOOL(on));
}

bool cogito_node_get_editable(cogito_node* node) {
  if (!node) return false;
  ErgoVal v = cogito_node_get_editable_ergo(EV_OBJ(node));
  return ergo_as_bool(v);
}

void cogito_node_set_class(cogito_node* node, const char* cls) {
  if (!node) return;
  ErgoVal cv = cogito_val_from_cstr(cls);
  cogito_node_set_class_ergo(EV_OBJ(node), cv);
  if (cv.tag == EVT_STR) ergo_release_val(cv);
}

void cogito_node_set_a11y_label(cogito_node* node, const char* label) {
  if (!node) return;
  ErgoVal lv = cogito_val_from_cstr(label);
  cogito_node_set_a11y_label_ergo(EV_OBJ(node), lv);
  if (lv.tag == EVT_STR) ergo_release_val(lv);
}

void cogito_node_set_a11y_role(cogito_node* node, const char* role) {
  if (!node) return;
  ErgoVal rv = cogito_val_from_cstr(role);
  cogito_node_set_a11y_role_ergo(EV_OBJ(node), rv);
  if (rv.tag == EVT_STR) ergo_release_val(rv);
}

void cogito_node_set_tooltip(cogito_node* node, const char* text) {
  if (!node) return;
  ErgoVal tv = cogito_val_from_cstr(text);
  ErgoStr* ts = tv.tag == EVT_STR ? (ErgoStr*)tv.as.p : NULL;
  cogito_node_set_tooltip_ergo((CogitoNode*)node, ts);
  if (tv.tag == EVT_STR) ergo_release_val(tv);
}

void cogito_node_on_click(cogito_node* node, cogito_node_fn fn, void* user) {
  if (!node) return;
  CogitoNode* n = (CogitoNode*)node;
  if (!fn) { cogito_set_fn(&n->on_click, NULL); return; }
  CogitoCbNode* env = (CogitoCbNode*)calloc(1, sizeof(*env));
  env->fn = fn;
  env->user = user;
  ErgoFn* wrap = cogito_make_fn(cogito_cb_node, env);
  cogito_set_fn(&n->on_click, wrap);
}

void cogito_node_on_change(cogito_node* node, cogito_node_fn fn, void* user) {
  if (!node) return;
  CogitoNode* n = (CogitoNode*)node;
  if (!fn) { cogito_set_fn(&n->on_change, NULL); return; }
  CogitoCbNode* env = (CogitoCbNode*)calloc(1, sizeof(*env));
  env->fn = fn;
  env->user = user;
  ErgoFn* wrap = cogito_make_fn(cogito_cb_node, env);
  cogito_set_fn(&n->on_change, wrap);
}

void cogito_node_on_select(cogito_node* node, cogito_index_fn fn, void* user) {
  if (!node) return;
  CogitoNode* n = (CogitoNode*)node;
  if (!fn) { cogito_set_fn(&n->on_select, NULL); return; }
  CogitoCbIndex* env = (CogitoCbIndex*)calloc(1, sizeof(*env));
  env->fn = fn;
  env->user = user;
  env->node = n;
  ErgoFn* wrap = cogito_make_fn(cogito_cb_index, env);
  cogito_set_fn(&n->on_select, wrap);
}

void cogito_node_on_activate(cogito_node* node, cogito_index_fn fn, void* user) {
  if (!node) return;
  CogitoNode* n = (CogitoNode*)node;
  if (!fn) { cogito_set_fn(&n->on_activate, NULL); return; }
  CogitoCbIndex* env = (CogitoCbIndex*)calloc(1, sizeof(*env));
  env->fn = fn;
  env->user = user;
  env->node = n;
  ErgoFn* wrap = cogito_make_fn(cogito_cb_index, env);
  cogito_set_fn(&n->on_activate, wrap);
}

void cogito_dropdown_set_items(cogito_node* dropdown, const char** items, size_t count) {
  if (!dropdown) return;
  ErgoArr* arr = ergo_arr_new(count);
  if (arr) arr->len = count;
  for (size_t i = 0; i < count; i++) {
    ErgoVal sv = cogito_val_from_cstr(items[i]);
    ergo_arr_set(arr, i, sv);
  }
  cogito_dropdown_set_items_ergo(EV_OBJ(dropdown), EV_ARR(arr));
  ergo_release_val(EV_ARR(arr));
}

int cogito_dropdown_get_selected(cogito_node* dropdown) {
  if (!dropdown) return -1;
  ErgoVal v = cogito_dropdown_get_selected_ergo(EV_OBJ(dropdown));
  return (int)ergo_as_int(v);
}

void cogito_dropdown_set_selected(cogito_node* dropdown, int idx) {
  if (!dropdown) return;
  cogito_dropdown_set_selected_ergo(EV_OBJ(dropdown), EV_INT(idx));
}

void cogito_tabs_set_items(cogito_node* tabs, const char** items, size_t count) {
  if (!tabs) return;
  ErgoArr* arr = ergo_arr_new(count);
  if (arr) arr->len = count;
  for (size_t i = 0; i < count; i++) {
    ErgoVal sv = cogito_val_from_cstr(items[i]);
    ergo_arr_set(arr, i, sv);
  }
  cogito_tabs_set_items_ergo(EV_OBJ(tabs), EV_ARR(arr));
  ergo_release_val(EV_ARR(arr));
}

void cogito_tabs_set_ids(cogito_node* tabs, const char** ids, size_t count) {
  if (!tabs) return;
  ErgoArr* arr = ergo_arr_new(count);
  if (arr) arr->len = count;
  for (size_t i = 0; i < count; i++) {
    ErgoVal sv = cogito_val_from_cstr(ids[i]);
    ergo_arr_set(arr, i, sv);
  }
  cogito_tabs_set_ids_ergo(EV_OBJ(tabs), EV_ARR(arr));
  ergo_release_val(EV_ARR(arr));
}

int cogito_tabs_get_selected(cogito_node* tabs) {
  if (!tabs) return -1;
  ErgoVal v = cogito_tabs_get_selected_ergo(EV_OBJ(tabs));
  return (int)ergo_as_int(v);
}

void cogito_tabs_set_selected(cogito_node* tabs, int idx) {
  if (!tabs) return;
  cogito_tabs_set_selected_ergo(EV_OBJ(tabs), EV_INT(idx));
}

void cogito_tabs_bind(cogito_node* tabs, cogito_node* view_switcher) {
  if (!tabs || !view_switcher) return;
  cogito_tabs_bind_ergo(EV_OBJ(tabs), EV_OBJ(view_switcher));
}

double cogito_slider_get_value(cogito_node* slider) {
  if (!slider) return 0.0;
  ErgoVal v = cogito_slider_get_value_ergo(EV_OBJ(slider));
  return ergo_as_float(v);
}

void cogito_slider_set_value(cogito_node* slider, double value) {
  if (!slider) return;
  cogito_slider_set_value_ergo(EV_OBJ(slider), EV_FLOAT(value));
}

bool cogito_checkbox_get_checked(cogito_node* cb) {
  if (!cb) return false;
  ErgoVal v = cogito_checkbox_get_checked_ergo(EV_OBJ(cb));
  return ergo_as_bool(v);
}

void cogito_checkbox_set_checked(cogito_node* cb, bool checked) {
  if (!cb) return;
  cogito_checkbox_set_checked_ergo(EV_OBJ(cb), EV_BOOL(checked));
}

bool cogito_switch_get_checked(cogito_node* sw) {
  if (!sw) return false;
  ErgoVal v = cogito_switch_get_checked_ergo(EV_OBJ(sw));
  return ergo_as_bool(v);
}

void cogito_switch_set_checked(cogito_node* sw, bool checked) {
  if (!sw) return;
  cogito_switch_set_checked_ergo(EV_OBJ(sw), EV_BOOL(checked));
}

void cogito_textfield_set_text(cogito_node* tf, const char* text) {
  if (!tf) return;
  ErgoVal tv = cogito_val_from_cstr(text);
  cogito_textfield_set_text_ergo(EV_OBJ(tf), tv);
  if (tv.tag == EVT_STR) ergo_release_val(tv);
}

const char* cogito_textfield_get_text(cogito_node* tf) {
  if (!tf) return NULL;
  ErgoVal v = cogito_textfield_get_text_ergo(EV_OBJ(tf));
  return v.tag == EVT_STR ? ((ErgoStr*)v.as.p)->data : NULL;
}

void cogito_textview_set_text(cogito_node* tv, const char* text) {
  if (!tv) return;
  ErgoVal sv = cogito_val_from_cstr(text);
  cogito_textview_set_text_ergo(EV_OBJ(tv), sv);
  if (sv.tag == EVT_STR) ergo_release_val(sv);
}

const char* cogito_textview_get_text(cogito_node* tv) {
  if (!tv) return NULL;
  ErgoVal v = cogito_textview_get_text_ergo(EV_OBJ(tv));
  return v.tag == EVT_STR ? ((ErgoStr*)v.as.p)->data : NULL;
}

void cogito_searchfield_set_text(cogito_node* sf, const char* text) {
  if (!sf) return;
  ErgoVal sv = cogito_val_from_cstr(text);
  cogito_searchfield_set_text_ergo(EV_OBJ(sf), sv);
  if (sv.tag == EVT_STR) ergo_release_val(sv);
}

const char* cogito_searchfield_get_text(cogito_node* sf) {
  if (!sf) return NULL;
  ErgoVal v = cogito_searchfield_get_text_ergo(EV_OBJ(sf));
  return v.tag == EVT_STR ? ((ErgoStr*)v.as.p)->data : NULL;
}

void cogito_progress_set_value(cogito_node* prog, double value) {
  if (!prog) return;
  cogito_progress_set_value_ergo(EV_OBJ(prog), EV_FLOAT(value));
}

double cogito_progress_get_value(cogito_node* prog) {
  if (!prog) return 0.0;
  ErgoVal v = cogito_progress_get_value_ergo(EV_OBJ(prog));
  return ergo_as_float(v);
}

void cogito_stepper_set_value(cogito_node* stepper, double value) {
  if (!stepper) return;
  CogitoNode* n = (CogitoNode*)stepper;
  if (value < n->stepper_min) value = n->stepper_min;
  if (value > n->stepper_max) value = n->stepper_max;
  n->stepper_value = value;
  cogito_invoke_change(n);
}

double cogito_stepper_get_value(cogito_node* stepper) {
  ErgoVal v = cogito_stepper_get_value_ergo(EV_OBJ(stepper));
  return ergo_as_float(v);
}

void cogito_stepper_on_change(cogito_node* stepper, cogito_node_fn fn, void* user) {
  if (!stepper) return;
  if (!fn) { cogito_stepper_on_change_ergo(EV_OBJ(stepper), EV_NULLV); return; }
  CogitoCbNode* env = (CogitoCbNode*)calloc(1, sizeof(*env));
  env->fn = fn;
  env->user = user;
  ErgoFn* wrap = cogito_make_fn(cogito_cb_node, env);
  cogito_stepper_on_change_ergo(EV_OBJ(stepper), EV_FN(wrap));
  ergo_release_val(EV_FN(wrap));
}

void cogito_segmented_on_select(cogito_node* seg, cogito_node_fn fn, void* user) {
  if (!seg) return;
  if (!fn) { cogito_segmented_on_select_ergo(EV_OBJ(seg), EV_NULLV); return; }
  CogitoCbNode* env = (CogitoCbNode*)calloc(1, sizeof(*env));
  env->fn = fn;
  env->user = user;
  ErgoFn* wrap = cogito_make_fn(cogito_cb_node, env);
  cogito_segmented_on_select_ergo(EV_OBJ(seg), EV_FN(wrap));
  ergo_release_val(EV_FN(wrap));
}

void cogito_load_sum_file(const char* path) {
  if (!path) return;
  cogito_load_sum_file_ergo(path);
}

void cogito_load_sum_inline(const char* src) {
  if (!src) return;
  cogito_load_sum_source(src);
}

bool cogito_debug_style(void) {
  return cogito_debug_style_enabled_internal();
}

void cogito_style_dump(cogito_node* node) {
  if (!node) return;
  cogito_style_dump_internal((CogitoNode*)node);
}

void cogito_style_dump_tree(cogito_node* root, int depth) {
  if (!root) return;
  if (depth < 0) depth = 0;
  cogito_style_dump_tree_internal((CogitoNode*)root, depth);
}

void cogito_style_dump_button_demo(void) {
  cogito_node* demo = cogito_button_new("Style Debug");
  if (!demo) return;
  cogito_style_dump(demo);
  cogito_node_free(demo);
}

void cogito_node_build(cogito_node* node, cogito_node_fn builder, void* user) {
  if (!node || !builder) return;
  CogitoCbNode* env = (CogitoCbNode*)calloc(1, sizeof(*env));
  env->fn = builder;
  env->user = user;
  ErgoFn* wrap = cogito_make_fn(cogito_cb_node, env);
  cogito_build_ergo(EV_OBJ(node), EV_FN(wrap));
}

void cogito_pointer_capture(cogito_node* node) {
  if (!node) { cogito_pointer_capture_set_ergo(EV_NULLV); return; }
  cogito_pointer_capture_set_ergo(EV_OBJ(node));
}

void cogito_pointer_release(void) {
  cogito_pointer_capture_clear_ergo();
}

void cogito_label_set_wrap(cogito_node* label, bool on) {
  if (!label) return;
  cogito_label_set_wrap_ergo(EV_OBJ(label), EV_BOOL(on));
}

void cogito_label_set_ellipsis(cogito_node* label, bool on) {
  if (!label) return;
  cogito_label_set_ellipsis_ergo(EV_OBJ(label), EV_BOOL(on));
}

void cogito_label_set_align(cogito_node* label, int align) {
  if (!label) return;
  cogito_label_set_align_ergo(EV_OBJ(label), EV_INT(align));
}

void cogito_image_set_icon(cogito_node* image, const char* icon) {
  if (!image) return;
  ErgoVal iv = cogito_val_from_cstr(icon);
  cogito_image_set_icon_ergo(EV_OBJ(image), iv);
  if (iv.tag == EVT_STR) ergo_release_val(iv);
}

cogito_node* cogito_appbar_add_button(cogito_node* appbar, const char* icon, cogito_node_fn fn, void* user) {
  if (!appbar) return NULL;
  ErgoVal iv = cogito_val_from_cstr(icon);
  ErgoVal handler = EV_NULLV;
  if (fn) {
    CogitoCbNode* env = (CogitoCbNode*)calloc(1, sizeof(*env));
    env->fn = fn;
    env->user = user;
    ErgoFn* wrap = cogito_make_fn(cogito_cb_node, env);
    handler = EV_FN(wrap);
  }
  ErgoVal v = cogito_appbar_add_button_ergo(EV_OBJ(appbar), iv, handler);
  if (iv.tag == EVT_STR) ergo_release_val(iv);
  if (handler.tag == EVT_FN) ergo_release_val(handler);
  return cogito_from_val(v);
}

void cogito_appbar_set_controls(cogito_node* appbar, const char* layout) {
  if (!appbar) return;
  ErgoVal lv = cogito_val_from_cstr(layout);
  cogito_appbar_set_controls_ergo(EV_OBJ(appbar), lv);
  if (lv.tag == EVT_STR) ergo_release_val(lv);
}

void cogito_appbar_set_title(cogito_node* appbar, const char* title) {
  if (!appbar) return;
  ErgoVal tv = cogito_val_from_cstr(title);
  cogito_appbar_set_title_ergo(EV_OBJ(appbar), tv);
  if (tv.tag == EVT_STR) ergo_release_val(tv);
}

void cogito_appbar_set_subtitle(cogito_node* appbar, const char* subtitle) {
  if (!appbar) return;
  ErgoVal sv = cogito_val_from_cstr(subtitle);
  cogito_appbar_set_subtitle_ergo(EV_OBJ(appbar), sv);
  if (sv.tag == EVT_STR) ergo_release_val(sv);
}

void cogito_dialog_slot_show(cogito_node* slot, cogito_node* dialog) {
  if (!slot || !dialog) return;
  cogito_dialog_slot_show_ergo(EV_OBJ(slot), EV_OBJ(dialog));
}

void cogito_dialog_slot_clear(cogito_node* slot) {
  if (!slot) return;
  cogito_dialog_slot_clear_ergo(EV_OBJ(slot));
}

void cogito_window_set_dialog(cogito_window* window, cogito_node* dialog) {
  if (!window || !dialog) return;
  cogito_window_set_dialog_ergo(EV_OBJ(window), EV_OBJ(dialog));
}

void cogito_window_clear_dialog(cogito_window* window) {
  if (!window) return;
  cogito_window_clear_dialog_ergo(EV_OBJ(window));
}

void cogito_fixed_set_pos(cogito_node* fixed, cogito_node* child, int x, int y) {
  if (!fixed || !child) return;
  cogito_fixed_set_pos_ergo(EV_OBJ(fixed), EV_OBJ(child), EV_INT(x), EV_INT(y));
}

void cogito_scroller_set_axes(cogito_node* scroller, bool h, bool v) {
  if (!scroller) return;
  cogito_scroller_set_axes_ergo(EV_OBJ(scroller), EV_BOOL(h), EV_BOOL(v));
}

void cogito_grid_set_gap(cogito_node* grid, int x, int y) {
  if (!grid) return;
  cogito_grid_set_gap_ergo(EV_OBJ(grid), EV_INT(x), EV_INT(y));
}

void cogito_grid_set_span(cogito_node* child, int col_span, int row_span) {
  if (!child) return;
  cogito_grid_set_span_ergo(EV_OBJ(child), EV_INT(col_span), EV_INT(row_span));
}

void cogito_grid_set_align(cogito_node* child, int halign, int valign) {
  if (!child) return;
  cogito_grid_set_align_ergo(EV_OBJ(child), EV_INT(halign), EV_INT(valign));
}

void cogito_button_set_text(cogito_node* button, const char* text) {
  if (!button) return;
  ErgoVal tv = cogito_val_from_cstr(text);
  cogito_button_set_text_ergo(EV_OBJ(button), tv);
  if (tv.tag == EVT_STR) ergo_release_val(tv);
}

void cogito_button_add_menu(cogito_node* button, const char* label, cogito_node_fn fn, void* user) {
  if (!button) return;
  ErgoVal lv = cogito_val_from_cstr(label);
  ErgoVal handler = EV_NULLV;
  if (fn) {
    CogitoCbNode* env = (CogitoCbNode*)calloc(1, sizeof(*env));
    env->fn = fn;
    env->user = user;
    ErgoFn* wrap = cogito_make_fn(cogito_cb_node, env);
    handler = EV_FN(wrap);
  }
  cogito_button_add_menu_ergo(EV_OBJ(button), lv, handler);
  if (lv.tag == EVT_STR) ergo_release_val(lv);
  if (handler.tag == EVT_FN) ergo_release_val(handler);
}

void cogito_iconbtn_add_menu(cogito_node* button, const char* label, cogito_node_fn fn, void* user) {
  if (!button) return;
  ErgoVal lv = cogito_val_from_cstr(label);
  ErgoVal handler = EV_NULLV;
  if (fn) {
    CogitoCbNode* env = (CogitoCbNode*)calloc(1, sizeof(*env));
    env->fn = fn;
    env->user = user;
    ErgoFn* wrap = cogito_make_fn(cogito_cb_node, env);
    handler = EV_FN(wrap);
  }
  cogito_iconbtn_add_menu_ergo(EV_OBJ(button), lv, handler);
  if (lv.tag == EVT_STR) ergo_release_val(lv);
  if (handler.tag == EVT_FN) ergo_release_val(handler);
}

void cogito_checkbox_on_change(cogito_node* cb, cogito_node_fn fn, void* user) {
  if (!cb) return;
  if (!fn) { cogito_checkbox_on_change_ergo(EV_OBJ(cb), EV_NULLV); return; }
  CogitoCbNode* env = (CogitoCbNode*)calloc(1, sizeof(*env));
  env->fn = fn;
  env->user = user;
  ErgoFn* wrap = cogito_make_fn(cogito_cb_node, env);
  cogito_checkbox_on_change_ergo(EV_OBJ(cb), EV_FN(wrap));
  ergo_release_val(EV_FN(wrap));
}

void cogito_switch_on_change(cogito_node* sw, cogito_node_fn fn, void* user) {
  if (!sw) return;
  if (!fn) { cogito_switch_on_change_ergo(EV_OBJ(sw), EV_NULLV); return; }
  CogitoCbNode* env = (CogitoCbNode*)calloc(1, sizeof(*env));
  env->fn = fn;
  env->user = user;
  ErgoFn* wrap = cogito_make_fn(cogito_cb_node, env);
  cogito_switch_on_change_ergo(EV_OBJ(sw), EV_FN(wrap));
  ergo_release_val(EV_FN(wrap));
}

void cogito_textfield_on_change(cogito_node* tf, cogito_node_fn fn, void* user) {
  if (!tf) return;
  if (!fn) { cogito_textfield_on_change_ergo(EV_OBJ(tf), EV_NULLV); return; }
  CogitoCbNode* env = (CogitoCbNode*)calloc(1, sizeof(*env));
  env->fn = fn;
  env->user = user;
  ErgoFn* wrap = cogito_make_fn(cogito_cb_node, env);
  cogito_textfield_on_change_ergo(EV_OBJ(tf), EV_FN(wrap));
  ergo_release_val(EV_FN(wrap));
}

void cogito_textview_on_change(cogito_node* tv, cogito_node_fn fn, void* user) {
  if (!tv) return;
  if (!fn) { cogito_textview_on_change_ergo(EV_OBJ(tv), EV_NULLV); return; }
  CogitoCbNode* env = (CogitoCbNode*)calloc(1, sizeof(*env));
  env->fn = fn;
  env->user = user;
  ErgoFn* wrap = cogito_make_fn(cogito_cb_node, env);
  cogito_textview_on_change_ergo(EV_OBJ(tv), EV_FN(wrap));
  ergo_release_val(EV_FN(wrap));
}

void cogito_searchfield_on_change(cogito_node* sf, cogito_node_fn fn, void* user) {
  if (!sf) return;
  if (!fn) { cogito_searchfield_on_change_ergo(EV_OBJ(sf), EV_NULLV); return; }
  CogitoCbNode* env = (CogitoCbNode*)calloc(1, sizeof(*env));
  env->fn = fn;
  env->user = user;
  ErgoFn* wrap = cogito_make_fn(cogito_cb_node, env);
  cogito_searchfield_on_change_ergo(EV_OBJ(sf), EV_FN(wrap));
  ergo_release_val(EV_FN(wrap));
}

void cogito_dropdown_on_change(cogito_node* dropdown, cogito_node_fn fn, void* user) {
  if (!dropdown) return;
  if (!fn) { cogito_dropdown_on_change_ergo(EV_OBJ(dropdown), EV_NULLV); return; }
  CogitoCbNode* env = (CogitoCbNode*)calloc(1, sizeof(*env));
  env->fn = fn;
  env->user = user;
  ErgoFn* wrap = cogito_make_fn(cogito_cb_node, env);
  cogito_dropdown_on_change_ergo(EV_OBJ(dropdown), EV_FN(wrap));
  ergo_release_val(EV_FN(wrap));
}

void cogito_slider_on_change(cogito_node* slider, cogito_node_fn fn, void* user) {
  if (!slider) return;
  if (!fn) { cogito_slider_on_change_ergo(EV_OBJ(slider), EV_NULLV); return; }
  CogitoCbNode* env = (CogitoCbNode*)calloc(1, sizeof(*env));
  env->fn = fn;
  env->user = user;
  ErgoFn* wrap = cogito_make_fn(cogito_cb_node, env);
  cogito_slider_on_change_ergo(EV_OBJ(slider), EV_FN(wrap));
  ergo_release_val(EV_FN(wrap));
}

void cogito_tabs_on_change(cogito_node* tabs, cogito_node_fn fn, void* user) {
  if (!tabs) return;
  if (!fn) { cogito_tabs_on_change_ergo(EV_OBJ(tabs), EV_NULLV); return; }
  CogitoCbNode* env = (CogitoCbNode*)calloc(1, sizeof(*env));
  env->fn = fn;
  env->user = user;
  ErgoFn* wrap = cogito_make_fn(cogito_cb_node, env);
  cogito_tabs_on_change_ergo(EV_OBJ(tabs), EV_FN(wrap));
  ergo_release_val(EV_FN(wrap));
}

void cogito_datepicker_on_change(cogito_node* datepicker, cogito_node_fn fn, void* user) {
  if (!datepicker) return;
  if (!fn) { cogito_datepicker_on_change_ergo(EV_OBJ(datepicker), EV_NULLV); return; }
  CogitoCbNode* env = (CogitoCbNode*)calloc(1, sizeof(*env));
  env->fn = fn;
  env->user = user;
  ErgoFn* wrap = cogito_make_fn(cogito_cb_node, env);
  cogito_datepicker_on_change_ergo(EV_OBJ(datepicker), EV_FN(wrap));
  ergo_release_val(EV_FN(wrap));
}

void cogito_colorpicker_on_change(cogito_node* colorpicker, cogito_node_fn fn, void* user) {
  if (!colorpicker) return;
  if (!fn) { cogito_colorpicker_on_change_ergo(EV_OBJ(colorpicker), EV_NULLV); return; }
  CogitoCbNode* env = (CogitoCbNode*)calloc(1, sizeof(*env));
  env->fn = fn;
  env->user = user;
  ErgoFn* wrap = cogito_make_fn(cogito_cb_node, env);
  cogito_colorpicker_on_change_ergo(EV_OBJ(colorpicker), EV_FN(wrap));
  ergo_release_val(EV_FN(wrap));
}

void cogito_list_on_select(cogito_node* list, cogito_index_fn fn, void* user) {
  if (!list) return;
  if (!fn) { cogito_list_on_select_ergo(EV_OBJ(list), EV_NULLV); return; }
  CogitoCbIndex* env = (CogitoCbIndex*)calloc(1, sizeof(*env));
  env->fn = fn;
  env->user = user;
  env->node = (CogitoNode*)list;
  ErgoFn* wrap = cogito_make_fn(cogito_cb_index, env);
  cogito_list_on_select_ergo(EV_OBJ(list), EV_FN(wrap));
  ergo_release_val(EV_FN(wrap));
}

void cogito_list_on_activate(cogito_node* list, cogito_index_fn fn, void* user) {
  if (!list) return;
  if (!fn) { cogito_list_on_activate_ergo(EV_OBJ(list), EV_NULLV); return; }
  CogitoCbIndex* env = (CogitoCbIndex*)calloc(1, sizeof(*env));
  env->fn = fn;
  env->user = user;
  env->node = (CogitoNode*)list;
  ErgoFn* wrap = cogito_make_fn(cogito_cb_index, env);
  cogito_list_on_activate_ergo(EV_OBJ(list), EV_FN(wrap));
  ergo_release_val(EV_FN(wrap));
}

void cogito_grid_on_select(cogito_node* grid, cogito_index_fn fn, void* user) {
  if (!grid) return;
  if (!fn) { cogito_grid_on_select_ergo(EV_OBJ(grid), EV_NULLV); return; }
  CogitoCbIndex* env = (CogitoCbIndex*)calloc(1, sizeof(*env));
  env->fn = fn;
  env->user = user;
  env->node = (CogitoNode*)grid;
  ErgoFn* wrap = cogito_make_fn(cogito_cb_index, env);
  cogito_grid_on_select_ergo(EV_OBJ(grid), EV_FN(wrap));
  ergo_release_val(EV_FN(wrap));
}

void cogito_grid_on_activate(cogito_node* grid, cogito_index_fn fn, void* user) {
  if (!grid) return;
  if (!fn) { cogito_grid_on_activate_ergo(EV_OBJ(grid), EV_NULLV); return; }
  CogitoCbIndex* env = (CogitoCbIndex*)calloc(1, sizeof(*env));
  env->fn = fn;
  env->user = user;
  env->node = (CogitoNode*)grid;
  ErgoFn* wrap = cogito_make_fn(cogito_cb_index, env);
  cogito_grid_on_activate_ergo(EV_OBJ(grid), EV_FN(wrap));
  ergo_release_val(EV_FN(wrap));
}

void cogito_nav_rail_on_change(cogito_node* rail, cogito_index_fn fn, void* user) {
  if (!rail) return;
  if (!fn) { cogito_nav_rail_on_change_ergo(EV_OBJ(rail), EV_NULLV); return; }
  CogitoCbIndex* env = (CogitoCbIndex*)calloc(1, sizeof(*env));
  env->fn = fn;
  env->user = user;
  env->node = (CogitoNode*)rail;
  ErgoFn* wrap = cogito_make_fn(cogito_cb_index, env);
  cogito_nav_rail_on_change_ergo(EV_OBJ(rail), EV_FN(wrap));
  ergo_release_val(EV_FN(wrap));
}

void cogito_bottom_nav_on_change(cogito_node* nav, cogito_index_fn fn, void* user) {
  if (!nav) return;
  if (!fn) { cogito_bottom_nav_on_change_ergo(EV_OBJ(nav), EV_NULLV); return; }
  CogitoCbIndex* env = (CogitoCbIndex*)calloc(1, sizeof(*env));
  env->fn = fn;
  env->user = user;
  env->node = (CogitoNode*)nav;
  ErgoFn* wrap = cogito_make_fn(cogito_cb_index, env);
  cogito_bottom_nav_on_change_ergo(EV_OBJ(nav), EV_FN(wrap));
  ergo_release_val(EV_FN(wrap));
}

cogito_node* cogito_chip_new(const char* text) {
  ErgoVal tv = cogito_val_from_cstr(text);
  ErgoVal v = cogito_chip_new_ergo(tv);
  if (tv.tag == EVT_STR) ergo_release_val(tv);
  return cogito_from_val(v);
}

cogito_node* cogito_fab_new(const char* icon) {
  ErgoVal iv = cogito_val_from_cstr(icon);
  ErgoVal v = cogito_fab_new_ergo(iv);
  if (iv.tag == EVT_STR) ergo_release_val(iv);
  return cogito_from_val(v);
}

void cogito_chip_set_selected(cogito_node* chip, bool selected) {
  if (!chip) return;
  cogito_chip_set_selected_ergo(EV_OBJ(chip), EV_BOOL(selected));
}

bool cogito_chip_get_selected(cogito_node* chip) {
  if (!chip) return false;
  ErgoVal v = cogito_chip_get_selected_ergo(EV_OBJ(chip));
  return ergo_as_bool(v);
}

void cogito_chip_set_closable(cogito_node* chip, bool closable) {
  if (!chip) return;
  cogito_chip_set_closable_ergo(EV_OBJ(chip), EV_BOOL(closable));
}

void cogito_chip_on_click(cogito_node* chip, cogito_node_fn fn, void* user) {
  if (!chip) return;
  CogitoNode* n = (CogitoNode*)chip;
  if (!fn) { cogito_set_fn(&n->on_click, NULL); return; }
  CogitoCbNode* env = (CogitoCbNode*)calloc(1, sizeof(*env));
  env->fn = fn;
  env->user = user;
  ErgoFn* wrap = cogito_make_fn(cogito_cb_node, env);
  cogito_set_fn(&n->on_click, wrap);
}

void cogito_chip_on_close(cogito_node* chip, cogito_node_fn fn, void* user) {
  if (!chip) return;
  if (!fn) { cogito_chip_on_close_ergo(EV_OBJ(chip), EV_NULLV); return; }
  CogitoCbNode* env = (CogitoCbNode*)calloc(1, sizeof(*env));
  env->fn = fn;
  env->user = user;
  ErgoFn* wrap = cogito_make_fn(cogito_cb_node, env);
  cogito_chip_on_close_ergo(EV_OBJ(chip), EV_FN(wrap));
  ergo_release_val(EV_FN(wrap));
}

void cogito_fab_set_extended(cogito_node* fab, bool extended, const char* label) {
  if (!fab) return;
  ErgoVal lv = cogito_val_from_cstr(label);
  cogito_fab_set_extended_ergo(EV_OBJ(fab), EV_BOOL(extended), lv);
  if (lv.tag == EVT_STR) ergo_release_val(lv);
}

void cogito_fab_on_click(cogito_node* fab, cogito_node_fn fn, void* user) {
  if (!fab) return;
  CogitoNode* n = (CogitoNode*)fab;
  if (!fn) { cogito_set_fn(&n->on_click, NULL); return; }
  CogitoCbNode* env = (CogitoCbNode*)calloc(1, sizeof(*env));
  env->fn = fn;
  env->user = user;
  ErgoFn* wrap = cogito_make_fn(cogito_cb_node, env);
  cogito_set_fn(&n->on_click, wrap);
}

cogito_node* cogito_nav_rail_new(void) {
  ErgoVal v = cogito_nav_rail_new_ergo();
  return cogito_from_val(v);
}

cogito_node* cogito_bottom_nav_new(void) {
  ErgoVal v = cogito_bottom_nav_new_ergo();
  return cogito_from_val(v);
}

void cogito_view_switcher_set_active(cogito_node* view_switcher, const char* id) {
  if (!view_switcher) return;
  ErgoVal iv = cogito_val_from_cstr(id);
  cogito_view_switcher_set_active_ergo(EV_OBJ(view_switcher), iv);
  if (iv.tag == EVT_STR) ergo_release_val(iv);
}

void cogito_toast_set_text(cogito_node* toast, const char* text) {
  if (!toast) return;
  ErgoVal tv = cogito_val_from_cstr(text);
  cogito_toast_set_text_ergo(EV_OBJ(toast), tv);
  if (tv.tag == EVT_STR) ergo_release_val(tv);
}

void cogito_toast_on_click(cogito_node* toast, cogito_node_fn fn, void* user) {
  if (!toast) return;
  if (!fn) { cogito_toast_on_click_ergo(EV_OBJ(toast), EV_NULLV); return; }
  CogitoCbNode* env = (CogitoCbNode*)calloc(1, sizeof(*env));
  env->fn = fn;
  env->user = user;
  ErgoFn* wrap = cogito_make_fn(cogito_cb_node, env);
  cogito_toast_on_click_ergo(EV_OBJ(toast), EV_FN(wrap));
  ergo_release_val(EV_FN(wrap));
}

cogito_window* cogito_node_window(cogito_node* node) {
  if (!node) return NULL;
  ErgoVal v = cogito_node_window_val_ergo(EV_OBJ(node));
  return v.tag == EVT_OBJ ? (cogito_window*)v.as.p : NULL;
}

void cogito_toast_set_action(cogito_node* toast, const char* action_text, cogito_node_fn fn, void* user) {
  if (!toast) return;
  ErgoVal tv = cogito_val_from_cstr(action_text);
  ErgoVal handler = EV_NULLV;
  if (fn) {
    CogitoCbNode* env = (CogitoCbNode*)calloc(1, sizeof(*env));
    env->fn = fn;
    env->user = user;
    ErgoFn* wrap = cogito_make_fn(cogito_cb_node, env);
    handler = EV_FN(wrap);
  }
  cogito_toast_set_action_ergo(EV_OBJ(toast), tv, handler);
  if (tv.tag == EVT_STR) ergo_release_val(tv);
  if (handler.tag == EVT_FN) ergo_release_val(handler);
}

void cogito_nav_rail_set_items(cogito_node* rail, const char** labels, const char** icons, size_t count) {
  if (!rail) return;
  ErgoArr* labels_arr = ergo_arr_new(count);
  ErgoArr* icons_arr = ergo_arr_new(count);
  if (labels_arr) labels_arr->len = count;
  if (icons_arr) icons_arr->len = count;
  for (size_t i = 0; i < count; i++) {
    ErgoVal lv = cogito_val_from_cstr(labels[i]);
    ergo_arr_set(labels_arr, i, lv);
    if (icons && icons[i]) {
      ErgoVal iv = cogito_val_from_cstr(icons[i]);
      ergo_arr_set(icons_arr, i, iv);
    }
  }
  cogito_nav_rail_set_items_ergo(EV_OBJ(rail), EV_ARR(labels_arr), EV_ARR(icons_arr));
  ergo_release_val(EV_ARR(labels_arr));
  ergo_release_val(EV_ARR(icons_arr));
}

int cogito_nav_rail_get_selected(cogito_node* rail) {
  if (!rail) return -1;
  ErgoVal v = cogito_nav_rail_get_selected_ergo(EV_OBJ(rail));
  return (int)ergo_as_int(v);
}

void cogito_nav_rail_set_selected(cogito_node* rail, int idx) {
  if (!rail) return;
  cogito_nav_rail_set_selected_ergo(EV_OBJ(rail), EV_INT(idx));
}

void cogito_bottom_nav_set_items(cogito_node* nav, const char** labels, const char** icons, size_t count) {
  if (!nav) return;
  ErgoArr* labels_arr = ergo_arr_new(count);
  ErgoArr* icons_arr = ergo_arr_new(count);
  if (labels_arr) labels_arr->len = count;
  if (icons_arr) icons_arr->len = count;
  for (size_t i = 0; i < count; i++) {
    ErgoVal lv = cogito_val_from_cstr(labels[i]);
    ergo_arr_set(labels_arr, i, lv);
    if (icons && icons[i]) {
      ErgoVal iv = cogito_val_from_cstr(icons[i]);
      ergo_arr_set(icons_arr, i, iv);
    }
  }
  cogito_bottom_nav_set_items_ergo(EV_OBJ(nav), EV_ARR(labels_arr), EV_ARR(icons_arr));
  ergo_release_val(EV_ARR(labels_arr));
  ergo_release_val(EV_ARR(icons_arr));
}

int cogito_bottom_nav_get_selected(cogito_node* nav) {
  if (!nav) return -1;
  ErgoVal v = cogito_bottom_nav_get_selected_ergo(EV_OBJ(nav));
  return (int)ergo_as_int(v);
}

void cogito_bottom_nav_set_selected(cogito_node* nav, int idx) {
  if (!nav) return;
  cogito_bottom_nav_set_selected_ergo(EV_OBJ(nav), EV_INT(idx));
}

void cogito_dialog_close(cogito_node* dialog) {
  if (!dialog) return;
  cogito_dialog_close_ergo(EV_OBJ(dialog));
}

void cogito_dialog_remove(cogito_node* dialog) {
  if (!dialog) return;
  cogito_dialog_remove_ergo(EV_OBJ(dialog));
}

cogito_node* cogito_node_parent(cogito_node* node) {
  if (!node) return NULL;
  ErgoVal v = cogito_node_parent_ergo(EV_OBJ(node));
  return v.tag == EVT_OBJ ? (cogito_node*)v.as.p : NULL;
}
