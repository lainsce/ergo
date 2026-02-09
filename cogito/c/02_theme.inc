static void cogito_style_set_padding(CogitoStyle* s, int top, int right, int bottom, int left) {
  s->has_padding = true;
  s->padding_top = top;
  s->padding_right = right;
  s->padding_bottom = bottom;
  s->padding_left = left;
}

static void cogito_style_set_margin(CogitoStyle* s, int top, int right, int bottom, int left) {
  s->has_margin = true;
  s->margin_top = top;
  s->margin_right = right;
  s->margin_bottom = bottom;
  s->margin_left = left;
}

static void cogito_style_set_radius(CogitoStyle* s, int tl, int tr, int br, int bl) {
  s->has_radius_tl = true;
  s->has_radius_tr = true;
  s->has_radius_br = true;
  s->has_radius_bl = true;
  s->radius_tl = tl;
  s->radius_tr = tr;
  s->radius_br = br;
  s->radius_bl = bl;
}

static int cogito_label_class_from_name(const char* s) {
  if (!s || !s[0]) return COGITO_LABEL_CLASS_NONE;
  if (strcmp(s, "title") == 0) return COGITO_LABEL_CLASS_TITLE;
  if (strcmp(s, "subtitle") == 0 || strcmp(s, "sub-title") == 0) return COGITO_LABEL_CLASS_SUBTITLE;
  if (strcmp(s, "body") == 0) return COGITO_LABEL_CLASS_BODY;
  return COGITO_LABEL_CLASS_NONE;
}

static void cogito_theme_set_defaults(void) {
  cogito_style_clear(&cogito_theme.base);
  for (int i = 0; i < COGITO_KIND_COUNT; i++) {
    cogito_style_clear(&cogito_theme.per_kind[i]);
    cogito_style_clear(&cogito_theme.per_kind_hover[i]);
    cogito_style_clear(&cogito_theme.per_kind_active[i]);
    cogito_style_clear(&cogito_theme.per_kind_checked[i]);
    cogito_style_clear(&cogito_theme.per_kind_disabled[i]);
  }
  for (int i = 0; i < 4; i++) {
    cogito_style_clear(&cogito_theme.label_class[i]);
    cogito_style_clear(&cogito_theme.label_class_hover[i]);
    cogito_style_clear(&cogito_theme.label_class_active[i]);
    cogito_style_clear(&cogito_theme.label_class_checked[i]);
    cogito_style_clear(&cogito_theme.label_class_disabled[i]);
  }
  cogito_style_clear(&cogito_theme.class_mono);
  cogito_style_clear(&cogito_theme.class_mono_hover);
  cogito_style_clear(&cogito_theme.class_mono_active);
  cogito_style_clear(&cogito_theme.class_mono_checked);
  cogito_style_clear(&cogito_theme.class_mono_disabled);
  cogito_style_clear(&cogito_theme.class_tabular);
  cogito_style_clear(&cogito_theme.class_tabular_hover);
  cogito_style_clear(&cogito_theme.class_tabular_active);
  cogito_style_clear(&cogito_theme.class_tabular_checked);
  cogito_style_clear(&cogito_theme.class_tabular_disabled);
  cogito_style_clear(&cogito_theme.menu);
  cogito_style_clear(&cogito_theme.menu_item);
  cogito_style_clear(&cogito_theme.appbar_child_button);
  cogito_style_clear(&cogito_theme.appbar_child_button_hover);
  cogito_style_clear(&cogito_theme.appbar_child_button_active);
  cogito_style_clear(&cogito_theme.appbar_child_iconbtn);
  cogito_style_clear(&cogito_theme.appbar_child_iconbtn_hover);
  cogito_style_clear(&cogito_theme.appbar_child_iconbtn_active);
  cogito_style_clear(&cogito_theme.appbar_win_btn_all);
  cogito_style_clear(&cogito_theme.appbar_win_btn_hover_all);
  cogito_style_clear(&cogito_theme.appbar_win_btn_active_all);
  for (int i = 0; i < 3; i++) {
    cogito_style_clear(&cogito_theme.appbar_win_btn[i]);
    cogito_style_clear(&cogito_theme.appbar_win_btn_hover[i]);
    cogito_style_clear(&cogito_theme.appbar_win_btn_active[i]);
  }
  cogito_style_clear(&cogito_theme.checkbox_check);
  cogito_style_clear(&cogito_theme.checkbox_check_hover);
  cogito_style_clear(&cogito_theme.checkbox_check_active);
  cogito_style_clear(&cogito_theme.checkbox_check_checked);
  cogito_style_clear(&cogito_theme.radio_check);
  cogito_style_clear(&cogito_theme.radio_check_hover);
  cogito_style_clear(&cogito_theme.radio_check_active);
  cogito_style_clear(&cogito_theme.radio_check_checked);
  cogito_style_clear(&cogito_theme.checkbox_box);
  cogito_style_clear(&cogito_theme.checkbox_box_hover);
  cogito_style_clear(&cogito_theme.checkbox_box_active);
  cogito_style_clear(&cogito_theme.checkbox_box_checked);
  cogito_style_clear(&cogito_theme.radio_box);
  cogito_style_clear(&cogito_theme.radio_box_hover);
  cogito_style_clear(&cogito_theme.radio_box_active);
  cogito_style_clear(&cogito_theme.radio_box_checked);
  cogito_style_clear(&cogito_theme.switch_track);
  cogito_style_clear(&cogito_theme.switch_track_hover);
  cogito_style_clear(&cogito_theme.switch_track_active);
  cogito_style_clear(&cogito_theme.switch_track_checked);
  cogito_style_clear(&cogito_theme.switch_knob);
  cogito_style_clear(&cogito_theme.switch_knob_hover);
  cogito_style_clear(&cogito_theme.switch_knob_active);
  cogito_style_clear(&cogito_theme.switch_knob_checked);
  cogito_theme.base.has_text = true;
  cogito_theme.base.text = cogito_rgba(30, 30, 30, 255);

  CogitoStyle* win = &cogito_theme.per_kind[COGITO_WINDOW];
  win->has_bg = true;
  win->bg = cogito_rgba(245, 245, 245, 255);

  CogitoStyle* btn = &cogito_theme.per_kind[COGITO_BUTTON];
  btn->has_bg = true;
  btn->bg = cogito_rgba(230, 230, 230, 255);
  btn->has_radius = true;
  btn->radius = 6;

  CogitoStyle* icon = &cogito_theme.per_kind[COGITO_ICONBTN];
  icon->has_bg = true;
  icon->bg = cogito_rgba(230, 230, 230, 255);
  icon->has_radius = true;
  icon->radius = 16;

  CogitoStyle* list = &cogito_theme.per_kind[COGITO_LIST];
  list->has_bg = true;
  list->bg = cogito_rgba(255, 255, 255, 255);
  list->has_radius = true;
  list->radius = 4;
  list->has_selection = true;
  list->selection = cogito_rgba(208, 220, 245, 255);

  CogitoStyle* grid = &cogito_theme.per_kind[COGITO_GRID];
  *grid = *list;

  CogitoStyle* appbar = &cogito_theme.per_kind[COGITO_APPBAR];
  appbar->has_bg = true;
  appbar->bg = cogito_rgba(238, 238, 238, 255);
}

