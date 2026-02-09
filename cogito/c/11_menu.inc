static void cogito_menu_open_for(CogitoNode* n) {
  if (!n || n->menu_len == 0) return;
  int font_size = cogito_font_size();
  if (cogito_theme.menu.has_font_size && cogito_theme.menu.font_size > 0) {
    font_size = cogito_theme.menu.font_size;
  }
  CogitoStyle* item_style = &cogito_theme.menu_item;
  int pad_x = item_style->has_padding_left ? item_style->padding_left
              : (cogito_theme.menu.has_padding_left ? cogito_theme.menu.padding_left : 10);
  int pad_y = item_style->has_padding_top ? item_style->padding_top
              : (cogito_theme.menu.has_padding_top ? cogito_theme.menu.padding_top : 6);
  int item_h = font_size + pad_y * 2;
  int min_h = item_style->has_min_h ? item_style->min_h
              : (cogito_theme.menu.has_min_h ? cogito_theme.menu.min_h : 0);
  if (min_h > item_h) item_h = min_h;
  int menu_pad_top = 8;
  int menu_pad_bottom = 8;
  int max_w = 0;
  for (size_t i = 0; i < n->menu_len; i++) {
    const char* text = n->menu_labels[i] ? n->menu_labels[i]->data : "";
    int w = cogito_text_width_size(text, font_size);
    if (w > max_w) max_w = w;
  }
  int w = max_w + pad_x * 2;
  int h = (int)n->menu_len * item_h + menu_pad_top + menu_pad_bottom;
  cogito_menu.open = true;
  cogito_menu.owner = n;
  cogito_menu.item_h = item_h;
  cogito_menu.pad_x = pad_x;
  cogito_menu.pad_y = pad_y;
  cogito_menu.pad_top = menu_pad_top;
  cogito_menu.pad_bottom = menu_pad_bottom;
  cogito_menu.w = w;
  cogito_menu.h = h;
  int x = n->x;
  int y = n->y + n->h + 4;
  
  // Get window size from backend
  int win_w = 800, win_h = 600;  // Default fallback
  if (cogito_backend && cogito_backend_ready && cogito_active_window) {
    // Use window size from active window
    win_w = cogito_active_window->w;
    win_h = cogito_active_window->h;
  }
  
  if (w > win_w - 8) w = win_w - 8;
  if (h > win_h - 8) h = win_h - 8;
  if (x + w > win_w - 4) x = win_w - 4 - w;
  if (x < 4) x = 4;
  if (y + h > win_h - 4) {
    int above = n->y - h - 4;
    if (above >= 4) y = above;
    else y = win_h - 4 - h;
  }
  if (y < 4) y = 4;
  cogito_menu.w = w;
  cogito_menu.h = h;
  cogito_menu.x = x;
  cogito_menu.y = y;
}

static int cogito_menu_hit(const CogitoMenu* menu, int x, int y) {
  if (!menu || !menu->open) return -1;
  if (x < menu->x || y < menu->y || x > menu->x + menu->w || y > menu->y + menu->h) return -1;
  int rel_y = y - (menu->y + menu->pad_top);
  if (rel_y < 0) return -1;
  int idx = rel_y / menu->item_h;
  if (idx < 0) return -1;
  if (!menu->owner) return -1;
  if ((size_t)idx >= menu->owner->menu_len) return -1;
  return idx;
}

static float cogito_roundness_for(int w, int h, int radius) {
  if (radius <= 0) return 0.0f;
  float min = (float)(w < h ? w : h);
  if (min <= 0.0f) return 0.0f;
  float roundness = (float)radius / (min * 0.5f);
  if (roundness > 1.0f) roundness = 1.0f;
  if (roundness < 0.0f) roundness = 0.0f;
  return roundness;
}

// Stub: Drawing functions use backend
static void cogito_draw_rect(int x, int y, int w, int h, CogitoColor bg, int radius) {
  if (w <= 0 || h <= 0) return;
  if (cogito_backend && cogito_backend->draw_rect_rounded) {
    float roundness = cogito_roundness_for(w, h, radius);
    if (roundness > 0.0f) {
      cogito_backend->draw_rect_rounded(x, y, w, h, bg, roundness);
    } else {
      cogito_backend->draw_rect(x, y, w, h, bg);
    }
  }
}

