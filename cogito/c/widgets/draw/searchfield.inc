case COGITO_SEARCHFIELD: {
  bool hover = false, active = false;
  cogito_node_hover_state(n, &hover, &active);
  bool focused = (cogito_focused == n);
  CogitoStyle s = cogito_theme_resolve_node_state(n, hover || focused, active);

  // Draw rounded background
  CogitoColor bg = s.has_bg ? s.bg : n->bg;
  CogitoColor text_color = s.has_text ? s.text : n->text_color;
  CogitoColor border_col = s.has_border ? s.border : n->border_color;
  if (!s.has_border && !n->border_color_set) {
    border_col.a = 0;
  }
  int border_w = s.has_border_width ? s.border_width : 0;
  int radius = s.has_radius ? s.radius : (n->h / 2);

  // Animate colors
  cogito_anim_apply(n, &s, &bg, &text_color, &border_col, NULL);

  // Override border for focus state
  if (focused) {
    CogitoColor sel = s.has_selection ? s.selection : text_color;
    border_col = sel;
    border_w = 2;
  }

  cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);
  if (border_w > 0) {
    cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, border_col, radius, border_w);
  }

  // Draw search icon on the left
  int icon_size = 16;
  int icon_x = n->x + 12;
  CogitoColor icon_color = text_color;
  if (!focused && (!n->text || !n->text->data[0])) {
    icon_color.a = 150;  // Dimmer when empty and not focused
  }

  // Text area starts after icon
  int text_x = icon_x + icon_size + 8;
  int text_avail_w = n->w - (text_x - n->x) - 12;
  int size = cogito_node_font_size(n);
  int text_h = cogito_text_height_size(size);
  int text_y = n->y + (n->h - text_h) / 2;
  const char* txt = n->text ? n->text->data : "";
  bool has_text = txt[0] != '\0';
  const char* placeholder = (n->subtitle && n->subtitle->data && n->subtitle->data[0]) ? n->subtitle->data : "Search...";
  bool has_placeholder = placeholder && placeholder[0];
  bool float_placeholder = has_placeholder && (focused || has_text);
  if (has_placeholder) {
    double now = 0.0;
    if (cogito_backend && cogito_backend->get_time) {
      now = cogito_backend->get_time();
    }
    CogitoColor ph = text_color;
    ph.a = 120;
    int ph_y_target = text_y;
    if (float_placeholder) {
      ph_y_target = text_y - text_h - 4;
      int min_ph_y = n->y + 2;
      if (ph_y_target < min_ph_y) ph_y_target = min_ph_y;
    }
    if (!n->anim_layout_active) {
      n->anim_layout_active = true;
      n->anim_y = n->anim_y_from = n->anim_y_target = ph_y_target;
      n->anim_y_start = now;
    }
    int ph_y = cogito_anim_int(n, ph_y_target, &n->anim_y, &n->anim_y_from, &n->anim_y_target, &n->anim_y_start, now);
    cogito_draw_text_ellipsis(n, text_x, ph_y, text_avail_w, placeholder, ph, size, false, 0);
  }
  int icon_y = text_y + (text_h - icon_size) / 2;
  cogito_draw_icon_fallback(n, "sf:magnifyingglass", icon_x, icon_y, icon_size, icon_color);

  // Draw selection highlight
  if (focused && n->sel_start != n->sel_end) {
    int a = n->sel_start;
    int b = n->sel_end;
    if (a > b) { int tmp = a; a = b; b = tmp; }
    int sx = cogito_text_width_size_n_node(n, txt, a, size);
    int ex = cogito_text_width_size_n_node(n, txt, b, size);
    CogitoColor sel = s.has_selection ? s.selection : n->selection_color;
    int x0 = text_x + sx;
    int x1 = text_x + ex;
    if (x1 > x0 && x1 <= text_x + text_avail_w) {
      cogito_draw_rect(x0, text_y, x1 - x0, text_h, sel, 2);
    }
  }

  // Draw text or placeholder
  if (has_text) {
    cogito_draw_text_ellipsis(n, text_x, text_y, text_avail_w, txt, text_color, size, false, 0);
  }

  // Draw caret when focused
  if (focused) {
    int caret = n->caret;
    if (caret < 0) caret = 0;
    int tw = cogito_text_width_size_n_node(n, txt, caret, size);
    int cx = text_x + tw;
    int max_cx = text_x + text_avail_w;
    if (cx > max_cx) cx = max_cx;
    // Blink: show for 500ms, hide for 500ms
    double time = 0.0;
    if (cogito_backend && cogito_backend->get_time) {
      time = cogito_backend->get_time();
    }
    if ((int)(time * 2.0) % 2 == 0) {
      cogito_draw_line(cx, text_y, cx, text_y + text_h, text_color);
    }
  }
  break;
}