static void cogito_theme_init(void) {
  if (!cogito_theme_initialized) {
    cogito_theme_initialized = true;
    cogito_theme_set_defaults();
    if (!cogito_sum_default_loaded) {
      cogito_sum_default_loaded = true;
      cogito_load_sum_file("cogito/cogito_default.sum");
    }
  }
}

static CogitoStyle cogito_style_merge(const CogitoStyle* base, const CogitoStyle* over) {
  CogitoStyle out = *base;
  if (over->has_bg) { out.has_bg = true; out.bg = over->bg; }
  if (over->has_text) { out.has_text = true; out.text = over->text; }
  if (over->has_border) { out.has_border = true; out.border = over->border; }
  if (over->has_border_width) { out.has_border_width = true; out.border_width = over->border_width; }
  if (over->has_radius) { out.has_radius = true; out.radius = over->radius; }
  if (over->has_selection) { out.has_selection = true; out.selection = over->selection; }
  if (over->has_font_size) { out.has_font_size = true; out.font_size = over->font_size; }
  if (over->has_track) { out.has_track = true; out.track = over->track; }
  if (over->has_track_on) { out.has_track_on = true; out.track_on = over->track_on; }
  if (over->has_knob) { out.has_knob = true; out.knob = over->knob; }
  if (over->has_check) { out.has_check = true; out.check = over->check; }
  if (over->has_font_weight) { out.has_font_weight = true; out.font_weight = over->font_weight; }
  if (over->has_letter_spacing) { out.has_letter_spacing = true; out.letter_spacing = over->letter_spacing; }
  if (over->has_font_tabular) { out.has_font_tabular = true; out.font_tabular = over->font_tabular; }
  if (over->has_font_mono) { out.has_font_mono = true; out.font_mono = over->font_mono; }
  if (over->has_min_w) { out.has_min_w = true; out.min_w = over->min_w; }
  if (over->has_min_h) { out.has_min_h = true; out.min_h = over->min_h; }
  if (over->has_max_w) { out.has_max_w = true; out.max_w = over->max_w; }
  if (over->has_max_h) { out.has_max_h = true; out.max_h = over->max_h; }
  if (over->has_icon_size) { out.has_icon_size = true; out.icon_size = over->icon_size; }
  if (over->has_icon_color) { out.has_icon_color = true; out.icon_color = over->icon_color; }
  if (over->has_transition_ms) { out.has_transition_ms = true; out.transition_ms = over->transition_ms; }
  if (over->has_transition_ease) { out.has_transition_ease = true; out.transition_ease = over->transition_ease; }
  if (over->has_appbar_btn_size) { out.has_appbar_btn_size = true; out.appbar_btn_size = over->appbar_btn_size; }
  if (over->has_appbar_btn_gap) { out.has_appbar_btn_gap = true; out.appbar_btn_gap = over->appbar_btn_gap; }
  if (over->has_appbar_btn_pad_top) { out.has_appbar_btn_pad_top = true; out.appbar_btn_pad_top = over->appbar_btn_pad_top; }
  if (over->has_appbar_btn_pad_right) { out.has_appbar_btn_pad_right = true; out.appbar_btn_pad_right = over->appbar_btn_pad_right; }
  if (over->has_appbar_btn_close_color) { out.has_appbar_btn_close_color = true; out.appbar_btn_close_color = over->appbar_btn_close_color; }
  if (over->has_appbar_btn_min_color) { out.has_appbar_btn_min_color = true; out.appbar_btn_min_color = over->appbar_btn_min_color; }
  if (over->has_appbar_btn_max_color) { out.has_appbar_btn_max_color = true; out.appbar_btn_max_color = over->appbar_btn_max_color; }
  if (over->has_appbar_btn_border_color) { out.has_appbar_btn_border_color = true; out.appbar_btn_border_color = over->appbar_btn_border_color; }
  if (over->has_appbar_btn_border_width) { out.has_appbar_btn_border_width = true; out.appbar_btn_border_width = over->appbar_btn_border_width; }

  if (over->has_border_style) { out.has_border_style = true; out.border_style = over->border_style; }
  if (over->has_radius_tl) { out.has_radius_tl = true; out.radius_tl = over->radius_tl; }
  if (over->has_radius_tr) { out.has_radius_tr = true; out.radius_tr = over->radius_tr; }
  if (over->has_radius_br) { out.has_radius_br = true; out.radius_br = over->radius_br; }
  if (over->has_radius_bl) { out.has_radius_bl = true; out.radius_bl = over->radius_bl; }
  if (over->has_font_family) {
    out.has_font_family = true;
    strncpy(out.font_family, over->font_family, 63);
    out.font_family[63] = '\0';
  }
  if (over->has_box_shadow) {
    out.has_box_shadow = true;
    out.box_shadow = over->box_shadow;
  }

  if (over->has_padding) {
    out.has_padding = true;
    out.padding_left = over->padding_left;
    out.padding_top = over->padding_top;
    out.padding_right = over->padding_right;
    out.padding_bottom = over->padding_bottom;
  }
  if (over->has_padding_left) { out.has_padding_left = true; out.padding_left = over->padding_left; }
  if (over->has_padding_top) { out.has_padding_top = true; out.padding_top = over->padding_top; }
  if (over->has_padding_right) { out.has_padding_right = true; out.padding_right = over->padding_right; }
  if (over->has_padding_bottom) { out.has_padding_bottom = true; out.padding_bottom = over->padding_bottom; }

  if (over->has_margin) {
    out.has_margin = true;
    out.margin_left = over->margin_left;
    out.margin_top = over->margin_top;
    out.margin_right = over->margin_right;
    out.margin_bottom = over->margin_bottom;
  }
  if (over->has_margin_left) { out.has_margin_left = true; out.margin_left = over->margin_left; }
  if (over->has_margin_top) { out.has_margin_top = true; out.margin_top = over->margin_top; }
  if (over->has_margin_right) { out.has_margin_right = true; out.margin_right = over->margin_right; }
  if (over->has_margin_bottom) { out.has_margin_bottom = true; out.margin_bottom = over->margin_bottom; }

  if (over->has_opacity) { out.has_opacity = true; out.opacity = over->opacity; }
  if (over->has_track_height) { out.has_track_height = true; out.track_height = over->track_height; }

  return out;
}

