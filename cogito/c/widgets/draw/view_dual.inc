case COGITO_VIEW_DUAL: {
  bool hover = false, active = false;
  cogito_node_hover_state(n, &hover, &active);
  CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
  CogitoColor bg = s.has_bg ? s.bg : n->bg;
  CogitoColor border = s.has_border ? s.border : n->border_color;
  
  float ratio = n->view_dual.ratio;
  if (ratio <= 0.0f) ratio = 0.5f;
  
  // Draw background
  if (s.has_bg || n->bg_set) {
    int radius = s.has_radius ? s.radius : 0;
    cogito_draw_rect(n->x, n->y, n->w, n->h, bg, radius);
  }
  
  // Calculate handle position
  int handle_w = 8;
  int handle_x = n->x + (int)(n->w * ratio) - handle_w / 2;

  // Draw children (panels on either side of handle)
  for (size_t i = 0; i < n->len && i < 2; i++) {
    cogito_draw_node(n->children[i]);
  }

  // Draw resize handle - use themed border color
  CogitoColor handle_color = border;
  handle_color.a = (uint8_t)(border.a * 0.4f);
  if (handle_color.a < 30) handle_color.a = 30;
  cogito_draw_rect(handle_x, n->y, handle_w, n->h, handle_color, handle_w / 2);

  // Draw grip dots - use themed text color
  CogitoColor grip = s.has_text ? s.text : n->text_color;
  grip.a = (uint8_t)(border.a * 0.6f);
  if (grip.a < 40) grip.a = 40;
  int grip_y = n->y + n->h / 2 - 10;
  for (int i = 0; i < 4; i++) {
    cogito_draw_rect(handle_x + 2, grip_y + i * 6, 4, 3, grip, 1);
  }
  break;
}
