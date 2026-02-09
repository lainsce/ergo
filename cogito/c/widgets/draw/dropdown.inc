case COGITO_DROPDOWN: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      CogitoColor bg = n->bg;
      CogitoColor border = n->border_color;
      int border_w = n->border_width;
      int radius = n->border_radius;
      if (s.has_bg) bg = s.bg;
      if (s.has_border) border = s.border;
      if (s.has_border_width) border_w = s.border_width;
      if (s.has_radius) radius = s.radius;
      if (n->bg_set || border_w > 0) {
        cogito_draw_rect(n->x, n->y, n->w, n->h, bg, radius);
        if (border_w > 0) {
          cogito_draw_rect_lines(n->x, n->y, n->w, n->h, border, radius, border_w);
        }
      }
      int pad = 8;
      int size = cogito_node_font_size(n);
      CogitoColor text = n->text_color;
      if (cogito_has_selection_text) text = cogito_selection_text;
      else if (s.has_text) text = s.text;
      const char* label = "";
      if (n->menu_len > 0 && n->selected >= 0 && (size_t)n->selected < n->menu_len) {
        label = n->menu_labels[n->selected] ? n->menu_labels[n->selected]->data : "";
      }
      int th = cogito_text_height_size(size);
      int ty = n->y + (n->h - th) / 2;
      int icon_size = s.has_icon_size ? s.icon_size : 12;
      if (icon_size < 8) icon_size = 8;
      int icon_box = icon_size + 4;
      int text_w = n->w - pad * 2 - icon_box;
      cogito_draw_text_ellipsis(n, n->x + pad, ty, text_w, label, text, size, cogito_node_bold(n), 0);
      int ix = n->x + n->w - pad - icon_box + (icon_box - icon_size) / 2;
      int iy = n->y + (n->h - icon_size) / 2;
      // TODO: Replace with backend texture drawing
      // Texture2D tex = {0};
      // int tw = 0;
      // int th_i = 0;
      // CogitoColor tint = s.has_icon_color ? s.icon_color : text;
      // const char* drop_icon = "pan-down";
      // if (cogito_icon_load_texture(drop_icon, icon_size, &tex, &tw, &th_i)) {
      //   cogito_backend->draw_texture(...);
      // }
      // Draw dropdown arrow using lines
      int ax = n->x + n->w - pad - 8;
      int ay = n->y + n->h / 2 - 2;
      cogito_draw_line(ax, ay, ax + 4, ay + 6, text);
      cogito_draw_line(ax + 4, ay + 6, ax + 8, ay, text);
      break;
    }
