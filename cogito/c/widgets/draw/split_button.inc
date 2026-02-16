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

  // Split button dimensions
  int arrow_w = 40;
  int main_w = n->w - arrow_w;
  n->split_btn.arrow_x = n->x + main_w;

  // Draw shadow behind button - use border color derived
  CogitoColor shadow_col = border_color;
  shadow_col.a = (uint8_t)(border_color.a * 0.2f);
  cogito_draw_rect(n->x, n->y + 2, n->w, n->h, shadow_col, radius);

  // Draw main button area with left-side rounded corners only
  int main_radius = radius;
  cogito_draw_rect(n->x, n->y, main_w + 1, n->h, bg, main_radius);
  if (border_w > 0) {
    cogito_draw_rect_lines(n->x, n->y, main_w + 1, n->h, border_color, main_radius, border_w);
  }

  // Draw arrow button area (right side) - use hover state for visual feedback
  CogitoColor arrow_bg = bg;
  if (hover || active) {
    // Slightly darken arrow area when hovering
    arrow_bg.r = (uint8_t)((arrow_bg.r > 15) ? arrow_bg.r - 15 : 0);
    arrow_bg.g = (uint8_t)((arrow_bg.g > 15) ? arrow_bg.g - 15 : 0);
    arrow_bg.b = (uint8_t)((arrow_bg.b > 15) ? arrow_bg.b - 15 : 0);
  }
  
  // Draw arrow button background
  cogito_draw_rect(n->x + main_w, n->y, arrow_w, n->h, arrow_bg, radius);
  
  // Draw divider between main and arrow
  CogitoColor div = text_color;
  div.a = (uint8_t)(text_color.a * 0.12f);
  cogito_draw_rect(n->x + main_w, n->y + 8, 1, n->h - 16, div, 0);

  // Draw main button text
  if (n->text && n->text->data && n->text->data[0]) {
    int ts = cogito_node_font_size(n);
    int tw = cogito_text_width_size_node(n, n->text->data, ts);
    int th = cogito_text_height_size(ts);
    int tx = n->x + (main_w - tw) / 2;
    int ty = n->y + (n->h - th) / 2;
    cogito_draw_text_size_node(n, tx, ty, n->text->data, text_color, ts, true);
  }

  // Draw dropdown arrow (chevron style)
  int ax = n->x + main_w + arrow_w / 2 - 5;
  int ay = n->y + n->h / 2 - 4;
  CogitoColor arrow_col = text_color;
  arrow_col.a = (uint8_t)(text_color.a * 0.7f);
  // Draw chevron (three lines forming downward arrow)
  cogito_draw_rect(ax, ay, 10, 1, arrow_col, 0);
  cogito_draw_rect(ax + 1, ay + 1, 8, 1, arrow_col, 0);
  cogito_draw_rect(ax + 2, ay + 2, 6, 1, arrow_col, 0);
  cogito_draw_rect(ax + 3, ay + 3, 4, 1, arrow_col, 0);
  cogito_draw_rect(ax + 4, ay + 4, 2, 1, arrow_col, 0);
  break;
}
