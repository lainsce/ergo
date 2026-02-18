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

      int pad = 8;
      int size = cogito_node_font_size(n);
      const char* t = n->text ? n->text->data : "";
      bool has_text = (t[0] != '\0');
      int th = cogito_text_height_size(size);
      const char* placeholder = (n->subtitle && n->subtitle->data && n->subtitle->data[0]) ? n->subtitle->data : NULL;
      bool has_placeholder = placeholder && placeholder[0];
      const char* hint = (n->hint && n->hint->data && n->hint->data[0]) ? n->hint->data : NULL;
      bool has_hint = hint && hint[0];
      int hint_size = cogito_label_class_font_size(COGITO_LABEL_CLASS_CAPTION, 12);
      bool hint_bold = cogito_label_class_bold(COGITO_LABEL_CLASS_CAPTION, true);
      CogitoColor hint_base = text;
      hint_base.a = (uint8_t)(hint_base.a * 0.78f);
      CogitoColor hint_color = cogito_label_class_color(COGITO_LABEL_CLASS_CAPTION, hint_base);
      int hint_h = has_hint ? cogito_text_height_size(hint_size) : 0;
      int helper_extra = has_hint ? (4 + hint_h) : 0;
      int field_h = n->h - helper_extra;
      bool draw_hint = has_hint;
      if (field_h < 24) {
        field_h = n->h;
        draw_hint = false;
      }

      if (n->bg_set) {
        cogito_draw_rect_node(n, &s, n->x, n->y, n->w, field_h, bg, radius);
      }

      int ty = n->y + (field_h - th) / 2;
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
      if (draw_hint) {
        int hint_y = n->y + field_h + 4;
        cogito_draw_text_ellipsis(n, n->x + pad, hint_y, n->w - pad * 2, hint, hint_color, hint_size, hint_bold, 0);
      }
      break;
    }
