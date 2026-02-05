static void cogito_colorpicker_sync_hex(CogitoNode* n) {
  if (!n || n->kind != COGITO_COLORPICKER) return;
  Color c = cogito_hct_to_rgb(n->color_h, n->color_c, n->color_t);
  char buf[8];
  cogito_color_to_hex(c, buf);
  cogito_text_set_from_buf(n, buf, strlen(buf));
}

static void cogito_colorpicker_layout(CogitoNode* n, int* out_pad_l, int* out_pad_t, int* out_pad_r, int* out_pad_b,
                                      int* out_label_w, int* out_row_h, int* out_slider_h, int* out_gap,
                                      int* out_hex_x, int* out_hex_y, int* out_hex_w, int* out_hex_h) {
  CogitoStyle s = cogito_theme_resolve_node(n);
  int pad_l = s.has_padding_left ? s.padding_left : 10;
  int pad_t = s.has_padding_top ? s.padding_top : 10;
  int pad_r = s.has_padding_right ? s.padding_right : pad_l;
  int pad_b = s.has_padding_bottom ? s.padding_bottom : pad_t;
  int size = cogito_node_font_size(n);
  int row_h = size + 10;
  int slider_h = 6;
  int gap = 8;
  int label_w = 12;
  int hex_x = n->x + pad_l;
  int hex_y = n->y + pad_t + (row_h + gap) * 3;
  int hex_w = n->w - pad_l - pad_r;
  int hex_h = row_h;
  if (out_pad_l) *out_pad_l = pad_l;
  if (out_pad_t) *out_pad_t = pad_t;
  if (out_pad_r) *out_pad_r = pad_r;
  if (out_pad_b) *out_pad_b = pad_b;
  if (out_label_w) *out_label_w = label_w;
  if (out_row_h) *out_row_h = row_h;
  if (out_slider_h) *out_slider_h = slider_h;
  if (out_gap) *out_gap = gap;
  if (out_hex_x) *out_hex_x = hex_x;
  if (out_hex_y) *out_hex_y = hex_y;
  if (out_hex_w) *out_hex_w = hex_w;
  if (out_hex_h) *out_hex_h = hex_h;
}

static void cogito_colorpicker_slider_rect(CogitoNode* n, int idx, int* out_x, int* out_y, int* out_w, int* out_h) {
  int pad_l = 0, pad_t = 0, pad_r = 0, label_w = 0, row_h = 0, slider_h = 0, gap = 0;
  cogito_colorpicker_layout(n, &pad_l, &pad_t, &pad_r, NULL, &label_w, &row_h, &slider_h, &gap, NULL, NULL, NULL, NULL);
  int row_y = n->y + pad_t + idx * (row_h + gap);
  int x = n->x + pad_l + label_w + 6;
  int w = n->w - pad_l - pad_r - label_w - 6;
  int y = row_y + (row_h - slider_h) / 2;
  if (w < 1) w = 1;
  if (out_x) *out_x = x;
  if (out_y) *out_y = y;
  if (out_w) *out_w = w;
  if (out_h) *out_h = slider_h;
}

static bool cogito_colorpicker_hex_rect(CogitoNode* n, int* out_x, int* out_y, int* out_w, int* out_h) {
  int hx = 0, hy = 0, hw = 0, hh = 0;
  cogito_colorpicker_layout(n, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &hx, &hy, &hw, &hh);
  if (out_x) *out_x = hx;
  if (out_y) *out_y = hy;
  if (out_w) *out_w = hw;
  if (out_h) *out_h = hh;
  return true;
}

static int cogito_colorpicker_hit_slider(CogitoNode* n, int mx, int my) {
  for (int i = 0; i < 3; i++) {
    int sx = 0, sy = 0, sw = 0, sh = 0;
    cogito_colorpicker_slider_rect(n, i, &sx, &sy, &sw, &sh);
    if (cogito_hit_rect(mx, my, sx, sy - 6, sw, sh + 12)) return i;
  }
  return -1;
}

static bool cogito_colorpicker_set_from_mouse(CogitoNode* n, int channel, int mx) {
  int sx = 0, sy = 0, sw = 0, sh = 0;
  cogito_colorpicker_slider_rect(n, channel, &sx, &sy, &sw, &sh);
  double t = (sw > 0) ? ((double)(mx - sx) / (double)sw) : 0.0;
  if (t < 0.0) t = 0.0;
  if (t > 1.0) t = 1.0;
  double prev = 0.0;
  if (channel == 0) { prev = n->color_h; n->color_h = t * 360.0; }
  else if (channel == 1) { prev = n->color_c; n->color_c = t * 100.0; }
  else { prev = n->color_t; n->color_t = t * 100.0; }
  if (!n->color_hex_editing) {
    cogito_colorpicker_sync_hex(n);
  }
  return prev != (channel == 0 ? n->color_h : (channel == 1 ? n->color_c : n->color_t));
}

