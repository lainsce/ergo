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

  // Calculate blur and spread based on level
  // Level 1: blur=3, spread=-0.5
  // Level 2: blur=6, spread=-1.5
  // Level 3: blur=12, spread=-3
  // Level 4: blur=24, spread=-6
  // Level 5: blur=24, spread=-12
  int blur = 3;
  int spread = 0;
  int offset_y = 0;
  
  switch(level) {
    case 1: blur = 3; spread = 0; offset_y = 1; break;   // -0.5 spread equivalent (0)
    case 2: blur = 6; spread = -1; offset_y = 2; break;  // -1.5 spread equivalent (-1)
    case 3: blur = 12; spread = -3; offset_y = 3; break; // -3 spread
    case 4: blur = 24; spread = -6; offset_y = 4; break; // -6 spread
    case 5: blur = 24; spread = -12; offset_y = 6; break; // -12 spread
  }

  // Consistent 6% opacity black (approximately 15 alpha)
  int alpha = 15;
  CogitoColor shadow_col = cogito_rgba(0, 0, 0, (uint8_t)alpha);
  
  // Draw ambient shadow (above the element)
  cogito_draw_shadow_blur_radii(x, y - offset_y, w, h, tl, tr, br, bl, 
                                 spread + 1, blur, shadow_col);
  
  // Draw key shadow (offset below the element)
  cogito_draw_shadow_blur_radii(x, y + offset_y, w, h, tl, tr, br, bl, 
                                 spread, blur, shadow_col);
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

// Window decoration (traffic lights / CSD buttons)
#include "widgets/util/traffic_lights.inc"

// Inspector panel (debug overlay)
#include "widgets/util/inspector.inc"

// Scrollbar geometry and hit-test (overlay scrollbars)
#include "widgets/util/scrollbar.inc"

static void cogito_draw_node(CogitoNode* n);

#include "widgets/draw/toasts.inc"


static void cogito_draw_node(CogitoNode* n) {
  if (!n) return;
  // Use the cached main pointer position (set once per frame in the event loop)
  // instead of calling get_mouse_position per-node which is a backend API call.
  if (n->tooltip && n->tooltip->data && n->tooltip->data[0] && !n->disabled) {
    if (cogito_main_pointer_valid &&
        cogito_hit_node(n, cogito_main_pointer_x, cogito_main_pointer_y)) {
      cogito_tooltip_hovered = n;
    }
  }
  switch (n->kind) {

    #include "widgets/draw/button.inc"

    #include "widgets/draw/carousel.inc"

    #include "widgets/draw/carousel_item.inc"

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

case COGITO_TOASTS:
      if (!cogito_toasts_force_draw) {
        CogitoNode* win = cogito_node_window(n);
        if (win) break;
      }
      for (size_t i = 0; i < n->len; i++) {
        cogito_draw_node(n->children[i]);
      }
      break;

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

    #include "widgets/draw/card.inc"

    #include "widgets/draw/avatar.inc"

    #include "widgets/draw/badge.inc"

    #include "widgets/draw/banner.inc"

    #include "widgets/draw/bottom_sheet.inc"

    #include "widgets/draw/timepicker.inc"

    #include "widgets/draw/active_indicator.inc"

    #include "widgets/draw/switchbar.inc"

    #include "widgets/draw/content_list.inc"

    #include "widgets/draw/empty_page.inc"

    #include "widgets/draw/tip_view.inc"

    #include "widgets/draw/settings_window.inc"

    #include "widgets/draw/settings_page.inc"

    #include "widgets/draw/settings_list.inc"

    #include "widgets/draw/settings_row.inc"

    #include "widgets/draw/welcome_screen.inc"

    #include "widgets/draw/view_dual.inc"

    #include "widgets/draw/view_chooser.inc"

    #include "widgets/draw/about_window.inc"

    #include "widgets/draw/split_button.inc"

    case COGITO_KIND_COUNT:
      break;
  

}
}

// Overlay drawing (menu, tooltip)
#include "widgets/draw/menu.inc"
#include "widgets/draw/tooltip_overlay.inc"
