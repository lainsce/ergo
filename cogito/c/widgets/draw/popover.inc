case COGITO_POPOVER: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      if (s.has_bg) { n->bg = s.bg; n->bg_set = true; }
      if (s.has_border) { n->border_color = s.border; n->border_color_set = true; }
      if (s.has_border_width) { n->border_width = s.border_width; n->border_width_set = true; }
      if (s.has_radius) n->border_radius = s.radius;
      if (s.has_shadow) { n->shadow_level = s.shadow_level; n->shadow_set = true; }

      // Elevation shadow for popovers
      if (n->shadow_set && n->shadow_level > 0) {
        cogito_draw_shadow(n->x, n->y, n->w, n->h, n->border_radius, n->shadow_level);
      }

      if (n->bg_set || n->border_width > 0) {
        cogito_draw_rect(n->x, n->y, n->w, n->h, n->bg, n->border_radius);
        if (n->border_width > 0) {
          cogito_draw_rect_lines(n->x, n->y, n->w, n->h, n->border_color, n->border_radius, n->border_width);
        }
      }

      for (size_t i = 0; i < n->len; i++) {
        cogito_draw_node(n->children[i]);
      }
      break;
    }

