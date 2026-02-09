case COGITO_DIALOG: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      if (s.has_bg) { n->bg = s.bg; n->bg_set = true; }
      if (s.has_border) { n->border_color = s.border; n->border_color_set = true; }
      if (s.has_border_width) { n->border_width = s.border_width; n->border_width_set = true; }
      if (s.has_radius) n->border_radius = s.radius;
      if (s.has_shadow) { n->shadow_level = s.shadow_level; n->shadow_set = true; }

      // Draw box-shadow from SUM
      if (s.has_box_shadow && s.box_shadow.blur > 0) {
        int shadow_level = (int)(s.box_shadow.blur / 3);
        if (shadow_level < 1) shadow_level = 1;
        if (shadow_level > 5) shadow_level = 5;
        cogito_draw_shadow(n->x, n->y, n->w, n->h, n->border_radius, shadow_level);
      } else if (n->shadow_set && n->shadow_level > 0) {
        cogito_draw_shadow(n->x, n->y, n->w, n->h, n->border_radius, n->shadow_level);
      }

      if (n->bg_set) {
        cogito_draw_rect(n->x, n->y, n->w, n->h, n->bg, n->border_radius);
      }
      if (n->border_width > 0) {
        cogito_draw_rect_lines(n->x, n->y, n->w, n->h, n->border_color, n->border_radius, n->border_width);
      }

      // Draw title if set
      if (n->text && n->text->data[0]) {
        CogitoColor title_color = s.has_text ? s.text : n->text_color;
        int title_size = 20;
        int title_h = cogito_text_height_size(title_size);
        int pad_l = s.has_padding_left ? s.padding_left : 24;
        int pad_t = s.has_padding_top ? s.padding_top : 24;
        cogito_draw_text_size_node(n, n->x + pad_l, n->y + pad_t, n->text->data, title_color, title_size, false);
        (void)title_h;
      }

      // Draw children
      for (size_t i = 0; i < n->len; i++) {
        cogito_draw_node(n->children[i]);
      }
      break;
    }
