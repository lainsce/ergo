case COGITO_CONTENT_LIST: {
  CogitoStyle s = cogito_theme_resolve_node(n);
  CogitoColor bg = s.has_bg ? s.bg : n->bg;
  CogitoColor border_color = s.has_border ? s.border : n->border_color;
  int border_w = s.has_border_width ? s.border_width : (n->border_width_set ? n->border_width : 1);

  // Draw overall background with radius: 12 top, 12 bottom
  int r = s.has_radius ? s.radius : 12;
  if (s.has_bg) {
    cogito_draw_rect(n->x, n->y, n->w, n->h, bg, r);
  }
  if (border_w > 0 && (s.has_border || n->border_width_set)) {
    cogito_draw_rect_lines(n->x, n->y, n->w, n->h, border_color, r, border_w);
  }

  // Draw children with dividers between them
  for (size_t i = 0; i < n->len; i++) {
    cogito_draw_node(n->children[i]);
    // Draw divider between items (not after last)
    if (i + 1 < n->len) {
      CogitoColor div_color = border_color;
      div_color.a = (uint8_t)(div_color.a * 0.3f);
      int dy = n->children[i]->y + n->children[i]->h;
      cogito_draw_rect(n->x + 16, dy, n->w - 32, 1, div_color, 0);
    }
  }
  break;
}
