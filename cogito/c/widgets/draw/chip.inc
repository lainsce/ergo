case COGITO_CHIP: {
  bool hover = false, active = false;
  cogito_node_hover_state(n, &hover, &active);
  CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);

  // Draw pill-shaped background
  Color bg = n->chip_selected ? (s.has_selection ? s.selection : n->selection_color) : (s.has_bg ? s.bg : n->bg);
  int radius = n->h / 2;  // Fully rounded
  cogito_draw_rect(n->x, n->y, n->w, n->h, bg, radius);

  // Draw border if set
  if (s.has_border && s.has_border_width && s.border_width > 0) {
    cogito_draw_rect_lines(n->x, n->y, n->w, n->h, s.border, radius, s.border_width);
  }

  // Draw text
  if (n->text) {
    Color text_color = s.has_text ? s.text : n->text_color;
    int size = cogito_node_font_size(n);
    int text_w = cogito_text_width_size_node(n, n->text->data, size);
    int text_h = cogito_text_height_size(size);
    int text_x = n->x + 16;  // Left padding
    int text_y = n->y + (n->h - text_h) / 2;
    cogito_draw_text_size_node(n, text_x, text_y, n->text->data, text_color, size, false);

    // Draw close icon if enabled
    if (n->chip_has_close) {
      int icon_size = 12;
      int icon_x = n->x + n->w - 16 - icon_size;  // Right side
      int icon_y = n->y + (n->h - icon_size) / 2;
      // Draw X for close icon
      DrawLine(icon_x, icon_y, icon_x + icon_size, icon_y + icon_size, text_color);
      DrawLine(icon_x + icon_size, icon_y, icon_x, icon_y + icon_size, text_color);
    }
  }
  break;
}
