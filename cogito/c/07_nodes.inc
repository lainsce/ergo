static void cogito_node_drop(ErgoObj* o) {
  CogitoNode* n = (CogitoNode*)o;
  if (n->text) {
    ergo_release_val(EV_STR(n->text));
    n->text = NULL;
  }
  if (n->subtitle) {
    ergo_release_val(EV_STR(n->subtitle));
    n->subtitle = NULL;
  }
  if (n->tooltip) {
    ergo_release_val(EV_STR(n->tooltip));
    n->tooltip = NULL;
  }
  if (n->icon) {
    ergo_release_val(EV_STR(n->icon));
    n->icon = NULL;
  }
  if (n->on_click) {
    ergo_release_val(EV_FN(n->on_click));
    n->on_click = NULL;
  }
  if (n->on_change) {
    ergo_release_val(EV_FN(n->on_change));
    n->on_change = NULL;
  }
  if (n->on_select) {
    ergo_release_val(EV_FN(n->on_select));
    n->on_select = NULL;
  }
  if (n->on_activate) {
    ergo_release_val(EV_FN(n->on_activate));
    n->on_activate = NULL;
  }
  if (n->builder) {
    ergo_release_val(EV_FN(n->builder));
    n->builder = NULL;
  }
  if (n->group.tag != EVT_NULL) {
    ergo_release_val(n->group);
    n->group = EV_NULLV;
  }
  if (n->menu_labels) {
    for (size_t i = 0; i < n->menu_len; i++) {
      if (n->menu_labels[i]) ergo_release_val(EV_STR(n->menu_labels[i]));
      if (n->menu_handlers[i]) ergo_release_val(EV_FN(n->menu_handlers[i]));
    }
    free(n->menu_labels);
    free(n->menu_handlers);
    n->menu_labels = NULL;
    n->menu_handlers = NULL;
    n->menu_len = 0;
    n->menu_cap = 0;
  }
  if (n->a11y_label) {
    ergo_release_val(EV_STR(n->a11y_label));
    n->a11y_label = NULL;
  }
  if (n->a11y_role) {
    ergo_release_val(EV_STR(n->a11y_role));
    n->a11y_role = NULL;
  }
  if (n->tab_labels) {
    for (size_t i = 0; i < n->tab_len; i++) {
      if (n->tab_labels[i]) ergo_release_val(EV_STR(n->tab_labels[i]));
      if (n->tab_ids && n->tab_ids[i]) ergo_release_val(EV_STR(n->tab_ids[i]));
    }
    free(n->tab_labels);
    free(n->tab_ids);
    n->tab_labels = NULL;
    n->tab_ids = NULL;
    n->tab_len = 0;
    n->tab_cap = 0;
  }
  if (n->nav_rail_labels) {
    for (size_t i = 0; i < n->nav_rail_len; i++) {
      if (n->nav_rail_labels[i]) ergo_release_val(EV_STR(n->nav_rail_labels[i]));
      if (n->nav_rail_icons && n->nav_rail_icons[i]) ergo_release_val(EV_STR(n->nav_rail_icons[i]));
    }
    free(n->nav_rail_labels);
    free(n->nav_rail_icons);
    n->nav_rail_labels = NULL;
    n->nav_rail_icons = NULL;
    n->nav_rail_len = 0;
    n->nav_rail_cap = 0;
  }
  if (n->bottom_nav_labels) {
    for (size_t i = 0; i < n->bottom_nav_len; i++) {
      if (n->bottom_nav_labels[i]) ergo_release_val(EV_STR(n->bottom_nav_labels[i]));
      if (n->bottom_nav_icons && n->bottom_nav_icons[i]) ergo_release_val(EV_STR(n->bottom_nav_icons[i]));
    }
    free(n->bottom_nav_labels);
    free(n->bottom_nav_icons);
    n->bottom_nav_labels = NULL;
    n->bottom_nav_icons = NULL;
    n->bottom_nav_len = 0;
    n->bottom_nav_cap = 0;
  }
  if (n->view_id) {
    ergo_release_val(EV_STR(n->view_id));
    n->view_id = NULL;
  }
  if (n->view_active_id) {
    ergo_release_val(EV_STR(n->view_active_id));
    n->view_active_id = NULL;
  }
  if (n->children) {
    for (size_t i = 0; i < n->len; i++) {
      ergo_release_val(EV_OBJ(n->children[i]));
    }
    free(n->children);
    n->children = NULL;
    n->len = 0;
    n->cap = 0;
  }
  if (n->dialog) {
    ergo_release_val(EV_OBJ(n->dialog));
    n->dialog = NULL;
  }
}

