case COGITO_APPBAR: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      if (s.has_bg) { n->bg = s.bg; n->bg_set = true; }
      if (s.has_border) { n->border_color = s.border; n->border_color_set = true; }
      if (s.has_border_width) { n->border_width = s.border_width; n->border_width_set = true; }
      if (s.has_radius) n->border_radius = s.radius;
      if (n->bg_set || n->border_width > 0) {
        if (n->shadow_set && n->shadow_level > 0) {
          cogito_draw_shadow(n->x, n->y, n->w, n->h, n->border_radius, n->shadow_level);
        }
        cogito_draw_rect(n->x, n->y, n->w, n->h, n->bg, n->border_radius);
        if (n->border_width > 0) {
          cogito_draw_rect_lines(n->x, n->y, n->w, n->h, n->border_color, n->border_radius, n->border_width);
        }
      }
      int btn = n->appbar_btn_size;
      int by = n->appbar_btn_y;
      int cx = n->appbar_btn_close_x;
      int r = btn / 2;
      int mx = 0, my = 0;
      if (cogito_backend && cogito_backend->get_mouse_position) {
        cogito_backend->get_mouse_position(&mx, &my);
      }
      bool down = false;
      if (cogito_backend && cogito_backend->is_mouse_button_down) {
        down = cogito_backend->is_mouse_button_down(0);  // Left button
      }
      bool over_close = (n->appbar_btn_close_x >= 0) && cogito_hit_rect(mx, my, cx, by, btn, btn);
      bool over_min = (n->appbar_btn_min_x >= 0) && cogito_hit_rect(mx, my, n->appbar_btn_min_x, by, btn, btn);
      bool over_max = (n->appbar_btn_max_x >= 0) && cogito_hit_rect(mx, my, n->appbar_btn_max_x, by, btn, btn);
      int state_close = over_close ? (down ? 2 : 1) : 0;
      int state_min = over_min ? (down ? 2 : 1) : 0;
      int state_max = over_max ? (down ? 2 : 1) : 0;
      CogitoStyle close_s = cogito_appbar_win_btn_style(0, state_close);
      CogitoStyle min_s = cogito_appbar_win_btn_style(1, state_min);
      CogitoStyle max_s = cogito_appbar_win_btn_style(2, state_max);
      CogitoColor close_bg = close_s.has_bg ? close_s.bg : cogito_rgba(255, 95, 87, 255);
      CogitoColor min_bg = min_s.has_bg ? min_s.bg : cogito_rgba(254, 188, 46, 255);
      CogitoColor max_bg = max_s.has_bg ? max_s.bg : cogito_rgba(40, 200, 64, 255);
      if (n->appbar_btn_close_x >= 0 && cogito_backend && cogito_backend->draw_circle) {
        cogito_backend->draw_circle(cx + r, by + r, (float)r, close_bg);
      }
      if (n->appbar_btn_min_x >= 0 && cogito_backend && cogito_backend->draw_circle) {
        cogito_backend->draw_circle(n->appbar_btn_min_x + r, by + r, (float)r, min_bg);
      }
      if (n->appbar_btn_max_x >= 0 && cogito_backend && cogito_backend->draw_circle) {
        cogito_backend->draw_circle(n->appbar_btn_max_x + r, by + r, (float)r, max_bg);
      }
      if (n->appbar_btn_close_x >= 0 && (close_s.has_border || close_s.has_border_width)) {
        int bw = close_s.has_border_width ? close_s.border_width : 1;
        CogitoColor bc = close_s.has_border ? close_s.border : cogito_rgba(0, 0, 0, 40);
        if (bw > 0 && cogito_backend && cogito_backend->draw_circle_lines) {
          cogito_backend->draw_circle_lines(cx + r, by + r, (float)r, bc, bw);
        }
      }
      if (n->appbar_btn_min_x >= 0 && (min_s.has_border || min_s.has_border_width)) {
        int bw = min_s.has_border_width ? min_s.border_width : 1;
        CogitoColor bc = min_s.has_border ? min_s.border : cogito_rgba(0, 0, 0, 40);
        if (bw > 0 && cogito_backend && cogito_backend->draw_circle_lines) {
          cogito_backend->draw_circle_lines(n->appbar_btn_min_x + r, by + r, (float)r, bc, bw);
        }
      }
      if (n->appbar_btn_max_x >= 0 && (max_s.has_border || max_s.has_border_width)) {
        int bw = max_s.has_border_width ? max_s.border_width : 1;
        CogitoColor bc = max_s.has_border ? max_s.border : cogito_rgba(0, 0, 0, 40);
        if (bw > 0 && cogito_backend && cogito_backend->draw_circle_lines) {
          cogito_backend->draw_circle_lines(n->appbar_btn_max_x + r, by + r, (float)r, bc, bw);
        }
      }
      if (n->text) {
        int tsz = cogito_label_class_font_size(COGITO_LABEL_CLASS_TITLE, 24);
        int ty = n->appbar_title_y ? n->appbar_title_y : (n->y + 12);
        CogitoColor tcol = cogito_label_class_color(COGITO_LABEL_CLASS_TITLE, s.has_text ? s.text : n->text_color);
        bool tbold = cogito_label_class_bold(COGITO_LABEL_CLASS_TITLE, true);
        cogito_draw_text_size_node(n, n->x + 16, ty, n->text->data, tcol, tsz, tbold);
      }
      if (n->subtitle) {
        int tsz = cogito_label_class_font_size(COGITO_LABEL_CLASS_TITLE, 24);
        int t_h = cogito_text_height_size(tsz);
        int ssz = cogito_label_class_font_size(COGITO_LABEL_CLASS_SUBTITLE, 18);
        int sy = n->appbar_subtitle_y ? n->appbar_subtitle_y : (n->y + 12 + t_h + 4);
        CogitoColor scol = cogito_label_class_color(COGITO_LABEL_CLASS_SUBTITLE, s.has_text ? s.text : n->text_color);
        bool sbold = cogito_label_class_bold(COGITO_LABEL_CLASS_SUBTITLE, false);
        cogito_draw_text_size_node(n, n->x + 16, sy, n->subtitle->data, scol, ssz, sbold);
      }
      for (size_t i = 0; i < n->len; i++) {
        cogito_draw_node(n->children[i]);
      }
      break;
    }
