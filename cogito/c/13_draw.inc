static float cogito_roundness_for(int w, int h, int radius) {
  if (radius <= 0) return 0.0f;
  float min = (float)(w < h ? w : h);
  if (min <= 0.0f) return 0.0f;
  float roundness = (float)radius / (min * 0.5f);
  if (roundness > 1.0f) roundness = 1.0f;
  if (roundness < 0.0f) roundness = 0.0f;
  return roundness;
}

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

static int cogito_int_max(int a, int b) {
  return a > b ? a : b;
}

static void cogito_clamp_corner_radii(int w, int h, int* tl, int* tr, int* br, int* bl) {
  if (!tl || !tr || !br || !bl) return;
  int max_r = (w < h ? w : h) / 2;
  if (max_r < 0) max_r = 0;
  if (*tl < 0) *tl = 0;
  if (*tr < 0) *tr = 0;
  if (*br < 0) *br = 0;
  if (*bl < 0) *bl = 0;
  if (*tl > max_r) *tl = max_r;
  if (*tr > max_r) *tr = max_r;
  if (*br > max_r) *br = max_r;
  if (*bl > max_r) *bl = max_r;
}

static bool cogito_corner_radii_uniform(int tl, int tr, int br, int bl) {
  return tl == tr && tr == br && br == bl;
}

static void cogito_draw_rect_plain(int x, int y, int w, int h, CogitoColor color) {
  if (w <= 0 || h <= 0) return;
  if (cogito_backend && cogito_backend->draw_rect) {
    cogito_backend->draw_rect(x, y, w, h, color);
  }
}

static void cogito_draw_corner_fill(int cx, int cy, int r, int sx, int sy, int sw, int sh, CogitoColor color) {
  if (r <= 0 || sw <= 0 || sh <= 0) return;
  if (!cogito_backend || !cogito_backend->draw_circle) return;
  if (cogito_backend->begin_scissor && cogito_backend->end_scissor) {
    cogito_backend->begin_scissor(sx, sy, sw, sh);
    cogito_backend->draw_circle(cx, cy, (float)r, color);
    cogito_backend->end_scissor();
  } else {
    cogito_backend->draw_circle(cx, cy, (float)r, color);
  }
}

static void cogito_draw_corner_outline(int cx, int cy, int r, int sx, int sy, int sw, int sh, CogitoColor color, int thickness) {
  if (r <= 0 || sw <= 0 || sh <= 0 || thickness <= 0) return;
  if (!cogito_backend || !cogito_backend->draw_circle_lines) return;
  if (cogito_backend->begin_scissor && cogito_backend->end_scissor) {
    cogito_backend->begin_scissor(sx, sy, sw, sh);
    cogito_backend->draw_circle_lines(cx, cy, (float)r, color, thickness);
    cogito_backend->end_scissor();
  } else {
    cogito_backend->draw_circle_lines(cx, cy, (float)r, color, thickness);
  }
}

static void cogito_draw_rect_radii(int x, int y, int w, int h, CogitoColor bg, int tl, int tr, int br, int bl) {
  if (w <= 0 || h <= 0) return;
  cogito_clamp_corner_radii(w, h, &tl, &tr, &br, &bl);
  if (cogito_corner_radii_uniform(tl, tr, br, bl)) {
    cogito_draw_rect(x, y, w, h, bg, tl);
    return;
  }

  int top = cogito_int_max(tl, tr);
  int bottom = cogito_int_max(bl, br);
  int left = cogito_int_max(tl, bl);
  int right = cogito_int_max(tr, br);

  cogito_draw_rect_plain(x + left, y, w - left - right, h, bg);
  cogito_draw_rect_plain(x, y + tl, left, h - tl - bl, bg);
  cogito_draw_rect_plain(x + w - right, y + tr, right, h - tr - br, bg);
  cogito_draw_rect_plain(x + tl, y, w - tl - tr, top, bg);
  cogito_draw_rect_plain(x + bl, y + h - bottom, w - bl - br, bottom, bg);

  cogito_draw_corner_fill(x + tl, y + tl, tl, x, y, tl + 1, tl + 1, bg);
  cogito_draw_corner_fill(x + w - tr - 1, y + tr, tr, x + w - tr - 1, y, tr + 1, tr + 1, bg);
  cogito_draw_corner_fill(x + w - br - 1, y + h - br - 1, br, x + w - br - 1, y + h - br - 1, br + 1, br + 1, bg);
  cogito_draw_corner_fill(x + bl, y + h - bl - 1, bl, x, y + h - bl - 1, bl + 1, bl + 1, bg);
}

static void cogito_draw_rect_lines_radii(int x, int y, int w, int h, CogitoColor border, int tl, int tr, int br, int bl, int thickness) {
  if (w <= 0 || h <= 0 || thickness <= 0) return;
  cogito_clamp_corner_radii(w, h, &tl, &tr, &br, &bl);
  if (cogito_corner_radii_uniform(tl, tr, br, bl)) {
    cogito_draw_rect_lines(x, y, w, h, border, tl, thickness);
    return;
  }

  cogito_draw_rect_plain(x + tl, y, w - tl - tr, thickness, border);
  cogito_draw_rect_plain(x + bl, y + h - thickness, w - bl - br, thickness, border);
  cogito_draw_rect_plain(x, y + tl, thickness, h - tl - bl, border);
  cogito_draw_rect_plain(x + w - thickness, y + tr, thickness, h - tr - br, border);

  cogito_draw_corner_outline(x + tl, y + tl, tl, x, y, tl + thickness, tl + thickness, border, thickness);
  cogito_draw_corner_outline(x + w - tr - 1, y + tr, tr, x + w - tr - thickness, y, tr + thickness, tr + thickness, border, thickness);
  cogito_draw_corner_outline(x + w - br - 1, y + h - br - 1, br, x + w - br - thickness, y + h - br - thickness, br + thickness, br + thickness, border, thickness);
  cogito_draw_corner_outline(x + bl, y + h - bl - 1, bl, x, y + h - bl - thickness, bl + thickness, bl + thickness, border, thickness);
}

