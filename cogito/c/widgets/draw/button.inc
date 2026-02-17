case COGITO_BUTTON: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      if (n->parent && n->parent->kind == COGITO_APPBAR) {
        // Use cached pointer position instead of querying backend
        int mx = cogito_main_pointer_x;
        int my = cogito_main_pointer_y;
        bool over = cogito_hit_rect(mx, my, n->x, n->y, n->w, n->h);
        bool down = over && (cogito_backend && cogito_backend->is_mouse_button_down(0));
        if (down) {
          cogito_style_merge_into(&s, &cogito_theme.appbar_child_button_active, &s);
        } else if (over) {
          cogito_style_merge_into(&s, &cogito_theme.appbar_child_button_hover, &s);
        }
      }
      CogitoColor bg = n->bg;
      CogitoColor border = n->border_color;
      CogitoColor text = n->text_color;
      int border_w = n->border_width;
      int radius = n->border_radius;
      if (s.has_bg) bg = s.bg;
      if (s.has_border) border = s.border;
      if (s.has_border_width) border_w = s.border_width;
      if (s.has_radius) radius = s.radius;
      int circular = n->h / 2;
      if (circular > radius) radius = circular;
      // Animate colors
      if (s.has_text) text = s.text;
      else if (cogito_has_selection_text) text = cogito_selection_text;
      cogito_anim_apply(n, &s, &bg, &text, &border, NULL);
      if (n->bg_set || border_w > 0) {
        if (n->shadow_set && n->shadow_level > 0) {
          cogito_draw_shadow_node_level(n, &s, n->x, n->y, n->w, n->h, radius, n->shadow_level);
        }
        cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);
        // Draw ripple effect for Primary-colored buttons (clipped to widget bounds)
        if (cogito_ripple_update(n)) {
          float r_radius, r_alpha;
          cogito_ripple_get(n, &r_radius, &r_alpha);
          if (r_radius > 0 && r_alpha > 0) {
            CogitoColor ripple_color = text;
            ripple_color.a = (uint8_t)(255 * r_alpha);
            int cx = n->x + n->ripple_x;
            int cy = n->y + n->ripple_y;
            cogito_draw_circle_clipped(cx, cy, r_radius, n->x, n->y, n->w, n->h, ripple_color);
          }
        }
        if (border_w > 0) {
          cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, border, radius, border_w);
        }
      }
      if (n->text) {
        int size = cogito_node_font_size(n);
        int tw = cogito_text_width_size_node(n, n->text->data, size);
        int th = cogito_text_height_size(size);
        int tx = n->x + (n->w - tw) / 2;
        int ty = n->y + (n->h - th) / 2;
        cogito_draw_text_size_node(n, tx, ty, n->text->data, text, size, cogito_node_bold(n));
      }
      break;
    }
