case COGITO_BUTTON: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      if (n->parent && n->parent->kind == COGITO_APPBAR) {
        int mx = (int)GetMousePosition().x;
        int my = (int)GetMousePosition().y;
        bool over = cogito_hit_rect(mx, my, n->x, n->y, n->w, n->h);
        bool down = over && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        if (down) {
          s = cogito_style_merge(&s, &cogito_theme.appbar_child_button_active);
        } else if (over) {
          s = cogito_style_merge(&s, &cogito_theme.appbar_child_button_hover);
        }
      }
      Color bg = n->bg;
      Color border = n->border_color;
      int border_w = n->border_width;
      int radius = n->border_radius;
      if (s.has_bg) bg = s.bg;
      if (s.has_border) border = s.border;
      if (s.has_border_width) border_w = s.border_width;
      if (s.has_radius) radius = s.radius;
      if (n->bg_set || border_w > 0) {
        if (n->shadow_set && n->shadow_level > 0) {
          cogito_draw_shadow(n->x, n->y, n->w, n->h, radius, n->shadow_level);
        }
        cogito_draw_rect(n->x, n->y, n->w, n->h, bg, radius);
        if (border_w > 0) {
          cogito_draw_rect_lines(n->x, n->y, n->w, n->h, border, radius, border_w);
        }
      }
      if (n->text) {
        int size = cogito_node_font_size(n);
        int tw = cogito_text_width_size(n->text->data, size);
        int th = cogito_text_height_size(size);
        int tx = n->x + (n->w - tw) / 2;
        int ty = n->y + (n->h - th) / 2;
        Color text = n->text_color;
        if (s.has_text) text = s.text;
        cogito_draw_text_size(tx, ty, n->text->data, text, size, cogito_node_bold(n));
      }
      break;
    }
    
