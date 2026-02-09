case COGITO_LIST: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      if (s.has_bg) { n->bg = s.bg; n->bg_set = true; }
      if (s.has_border) { n->border_color = s.border; n->border_color_set = true; }
      if (s.has_border_width) { n->border_width = s.border_width; n->border_width_set = true; }
      if (s.has_radius) n->border_radius = s.radius;
      if (n->bg_set || n->border_width > 0) {
        if (n->shadow_set && n->shadow_level > 0) {
          cogito_draw_shadow(n->x, n->y, n->w, n->h, n->border_radius, n->shadow_level);
        }
        cogito_draw_rect(n->x, n->y, n->w, n->h, n->bg, n->border_radius);
        if (n->border_width > 0) {
          cogito_draw_rect_lines(n->x, n->y, n->w, n->h, n->border_color, n->border_radius, n->border_width);
        }
      }
      bool has_selection = n->selected >= 0 && (size_t)n->selected < n->len;
      CogitoColor sel_bg = s.has_selection ? s.selection : n->selection_color;
      if (has_selection) {
        CogitoNode* sel = n->children[n->selected];
        cogito_draw_rect(sel->x, sel->y, sel->w, sel->h, sel_bg, 0);
      }
      for (size_t i = 0; i < n->len; i++) {
        CogitoNode* c = n->children[i];
        if (has_selection && (size_t)n->selected == i) {
          cogito_has_selection_text = true;
          cogito_selection_text = cogito_on_color(sel_bg);
          cogito_draw_node(c);
          cogito_has_selection_text = false;
        } else {
          cogito_draw_node(c);
        }
      }
      break;
    }