static CogitoStyle cogito_theme_resolve(CogitoKind kind) {
  return cogito_style_merge(&cogito_theme.base, &cogito_theme.per_kind[kind]);
}

static CogitoStyle cogito_theme_resolve_node(const CogitoNode* n) {
  CogitoStyle s = cogito_theme_resolve(n->kind);
  if (n->kind == COGITO_LABEL && n->label_class > COGITO_LABEL_CLASS_NONE && n->label_class < 4) {
    s = cogito_style_merge(&s, &cogito_theme.label_class[n->label_class]);
  }
  if (n->font_mono) {
    s = cogito_style_merge(&s, &cogito_theme.class_mono);
  }
  if (n->font_tabular) {
    s = cogito_style_merge(&s, &cogito_theme.class_tabular);
  }
  if (n->button_outlined) {
    s = cogito_style_merge(&s, &cogito_theme.button_outlined);
  }
  if (n->button_text) {
    s = cogito_style_merge(&s, &cogito_theme.button_text);
  }
  if (n->parent && n->parent->kind == COGITO_APPBAR) {
    if (n->kind == COGITO_BUTTON) {
      s = cogito_style_merge(&s, &cogito_theme.appbar_child_button);
    } else if (n->kind == COGITO_ICONBTN) {
      s = cogito_style_merge(&s, &cogito_theme.appbar_child_iconbtn);
    }
  }
  // Apply custom class styles if class_name is set
  if (n->class_name && n->class_name->data && n->class_name->len > 0) {
    // Preserve window background color - custom classes should not override it
    bool is_window = (n->kind == COGITO_WINDOW);
    CogitoColor saved_bg = s.bg;
    bool saved_has_bg = s.has_bg;
    for (int i = 0; i < cogito_theme.custom_class_count; i++) {
      // Match both class name and widget kind
      if (strcmp(cogito_theme.custom_classes[i].name, n->class_name->data) == 0 &&
          cogito_theme.custom_classes[i].kind == n->kind) {
        s = cogito_style_merge(&s, &cogito_theme.custom_classes[i].style);
      }
    }
    // Restore window background if it was overridden by custom class
    if (is_window && saved_has_bg) {
      s.bg = saved_bg;
      s.has_bg = saved_has_bg;
    }
  }
  return s;
}

static CogitoStyle cogito_checkbox_check_style(bool is_radio, bool checked, bool hover, bool active) {
  CogitoStyle base = is_radio ? cogito_theme.radio_check : cogito_theme.checkbox_check;
  if (checked) {
    CogitoStyle over = is_radio ? cogito_theme.radio_check_checked : cogito_theme.checkbox_check_checked;
    base = cogito_style_merge(&base, &over);
  }
  if (active) {
    CogitoStyle over = is_radio ? cogito_theme.radio_check_active : cogito_theme.checkbox_check_active;
    return cogito_style_merge(&base, &over);
  }
  if (hover) {
    CogitoStyle over = is_radio ? cogito_theme.radio_check_hover : cogito_theme.checkbox_check_hover;
    return cogito_style_merge(&base, &over);
  }
  return base;
}

static CogitoStyle cogito_checkbox_box_style(bool is_radio, bool checked, bool hover, bool active) {
  CogitoStyle base = is_radio ? cogito_theme.radio_box : cogito_theme.checkbox_box;
  if (checked) {
    CogitoStyle over = is_radio ? cogito_theme.radio_box_checked : cogito_theme.checkbox_box_checked;
    base = cogito_style_merge(&base, &over);
  }
  if (active) {
    CogitoStyle over = is_radio ? cogito_theme.radio_box_active : cogito_theme.checkbox_box_active;
    return cogito_style_merge(&base, &over);
  }
  if (hover) {
    CogitoStyle over = is_radio ? cogito_theme.radio_box_hover : cogito_theme.checkbox_box_hover;
    return cogito_style_merge(&base, &over);
  }
  return base;
}

static CogitoStyle cogito_switch_track_style(bool checked, bool hover, bool active) {
  CogitoStyle base = cogito_theme.switch_track;
  if (checked) {
    base = cogito_style_merge(&base, &cogito_theme.switch_track_checked);
  }
  if (active) {
    return cogito_style_merge(&base, &cogito_theme.switch_track_active);
  }
  if (hover) {
    return cogito_style_merge(&base, &cogito_theme.switch_track_hover);
  }
  return base;
}

