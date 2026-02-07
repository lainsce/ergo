case COGITO_TABS: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      Color bg = n->bg;
      if (s.has_bg) bg = s.bg;
      if (n->bg_set || s.has_bg) {
        cogito_draw_rect(n->x, n->y, n->w, n->h, bg, n->border_radius);
      }
      Color text = n->text_color;
      if (cogito_has_selection_text) text = cogito_selection_text;
      else if (s.has_text) text = s.text;
      Color sel = s.has_selection ? s.selection : n->selection_color;
      int size = cogito_node_font_size(n);
      int th = cogito_text_height_size(size);
      int pad_x = 12;
      int pad_y = 8;
      int gap = 6;
      int tab_h = th + pad_y * 2;
      int ty = n->y + (n->h - tab_h) / 2;
      int cx = n->x + n->padding_left;
      for (size_t i = 0; i < n->tab_len; i++) {
        const char* t = n->tab_labels[i] ? n->tab_labels[i]->data : "";
        int tw = cogito_text_width_size_node(n, t, size);
        int tab_w = tw + pad_x * 2;
        if (n->selected == (int)i) {
          int inset = 2;
          int sx = cx + inset;
          int sy = ty + inset;
          int sw = tab_w - inset * 2;
          int sh = tab_h - inset * 2;
          if (sw < 0) sw = 0;
          if (sh < 0) sh = 0;
          int base_radius = n->border_radius;
          int sel_radius = base_radius - inset;
          if (sel_radius < 0) sel_radius = 0;
          cogito_draw_rect(sx, sy, sw, sh, sel, sel_radius);
        }
        cogito_draw_text_size_node(n, cx + pad_x, ty + pad_y, t, text, size, cogito_node_bold(n));
        cx += tab_w + gap;
      }
      break;
    }
