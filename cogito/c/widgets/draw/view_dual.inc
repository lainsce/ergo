case COGITO_VIEW_DUAL: {
  CogitoStyle s = cogito_theme_resolve_node(n);
  float ratio = n->view_dual.ratio;
  if (ratio <= 0.0f) ratio = 0.5f;
  int handle_w = 4;
  int handle_x = n->x + (int)(n->w * ratio) - handle_w / 2;

  // Draw children
  for (size_t i = 0; i < n->len && i < 2; i++) {
    cogito_draw_node(n->children[i]);
  }

  // Draw resize handle
  CogitoColor handle_color = s.has_border ? s.border : n->border_color;
  handle_color.a = (uint8_t)(handle_color.a * 0.4f);
  cogito_draw_rect(handle_x, n->y, handle_w, n->h, handle_color, 0);

  // Draw grip dots in center of handle
  CogitoColor grip = handle_color;
  grip.a = (uint8_t)(grip.a * 1.5f > 255 ? 255 : grip.a * 1.5f);
  int grip_y = n->y + n->h / 2 - 12;
  for (int i = 0; i < 3; i++) {
    cogito_draw_rect(handle_x + 1, grip_y + i * 8, 2, 2, grip, 1);
  }
  break;
}
