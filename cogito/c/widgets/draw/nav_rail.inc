case COGITO_NAV_RAIL: {
  CogitoStyle s = cogito_theme_resolve_node(n);

  // Draw background
  CogitoColor bg = s.has_bg ? s.bg : n->bg;
  cogito_draw_rect(n->x, n->y, n->w, n->h, bg, 0);

  CogitoNode* fab = NULL;
  for (size_t i = 0; i < n->len; i++) {
    if (n->children[i] && n->children[i]->kind == COGITO_FAB) {
      fab = n->children[i];
      break;
    }
  }

  int item_h = 56;  // Standard touch target
  int icon_size = 16;
  int item_y = n->y + n->padding_top;
  if (fab && fab->w > 0 && fab->h > 0) {
    int min_item_y = fab->y + fab->h + 12;
    if (item_y < min_item_y) item_y = min_item_y;
  }

  for (size_t i = 0; i < n->nav_rail.len; i++) {
    bool selected = (int)i == n->nav_rail.selected;
    bool hover = false;

    // Check if mouse is over this item
    int mx = 0, my = 0;
    if (cogito_backend && cogito_backend->get_mouse_position) {
      cogito_backend->get_mouse_position(&mx, &my);
    }
    if (cogito_hit_rect(mx, my, n->x, item_y, n->w, item_h)) {
      hover = true;
    }

    // Draw selection/hover background
    int indicator_w = 64;  // Width of the button background
    int indicator_h = 56;
    int bg_x = n->x + (n->w - indicator_w) / 2;
    int bg_y = item_y + (indicator_h - 32) / 2;
    if (selected || hover) {
      CogitoColor hover_tint = s.has_text ? s.text : n->text_color;
      CogitoColor item_bg = selected
        ? (s.has_selection ? s.selection : n->selection_color)
        : (s.has_bg ? cogito_mix(s.bg, hover_tint, 0.1f) : n->bg);
      int radius = 16;
      cogito_draw_rect(bg_x, bg_y, indicator_w, 32, item_bg, radius);
    }

    // Draw icon
    if (n->nav_rail.icons && n->nav_rail.icons[i]) {
      CogitoColor icon_color = s.has_text ? s.text : n->text_color;
      if (selected) {
        CogitoColor sel_bg = s.has_selection ? s.selection : n->selection_color;
        icon_color = cogito_on_color(sel_bg);
      } else if (hover) {
        icon_color = s.has_text ? s.text : n->text_color;
      }
      int icon_x = bg_x + (indicator_w - icon_size) / 2;
      int icon_y = item_y + 14;
      cogito_draw_icon_fallback(n, n->nav_rail.icons[i]->data, icon_x, icon_y, icon_size, icon_color);
    }

    // Draw label below icon
    if (n->nav_rail.labels && n->nav_rail.labels[i]) {
      CogitoColor text_color = s.has_text ? s.text : n->text_color;
      if (selected) {
        CogitoColor sel_bg = s.has_selection ? s.selection : n->selection_color;
        text_color = cogito_on_color(sel_bg);
      }
      int size = 12;  // Small label size
      int label_w = cogito_text_width_size_node(n, n->nav_rail.labels[i]->data, size);
      int label_x = bg_x + (indicator_w - label_w) / 2;
      int label_y = item_y + 28;
      cogito_draw_text_size_node(n, label_x, label_y, n->nav_rail.labels[i]->data, text_color, size, false);
    }

    item_y += item_h;
  }
  if (fab) cogito_draw_node(fab);
  break;
}
