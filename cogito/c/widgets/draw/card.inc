case COGITO_CARD: {
  bool hover = false, active = false;
  cogito_node_hover_state(n, &hover, &active);
  CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);

  CogitoColor bg           = s.has_bg          ? s.bg           : n->bg;
  CogitoColor text_color   = s.has_text         ? s.text         : n->text_color;
  CogitoColor border_color = s.has_border       ? s.border       : n->border_color;
  int border_w = s.has_border_width ? s.border_width : (n->border_width_set ? n->border_width : 0);
  // Cogito Expressive: Card — L=16dp default (elevated). Overrideable via theme or node.
  int radius = s.has_radius ? s.radius : (n->radius_set ? n->border_radius : 16);
  int shadow = s.has_shadow ? s.shadow_level : n->shadow_level;

  cogito_anim_apply(n, &s, &bg, &text_color, &border_color, NULL);

  // Shadow
  if (shadow > 0) {
    cogito_draw_shadow_node_level(n, &s, n->x, n->y, n->w, n->h, radius, shadow);
  }
  // Background
  cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);

  // State layer for interactive cards (on_action set): hover 8%, press 12%
  if (n->on_action) {
    CogitoColor state = text_color;
    if (active)     state.a = 30;
    else if (hover) state.a = 20;
    else            state.a = 0;
    if (state.a > 0) {
      cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, state, radius);
    }
  }
  // Border
  if (border_w > 0) {
    cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, border_color, radius, border_w);
  }
  // Title
  if (n->text && n->text->data && n->text->data[
    0
  ]) {
    CogitoStyle ts = cogito_theme_resolve_node(n);
    int pad_l = ts.has_padding_left ? ts.padding_left : 16;
    int pad_t = ts.has_padding_top  ? ts.padding_top  : 16;
    int sz    = cogito_node_font_size(n);
    int tx    = n->x + pad_l;
    int ty    = n->y + pad_t;
    cogito_draw_text_size_node(n, tx, ty, n->text->data, text_color, sz, true);
  }
  // Children
  for (size_t i = 0; i < n->len; i++) {
    cogito_draw_node(n->children[i
    ]);
  }
  break;
}