static CogitoStyle cogito_switch_knob_style(bool checked, bool hover, bool active) {
  CogitoStyle base = cogito_theme.switch_knob;
  if (checked) {
    base = cogito_style_merge(&base, &cogito_theme.switch_knob_checked);
  }
  if (active) {
    return cogito_style_merge(&base, &cogito_theme.switch_knob_active);
  }
  if (hover) {
    return cogito_style_merge(&base, &cogito_theme.switch_knob_hover);
  }
  return base;
}

static void cogito_node_hover_state(const CogitoNode* n, bool* out_hover, bool* out_active) {
  if (!out_hover || !out_active) return;
  *out_hover = false;
  *out_active = false;
  if (!n) return;
  if (n->disabled) return;
  if (cogito_pointer_capture) {
    if (cogito_pointer_capture != n) return;
    *out_hover = true;
    *out_active = cogito_backend->is_mouse_button_down(0); // 0 = left button
    return;
  }
  int mx = 0, my = 0;
  cogito_backend->get_mouse_position(&mx, &my);
  bool hover = cogito_hit_rect(mx, my, n->x, n->y, n->w, n->h);
  bool active = hover && cogito_backend->is_mouse_button_down(0); // 0 = left button
  *out_hover = hover;
  *out_active = active;
}

static bool cogito_node_disabled(const CogitoNode* n);
static void cogito_toast_close_rect(CogitoNode* n, int* out_x, int* out_y, int* out_w, int* out_h);

static CogitoStyle cogito_theme_resolve_node_state(const CogitoNode* n, bool hover, bool active) {
  CogitoStyle s = cogito_theme_resolve_node(n);
  bool checked = false;
  if (n && (n->kind == COGITO_CHECKBOX || n->kind == COGITO_SWITCH)) checked = n->checked;
  else if (n && n->kind == COGITO_CHIP) checked = n->chip_selected;
  if (n->kind == COGITO_LABEL && n->label_class > COGITO_LABEL_CLASS_NONE && n->label_class < 4) {
    if (checked) s = cogito_style_merge(&s, &cogito_theme.label_class_checked[n->label_class]);
    if (cogito_node_disabled(n)) {
      s = cogito_style_merge(&s, &cogito_theme.label_class_disabled[n->label_class]);
    }
    if (!cogito_node_disabled(n)) {
      if (active) s = cogito_style_merge(&s, &cogito_theme.label_class_active[n->label_class]);
      else if (hover) s = cogito_style_merge(&s, &cogito_theme.label_class_hover[n->label_class]);
    }
  }
  if (n->font_mono) {
    if (checked) s = cogito_style_merge(&s, &cogito_theme.class_mono_checked);
    if (cogito_node_disabled(n)) {
      s = cogito_style_merge(&s, &cogito_theme.class_mono_disabled);
    }
    if (!cogito_node_disabled(n)) {
      if (active) s = cogito_style_merge(&s, &cogito_theme.class_mono_active);
      else if (hover) s = cogito_style_merge(&s, &cogito_theme.class_mono_hover);
    }
  }
  if (n->font_tabular) {
    if (checked) s = cogito_style_merge(&s, &cogito_theme.class_tabular_checked);
    if (cogito_node_disabled(n)) {
      s = cogito_style_merge(&s, &cogito_theme.class_tabular_disabled);
    }
    if (!cogito_node_disabled(n)) {
      if (active) s = cogito_style_merge(&s, &cogito_theme.class_tabular_active);
      else if (hover) s = cogito_style_merge(&s, &cogito_theme.class_tabular_hover);
    }
  }
  if (n->button_outlined) {
    if (cogito_node_disabled(n)) {
      s = cogito_style_merge(&s, &cogito_theme.button_outlined_disabled);
    }
    if (!cogito_node_disabled(n)) {
      if (active) s = cogito_style_merge(&s, &cogito_theme.button_outlined_active);
      else if (hover) s = cogito_style_merge(&s, &cogito_theme.button_outlined_hover);
    }
  }
  if (n->button_text) {
    if (cogito_node_disabled(n)) {
      s = cogito_style_merge(&s, &cogito_theme.button_text_disabled);
    }
    if (!cogito_node_disabled(n)) {
      if (active) s = cogito_style_merge(&s, &cogito_theme.button_text_active);
      else if (hover) s = cogito_style_merge(&s, &cogito_theme.button_text_hover);
    }
  }
  // Apply custom class state variants
  if (n->class_name && n->class_name->data && n->class_name->len > 0) {
    for (int i = 0; i < cogito_theme.custom_class_count; i++) {
      if (strcmp(cogito_theme.custom_classes[i].name, n->class_name->data) == 0 &&
          cogito_theme.custom_classes[i].kind == n->kind) {
        if (checked) s = cogito_style_merge(&s, &cogito_theme.custom_classes[i].checked);
        if (cogito_node_disabled(n)) {
          s = cogito_style_merge(&s, &cogito_theme.custom_classes[i].disabled);
        } else {
          if (active) s = cogito_style_merge(&s, &cogito_theme.custom_classes[i].active);
          else if (hover) s = cogito_style_merge(&s, &cogito_theme.custom_classes[i].hover);
        }
        break; // Found the matching custom class
      }
    }
  }
  if (checked) s = cogito_style_merge(&s, &cogito_theme.per_kind_checked[n->kind]);
  if (cogito_node_disabled(n)) {
    s = cogito_style_merge(&s, &cogito_theme.per_kind_disabled[n->kind]);
    return s;
  }
  if (active) s = cogito_style_merge(&s, &cogito_theme.per_kind_active[n->kind]);
  else if (hover) s = cogito_style_merge(&s, &cogito_theme.per_kind_hover[n->kind]);
  return s;
}

