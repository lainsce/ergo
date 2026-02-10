case COGITO_IMAGE: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      if (s.has_bg) { n->bg = s.bg; n->bg_set = true; }
      else { n->bg_set = false; }
      if (s.has_border) { n->border_color = s.border; n->border_color_set = true; }
      else { n->border_color_set = false; }
      if (s.has_border_width) { n->border_width = s.border_width; n->border_width_set = true; }
      else { n->border_width_set = false; }
      if (s.has_radius) n->border_radius = s.radius;
      if (n->bg_set || n->border_width > 0) {
        if (n->shadow_set && n->shadow_level > 0) {
          cogito_draw_shadow_node_level(n, &s, n->x, n->y, n->w, n->h, n->border_radius, n->shadow_level);
        }
        cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, n->bg, n->border_radius);
        if (n->border_width > 0) {
          cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, n->border_color, n->border_radius, n->border_width);
        }
      }
      CogitoColor icon_color = cogito_has_selection_text ? cogito_selection_text : (s.has_text ? s.text : cogito_on_color(n->selection_color));
      if (n->icon && n->icon->data) {
        int target = n->w < n->h ? n->w : n->h;
        if (target <= 0) target = 16;
        int ix = n->x + (n->w - target) / 2;
        int iy = n->y + (n->h - target) / 2;
        cogito_draw_icon_fallback(n, n->icon->data, ix, iy, target, icon_color);
      }
      break;
    }
