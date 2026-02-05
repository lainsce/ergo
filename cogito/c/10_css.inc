
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

static void cogito_css_apply_property(CogitoStyle* style, const char* prop, const char** p, bool selection_mode) {
  if (selection_mode) {
    if (strcmp(prop, "background") == 0 || strcmp(prop, "background-color") == 0 ||
        strcmp(prop, "selection-color") == 0 || strcmp(prop, "selection-background") == 0 ||
        strcmp(prop, "highlight-color") == 0) {
      Color c;
      if (cogito_css_parse_color(p, &c)) { style->has_selection = true; style->selection = c; }
      return;
    }
  } else if (strcmp(prop, "selection-color") == 0 || strcmp(prop, "selection-background") == 0 || strcmp(prop, "highlight-color") == 0) {
    return;
  }
  if (strcmp(prop, "background") == 0 || strcmp(prop, "background-color") == 0) {
    Color c;
    if (cogito_css_parse_color(p, &c)) { style->has_bg = true; style->bg = c; }
    return;
  }
  if (strcmp(prop, "icon-size") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_icon_size = true; style->icon_size = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "icon-color") == 0 || strcmp(prop, "icon-tint") == 0) {
    Color c;
    if (cogito_css_parse_color(p, &c)) { style->has_icon_color = true; style->icon_color = c; }
    return;
  }
  if (strcmp(prop, "transition") == 0) {
    double v = 0.0;
    if (cogito_css_read_number(p, &v)) {
      int ms = (int)lround(v);
      if (strstr(*p, "s")) {
        ms = (int)lround(v * 1000.0);
      }
      style->has_transition_ms = true;
      style->transition_ms = ms;
    }
    char ease[32] = {0};
    cogito_css_read_ident(p, ease, sizeof(ease));
    if (ease[0]) {
      style->has_transition_ease = true;
      if (strcmp(ease, "ease-in") == 0) style->transition_ease = 2;
      else if (strcmp(ease, "ease-out") == 0) style->transition_ease = 3;
      else if (strcmp(ease, "ease-in-out") == 0) style->transition_ease = 4;
      else if (strcmp(ease, "ease") == 0) style->transition_ease = 1;
      else style->transition_ease = 0;
    }
    return;
  }
  if (strcmp(prop, "transition-duration") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) {
      int ms = (int)lround(v);
      if (strstr(*p, "s")) ms = (int)lround(v * 1000.0);
      style->has_transition_ms = true;
      style->transition_ms = ms;
    }
    return;
  }
  if (strcmp(prop, "transition-easing") == 0 || strcmp(prop, "transition-timing-function") == 0) {
    char ease[32] = {0};
    cogito_css_read_ident(p, ease, sizeof(ease));
    if (ease[0]) {
      style->has_transition_ease = true;
      if (strcmp(ease, "ease-in") == 0) style->transition_ease = 2;
      else if (strcmp(ease, "ease-out") == 0) style->transition_ease = 3;
      else if (strcmp(ease, "ease-in-out") == 0) style->transition_ease = 4;
      else if (strcmp(ease, "ease") == 0) style->transition_ease = 1;
      else style->transition_ease = 0;
    }
    return;
  }
  if (strcmp(prop, "color") == 0 || strcmp(prop, "text-color") == 0) {
    Color c;
    if (cogito_css_parse_color(p, &c)) { style->has_text = true; style->text = c; }
    return;
  }
  if (strcmp(prop, "border-color") == 0) {
    Color c;
    if (cogito_css_parse_color(p, &c)) { style->has_border = true; style->border = c; }
    return;
  }
  if (strcmp(prop, "border-width") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_border_width = true; style->border_width = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "border-radius") == 0 || strcmp(prop, "radius") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_radius = true; style->radius = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "font-size") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_font_size = true; style->font_size = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "letter-spacing") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_letter_spacing = true; style->letter_spacing = (float)v; }
    return;
  }
  if (strcmp(prop, "min-width") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_min_w = true; style->min_w = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "min-height") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_min_h = true; style->min_h = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "max-width") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_max_w = true; style->max_w = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "max-height") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_max_h = true; style->max_h = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "appbar-btn-size") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_appbar_btn_size = true; style->appbar_btn_size = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "appbar-btn-gap") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_appbar_btn_gap = true; style->appbar_btn_gap = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "appbar-btn-top") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_appbar_btn_pad_top = true; style->appbar_btn_pad_top = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "appbar-btn-right") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_appbar_btn_pad_right = true; style->appbar_btn_pad_right = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "appbar-btn-close-color") == 0) {
    Color c;
    if (cogito_css_parse_color(p, &c)) { style->has_appbar_btn_close_color = true; style->appbar_btn_close_color = c; }
    return;
  }
  if (strcmp(prop, "appbar-btn-min-color") == 0) {
    Color c;
    if (cogito_css_parse_color(p, &c)) { style->has_appbar_btn_min_color = true; style->appbar_btn_min_color = c; }
    return;
  }
  if (strcmp(prop, "appbar-btn-max-color") == 0) {
    Color c;
    if (cogito_css_parse_color(p, &c)) { style->has_appbar_btn_max_color = true; style->appbar_btn_max_color = c; }
    return;
  }
  if (strcmp(prop, "appbar-btn-border-color") == 0) {
    Color c;
    if (cogito_css_parse_color(p, &c)) { style->has_appbar_btn_border_color = true; style->appbar_btn_border_color = c; }
    return;
  }
  if (strcmp(prop, "appbar-btn-border-width") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_appbar_btn_border_width = true; style->appbar_btn_border_width = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "font-weight") == 0) {
    double v = 0;
    const char* s = *p;
    char ident[32];
    if (cogito_css_read_ident(&s, ident, sizeof(ident))) {
      if (strcmp(ident, "bold") == 0) { style->has_font_weight = true; style->font_weight = 700; *p = s; return; }
      if (strcmp(ident, "normal") == 0) { style->has_font_weight = true; style->font_weight = 400; *p = s; return; }
    }
    if (cogito_css_read_number(p, &v)) { style->has_font_weight = true; style->font_weight = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "font-variant-numeric") == 0) {
    const char* s = *p;
    char ident[64];
    bool tabular = false;
    while (cogito_css_read_ident(&s, ident, sizeof(ident))) {
      if (strcmp(ident, "tabular-nums") == 0 || strcmp(ident, "tnum") == 0) {
        tabular = true;
      }
      while (*s && (isspace((unsigned char)*s) || *s == ',')) s++;
    }
    style->has_font_tabular = true;
    style->font_tabular = tabular;
    *p = s;
    return;
  }
  if (strcmp(prop, "font-family") == 0) {
    const char* s = *p;
    char ident[64];
    bool mono = false;
    while (cogito_css_read_ident(&s, ident, sizeof(ident))) {
      if (strcmp(ident, "monospace") == 0 || strcmp(ident, "mono") == 0) {
        mono = true;
      }
      while (*s && (isspace((unsigned char)*s) || *s == ',')) s++;
    }
    style->has_font_mono = true;
    style->font_mono = mono;
    *p = s;
    return;
  }
  if (strcmp(prop, "padding") == 0) {
    int vals[4] = {0, 0, 0, 0};
    int count = cogito_css_parse_ints(p, vals, 4);
    if (count == 1) {
      cogito_style_set_padding(style, vals[0], vals[0], vals[0], vals[0]);
    } else if (count == 2) {
      cogito_style_set_padding(style, vals[0], vals[1], vals[0], vals[1]);
    } else if (count == 3) {
      cogito_style_set_padding(style, vals[0], vals[1], vals[2], vals[1]);
    } else if (count >= 4) {
      cogito_style_set_padding(style, vals[0], vals[1], vals[2], vals[3]);
    }
    return;
  }
  if (strcmp(prop, "padding-left") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_padding_left = true; style->padding_left = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "padding-top") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_padding_top = true; style->padding_top = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "padding-right") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_padding_right = true; style->padding_right = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "padding-bottom") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_padding_bottom = true; style->padding_bottom = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "margin") == 0) {
    int vals[4] = {0, 0, 0, 0};
    int count = cogito_css_parse_ints(p, vals, 4);
    if (count == 1) {
      cogito_style_set_margin(style, vals[0], vals[0], vals[0], vals[0]);
    } else if (count == 2) {
      cogito_style_set_margin(style, vals[0], vals[1], vals[0], vals[1]);
    } else if (count == 3) {
      cogito_style_set_margin(style, vals[0], vals[1], vals[2], vals[1]);
    } else if (count >= 4) {
      cogito_style_set_margin(style, vals[0], vals[1], vals[2], vals[3]);
    }
    return;
  }
  if (strcmp(prop, "margin-left") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_margin_left = true; style->margin_left = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "margin-top") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_margin_top = true; style->margin_top = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "margin-right") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_margin_right = true; style->margin_right = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "margin-bottom") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_margin_bottom = true; style->margin_bottom = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "item-padding") == 0 || strcmp(prop, "menu-item-padding") == 0) {
    int vals[4] = {0, 0, 0, 0};
    int count = cogito_css_parse_ints(p, vals, 4);
    if (count == 1) {
      style->has_padding = true;
      style->padding_left = vals[0];
      style->padding_top = vals[0];
      style->padding_right = vals[0];
      style->padding_bottom = vals[0];
    } else if (count == 2) {
      style->has_padding = true;
      style->padding_top = vals[0];
      style->padding_bottom = vals[0];
      style->padding_left = vals[1];
      style->padding_right = vals[1];
    } else if (count == 3) {
      style->has_padding = true;
      style->padding_top = vals[0];
      style->padding_left = vals[1];
      style->padding_right = vals[1];
      style->padding_bottom = vals[2];
    } else if (count >= 4) {
      style->has_padding = true;
      style->padding_top = vals[0];
      style->padding_right = vals[1];
      style->padding_bottom = vals[2];
      style->padding_left = vals[3];
    }
    return;
  }
  if (strcmp(prop, "item-height") == 0 || strcmp(prop, "menu-item-height") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_margin_top = true; style->margin_top = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "box-shadow") == 0 || strcmp(prop, "elevation") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) {
      int level = (int)lround(v);
      if (level < 0) level = 0;
      if (level > 5) level = 5;
      style->has_shadow = true;
      style->shadow_level = level;
    }
    return;
  }
  if (strcmp(prop, "track-color") == 0 || strcmp(prop, "track") == 0) {
    Color c;
    if (cogito_css_parse_color(p, &c)) { style->has_track = true; style->track = c; }
    return;
  }
  if (strcmp(prop, "knob-color") == 0 || strcmp(prop, "knob") == 0) {
    Color c;
    if (cogito_css_parse_color(p, &c)) { style->has_knob = true; style->knob = c; }
    return;
  }
  if (strcmp(prop, "knob-width") == 0 || strcmp(prop, "knob-w") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_min_w = true; style->min_w = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "knob-height") == 0 || strcmp(prop, "knob-h") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) { style->has_min_h = true; style->min_h = (int)lround(v); }
    return;
  }
  if (strcmp(prop, "check-color") == 0 || strcmp(prop, "check") == 0) {
    Color c;
    if (cogito_css_parse_color(p, &c)) { style->has_check = true; style->check = c; }
    return;
  }
}

