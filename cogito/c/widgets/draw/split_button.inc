case COGITO_SPLIT_BUTTON: {
  bool hover = false, active = false;
  cogito_node_hover_state(n, &hover, &active);
  CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);

  CogitoColor bg = s.has_bg ? s.bg : n->bg;
  CogitoColor text_color = s.has_text ? s.text : n->text_color;
  CogitoColor border_color = s.has_border ? s.border : n->border_color;
  int border_w = s.has_border_width ? s.border_width : 0;

  cogito_anim_apply(n, &s, &bg, &text_color, &border_color, NULL);

  // Split button dimensions
  int menu_w = n->h;
  if (menu_w < 56) menu_w = 56;
  bool menu_active = cogito_menu.open && cogito_menu.owner == n;
  int gap = 2;
  if (menu_w > n->w - 1) menu_w = n->w - 1;
  if (menu_w < 1) menu_w = 1;
  if (gap > n->w - menu_w - 1) gap = n->w - menu_w - 1;
  if (gap < 0) gap = 0;
  int main_w = n->w - menu_w - gap;
  if (main_w < 1) main_w = 1;
  int menu_x = n->x + main_w + gap;
  n->split_btn.arrow_x = menu_x;

  int outer_radius = n->h / 2;
  if (outer_radius < 0) outer_radius = 0;
  int main_inner_radius = 4;
  if (main_inner_radius > outer_radius) main_inner_radius = outer_radius;
  int menu_target_inner_radius = menu_active ? outer_radius : 4;
  if (menu_target_inner_radius > outer_radius) menu_target_inner_radius = outer_radius;
  double now = 0.0;
  if (cogito_backend && cogito_backend->get_time) now = cogito_backend->get_time();
  if (!n->anim_layout_active) {
    n->anim_layout_active = true;
    n->anim_h = n->anim_h_from = n->anim_h_target = menu_target_inner_radius;
    n->anim_h_start = now;
  }
  int prev_anim_ms = n->anim_ms;
  int prev_anim_ease = n->anim_ease;
  if (n->anim_ms <= 0) n->anim_ms = 180;
  if (n->anim_ease <= 0) n->anim_ease = 4; // ease-in-out fallback
  int menu_inner_radius = cogito_anim_int(n, menu_target_inner_radius,
                                          &n->anim_h, &n->anim_h_from, &n->anim_h_target,
                                          &n->anim_h_start, now);
  n->anim_ms = prev_anim_ms;
  n->anim_ease = prev_anim_ease;

  // Draw main button with circular outer corners and 4px inner corners
  cogito_draw_rect_radii_aa(n->x, n->y, main_w, n->h, bg,
                            outer_radius, main_inner_radius, main_inner_radius, outer_radius);
  if (border_w > 0) {
    cogito_draw_rect_lines_radii_aa(n->x, n->y, main_w, n->h, border_color,
                                    outer_radius, main_inner_radius, main_inner_radius, outer_radius, border_w);
  }

  // Draw menu button with reversed corner radii; when menu is active make it fully circular.
  cogito_draw_rect_radii_aa(menu_x, n->y, menu_w, n->h, bg,
                            menu_inner_radius, outer_radius, outer_radius, menu_inner_radius);
  if (border_w > 0) {
    cogito_draw_rect_lines_radii_aa(menu_x, n->y, menu_w, n->h, border_color,
                                    menu_inner_radius, outer_radius, outer_radius, menu_inner_radius, border_w);
  }

  // Draw main button text
  if (n->text && n->text->data && n->text->data[0]) {
    int ts = cogito_node_font_size(n);
    int tw = cogito_text_width_size_node(n, n->text->data, ts);
    int th = cogito_text_height_size(ts);
    int tx = n->x + (main_w - tw) / 2;
    int ty = n->y + (n->h - th) / 2;
    cogito_draw_text_size_node(n, tx, ty, n->text->data, text_color, ts, true);
  }

  // Draw dropdown chevron via platform icon theme (SF symbol on macOS)
  int icon_size = s.has_icon_size ? s.icon_size : 22;
  if (icon_size < 8) icon_size = 8;
  int max_icon = n->h - 8;
  if (max_icon < 8) max_icon = 8;
  if (icon_size > max_icon) icon_size = max_icon;
  int ix = menu_x + (menu_w - icon_size) / 2;
  int iy = n->y + (n->h - icon_size) / 2;
  CogitoColor arrow_col = s.has_icon_color ? s.icon_color : text_color;
  cogito_draw_icon_fallback(n, menu_active ? "chevron.up" : "chevron.down", ix, iy, icon_size, arrow_col);
  break;
}