static void cogito_apply_style_to_node(CogitoNode* n) {
  if (!n) return;
  cogito_theme_init();
  CogitoStyle s = cogito_theme_resolve_node(n);
  if (s.has_bg) { n->bg = s.bg; n->bg_set = true; }
  else { n->bg_set = false; }
  if (s.has_text) { n->text_color = s.text; n->text_color_set = true; }
  else { n->text_color_set = false; }
  if (s.has_border) { n->border_color = s.border; n->border_color_set = true; }
  else { n->border_color_set = false; n->border_color = cogito_rgba(0, 0, 0, 0); }
  if (s.has_border_width) { n->border_width = s.border_width; n->border_width_set = true; }
  else { n->border_width = 0; n->border_width_set = false; }
  if (s.has_radius) { n->border_radius = s.radius; n->radius_set = true; }
  else { n->border_radius = 0; n->radius_set = false; }
  if (s.has_selection) { n->selection_color = s.selection; n->selection_set = true; }
  else { n->selection_set = false; }
  if (s.has_track) { n->track_color = s.track; n->track_set = true; }
  else { n->track_set = false; }
  if (s.has_track_on) { n->track_on_color = s.track_on; n->track_on_set = true; }
  else { n->track_on_set = false; }
  if (s.has_knob) { n->knob_color = s.knob; n->knob_set = true; }
  else { n->knob_set = false; }
  if (s.has_check) { n->check_color = s.check; n->check_set = true; }
  else { n->check_set = false; }
  if (s.has_font_weight && (n->kind == COGITO_LABEL || n->kind == COGITO_BUTTON)) {
    n->font_weight = s.font_weight;
    n->font_weight_set = true;
  } else if (n->kind == COGITO_LABEL || n->kind == COGITO_BUTTON) {
    n->font_weight_set = false;
    n->font_weight = 400;
  }
  if (s.has_letter_spacing) {
    n->letter_spacing = s.letter_spacing;
    n->letter_spacing_set = true;
  } else {
    n->letter_spacing_set = false;
    n->letter_spacing = 0.0f;
  }
  if (s.has_font_tabular) {
    n->font_tabular = s.font_tabular;
  }
  if (s.has_font_mono) {
    n->font_mono = s.font_mono;
  }
  if (s.has_min_w) { n->min_w = s.min_w; n->min_w_set = true; }
  else { n->min_w_set = false; }
  if (s.has_min_h) { n->min_h = s.min_h; n->min_h_set = true; }
  else { n->min_h_set = false; }
  if (s.has_max_w) { n->max_w = s.max_w; n->max_w_set = true; }
  else { n->max_w_set = false; }
  if (s.has_max_h) { n->max_h = s.max_h; n->max_h_set = true; }
  else { n->max_h_set = false; }
  if (s.has_shadow) { n->shadow_level = s.shadow_level; n->shadow_set = true; }
  else { n->shadow_set = false; }
  if (s.has_border_style) { n->border_style = s.border_style; n->border_style_set = true; }
  else { n->border_style_set = false; }
  if (s.has_radius_tl) { n->radius_tl = s.radius_tl; n->radius_tl_set = true; }
  else { n->radius_tl_set = false; }
  if (s.has_radius_tr) { n->radius_tr = s.radius_tr; n->radius_tr_set = true; }
  else { n->radius_tr_set = false; }
  if (s.has_radius_br) { n->radius_br = s.radius_br; n->radius_br_set = true; }
  else { n->radius_br_set = false; }
  if (s.has_radius_bl) { n->radius_bl = s.radius_bl; n->radius_bl_set = true; }
  else { n->radius_bl_set = false; }
  if (s.has_font_family) {
    strncpy(n->font_family, s.font_family, 63);
    n->font_family[63] = '\0';
    n->font_family_set = true;
  } else {
    n->font_family_set = false;
  }
  if (s.has_box_shadow) { n->box_shadow = s.box_shadow; n->box_shadow_set = true; }
  else { n->box_shadow_set = false; }
  if (s.has_track_height) { n->track_height = s.track_height; n->track_height_set = true; }
  else { n->track_height_set = false; }
  if (s.has_font_size) {
    if (n->kind == COGITO_WINDOW) {
      int sz = s.font_size;
      if (sz > 0 && sz != cogito_font_size_value) {
        cogito_font_size_value = sz;
        if (cogito_backend_ready && cogito_font_ready && cogito_font_from_env) {
          const char* font_path = cogito_font_path_active;
          if (font_path && font_path[0] && cogito_font_loaded_size != sz) {
            cogito_backend->font_unload(cogito_font);
            if (cogito_load_font_file(&cogito_font, font_path, sz, cogito_font_weight_normal())) {
              cogito_font_ready = true;
              cogito_font_loaded_size = sz;
            } else {
              cogito_font_ready = false;
            }
          }
        }
        if (cogito_backend_ready && cogito_font_mono_ready && cogito_font_mono_from_env) {
          const char* mono_path = cogito_font_mono_path_active;
          if (mono_path && mono_path[0] && cogito_font_mono_loaded_size != sz) {
            cogito_backend->font_unload(cogito_font_mono);
            if (cogito_load_font_file(&cogito_font_mono, mono_path, sz, cogito_font_weight_normal())) {
              cogito_font_mono_ready = true;
              cogito_font_mono_loaded_size = sz;
            } else {
              cogito_font_mono_ready = false;
            }
          }
        }
        if (cogito_backend_ready && cogito_font_bold_ready && cogito_font_bold_from_env) {
          const char* bold_path = cogito_font_bold_path_active;
          if (bold_path && bold_path[0] && cogito_font_bold_loaded_size != sz) {
            cogito_backend->font_unload(cogito_font_bold);
            bool weight_applied = false;
            if (cogito_load_font_file_ex(&cogito_font_bold, bold_path, sz, cogito_font_weight_bold(), &weight_applied)) {
              if (cogito_font_bold_variable && !weight_applied) {
                cogito_backend->font_unload(cogito_font_bold);
                cogito_font_bold_ready = false;
                cogito_font_bold_variable = false;
              } else {
                cogito_font_bold_ready = true;
                cogito_font_bold_loaded_size = sz;
              }
            } else {
              cogito_font_bold_ready = false;
              cogito_font_bold_variable = false;
            }
          }
        }
      }
    } else if (n->kind == COGITO_LABEL || n->kind == COGITO_BUTTON) {
      n->font_size = s.font_size;
      n->font_size_set = true;
    }
  } else if (n->kind == COGITO_LABEL || n->kind == COGITO_BUTTON) {
    n->font_size_set = false;
  }

  if (!n->padding_set) {
    if (s.has_padding) {
      n->padding_left = s.padding_left;
      n->padding_top = s.padding_top;
      n->padding_right = s.padding_right;
      n->padding_bottom = s.padding_bottom;
    }
    if (s.has_padding_left) n->padding_left = s.padding_left;
    if (s.has_padding_top) n->padding_top = s.padding_top;
    if (s.has_padding_right) n->padding_right = s.padding_right;
    if (s.has_padding_bottom) n->padding_bottom = s.padding_bottom;
  }

  if (!n->margin_set) {
    if (s.has_margin) {
      n->margin_left = s.margin_left;
      n->margin_top = s.margin_top;
      n->margin_right = s.margin_right;
      n->margin_bottom = s.margin_bottom;
    }
    if (s.has_margin_left) n->margin_left = s.margin_left;
    if (s.has_margin_top) n->margin_top = s.margin_top;
    if (s.has_margin_right) n->margin_right = s.margin_right;
    if (s.has_margin_bottom) n->margin_bottom = s.margin_bottom;
  }
  if (s.has_opacity) { n->opacity = s.opacity; n->opacity_set = true; }
  else { n->opacity_set = false; n->opacity = 1.0f; }
}