static int cogito_label_class_from_name(const char* s) {
  if (!s || !s[0]) return COGITO_LABEL_CLASS_NONE;
  if (strcmp(s, "title") == 0) return COGITO_LABEL_CLASS_TITLE;
  if (strcmp(s, "subtitle") == 0 || strcmp(s, "sub-title") == 0) return COGITO_LABEL_CLASS_SUBTITLE;
  if (strcmp(s, "body") == 0) return COGITO_LABEL_CLASS_BODY;
  return COGITO_LABEL_CLASS_NONE;
}

static bool cogito_css_selector_kind(const char* sel, CogitoKind* out_kind, bool* out_base, int* out_label_class, bool* out_menu, int* out_class) {
  if (!sel || !sel[0]) return false;
  char buf[64];
  size_t n = strlen(sel);
  if (n >= sizeof(buf)) n = sizeof(buf) - 1;
  memcpy(buf, sel, n);
  buf[n] = 0;
  char* class_part = NULL;
  if (buf[0] == '.') {
    class_part = buf + 1;
    buf[0] = 0;
  } else {
    char* dot = strchr(buf, '.');
    if (dot) {
      *dot = 0;
      class_part = dot + 1;
    }
  }
  const char* s = buf;
  if (*s == '#') s++;
  if (strcmp(s, "*") == 0 || strcmp(s, "all") == 0 || strcmp(s, "base") == 0 || strcmp(s, "root") == 0) {
    if (out_base) *out_base = true;
    return true;
  }
  if (strcmp(s, "menu") == 0) {
    if (out_menu) *out_menu = true;
    return true;
  }
  if (class_part) {
    if (strcmp(class_part, "monospace") == 0 || strcmp(class_part, "mono") == 0) {
      if (out_class) *out_class = COGITO_STYLE_CLASS_MONO;
      return true;
    }
    if (strcmp(class_part, "tabular") == 0 || strcmp(class_part, "tnum") == 0) {
      if (out_class) *out_class = COGITO_STYLE_CLASS_TABULAR;
      return true;
    }
    int cls = cogito_label_class_from_name(class_part);
    if (cls != COGITO_LABEL_CLASS_NONE) {
      if (s[0] == 0 || strcmp(s, "label") == 0) {
        if (out_label_class) *out_label_class = cls;
        if (out_kind) *out_kind = COGITO_LABEL;
        return true;
      }
    }
    if (s[0] == 0) {
      if (strcmp(class_part, "window") == 0) { if (out_kind) *out_kind = COGITO_WINDOW; return true; }
      if (strcmp(class_part, "vstack") == 0) { if (out_kind) *out_kind = COGITO_VSTACK; return true; }
      if (strcmp(class_part, "hstack") == 0) { if (out_kind) *out_kind = COGITO_HSTACK; return true; }
      if (strcmp(class_part, "zstack") == 0) { if (out_kind) *out_kind = COGITO_ZSTACK; return true; }
      if (strcmp(class_part, "fixed") == 0) { if (out_kind) *out_kind = COGITO_FIXED; return true; }
      if (strcmp(class_part, "scroller") == 0 || strcmp(class_part, "scroll") == 0) { if (out_kind) *out_kind = COGITO_SCROLLER; return true; }
      if (strcmp(class_part, "textfield") == 0 || strcmp(class_part, "text-field") == 0) { if (out_kind) *out_kind = COGITO_TEXTFIELD; return true; }
      if (strcmp(class_part, "searchfield") == 0 || strcmp(class_part, "search-field") == 0) { if (out_kind) *out_kind = COGITO_SEARCHFIELD; return true; }
      if (strcmp(class_part, "textview") == 0 || strcmp(class_part, "text-view") == 0) { if (out_kind) *out_kind = COGITO_TEXTVIEW; return true; }
      if (strcmp(class_part, "dropdown") == 0 || strcmp(class_part, "select") == 0) { if (out_kind) *out_kind = COGITO_DROPDOWN; return true; }
      if (strcmp(class_part, "date-picker") == 0 || strcmp(class_part, "datepicker") == 0) { if (out_kind) *out_kind = COGITO_DATEPICKER; return true; }
      if (strcmp(class_part, "stepper") == 0) { if (out_kind) *out_kind = COGITO_STEPPER; return true; }
      if (strcmp(class_part, "slider") == 0) { if (out_kind) *out_kind = COGITO_SLIDER; return true; }
      if (strcmp(class_part, "tabs") == 0) { if (out_kind) *out_kind = COGITO_TABS; return true; }
      if (strcmp(class_part, "segmented") == 0 || strcmp(class_part, "segmented-control") == 0) { if (out_kind) *out_kind = COGITO_SEGMENTED; return true; }
      if (strcmp(class_part, "view-switcher") == 0 || strcmp(class_part, "viewswitcher") == 0) { if (out_kind) *out_kind = COGITO_VIEWSWITCHER; return true; }
      if (strcmp(class_part, "progress") == 0) { if (out_kind) *out_kind = COGITO_PROGRESS; return true; }
      if (strcmp(class_part, "tree") == 0 || strcmp(class_part, "treeview") == 0) { if (out_kind) *out_kind = COGITO_TREEVIEW; return true; }
      if (strcmp(class_part, "color") == 0 || strcmp(class_part, "colorpicker") == 0 || strcmp(class_part, "color-picker") == 0) { if (out_kind) *out_kind = COGITO_COLORPICKER; return true; }
      if (strcmp(class_part, "toasts") == 0) { if (out_kind) *out_kind = COGITO_TOASTS; return true; }
      if (strcmp(class_part, "toast") == 0) { if (out_kind) *out_kind = COGITO_TOAST; return true; }
      if (strcmp(class_part, "tooltip") == 0) { if (out_kind) *out_kind = COGITO_TOOLTIP; return true; }
      if (strcmp(class_part, "toolbar") == 0 || strcmp(class_part, "bottom-toolbar") == 0) { if (out_kind) *out_kind = COGITO_TOOLBAR; return true; }
      if (strcmp(class_part, "button") == 0) { if (out_kind) *out_kind = COGITO_BUTTON; return true; }
      if (strcmp(class_part, "iconbtn") == 0 || strcmp(class_part, "icon-button") == 0) { if (out_kind) *out_kind = COGITO_ICONBTN; return true; }
      if (strcmp(class_part, "image") == 0 || strcmp(class_part, "img") == 0) { if (out_kind) *out_kind = COGITO_IMAGE; return true; }
      if (strcmp(class_part, "label") == 0) { if (out_kind) *out_kind = COGITO_LABEL; return true; }
      if (strcmp(class_part, "checkbox") == 0 || strcmp(class_part, "radio") == 0) { if (out_kind) *out_kind = COGITO_CHECKBOX; return true; }
      if (strcmp(class_part, "switch") == 0) { if (out_kind) *out_kind = COGITO_SWITCH; return true; }
      if (strcmp(class_part, "list") == 0) { if (out_kind) *out_kind = COGITO_LIST; return true; }
      if (strcmp(class_part, "grid") == 0) { if (out_kind) *out_kind = COGITO_GRID; return true; }
      if (strcmp(class_part, "appbar") == 0 || strcmp(class_part, "app-bar") == 0) { if (out_kind) *out_kind = COGITO_APPBAR; return true; }
      if (strcmp(class_part, "menu") == 0) { if (out_menu) *out_menu = true; return true; }
    }
    if (strcmp(class_part, "checkbox") == 0 || strcmp(class_part, "radio") == 0) {
      if (s[0] == 0 || strcmp(s, "checkbox") == 0) {
        if (out_kind) *out_kind = COGITO_CHECKBOX;
        return true;
      }
    }
    if (strcmp(class_part, "switch") == 0) {
      if (s[0] == 0 || strcmp(s, "switch") == 0) {
        if (out_kind) *out_kind = COGITO_SWITCH;
        return true;
      }
    }
  }
  if (strcmp(s, "window") == 0) { if (out_kind) *out_kind = COGITO_WINDOW; return true; }
  if (strcmp(s, "vstack") == 0) { if (out_kind) *out_kind = COGITO_VSTACK; return true; }
  if (strcmp(s, "hstack") == 0) { if (out_kind) *out_kind = COGITO_HSTACK; return true; }
  if (strcmp(s, "zstack") == 0) { if (out_kind) *out_kind = COGITO_ZSTACK; return true; }
  if (strcmp(s, "fixed") == 0) { if (out_kind) *out_kind = COGITO_FIXED; return true; }
  if (strcmp(s, "scroller") == 0 || strcmp(s, "scroll") == 0) { if (out_kind) *out_kind = COGITO_SCROLLER; return true; }
  if (strcmp(s, "textfield") == 0 || strcmp(s, "text-field") == 0) { if (out_kind) *out_kind = COGITO_TEXTFIELD; return true; }
  if (strcmp(s, "searchfield") == 0 || strcmp(s, "search-field") == 0) { if (out_kind) *out_kind = COGITO_SEARCHFIELD; return true; }
  if (strcmp(s, "textview") == 0 || strcmp(s, "text-view") == 0) { if (out_kind) *out_kind = COGITO_TEXTVIEW; return true; }
  if (strcmp(s, "dropdown") == 0 || strcmp(s, "select") == 0) { if (out_kind) *out_kind = COGITO_DROPDOWN; return true; }
  if (strcmp(s, "date-picker") == 0 || strcmp(s, "datepicker") == 0) { if (out_kind) *out_kind = COGITO_DATEPICKER; return true; }
  if (strcmp(s, "stepper") == 0) { if (out_kind) *out_kind = COGITO_STEPPER; return true; }
  if (strcmp(s, "slider") == 0) { if (out_kind) *out_kind = COGITO_SLIDER; return true; }
  if (strcmp(s, "tabs") == 0) { if (out_kind) *out_kind = COGITO_TABS; return true; }
  if (strcmp(s, "segmented") == 0 || strcmp(s, "segmented-control") == 0) { if (out_kind) *out_kind = COGITO_SEGMENTED; return true; }
  if (strcmp(s, "view-switcher") == 0 || strcmp(s, "viewswitcher") == 0) { if (out_kind) *out_kind = COGITO_VIEWSWITCHER; return true; }
  if (strcmp(s, "progress") == 0) { if (out_kind) *out_kind = COGITO_PROGRESS; return true; }
  if (strcmp(s, "tree") == 0 || strcmp(s, "treeview") == 0) { if (out_kind) *out_kind = COGITO_TREEVIEW; return true; }
  if (strcmp(s, "color") == 0 || strcmp(s, "colorpicker") == 0 || strcmp(s, "color-picker") == 0) { if (out_kind) *out_kind = COGITO_COLORPICKER; return true; }
  if (strcmp(s, "toasts") == 0) { if (out_kind) *out_kind = COGITO_TOASTS; return true; }
  if (strcmp(s, "toast") == 0) { if (out_kind) *out_kind = COGITO_TOAST; return true; }
  if (strcmp(s, "tooltip") == 0) { if (out_kind) *out_kind = COGITO_TOOLTIP; return true; }
  if (strcmp(s, "toolbar") == 0 || strcmp(s, "bottom-toolbar") == 0) { if (out_kind) *out_kind = COGITO_TOOLBAR; return true; }
  if (strcmp(s, "button") == 0) { if (out_kind) *out_kind = COGITO_BUTTON; return true; }
  if (strcmp(s, "iconbtn") == 0 || strcmp(s, "icon-button") == 0) { if (out_kind) *out_kind = COGITO_ICONBTN; return true; }
  if (strcmp(s, "image") == 0 || strcmp(s, "img") == 0) { if (out_kind) *out_kind = COGITO_IMAGE; return true; }
  if (strcmp(s, "label") == 0) { if (out_kind) *out_kind = COGITO_LABEL; return true; }
  if (strcmp(s, "checkbox") == 0) { if (out_kind) *out_kind = COGITO_CHECKBOX; return true; }
  if (strcmp(s, "switch") == 0) { if (out_kind) *out_kind = COGITO_SWITCH; return true; }
  if (strcmp(s, "list") == 0) { if (out_kind) *out_kind = COGITO_LIST; return true; }
  if (strcmp(s, "grid") == 0) { if (out_kind) *out_kind = COGITO_GRID; return true; }
  if (strcmp(s, "dialog") == 0) { if (out_kind) *out_kind = COGITO_DIALOG; return true; }
  if (strcmp(s, "dialog-slot") == 0 || strcmp(s, "dialogslot") == 0) { if (out_kind) *out_kind = COGITO_DIALOG_SLOT; return true; }
  if (strcmp(s, "appbar") == 0 || strcmp(s, "app-bar") == 0) { if (out_kind) *out_kind = COGITO_APPBAR; return true; }
  return false;
}