static void cogito_resolve_node_radii(const CogitoNode* n, const CogitoStyle* s, int fallback_radius,
                                      int w, int h, int* out_tl, int* out_tr, int* out_br, int* out_bl) {
  int tl = fallback_radius;
  int tr = fallback_radius;
  int br = fallback_radius;
  int bl = fallback_radius;
  if (n) {
    if (n->radius_tl_set) tl = n->radius_tl;
    if (n->radius_tr_set) tr = n->radius_tr;
    if (n->radius_br_set) br = n->radius_br;
    if (n->radius_bl_set) bl = n->radius_bl;
  }
  if (s) {
    if (s->has_radius) {
      tl = s->radius;
      tr = s->radius;
      br = s->radius;
      bl = s->radius;
    }
    if (s->has_radius_tl) tl = s->radius_tl;
    if (s->has_radius_tr) tr = s->radius_tr;
    if (s->has_radius_br) br = s->radius_br;
    if (s->has_radius_bl) bl = s->radius_bl;
  }
  cogito_clamp_corner_radii(w, h, &tl, &tr, &br, &bl);
  if (out_tl) *out_tl = tl;
  if (out_tr) *out_tr = tr;
  if (out_br) *out_br = br;
  if (out_bl) *out_bl = bl;
}

static void cogito_draw_rect_node(const CogitoNode* n, const CogitoStyle* s,
                                  int x, int y, int w, int h, CogitoColor bg, int fallback_radius) {
  int tl = fallback_radius, tr = fallback_radius, br = fallback_radius, bl = fallback_radius;
  cogito_resolve_node_radii(n, s, fallback_radius, w, h, &tl, &tr, &br, &bl);
  cogito_draw_rect_radii(x, y, w, h, bg, tl, tr, br, bl);
}

static void cogito_draw_rect_lines_node(const CogitoNode* n, const CogitoStyle* s,
                                        int x, int y, int w, int h, CogitoColor border, int fallback_radius, int thickness) {
  int tl = fallback_radius, tr = fallback_radius, br = fallback_radius, bl = fallback_radius;
  cogito_resolve_node_radii(n, s, fallback_radius, w, h, &tl, &tr, &br, &bl);
  cogito_draw_rect_lines_radii(x, y, w, h, border, tl, tr, br, bl, thickness);
}

static float cogito_shadow_gauss(float x, float sigma) {
  if (sigma <= 0.0001f) return 0.0f;
  return expf(-(x * x) / (2.0f * sigma * sigma));
}

static void cogito_draw_shadow_blur_radii(int x, int y, int w, int h,
                                          int tl, int tr, int br, int bl,
                                          int spread, int blur, CogitoColor color) {
  if (w <= 0 || h <= 0 || color.a == 0) return;
  if (spread < 0) spread = 0;
  if (blur < 0) blur = 0;
  int steps = blur > 0 ? blur : 1;
  if (steps > 48) steps = 48;
  float sigma = (float)(blur > 0 ? blur : 1) * 0.62f;

  for (int d = steps; d >= 0; d--) {
    float dist = (float)d;
    float weight = cogito_shadow_gauss(dist, sigma);
    int a = (int)lroundf((float)color.a * 2.2f * weight / (float)(steps + 1));
    if (a < 1) continue;
    if (a > 255) a = 255;
    CogitoColor c = color;
    c.a = (uint8_t)a;
    int s = spread + d;
    cogito_draw_rect_radii(x - s, y - s, w + s * 2, h + s * 2, c, tl + s, tr + s, br + s, bl + s);
  }
}

static void cogito_draw_shadow_radii_level(int x, int y, int w, int h,
                                           int tl, int tr, int br, int bl,
                                           int level) {
  if (level <= 0 || w <= 0 || h <= 0) return;
  if (level > 5) level = 5;

  int blur = 4 + level * 3;
  int spread = level;
  int key_offset_y = level;
  int ambient_offset_y = level / 2;
  int alpha = 28 + level * 14;
  if (alpha > 150) alpha = 150;

  CogitoColor ambient = cogito_rgba(0, 0, 0, (uint8_t)((alpha * 3) / 5));
  CogitoColor key = cogito_rgba(0, 0, 0, (uint8_t)((alpha * 4) / 5));

  cogito_draw_shadow_blur_radii(x, y + ambient_offset_y, w, h, tl, tr, br, bl, spread + 1, blur + 2, ambient);
  cogito_draw_shadow_blur_radii(x, y + key_offset_y, w, h, tl, tr, br, bl, spread, blur, key);
}

static void cogito_draw_shadow_radii_box(int x, int y, int w, int h,
                                         int tl, int tr, int br, int bl,
                                         const CogitoBoxShadow* bs) {
  if (!bs || bs->inset || w <= 0 || h <= 0 || bs->color.a == 0) return;
  int dx = (int)lroundf(bs->dx);
  int dy = (int)lroundf(bs->dy);
  int spread = (int)lroundf(bs->spread);
  int blur = (int)lroundf(bs->blur);
  if (blur < 0) blur = 0;

  int bx = x - spread;
  int by = y - spread;
  int bw = w + spread * 2;
  int bh = h + spread * 2;
  int btl = tl + spread;
  int btr = tr + spread;
  int bbr = br + spread;
  int bbl = bl + spread;
  if (btl < 0) btl = 0;
  if (btr < 0) btr = 0;
  if (bbr < 0) bbr = 0;
  if (bbl < 0) bbl = 0;
  if (bw <= 0 || bh <= 0) return;

  cogito_draw_shadow_blur_radii(bx + dx, by + dy, bw, bh, btl, btr, bbr, bbl, 0, blur, bs->color);
}

static void cogito_draw_shadow_node_level(const CogitoNode* n, const CogitoStyle* s,
                                          int x, int y, int w, int h, int fallback_radius, int level) {
  int tl = fallback_radius, tr = fallback_radius, br = fallback_radius, bl = fallback_radius;
  cogito_resolve_node_radii(n, s, fallback_radius, w, h, &tl, &tr, &br, &bl);
  cogito_draw_shadow_radii_level(x, y, w, h, tl, tr, br, bl, level);
}

static void cogito_draw_shadow_node_box(const CogitoNode* n, const CogitoStyle* s,
                                        int x, int y, int w, int h, int fallback_radius, const CogitoBoxShadow* bs) {
  int tl = fallback_radius, tr = fallback_radius, br = fallback_radius, bl = fallback_radius;
  cogito_resolve_node_radii(n, s, fallback_radius, w, h, &tl, &tr, &br, &bl);
  cogito_draw_shadow_radii_box(x, y, w, h, tl, tr, br, bl, bs);
}

static void cogito_draw_line(int x1, int y1, int x2, int y2, CogitoColor color) {
  if (cogito_backend && cogito_backend->draw_line) {
    cogito_backend->draw_line(x1, y1, x2, y2, color, 1);
  }
}

