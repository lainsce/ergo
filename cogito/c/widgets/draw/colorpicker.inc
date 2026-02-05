case COGITO_COLORPICKER: {
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
      if (!n->text || !n->text->data || !n->text->data[0] || !n->color_hex_editing) {
        cogito_colorpicker_sync_hex(n);
      }
      int label_w = 0, row_h = 0, slider_h = 0, gap = 0;
      int hex_x = 0, hex_y = 0, hex_w = 0, hex_h = 0;
      cogito_colorpicker_layout(n, NULL, NULL, NULL, NULL, &label_w, &row_h, &slider_h, &gap, &hex_x, &hex_y, &hex_w, &hex_h);
      Color text = s.has_text ? s.text : n->text_color;
      Color accent = s.has_selection ? s.selection : cogito_rgba(114, 222, 194, 255);
      int size = cogito_node_font_size(n);
      const char* labels[3] = {"H", "C", "T"};
      double values[3] = {n->color_h, n->color_c, n->color_t};
      double ranges[3] = {360.0, 100.0, 100.0};
      for (int i = 0; i < 3; i++) {
        int row_y = n->y + (s.has_padding_top ? s.padding_top : 10) + i * (row_h + gap);
        int lx = n->x + (s.has_padding_left ? s.padding_left : 10);
        int ly = row_y + (row_h - cogito_text_height_size(size)) / 2;
        cogito_draw_text_size_node(n, lx, ly, labels[i], text, size, false);
        int sx = 0, sy = 0, sw = 0, sh = 0;
        cogito_colorpicker_slider_rect(n, i, &sx, &sy, &sw, &sh);
        DrawLine(sx, sy + sh / 2, sx + sw, sy + sh / 2, cogito_rgba(190, 190, 190, 255));
        double t = (ranges[i] > 0.0) ? (values[i] / ranges[i]) : 0.0;
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;
        int kx = sx + (int)(sw * t);
        DrawCircle(kx, sy + sh / 2, 6, accent);
      }
      CogitoStyle hs = cogito_theme_resolve(COGITO_TEXTFIELD);
      Color hex_bg = hs.has_bg ? hs.bg : cogito_rgba(245, 245, 245, 255);
      Color hex_border = hs.has_border ? hs.border : cogito_rgba(0, 0, 0, 0);
      int hex_bw = hs.has_border_width ? hs.border_width : 0;
      int hex_radius = hs.has_radius ? hs.radius : 6;
      cogito_draw_rect(hex_x, hex_y, hex_w, hex_h, hex_bg, hex_radius);
      if (hex_bw > 0) {
        cogito_draw_rect_lines(hex_x, hex_y, hex_w, hex_h, hex_border, hex_radius, hex_bw);
      }
      int pad = 8;
      const char* hex = n->text ? n->text->data : "#000000";
      int th = cogito_text_height_size(size);
      int ty = hex_y + (hex_h - th) / 2;
      if (n->sel_start != n->sel_end) {
        int a = n->sel_start;
        int b = n->sel_end;
        if (a > b) { int tmp = a; a = b; b = tmp; }
        int sx = cogito_text_width_size_n_node(n, hex, a, size);
        int ex = cogito_text_width_size_n_node(n, hex, b, size);
        int x0 = hex_x + pad + sx;
        int x1 = hex_x + pad + ex;
        if (x1 > x0) {
          cogito_draw_rect(x0, ty, x1 - x0, th, accent, 2);
        }
      }
      cogito_draw_text_ellipsis(n, hex_x + pad, ty, hex_w - pad * 2, hex, text, size, false, 0);
      if (cogito_focused == n && n->color_hex_editing) {
        int caret = n->caret;
        if (caret < 0) caret = 0;
        int tw = cogito_text_width_size_n_node(n, hex, caret, size);
        int cx = hex_x + pad + tw;
        int max_x = hex_x + hex_w - pad;
        if (cx > max_x) cx = max_x;
        DrawLine(cx, ty, cx, ty + th, text);
      }
      break;
    }
    
