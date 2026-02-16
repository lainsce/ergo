case COGITO_TEXTFIELD: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, false);
      CogitoColor bg = n->bg;
      CogitoColor text = n->text_color;
      int radius = n->border_radius;
      if (s.has_bg) bg = s.bg;
      if (s.has_radius) radius = s.radius;
      if (cogito_has_selection_text) text = cogito_selection_text;
      else if (s.has_text) text = s.text;
      cogito_anim_apply(n, &s, &bg, &text, NULL, NULL);
      if (n->bg_set) {
        cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);
      }
      // Draw focus border only when focused via keyboard (tab), not mouse click
      int pad = 8;
      int size = cogito_node_font_size(n);
      const char* t = n->text ? n->text->data : "";
      bool has_text = (t[0] != '\0');
      int th = cogito_text_height_size(size);
      const char* placeholder = (n->subtitle && n->subtitle->data && n->subtitle->data[0]) ? n->subtitle->data : NULL;
      bool has_placeholder = placeholder && placeholder[0];
      int ty = n->y + (n->h - th) / 2;
      bool focused = (cogito_focused == n && !n->disabled && n->editable);
      bool float_placeholder = has_placeholder && (focused || has_text);
      if (has_placeholder) {
        double now = 0.0;
        if (cogito_backend && cogito_backend->get_time) {
          now = cogito_backend->get_time();
        }
        CogitoColor ph = text;
        ph.a = 140;
        int py_target = ty;
        if (float_placeholder) {
          py_target = ty - th - 4;
          int min_py = n->y + 2;
          if (py_target < min_py) py_target = min_py;
        }
        if (!n->anim_layout_active) {
          n->anim_layout_active = true;
          n->anim_y = n->anim_y_from = n->anim_y_target = py_target;
          n->anim_y_start = now;
        }
        int py = cogito_anim_int(n, py_target, &n->anim_y, &n->anim_y_from, &n->anim_y_target, &n->anim_y_start, now);
        cogito_draw_text_ellipsis(n, n->x + pad, py, n->w - pad * 2, placeholder, ph, size, false, 0);
      }
      if (n->text_input.sel_start != n->text_input.sel_end) {
        int a = n->text_input.sel_start;
        int b = n->text_input.sel_end;
        if (a > b) { int tmp = a; a = b; b = tmp; }
        int sx = cogito_text_width_size_n_node(n, t, a, size);
        int ex = cogito_text_width_size_n_node(n, t, b, size);
        int x0 = n->x + pad + sx;
        int x1 = n->x + pad + ex;
        int max_x = n->x + n->w - pad;
        if (x0 < n->x + pad) x0 = n->x + pad;
        if (x1 > max_x) x1 = max_x;
        if (x1 > x0) {
          CogitoColor sel = s.has_selection ? s.selection : n->selection_color;
          cogito_draw_rect(x0, ty, x1 - x0, th, sel, 2);
        }
      }
      cogito_draw_text_ellipsis(n, n->x + pad, ty, n->w - pad * 2, t, text, size, cogito_node_bold(n), 0);
      if (n->text_input.sel_start != n->text_input.sel_end) {
        int a2 = n->text_input.sel_start;
        int b2 = n->text_input.sel_end;
        if (a2 > b2) { int tmp = a2; a2 = b2; b2 = tmp; }
        int len2 = (int)strlen(t);
        if (a2 < 0) a2 = 0;
        if (b2 > len2) b2 = len2;
        if (b2 > a2) {
          CogitoColor sel2 = s.has_selection ? s.selection : n->selection_color;
          CogitoColor sel_text = cogito_on_color(sel2);
          int sx2 = cogito_text_width_size_n_node(n, t, a2, size);
          char sbuf[1024];
          int slen = b2 - a2;
          if (slen > 1000) slen = 1000;
          memcpy(sbuf, t + a2, (size_t)slen);
          sbuf[slen] = '\0';
          cogito_draw_text_size_node(n, n->x + pad + sx2, ty, sbuf, sel_text, size, cogito_node_bold(n));
        }
      }
      if (cogito_focused == n && !n->disabled && n->editable) {
        int caret = n->text_input.caret;
        if (caret < 0) caret = 0;
        int tw = cogito_text_width_size_n_node(n, t, caret, size);
        int cx = n->x + pad + tw;
        int max_x = n->x + n->w - pad;
        if (cx > max_x) cx = max_x;
        cogito_draw_line(cx, ty, cx, ty + th, text);
      }
      break;
    }