static const char* cogito_icon_text_fallback(const char* icon) {
  if (!icon || !icon[0]) return "?";
  if (strncmp(icon, "sf:", 3) == 0) return cogito_icon_text_fallback(icon + 3);
  if (strstr(icon, "window-close")) return "x";
  if (strstr(icon, "window-minimize")) return "-";
  if (strstr(icon, "window-maximize")) return "+";
  if (strcmp(icon, "x") == 0 || strstr(icon, "xmark")) return "x";
  if (strstr(icon, "plus")) return "+";
  if (strstr(icon, "minus")) return "-";
  if (strstr(icon, "add")) return "+";
  if (strstr(icon, "remove")) return "-";
  if (strstr(icon, "check")) return "v";
  if (strstr(icon, "star")) return "*";
  if (strstr(icon, "photo") || strstr(icon, "image")) return "[]";
  if (strstr(icon, "search") || strstr(icon, "magnifyingglass")) return "o";
  if (strstr(icon, "chevron") || strstr(icon, "pan-")) return "v";
  if (strstr(icon, "arrow")) return ">";
  if (strstr(icon, "question") || strstr(icon, "help")) return "?";
  if (strstr(icon, "equal")) return "=";
  return icon;
}

static void cogito_draw_icon_fallback(CogitoNode* n, const char* icon, int x, int y, int size, CogitoColor color) {
  if (!n || !icon || !icon[0] || size <= 0) return;
  if (cogito_backend && cogito_backend->draw_texture) {
    CogitoTexture* tex = NULL;
    int tw = 0;
    int th = 0;
    if (cogito_icon_load_texture(icon, size, &tex, &tw, &th) && tex) {
      CogitoRect src = {0, 0, tw, th};
      int dx = x;
      int dy = y;
      int dw = size;
      int dh = size;
      if (tw > 0 && th > 0) {
        if (tw > th) {
          dh = (size * th + tw / 2) / tw;
          if (dh < 1) dh = 1;
          dy = y + (size - dh) / 2;
        } else if (th > tw) {
          dw = (size * tw + th / 2) / th;
          if (dw < 1) dw = 1;
          dx = x + (size - dw) / 2;
        }
      }
      CogitoRect dst = {dx, dy, dw, dh};
      cogito_backend->draw_texture(tex, src, dst, color);
      return;
    }
  }
  const char* icon_name = (strncmp(icon, "sf:", 3) == 0) ? icon + 3 : icon;

  if (strstr(icon_name, "magnifyingglass") || strstr(icon_name, "search")) {
    int cx = x + size / 2 - 1;
    int cy = y + size / 2 - 1;
    int r = size / 2 - 3;
    if (r < 2) r = 2;
    cogito_draw_rect(cx - r, cy - r, r * 2, r * 2, color, r);
    cogito_draw_line(x + size - 4, y + size - 4, x + size, y + size, color);
    return;
  }
  if (strstr(icon_name, "plus") || strstr(icon_name, "add")) {
    int cx = x + size / 2;
    int cy = y + size / 2;
    int r = size / 2 - 2;
    if (r < 2) r = 2;
    cogito_draw_line(cx - r, cy, cx + r, cy, color);
    cogito_draw_line(cx, cy - r, cx, cy + r, color);
    return;
  }
  if (strstr(icon_name, "minus") || strstr(icon_name, "remove")) {
    int cx = x + size / 2;
    int cy = y + size / 2;
    int r = size / 2 - 2;
    if (r < 2) r = 2;
    cogito_draw_line(cx - r, cy, cx + r, cy, color);
    return;
  }
  if (strstr(icon_name, "xmark") || strcmp(icon_name, "x") == 0 || strstr(icon_name, "window-close")) {
    cogito_draw_line(x, y, x + size, y + size, color);
    cogito_draw_line(x + size, y, x, y + size, color);
    return;
  }
  if (strstr(icon_name, "photo") || strstr(icon_name, "image")) {
    int r = size / 8;
    if (r < 2) r = 2;
    cogito_draw_rect_lines(x, y, size, size, color, r, 1);
    cogito_draw_line(x + 3, y + size - 4, x + size / 2, y + size / 2, color);
    cogito_draw_line(x + size / 2, y + size / 2, x + size - 3, y + size - 6, color);
    return;
  }
  if (strstr(icon_name, "chevron") || strstr(icon_name, "pan-")) {
    int mid = x + size / 2;
    int top = y + size / 3;
    int bot = y + (size * 2) / 3;
    cogito_draw_line(mid - 4, top, mid, bot, color);
    cogito_draw_line(mid, bot, mid + 4, top, color);
    return;
  }

  int fs = size;
  if (fs < 10) fs = 10;
  const char* txt = cogito_icon_text_fallback(icon);
  int tw = cogito_text_width_size_node(n, txt, fs);
  int th = cogito_text_height_size(fs);
  int tx = x + (size - tw) / 2;
  int ty = y + (size - th) / 2;
  cogito_draw_text_size_node(n, tx, ty, txt, color, fs, false);
}

typedef struct {
  bool over_close;
  bool over_min;
  bool over_max;
  bool over_cluster;
  int state_close;
  int state_min;
  int state_max;
  int close_lx;
  int min_lx;
  int max_lx;
  int lights_y;
  int light_d;
  int light_rad;
} CogitoTrafficLightsState;

static void cogito_traffic_lights_compute(int close_x, int min_x, int max_x, int by, int btn,
                                          int mx, int my, bool down,
                                          CogitoTrafficLightsState* out) {
  if (!out) return;
  memset(out, 0, sizeof(*out));
  if (btn <= 0) return;

  int center_off = btn / 2;
  float light_r = (btn / 14.0f) * 9.0f;
  int light_d = (int)(light_r * 2.0f + 0.5f);
  if (light_d < 2) light_d = 2;

  out->light_d = light_d;
  out->light_rad = light_d / 2;
  out->lights_y = by + center_off - light_d / 2;
  out->close_lx = close_x + center_off - light_d / 2;
  out->min_lx = min_x + center_off - light_d / 2;
  out->max_lx = max_x + center_off - light_d / 2;

  out->over_close = (close_x >= 0) && cogito_hit_rect(mx, my, close_x, by, btn, btn);
  out->over_min = (min_x >= 0) && cogito_hit_rect(mx, my, min_x, by, btn, btn);
  out->over_max = (max_x >= 0) && cogito_hit_rect(mx, my, max_x, by, btn, btn);

  int cluster_left = 0;
  int cluster_right = 0;
  bool cluster_init = false;
  if (close_x >= 0) {
    cluster_left = close_x;
    cluster_right = close_x + btn;
    cluster_init = true;
  }
  if (min_x >= 0) {
    if (!cluster_init || min_x < cluster_left) cluster_left = min_x;
    if (!cluster_init || min_x + btn > cluster_right) cluster_right = min_x + btn;
    cluster_init = true;
  }
  if (max_x >= 0) {
    if (!cluster_init || max_x < cluster_left) cluster_left = max_x;
    if (!cluster_init || max_x + btn > cluster_right) cluster_right = max_x + btn;
    cluster_init = true;
  }
  if (cluster_init) {
    out->over_cluster = cogito_hit_rect(mx, my, cluster_left, by, cluster_right - cluster_left, btn);
  }

  out->state_close = out->over_close ? (down ? 2 : 1) : 0;
  out->state_min = out->over_min ? (down ? 2 : 1) : 0;
  out->state_max = out->over_max ? (down ? 2 : 1) : 0;
}

