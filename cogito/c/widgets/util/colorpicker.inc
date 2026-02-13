static void cogito_colorpicker_sync_hex(CogitoNode* n) {
  if (!n || n->kind != COGITO_COLORPICKER) return;
  CogitoColor c = cogito_hct_to_rgb(n->color_h, n->color_c, n->color_t);
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
  int preview_w = row_h;
  int preview_gap = 8;
  int hex_x = n->x + pad_l + preview_w + preview_gap;
  int hex_y = n->y + pad_t + (row_h + gap) * 3;
  int hex_w = n->w - pad_l - pad_r - preview_w - preview_gap;
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