static void cogito_apply_style_tree(CogitoNode* n) {
  if (!n) return;
  cogito_apply_style_to_node(n);
  for (size_t i = 0; i < n->len; i++) {
    cogito_apply_style_tree(n->children[i]);
  }
}

static int cogito_font_size(void) {
  return cogito_font_size_value > 0 ? cogito_font_size_value : 16;
}

static int cogito_label_font_size(const CogitoNode* n) {
  if (!n) return cogito_font_size();
  if (n->font_size_set && n->font_size > 0) return n->font_size;
  switch (n->label_class) {
    case COGITO_LABEL_CLASS_TITLE: return 32;
    case COGITO_LABEL_CLASS_SUBTITLE: return 26;
    case COGITO_LABEL_CLASS_BODY: return 16;
    default: return cogito_font_size();
  }
}

static bool cogito_label_bold(const CogitoNode* n);

static int cogito_node_font_size(const CogitoNode* n) {
  if (!n) return cogito_font_size();
  if (n->kind == COGITO_LABEL) return cogito_label_font_size(n);
  if (n->font_size_set && n->font_size > 0) return n->font_size;
  return cogito_font_size();
}

static bool cogito_node_bold(const CogitoNode* n) {
  if (!n) return false;
  if (n->kind == COGITO_LABEL) return cogito_label_bold(n);
  if (n->font_weight_set) return n->font_weight >= 600;
  return false;
}

static bool cogito_node_disabled(const CogitoNode* n) {
  return n && n->disabled;
}

static void cogito_apply_size_constraints(const CogitoNode* n, int* w, int* h) {
  if (!n || !w || !h) return;
  if (n->min_w_set && *w < n->min_w) *w = n->min_w;
  if (n->min_h_set && *h < n->min_h) *h = n->min_h;
  if (n->max_w_set && n->max_w > 0 && *w > n->max_w) *w = n->max_w;
  if (n->max_h_set && n->max_h > 0 && *h > n->max_h) *h = n->max_h;
}

static void cogito_appbar_button_metrics(int* size, int* gap, int* pad_top, int* pad_right) {
  CogitoStyle s = cogito_theme.appbar_win_btn_all;
  if (size) {
    if (s.has_min_w) *size = s.min_w;
    else if (s.has_min_h) *size = s.min_h;
    else *size = 12;
  }
  if (gap) *gap = 9;
  if (pad_top) *pad_top = 9;
  if (pad_right) *pad_right = 9;
}

static int cogito_appbar_ctrl_index(char c) {
  if (c == 'C') return 0;
  if (c == 'I') return 1;
  if (c == 'M') return 2;
  return -1;
}

static void cogito_appbar_controls_positions(CogitoNode* n, int x, int w, int btn_size, int gap, int pad, bool allow_max, int* out_close, int* out_min, int* out_max) {
  int pos[3] = {-1, -1, -1};
  bool seen[3] = {false, false, false};
  const char* s = n->appbar_controls[0] ? n->appbar_controls : "CIM|";
  const char* split = strchr(s, '|');
  size_t left_len = split ? (size_t)(split - s) : strlen(s);
  const char* right = split ? split + 1 : "";
  size_t right_len = strlen(right);

  int lx = x + pad;
  for (size_t i = 0; i < left_len; i++) {
    int idx = cogito_appbar_ctrl_index(s[i]);
    if (!allow_max && idx == 2) continue;
    if (idx < 0 || seen[idx]) continue;
    pos[idx] = lx;
    seen[idx] = true;
    lx += btn_size + gap;
  }

  int rx = x + w - pad - btn_size;
  for (size_t i = 0; i < right_len; i++) {
    char c = right[right_len - 1 - i];
    int idx = cogito_appbar_ctrl_index(c);
    if (!allow_max && idx == 2) continue;
    if (idx < 0 || seen[idx]) continue;
    pos[idx] = rx;
    seen[idx] = true;
    rx -= btn_size + gap;
  }

  if (out_close) *out_close = pos[0];
  if (out_min) *out_min = pos[1];
  if (out_max) *out_max = pos[2];
}

// Helper functions for multi-appbar support
static CogitoNode* cogito_find_appbar_recursive(CogitoNode* n) {
  if (!n) return NULL;
  if (n->kind == COGITO_APPBAR) return n;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* found = cogito_find_appbar_recursive(n->children[i]);
    if (found) return found;
  }
  return NULL;
}