static CogitoNode* cogito_node_new(CogitoKind kind) {
  CogitoNode* n = (CogitoNode*)ergo_obj_new(sizeof(CogitoNode), cogito_node_drop);
  n->kind = kind;
  n->parent = NULL;
  n->children = NULL;
  n->len = 0;
  n->cap = 0;
  n->text = NULL;
  n->subtitle = NULL;
  n->icon = NULL;
  n->tooltip = NULL;
  n->on_click = NULL;
  n->on_change = NULL;
  n->on_select = NULL;
  n->on_activate = NULL;
  n->builder = NULL;
  n->group = EV_NULLV;
  n->checked = false;
  n->x = 0;
  n->y = 0;
  n->w = 0;
  n->h = 0;
  n->grid_cols = 1;
  n->grid_gap_x = 6;
  n->grid_gap_y = 6;
  n->grid_align_x = -1;
  n->grid_align_y = -1;
  n->grid_span_x = 1;
  n->grid_span_y = 1;
  n->fixed_x = 0;
  n->fixed_y = 0;
  n->scroll_x = 0;
  n->scroll_y = 0;
  n->scroll_content_w = 0;
  n->scroll_content_h = 0;
  n->scroll_horz = false;
  n->scroll_vert = true;
  n->selected = -1;
  n->should_close = false;
  n->menu_labels = NULL;
  n->menu_handlers = NULL;
  n->menu_len = 0;
  n->menu_cap = 0;
  n->tab_labels = NULL;
  n->tab_ids = NULL;
  n->tab_len = 0;
  n->tab_cap = 0;
  n->nav_rail_labels = NULL;
  n->nav_rail_icons = NULL;
  n->nav_rail_len = 0;
  n->nav_rail_cap = 0;
  n->nav_rail_selected = 0;
  n->bottom_nav_labels = NULL;
  n->bottom_nav_icons = NULL;
  n->bottom_nav_len = 0;
  n->bottom_nav_cap = 0;
  n->bottom_nav_selected = 0;
  n->appbar_btn_close_x = 0;
  n->appbar_btn_min_x = 0;
  n->appbar_btn_max_x = 0;
  n->appbar_btn_y = 0;
  n->appbar_btn_size = 0;
  n->appbar_title_y = 0;
  n->appbar_subtitle_y = 0;
  n->appbar_controls[0] = 0;
  n->dialog = NULL;
  n->view_switcher = NULL;
  n->dialog_open = false;
  n->resizable = true;
  n->fixed_pos_set = false;
  n->text_wrap = false;
  n->text_ellipsis = false;
  n->text_align = 0;
  n->disabled = false;
  n->view_id = NULL;
  n->view_active_id = NULL;
  n->slider_min = 0.0;
  n->slider_max = 1.0;
  n->slider_value = 0.0;
  n->slider_dragging = false;
  n->stepper_min = 0.0;
  n->stepper_max = 100.0;
  n->stepper_value = 0.0;
  n->stepper_step = 1.0;
  n->date_year = 0;
  n->date_month = 0;
  n->date_day = 0;
  n->color_h = 0.0;
  n->color_c = 0.0;
  n->color_t = 0.0;
  n->color_hex_editing = false;
  n->caret = 0;
  n->sel_start = 0;
  n->sel_end = 0;
  n->selecting = false;
  n->font_mono = false;
  n->font_tabular = false;
  n->letter_spacing_set = false;
  n->letter_spacing = 0.0f;
  n->a11y_label = NULL;
  n->a11y_role = NULL;
  n->anim_inited = false;
  n->anim_start = 0.0;
  n->anim_ms = 0;
  n->anim_ease = 0;
  n->anim_bg = cogito_rgba(0, 0, 0, 0);
  n->anim_bg_from = n->anim_bg;
  n->anim_bg_target = n->anim_bg;
  n->anim_text = cogito_rgba(0, 0, 0, 0);
  n->anim_text_from = n->anim_text;
  n->anim_text_target = n->anim_text;
  n->anim_border = cogito_rgba(0, 0, 0, 0);
  n->anim_border_from = n->anim_border;
  n->anim_border_target = n->anim_border;
  n->anim_selection = cogito_rgba(0, 0, 0, 0);
  n->anim_selection_from = n->anim_selection;
  n->anim_selection_target = n->anim_selection;
  n->progress_value = 0.0;
  n->margin_left = 0;
  n->margin_top = 0;
  n->margin_right = 0;
  n->margin_bottom = 0;
  n->padding_left = 0;
  n->padding_top = 0;
  n->padding_right = 0;
  n->padding_bottom = 0;
  n->align = 0;
  // Layout widgets default to 0 gap, content widgets default to 8 for natural spacing
  bool is_layout = (kind == COGITO_VSTACK || kind == COGITO_HSTACK || kind == COGITO_ZSTACK ||
                    kind == COGITO_FIXED || kind == COGITO_SCROLLER);
  n->gap = is_layout ? 0 : 8;
  n->auto_size = false;
  n->margin_set = false;
  n->padding_set = false;
  n->bg_set = false;
  n->text_color_set = false;
  n->border_color_set = false;
  n->border_width_set = false;
  n->radius_set = false;
  n->selection_set = false;
  n->track_set = false;
  n->track_on_set = false;
  n->knob_set = false;
  n->check_set = false;
  n->font_weight_set = false;
  n->letter_spacing_set = false;
  n->min_w_set = false;
  n->min_h_set = false;
  n->max_w_set = false;
  n->max_h_set = false;
  n->shadow_set = false;
  n->bg = cogito_rgba(0, 0, 0, 0);
  n->text_color = cogito_rgba(30, 30, 30, 255);
  n->border_color = cogito_rgba(0, 0, 0, 0);
  n->selection_color = cogito_rgba(208, 220, 245, 255);
  n->track_color = cogito_rgba(170, 170, 170, 255);
  n->track_on_color = cogito_rgba(90, 160, 110, 255);
  n->knob_color = cogito_rgba(250, 250, 250, 255);
  n->check_color = cogito_rgba(30, 30, 30, 255);
  n->border_width = 0;
  n->border_radius = 0;
  n->label_class = COGITO_LABEL_CLASS_NONE;
  n->font_size = 0;
  n->font_size_set = false;
  n->font_weight = 400;
  n->letter_spacing = 0.0f;
  n->min_w = 0;
  n->min_h = 0;
  n->max_w = 0;
  n->max_h = 0;
  n->shadow_level = 0;
  cogito_apply_style_to_node(n);
  return n;
}

