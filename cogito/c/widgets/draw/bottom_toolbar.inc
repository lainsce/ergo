case COGITO_TOOLBAR: {
      bool hover = false;
      bool active = false;
      bool floating = cogito_toolbar_is_floating(n);
      bool vibrant = floating && n->toolbar.vibrant;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      
      // Get tertiary colors for vibrant mode
      CogitoColor tertiary = {0}, tertiary_container = {0};
      if (vibrant) {
        double h = 0.0, c = 0.0, t = 0.0;
        cogito_rgb_to_hct(cogito_theme_accent, &h, &c, &t);
        double tert_h = fmod(h + 60.0, 360.0);
        if (tert_h < 0.0) tert_h += 360.0;
        double tert_c = 24.0;
        tertiary = cogito_hct_to_rgb(tert_h, tert_c, cogito_theme_dark ? 80.0 : 40.0);
        tertiary_container = cogito_hct_to_rgb(tert_h, tert_c, cogito_theme_dark ? 30.0 : 90.0);
      }
      
      if (s.has_bg) { n->bg = s.bg; n->bg_set = true; }
      
      // Override with tertiary colors in vibrant mode
      if (vibrant) {
        n->bg = tertiary_container;
        n->bg_set = true;
        n->border_color = tertiary;
        n->border_width = 1;
      }
      
      if (s.has_radius) {
        n->border_radius = s.radius;
      } else if (floating) {
        n->border_radius = n->h / 2;
      }
      if (n->bg_set) {
        int shadow_level = 0;
        if (n->shadow_set && n->shadow_level > 0) {
          shadow_level = n->shadow_level;
        } else if (floating) {
          shadow_level = 1;
        }
        if (shadow_level > 0) {
          cogito_draw_shadow_node_level(n, &s, n->x, n->y, n->w, n->h, n->border_radius, shadow_level);
        }
        cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, n->bg, n->border_radius);
        if (n->border_width > 0) {
          cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, n->border_color, n->border_radius, n->border_width);
        }
      }
      for (size_t i = 0; i < n->len; i++) {
        CogitoNode* c = n->children[i];
        if (!c || c->w <= 0 || c->h <= 0) continue;
        cogito_draw_node(c);
      }
      break;
    }
    
