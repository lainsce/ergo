case COGITO_SEARCHFIELD: {
  bool hover = false, active = false;
  cogito_node_hover_state(n, &hover, &active);
  CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);

  // Draw rounded background
  Color bg = s.has_bg ? s.bg : n->bg;
  int radius = s.has_radius ? s.radius : (n->h / 2);
  cogito_draw_rect(n->x, n->y, n->w, n->h, bg, radius);

  // Draw border if set
  if (s.has_border && s.has_border_width && s.border_width > 0) {
    cogito_draw_rect_lines(n->x, n->y, n->w, n->h, s.border, radius, s.border_width);
  }

  // Draw search icon on the left
  int icon_size = 16;
  int icon_x = n->x + 12;
  int icon_y = n->y + (n->h - icon_size) / 2;
  Color icon_color = cogito_has_selection_text ? cogito_selection_text : (s.has_text ? s.text : n->text_color);
  
  // Draw magnifying glass icon (circle + line)
  DrawCircleLines(icon_x + icon_size/2, icon_y + icon_size/2, icon_size/2 - 2, icon_color);
  DrawLine(icon_x + icon_size - 3, icon_y + icon_size - 3, icon_x + icon_size, icon_y + icon_size, icon_color);

  // Draw text
  if (n->text && n->text->data[0]) {
    int size = cogito_node_font_size(n);
    int text_h = cogito_text_height_size(size);
    int text_x = icon_x + icon_size + 8;  // After icon + gap
    int text_y = n->y + (n->h - text_h) / 2;
    cogito_draw_text_size_node(n, text_x, text_y, n->text->data, icon_color, size, false);
  }
  break;
}