static void cogito_draw_traffic_lights_styled(const CogitoStyle* base_style,
                                              CogitoColor fallback_btn,
                                              CogitoColor fallback_border,
                                              int close_x, int min_x, int max_x,
                                              const CogitoTrafficLightsState* st) {
  if (!st) return;
  CogitoStyle empty = {0};
  const CogitoStyle* s = base_style ? base_style : &empty;

  CogitoStyle close_s = cogito_appbar_win_btn_style(0, st->state_close);
  CogitoStyle min_s = cogito_appbar_win_btn_style(1, st->state_min);
  CogitoStyle max_s = cogito_appbar_win_btn_style(2, st->state_max);

  CogitoColor close_bg = close_s.has_bg ? close_s.bg
    : (s->has_appbar_btn_close_color ? s->appbar_btn_close_color : fallback_btn);
  CogitoColor min_bg = min_s.has_bg ? min_s.bg
    : (s->has_appbar_btn_min_color ? s->appbar_btn_min_color : fallback_btn);
  CogitoColor max_bg = max_s.has_bg ? max_s.bg
    : (s->has_appbar_btn_max_color ? s->appbar_btn_max_color : fallback_btn);
  CogitoColor glyph_fallback = s->has_text ? s->text : cogito_rgba(28, 28, 28, 220);
  CogitoColor close_glyph = close_s.has_text ? close_s.text : glyph_fallback;
  CogitoColor min_glyph = min_s.has_text ? min_s.text : glyph_fallback;
  CogitoColor max_glyph = max_s.has_text ? max_s.text : glyph_fallback;

  if (close_x >= 0) {
    cogito_draw_rect(st->close_lx, st->lights_y, st->light_d, st->light_d, close_bg, st->light_rad);
  }
  if (min_x >= 0) {
    cogito_draw_rect(st->min_lx, st->lights_y, st->light_d, st->light_d, min_bg, st->light_rad);
  }
  if (max_x >= 0) {
    cogito_draw_rect(st->max_lx, st->lights_y, st->light_d, st->light_d, max_bg, st->light_rad);
  }

  if (close_x >= 0 && (close_s.has_border || close_s.has_border_width)) {
    int bw = close_s.has_border_width ? close_s.border_width : 1;
    CogitoColor bc = close_s.has_border ? close_s.border : fallback_border;
    if (bw > 0) {
      cogito_draw_rect_lines(st->close_lx, st->lights_y, st->light_d, st->light_d, bc, st->light_rad, bw);
    }
  }
  if (min_x >= 0 && (min_s.has_border || min_s.has_border_width)) {
    int bw = min_s.has_border_width ? min_s.border_width : 1;
    CogitoColor bc = min_s.has_border ? min_s.border : fallback_border;
    if (bw > 0) {
      cogito_draw_rect_lines(st->min_lx, st->lights_y, st->light_d, st->light_d, bc, st->light_rad, bw);
    }
  }
  if (max_x >= 0 && (max_s.has_border || max_s.has_border_width)) {
    int bw = max_s.has_border_width ? max_s.border_width : 1;
    CogitoColor bc = max_s.has_border ? max_s.border : fallback_border;
    if (bw > 0) {
      cogito_draw_rect_lines(st->max_lx, st->lights_y, st->light_d, st->light_d, bc, st->light_rad, bw);
    }
  }

  if (!st->over_cluster) return;
  int glyph_d = (st->light_d * 5) / 10;
  if (glyph_d < 2) glyph_d = 2;
  if (glyph_d > st->light_d) glyph_d = st->light_d;
  int inset = (st->light_d - glyph_d) / 2;

  if (close_x >= 0) {
    int x0 = st->close_lx + inset;
    int y0 = st->lights_y + inset;
    int x1 = st->close_lx + st->light_d - inset - 1;
    int y1 = st->lights_y + st->light_d - inset - 1;
    if (x1 < x0) x1 = x0;
    if (y1 < y0) y1 = y0;
    cogito_draw_line(x0, y0, x1, y1, close_glyph);
    cogito_draw_line(x1, y0, x0, y1, close_glyph);
  }

  if (min_x >= 0) {
    int x0 = st->min_lx + inset;
    int x1 = st->min_lx + st->light_d - inset - 1;
    int y = st->lights_y + st->light_d / 2;
    if (x1 < x0) x1 = x0;
    cogito_draw_line(x0, y, x1, y, min_glyph);
  }

  if (max_x >= 0) {
    int x0 = st->max_lx + inset;
    int x1 = st->max_lx + st->light_d - inset - 1;
    int y = st->lights_y + st->light_d / 2;
    int x = st->max_lx + st->light_d / 2;
    int y0 = st->lights_y + inset;
    int y1 = st->lights_y + st->light_d - inset - 1;
    if (x1 < x0) x1 = x0;
    if (y1 < y0) y1 = y0;
    cogito_draw_line(x0, y, x1, y, max_glyph);
    cogito_draw_line(x, y0, x, y1, max_glyph);
  }
}

static void cogito_inspector_color_hex(CogitoColor c, char* out, size_t cap) {
  if (!out || cap == 0) return;
  if (c.a == 255) snprintf(out, cap, "#%02X%02X%02X", (unsigned int)c.r, (unsigned int)c.g, (unsigned int)c.b);
  else snprintf(out, cap, "#%02X%02X%02X%02X", (unsigned int)c.r, (unsigned int)c.g, (unsigned int)c.b, (unsigned int)c.a);
}

static void cogito_inspector_panel_rect(CogitoNode* win, int* out_x, int* out_y, int* out_w, int* out_h, int* out_tree_w) {
  if (!win) return;
  int view_x = cogito_inspector_view_x;
  int view_y = cogito_inspector_view_y;
  int view_w = cogito_inspector_view_w > 0 ? cogito_inspector_view_w : win->w;
  int view_h = cogito_inspector_view_h > 0 ? cogito_inspector_view_h : win->h;
  if (view_w < 0) view_w = 0;
  if (view_h < 0) view_h = 0;
  int panel_w = view_w / 2;
  if (panel_w < 320) panel_w = view_w;
  if (panel_w > 520) panel_w = 520;
  int x = view_x + view_w - panel_w;
  if (x < 0) x = 0;
  int y = view_y;
  int h = view_h;
  int tree_w = (panel_w * 45) / 100;
  if (tree_w < 170) tree_w = panel_w / 2;
  if (tree_w > panel_w - 120) tree_w = panel_w - 120;
  if (out_x) *out_x = x;
  if (out_y) *out_y = y;
  if (out_w) *out_w = panel_w;
  if (out_h) *out_h = h;
  if (out_tree_w) *out_tree_w = tree_w;
}

