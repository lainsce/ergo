case COGITO_SIDE_SHEET: {
  bool hover = false, active = false;
  cogito_node_hover_state(n, &hover, &active);
  CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);

  CogitoColor bg = s.has_bg ? s.bg : n->bg;
  CogitoColor text_color = s.has_text ? s.text : n->text_color;
  int radius = s.has_radius ? s.radius : (n->radius_set ? n->border_radius : 28);

  cogito_anim_apply(n, &s, &bg, &text_color, NULL, NULL);

  // Draw modal scrim when the sheet is overlaid in ZStack/Window.
  CogitoNode* host = n->parent;
  if (host && host->kind != COGITO_DIALOG_SLOT &&
      (host->kind == COGITO_ZSTACK || host->kind == COGITO_WINDOW)) {
    CogitoStyle slot_style = cogito_theme_resolve(COGITO_DIALOG_SLOT);
    CogitoColor scrim = slot_style.has_bg ? slot_style.bg : n->bg;
    if (scrim.a == 0) {
      CogitoStyle ws = cogito_theme_resolve(COGITO_WINDOW);
      scrim = ws.has_bg ? ws.bg : n->bg;
      scrim.a = 110;
    }
    CogitoNode* win = cogito_node_window(n);
    bool window_overlay = (host->kind == COGITO_WINDOW);
    if (!window_overlay && host->kind == COGITO_ZSTACK && win && host->parent == win) {
      window_overlay = true;
    }
    int sx = window_overlay ? 0 : host->x;
    int sy = window_overlay ? 0 : host->y;
    int sw = window_overlay && win ? win->w : host->w;
    int sh = window_overlay && win ? win->h : host->h;
    int cover_w = window_overlay && win ? win->w : host->w;
    if (cover_w > 0) {
      float cover = (float)n->w / (float)cover_w;
      if (cover > 0.85f) {
        int boosted = 110 + (int)lroundf((cover - 0.85f) / 0.15f * 90.0f);
        if (boosted > 220) boosted = 220;
        if (scrim.a < boosted) scrim.a = (uint8_t)boosted;
      }
    }
    if (sw > 0 && sh > 0) {
      cogito_draw_rect(sx, sy, sw, sh, scrim, 0);
    }
  }

  // Draw docked sheet surface.
  cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);

  // Draw title
  int content_x = n->x + 28;
  if (n->text && n->text->data && n->text->data[0]) {
    int pad_l = n->padding_left;
    int pad_t = n->padding_top;
    int size = cogito_node_font_size(n);
    int tw = cogito_text_width_size_node(n, n->text->data, size);
    int gap = n->gap > 0 ? n->gap : 8;
    cogito_draw_text_size_node(n, n->x + pad_l + content_x, n->y + pad_t + 12,
                               n->text->data, text_color, size, true);
    (void)tw;
    (void)gap;
  }

  // Draw children
  for (size_t i = 0; i < n->len; i++) {
    cogito_draw_node(n->children[i]);
  }
  break;
}
