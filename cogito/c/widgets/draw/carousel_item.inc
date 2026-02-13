case COGITO_CAROUSEL_ITEM: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      CogitoColor bg = s.has_bg ? s.bg : n->bg;
      CogitoColor border = s.has_border ? s.border : n->border_color;
      int radius = s.has_radius ? s.radius : n->border_radius;
      int border_w = s.has_border_width ? s.border_width : n->border_width;
      
      cogito_anim_apply(n, &s, &bg, NULL, &border, NULL);
      
      if (n->bg_set || s.has_bg || border_w > 0) {
        if (n->shadow_set && n->shadow_level > 0) {
          cogito_draw_shadow_node_level(n, &s, n->x, n->y, n->w, n->h, radius, n->shadow_level);
        }
        cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);
        if (border_w > 0) {
          cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, border, radius, border_w);
        }
      }
      
      // Draw children - clipping is handled by parent carousel
      for (size_t i = 0; i < n->len; i++) {
        cogito_draw_node(n->children[i]);
      }
      break;
    }