static void cogito_inspector_tree_metrics(int* out_header_h, int* out_row_h, int* out_pad) {
  if (out_header_h) *out_header_h = 32;
  if (out_row_h) *out_row_h = 22;
  if (out_pad) *out_pad = 6;
}

static bool cogito_inspector_is_collapsed(CogitoNode* n) {
  if (!n) return false;
  for (int i = 0; i < cogito_inspector_collapsed_count; i++) {
    if (cogito_inspector_collapsed_nodes[i] == n) {
      return cogito_inspector_collapsed_values[i];
    }
  }
  return false;
}

static void cogito_inspector_set_collapsed(CogitoNode* n, bool collapsed) {
  if (!n) return;
  for (int i = 0; i < cogito_inspector_collapsed_count; i++) {
    if (cogito_inspector_collapsed_nodes[i] == n) {
      if (collapsed) {
        cogito_inspector_collapsed_values[i] = true;
      } else {
        for (int j = i; j + 1 < cogito_inspector_collapsed_count; j++) {
          cogito_inspector_collapsed_nodes[j] = cogito_inspector_collapsed_nodes[j + 1];
          cogito_inspector_collapsed_values[j] = cogito_inspector_collapsed_values[j + 1];
        }
        cogito_inspector_collapsed_count--;
      }
      return;
    }
  }
  if (collapsed && cogito_inspector_collapsed_count < COGITO_INSPECTOR_MAX_NODES) {
    cogito_inspector_collapsed_nodes[cogito_inspector_collapsed_count] = n;
    cogito_inspector_collapsed_values[cogito_inspector_collapsed_count] = true;
    cogito_inspector_collapsed_count++;
  }
}

static void cogito_inspector_collect_nodes(CogitoNode* n, int depth) {
  if (!n) return;
  if (cogito_inspector_node_count >= COGITO_INSPECTOR_MAX_NODES) return;
  cogito_inspector_nodes[cogito_inspector_node_count] = n;
  cogito_inspector_depths[cogito_inspector_node_count] = depth;
  cogito_inspector_node_count++;
  if (n->len == 0 || cogito_inspector_is_collapsed(n)) return;
  for (size_t i = 0; i < n->len; i++) {
    cogito_inspector_collect_nodes(n->children[i], depth + 1);
  }
}

static void cogito_inspector_rebuild_tree(CogitoNode* win) {
  cogito_inspector_node_count = 0;
  cogito_inspector_collect_nodes(win, 0);
}

static void cogito_inspector_node_label(CogitoNode* n, char* out, size_t cap) {
  if (!out || cap == 0) return;
  if (!n) {
    snprintf(out, cap, "<null>");
    return;
  }
  const char* kind = cogito_kind_name(n->kind);
  const char* cls = (n->class_name && n->class_name->data && n->class_name->data[0]) ? n->class_name->data : NULL;
  if (cls) snprintf(out, cap, "%s.%s", kind, cls);
  else snprintf(out, cap, "%s", kind);
}

static CogitoNode* cogito_inspector_pick_node_recursive(CogitoNode* n, int x, int y) {
  if (!n || !cogito_hit_node(n, x, y)) return NULL;
  for (size_t i = n->len; i > 0; i--) {
    CogitoNode* hit = cogito_inspector_pick_node_recursive(n->children[i - 1], x, y);
    if (hit) return hit;
  }
  return n;
}

static bool cogito_inspector_handle_click(CogitoNode* win, int x, int y) {
  if (!cogito_inspector_open || !win) return false;
  int px = 0, py = 0, pw = 0, ph = 0, tree_w = 0;
  cogito_inspector_panel_rect(win, &px, &py, &pw, &ph, &tree_w);
  if (!cogito_hit_rect(x, y, px, py, pw, ph)) return false;

  cogito_inspector_rebuild_tree(win);
  int header_h = 0;
  int row_h = 0;
  int pad = 0;
  cogito_inspector_tree_metrics(&header_h, &row_h, &pad);
  int list_y = py + header_h + 4;
  int list_h = ph - (header_h + 8);
  if (cogito_hit_rect(x, y, px, list_y, tree_w, list_h)) {
    int row = (y - list_y) / row_h;
    if (row >= 0 && row < cogito_inspector_node_count) {
      CogitoNode* n = cogito_inspector_nodes[row];
      int depth = cogito_inspector_depths[row];
      int row_y = list_y + row * row_h;
      int indent_x = px + pad + depth * 12;
      int chevron_x = indent_x;
      int chevron_y = row_y + (row_h - 12) / 2;
      int chevron_w = 10;
      int chevron_h = 12;
      if (n && n->len > 0 && cogito_hit_rect(x, y, chevron_x, chevron_y, chevron_w, chevron_h)) {
        cogito_inspector_set_collapsed(n, !cogito_inspector_is_collapsed(n));
      } else {
        cogito_inspector_selected = n;
      }
    }
  }
  return true;
}

static void cogito_draw_inspector_highlight(void) {
  if (!cogito_inspector_open || !cogito_inspector_selected) return;
  CogitoNode* n = cogito_inspector_selected;
  if (n->w <= 0 || n->h <= 0) return;
  CogitoColor red = cogito_rgba(220, 44, 44, 255);
  cogito_draw_rect_lines(n->x - 1, n->y - 1, n->w + 2, n->h + 2, red, n->border_radius, 2);
}

static int cogito_inspector_draw_prop_kv_row(CogitoNode* win, int x, int y, int w,
                                             const char* key, const char* value,
                                             CogitoColor key_color, CogitoColor value_color,
                                             CogitoColor row_bg, CogitoColor row_border,
                                             int text_size, int row_pad) {
  if (!win || !key || !value || w <= 0) return y;
  int row_h = cogito_text_height_size(text_size) + row_pad * 2;
  if (row_h < 16) row_h = 16;
  cogito_draw_rect(x, y, w, row_h, row_bg, 4);
  cogito_draw_rect_lines(x, y, w, row_h, row_border, 4, 1);

  int key_x = x + row_pad;
  int key_w = cogito_text_width_size_node(win, key, text_size);
  int value_w = cogito_text_width_size_node(win, value, text_size);
  int value_x = x + w - row_pad - value_w;
  int min_value_x = key_x + key_w + 10;
  if (value_x < min_value_x) value_x = min_value_x;

  cogito_draw_text_size_node(win, key_x, y + row_pad, key, key_color, text_size, false);
  cogito_draw_text_size_node(win, value_x, y + row_pad, value, value_color, text_size, false);
  return y + row_h + 4;
}

