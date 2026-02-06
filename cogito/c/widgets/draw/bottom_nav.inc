case COGITO_BOTTOM_NAV: {
  CogitoStyle s = cogito_theme_resolve_node(n);

  // Draw background
  Color bg = s.has_bg ? s.bg : n->bg;
  cogito_draw_rect(n->x, n->y, n->w, n->h, bg, 0);

  if (n->bottom_nav_len == 0) break;

  int item_w = n->w / (int)n->bottom_nav_len;
  int icon_size = 24;
  int label_size = 12;

  for (size_t i = 0; i < n->bottom_nav_len; i++) {
    bool selected = (int)i == n->bottom_nav_selected;
    bool hover = false;

    int item_x = n->x + (int)i * item_w;

    // Check if mouse is over this item
    int mx = (int)GetMousePosition().x;
    int my = (int)GetMousePosition().y;
    if (cogito_hit_rect(mx, my, item_x, n->y, item_w, n->h)) {
      hover = true;
    }

    // Draw selection/hover background
    if (selected || hover) {
      Color item_bg = selected
        ? (s.has_selection ? s.selection : n->selection_color)
        : (s.has_bg ? cogito_mix(s.bg, (Color){128, 128, 128, 255}, 0.1f) : n->bg);
      // Bottom nav uses indicator pill above selected item
      if (selected) {
        int indicator_w = 64;
        int indicator_h = 32;
        int indicator_x = item_x + (item_w - indicator_w) / 2;
        int indicator_y = n->y + 8;
        cogito_draw_rect(indicator_x, indicator_y, indicator_w, indicator_h, item_bg, 16);
      }
    }

    // Draw icon
    if (n->bottom_nav_icons && n->bottom_nav_icons[i]) {
      Color icon_color = s.has_text ? s.text : n->text_color;
      if (selected) {
        icon_color = s.has_selection ? s.text : (Color){103, 80, 164, 255};
      }
      Texture2D tex;
      int tw = 0, th = 0;
      if (cogito_icon_load_texture(n->bottom_nav_icons[i]->data, icon_size, &tex, &tw, &th)) {
        int icon_x = item_x + (item_w - icon_size) / 2;
        int icon_y = n->y + 12;
        DrawTexturePro(tex,
                       (Rectangle){0, 0, tex.width, tex.height},
                       (Rectangle){icon_x, icon_y, icon_size, icon_size},
                       (Vector2){0, 0}, 0, icon_color);
      }
    }

    // Draw label below icon
    if (n->bottom_nav_labels && n->bottom_nav_labels[i]) {
      Color text_color = s.has_text ? s.text : n->text_color;
      int label_w = cogito_text_width_size_node(n, n->bottom_nav_labels[i]->data, label_size);
      int label_x = item_x + (item_w - label_w) / 2;
      int label_y = n->y + 12 + icon_size + 4;
      cogito_draw_text_size_node(n, label_x, label_y, n->bottom_nav_labels[i]->data, text_color, label_size, false);
    }
  }
  break;
}