static void cogito_draw_rect_lines(int x, int y, int w, int h, CogitoColor border, int radius, int thickness) {
  if (w <= 0 || h <= 0 || thickness <= 0) return;
  if (cogito_backend && cogito_backend->draw_rect_rounded_lines) {
    float roundness = cogito_roundness_for(w, h, radius);
    if (roundness > 0.0f) {
      cogito_backend->draw_rect_rounded_lines(x, y, w, h, border, roundness, thickness);
    } else {
      cogito_backend->draw_rect_lines(x, y, w, h, border, thickness);
    }
  }
}

// Use backend.h implementation
#define cogito_blend cogito_color_blend

static bool cogito_text_blend_premultiply(void) {
  static int cached = -1;
  if (cached >= 0) return cached == 1;
  const char* env = getenv("COGITO_TEXT_BLEND");
  if (env && env[0]) {
    if (strcmp(env, "premultiply") == 0 || strcmp(env, "premultiplied") == 0 ||
        strcmp(env, "1") == 0 || strcmp(env, "true") == 0 || strcmp(env, "yes") == 0) {
      cached = 1;
      return true;
    }
    if (strcmp(env, "alpha") == 0 || strcmp(env, "normal") == 0 ||
        strcmp(env, "0") == 0 || strcmp(env, "false") == 0 || strcmp(env, "no") == 0) {
      cached = 0;
      return false;
    }
  }
  // Default to normal alpha; premultiply can be enabled explicitly via env.
  cached = 0;
  return cached == 1;
}

static int cogito_font_filter_override(void) {
  static int cached = -2;
  if (cached != -2) return cached;
  const char* env = getenv("COGITO_FONT_FILTER");
  if (env && env[0]) {
    if (strcmp(env, "point") == 0 || strcmp(env, "nearest") == 0) {
      cached = 0;  // TEXTURE_FILTER_POINT equivalent
      return cached;
    }
    if (strcmp(env, "bilinear") == 0 || strcmp(env, "linear") == 0) {
      cached = 1;  // TEXTURE_FILTER_BILINEAR equivalent
      return cached;
    }
  }
  cached = -1; // auto
  return cached;
}

static void cogito_apply_font_filter(CogitoFont* f, int size) {
  // Phase 2: Implement font filtering
  (void)f;
  (void)size;
}

static float cogito_letter_spacing(const CogitoNode* n) {
  return (n && n->letter_spacing_set) ? n->letter_spacing : 0.0f;
}

__attribute__((unused))
static void cogito_draw_text(int x, int y, const char* text, CogitoColor color, float spacing) {
  (void)spacing;  // Phase 2: Implement letter spacing
  if (!text) return;
  if (cogito_backend && cogito_backend->draw_text && cogito_font_ready) {
    cogito_apply_font_filter(cogito_font, cogito_font_size());
    cogito_backend->draw_text(cogito_font, text, x, y, cogito_font_size(), color);
  }
}

static void cogito_draw_text_size_font(CogitoFont* f, int x, int y, const char* text, CogitoColor color, int size, bool bold, float spacing) {
  (void)spacing;  // Phase 2: Implement letter spacing
  (void)bold;     // Phase 2: Implement bold text
  if (!text) return;
  if (size <= 0) size = cogito_font_size();
  if (cogito_backend && cogito_backend->draw_text && cogito_font_ready && f) {
    cogito_apply_font_filter(f, size);
    cogito_backend->draw_text(f, text, x, y, size, color);
  }
}

__attribute__((unused))
static void cogito_draw_text_size(int x, int y, const char* text, CogitoColor color, int size, bool bold, float spacing) {
  cogito_draw_text_size_font(cogito_font, x, y, text, color, size, bold, spacing);
}

static void cogito_draw_text_node(const CogitoNode* n, int x, int y, const char* text, CogitoColor color) {
  float spacing = cogito_letter_spacing(n);
  CogitoFont* f = cogito_font_for_node(n);
  if (n && n->font_tabular) {
    f = cogito_font_tabular_for(f, cogito_font_size());
  }
  cogito_draw_text_size_font(f, x, y, text, color, cogito_font_size(), false, spacing);
}

static void cogito_draw_text_size_node(const CogitoNode* n, int x, int y, const char* text, CogitoColor color, int size, bool bold) {
  float spacing = cogito_letter_spacing(n);
  CogitoFont* f = cogito_font_for_node(n);
  bool synth_bold = bold;
  if (bold && f == cogito_font && cogito_font_bold_ready) {
    f = cogito_font_bold;
    synth_bold = false;
  }
  if (n && n->font_tabular) {
    f = cogito_font_tabular_for(f, size);
  }
  cogito_draw_text_size_font(f, x, y, text, color, size, synth_bold, spacing);
}