static void cogito_node_set_text(CogitoNode* n, ErgoStr* s) {
  if (!n) return;
  if (n->text) ergo_release_val(EV_STR(n->text));
  n->text = s;
  if (s) ergo_retain_val(EV_STR(s));
  int len = n->text ? (int)n->text->len : 0;
  if (n->caret > len) n->caret = len;
  if (n->sel_start > len) n->sel_start = len;
  if (n->sel_end > len) n->sel_end = len;
}

static void cogito_node_set_subtitle(CogitoNode* n, ErgoStr* s) {
  if (!n) return;
  if (n->subtitle) ergo_release_val(EV_STR(n->subtitle));
  n->subtitle = s;
  if (s) ergo_retain_val(EV_STR(s));
}

static void cogito_node_set_tooltip(CogitoNode* n, ErgoStr* s) {
  if (!n) return;
  if (n->tooltip) ergo_release_val(EV_STR(n->tooltip));
  n->tooltip = s;
  if (s) ergo_retain_val(EV_STR(s));
}

static void cogito_node_set_icon(CogitoNode* n, ErgoStr* s) {
  if (!n) return;
  if (n->icon) ergo_release_val(EV_STR(n->icon));
  n->icon = s;
  if (s) ergo_retain_val(EV_STR(s));
}

static void cogito_node_set_group(CogitoNode* n, ErgoVal group) {
  if (!n) return;
  if (n->group.tag != EVT_NULL) ergo_release_val(n->group);
  n->group = group;
  if (group.tag != EVT_NULL) ergo_retain_val(group);
}

