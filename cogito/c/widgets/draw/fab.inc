case COGITO_FAB: {
  bool hover = false, active = false;
  cogito_node_hover_state(n, &hover, &active);
  CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);

  Color bg = s.has_bg ? s.bg : n->bg;
  int radius = n->fab_extended ? 16 : (n->h / 2);

  // Animate colors
  // cogito_anim_apply(n, &s, &bg, NULL, NULL, NULL);

  // Draw box shadow (elevation)
  if (s.has_box_shadow && s.box_shadow.blur > 0) {
    int shadow_level = (int)(s.box_shadow.blur / 2);
    if (shadow_level < 1) shadow_level = 1;
    if (shadow_level > 5) shadow_level = 5;
    cogito_draw_shadow(n->x, n->y, n->w, n->h, radius, shadow_level);
  }

  // Draw elevated circular/rounded background
  cogito_draw_rect(n->x, n->y, n->w, n->h, bg, radius);

  // State layer for hover/press
  if (hover || active) {
    Color state_layer = s.has_text ? s.text : n->text_color;
    state_layer.a = active ? 25 : 15;
    cogito_draw_rect(n->x, n->y, n->w, n->h, state_layer, radius);
  }

  // Draw icon
  if (n->text && n->text->data[0]) {
    int icon_size = 24;
    int icon_x = n->fab_extended ? (n->x + 16) : (n->x + (n->w - icon_size) / 2);
    int icon_y = n->y + (n->h - icon_size) / 2;

    Texture2D tex = {0};
    int tw = 0, th_i = 0;
    Color icon_color = s.has_text ? s.text : n->text_color;

    if (cogito_icon_load_texture(n->text->data, icon_size, &tex, &tw, &th_i)) {
      float dw = (float)icon_size;
      float dh = (float)icon_size;
      if (tw > 0 && th_i > 0) {
        float sx = (float)icon_size / (float)tw;
        float sy = (float)icon_size / (float)th_i;
        float scale = sx < sy ? sx : sy;
        dw = (float)tw * scale;
        dh = (float)th_i * scale;
      }
      float dx = icon_x + ((float)icon_size - dw) * 0.5f;
      float dy = icon_y + ((float)icon_size - dh) * 0.5f;
      DrawTexturePro(tex, (Rectangle){0, 0, (float)tw, (float)th_i},
                     (Rectangle){dx, dy, dw, dh},
                     (Vector2){0, 0}, 0.0f, icon_color);
    } else {
      int size = cogito_node_font_size(n);
      cogito_draw_text_size_node(n, icon_x, icon_y, n->text->data, icon_color, size, false);
    }
  }

  // Draw label if extended
  if (n->fab_extended && n->subtitle && n->subtitle->data[0]) {
    Color text_color = s.has_text ? s.text : n->text_color;
    int size = cogito_node_font_size(n);
    int text_h = cogito_text_height_size(size);
    int label_x = n->x + 16 + 24 + 12;
    int label_y = n->y + (n->h - text_h) / 2;
    cogito_draw_text_size_node(n, label_x, label_y, n->subtitle->data, text_color, size, false);
  }
  break;
}

