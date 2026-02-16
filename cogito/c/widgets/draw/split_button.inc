case COGITO_SPLIT_BUTTON: {
  bool hover = false, active = false;
  cogito_node_hover_state(n, &hover, &active);
  CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);

  CogitoColor bg = s.has_bg ? s.bg : n->bg;
  CogitoColor text_color = s.has_text ? s.text : n->text_color;
  CogitoColor border_color = s.has_border ? s.border : n->border_color;
  int radius = s.has_radius ? s.radius : 8;
  int border_w = s.has_border_width ? s.border_width : 0;

  cogito_anim_apply(n, &s, &bg, &text_color, &border_color, NULL);

  int arrow_w = 32;
  int main_w = n->w - arrow_w;
  n->split_btn.arrow_x = n->x + main_w;

  // Draw main button area
  cogito_draw_rect(n->x, n->y, n->w, n->h, bg, radius);
  if (border_w > 0) {
    cogito_draw_rect_lines(n->x, n->y, n->w, n->h, border_color, radius, border_w);
  }

  // Draw divider between main and arrow
  CogitoColor div = text_color;
  div.a = (uint8_t)(div.a * 0.3f);
  cogito_draw_rect(n->x + main_w, n->y + 4, 1, n->h - 8, div, 0);

  // Draw main button text
  if (n->text && n->text->data && n->text->data[0]) {
    int ts = cogito_node_font_size(n);
    int tw = cogito_text_width_size_node(n, n->text->data, ts);
    int th = cogito_text_height_size(ts);
    int tx = n->x + (main_w - tw) / 2;
    int ty = n->y + (n->h - th) / 2;
    cogito_draw_text_size_node(n, tx, ty, n->text->data, text_color, ts, true);
  }

  // Draw dropdown arrow
  int ax = n->x + main_w + arrow_w / 2 - 3;
  int ay = n->y + n->h / 2 - 1;
  CogitoColor arrow_col = text_color;
  arrow_col.a = (uint8_t)(arrow_col.a * 0.6f);
  cogito_draw_rect(ax, ay, 6, 1, arrow_col, 0);
  cogito_draw_rect(ax + 1, ay + 1, 4, 1, arrow_col, 0);
  cogito_draw_rect(ax + 2, ay + 2, 2, 1, arrow_col, 0);
  break;
}
