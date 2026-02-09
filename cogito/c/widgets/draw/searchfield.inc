case COGITO_SEARCHFIELD: {
  bool hover = false, active = false;
  cogito_node_hover_state(n, &hover, &active);
  bool focused = (cogito_focused == n);
  CogitoStyle s = cogito_theme_resolve_node_state(n, hover || focused, active);

  // Draw rounded background
  CogitoColor bg = s.has_bg ? s.bg : n->bg;
  CogitoColor text_color = s.has_text ? s.text : n->text_color;
  CogitoColor border_col = s.has_border ? s.border : cogito_rgba(0, 0, 0, 0);
  int border_w = s.has_border_width ? s.border_width : 0;
  int radius = s.has_radius ? s.radius : (n->h / 2);

  // Animate colors
  //cogito_anim_apply(n, &s, &bg, &text_color, &border_col, NULL);

  // Override border for focus state
  if (focused) {
    CogitoColor sel = s.has_selection ? s.selection : text_color;
    border_col = sel;
    border_w = 2;
  }

  cogito_draw_rect(n->x, n->y, n->w, n->h, bg, radius);
  if (border_w > 0) {
    cogito_draw_rect_lines(n->x, n->y, n->w, n->h, border_col, radius, border_w);
  }

  // Draw search icon on the left
  int icon_size = 16;
  int icon_x = n->x + 12;
  int icon_y = n->y + (n->h - icon_size) / 2;
  CogitoColor icon_color = text_color;
  if (!focused && (!n->text || !n->text->data[0])) {
    icon_color.a = 150;  // Dimmer when empty and not focused
  }

  // Try to load icon texture, fall back to drawn icon
  {
    // TODO: Replace with backend texture drawing
    // Texture2D tex = {0};
    // int tw = 0, th_i = 0;
    // if (cogito_icon_load_texture("sf:magnifyingglass", icon_size, &tex, &tw, &th_i)) {
    //   cogito_backend->draw_texture(...);
    // } else {
    // Fallback: draw magnifying glass
    // DrawCircleLines(icon_x + icon_size/2 - 1, icon_y + icon_size/2 - 1, icon_size/2 - 3, icon_color);
    // DrawLineEx((Vector2){(float)(icon_x + icon_size - 4), (float)(icon_y + icon_size - 4)},
    //            (Vector2){(float)(icon_x + icon_size), (float)(icon_y + icon_size)}, 2.0f, icon_color);
    // Draw circle outline
    int cx = icon_x + icon_size/2 - 1;
    int cy = icon_y + icon_size/2 - 1;
    int r = icon_size/2 - 3;
    cogito_draw_rect(cx - r, cy - r, r * 2, r * 2, icon_color, r);
    // Draw handle line
    cogito_draw_line(icon_x + icon_size - 4, icon_y + icon_size - 4, icon_x + icon_size, icon_y + icon_size, icon_color);
    // }
  }

  // Text area starts after icon
  int text_x = icon_x + icon_size + 8;
  int text_avail_w = n->w - (text_x - n->x) - 12;
  int size = cogito_node_font_size(n);
  int text_h = cogito_text_height_size(size);
  int text_y = n->y + (n->h - text_h) / 2;

  const char* txt = n->text ? n->text->data : "";
  bool has_text = txt[0] != '\0';

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
  } else if (!focused) {
    // Draw placeholder
    CogitoColor placeholder = text_color;
    placeholder.a = 120;
    cogito_draw_text_size_node(n, text_x, text_y, "Search...", placeholder, size, false);
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
