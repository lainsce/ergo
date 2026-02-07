case COGITO_CHIP: {
  bool hover = false, active = false;
  cogito_node_hover_state(n, &hover, &active);
  CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);

  Color bg = s.has_bg ? s.bg : n->bg;
  Color text_color = s.has_text ? s.text : n->text_color;
  int radius = n->h / 2;

  // Animate colors
  cogito_anim_apply(n, &s, &bg, &text_color, NULL, NULL);

  // Draw pill-shaped background
  cogito_draw_rect(n->x, n->y, n->w, n->h, bg, radius);

  // Draw border (outline for unselected chips)
  if (s.has_border && s.has_border_width && s.border_width > 0) {
    cogito_draw_rect_lines(n->x, n->y, n->w, n->h, s.border, radius, s.border_width);
  }

  // Draw checkmark for selected chips
  if (n->chip_selected) {
    int check_size = 12;
    int check_x = n->x + 8;
    int check_cy = n->y + n->h / 2;
    DrawLineEx((Vector2){(float)check_x, (float)check_cy},
               (Vector2){(float)(check_x + check_size * 3 / 8), (float)(check_cy + check_size * 3 / 8)}, 1.5f, text_color);
    DrawLineEx((Vector2){(float)(check_x + check_size * 3 / 8), (float)(check_cy + check_size * 3 / 8)},
               (Vector2){(float)(check_x + check_size), (float)(check_cy - check_size * 3 / 8)}, 1.5f, text_color);
  }

  // Draw text
  if (n->text) {
    int size = cogito_node_font_size(n);
    int text_h = cogito_text_height_size(size);
    int text_x = n->x + (n->chip_selected ? 24 : 16);
    int text_y = n->y + (n->h - text_h) / 2;
    cogito_draw_text_size_node(n, text_x, text_y, n->text->data, text_color, size, false);

    // Draw close icon if enabled
    if (n->chip_has_close) {
      int icon_size = 12;
      int icon_x = n->x + n->w - 16 - icon_size;
      int icon_y = n->y + (n->h - icon_size) / 2;
      Color close_col = text_color;
      close_col.a = 180;
      DrawLineEx((Vector2){(float)icon_x, (float)icon_y},
                 (Vector2){(float)(icon_x + icon_size), (float)(icon_y + icon_size)}, 1.5f, close_col);
      DrawLineEx((Vector2){(float)(icon_x + icon_size), (float)icon_y},
                 (Vector2){(float)icon_x, (float)(icon_y + icon_size)}, 1.5f, close_col);
    }
  }
  break;
}