static int cogito_inspector_draw_geometry_state_widget(CogitoNode* win, int x, int y, int w,
                                                       int geom_w, int geom_h,
                                                       bool hover, bool active, bool focused, bool disabled,
                                                       CogitoColor text, CogitoColor muted) {
  if (!win || w <= 0) return y;
  const int label_size = 12;
  const int label_h = cogito_text_height_size(label_size);
  if (geom_w < 1) geom_w = 1;
  if (geom_h < 1) geom_h = 1;

  bool representation = (geom_w > 300 || geom_h > 300);
  int box_w = geom_w;
  int box_h = geom_h;
  if (representation) {
    float sx = 140.0f / (float)geom_w;
    float sy = 140.0f / (float)geom_h;
    float s = sx < sy ? sx : sy;
    if (s > 1.0f) s = 1.0f;
    box_w = (int)lroundf((float)geom_w * s);
    box_h = (int)lroundf((float)geom_h * s);
    if (box_w < 1) box_w = 1;
    if (box_h < 1) box_h = 1;
  }

  int canvas_pad = 8;
  int canvas_x = x + canvas_pad;
  int canvas_w = w - canvas_pad * 2;
  if (canvas_w < 16) canvas_w = 16;
  int box_x = x + (w - box_w) / 2;
  int top_y = y;
  int box_y = top_y + label_h + 4;

  char val[32];
  snprintf(val, sizeof(val), "w: %d%s", geom_w, representation ? " (repr)" : "");
  int tw = cogito_text_width_size_node(win, val, label_size);
  int wx = box_x + (box_w - tw) / 2;
  if (wx < canvas_x) wx = canvas_x;
  if (wx + tw > canvas_x + canvas_w) wx = canvas_x + canvas_w - tw;
  cogito_draw_text_size_node(win, wx, top_y, val, muted, label_size, false);

  CogitoColor box_bg = cogito_rgba(46, 204, 113, 50);
  CogitoColor box_border = cogito_rgba(22, 120, 66, 210);
  if (cogito_backend && cogito_backend->begin_scissor && cogito_backend->end_scissor) {
    cogito_backend->begin_scissor(canvas_x, box_y, canvas_w, box_h);
    cogito_draw_rect(box_x, box_y, box_w, box_h, box_bg, 0);
    cogito_draw_rect_lines(box_x, box_y, box_w, box_h, box_border, 0, 2);
    cogito_backend->end_scissor();
  } else {
    cogito_draw_rect(box_x, box_y, box_w, box_h, box_bg, 0);
    cogito_draw_rect_lines(box_x, box_y, box_w, box_h, box_border, 0, 2);
  }

  snprintf(val, sizeof(val), "h: %d", geom_h);
  int vh = cogito_text_height_size(label_size);
  int hw = cogito_text_width_size_node(win, val, label_size);
  int hx = x + w - hw - 2;
  int hy = box_y + (box_h - vh) / 2;
  cogito_draw_text_size_node(win, hx, hy, val, muted, label_size, false);

  int sy = box_y + box_h + 8;
  bool any = false;
  const char* states[4] = {"hover", "active", "focused", "disabled"};
  bool flags[4] = {hover, active, focused, disabled};
  for (int i = 0; i < 4; i++) {
    if (!flags[i]) continue;
    any = true;
    int sw = cogito_text_width_size_node(win, states[i], label_size);
    cogito_draw_text_size_node(win, x + (w - sw) / 2, sy, states[i], text, label_size, true);
    sy += label_h + 2;
  }
  if (!any) {
    const char* none = "no active state";
    int sw = cogito_text_width_size_node(win, none, label_size);
    cogito_draw_text_size_node(win, x + (w - sw) / 2, sy, none, muted, label_size, false);
    sy += label_h + 2;
  }
  return sy + 2;
}

