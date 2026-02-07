case COGITO_COLORPICKER: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      Color bg = s.has_bg ? s.bg : n->bg;
      Color border = s.has_border ? s.border : n->border_color;
      int border_w = s.has_border_width ? s.border_width : n->border_width;
      int radius = s.has_radius ? s.radius : n->border_radius;

      // Draw background
      cogito_draw_rect(n->x, n->y, n->w, n->h, bg, radius);
      if (border_w > 0) {
        cogito_draw_rect_lines(n->x, n->y, n->w, n->h, border, radius, border_w);
      }

      if (!n->text || !n->text->data || !n->text->data[0] || !n->color_hex_editing) {
        cogito_colorpicker_sync_hex(n);
      }

      int label_w = 0, row_h = 0, slider_h = 0, gap = 0;
      int hex_x = 0, hex_y = 0, hex_w = 0, hex_h = 0;
      cogito_colorpicker_layout(n, NULL, NULL, NULL, NULL, &label_w, &row_h, &slider_h, &gap, &hex_x, &hex_y, &hex_w, &hex_h);

      Color text_col = s.has_text ? s.text : n->text_color;
      Color accent = s.has_selection ? s.selection : n->selection_color;
      int track_h = s.has_track_height ? s.track_height : 4;

      int size = cogito_node_font_size(n);
      const char* labels[3] = {"H", "C", "T"};
      double values[3] = {n->color_h, n->color_c, n->color_t};
      double ranges[3] = {360.0, 100.0, 100.0};

      // Slider drawing parameters (matching Cogito Slider style)
      int thumb_w = 6;
      int thumb_h = 24;
      int thumb_r = 3;
      int thumb_gap = 4;
      int slider_pad = 8;
      Color inactive_track = text_col;
      inactive_track.a = 60;

      for (int i = 0; i < 3; i++) {
        int row_y = n->y + (s.has_padding_top ? s.padding_top : 12) + i * (row_h + gap);
        int lx = n->x + (s.has_padding_left ? s.padding_left : 12);
        int ly = row_y + (row_h - cogito_text_height_size(size)) / 2;

        // Draw label
        cogito_draw_text_size_node(n, lx, ly, labels[i], text_col, size, false);

        // Get slider rect
        int sx = 0, sy = 0, sw = 0, sh = 0;
        cogito_colorpicker_slider_rect(n, i, &sx, &sy, &sw, &sh);

        // Calculate thumb position
        double t = (ranges[i] > 0.0) ? (values[i] / ranges[i]) : 0.0;
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;

        int x0 = sx + slider_pad;
        int x1 = sx + sw - slider_pad;
        if (x1 < x0) x1 = x0;
        int kx = x0 + (int)((x1 - x0) * t);
        int track_cy = row_y + row_h / 2;

        // Thumb edges
        int thumb_left = kx - thumb_w / 2;
        int thumb_right = kx + thumb_w / 2;

        // Draw inactive track (right of thumb)
        int inactive_start = thumb_right + thumb_gap;
        if (inactive_start < x1) {
          int track_r = track_h / 2;
          cogito_draw_rect(inactive_start, track_cy - track_h / 2, x1 - inactive_start, track_h, inactive_track, track_r);
        }

        // Draw active track (left of thumb)
        int active_end = thumb_left - thumb_gap;
        if (active_end > x0) {
          int track_r = track_h / 2;
          cogito_draw_rect(x0, track_cy - track_h / 2, active_end - x0, track_h, accent, track_r);
        }

        // Draw rectangular thumb
        cogito_draw_rect(thumb_left, track_cy - thumb_h / 2, thumb_w, thumb_h, accent, thumb_r);
      }

      // Draw hex input field
      CogitoStyle hs = cogito_theme_resolve(COGITO_TEXTFIELD);
      Color hex_bg = hs.has_bg ? hs.bg : n->bg;
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

      // Draw selection highlight
      if (n->sel_start != n->sel_end) {
        int a = n->sel_start;
        int b = n->sel_end;
        if (a > b) { int tmp = a; a = b; b = tmp; }
        int sx_sel = cogito_text_width_size_n_node(n, hex, a, size);
        int ex = cogito_text_width_size_n_node(n, hex, b, size);
        int x0 = hex_x + pad + sx_sel;
        int x1 = hex_x + pad + ex;
        if (x1 > x0) {
          cogito_draw_rect(x0, ty, x1 - x0, th, accent, 2);
        }
      }

      cogito_draw_text_ellipsis(n, hex_x + pad, ty, hex_w - pad * 2, hex, text_col, size, false, 0);

      // Draw caret if focused and editing
      if (cogito_focused == n && n->color_hex_editing) {
        int caret = n->caret;
        if (caret < 0) caret = 0;
        int tw = cogito_text_width_size_n_node(n, hex, caret, size);
        int cx = hex_x + pad + tw;
        int max_x = hex_x + hex_w - pad;
        if (cx > max_x) cx = max_x;
        double time = GetTime();
        if ((int)(time * 2.0) % 2 == 0) {
          DrawLine(cx, ty, cx, ty + th, text_col);
        }
      }

      // Color preview rectangle
      {
        int prev_pad = s.has_padding_left ? s.padding_left : 12;
        int prev_x = n->x + prev_pad;
        int prev_sz = hex_h;
        Color prev_color = cogito_hct_to_rgb(n->color_h, n->color_c, n->color_t);
        cogito_draw_rect(prev_x, hex_y, prev_sz, prev_sz, prev_color, 6);
        Color prev_border = text_col;
        prev_border.a = 60;
        cogito_draw_rect_lines(prev_x, hex_y, prev_sz, prev_sz, prev_border, 6, 1);
      }
      break;
    }
