case COGITO_ABOUT_WINDOW: {
  CogitoStyle s = cogito_theme_resolve_node(n);
  CogitoColor bg = s.has_bg ? s.bg : n->bg;
  CogitoColor text_color = s.has_text ? s.text : n->text_color;
  int radius = s.has_radius ? s.radius : 16;

  int shadow = s.has_shadow ? s.shadow_level : 3;
  if (shadow > 0) {
    cogito_draw_shadow_node_level(n, &s, n->x, n->y, n->w, n->h, radius, shadow);
  }
  cogito_draw_rect(n->x, n->y, n->w, n->h, bg, radius);

  int cx = n->x + n->w / 2;
  int cy = n->y + 32;

  // Draw app icon
  if (n->icon && n->icon->data && n->icon->data[0]) {
    int icon_size = 64;
    cogito_draw_icon_fallback(n, n->icon->data, cx - icon_size / 2, cy, icon_size, text_color);
    cy += icon_size + 12;
  } else if (cogito_active_app && cogito_active_app->app_icon &&
             cogito_active_app->app_icon->data && cogito_active_app->app_icon->data[0]) {
    int icon_size = 64;
    cogito_draw_icon_fallback(n, cogito_active_app->app_icon->data, cx - icon_size / 2, cy, icon_size, text_color);
    cy += icon_size + 12;
  }

  // Draw app name (title)
  if (n->text && n->text->data && n->text->data[0]) {
    int ts = 20;
    int tw = cogito_text_width_size_node(n, n->text->data, ts);
    cogito_draw_text_size_node(n, cx - tw / 2, cy, n->text->data, text_color, ts, true);
    cy += cogito_text_height_size(ts) + 4;
  }

  // Draw version (subtitle)
  if (n->subtitle && n->subtitle->data && n->subtitle->data[0]) {
    int vs = cogito_node_font_size(n);
    CogitoColor ver_col = text_color;
    ver_col.a = (uint8_t)(ver_col.a * 0.6f);
    int vw = cogito_text_width_size_node(n, n->subtitle->data, vs);
    cogito_draw_text_size_node(n, cx - vw / 2, cy, n->subtitle->data, ver_col, vs, false);
    cy += cogito_text_height_size(vs) + 16;
  }

  // Draw website link
  if (n->action_text && n->action_text->data && n->action_text->data[0]) {
    int as = cogito_node_font_size(n);
    CogitoColor link_col = s.has_selection ? s.selection : text_color;
    int aw = cogito_text_width_size_node(n, n->action_text->data, as);
    int ax = cx - aw / 2;
    int ah = cogito_text_height_size(as) + 8;

    n->action_button_x = ax - 4;
    n->action_button_y = cy - 4;
    n->action_button_w = aw + 8;
    n->action_button_h = ah;

    cogito_draw_text_size_node(n, ax, cy, n->action_text->data, link_col, as, false);
  }

  // Draw children
  for (size_t i = 0; i < n->len; i++) {
    cogito_draw_node(n->children[i]);
  }
  break;
}
