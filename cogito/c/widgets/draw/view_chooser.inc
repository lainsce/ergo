case COGITO_VIEW_CHOOSER: {
  bool hover = false, active = false;
  cogito_node_hover_state(n, &hover, &active);
  CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);

  CogitoColor bg = s.has_bg ? s.bg : n->bg;
  CogitoColor text_color = s.has_text ? s.text : n->text_color;
  CogitoColor border_color = s.has_border ? s.border : n->border_color;
  int radius = s.has_radius ? s.radius : 8;
  int border_w = s.has_border_width ? s.border_width : 1;

  cogito_anim_apply(n, &s, &bg, &text_color, &border_color, NULL);

  // Draw dropdown-like background
  cogito_draw_rect(n->x, n->y, n->w, n->h, bg, radius);
  if (border_w > 0) {
    cogito_draw_rect_lines(n->x, n->y, n->w, n->h, border_color, radius, border_w);
  }

  // Draw selected item text
  int sel = n->selected >= 0 ? n->selected : 0;
  if (n->tab_labels && sel >= 0 && sel < (int)n->tab_len && n->tab_labels[sel]) {
    int ts = cogito_node_font_size(n);
    int th = cogito_text_height_size(ts);
    int tx = n->x + 12;
    int ty = n->y + (n->h - th) / 2;
    cogito_draw_text_size_node(n, tx, ty, n->tab_labels[sel]->data, text_color, ts, false);
  }

  // Draw dropdown arrow
  int arrow_x = n->x + n->w - 24;
  int arrow_y = n->y + n->h / 2 - 2;
  CogitoColor arrow_col = text_color;
  arrow_col.a = (uint8_t)(arrow_col.a * 0.5f);
  cogito_draw_rect(arrow_x, arrow_y, 5, 1, arrow_col, 0);
  cogito_draw_rect(arrow_x + 1, arrow_y + 1, 3, 1, arrow_col, 0);
  cogito_draw_rect(arrow_x + 2, arrow_y + 2, 1, 1, arrow_col, 0);
  break;
}