static void cogito_draw_inspector_panel(CogitoNode* win) {
  if (!cogito_inspector_open || !win) return;
  cogito_inspector_rebuild_tree(win);
  if (!cogito_inspector_selected) cogito_inspector_selected = win;

  int px = 0, py = 0, pw = 0, ph = 0, tree_w = 0;
  cogito_inspector_panel_rect(win, &px, &py, &pw, &ph, &tree_w);
  int header_h = 0;
  int row_h = 0;
  int pad = 0;
  cogito_inspector_tree_metrics(&header_h, &row_h, &pad);
  const int text_size = 13;

  CogitoColor bg = cogito_rgba(18, 21, 26, 255);
  CogitoColor panel = cogito_rgba(23, 26, 32, 255);
  CogitoColor border = cogito_rgba(75, 82, 95, 255);
  CogitoColor text = cogito_rgba(220, 226, 235, 255);
  CogitoColor muted = cogito_rgba(160, 170, 184, 255);
  CogitoColor selected_bg = cogito_rgba(45, 74, 124, 255);
  CogitoColor prop_row_bg = cogito_rgba(28, 33, 40, 255);
  CogitoColor prop_row_bg_alt = cogito_rgba(32, 37, 46, 255);
  CogitoColor prop_row_border = cogito_rgba(75, 82, 95, 128);

  cogito_draw_rect(px, py, pw, ph, bg, 0);
  cogito_draw_rect(px, py, tree_w, ph, panel, 0);
  cogito_draw_line(px + tree_w, py, px + tree_w, py + ph, border);
  cogito_draw_line(px, py + header_h, px + pw, py + header_h, border);

  cogito_draw_text_size_node(win, px + pad, py + header_h - 16, "Widget Tree", muted, 13, false);
  cogito_draw_text_size_node(win, px + tree_w + pad, py + header_h - 16, "Selected Node", muted, 13, false);

  int list_y = py + header_h + 4;
  int visible_rows = (ph - (header_h + 8)) / row_h;
  if (visible_rows < 0) visible_rows = 0;
  for (int i = 0; i < cogito_inspector_node_count && i < visible_rows; i++) {
    int row_y = list_y + i * row_h;
    CogitoNode* n = cogito_inspector_nodes[i];
    int depth = cogito_inspector_depths[i];
    if (n == cogito_inspector_selected) {
      cogito_draw_rect(px + 1, row_y, tree_w - 2, row_h, selected_bg, 0);
    }
    int indent_x = px + pad + depth * 12;
    int label_x = indent_x;
    if (n && n->len > 0) {
      bool collapsed = cogito_inspector_is_collapsed(n);
      int cx = indent_x + 4;
      int cy = row_y + row_h / 2;
      if (collapsed) {
        cogito_draw_line(cx - 1, cy - 3, cx + 2, cy, muted);
        cogito_draw_line(cx - 1, cy + 3, cx + 2, cy, muted);
      } else {
        cogito_draw_line(cx - 3, cy - 1, cx, cy + 2, muted);
        cogito_draw_line(cx + 3, cy - 1, cx, cy + 2, muted);
      }
      label_x = indent_x + 12;
    }
    char label[128];
    cogito_inspector_node_label(n, label, sizeof(label));
    if (label_x > px + tree_w - 60) label_x = px + tree_w - 60;
    cogito_draw_text_size_node(win, label_x, row_y + 2, label, text, text_size, false);
  }

  CogitoNode* s = cogito_inspector_selected;
  if (!s) return;
  int rx = px + tree_w + pad;
  int rw = pw - tree_w - pad * 2;
  if (rw < 40) rw = 40;
  int ry = py + header_h + 6;
  const int row_pad = 6;
  bool row_alt = false;

  char line[192];
  char value[192];
  char cbuf1[20], cbuf2[20], cbuf3[20], cbuf4[20];

  cogito_inspector_node_label(s, line, sizeof(line));
  ry = cogito_inspector_draw_prop_kv_row(
    win, rx, ry, rw, "node", line, muted, text,
    row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, 13, row_pad
  );
  row_alt = !row_alt;

  bool hover = false, active = false;
  if (!s->disabled && cogito_backend_ready && cogito_backend) {
    cogito_node_hover_state(s, &hover, &active);
  }
  bool focused = (s == cogito_focused);
  bool disabled = s->disabled;

  cogito_inspector_color_hex(s->bg, cbuf1, sizeof(cbuf1));
  cogito_inspector_color_hex(s->text_color, cbuf2, sizeof(cbuf2));
  cogito_inspector_color_hex(s->border_color, cbuf3, sizeof(cbuf3));
  cogito_inspector_color_hex(s->selection_color, cbuf4, sizeof(cbuf4));

  ry = cogito_inspector_draw_prop_kv_row(
    win, rx, ry, rw, "background", cbuf1, muted, text,
    row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad
  );
  row_alt = !row_alt;
  ry = cogito_inspector_draw_prop_kv_row(
    win, rx, ry, rw, "color", cbuf2, muted, text,
    row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad
  );
  row_alt = !row_alt;
  ry = cogito_inspector_draw_prop_kv_row(
    win, rx, ry, rw, "border-color", cbuf3, muted, text,
    row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad
  );
  row_alt = !row_alt;
  ry = cogito_inspector_draw_prop_kv_row(
    win, rx, ry, rw, "selection-color", cbuf4, muted, text,
    row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad
  );
  row_alt = !row_alt;
  snprintf(value, sizeof(value), "%d", s->border_width);
  ry = cogito_inspector_draw_prop_kv_row(
    win, rx, ry, rw, "border-width", value, muted, text,
    row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad
  );
  row_alt = !row_alt;
  snprintf(value, sizeof(value), "%d", s->border_radius);
  ry = cogito_inspector_draw_prop_kv_row(
    win, rx, ry, rw, "border-radius", value, muted, text,
    row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad
  );
  row_alt = !row_alt;
  snprintf(value, sizeof(value), "%d %d %d %d", s->padding_top, s->padding_right, s->padding_bottom, s->padding_left);
  ry = cogito_inspector_draw_prop_kv_row(
    win, rx, ry, rw, "padding", value, muted, text,
    row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad
  );
  row_alt = !row_alt;
  snprintf(value, sizeof(value), "%d %d %d %d", s->margin_top, s->margin_right, s->margin_bottom, s->margin_left);
  ry = cogito_inspector_draw_prop_kv_row(
    win, rx, ry, rw, "margin", value, muted, text,
    row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad
  );
  row_alt = !row_alt;
  snprintf(value, sizeof(value), "%.2f", s->opacity);
  ry = cogito_inspector_draw_prop_kv_row(
    win, rx, ry, rw, "opacity", value, muted, text,
    row_alt ? prop_row_bg_alt : prop_row_bg, prop_row_border, text_size, row_pad
  );

  ry += 6;
  (void)cogito_inspector_draw_geometry_state_widget(
    win, rx, ry, rw,
    s->w, s->h,
    hover, active, focused, disabled,
    text, muted
  );
}

static void cogito_draw_node(CogitoNode* n);

static void cogito_draw_toasts_overlay(CogitoNode* n) {
  if (!n) return;
  if (n->kind == COGITO_TOASTS) {
    cogito_toasts_force_draw = true;
    cogito_draw_node(n);
    cogito_toasts_force_draw = false;
    return;
  }
  for (size_t i = 0; i < n->len; i++) {
    cogito_draw_toasts_overlay(n->children[i]);
  }
  if ((n->kind == COGITO_WINDOW || n->kind == COGITO_DIALOG_SLOT) &&
      n->dialog && n->dialog_open) {
    cogito_draw_toasts_overlay(n->dialog);
  }
}

static void cogito_draw_node(CogitoNode* n) {
  if (!n) return;
  if (n->tooltip && n->tooltip->data && n->tooltip->data[0] && !n->disabled) {
    int mx = 0, my = 0;
    if (cogito_backend && cogito_backend->get_mouse_position) {
      cogito_backend->get_mouse_position(&mx, &my);
    }
    if (cogito_hit_node(n, mx, my)) {
      cogito_tooltip_hovered = n;
    }
  }
  switch (n->kind) {

    #include "widgets/draw/button.inc"

    #include "widgets/draw/carousel.inc"

    #include "widgets/draw/iconbtn.inc"

    #include "widgets/draw/image.inc"

    #include "widgets/draw/label.inc"

    #include "widgets/draw/textfield.inc"

    #include "widgets/draw/textview.inc"

    #include "widgets/draw/dropdown.inc"

    #include "widgets/draw/datepicker.inc"

    #include "widgets/draw/fab.inc"

    #include "widgets/draw/nav_rail.inc"

    #include "widgets/draw/bottom_nav.inc"

    #include "widgets/draw/slider.inc"

    #include "widgets/draw/colorpicker.inc"

    #include "widgets/draw/tabs.inc"

    #include "widgets/draw/progress.inc"

    #include "widgets/draw/toast.inc"

    #include "widgets/draw/checkbox.inc"

    #include "widgets/draw/chip.inc"

    #include "widgets/draw/switch.inc"

    #include "widgets/draw/list.inc"

    #include "widgets/draw/grid.inc"

    #include "widgets/draw/appbar.inc"

    #include "widgets/draw/dialog.inc"

    #include "widgets/draw/popover.inc"

    #include "widgets/draw/bottom_toolbar.inc"

    #include "widgets/draw/dialog_slot.inc"

    #include "widgets/draw/view_switcher.inc"

    #include "widgets/draw/toasts.inc"

    #include "widgets/draw/vstack.inc"

    #include "widgets/draw/hstack.inc"

    #include "widgets/draw/zstack.inc"

    #include "widgets/draw/fixed.inc"

    #include "widgets/draw/window.inc"

    #include "widgets/draw/scroller.inc"

    #include "widgets/draw/searchfield.inc"

    #include "widgets/draw/stepper.inc"

    #include "widgets/draw/segmented.inc"

    #include "widgets/draw/treeview.inc"

    #include "widgets/draw/tooltip.inc"

    #include "widgets/draw/divider.inc"

    case COGITO_KIND_COUNT:
      break;
  

}
}

