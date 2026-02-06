
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

static void cogito_sum_apply_property(CogitoStyle* style, const char* prop, const char** p, bool selection_mode) {
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
      if (strcmp(ease, "linear") == 0) style->transition_ease = 0;
      else if (strcmp(ease, "standard") == 0) style->transition_ease = 1;
      else if (strcmp(ease, "ease") == 0) style->transition_ease = 1;
      else if (strcmp(ease, "ease-in") == 0) style->transition_ease = 2;
      else if (strcmp(ease, "ease-out") == 0) style->transition_ease = 3;
      else if (strcmp(ease, "ease-in-out") == 0) style->transition_ease = 4;
      else if (strcmp(ease, "emphasized") == 0) style->transition_ease = 5;
      else if (strcmp(ease, "emphasized-out") == 0) style->transition_ease = 6;
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
      if (strcmp(ease, "linear") == 0) style->transition_ease = 0;
      else if (strcmp(ease, "standard") == 0) style->transition_ease = 1;
      else if (strcmp(ease, "ease") == 0) style->transition_ease = 1;
      else if (strcmp(ease, "ease-in") == 0) style->transition_ease = 2;
      else if (strcmp(ease, "ease-out") == 0) style->transition_ease = 3;
      else if (strcmp(ease, "ease-in-out") == 0) style->transition_ease = 4;
      else if (strcmp(ease, "emphasized") == 0) style->transition_ease = 5;
      else if (strcmp(ease, "emphasized-out") == 0) style->transition_ease = 6;
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
    int vals[4] = {0, 0, 0, 0};
    int count = cogito_css_parse_ints(p, vals, 4);
    if (count == 1) {
      // All corners same
      cogito_style_set_radius(style, vals[0], vals[0], vals[0], vals[0]);
      style->has_radius = true;
      style->radius = vals[0];
    } else if (count == 2) {
      // TL/BR = r1, TR/BL = r2
      cogito_style_set_radius(style, vals[0], vals[1], vals[0], vals[1]);
    } else if (count == 3) {
      // TL = r1, TR/BL = r2, BR = r3
      cogito_style_set_radius(style, vals[0], vals[1], vals[2], vals[1]);
    } else if (count >= 4) {
      // TL TR BR BL
      cogito_style_set_radius(style, vals[0], vals[1], vals[2], vals[3]);
    }
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
  if (strcmp(prop, "box-shadow") == 0) {
    // SUM spec: box-shadow: <dx> <dy> [<blur>] [<spread>] <color> [inset]
    // For simplicity, we parse a single shadow (not bracketed multiple shadows yet)
    cogito_css_skip_ws(p);

    // Skip optional opening bracket
    if (**p == '[') {
      (*p)++;
      cogito_css_skip_ws(p);
    }

    // Parse dx, dy
    double dx = 0, dy = 0, blur = 0, spread = 0;
    if (!cogito_css_read_number(p, &dx)) return;
    if (!cogito_css_read_number(p, &dy)) return;

    // Parse optional blur
    const char* save = *p;
    cogito_css_skip_ws(&save);
    if (save[0] >= '0' && save[0] <= '9') {
      *p = save;
      cogito_css_read_number(p, &blur);
    }

    // Parse optional spread
    save = *p;
    cogito_css_skip_ws(&save);
    if (save[0] >= '0' && save[0] <= '9') {
      *p = save;
      cogito_css_read_number(p, &spread);
    }

    // Parse color
    Color c = {0, 0, 0, 255};
    if (!cogito_css_parse_color(p, &c)) return;

    // Parse optional inset
    bool inset = false;
    char kw[16];
    cogito_css_skip_ws(p);
    const char* kw_start = *p;
    if (cogito_css_read_ident(p, kw, sizeof(kw))) {
      if (strcmp(kw, "inset") == 0) {
        inset = true;
      } else {
        // Not inset, rewind
        *p = kw_start;
      }
    }

    // Skip optional closing bracket
    cogito_css_skip_ws(p);
    if (**p == ']') (*p)++;

    style->has_box_shadow = true;
    style->box_shadow.dx = (float)dx;
    style->box_shadow.dy = (float)dy;
    style->box_shadow.blur = (float)blur;
    style->box_shadow.spread = (float)spread;
    style->box_shadow.color = c;
    style->box_shadow.inset = inset;
    return;
  }
  if (strcmp(prop, "elevation") == 0) {
    // Keep legacy elevation for backward compatibility
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
  // SUM spec: font shorthand  font: <family> <size> <weight>
  if (strcmp(prop, "font") == 0) {
    // Parse family (quoted string or bare ident)
    cogito_css_skip_ws(p);
    style->font_family[0] = '\0';
    if (**p == '"') {
      (*p)++;
      size_t i = 0;
      while (**p && **p != '"' && i < sizeof(style->font_family) - 1) {
        style->font_family[i++] = **p;
        (*p)++;
      }
      style->font_family[i] = '\0';
      if (**p == '"') (*p)++;
      style->has_font_family = true;
    } else {
      char fam[64];
      if (cogito_css_read_ident(p, fam, sizeof(fam))) {
        strncpy(style->font_family, fam, sizeof(style->font_family) - 1);
        style->font_family[sizeof(style->font_family) - 1] = '\0';
        style->has_font_family = true;
      }
    }
    // Size
    double sz;
    if (cogito_css_read_number(p, &sz)) {
      style->has_font_size = true;
      style->font_size = (int)lround(sz);
    }
    // Weight
    cogito_css_skip_ws(p);
    const char* ws = *p;
    char wident[32];
    if (cogito_css_read_ident(&ws, wident, sizeof(wident))) {
      if (strcmp(wident, "bold") == 0) { style->has_font_weight = true; style->font_weight = 700; *p = ws; }
      else if (strcmp(wident, "normal") == 0) { style->has_font_weight = true; style->font_weight = 400; *p = ws; }
    } else {
      double wv;
      if (cogito_css_read_number(p, &wv)) {
        style->has_font_weight = true;
        style->font_weight = (int)lround(wv);
      }
    }
    return;
  }
  if (strcmp(prop, "opacity") == 0) {
    double v;
    if (cogito_css_read_number(p, &v)) {
      // Clamp opacity between 0.0 and 1.0
      if (v < 0.0) v = 0.0;
      if (v > 1.0) v = 1.0;
      style->has_opacity = true;
      style->opacity = (float)v;
    }
    return;
  }
  // SUM spec: border shorthand  border: <width> <style> <color>
  if (strcmp(prop, "border") == 0) {
    double bw;
    if (cogito_css_read_number(p, &bw)) {
      style->has_border_width = true;
      style->border_width = (int)lround(bw);
    }
    cogito_css_skip_ws(p);
    char bstyle[16];
    if (cogito_css_read_ident(p, bstyle, sizeof(bstyle))) {
      style->has_border_style = true;
      if (strcmp(bstyle, "none") == 0) style->border_style = COGITO_BORDER_NONE;
      else if (strcmp(bstyle, "solid") == 0) style->border_style = COGITO_BORDER_SOLID;
      else if (strcmp(bstyle, "dashed") == 0) style->border_style = COGITO_BORDER_DASHED;
      else if (strcmp(bstyle, "dotted") == 0) style->border_style = COGITO_BORDER_DOTTED;
      else style->border_style = COGITO_BORDER_SOLID; // default to solid
    }
    Color bc;
    if (cogito_css_parse_color(p, &bc)) {
      style->has_border = true;
      style->border = bc;
    }
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

static bool cogito_sum_selector_kind(const char* sel, CogitoKind* out_kind, bool* out_base, int* out_label_class, bool* out_menu, int* out_class) {
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
    if (strcmp(class_part, "outlined") == 0) {
      if (out_class) *out_class = COGITO_STYLE_CLASS_OUTLINED;
      return true;
    }
    if (strcmp(class_part, "text") == 0) {
      if (out_class) *out_class = COGITO_STYLE_CLASS_TEXT;
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

static bool cogito_sum_parse_token(const char* tok, char* base, size_t base_cap, char* state, size_t state_cap) {
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

static bool cogito_sum_selector_has_class(const char* sel, const char* name) {
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

static void cogito_sum_add_styles(const char* parent, const char* child, CogitoStyle** styles, int* flags, int* count, int max) {
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
    if (cogito_sum_selector_kind(sel, &kind, &is_base, &label_class, &is_menu, &class_kind)) {
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
        } else if (class_kind == COGITO_STYLE_CLASS_OUTLINED) {
          if (disabled) style = &cogito_theme.button_outlined_disabled;
          else if (active) style = &cogito_theme.button_outlined_active;
          else if (hover) style = &cogito_theme.button_outlined_hover;
          else style = &cogito_theme.button_outlined;
        } else if (class_kind == COGITO_STYLE_CLASS_TEXT) {
          if (disabled) style = &cogito_theme.button_text_disabled;
          else if (active) style = &cogito_theme.button_text_active;
          else if (hover) style = &cogito_theme.button_text_hover;
          else style = &cogito_theme.button_text;
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
  if (!cogito_sum_parse_token(parent, parent_base, sizeof(parent_base), parent_state, sizeof(parent_state))) return;
  if (!cogito_sum_parse_token(child, child_base, sizeof(child_base), child_state, sizeof(child_state))) return;
  bool hover = strcmp(child_state, "hover") == 0;
  bool active = strcmp(child_state, "active") == 0;
  bool checked_state = strcmp(parent_state, "checked") == 0;
  bool parent_radio = strcmp(parent_base, "radio") == 0 || cogito_sum_selector_has_class(parent, "radio");
  bool parent_checkbox = strcmp(parent_base, "checkbox") == 0 || cogito_sum_selector_has_class(parent, "checkbox");
  bool parent_switch = strcmp(parent_base, "switch") == 0 || cogito_sum_selector_has_class(parent, "switch");
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

// ---- SUM indentation-based parser ----

static bool cogito_sum_line_is_blank_or_comment(const char* line, size_t len) {
  for (size_t i = 0; i < len; i++) {
    char c = line[i];
    if (c == ';') return true;
    if (c != ' ' && c != '\t') return false;
  }
  return true;
}

static int cogito_sum_indent_level(const char* line, size_t len) {
  int indent = 0;
  for (size_t i = 0; i < len; i++) {
    if (line[i] == ' ') indent++;
    else if (line[i] == '\t') indent += 2;  // treat tab as 2 spaces
    else break;
  }
  return indent;
}

static void cogito_parse_sum(const char* src) {
  if (!src || !*src) return;
  cogito_style_version++;

  const char* p = src;

  while (*p) {
    // Skip blank/comment lines
    const char* line_start = p;
    const char* line_end = line_start;
    while (*line_end && *line_end != '\n') line_end++;
    size_t line_len = (size_t)(line_end - line_start);

    if (cogito_sum_line_is_blank_or_comment(line_start, line_len)) {
      p = *line_end ? line_end + 1 : line_end;
      continue;
    }

    // Check indentation — selector lines have 0 indent
    int indent = cogito_sum_indent_level(line_start, line_len);
    if (indent > 0) {
      // Stray indented line outside a rule — skip
      p = *line_end ? line_end + 1 : line_end;
      continue;
    }

    // This is a selector line — parse selector(s)
    CogitoStyle* styles[16];
    int style_flags[16] = {0};
    int style_count = 0;

    // Extract selector text from this line
    char sel_line[256];
    if (line_len >= sizeof(sel_line)) line_len = sizeof(sel_line) - 1;
    memcpy(sel_line, line_start, line_len);
    sel_line[line_len] = 0;

    // Strip trailing comment
    char* semi = strchr(sel_line, ';');
    if (semi) *semi = 0;

    // Trim trailing whitespace
    size_t sl = strlen(sel_line);
    while (sl > 0 && (sel_line[sl-1] == ' ' || sel_line[sl-1] == '\t' || sel_line[sl-1] == '\r')) {
      sel_line[--sl] = 0;
    }

    // Parse selector(s) — support comma-separated selectors on one line
    {
      const char* sp = sel_line;
      while (*sp) {
        while (*sp == ' ' || *sp == '\t') sp++;
        if (!*sp) break;

        // Read first token (parent or only selector)
        char parent[64] = {0};
        size_t pi = 0;
        while (*sp && *sp != ' ' && *sp != '\t' && *sp != ',') {
          if (pi < sizeof(parent) - 1) parent[pi++] = *sp;
          sp++;
        }
        parent[pi] = 0;

        // Skip spaces to see if there's a child selector
        while (*sp == ' ' || *sp == '\t') sp++;

        char child[64] = {0};
        if (*sp && *sp != ',') {
          size_t ci = 0;
          while (*sp && *sp != ' ' && *sp != '\t' && *sp != ',') {
            if (ci < sizeof(child) - 1) child[ci++] = *sp;
            sp++;
          }
          child[ci] = 0;
        }

        cogito_sum_add_styles(parent, child[0] ? child : NULL, styles, style_flags, &style_count, (int)(sizeof(styles) / sizeof(styles[0])));

        // Skip to next comma or end
        while (*sp == ' ' || *sp == '\t') sp++;
        if (*sp == ',') { sp++; continue; }
        break;
      }
    }

    // Advance past selector line
    p = *line_end ? line_end + 1 : line_end;

    if (style_count == 0) {
      // Unknown selector — skip indented block
      while (*p) {
        const char* bl = p;
        const char* be = bl;
        while (*be && *be != '\n') be++;
        size_t blen = (size_t)(be - bl);
        if (cogito_sum_line_is_blank_or_comment(bl, blen)) {
          p = *be ? be + 1 : be;
          continue;
        }
        if (cogito_sum_indent_level(bl, blen) == 0) break;
        p = *be ? be + 1 : be;
      }
      continue;
    }

    // Parse indented declaration lines
    while (*p) {
      const char* dl = p;
      const char* de = dl;
      while (*de && *de != '\n') de++;
      size_t dlen = (size_t)(de - dl);

      if (cogito_sum_line_is_blank_or_comment(dl, dlen)) {
        p = *de ? de + 1 : de;
        continue;
      }

      int di = cogito_sum_indent_level(dl, dlen);
      if (di == 0) break;  // Next selector — stop

      // Extract declaration line content (skip indent)
      const char* dc = dl;
      while (*dc == ' ' || *dc == '\t') dc++;

      // Strip comment
      char decl[256];
      size_t dremain = (size_t)(de - dc);
      if (dremain >= sizeof(decl)) dremain = sizeof(decl) - 1;
      memcpy(decl, dc, dremain);
      decl[dremain] = 0;
      char* dsemi = strchr(decl, ';');
      if (dsemi) *dsemi = 0;

      // Trim trailing whitespace
      size_t dll = strlen(decl);
      while (dll > 0 && (decl[dll-1] == ' ' || decl[dll-1] == '\t' || decl[dll-1] == '\r')) {
        decl[--dll] = 0;
      }

      // Parse property: value
      char prop[64] = {0};
      const char* dp = decl;
      size_t ppi = 0;
      while (*dp && *dp != ':' && *dp != ' ' && *dp != '\t') {
        if (ppi < sizeof(prop) - 1) prop[ppi++] = *dp;
        dp++;
      }
      prop[ppi] = 0;

      // Skip to colon
      while (*dp == ' ' || *dp == '\t') dp++;
      if (*dp == ':') dp++;
      while (*dp == ' ' || *dp == '\t') dp++;

      // Strip 'sp' unit suffixes from values for compatibility
      // (SUM uses Nsp, Cogito internally uses bare numbers)
      char val_buf[256];
      {
        size_t vi = 0;
        const char* vp = dp;
        while (*vp && vi < sizeof(val_buf) - 1) {
          // If we see a digit followed by 'sp', skip the 'sp'
          if (vp[0] == 's' && vp[1] == 'p' && vi > 0 &&
              (val_buf[vi-1] >= '0' && val_buf[vi-1] <= '9')) {
            vp += 2;
            continue;
          }
          val_buf[vi++] = *vp++;
        }
        val_buf[vi] = 0;
        dp = val_buf;
      }

      // Apply property to all matched styles
      for (int i = 0; i < style_count; i++) {
        const char* v = dp;
        cogito_sum_apply_property(styles[i], prop, &v, style_flags[i] != 0);
      }

      p = *de ? de + 1 : de;
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

static const char* cogito_default_sum_path(void) {
  const char* stdlib = getenv("ERGO_STDLIB");
  if (stdlib && stdlib[0]) {
    static char buf[PATH_MAX];
    snprintf(buf, sizeof(buf), "%s/cogito_default.sum", stdlib);
    if (cogito_file_exists(buf)) return buf;
  }
  if (cogito_file_exists("cogito/cogito_default.sum")) return "cogito/cogito_default.sum";
  if (cogito_file_exists("../cogito/cogito_default.sum")) return "../cogito/cogito_default.sum";
  return "cogito/cogito_default.sum";
}

static const char* cogito_dark_sum_path(void) {
  const char* stdlib = getenv("ERGO_STDLIB");
  if (stdlib && stdlib[0]) {
    static char buf[PATH_MAX];
    snprintf(buf, sizeof(buf), "%s/cogito_dark.sum", stdlib);
    if (cogito_file_exists(buf)) return buf;
  }
  if (cogito_file_exists("cogito/cogito_dark.sum")) return "cogito/cogito_dark.sum";
  if (cogito_file_exists("../cogito/cogito_dark.sum")) return "../cogito/cogito_dark.sum";
  return "cogito/cogito_dark.sum";
}

static void cogito_apply_sum_inline(const char* src) {
  if (!src || !src[0]) return;
  cogito_parse_sum(src);
}

static void cogito_apply_sum_file_inline(const char* path) {
  size_t len = 0;
  char* src = cogito_read_file(path, &len);
  if (!src) return;
  cogito_parse_sum(src);
  free(src);
}

static void cogito_apply_default_sum(void) {
  size_t len = 0;
  const char* path = cogito_default_sum_path();
  char* src = cogito_read_file(path, &len);
  if (!src) return;
  cogito_parse_sum(src);
  free(src);
}

static void cogito_load_sum_file(const char* path) {
  size_t len = 0;
  char* src = cogito_read_file(path, &len);
  if (!src) {
    if (cogito_debug_enabled()) {
      fprintf(stderr, "cogito: sum not found: %s\n", path ? path : "");
      fflush(stderr);
    }
    return;
  }
  cogito_theme_initialized = true;
  cogito_theme_set_defaults();
  cogito_parse_sum(src);
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

static void cogito_load_sum(ErgoVal pathv) {
  ErgoStr* ps = stdr_to_string(pathv);
  if (ps && ps->data) {
    size_t len = 0;
    char* src = cogito_read_file(ps->data, &len);
    if (src) {
      if (cogito_user_sum) free(cogito_user_sum);
      cogito_user_sum = src;
      cogito_apply_theme(cogito_theme_dark, cogito_theme_accent);
    }
  }
  if (ps) ergo_release_val(EV_STR(ps));
}

__attribute__((unused))
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

static void cogito_sum_append(char* dst, size_t dst_cap, size_t* dst_off, const char* fmt, ...) {
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

static void cogito_build_dynamic_sum(bool dark, Color accent, char* out, size_t cap) {
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
  // Derive surface_tint from primary color for color harmony
  Color surface_tint = dark
    ? cogito_mix(primary, surface, 0.08f)
    : cogito_mix(primary, cogito_rgba(255, 255, 255, 255), 0.92f);
  Color on_surface = dark ? (Color){240, 240, 244, 255} : (Color){20, 20, 22, 255};
  Color outline = dark ? (Color){90, 90, 94, 255} : (Color){200, 200, 204, 255};
  // Derive inactive_track from primary for color harmony
  Color inactive_track = dark
    ? cogito_mix(primary, cogito_rgba(128, 128, 128, 255), 0.7f)
    : cogito_mix(primary, cogito_rgba(128, 128, 128, 255), 0.6f);
  char p[8], op[8], pc[8], opc[8], sec[8], bg[8], surf[8], surft[8], ons[8], outl[8], itrack[8];
  cogito_color_to_hex(primary, p);
  cogito_color_to_hex(on_primary, op);
  cogito_color_to_hex(primary_container, pc);
  cogito_color_to_hex(on_primary_container, opc);
  cogito_color_to_hex(secondary, sec);
  cogito_color_to_hex(background, bg);
  cogito_color_to_hex(surface, surf);
  cogito_color_to_hex(surface_tint, surft);
  cogito_color_to_hex(on_surface, ons);
  cogito_color_to_hex(outline, outl);
  cogito_color_to_hex(inactive_track, itrack);

  // Compute hover/active colors
  Color p_hover = cogito_mix(primary, dark ? cogito_rgba(255, 255, 255, 255) : cogito_rgba(0, 0, 0, 255), 0.1f);
  Color p_active = cogito_mix(primary, dark ? cogito_rgba(0, 0, 0, 255) : cogito_rgba(255, 255, 255, 255), 0.1f);
  Color surft_hover = cogito_mix(surface_tint, dark ? cogito_rgba(255, 255, 255, 255) : cogito_rgba(0, 0, 0, 255), 0.05f);
  char ph[8], pa[8], surfth[8];
  cogito_color_to_hex(p_hover, ph);
  cogito_color_to_hex(p_active, pa);
  cogito_color_to_hex(surft_hover, surfth);
    Color nav_bg = dark ? (Color){43, 41, 48, 255} : (Color){247, 242, 250, 255};
  Color nav_sel = dark ? (Color){74, 68, 88, 255} : (Color){232, 222, 248, 255};
  char navbg[8], navsel[8];
  cogito_color_to_hex(nav_bg, navbg);
  cogito_color_to_hex(nav_sel, navsel);
  cogito_sum_append(out, cap, &off, "nav_rail\n  background: %s\n  min-width: 80\n  padding: 4 0\n", navbg);
  cogito_sum_append(out, cap, &off, "nav_rail:selection\n  background: %s\n", navsel);
  Color bottom_bg = dark ? (Color){28, 27, 31, 255} : (Color){255, 251, 254, 255};
  Color bottom_sel = dark ? (Color){74, 68, 88, 255} : (Color){232, 222, 248, 255};
  char bottombg[8], bottomsel[8];
  cogito_color_to_hex(bottom_bg, bottombg);
  cogito_color_to_hex(bottom_sel, bottomsel);
    Color tabs_bg = dark ? (Color){38, 38, 40, 255} : (Color){242, 242, 242, 255};
  Color tabs_sel = dark ? surface : (Color){255, 255, 255, 255};
  char tabsbg[8], tabssel[8];
  cogito_color_to_hex(tabs_bg, tabsbg);
  cogito_color_to_hex(tabs_sel, tabssel);
  Color progress_bg = dark ? (Color){60, 60, 62, 255} : (Color){221, 221, 221, 255};
  char progbg[8];
  cogito_color_to_hex(progress_bg, progbg);
  Color toast_bg = dark ? (Color){34, 34, 34, 255} : (Color){34, 34, 34, 255};
  Color toast_fg = (Color){255, 255, 255, 255};
  char toastbg[8], toastfg[8];
  cogito_color_to_hex(toast_bg, toastbg);
  cogito_color_to_hex(toast_fg, toastfg);
  // Derive iconbtn background from outline for subtle state layer
  Color iconbtn_bg = cogito_mix(outline, surface, 0.9f);
  char iconbg[8];
  cogito_color_to_hex(iconbtn_bg, iconbg);
  Color tooltip_bg = dark ? (Color){30, 30, 30, 255} : (Color){30, 30, 30, 255};
  Color tooltip_fg = (Color){255, 255, 255, 255};
  char tooltipbg[8], tooltipfg[8];
  // Derive checkbox/radio box background from outline for subtle state layer
  Color checkbox_bg = cogito_mix(outline, surface, 0.88f);
  char checkbg[8];
  cogito_color_to_hex(checkbox_bg, checkbg);
  cogito_color_to_hex(tooltip_bg, tooltipbg);
  cogito_color_to_hex(tooltip_fg, tooltipfg);
  Color check_hover = dark ? (Color){240, 240, 240, 255} : (Color){17, 17, 17, 255};
  Color check_active = dark ? (Color){220, 220, 220, 255} : (Color){0, 0, 0, 255};
  char checkhov[8], checkact[8];
  cogito_color_to_hex(check_hover, checkhov);
  cogito_color_to_hex(check_active, checkact);
  Color switch_track = dark ? (Color){180, 180, 180, 255} : (Color){180, 180, 180, 255};
  Color switch_track_hover = dark ? (Color){196, 196, 196, 255} : (Color){196, 196, 196, 255};
  char strack[8], strackh[8];
  cogito_color_to_hex(switch_track, strack);
  cogito_color_to_hex(switch_track_hover, strackh);
  Color knob_color = dark ? (Color){255, 255, 255, 255} : (Color){255, 255, 255, 255};
  Color knob_hover = dark ? (Color){244, 244, 244, 255} : (Color){244, 244, 244, 255};
  Color knob_active = dark ? (Color){232, 232, 232, 255} : (Color){232, 232, 232, 255};
  char knob[8], knobh[8], knoba[8];
  cogito_color_to_hex(knob_color, knob);
  cogito_color_to_hex(knob_hover, knobh);
  cogito_color_to_hex(knob_active, knoba);
  Color appbar_bg = dark ? (Color){38, 38, 40, 255} : (Color){240, 240, 240, 255};
  char appbarbg[8];
  cogito_color_to_hex(appbar_bg, appbarbg);
  // Appbar iconbtn base state with subtle background
  Color appbar_iconbtn_base = cogito_mix(on_surface, surface, 0.95f);
  char appiconbg[8];
  cogito_color_to_hex(appbar_iconbtn_base, appiconbg);

  // SUM format: selector on unindented line, properties indented below
  cogito_sum_append(out, cap, &off, "*\n  color: %s\n  font-size: 13\n", ons);
  cogito_sum_append(out, cap, &off, "window\n  background: %s\n", bg);
  cogito_sum_append(out, cap, &off, "button\n  background: %s\n  color: %s\n  border-radius: 20\n  min-height: 40\n  padding: 10 24\n", p, op);
  cogito_sum_append(out, cap, &off, "button:hover\n  background: %s\n", ph);
  cogito_sum_append(out, cap, &off, "button:active\n  background: %s\n", pa);
  cogito_sum_append(out, cap, &off, "button.outlined\n  background: transparent\n  color: %s\n  border: 1 solid %s\n  box-shadow: none\n", p, outl);
  cogito_sum_append(out, cap, &off, "button.outlined:hover\n  background: %s12\n  border-color: %s\n", p, p);
  cogito_sum_append(out, cap, &off, "button.outlined:active\n  background: %s1f\n", p);
  cogito_sum_append(out, cap, &off, "button.outlined:disabled\n  color: %s61\n  border-color: %s12\n", ons, ons);
  cogito_sum_append(out, cap, &off, "button.text\n  background: transparent\n  color: %s\n  box-shadow: none\n  padding: 10 12\n", p);
  cogito_sum_append(out, cap, &off, "button.text:hover\n  background: %s12\n", p);
  cogito_sum_append(out, cap, &off, "button.text:active\n  background: %s1f\n", p);
  cogito_sum_append(out, cap, &off, "button.text:disabled\n  color: %s61\n", ons);
  cogito_sum_append(out, cap, &off, "fab\n  background: %s\n  color: %s\n  border-radius: 100\n  min-width: 56\n  min-height: 56\n", p, op);
  cogito_sum_append(out, cap, &off, "fab:hover\n  background: %s\n", ph);
  cogito_sum_append(out, cap, &off, "fab:active\n  background: %s\n", pa);
  cogito_sum_append(out, cap, &off, "bottom_nav\n  background: %s\n  min-height: 80\n  padding: 0 8\n", bottombg);
  cogito_sum_append(out, cap, &off, "bottom_nav:selection\n  background: %s\n", bottomsel);
  cogito_sum_append(out, cap, &off, "textfield\n  background: %s\n  border-radius: 4 4 0 0\n  border: 0 none #000000\n  min-height: 56\n  padding: 8 16\n", surft);
  cogito_sum_append(out, cap, &off, "textfield:hover\n  background: %s\n", surfth);
  cogito_sum_append(out, cap, &off, ".textfield\n  background: %s\n  border-radius: 4 4 0 0\n  min-height: 56\n  padding: 8 16\n", surft);
  cogito_sum_append(out, cap, &off, "searchfield\n  background: %s\n  border-radius: 18\n  min-height: 36\n  min-width: 180\n  padding: 6 12\n", surf);
  cogito_sum_append(out, cap, &off, ".searchfield\n  background: %s\n  border-radius: 18\n  min-height: 36\n  min-width: 180\n  padding: 6 12\n", surf);
  cogito_sum_append(out, cap, &off, "textview\n  background: %s\n  border-radius: 8\n  min-height: 96\n", surf);
  cogito_sum_append(out, cap, &off, ".textview\n  background: %s\n  border-radius: 8\n  min-height: 96\n", surf);
  cogito_sum_append(out, cap, &off, "textfield:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off, ".textfield:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off, "textview:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off, ".textview:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off, "dropdown\n  background: %s\n  border-radius: 4 4 0 0\n  min-height: 56\n  padding: 8 16\n", surft);
  cogito_sum_append(out, cap, &off, "dropdown:hover\n  background: %s\n", surfth);
  cogito_sum_append(out, cap, &off, ".dropdown\n  background: %s\n  border-radius: 4 4 0 0\n  min-height: 56\n  padding: 8 16\n", surft);
  cogito_sum_append(out, cap, &off, "datepicker\n  background: %s\n  border-radius: 12\n  min-width: 320\n  min-height: 320\n  padding: 16\n", surf);
  cogito_sum_append(out, cap, &off, ".datepicker\n  background: %s\n  border-radius: 12\n  min-width: 320\n  min-height: 320\n  padding: 16\n", surf);
  cogito_sum_append(out, cap, &off, "datepicker:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off, ".datepicker:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off, "slider\n  background: %s\n  color: %s\n  min-height: 48\n  min-width: 200\n", itrack, p);
  cogito_sum_append(out, cap, &off, "slider:hover\n  color: %s\n", ph);
  cogito_sum_append(out, cap, &off, "slider:active\n  color: %s\n", pa);
  cogito_sum_append(out, cap, &off, ".slider\n  background: %s\n  color: %s\n  min-height: 48\n  min-width: 200\n", itrack, p);
  cogito_sum_append(out, cap, &off, "stepper\n  background: %s\n  border-radius: 8\n  min-height: 32\n  min-width: 96\n", surf);
  cogito_sum_append(out, cap, &off, ".stepper\n  background: %s\n  border-radius: 8\n  min-height: 32\n  min-width: 96\n", surf);
  cogito_sum_append(out, cap, &off, "tabs\n  background: %s\n  border-radius: 10\n  min-height: 36\n", tabsbg);
  cogito_sum_append(out, cap, &off, ".tabs\n  background: %s\n  border-radius: 10\n  min-height: 36\n", tabsbg);
  cogito_sum_append(out, cap, &off, "tabs:selection\n  background: %s\n", tabssel);
  cogito_sum_append(out, cap, &off, "segmented\n  background: %s\n  border-radius: 10\n  min-height: 36\n", tabsbg);
  cogito_sum_append(out, cap, &off, ".segmented\n  background: %s\n  border-radius: 10\n  min-height: 36\n", tabsbg);
  cogito_sum_append(out, cap, &off, "view-switcher\n  background: %s\n", surf);
  cogito_sum_append(out, cap, &off, ".view-switcher\n  background: %s\n", surf);
  cogito_sum_append(out, cap, &off, "progress\n  background: %s\n  border-radius: 6\n  min-height: 10\n  min-width: 120\n", progbg);
  cogito_sum_append(out, cap, &off, ".progress\n  background: %s\n  border-radius: 6\n  min-height: 10\n  min-width: 120\n", progbg);
  cogito_sum_append(out, cap, &off, "progress:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off, "toasts\n  padding: 12\n");
  cogito_sum_append(out, cap, &off, ".toasts\n  padding: 12\n");
  cogito_sum_append(out, cap, &off, "toast\n  background: %s\n  color: %s\n  border-radius: 10\n  min-height: 34\n", toastbg, toastfg);
  cogito_sum_append(out, cap, &off, ".toast\n  background: %s\n  color: %s\n  border-radius: 10\n  min-height: 34\n", toastbg, toastfg);
  cogito_sum_append(out, cap, &off, "treeview\n  background: %s\n  border-radius: 8\n  min-height: 180\n  min-width: 200\n  padding: 8\n", surf);
  cogito_sum_append(out, cap, &off, ".treeview\n  background: %s\n  border-radius: 8\n  min-height: 180\n  min-width: 200\n  padding: 8\n", surf);
  cogito_sum_append(out, cap, &off, "colorpicker\n  background: %s\n  border-radius: 12\n  min-width: 320\n  min-height: 320\n  padding: 16\n", surf);
  cogito_sum_append(out, cap, &off, ".colorpicker\n  background: %s\n  border-radius: 12\n  min-width: 320\n  min-height: 320\n  padding: 16\n", surf);
  cogito_sum_append(out, cap, &off, "colorpicker:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off, ".colorpicker:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off, "toolbar\n  background: %s\n  min-height: 44\n  padding: 8 12\n", surf);
  cogito_sum_append(out, cap, &off, ".toolbar\n  background: %s\n  min-height: 44\n  padding: 8 12\n", surf);
  cogito_sum_append(out, cap, &off, ".bottom-toolbar\n  background: %s\n  min-height: 44\n  padding: 8 12\n", surf);
  cogito_sum_append(out, cap, &off, "iconbtn\n  background: %s\n  border-radius: 99\n  min-width: 42\n  min-height: 42\n", iconbg);
  cogito_sum_append(out, cap, &off, "image\n  min-width: 20\n  min-height: 20\n");
  cogito_sum_append(out, cap, &off, "label\n  color: %s\n", ons);
  cogito_sum_append(out, cap, &off, ".tooltip\n  background: %s\n  color: %s\n  border-radius: 6\n  font-size: 13\n  padding: 6 8\n", tooltipbg, tooltipfg);
  cogito_sum_append(out, cap, &off, ".checkbox\n  color: %s\n  min-height: 24\n  min-width: 24\n", ons);
  cogito_sum_append(out, cap, &off, ".radio\n  color: %s\n  min-height: 24\n  min-width: 24\n", ons);
  cogito_sum_append(out, cap, &off, "checkbox\n  color: %s\n  min-height: 24\n  min-width: 24\n", ons);
  cogito_sum_append(out, cap, &off, ".checkbox:checked\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off, ".radio:checked\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off, ".checkbox .check\n  color: %s\n", ons);
  cogito_sum_append(out, cap, &off, ".radio .check\n  color: %s\n", ons);
  cogito_sum_append(out, cap, &off, ".checkbox:checked .check\n  color: %s\n", ons);
  cogito_sum_append(out, cap, &off, ".radio:checked .check\n  color: %s\n", ons);
  cogito_sum_append(out, cap, &off, ".checkbox .box\n  background: %s\n  border-radius: 4\n", checkbg);
  cogito_sum_append(out, cap, &off, ".radio .box\n  background: %s\n  border-radius: 4\n", checkbg);
  cogito_sum_append(out, cap, &off, "checkbox .box\n  background: %s\n  border-radius: 4\n", checkbg);
  cogito_sum_append(out, cap, &off, "radio .box\n  background: %s\n  border-radius: 4\n", checkbg);
  cogito_sum_append(out, cap, &off, ".checkbox:checked .box\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off, ".radio:checked .box\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off, ".checkbox .box:hover\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off, ".radio .box:hover\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off, ".checkbox .box:active\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off, ".radio .box:active\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off, ".checkbox .check:hover\n  color: %s\n", checkhov);
  cogito_sum_append(out, cap, &off, ".radio .check:hover\n  color: %s\n", checkhov);
  cogito_sum_append(out, cap, &off, ".checkbox .check:active\n  color: %s\n", checkact);
  cogito_sum_append(out, cap, &off, ".radio .check:active\n  color: %s\n", checkact);
  cogito_sum_append(out, cap, &off, ".switch\n  color: %s\n  min-height: 28\n  min-width: 64\n", ons);
  cogito_sum_append(out, cap, &off, "switch\n  color: %s\n  min-height: 28\n  min-width: 64\n", ons);
  cogito_sum_append(out, cap, &off, ".switch .track\n  background: %s\n", strack);
  cogito_sum_append(out, cap, &off, "switch track\n  background: %s\n", strack);
  cogito_sum_append(out, cap, &off, ".switch:checked .track\n  background: %s\n  border-radius: 100\n", p);
  cogito_sum_append(out, cap, &off, "switch:checked track\n  background: %s\n  border-radius: 100\n", p);
  cogito_sum_append(out, cap, &off, ".switch .track:hover\n  background: %s\n", strackh);
  cogito_sum_append(out, cap, &off, "switch track:hover\n  background: %s\n", strackh);
  cogito_sum_append(out, cap, &off, ".switch .track:active\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off, "switch track:active\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off, ".switch .knob\n  background: %s\n  min-width: 32\n  min-height: 24\n", knob);
  cogito_sum_append(out, cap, &off, "switch knob\n  background: %s\n  min-width: 32\n  min-height: 24\n", knob);
  cogito_sum_append(out, cap, &off, ".switch .knob:hover\n  background: %s\n", knobh);
  cogito_sum_append(out, cap, &off, "switch knob:hover\n  background: %s\n", knobh);
  cogito_sum_append(out, cap, &off, ".switch .knob:active\n  background: %s\n", knoba);
  cogito_sum_append(out, cap, &off, "switch knob:active\n  background: %s\n", knoba);
  cogito_sum_append(out, cap, &off, ".monospace\n  font-size: 13\n  font-weight: normal\n");
  cogito_sum_append(out, cap, &off, ".tabular\n  font-size: 13\n  font-weight: normal\n");
  cogito_sum_append(out, cap, &off, ".title\n  font-size: 32\n  font-weight: normal\n");
  cogito_sum_append(out, cap, &off, ".subtitle\n  font-size: 20\n  font-weight: normal\n");
  cogito_sum_append(out, cap, &off, ".body\n  font-size: 11\n  font-weight: normal\n");
  cogito_sum_append(out, cap, &off, "list\n  background: %s\n  border-radius: 0\n", surf);
  cogito_sum_append(out, cap, &off, "list:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off, "grid\n  background: %s\n  border-radius: 8\n", surf);
  cogito_sum_append(out, cap, &off, "grid:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off, "appbar\n  background: %s\n", appbarbg);
  cogito_sum_append(out, cap, &off, "dialog\n  background: %s\n  border-radius: 26\n  min-width: 240\n  min-height: 240\n  padding: 24\n", surf);
  cogito_sum_append(out, cap, &off, "popover\n  background: %s\n  border-radius: 12\n  padding: 16\n  box-shadow: [ 0 2 8 0 #0000001f ]\n", surf);
  cogito_sum_append(out, cap, &off, ".appbar .window-close\n  background: #ff5f57\n");
  cogito_sum_append(out, cap, &off, ".appbar .window-min\n  background: #febc2e\n");
  cogito_sum_append(out, cap, &off, ".appbar .window-max\n  background: #28c840\n");
  cogito_sum_append(out, cap, &off, ".appbar .window-close:hover\n  background: #ff7b73\n");
  cogito_sum_append(out, cap, &off, ".appbar .window-close:active\n  background: #e5534d\n");
  cogito_sum_append(out, cap, &off, ".appbar .window-min:hover\n  background: #ffd066\n");
  cogito_sum_append(out, cap, &off, ".appbar .window-min:active\n  background: #e0a52a\n");
  cogito_sum_append(out, cap, &off, ".appbar .window-max:hover\n  background: #52d870\n");
  cogito_sum_append(out, cap, &off, ".appbar .window-max:active\n  background: #1ea64a\n");
  cogito_sum_append(out, cap, &off, ".appbar .button:hover\n  background: %s\n", surf);
  cogito_sum_append(out, cap, &off, ".appbar .button:active\n  background: %s\n", outl);
  cogito_sum_append(out, cap, &off, ".appbar .iconbtn\n  background: %s\n", appiconbg);
  cogito_sum_append(out, cap, &off, ".appbar .iconbtn:hover\n  background: %s\n", outl);
  cogito_sum_append(out, cap, &off, ".appbar .iconbtn:active\n  background: %s\n", bg);
  cogito_sum_append(out, cap, &off, "menu\n  background: %s\n  border-radius: 4\n  color: %s\n  box-shadow: 2\n  font-size: 13\n", surf, ons);
  cogito_sum_append(out, cap, &off, "menu:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off, "menu item\n  border-radius: 0\n  padding: 8 12\n  min-height: 42\n");
}

static void cogito_apply_theme(bool dark, Color accent) {
  cogito_theme_initialized = true;
  cogito_sum_default_loaded = true;
  cogito_theme_set_defaults();
  cogito_apply_default_sum();
  if (dark) {
    cogito_apply_sum_file_inline(cogito_dark_sum_path());
  }
  char dyn[16384];
  cogito_build_dynamic_sum(dark, accent, dyn, sizeof(dyn));
  if (getenv("COGITO_THEME_DEBUG")) {
    fprintf(stderr, "Cogito dynamic SUM:\\n%s\\n", dyn);
  }
  cogito_apply_sum_inline(dyn);
  if (cogito_user_sum && cogito_user_sum[0]) {
    cogito_apply_sum_inline(cogito_user_sum);
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
