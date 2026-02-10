case COGITO_TREEVIEW: {
  bool hover = false, active = false;
  cogito_node_hover_state(n, &hover, &active);
  CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);

  // Draw background
  CogitoColor bg = s.has_bg ? s.bg : n->bg;
  int radius = s.has_radius ? s.radius : 8;
  cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);

  // Draw border if set
  if (s.has_border && s.has_border_width && s.border_width > 0) {
    cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, s.border, radius, s.border_width);
  }

  // Draw tree rows.
  for (size_t i = 0; i < n->len; i++) {
    cogito_draw_node(n->children[i]);
  }
  break;
}