static void cogito_draw_menu(void) {
  if (!cogito_menu.open || !cogito_menu.owner) return;
  CogitoColor bg = cogito_theme.menu.has_bg ? cogito_theme.menu.bg : cogito_rgba(250, 250, 250, 255);
  CogitoColor border = cogito_theme.menu.has_border ? cogito_theme.menu.border : cogito_rgba(180, 180, 180, 255);
  CogitoColor text_color = cogito_theme.menu.has_text ? cogito_theme.menu.text : cogito_rgba(30, 30, 30, 255);
  CogitoColor sel = cogito_theme.menu.has_selection ? cogito_theme.menu.selection : cogito_rgba(220, 230, 250, 255);
  int radius = cogito_theme.menu.has_radius ? cogito_theme.menu.radius : 4;
  bool has_border = cogito_theme.menu.has_border || cogito_theme.menu.has_border_width;
  int border_w = cogito_theme.menu.has_border_width ? cogito_theme.menu.border_width : 1;
  int shadow_level = cogito_theme.menu.has_shadow ? cogito_theme.menu.shadow_level : 0;
  int font_size = cogito_font_size();
  if (cogito_theme.menu.has_font_size && cogito_theme.menu.font_size > 0) {
    font_size = cogito_theme.menu.font_size;
  }
  int x = cogito_menu.x;
  int y = cogito_menu.y;
  int w = cogito_menu.w;
  int h = cogito_menu.h;
  if (shadow_level > 0) {
    int tl = radius, tr = radius, br = radius, bl = radius;
    cogito_resolve_node_radii(NULL, &cogito_theme.menu, radius, w, h, &tl, &tr, &br, &bl);
    cogito_draw_shadow_radii_level(x, y, w, h, tl, tr, br, bl, shadow_level);
  }
  cogito_draw_rect_node(NULL, &cogito_theme.menu, x, y, w, h, bg, radius);
  if (has_border && border_w > 0) {
    cogito_draw_rect_lines_node(NULL, &cogito_theme.menu, x, y, w, h, border, radius, border_w);
  }
  if (cogito_menu.owner->kind == COGITO_DROPDOWN && w > 2) {
    int seam_y = -1;
    if (y == cogito_menu.owner->y + cogito_menu.owner->h) seam_y = y;
    if (y + h == cogito_menu.owner->y) seam_y = y + h - 1;
    if (seam_y >= y && seam_y < y + h) {
      cogito_draw_line(x + 1, seam_y, x + w - 2, seam_y, border);
    }
  }
  int mx = 0, my = 0;
  if (cogito_backend && cogito_backend->get_mouse_position) {
    cogito_backend->get_mouse_position(&mx, &my);
  }
  int hover = cogito_menu_hit(&cogito_menu, mx, my);
  int item_radius = cogito_theme.menu_item.has_radius ? cogito_theme.menu_item.radius : 0;
  for (size_t i = 0; i < cogito_menu.owner->menu_len; i++) {
    int iy = y + cogito_menu.pad_top + (int)i * cogito_menu.item_h;
    if ((int)i == hover) {
      int sel_radius = item_radius;
      if (sel_radius < 0) sel_radius = 0;
      cogito_draw_rect(x, iy, w, cogito_menu.item_h, sel, sel_radius);
    }
    const char* text = cogito_menu.owner->menu_labels[i] ? cogito_menu.owner->menu_labels[i]->data : "";
    int th = cogito_text_height_size(font_size);
    int ty = iy + (cogito_menu.item_h - th) / 2;
    cogito_draw_text_size_node(cogito_menu.owner, x + cogito_menu.pad_x, ty, text, text_color, font_size, false);
  }
}

static void cogito_draw_tooltip(CogitoNode* win, CogitoNode* target) {
  if (!win || !target || !target->tooltip || !target->tooltip->data) return;
  CogitoStyle s = cogito_theme_resolve(COGITO_TOOLTIP);
  CogitoColor bg = s.has_bg ? s.bg : cogito_rgba(30, 30, 30, 240);
  CogitoColor text = s.has_text ? s.text : cogito_rgba(255, 255, 255, 255);
  int text_l = cogito_luma(text);
  int bg_l = cogito_luma(bg);
  if (abs(text_l - bg_l) < 80) {
    text = cogito_on_color(bg);
  }
  CogitoColor border = s.has_border ? s.border : cogito_rgba(0, 0, 0, 0);
  int border_w = s.has_border_width ? s.border_width : 0;
  int radius = s.has_radius ? s.radius : 6;
  int font_size = s.has_font_size ? s.font_size : cogito_font_size();
  int pad_x = s.has_padding_left ? s.padding_left : 8;
  int pad_y = s.has_padding_top ? s.padding_top : 6;

  const char* t = target->tooltip->data;
  int tw = cogito_text_width_size_node(target, t, font_size);
  int th = cogito_text_height_size(font_size);
  int w = tw + pad_x * 2;
  int h = th + pad_y * 2;

  int mx = 0, my = 0;
  if (cogito_backend && cogito_backend->get_mouse_position) {
    cogito_backend->get_mouse_position(&mx, &my);
  }
  int x = mx + 12;
  int y = my + 16;
  if (x < 4) x = 4;
  if (x + w > win->w - 4) x = win->w - 4 - w;
  if (y + h > win->h - 4) y = win->h - 4 - h;
  if (y < 4) y = 4;

  cogito_draw_rect(x, y, w, h, bg, radius);
  if (border_w > 0) {
    cogito_draw_rect_lines(x, y, w, h, border, radius, border_w);
  }
  cogito_draw_text_size_node(target, x + pad_x, y + pad_y, t, text, font_size, false);
}
