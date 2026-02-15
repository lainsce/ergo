case COGITO_BOTTOM_SHEET: {
  bool hover = false, active = false;
  cogito_node_hover_state(n, &hover, &active);
  CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);

  CogitoColor bg = s.has_bg ? s.bg : n->bg;
  CogitoColor text_color = s.has_text ? s.text : n->text_color;
  int radius = s.has_radius ? s.radius : (n->radius_set ? n->border_radius : 28);

  cogito_anim_apply(n, &s, &bg, &text_color, NULL, NULL);

  // Draw shadow above sheet
  cogito_draw_shadow_node_level(n, &s, n->x, n->y, n->w, n->h, radius, 3);

  // Draw background with rounded top corners only
  cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);

  // Draw drag handle
  int handle_w = 32;
  int handle_h = 4;
  int handle_x = n->x + (n->w - handle_w) / 2;
  int handle_y = n->y + 12;
  CogitoColor handle_color = text_color;
  handle_color.a = 100;
  cogito_draw_rect(handle_x, handle_y, handle_w, handle_h, handle_color, handle_h / 2);

  // Draw title
  int content_y = n->y + 28;
  if (n->text && n->text->data && n->text->data[0]) {
    int pad_l = s.has_padding_left ? s.padding_left : 16;
    int size = cogito_node_font_size(n);
    int th = cogito_text_height_size(size);
    int gap = n->gap > 0 ? n->gap : 8;
    cogito_draw_text_size_node(n, n->x + pad_l, content_y + (s.has_padding_top ? s.padding_top : 16),
                               n->text->data, text_color, size, true);
    (void)th;
    (void)gap;
  }

  // Draw children
  for (size_t i = 0; i < n->len; i++) {
    cogito_draw_node(n->children[i]);
  }
  break;
}
