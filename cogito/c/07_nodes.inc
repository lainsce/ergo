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
  if (n->hint) {
    ergo_release_val(EV_STR(n->hint));
    n->hint = NULL;
  }
  if (n->tooltip) {
    ergo_release_val(EV_STR(n->tooltip));
    n->tooltip = NULL;
  }
  if (n->icon) {
    ergo_release_val(EV_STR(n->icon));
    n->icon = NULL;
  }
  if ((n->kind == COGITO_IMAGE || n->kind == COGITO_AVATAR) &&
      n->image.texture && cogito_backend && cogito_backend->texture_destroy) {
    cogito_backend->texture_destroy(n->image.texture);
    n->image.texture = NULL;
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
      if (n->tab_icons && n->tab_icons[i]) ergo_release_val(EV_STR(n->tab_icons[i]));
      if (n->tab_ids && n->tab_ids[i]) ergo_release_val(EV_STR(n->tab_ids[i]));
    }
    free(n->tab_labels);
    free(n->tab_icons);
    free(n->tab_ids);
    n->tab_labels = NULL;
    n->tab_icons = NULL;
    n->tab_ids = NULL;
    n->tab_len = 0;
    n->tab_cap = 0;
  }
  if (n->kind == COGITO_NAV_RAIL) {
    if (n->nav_rail.labels) {
      for (size_t i = 0; i < n->nav_rail.len; i++) {
        if (n->nav_rail.labels[i]) ergo_release_val(EV_STR(n->nav_rail.labels[i]));
        if (n->nav_rail.icons && n->nav_rail.icons[i]) ergo_release_val(EV_STR(n->nav_rail.icons[i]));
      }
      free(n->nav_rail.labels);
      free(n->nav_rail.icons);
    }
    if (n->nav_rail.badges) free(n->nav_rail.badges);
  }
  if (n->kind == COGITO_BOTTOM_NAV && n->bottom_nav.labels) {
    for (size_t i = 0; i < n->bottom_nav.len; i++) {
      if (n->bottom_nav.labels[i]) ergo_release_val(EV_STR(n->bottom_nav.labels[i]));
      if (n->bottom_nav.icons && n->bottom_nav.icons[i]) ergo_release_val(EV_STR(n->bottom_nav.icons[i]));
    }
    free(n->bottom_nav.labels);
    free(n->bottom_nav.icons);
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
  // Zero everything after the ErgoObj base (which ergo_obj_new already set up)
  memset((char*)n + sizeof(ErgoObj), 0, sizeof(CogitoNode) - sizeof(ErgoObj));
  n->kind = kind;
  // Non-zero common defaults
  n->selected = -1;
  n->resizable = true;
  n->editable = true;
  n->grid_span_x = 1;
  n->grid_span_y = 1;
  n->grid_align_x = -1;
  n->grid_align_y = -1;
  n->text_color = cogito_rgba(30, 30, 30, 255);
  n->border_color = cogito_rgba(25, 25, 25, 255);
  n->selection_color = cogito_rgba(208, 220, 245, 255);
  n->track_color = cogito_rgba(170, 170, 170, 255);
  n->track_on_color = cogito_rgba(90, 160, 110, 255);
  n->knob_color = cogito_rgba(250, 250, 250, 255);
  n->check_color = cogito_rgba(30, 30, 30, 255);
  n->font_weight = 400;
  n->opacity = 1.0f;
  n->opacity_from = 1.0f;
  n->opacity_target = 1.0f;
  // Layout widgets default to 0 gap, content widgets default to 8 for natural spacing
  bool is_layout = (kind == COGITO_VSTACK || kind == COGITO_HSTACK || kind == COGITO_ZSTACK ||
                    kind == COGITO_FIXED || kind == COGITO_SCROLLER || kind == COGITO_LIST);
  n->gap = is_layout ? 0 : 8;
  // Kind-specific non-zero defaults
  switch (kind) {
    case COGITO_SCROLLER: n->scroller.vert = true; break;
    case COGITO_SLIDER:
      n->slider.max = 1.0;
      n->slider.value2 = 1.0;
      n->slider.mode = COGITO_SLIDER_MODE_NORMAL;
      n->slider.active_handle = 0;
      n->slider.size = COGITO_SLIDER_SIZE_XS;
      break;
    case COGITO_STEPPER: n->stepper.max = 100.0; n->stepper.step = 1.0; break;
    case COGITO_GRID: n->grid.cols = 1; n->grid.gap_x = 6; n->grid.gap_y = 6; break;
    case COGITO_IMAGE: case COGITO_AVATAR: n->image.avg_lum = 0.5f; break;
    case COGITO_VIEW_DUAL: n->view_dual.ratio = 0.5f; break;
    default: break;
  }
  cogito_apply_style_to_node(n);
  return n;
}

static void cogito_node_set_text(CogitoNode* n, ErgoStr* s) {
  if (!n) return;
  if (s) ergo_retain_val(EV_STR(s));
  if (n->text) ergo_release_val(EV_STR(n->text));
  n->text = s;
  int len = n->text ? (int)n->text->len : 0;
  if (n->text_input.caret > len) n->text_input.caret = len;
  if (n->text_input.sel_start > len) n->text_input.sel_start = len;
  if (n->text_input.sel_end > len) n->text_input.sel_end = len;
}

static void cogito_node_set_subtitle(CogitoNode* n, ErgoStr* s) {
  if (!n) return;
  if (s) ergo_retain_val(EV_STR(s));
  if (n->subtitle) ergo_release_val(EV_STR(n->subtitle));
  n->subtitle = s;
}

static void cogito_node_set_hint(CogitoNode* n, ErgoStr* s) {
  if (!n) return;
  if (s) ergo_retain_val(EV_STR(s));
  if (n->hint) ergo_release_val(EV_STR(n->hint));
  n->hint = s;
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

    #include "widgets/intrinsic/active_indicator.inc"

    #include "widgets/intrinsic/switchbar.inc"

    #include "widgets/intrinsic/content_list.inc"

    #include "widgets/intrinsic/empty_page.inc"

    #include "widgets/intrinsic/tip_view.inc"

    #include "widgets/intrinsic/settings_window.inc"

    #include "widgets/intrinsic/settings_page.inc"

    #include "widgets/intrinsic/settings_list.inc"

    #include "widgets/intrinsic/settings_row.inc"

    #include "widgets/intrinsic/welcome_screen.inc"

    #include "widgets/intrinsic/view_dual.inc"

    #include "widgets/intrinsic/view_chooser.inc"

    #include "widgets/intrinsic/about_window.inc"

    #include "widgets/intrinsic/split_button.inc"

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
