// Appbar widget utility functions
// Metrics, positions, controls layout, and navigation helpers.

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
  const char* s = n->appbar.controls[0] ? n->appbar.controls : "CIM|";
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
  if (app->appbar.btn_close_x >= 0 &&
      cogito_hit_rect(x, y, app->appbar.btn_close_x, app->appbar.btn_y, app->appbar.btn_size, app->appbar.btn_size)) return true;
  if (app->appbar.btn_min_x >= 0 &&
      cogito_hit_rect(x, y, app->appbar.btn_min_x, app->appbar.btn_y, app->appbar.btn_size, app->appbar.btn_size)) return true;
  if (app->appbar.btn_max_x >= 0 &&
      cogito_hit_rect(x, y, app->appbar.btn_max_x, app->appbar.btn_y, app->appbar.btn_size, app->appbar.btn_size)) return true;
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
