case COGITO_TEXTFIELD: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      Color bg = n->bg;
      Color border = n->border_color;
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
      if (cogito_focused == n && !n->disabled) {
        Color focus = s.has_selection ? s.selection : n->selection_color;
        cogito_draw_rect_lines(n->x, n->y, n->w, n->h, focus, radius, border_w > 0 ? border_w : 2);
      }
      int pad = 8;
      int size = cogito_node_font_size(n);
      Color text = n->text_color;
      if (cogito_has_selection_text) text = cogito_selection_text;
      else if (s.has_text) text = s.text;
      const char* t = n->text ? n->text->data : "";
      int th = cogito_text_height_size(size);
      int ty = n->y + (n->h - th) / 2;
      if (n->sel_start != n->sel_end) {
        int a = n->sel_start;
        int b = n->sel_end;
        if (a > b) { int tmp = a; a = b; b = tmp; }
        int sx = cogito_text_width_size_n_node(n, t, a, size);
        int ex = cogito_text_width_size_n_node(n, t, b, size);
        int x0 = n->x + pad + sx;
        int x1 = n->x + pad + ex;
        int max_x = n->x + n->w - pad;
        if (x0 < n->x + pad) x0 = n->x + pad;
        if (x1 > max_x) x1 = max_x;
        if (x1 > x0) {
          Color sel = s.has_selection ? s.selection : n->selection_color;
          cogito_draw_rect(x0, ty, x1 - x0, th, sel, 2);
        }
      }
      cogito_draw_text_ellipsis(n, n->x + pad, ty, n->w - pad * 2, t, text, size, cogito_node_bold(n), 0);
      if (n->sel_start != n->sel_end) {
        int a2 = n->sel_start;
        int b2 = n->sel_end;
        if (a2 > b2) { int tmp = a2; a2 = b2; b2 = tmp; }
        int len2 = (int)strlen(t);
        if (a2 < 0) a2 = 0;
        if (b2 > len2) b2 = len2;
        if (b2 > a2) {
          Color sel2 = s.has_selection ? s.selection : n->selection_color;
          Color sel_text = cogito_on_color(sel2);
          int sx2 = cogito_text_width_size_n_node(n, t, a2, size);
          char sbuf[1024];
          int slen = b2 - a2;
          if (slen > 1000) slen = 1000;
          memcpy(sbuf, t + a2, (size_t)slen);
          sbuf[slen] = '\0';
          cogito_draw_text_size_node(n, n->x + pad + sx2, ty, sbuf, sel_text, size, cogito_node_bold(n));
        }
      }
      if (cogito_focused == n && !n->disabled) {
        int caret = n->caret;
        if (caret < 0) caret = 0;
        int tw = cogito_text_width_size_n_node(n, t, caret, size);
        int cx = n->x + pad + tw;
        int max_x = n->x + n->w - pad;
        if (cx > max_x) cx = max_x;
        DrawLine(cx, ty, cx, ty + th, text);
      }
      break;
    }