static CogitoNode* cogito_find_appbar(CogitoNode* win) {
  if (!win) return NULL;
  return cogito_find_appbar_recursive(win);
}

static CogitoNode* cogito_find_appbar_at_point_recursive(CogitoNode* n, int x, int y) {
  if (!n) return NULL;
  if (n->kind == COGITO_APPBAR && cogito_hit_rect(x, y, n->x, n->y, n->w, n->h)) {
    return n;
  }
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* hit = cogito_find_appbar_at_point_recursive(n->children[i], x, y);
    if (hit) return hit;
  }
  return NULL;
}

static CogitoNode* cogito_find_appbar_at_point(CogitoNode* win, int x, int y) {
  if (!win) return NULL;
  return cogito_find_appbar_at_point_recursive(win, x, y);
}

static bool cogito_is_first_appbar(CogitoNode* win, CogitoNode* appbar) {
  if (!win || !appbar) return false;
  CogitoNode* first = cogito_find_appbar(win);
  return first == appbar;
}

static bool cogito_point_over_appbar_buttons(CogitoNode* app, int x, int y) {
  if (!app) return false;
  if (app->appbar_btn_close_x >= 0 &&
      cogito_hit_rect(x, y, app->appbar_btn_close_x, app->appbar_btn_y, app->appbar_btn_size, app->appbar_btn_size)) return true;
  if (app->appbar_btn_min_x >= 0 &&
      cogito_hit_rect(x, y, app->appbar_btn_min_x, app->appbar_btn_y, app->appbar_btn_size, app->appbar_btn_size)) return true;
  if (app->appbar_btn_max_x >= 0 &&
      cogito_hit_rect(x, y, app->appbar_btn_max_x, app->appbar_btn_y, app->appbar_btn_size, app->appbar_btn_size)) return true;
  for (size_t i = 0; i < app->len; i++) {
    CogitoNode* c = app->children[i];
    if (cogito_hit_rect(x, y, c->x, c->y, c->w, c->h)) return true;
  }
  return false;
}

static int cogito_label_class_font_size(int cls, int fallback);
static int cogito_text_height_size(int size);
static void cogito_intrinsic_size(CogitoNode* n, int* out_w, int* out_h);

static void cogito_appbar_row_positions(
  CogitoNode* n,
  int* out_buttons_y,
  int* out_title_y,
  int* out_subtitle_y,
  int* out_child_y,
  int* out_appbar_h
) {
  int btn_size = 12;
  int btn_gap = 6;
  int btn_pad_top = 9;
  int btn_pad_right = 9;
  cogito_appbar_button_metrics(&btn_size, &btn_gap, &btn_pad_top, &btn_pad_right);

  const int has_title = (n->text && n->text->len);
  const int has_subtitle = (n->subtitle && n->subtitle->len);
  const int has_children = (n->len > 0);

  int title_h = 0;
  int subtitle_h = 0;

  if (has_title) {
    int tsz = cogito_label_class_font_size(COGITO_LABEL_CLASS_TITLE, 32);
    title_h = cogito_text_height_size(tsz);
  }
  if (has_subtitle) {
    int ssz = cogito_label_class_font_size(COGITO_LABEL_CLASS_SUBTITLE, 20);
    subtitle_h = cogito_text_height_size(ssz);
  }

  int icon_row_h = 0;
  if (has_children) {
    for (size_t i = 0; i < n->len; i++) {
      int cw = 0;
      int ch = 0;
      cogito_intrinsic_size(n->children[i], &cw, &ch);
      if (ch > icon_row_h) icon_row_h = ch;
    }
    if (icon_row_h == 0) icon_row_h = btn_size;
  }

  const int y_buttons = n->y + btn_pad_top;

  // Baseline where text rows start (same as your previous y1)
  int cursor_y = y_buttons + btn_size + 18;

  int y_title = 0;
  int y_subtitle = 0;

  if (has_title) {
    y_title = cursor_y;
    cursor_y += title_h + 12;
  }

  if (has_subtitle) {
    y_subtitle = cursor_y;
    cursor_y += subtitle_h + 12;
  }

  int y_child = 0;
  if (has_children) {
    y_child = cursor_y;
    cursor_y += icon_row_h + 12; // bottom padding for the child row
  }

  const int h = cursor_y - n->y;

  if (out_buttons_y)  *out_buttons_y  = y_buttons;
  if (out_title_y)    *out_title_y    = y_title;     // 0 if no title
  if (out_subtitle_y) *out_subtitle_y = y_subtitle;  // 0 if no subtitle
  if (out_child_y)    *out_child_y    = y_child;     // 0 if no children
  if (out_appbar_h)   *out_appbar_h   = h;
}

static CogitoStyle cogito_appbar_win_btn_style(int kind, int state) {
  CogitoStyle s = cogito_style_merge(&cogito_theme.appbar_win_btn_all, &cogito_theme.appbar_win_btn[kind]);
  if (state == 1) {
    s = cogito_style_merge(&s, &cogito_theme.appbar_win_btn_hover_all);
    s = cogito_style_merge(&s, &cogito_theme.appbar_win_btn_hover[kind]);
  } else if (state == 2) {
    s = cogito_style_merge(&s, &cogito_theme.appbar_win_btn_active_all);
    s = cogito_style_merge(&s, &cogito_theme.appbar_win_btn_active[kind]);
  }
  return s;
}

static bool cogito_label_bold(const CogitoNode* n) {
  if (!n) return false;
  if (n->font_weight_set) return n->font_weight >= 600;
  return n->label_class == COGITO_LABEL_CLASS_TITLE;
}

static int cogito_label_class_font_size(int cls, int fallback) {
  cogito_theme_init();
  if (cls > COGITO_LABEL_CLASS_NONE && cls < 4) {
    CogitoStyle* s = &cogito_theme.label_class[cls];
    if (s->has_font_size && s->font_size > 0) return s->font_size;
  }
  return fallback;
}

