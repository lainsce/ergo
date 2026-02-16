case COGITO_CHIP: {
  bool hover = false, active = false;
  cogito_node_hover_state(n, &hover, &active);
  CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);

  CogitoColor bg = s.has_bg ? s.bg : n->bg;
  CogitoColor text_color = s.has_text ? s.text : n->text_color;
  CogitoColor border_color = s.has_border ? s.border : n->border_color;
  int border_w = s.has_border_width ? s.border_width : 0;
  int radius = n->h / 2;

  // Animate colors
  cogito_anim_apply(n, &s, &bg, &text_color, &border_color, NULL);

  // Draw pill-shaped background
  cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);

  // Draw border (outline for unselected chips)
  if (border_w > 0) {
    cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, border_color, radius, border_w);
  }

  // Draw checkmark for selected chips
  if (n->chip.selected) {
    int check_size = 12;
    int check_x = n->x + 8;
    int check_cy = n->y + n->h / 2;
    // TODO: Replace with backend line drawing
    // DrawLineEx((Vector2){(float)check_x, (float)check_cy},
    //            (Vector2){(float)(check_x + check_size * 3 / 8), (float)(check_cy + check_size * 3 / 8)}, 1.5f, text_color);
    // DrawLineEx((Vector2){(float)(check_x + check_size * 3 / 8), (float)(check_cy + check_size * 3 / 8)},
    //            (Vector2){(float)(check_x + check_size), (float)(check_cy - check_size * 3 / 8)}, 1.5f, text_color);
    int mid_x = check_x + check_size * 3 / 8;
    int mid_y = check_cy + check_size * 3 / 8;
    int end_x = check_x + check_size;
    int end_y = check_cy - check_size * 3 / 8;
    cogito_draw_line(check_x, check_cy, mid_x, mid_y, text_color);
    cogito_draw_line(mid_x, mid_y, end_x, end_y, text_color);
  }

  // Draw text
  if (n->text) {
    int size = cogito_node_font_size(n);
    int text_h = cogito_text_height_size(size);
    int text_x = n->x + (n->chip.selected ? 24 : 16);
    int text_y = n->y + (n->h - text_h) / 2;
    cogito_draw_text_size_node(n, text_x, text_y, n->text->data, text_color, size, false);

    // Draw close icon if enabled
    if (n->chip.has_close) {
      int icon_size = 12;
      int icon_x = n->x + n->w - 16 - icon_size;
      int icon_y = n->y + (n->h - icon_size) / 2;
      CogitoColor close_col = text_color;
      close_col.a = 180;
      // TODO: Replace with backend line drawing
      // DrawLineEx((Vector2){(float)icon_x, (float)icon_y},
      //            (Vector2){(float)(icon_x + icon_size), (float)(icon_y + icon_size)}, 1.5f, close_col);
      // DrawLineEx((Vector2){(float)(icon_x + icon_size), (float)icon_y},
      //            (Vector2){(float)icon_x, (float)(icon_y + icon_size)}, 1.5f, close_col);
      cogito_draw_line(icon_x, icon_y, icon_x + icon_size, icon_y + icon_size, close_col);
      cogito_draw_line(icon_x + icon_size, icon_y, icon_x, icon_y + icon_size, close_col);
    }
  }
  break;
}