static bool cogito_css_parse_token(const char* tok, char* base, size_t base_cap, char* state, size_t state_cap) {
  if (!tok || !tok[0]) return false;
  while (*tok == '.' || *tok == '#') tok++;
  if (!*tok) return false;
  char buf[64];
  size_t len = strlen(tok);
  if (len >= sizeof(buf)) len = sizeof(buf) - 1;
  memcpy(buf, tok, len);
  buf[len] = 0;
  char* state_ptr = NULL;
  char* dot = strchr(buf, '.');
  char* colon = strchr(buf, ':');
  if (dot && colon) state_ptr = dot < colon ? dot : colon;
  else if (dot) state_ptr = dot;
  else if (colon) state_ptr = colon;
  if (state_ptr) {
    *state_ptr = 0;
    state_ptr++;
  }
  if (base && base_cap) {
    strncpy(base, buf, base_cap - 1);
    base[base_cap - 1] = 0;
  }
  if (state && state_cap) {
    state[0] = 0;
    if (state_ptr && state_ptr[0]) {
      strncpy(state, state_ptr, state_cap - 1);
      state[state_cap - 1] = 0;
    }
  }
  return true;
}

static bool cogito_css_selector_has_class(const char* sel, const char* name) {
  if (!sel || !name || !name[0]) return false;
  const char* p = sel;
  while (*p) {
    if (*p == '.') {
      p++;
      const char* start = p;
      while (*p && *p != '.' && *p != ':' && *p != '#' && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r' && *p != ',') p++;
      size_t len = (size_t)(p - start);
      if (len > 0 && strlen(name) == len && strncmp(start, name, len) == 0) return true;
    } else {
      p++;
    }
  }
  return false;
}

static void cogito_css_add_styles(const char* parent, const char* child, CogitoStyle** styles, int* flags, int* count, int max) {
  if (!count || !styles || max <= 0) return;
  if (!child || !child[0]) {
    bool is_base = false;
    bool is_menu = false;
    CogitoKind kind = COGITO_WINDOW;
    int label_class = COGITO_LABEL_CLASS_NONE;
    int class_kind = COGITO_STYLE_CLASS_NONE;
    char buf[64];
    char state[16] = {0};
    const char* sel = parent;
    if (parent) {
      size_t n = strlen(parent);
      if (n >= sizeof(buf)) n = sizeof(buf) - 1;
      memcpy(buf, parent, n);
      buf[n] = 0;
      char* colon = strchr(buf, ':');
      if (colon) {
        *colon = 0;
        strncpy(state, colon + 1, sizeof(state) - 1);
        state[sizeof(state) - 1] = 0;
        cogito_trim(state);
      }
      sel = buf;
    }
    if (cogito_css_selector_kind(sel, &kind, &is_base, &label_class, &is_menu, &class_kind)) {
      CogitoStyle* style = NULL;
      bool hover = strcmp(state, "hover") == 0;
      bool active = strcmp(state, "active") == 0;
      bool checked = strcmp(state, "checked") == 0;
      bool disabled = strcmp(state, "disabled") == 0;
      if (class_kind != COGITO_STYLE_CLASS_NONE) {
        if (class_kind == COGITO_STYLE_CLASS_MONO) {
          if (checked) style = &cogito_theme.class_mono_checked;
          else if (disabled) style = &cogito_theme.class_mono_disabled;
          else if (active) style = &cogito_theme.class_mono_active;
          else if (hover) style = &cogito_theme.class_mono_hover;
          else style = &cogito_theme.class_mono;
        } else if (class_kind == COGITO_STYLE_CLASS_TABULAR) {
          if (checked) style = &cogito_theme.class_tabular_checked;
          else if (disabled) style = &cogito_theme.class_tabular_disabled;
          else if (active) style = &cogito_theme.class_tabular_active;
          else if (hover) style = &cogito_theme.class_tabular_hover;
          else style = &cogito_theme.class_tabular;
        }
      } else if (label_class != COGITO_LABEL_CLASS_NONE) {
        if (checked) style = &cogito_theme.label_class_checked[label_class];
        else if (disabled) style = &cogito_theme.label_class_disabled[label_class];
        else if (active) style = &cogito_theme.label_class_active[label_class];
        else if (hover) style = &cogito_theme.label_class_hover[label_class];
        else style = &cogito_theme.label_class[label_class];
      } else if (is_menu) {
        style = &cogito_theme.menu;
      } else {
        if (is_base) {
          style = &cogito_theme.base;
        } else if (checked) {
          style = &cogito_theme.per_kind_checked[kind];
        } else if (disabled) {
          style = &cogito_theme.per_kind_disabled[kind];
        } else if (active) {
          style = &cogito_theme.per_kind_active[kind];
        } else if (hover) {
          style = &cogito_theme.per_kind_hover[kind];
        } else {
          style = &cogito_theme.per_kind[kind];
        }
      }
      if (style && *count < max) {
        styles[(*count)] = style;
        if (flags && strcmp(state, "selection") == 0) flags[(*count)] = 1;
        (*count)++;
      }
    }
    return;
  }
  char parent_base[32] = {0};
  char child_base[32] = {0};
  char child_state[16] = {0};
  char parent_state[16] = {0};
  if (!cogito_css_parse_token(parent, parent_base, sizeof(parent_base), parent_state, sizeof(parent_state))) return;
  if (!cogito_css_parse_token(child, child_base, sizeof(child_base), child_state, sizeof(child_state))) return;
  bool hover = strcmp(child_state, "hover") == 0;
  bool active = strcmp(child_state, "active") == 0;
  bool checked_state = strcmp(parent_state, "checked") == 0;
  bool parent_radio = strcmp(parent_base, "radio") == 0 || cogito_css_selector_has_class(parent, "radio");
  bool parent_checkbox = strcmp(parent_base, "checkbox") == 0 || cogito_css_selector_has_class(parent, "checkbox");
  bool parent_switch = strcmp(parent_base, "switch") == 0 || cogito_css_selector_has_class(parent, "switch");
  if ((parent_checkbox || parent_radio) && strcmp(child_base, "check") == 0) {
    CogitoStyle* style = NULL;
    if (parent_radio) {
      if (checked_state) style = &cogito_theme.radio_check_checked;
      else if (hover) style = &cogito_theme.radio_check_hover;
      else if (active) style = &cogito_theme.radio_check_active;
      else style = &cogito_theme.radio_check;
    } else {
      if (checked_state) style = &cogito_theme.checkbox_check_checked;
      else if (hover) style = &cogito_theme.checkbox_check_hover;
      else if (active) style = &cogito_theme.checkbox_check_active;
      else style = &cogito_theme.checkbox_check;
    }
    if (*count < max) {
      styles[(*count)] = style;
      if (flags && strcmp(child_state, "selection") == 0) flags[(*count)] = 1;
      (*count)++;
    }
    return;
  }
  if ((parent_checkbox || parent_radio) && strcmp(child_base, "box") == 0) {
    CogitoStyle* style = NULL;
    if (parent_radio) {
      if (checked_state) style = &cogito_theme.radio_box_checked;
      else if (hover) style = &cogito_theme.radio_box_hover;
      else if (active) style = &cogito_theme.radio_box_active;
      else style = &cogito_theme.radio_box;
    } else {
      if (checked_state) style = &cogito_theme.checkbox_box_checked;
      else if (hover) style = &cogito_theme.checkbox_box_hover;
      else if (active) style = &cogito_theme.checkbox_box_active;
      else style = &cogito_theme.checkbox_box;
    }
    if (*count < max) {
      styles[(*count)] = style;
      if (flags && strcmp(child_state, "selection") == 0) flags[(*count)] = 1;
      (*count)++;
    }
    return;
  }
  if (parent_switch && strcmp(child_base, "track") == 0) {
    CogitoStyle* style = NULL;
    if (checked_state) style = &cogito_theme.switch_track_checked;
    else if (hover) style = &cogito_theme.switch_track_hover;
    else if (active) style = &cogito_theme.switch_track_active;
    else style = &cogito_theme.switch_track;
    if (*count < max) {
      styles[(*count)] = style;
      if (flags && strcmp(child_state, "selection") == 0) flags[(*count)] = 1;
      (*count)++;
    }
    return;
  }
  if (parent_switch && strcmp(child_base, "knob") == 0) {
    CogitoStyle* style = NULL;
    if (checked_state) style = &cogito_theme.switch_knob_checked;
    else if (hover) style = &cogito_theme.switch_knob_hover;
    else if (active) style = &cogito_theme.switch_knob_active;
    else style = &cogito_theme.switch_knob;
    if (*count < max) {
      styles[(*count)] = style;
      if (flags && strcmp(child_state, "selection") == 0) flags[(*count)] = 1;
      (*count)++;
    }
    return;
  }
  if (strcmp(parent_base, "menu") == 0) {
    if (strcmp(child_base, "item") == 0) {
      if (*count < max) {
        styles[(*count)] = &cogito_theme.menu_item;
        if (flags && strcmp(child_state, "selection") == 0) flags[(*count)] = 1;
        (*count)++;
      }
    }
    return;
  }
  if (strcmp(parent_base, "appbar") != 0) return;

  if (strcmp(child_base, "button") == 0) {
    CogitoStyle* style = NULL;
    if (hover) style = &cogito_theme.appbar_child_button_hover;
    else if (active) style = &cogito_theme.appbar_child_button_active;
    else style = &cogito_theme.appbar_child_button;
    if (*count < max) {
      styles[(*count)] = style;
      if (flags && strcmp(child_state, "selection") == 0) flags[(*count)] = 1;
      (*count)++;
    }
    return;
  }
  if (strcmp(child_base, "iconbtn") == 0 || strcmp(child_base, "icon-button") == 0) {
    CogitoStyle* style = NULL;
    if (hover) style = &cogito_theme.appbar_child_iconbtn_hover;
    else if (active) style = &cogito_theme.appbar_child_iconbtn_active;
    else style = &cogito_theme.appbar_child_iconbtn;
    if (*count < max) {
      styles[(*count)] = style;
      if (flags && strcmp(child_state, "selection") == 0) flags[(*count)] = 1;
      (*count)++;
    }
    return;
  }
  CogitoStyle* all = NULL;
  if (hover) all = &cogito_theme.appbar_win_btn_hover_all;
  else if (active) all = &cogito_theme.appbar_win_btn_active_all;
  else all = &cogito_theme.appbar_win_btn_all;

  if (strcmp(child_base, "window-button") == 0) {
    if (*count < max) {
      styles[(*count)] = all;
      if (flags && strcmp(child_state, "selection") == 0) flags[(*count)] = 1;
      (*count)++;
    }
    return;
  }
  int idx = -1;
  if (strcmp(child_base, "window-close") == 0) idx = 0;
  else if (strcmp(child_base, "window-min") == 0) idx = 1;
  else if (strcmp(child_base, "window-max") == 0) idx = 2;
  if (idx >= 0) {
    CogitoStyle* style = NULL;
    if (hover) style = &cogito_theme.appbar_win_btn_hover[idx];
    else if (active) style = &cogito_theme.appbar_win_btn_active[idx];
    else style = &cogito_theme.appbar_win_btn[idx];
    if (*count < max) {
      styles[(*count)] = style;
      if (flags && strcmp(child_state, "selection") == 0) flags[(*count)] = 1;
      (*count)++;
    }
    return;
  }
}

static void cogito_parse_css(const char* src) {
  const char* p = src;
  cogito_style_version++;
  while (*p) {
    cogito_css_skip_ws(&p);
    if (!*p) break;
    CogitoStyle* styles[16];
    int style_flags[16] = {0};
    int style_count = 0;
    while (*p) {
      char parent[64] = {0};
      char child[64] = {0};
      if (!cogito_css_read_selector(&p, parent, sizeof(parent))) {
        p++;
        break;
      }
      cogito_css_skip_ws(&p);
      if (*p && *p != '{' && *p != ',') {
        if (!cogito_css_read_selector(&p, child, sizeof(child))) {
          child[0] = 0;
        }
      }
      cogito_css_add_styles(parent, child[0] ? child : NULL, styles, style_flags, &style_count, (int)(sizeof(styles) / sizeof(styles[0])));
      cogito_css_skip_ws(&p);
      if (*p == ',') {
        p++;
        cogito_css_skip_ws(&p);
        continue;
      }
      break;
    }
    cogito_css_skip_ws(&p);
    if (*p != '{') {
      while (*p && *p != '{') p++;
      if (*p == '{') p++;
    } else {
      p++;
    }
    if (style_count == 0) {
      int depth = 1;
      while (*p && depth > 0) {
        if (*p == '{') depth++;
        else if (*p == '}') depth--;
        p++;
      }
      continue;
    }
    while (*p) {
      cogito_css_skip_ws(&p);
      if (*p == '}') { p++; break; }
      char prop[64];
      if (!cogito_css_read_ident(&p, prop, sizeof(prop))) { p++; continue; }
      cogito_css_skip_ws(&p);
      if (*p == ':') {
        p++;
      } else {
        while (*p && *p != ';' && *p != '}') p++;
        if (*p == ';') p++;
        continue;
      }
      const char* vp = p;
      for (int i = 0; i < style_count; i++) {
        const char* v = vp;
        cogito_css_apply_property(styles[i], prop, &v, style_flags[i] != 0);
      }
      while (*p && *p != ';' && *p != '}') p++;
      if (*p == ';') p++;
    }
  }
}

static char* cogito_read_file(const char* path, size_t* out_len) {
  if (!path || !path[0]) return NULL;
  FILE* f = fopen(path, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END);
  long len = ftell(f);
  if (len < 0) { fclose(f); return NULL; }
  fseek(f, 0, SEEK_SET);
  char* buf = (char*)malloc((size_t)len + 1);
  if (!buf) { fclose(f); return NULL; }
  size_t got = fread(buf, 1, (size_t)len, f);
  fclose(f);
  buf[got] = 0;
  if (out_len) *out_len = got;
  return buf;
}

static const char* cogito_default_css_path(void) {
  const char* stdlib = getenv("ERGO_STDLIB");
  if (stdlib && stdlib[0]) {
    static char buf[PATH_MAX];
    snprintf(buf, sizeof(buf), "%s/cogito_default.css", stdlib);
    if (cogito_file_exists(buf)) return buf;
  }
  if (cogito_file_exists("cogito/cogito_default.css")) return "cogito/cogito_default.css";
  if (cogito_file_exists("../cogito/cogito_default.css")) return "../cogito/cogito_default.css";
  return "cogito/cogito_default.css";
}

static const char* cogito_dark_css_path(void) {
  const char* stdlib = getenv("ERGO_STDLIB");
  if (stdlib && stdlib[0]) {
    static char buf[PATH_MAX];
    snprintf(buf, sizeof(buf), "%s/cogito_dark.css", stdlib);
    if (cogito_file_exists(buf)) return buf;
  }
  if (cogito_file_exists("cogito/cogito_dark.css")) return "cogito/cogito_dark.css";
  if (cogito_file_exists("../cogito/cogito_dark.css")) return "../cogito/cogito_dark.css";
  return "cogito/cogito_dark.css";
}

static void cogito_apply_css_inline(const char* src) {
  if (!src || !src[0]) return;
  cogito_parse_css(src);
}

static void cogito_apply_css_file_inline(const char* path) {
  size_t len = 0;
  char* src = cogito_read_file(path, &len);
  if (!src) return;
  cogito_parse_css(src);
  free(src);
}

static void cogito_apply_default_css(void) {
  size_t len = 0;
  const char* path = cogito_default_css_path();
  char* src = cogito_read_file(path, &len);
  if (!src) return;
  cogito_parse_css(src);
  free(src);
}

static void cogito_load_css_file(const char* path) {
  size_t len = 0;
  char* src = cogito_read_file(path, &len);
  if (!src) {
    if (cogito_debug_enabled()) {
      fprintf(stderr, "cogito: css not found: %s\n", path ? path : "");
      fflush(stderr);
    }
    return;
  }
  cogito_theme_initialized = true;
  cogito_theme_set_defaults();
  cogito_parse_css(src);
  free(src);
  extern CogitoNode* cogito_active_window;
  if (cogito_active_window) {
    cogito_apply_style_tree(cogito_active_window);
    if (cogito_active_window->auto_size) {
      int dw = 0;
      int dh = 0;
      cogito_window_desired_size(cogito_active_window, &dw, &dh);
      if (dw > 0 && dh > 0) {
        cogito_active_window->w = dw;
        cogito_active_window->h = dh;
        if (cogito_raylib_ready) SetWindowSize(dw, dh);
      }
    }
    cogito_window_relayout(cogito_active_window);
  }
}

static void cogito_load_css(ErgoVal pathv) {
  ErgoStr* ps = stdr_to_string(pathv);
  if (ps && ps->data) {
    size_t len = 0;
    char* src = cogito_read_file(ps->data, &len);
    if (src) {
      if (cogito_user_css) free(cogito_user_css);
      cogito_user_css = src;
      cogito_apply_theme(cogito_theme_dark, cogito_theme_accent);
    }
  }
  if (ps) ergo_release_val(EV_STR(ps));
}

static bool cogito_strcasestr(const char* hay, const char* needle) {
  if (!hay || !needle || !*needle) return false;
  size_t nlen = strlen(needle);
  for (const char* p = hay; *p; p++) {
    size_t i = 0;
    for (; i < nlen; i++) {
      char a = (char)tolower((unsigned char)p[i]);
      char b = (char)tolower((unsigned char)needle[i]);
      if (!p[i] || a != b) break;
    }
    if (i == nlen) return true;
  }
  return false;
}

static bool cogito_system_is_dark(void) {
#if defined(__APPLE__)
  Class udc = (Class)objc_getClass("NSUserDefaults");
  if (!udc) return false;
  SEL sel_std = sel_registerName("standardUserDefaults");
  id defaults = ((id(*)(Class, SEL))objc_msgSend)(udc, sel_std);
  if (!defaults) return false;
  SEL sel_str = sel_registerName("stringForKey:");
  id key = cogito_nsstring("AppleInterfaceStyle");
  id style = ((id(*)(id, SEL, id))objc_msgSend)(defaults, sel_str, key);
  if (!style) return false;
  SEL sel_utf8 = sel_registerName("UTF8String");
  const char* s = ((const char*(*)(id, SEL))objc_msgSend)(style, sel_utf8);
  return s && strcmp(s, "Dark") == 0;
#elif defined(_WIN32)
  DWORD val = 1;
  DWORD size = sizeof(val);
  if (RegGetValueA(HKEY_CURRENT_USER,
                   "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                   "AppsUseLightTheme",
                   RRF_RT_REG_DWORD, NULL, &val, &size) == ERROR_SUCCESS) {
    return val == 0;
  }
  return false;
#else
  const char* gtk = getenv("GTK_THEME");
  if (cogito_strcasestr(gtk, "dark")) return true;
  const char* gnome = getenv("GNOME_THEME");
  if (cogito_strcasestr(gnome, "dark")) return true;
  return false;
#endif
}

static bool cogito_system_accent(Color* out) {
#if defined(__APPLE__)
  // Prefer AppleHighlightColor (string) to avoid accent index mismatches.
  Class udc = (Class)objc_getClass("NSUserDefaults");
  if (!udc) return false;
  SEL sel_std = sel_registerName("standardUserDefaults");
  id defaults = ((id(*)(Class, SEL))objc_msgSend)(udc, sel_std);
  if (!defaults) return false;
  SEL sel_str = sel_registerName("stringForKey:");
  id key_hl = cogito_nsstring("AppleHighlightColor");
  id hl = ((id(*)(id, SEL, id))objc_msgSend)(defaults, sel_str, key_hl);
  if (hl) {
    SEL sel_utf8 = sel_registerName("UTF8String");
    const char* s = ((const char*(*)(id, SEL))objc_msgSend)(hl, sel_utf8);
    if (s && *s) {
      char* end = NULL;
      double r = strtod(s, &end);
      if (end && end != s) {
        double g = strtod(end, &end);
        if (end) {
          double b = strtod(end, &end);
          if (r >= 0.0 && g >= 0.0 && b >= 0.0) {
            if (r > 1.0) r = 1.0;
            if (g > 1.0) g = 1.0;
            if (b > 1.0) b = 1.0;
            *out = cogito_rgba((int)(r * 255.0 + 0.5),
                               (int)(g * 255.0 + 0.5),
                               (int)(b * 255.0 + 0.5),
                               255);
            return true;
          }
        }
      }
    }
  }
  SEL sel_obj = sel_registerName("objectForKey:");
  id key = cogito_nsstring("AppleAccentColor");
  id num = ((id(*)(id, SEL, id))objc_msgSend)(defaults, sel_obj, key);
  if (!num) return false;
  SEL sel_int = sel_registerName("integerValue");
  int v = (int)((long(*)(id, SEL))objc_msgSend)(num, sel_int);
  Color c = cogito_rgba(10, 132, 255, 255);
  switch (v) {
    case -1: c = cogito_rgba(10, 132, 255, 255); break;   // blue
    case 0: c = cogito_rgba(142, 142, 147, 255); break;   // graphite
    case 1: c = cogito_rgba(191, 90, 242, 255); break;    // purple
    case 2: c = cogito_rgba(255, 55, 95, 255); break;     // pink
    case 3: c = cogito_rgba(255, 59, 48, 255); break;     // red
    case 4: c = cogito_rgba(255, 159, 10, 255); break;    // orange
    case 5: c = cogito_rgba(255, 214, 10, 255); break;    // yellow
    case 6: c = cogito_rgba(50, 215, 75, 255); break;     // green
    default: break;
  }
  *out = c;
  return true;
#elif defined(_WIN32)
  DWORD val = 0;
  DWORD size = sizeof(val);
  if (RegGetValueA(HKEY_CURRENT_USER,
                   "Software\\Microsoft\\Windows\\DWM",
                   "ColorizationColor",
                   RRF_RT_REG_DWORD, NULL, &val, &size) == ERROR_SUCCESS) {
    Color c = cogito_rgba((val >> 16) & 0xFF, (val >> 8) & 0xFF, val & 0xFF, 255);
    *out = c;
    return true;
  }
  return false;
#else
  (void)out;
  return false;
#endif
}

static void cogito_css_append(char* dst, size_t dst_cap, size_t* dst_off, const char* fmt, ...) {
  if (!dst || !dst_off || *dst_off >= dst_cap) return;
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(dst + *dst_off, dst_cap - *dst_off, fmt, ap);
  va_end(ap);
  if (n < 0) return;
  if ((size_t)n >= dst_cap - *dst_off) {
    *dst_off = dst_cap - 1;
  } else {
    *dst_off += (size_t)n;
  }
}

static void cogito_build_dynamic_css(bool dark, Color accent, char* out, size_t cap) {
  size_t off = 0;
  if (cap == 0) return;
  out[0] = 0;
  double h = 0.0, c = 0.0, t = 0.0;
  cogito_rgb_to_hct(accent, &h, &c, &t);
  double c_primary = c < 24.0 ? 24.0 : c;
  double c_secondary = c_primary * 0.35;
  if (c_secondary < 8.0) c_secondary = 8.0;
  Color primary = accent;
  Color on_primary = cogito_on_color(primary);
  Color primary_container = dark
    ? cogito_mix(primary, cogito_rgba(0, 0, 0, 255), 0.45f)
    : cogito_mix(primary, cogito_rgba(255, 255, 255, 255), 0.65f);
  Color on_primary_container = cogito_on_color(primary_container);
  Color secondary = dark
    ? cogito_mix(primary, cogito_rgba(255, 255, 255, 255), 0.25f)
    : cogito_mix(primary, cogito_rgba(0, 0, 0, 255), 0.15f);
  Color background = dark ? (Color){20, 20, 22, 255} : (Color){245, 245, 248, 255};
  Color surface = dark ? (Color){28, 28, 30, 255} : (Color){252, 252, 254, 255};
  Color on_surface = dark ? (Color){240, 240, 244, 255} : (Color){20, 20, 22, 255};
  Color outline = dark ? (Color){90, 90, 94, 255} : (Color){200, 200, 204, 255};
  char p[8], op[8], pc[8], opc[8], sec[8], bg[8], surf[8], ons[8], outl[8];
  cogito_color_to_hex(primary, p);
  cogito_color_to_hex(on_primary, op);
  cogito_color_to_hex(primary_container, pc);
  cogito_color_to_hex(on_primary_container, opc);
  cogito_color_to_hex(secondary, sec);
  cogito_color_to_hex(background, bg);
  cogito_color_to_hex(surface, surf);
  cogito_color_to_hex(on_surface, ons);
  cogito_color_to_hex(outline, outl);
  // Order mirrors cogito_default.css (color-only overrides, same selectors).
  cogito_css_append(out, cap, &off, "* { color: %s; }\n", ons);
  cogito_css_append(out, cap, &off, "window { background: %s; }\n", bg);
  cogito_css_append(out, cap, &off, "button { background: %s; }\n", pc);
  cogito_css_append(out, cap, &off, "textfield, .textfield { background: %s; }\n", surf);
  cogito_css_append(out, cap, &off, "searchfield, .searchfield { background: %s; }\n", surf);
  cogito_css_append(out, cap, &off, "textview, .textview { background: %s; }\n", surf);
  cogito_css_append(out, cap, &off, "textfield:selection, .textfield:selection { background: %s; }\n", p);
  cogito_css_append(out, cap, &off, "textview:selection, .textview:selection { background: %s; }\n", p);
  cogito_css_append(out, cap, &off, "dropdown, .dropdown { background: %s; }\n", surf);
  cogito_css_append(out, cap, &off, "datepicker, .datepicker { background: %s; }\n", surf);
  cogito_css_append(out, cap, &off, "datepicker:selection, .datepicker:selection { background: %s; }\n", p);
  cogito_css_append(out, cap, &off, "slider, .slider { background: %s; color: %s; }\n", surf, ons);
  cogito_css_append(out, cap, &off, "stepper, .stepper { background: %s; }\n", surf);
  cogito_css_append(out, cap, &off, "tabs, .tabs { background: %s; }\n", surf);
  cogito_css_append(out, cap, &off, "tabs:selection { background: %s; }\n", pc);
  cogito_css_append(out, cap, &off, "segmented, .segmented { background: %s; }\n", surf);
  cogito_css_append(out, cap, &off, "view-switcher, .view-switcher { background: %s; }\n", surf);
  cogito_css_append(out, cap, &off, "progress, .progress { background: %s; }\n", surf);
  cogito_css_append(out, cap, &off, "progress:selection { background: %s; }\n", p);
  cogito_css_append(out, cap, &off, "toasts, .toasts { color: %s; }\n", ons);
  cogito_css_append(out, cap, &off, "toast, .toast { background: %s; color: %s; }\n", ons, bg);
  cogito_css_append(out, cap, &off, "treeview, .treeview { background: %s; }\n", surf);
  cogito_css_append(out, cap, &off, "colorpicker, .colorpicker { background: %s; }\n", surf);
  cogito_css_append(out, cap, &off, "colorpicker:selection, .colorpicker:selection { background: %s; }\n", p);
  cogito_css_append(out, cap, &off, "toolbar, .toolbar, .bottom-toolbar { background: %s; }\n", surf);
  cogito_css_append(out, cap, &off, "iconbtn { background: %s; }\n", pc);
  cogito_css_append(out, cap, &off, "label { color: %s; }\n", ons);
  cogito_css_append(out, cap, &off, ".tooltip { background: %s; color: %s; }\n", ons, bg);
  cogito_css_append(out, cap, &off, ".checkbox, .radio, checkbox { color: %s; }\n", ons);
  cogito_css_append(out, cap, &off, ".checkbox:checked,\n.radio:checked { background: %s; }\n", bg);
  cogito_css_append(out, cap, &off, ".checkbox .check,\n.radio .check { color: %s; }\n", ons);
  cogito_css_append(out, cap, &off, ".checkbox:checked .check,\n.radio:checked .check { color: %s; }\n", op);
  cogito_css_append(out, cap, &off, ".checkbox .box,\n.radio .box { background: %s; border-color: %s; }\n", surf, outl);
  cogito_css_append(out, cap, &off, ".checkbox:checked .box,\n.radio:checked .box { background: %s; }\n", p);
  cogito_css_append(out, cap, &off, ".checkbox .box:hover,\n.radio .box:hover { background: %s; }\n", surf);
  cogito_css_append(out, cap, &off, ".checkbox .box:active,\n.radio .box:active { background: %s; }\n", surf);
  cogito_css_append(out, cap, &off, ".checkbox .check:hover,\n.radio .check:hover { color: %s; }\n", ons);
  cogito_css_append(out, cap, &off, ".checkbox .check:active,\n.radio .check:active { color: %s; }\n", ons);
  cogito_css_append(out, cap, &off, ".switch, switch { color: %s; }\n", ons);
  cogito_css_append(out, cap, &off, ".switch .track, switch track { background: %s; }\n", outl);
  cogito_css_append(out, cap, &off, ".switch:checked .track, switch:checked track { background: %s; }\n", p);
  cogito_css_append(out, cap, &off, ".switch .track:hover, switch track:hover { background: %s; }\n", outl);
  cogito_css_append(out, cap, &off, ".switch .track:active, switch track:active { background: %s; }\n", outl);
  cogito_css_append(out, cap, &off, ".switch .knob, switch knob { background: %s; }\n", surf);
  cogito_css_append(out, cap, &off, ".switch .knob:hover, switch knob:hover { background: %s; }\n", surf);
  cogito_css_append(out, cap, &off, ".switch .knob:active, switch knob:active { background: %s; }\n", surf);
  cogito_css_append(out, cap, &off, ".monospace { color: %s; }\n", ons);
  cogito_css_append(out, cap, &off, ".tabular { color: %s; }\n", ons);
  cogito_css_append(out, cap, &off, ".title { color: %s; }\n", ons);
  cogito_css_append(out, cap, &off, ".subtitle { color: %s; }\n", ons);
  cogito_css_append(out, cap, &off, ".body { color: %s; }\n", ons);
  cogito_css_append(out, cap, &off, "list { background: %s; }\n", surf);
  cogito_css_append(out, cap, &off, "list:selection { background: %s; }\n", p);
  cogito_css_append(out, cap, &off, "grid { background: %s; }\n", surf);
  cogito_css_append(out, cap, &off, "grid:selection { background: %s; }\n", p);
  cogito_css_append(out, cap, &off, "appbar { background: %s; }\n", surf);
  cogito_css_append(out, cap, &off, "dialog { background: %s; }\n", bg);
  cogito_css_append(out, cap, &off, ".appbar .window-button { border-color: %s; }\n", outl);
  cogito_css_append(out, cap, &off, ".appbar .window-close { background: #ff5f57; }\n");
  cogito_css_append(out, cap, &off, ".appbar .window-min { background: #febc2e; }\n");
  cogito_css_append(out, cap, &off, ".appbar .window-max { background: #28c840; }\n");
  cogito_css_append(out, cap, &off, ".appbar .window-close:hover { background: #ff7b73; }\n");
  cogito_css_append(out, cap, &off, ".appbar .window-close:active { background: #e5534d; }\n");
  cogito_css_append(out, cap, &off, ".appbar .window-min:hover { background: #ffd066; }\n");
  cogito_css_append(out, cap, &off, ".appbar .window-min:active { background: #e0a52a; }\n");
  cogito_css_append(out, cap, &off, ".appbar .window-max:hover { background: #52d870; }\n");
  cogito_css_append(out, cap, &off, ".appbar .window-max:active { background: #1ea64a; }\n");
  cogito_css_append(out, cap, &off, ".appbar .button:hover { background: %s; }\n", surf);
  cogito_css_append(out, cap, &off, ".appbar .button:active { background: %s; }\n", outl);
  cogito_css_append(out, cap, &off, ".appbar .iconbtn:hover { background: %s; }\n", outl);
  cogito_css_append(out, cap, &off, ".appbar .iconbtn:active { background: %s; }\n", bg);
  cogito_css_append(out, cap, &off, "menu { background: %s; color: %s; }\n", surf, ons);
  cogito_css_append(out, cap, &off, "menu:selection { background: %s; }\n", p);
  cogito_css_append(out, cap, &off, ".menu:selection { background: %s; }\n", p);
  cogito_css_append(out, cap, &off, "menu item { background: %s; }\n", surf);
}

static void cogito_apply_theme(bool dark, Color accent) {
  cogito_theme_initialized = true;
  cogito_css_default_loaded = true;
  cogito_theme_set_defaults();
  cogito_apply_default_css();
  if (dark) {
    cogito_apply_css_file_inline(cogito_dark_css_path());
  }
  char dyn[16384];
  cogito_build_dynamic_css(dark, accent, dyn, sizeof(dyn));
  if (getenv("COGITO_THEME_DEBUG")) {
    fprintf(stderr, "Cogito dynamic CSS:\\n%s\\n", dyn);
  }
  cogito_apply_css_inline(dyn);
  if (cogito_user_css && cogito_user_css[0]) {
    cogito_apply_css_inline(cogito_user_css);
  }
  cogito_theme_dark = dark;
  cogito_theme_accent = accent;
  extern CogitoNode* cogito_active_window;
  if (cogito_active_window) {
    cogito_apply_style_tree(cogito_active_window);
    cogito_window_relayout(cogito_active_window);
  }
}

static void cogito_menu_close(void) {
  cogito_menu.open = false;
  cogito_menu.owner = NULL;
}
