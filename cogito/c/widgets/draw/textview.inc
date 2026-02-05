case COGITO_TEXTVIEW: {
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
        Color focus = s.has_selection ? s.selection : (n->selection_set ? n->selection_color : cogito_rgba(120, 160, 220, 255));
        cogito_draw_rect_lines(n->x, n->y, n->w, n->h, focus, radius, border_w > 0 ? border_w : 2);
      }
      int pad = 8;
      int size = cogito_node_font_size(n);
      Color text = n->text_color;
      if (s.has_text) text = s.text;
      const char* t = n->text ? n->text->data : "";
      if (n->sel_start != n->sel_end) {
        int a = n->sel_start;
        int b = n->sel_end;
        if (a > b) { int tmp = a; a = b; b = tmp; }
        int max_w = n->w - pad * 2;
        if (max_w < 1) max_w = 1;
        int len = (int)strlen(t);
        int idx = 0;
        int line_h = cogito_text_height_size(size);
        int cy = n->y + pad;
        Color sel = s.has_selection ? s.selection : (n->selection_set ? n->selection_color : cogito_rgba(200, 220, 250, 255));
        while (idx < len) {
          int line_start = idx;
          int line_len = 0;
          int last_space = -1;
          for (int i = idx; i < len; i++) {
            char c = t[i];
            if (c == '\n') {
              line_len = i - idx;
              idx = i + 1;
              break;
            }
            int w = cogito_text_width_size_n_node(n, t + idx, i - idx + 1, size);
            if (c == ' ') last_space = i;
            if (w > max_w) {
              if (last_space >= idx) {
                line_len = last_space - idx;
                idx = last_space + 1;
              } else {
                line_len = i - idx;
                idx = i;
              }
              break;
            }
            if (i == len - 1) {
              line_len = i - idx + 1;
              idx = len;
            }
          }
          int line_end = line_start + line_len;
          if (b > line_start && a < line_end) {
            int sa = a > line_start ? a : line_start;
            int sb = b < line_end ? b : line_end;
            int sx = cogito_text_width_size_n_node(n, t + line_start, sa - line_start, size);
            int ex = cogito_text_width_size_n_node(n, t + line_start, sb - line_start, size);
            int x0 = n->x + pad + sx;
            int x1 = n->x + pad + ex;
            if (x1 > x0) {
              cogito_draw_rect(x0, cy, x1 - x0, line_h, sel, 2);
            }
          }
          cy += line_h;
        }
      }
      cogito_draw_text_wrapped(n, n->x + pad, n->y + pad, n->w - pad * 2, t, text, size, cogito_node_bold(n), 0, true);
      if (cogito_focused == n && !n->disabled) {
        int max_w = n->w - pad * 2;
        if (max_w < 1) max_w = 1;
        int len = (int)strlen(t);
        int idx = 0;
        int line_h = cogito_text_height_size(size);
        int cy = n->y + pad;
        int caret_idx = n->caret;
        if (caret_idx < 0) caret_idx = 0;
        if (caret_idx > len) caret_idx = len;
        while (idx <= len) {
          int line_start = idx;
          int line_len = 0;
          int last_space = -1;
          for (int i = idx; i < len; i++) {
            char c = t[i];
            if (c == '\n') {
              line_len = i - idx;
              idx = i + 1;
              break;
            }
            int w = cogito_text_width_size_n_node(n, t + idx, i - idx + 1, size);
            if (c == ' ') last_space = i;
            if (w > max_w) {
              if (last_space >= idx) {
                line_len = last_space - idx;
                idx = last_space + 1;
              } else {
                line_len = i - idx;
                idx = i;
              }
              break;
            }
            if (i == len - 1) {
              line_len = i - idx + 1;
              idx = len;
            }
          }
          int line_end = line_start + line_len;
          if (caret_idx >= line_start && caret_idx <= line_end) {
            int local = caret_idx - line_start;
            int cx = n->x + pad + cogito_text_width_size_n_node(n, t + line_start, local, size);
            int max_x = n->x + n->w - pad;
            if (cx > max_x) cx = max_x;
            DrawLine(cx, cy, cx, cy + line_h, text);
            break;
          }
          cy += line_h;
          if (idx >= len) break;
        }
      }
      break;
    }
    