static void cogito_children_add(CogitoNode* parent, CogitoNode* child) {
  if (!parent || !child) return;
  if (parent->len + 1 > parent->cap) {
    size_t next = parent->cap == 0 ? 4 : parent->cap * 2;
    parent->children = (CogitoNode**)realloc(parent->children, sizeof(CogitoNode*) * next);
    parent->cap = next;
  }
  parent->children[parent->len++] = child;
  child->parent = parent;
  ergo_retain_val(EV_OBJ(child));
}

static void cogito_node_add_menu(CogitoNode* n, ErgoStr* label, ErgoFn* handler) {
  if (!n) return;
  if (n->menu_len + 1 > n->menu_cap) {
    size_t next = n->menu_cap == 0 ? 4 : n->menu_cap * 2;
    n->menu_labels = (ErgoStr**)realloc(n->menu_labels, sizeof(ErgoStr*) * next);
    n->menu_handlers = (ErgoFn**)realloc(n->menu_handlers, sizeof(ErgoFn*) * next);
    n->menu_cap = next;
  }
  n->menu_labels[n->menu_len] = label;
  n->menu_handlers[n->menu_len] = handler;
  if (label) ergo_retain_val(EV_STR(label));
  if (handler) ergo_retain_val(EV_FN(handler));
  n->menu_len++;
}

static void cogito_intrinsic_size(CogitoNode* n, int* out_w, int* out_h) {
  int text_w = 0;
  int text_h = cogito_text_height();
  if (n->text) {
    int size = cogito_node_font_size(n);
    text_w = cogito_text_width_size_node(n, n->text->data, size);
    text_h = cogito_text_height_size(size);
  }
  int w = 0;
  int h = 0;
  int pad = 6;
  switch (n->kind) {

    #include "widgets/intrinsic/button.inc"

    #include "widgets/intrinsic/iconbtn.inc"

    #include "widgets/intrinsic/image.inc"

    #include "widgets/intrinsic/label.inc"

    #include "widgets/intrinsic/checkbox.inc"

    #include "widgets/intrinsic/chip.inc"

    #include "widgets/intrinsic/switch.inc"

    #include "widgets/intrinsic/textfield.inc"

    #include "widgets/intrinsic/textview.inc"

    #include "widgets/intrinsic/searchfield.inc"

    #include "widgets/intrinsic/dropdown.inc"

    #include "widgets/intrinsic/datepicker.inc"

    #include "widgets/intrinsic/fab.inc"

    #include "widgets/intrinsic/nav_rail.inc"

    #include "widgets/intrinsic/bottom_nav.inc"

    #include "widgets/intrinsic/stepper.inc"

    #include "widgets/intrinsic/slider.inc"

    #include "widgets/intrinsic/tabs.inc"

    #include "widgets/intrinsic/segmented.inc"

    #include "widgets/intrinsic/view_switcher.inc"

    #include "widgets/intrinsic/progress.inc"

    #include "widgets/intrinsic/treeview.inc"

    #include "widgets/intrinsic/colorpicker.inc"

    #include "widgets/intrinsic/toasts.inc"

    #include "widgets/intrinsic/bottom_toolbar.inc"

    #include "widgets/intrinsic/toast.inc"

    #include "widgets/intrinsic/tooltip.inc"

    #include "widgets/intrinsic/fixed.inc"

    #include "widgets/intrinsic/scroller.inc"

    #include "widgets/intrinsic/zstack.inc"

    #include "widgets/intrinsic/list.inc"

    #include "widgets/intrinsic/grid.inc"

    #include "widgets/intrinsic/dialog.inc"

    #include "widgets/intrinsic/dialog_slot.inc"

    #include "widgets/intrinsic/popover.inc"

    #include "widgets/intrinsic/appbar.inc"

    #include "widgets/intrinsic/vstack.inc"

    #include "widgets/intrinsic/hstack.inc"

    #include "widgets/intrinsic/window.inc"

    case COGITO_KIND_COUNT:
      break;
  

}
  w += n->padding_left + n->padding_right;
  h += n->padding_top + n->padding_bottom;
  cogito_apply_size_constraints(n, &w, &h);
  if (out_w) *out_w = w;
  if (out_h) *out_h = h;
}