static bool cogito_label_class_bold(int cls, bool fallback) {
  cogito_theme_init();
  if (cls > COGITO_LABEL_CLASS_NONE && cls < 4) {
    CogitoStyle* s = &cogito_theme.label_class[cls];
    if (s->has_font_weight) return s->font_weight >= 600;
  }
  return fallback;
}

static CogitoColor cogito_label_class_color(int cls, CogitoColor fallback) {
  cogito_theme_init();
  if (cls > COGITO_LABEL_CLASS_NONE && cls < 4) {
    CogitoStyle* s = &cogito_theme.label_class[cls];
    if (s->has_text) return s->text;
  }
  return fallback;
}

static int cogito_css_hex_value(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
  if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
  return -1;
}

static void cogito_css_skip_ws(const char** p) {
  while (**p) {
    if (isspace((unsigned char)**p)) { (*p)++; continue; }
    if ((*p)[0] == '/' && (*p)[1] == '/') {
      (*p) += 2;
      while (**p && **p != '\n') (*p)++;
      continue;
    }
    if ((*p)[0] == '/' && (*p)[1] == '*') {
      (*p) += 2;
      while (**p && !((*p)[0] == '*' && (*p)[1] == '/')) (*p)++;
      if (**p) (*p) += 2;
      continue;
    }
    break;
  }
}

static bool cogito_css_read_ident(const char** p, char* out, size_t cap) {
  size_t len = 0;
  if (!isalpha((unsigned char)**p) && **p != '_' && **p != '-' && **p != '.' && **p != '#') return false;
  while (**p && (isalnum((unsigned char)**p) || **p == '_' || **p == '-' || **p == '.' || **p == '#')) {
    if (len + 1 < cap) out[len++] = **p;
    (*p)++;
  }
  out[len] = 0;
  return len > 0;
}

static bool cogito_css_read_number(const char** p, double* out) {
  char* end = NULL;
  double v = strtod(*p, &end);
  if (end == *p) return false;
  *p = end;
  if ((*p)[0] == 'p' && (*p)[1] == 'x') (*p) += 2;
  *out = v;
  return true;
}

static bool cogito_css_parse_hex_color(const char** p, CogitoColor* out) {
  if (**p != '#') return false;
  const char* s = *p + 1;
  int len = 0;
  while (cogito_css_hex_value(s[len]) >= 0) len++;
  if (len != 3 && len != 4 && len != 6 && len != 8) return false;
  int r = 0, g = 0, b = 0, a = 255;
  if (len == 3 || len == 4) {
    r = cogito_css_hex_value(s[0]) * 17;
    g = cogito_css_hex_value(s[1]) * 17;
    b = cogito_css_hex_value(s[2]) * 17;
    if (len == 4) a = cogito_css_hex_value(s[3]) * 17;
  } else if (len == 6 || len == 8) {
    r = cogito_css_hex_value(s[0]) * 16 + cogito_css_hex_value(s[1]);
    g = cogito_css_hex_value(s[2]) * 16 + cogito_css_hex_value(s[3]);
    b = cogito_css_hex_value(s[4]) * 16 + cogito_css_hex_value(s[5]);
    if (len == 8) a = cogito_css_hex_value(s[6]) * 16 + cogito_css_hex_value(s[7]);
  }
  *out = cogito_rgba(r, g, b, a);
  *p += 1 + len;
  return true;
}

static bool cogito_css_parse_named_color(const char* name, CogitoColor* out) {
  if (!name) return false;
  if (strcmp(name, "transparent") == 0) { *out = cogito_rgba(0, 0, 0, 0); return true; }
  if (strcmp(name, "white") == 0) { *out = cogito_rgba(255, 255, 255, 255); return true; }
  if (strcmp(name, "black") == 0) { *out = cogito_rgba(0, 0, 0, 255); return true; }
  return false;
}

static bool cogito_css_parse_rgb_color(const char** p, CogitoColor* out) {
  const char* s = *p;
  bool has_alpha = false;
  if (strncmp(s, "rgba", 4) == 0) {
    has_alpha = true;
    s += 4;
  } else if (strncmp(s, "rgb", 3) == 0) {
    s += 3;
  } else {
    return false;
  }
  cogito_css_skip_ws(&s);
  if (*s != '(') return false;
  s++;
  double vals[4] = {0, 0, 0, 1};
  int count = 0;
  while (*s && count < (has_alpha ? 4 : 3)) {
    cogito_css_skip_ws(&s);
    double v = 0;
    if (!cogito_css_read_number(&s, &v)) break;
    vals[count++] = v;
    cogito_css_skip_ws(&s);
    if (*s == ',') s++;
  }
  cogito_css_skip_ws(&s);
  if (*s != ')') return false;
  s++;
  if (count < (has_alpha ? 4 : 3)) return false;
  int r = (int)lround(vals[0]);
  int g = (int)lround(vals[1]);
  int b = (int)lround(vals[2]);
  int a = 255;
  if (has_alpha) {
    if (vals[3] <= 1.0) a = (int)lround(vals[3] * 255.0);
    else a = (int)lround(vals[3]);
  }
  *out = cogito_rgba(r, g, b, a);
  *p = s;
  return true;
}

static bool cogito_css_parse_color(const char** p, CogitoColor* out) {
  cogito_css_skip_ws(p);
  if (**p == '#') return cogito_css_parse_hex_color(p, out);
  if (strncmp(*p, "rgb", 3) == 0) return cogito_css_parse_rgb_color(p, out);
  char name[32];
  const char* s = *p;
  if (!cogito_css_read_ident(&s, name, sizeof(name))) return false;
  if (cogito_css_parse_named_color(name, out)) { *p = s; return true; }
  return false;
}

static int cogito_css_parse_ints(const char** p, int* out, int max) {
  int count = 0;
  while (count < max) {
    cogito_css_skip_ws(p);
    double v = 0;
    if (!cogito_css_read_number(p, &v)) break;
    out[count++] = (int)lround(v);
    cogito_css_skip_ws(p);
    if (**p == ',') (*p)++;
  }
  return count;
}