static int cogito_text_wrap_height(const CogitoNode* n, const char* text, int size, int max_w) {
  if (!text || max_w <= 0) return 0;
  if (size <= 0) size = cogito_font_size();
  int line_h = cogito_text_height_size(size);
  int lines = 0;
  const char* p = text;
  while (*p) {
    while (*p == '\n') { lines++; p++; }
    if (!*p) break;
    int line_len = 0;
    int line_w = 0;
    int last_space = -1;
    int last_space_w = 0;
    const char* start = p;
    while (*p && *p != '\n') {
      int w = cogito_text_width_size_n_node(n, start, line_len + 1, size);
      if (*p == ' ' || *p == '\t') {
        last_space = line_len;
        last_space_w = w;
      }
      if (w > max_w && line_len > 0) {
        if (last_space >= 0) {
          line_len = last_space;
          line_w = last_space_w;
          p = start + line_len + 1;
        } else {
          line_w = cogito_text_width_size_n_node(n, start, line_len, size);
        }
        break;
      }
      line_len++;
      line_w = w;
      p++;
    }
    if (*p == '\n') p++;
    (void)line_w;
    lines++;
  }
  if (lines == 0) lines = 1;
  return lines * line_h;
}

static void cogito_draw_text_size_node(const CogitoNode* n, int x, int y, const char* text, Color color, int size, bool bold);

static void cogito_draw_text_ellipsis(const CogitoNode* n, int x, int y, int w, const char* text, Color color, int size, bool bold, int align) {
  if (!text || w <= 0) return;
  if (size <= 0) size = cogito_font_size();
  int len = (int)strlen(text);
  int full_w = cogito_text_width_size_node(n, text, size);
  if (full_w <= w) {
    int tx = x;
    if (align == 1) tx = x + (w - full_w) / 2;
    else if (align == 2) tx = x + (w - full_w);
    cogito_draw_text_size_node(n, tx, y, text, color, size, bold);
    return;
  }
  const char* dots = "...";
  int dots_w = cogito_text_width_size_node(n, dots, size);
  int max_w = w - dots_w;
  if (max_w < 0) max_w = 0;
  int cut = len;
  for (int i = 0; i < len; i++) {
    int ww = cogito_text_width_size_n_node(n, text, i + 1, size);
    if (ww > max_w) { cut = i; break; }
  }
  if (cut < 0) cut = 0;
  char buf[1024];
  if (cut > 1000) cut = 1000;
  memcpy(buf, text, (size_t)cut);
  buf[cut] = '\0';
  int line_w = cogito_text_width_size_node(n, buf, size) + dots_w;
  int tx = x;
  if (align == 1) tx = x + (w - line_w) / 2;
  else if (align == 2) tx = x + (w - line_w);
  cogito_draw_text_size_node(n, tx, y, buf, color, size, bold);
  cogito_draw_text_size_node(n, tx + cogito_text_width_size_node(n, buf, size), y, dots, color, size, bold);
}

static int cogito_draw_text_wrapped(const CogitoNode* n, int x, int y, int w, const char* text, Color color, int size, bool bold, int align, bool draw) {
  if (!text || w <= 0) return 0;
  if (size <= 0) size = cogito_font_size();
  int line_h = cogito_text_height_size(size);
  int lines = 0;
  const char* p = text;
  while (*p) {
    while (*p == '\n') { if (draw) { lines++; } else { lines++; } p++; }
    if (!*p) break;
    int line_len = 0;
    int last_space = -1;
    int last_space_w = 0;
    const char* start = p;
    int line_w = 0;
    while (*p && *p != '\n') {
      int wcur = cogito_text_width_size_n_node(n, start, line_len + 1, size);
      if (*p == ' ' || *p == '\t') {
        last_space = line_len;
        last_space_w = wcur;
      }
      if (wcur > w && line_len > 0) {
        if (last_space >= 0) {
          line_len = last_space;
          line_w = last_space_w;
          p = start + line_len + 1;
        } else {
          line_w = cogito_text_width_size_n_node(n, start, line_len, size);
        }
        break;
      }
      line_len++;
      line_w = wcur;
      p++;
    }
    if (line_len > 0) {
      int tx = x;
      if (align == 1) tx = x + (w - line_w) / 2;
      else if (align == 2) tx = x + (w - line_w);
      if (draw) {
        char buf[1024];
        int len_n = line_len;
        if (len_n > 1000) len_n = 1000;
        memcpy(buf, start, (size_t)len_n);
        buf[len_n] = '\0';
        cogito_draw_text_size_node(n, tx, y + lines * line_h, buf, color, size, bold);
      }
    }
    if (*p == '\n') p++;
    lines++;
  }
  if (lines == 0) lines = 1;
  return lines * line_h;
}

static bool cogito_debug_enabled(void) {
  const char* v = getenv("COGITO_DEBUG");
  return v && v[0] && v[0] != '0';
}

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
