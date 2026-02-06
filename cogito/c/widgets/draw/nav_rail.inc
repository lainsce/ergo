case COGITO_NAV_RAIL: {
  CogitoStyle s = cogito_theme_resolve_node(n);

  // Draw background
  Color bg = s.has_bg ? s.bg : n->bg;
  cogito_draw_rect(n->x, n->y, n->w, n->h, bg, 0);

  if (n->nav_rail_len == 0) break;

  int item_h = 56;  // Standard touch target
  int icon_size = 24;
  int item_y = n->y + n->padding_top;

  for (size_t i = 0; i < n->nav_rail_len; i++) {
    bool selected = (int)i == n->nav_rail_selected;
    bool hover = false;

    // Check if mouse is over this item
    int mx = (int)GetMousePosition().x;
    int my = (int)GetMousePosition().y;
    if (cogito_hit_rect(mx, my, n->x, item_y, n->w, item_h)) {
      hover = true;
    }

    // Draw selection/hover background
    if (selected || hover) {
      Color item_bg = selected
        ? (s.has_selection ? s.selection : n->selection_color)
        : (s.has_bg ? cogito_mix(s.bg, (Color){128, 128, 128, 255}, 0.1f) : n->bg);
      int indicator_w = selected ? 16 : 0;
      int radius = 16;  // Fully rounded indicator
      int bg_x = n->x + (n->w - indicator_w) / 2 - 32;
      int bg_y = item_y + (item_h - 32) / 2;
      cogito_draw_rect(bg_x, bg_y, 64, 32, item_bg, radius);
    }

    // Draw icon
    if (n->nav_rail_icons && n->nav_rail_icons[i]) {
      Color icon_color = s.has_text ? s.text : n->text_color;
      if (selected) {
        icon_color = s.has_selection ? s.text : (Color){103, 80, 164, 255};
      }
      Texture2D tex;
      int tw = 0, th = 0;
      if (cogito_icon_load_texture(n->nav_rail_icons[i]->data, icon_size, &tex, &tw, &th)) {
        int icon_x = n->x + (n->w - icon_size) / 2;
        int icon_y = item_y + 8;
        DrawTexturePro(tex,
                       (Rectangle){0, 0, tex.width, tex.height},
                       (Rectangle){icon_x, icon_y, icon_size, icon_size},
                       (Vector2){0, 0}, 0, icon_color);
      }
    }

    // Draw label below icon
    if (n->nav_rail_labels && n->nav_rail_labels[i]) {
      Color text_color = s.has_text ? s.text : n->text_color;
      int size = 11;  // Small label size
      int label_w = cogito_text_width_size_node(n, n->nav_rail_labels[i]->data, size);
      int label_x = n->x + (n->w - label_w) / 2;
      int label_y = item_y + 32 + 4;
      cogito_draw_text_size_node(n, label_x, label_y, n->nav_rail_labels[i]->data, text_color, size, false);
    }

    item_y += item_h;
  }
  break;
}
