case COGITO_DRAWING_AREA: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      CogitoColor bg = s.has_bg ? s.bg : (n->bg_set ? n->bg : cogito_theme.base.bg);
      CogitoColor border = s.has_border ? s.border : (n->border_color_set ? n->border_color : cogito_color(0, 0, 0, 40));
      int border_w = s.has_border_width ? s.border_width : (n->border_width_set ? n->border_width : 1);
      int radius = n->radius_set ? n->border_radius : (s.has_radius ? s.radius : n->border_radius);
      if (radius < 0) radius = 0;
      cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);
      if (border_w > 0) {
        cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, border, radius, border_w);
      }
      break;
    }
