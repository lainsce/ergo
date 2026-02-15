static bool cogito_val_equal(ErgoVal a, ErgoVal b) {
  if (a.tag != b.tag) return false;
  switch (a.tag) {
    case EVT_NULL: return true;
    case EVT_INT: return a.as.i == b.as.i;
    case EVT_FLOAT: return a.as.f == b.as.f;
    case EVT_BOOL: return a.as.b == b.as.b;
    case EVT_STR:
    case EVT_ARR:
    case EVT_OBJ:
    case EVT_FN:
      return a.as.p == b.as.p;
  }
  return false;
}

static bool cogito_group_match(ErgoVal a, ErgoVal b) {
  if (a.tag != b.tag) return false;
  if (a.tag == EVT_STR) {
    ErgoStr* sa = (ErgoStr*)a.as.p;
    ErgoStr* sb = (ErgoStr*)b.as.p;
    if (!sa || !sb) return false;
    if (sa->len != sb->len) return false;
    return memcmp(sa->data, sb->data, sa->len) == 0;
  }
  return cogito_val_equal(a, b);
}

static bool cogito_str_eq(const ErgoStr* a, const ErgoStr* b) {
  if (!a || !b) return false;
  if (a->len != b->len) return false;
  return memcmp(a->data, b->data, a->len) == 0;
}

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
  if (n->image_texture && cogito_backend && cogito_backend->texture_destroy) {
    cogito_backend->texture_destroy(n->image_texture);
    n->image_texture = NULL;
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
  if (n->on_action) {
    ergo_release_val(EV_FN(n->on_action));
    n->on_action = NULL;
  }
  if (n->action_text) {
    ergo_release_val(EV_STR(n->action_text));
    n->action_text = NULL;
  }
  if (n->class_name) {
    ergo_release_val(EV_STR(n->class_name));
    n->class_name = NULL;
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
  if (n->view_builder_ids) {
    for (size_t i = 0; i < n->view_builder_len; i++) {
      if (n->view_builder_ids[i]) ergo_release_val(EV_STR(n->view_builder_ids[i]));
      if (n->view_builders[i]) ergo_release_val(EV_FN(n->view_builders[i]));
    }
    free(n->view_builder_ids);
    free(n->view_builders);
    n->view_builder_ids = NULL;
    n->view_builders = NULL;
    n->view_builder_len = 0;
    n->view_builder_cap = 0;
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
  n->on_action = NULL;
  n->builder = NULL;
  n->group = EV_NULLV;
  n->action_text = NULL;
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
  n->tree_collapsed = false;
  n->divider_vertical = false;
  n->divider_inset = false;
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
  n->popover = NULL;
  n->popover_anchor = NULL;
  n->popover_open = false;
  n->class_name = NULL;
  n->resizable = true;
  n->fixed_pos_set = false;
  n->text_wrap = false;
  n->text_ellipsis = false;
  n->text_align = 0;
  n->disabled = false;
  n->editable = true;
  n->view_id = NULL;
  n->view_active_id = NULL;
  n->view_builder_ids = NULL;
  n->view_builders = NULL;
  n->view_builder_len = 0;
  n->view_builder_cap = 0;
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
  n->anim_bg_start = 0.0;
  n->anim_text = cogito_rgba(0, 0, 0, 0);
  n->anim_text_from = n->anim_text;
  n->anim_text_target = n->anim_text;
  n->anim_text_start = 0.0;
  n->anim_border = cogito_rgba(0, 0, 0, 0);
  n->anim_border_from = n->anim_border;
  n->anim_border_target = n->anim_border;
  n->anim_border_start = 0.0;
  n->anim_selection = cogito_rgba(0, 0, 0, 0);
  n->anim_selection_from = n->anim_selection;
  n->anim_selection_target = n->anim_selection;
  n->anim_selection_start = 0.0;
  n->anim_x = 0;
  n->anim_x_from = 0;
  n->anim_x_target = 0;
  n->anim_x_start = 0.0;
  n->anim_y = 0;
  n->anim_y_from = 0;
  n->anim_y_target = 0;
  n->anim_y_start = 0.0;
  n->anim_w = 0;
  n->anim_w_from = 0;
  n->anim_w_target = 0;
  n->anim_w_start = 0.0;
  n->anim_h = 0;
  n->anim_h_from = 0;
  n->anim_h_target = 0;
  n->anim_h_start = 0.0;
  n->anim_layout_active = false;
  n->active_latch_until = 0.0;
  n->opacity = 1.0f;
  n->opacity_from = 1.0f;
  n->opacity_target = 1.0f;
  n->opacity_start = 0.0;
  n->has_opacity = false;
  n->opacity_set = false;
  n->progress_value = 0.0;
  n->stepper_hit_minus = false;
  n->stepper_hit_plus = false;
  n->margin_left = 0;
  n->margin_top = 0;
  n->margin_right = 0;
  n->margin_bottom = 0;
  n->padding_left = 0;
  n->padding_top = 0;
  n->padding_right = 0;
  n->padding_bottom = 0;
  n->align = 0;
  n->hexpand = false;
  n->vexpand = false;
  // Layout widgets default to 0 gap, content widgets default to 8 for natural spacing
  bool is_layout = (kind == COGITO_VSTACK || kind == COGITO_HSTACK || kind == COGITO_ZSTACK ||
                    kind == COGITO_FIXED || kind == COGITO_SCROLLER || kind == COGITO_LIST);
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
  n->border_color = cogito_rgba(25, 25, 25, 255);
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
  n->image_texture = NULL;
  n->image_w = 0;
  n->image_h = 0;
  n->image_loaded = false;
  n->image_avg_lum = 0.5f;
  n->carousel_dragging = false;
  n->carousel_scroll_x = 0;
  n->carousel_drag_start_x = 0;
  n->carousel_drag_start_scroll = 0;
  n->carousel_active_index = 0;
  n->carousel_item_count = 0;
  n->carousel_velocity = 0.0f;
  n->carousel_scroll_xf = 0.0;
  n->carousel_anim_start = 0.0;
  n->carousel_anim_from = 0.0;
  n->carousel_anim_to = 0.0;
  n->carousel_animating = false;
  n->carousel_drag_last_time = 0.0;
  n->carousel_drag_last_xf = 0.0;
  n->border_style = COGITO_BORDER_NONE;
  n->border_style_set = false;
  n->radius_tl = 0;
  n->radius_tr = 0;
  n->radius_br = 0;
  n->radius_bl = 0;
  n->radius_tl_set = false;
  n->radius_tr_set = false;
  n->radius_br_set = false;
  n->radius_bl_set = false;
  n->font_family[0] = 0;
  n->font_family_set = false;
  n->box_shadow = (CogitoBoxShadow){0};
  n->box_shadow_set = false;
  n->track_height = 0;
  n->track_height_set = false;
  n->button_outlined = false;
  n->button_text = false;
  n->chip_selected = false;
  n->chip_has_close = false;
  n->fab_extended = false;
  n->action_button_x = 0;
  n->action_button_y = 0;
  n->action_button_w = 0;
  n->action_button_h = 0;
  cogito_apply_style_to_node(n);
  return n;
}

static void cogito_node_set_text(CogitoNode* n, ErgoStr* s) {
  if (!n) return;
  if (s) ergo_retain_val(EV_STR(s));
  if (n->text) ergo_release_val(EV_STR(n->text));
  n->text = s;
  int len = n->text ? (int)n->text->len : 0;
  if (n->caret > len) n->caret = len;
  if (n->sel_start > len) n->sel_start = len;
  if (n->sel_end > len) n->sel_end = len;
}

static void cogito_node_set_subtitle(CogitoNode* n, ErgoStr* s) {
  if (!n) return;
  if (s) ergo_retain_val(EV_STR(s));
  if (n->subtitle) ergo_release_val(EV_STR(n->subtitle));
  n->subtitle = s;
}

static void cogito_node_set_tooltip(CogitoNode* n, ErgoStr* s) {
  if (!n) return;
  if (s) ergo_retain_val(EV_STR(s));
  if (n->tooltip) ergo_release_val(EV_STR(n->tooltip));
  n->tooltip = s;
}

static void cogito_node_set_icon(CogitoNode* n, ErgoStr* s) {
  if (!n) return;
  if (s) ergo_retain_val(EV_STR(s));
  if (n->icon) ergo_release_val(EV_STR(n->icon));
  n->icon = s;
}

static void cogito_node_set_group(CogitoNode* n, ErgoVal group) {
  if (!n) return;
  if (group.tag != EVT_NULL) ergo_retain_val(group);
  if (n->group.tag != EVT_NULL) ergo_release_val(n->group);
  n->group = group;
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

    #include "widgets/intrinsic/carousel.inc"

    #include "widgets/intrinsic/carousel_item.inc"

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

    #include "widgets/intrinsic/divider.inc"

    #include "widgets/intrinsic/card.inc"

    #include "widgets/intrinsic/avatar.inc"

    #include "widgets/intrinsic/badge.inc"

    #include "widgets/intrinsic/banner.inc"

    #include "widgets/intrinsic/bottom_sheet.inc"

    #include "widgets/intrinsic/timepicker.inc"

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
