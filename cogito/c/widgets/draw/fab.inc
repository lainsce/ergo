case COGITO_FAB: {
  bool hover = false, active = false;
  cogito_node_hover_state(n, &hover, &active);
  CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);

  CogitoColor bg = s.has_bg ? s.bg : n->bg;
  CogitoColor text_color = s.has_text ? s.text : n->text_color;
  int radius = n->fab.extended ? 16 : (n->h / 2);

  // Animate colors
  cogito_anim_apply(n, &s, &bg, &text_color, NULL, NULL);

  // Draw box shadow (elevation)
  if (s.has_box_shadow && s.box_shadow.blur > 0) {
    cogito_draw_shadow_node_box(n, &s, n->x, n->y, n->w, n->h, radius, &s.box_shadow);
  } else if (n->shadow_set && n->shadow_level > 0) {
    cogito_draw_shadow_node_level(n, &s, n->x, n->y, n->w, n->h, radius, n->shadow_level);
  }

  // Draw elevated circular/rounded background
  cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);

  // State layer for hover/press
  if (hover || active) {
    CogitoColor state_layer = text_color;
    state_layer.a = active ? 25 : 15;
    cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, state_layer, radius);
  }

  // Draw icon
  if (n->text && n->text->data[0]) {
    int icon_size = 24;
    int icon_x = n->fab.extended ? (n->x + 16) : (n->x + (n->w - icon_size) / 2);
    int icon_y = n->y + (n->h - icon_size) / 2;

    cogito_draw_icon_fallback(n, n->text->data, icon_x, icon_y, icon_size, text_color);
  }

  // Draw label if extended
  if (n->fab.extended && n->subtitle && n->subtitle->data[0]) {
    int size = cogito_node_font_size(n);
    int text_h = cogito_text_height_size(size);
    int label_x = n->x + 16 + 24 + 12;
    int label_y = n->y + (n->h - text_h) / 2;
    cogito_draw_text_size_node(n, label_x, label_y, n->subtitle->data, text_color, size, false);
  }
  break;
}
