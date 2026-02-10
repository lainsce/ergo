case COGITO_APPBAR: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      CogitoColor bg = n->bg;
      CogitoColor border = n->border_color;
      CogitoColor text = s.has_text ? s.text : n->text_color;
      int border_w = n->border_width;
      int radius = n->border_radius;
      if (s.has_bg) bg = s.bg;
      if (s.has_border) border = s.border;
      if (s.has_border_width) border_w = s.border_width;
      if (s.has_radius) radius = s.radius;
      cogito_anim_apply(n, &s, &bg, &text, &border, NULL);
      if (n->bg_set || s.has_bg || border_w > 0) {
        if (n->shadow_set && n->shadow_level > 0) {
          cogito_draw_shadow_node_level(n, &s, n->x, n->y, n->w, n->h, radius, n->shadow_level);
        }
        cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);
        if (border_w > 0) {
          cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, border, radius, border_w);
        }
      }
	      int btn = n->appbar_btn_size;
	      int by = n->appbar_btn_y;
	      int cx = n->appbar_btn_close_x;
	      int center_off = btn / 2;
	      float light_r = (btn / 14.0) * 9.0;
	      int light_d = (int)(light_r * 2.0f + 0.5f);
	      if (light_d < 2) light_d = 2;
	      int light_rad = light_d / 2;
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
      CogitoColor fallback_btn = s.has_selection ? s.selection : n->selection_color;
      CogitoColor fallback_border = border;
      if (!s.has_border) fallback_border = text;
      if (fallback_border.a > 80) fallback_border.a = 80;
      CogitoColor close_bg = close_s.has_bg ? close_s.bg
        : (s.has_appbar_btn_close_color ? s.appbar_btn_close_color : fallback_btn);
      CogitoColor min_bg = min_s.has_bg ? min_s.bg
        : (s.has_appbar_btn_min_color ? s.appbar_btn_min_color : fallback_btn);
      CogitoColor max_bg = max_s.has_bg ? max_s.bg
        : (s.has_appbar_btn_max_color ? s.appbar_btn_max_color : fallback_btn);
	      int close_lx = cx + center_off - light_d / 2;
	      int min_lx = n->appbar_btn_min_x + center_off - light_d / 2;
	      int max_lx = n->appbar_btn_max_x + center_off - light_d / 2;
	      int lights_y = by + center_off - light_d / 2;
	      if (n->appbar_btn_close_x >= 0) {
	        cogito_draw_rect(close_lx, lights_y, light_d, light_d, close_bg, light_rad);
	      }
	      if (n->appbar_btn_min_x >= 0) {
	        cogito_draw_rect(min_lx, lights_y, light_d, light_d, min_bg, light_rad);
	      }
	      if (n->appbar_btn_max_x >= 0) {
	        cogito_draw_rect(max_lx, lights_y, light_d, light_d, max_bg, light_rad);
	      }
	      if (n->appbar_btn_close_x >= 0 && (close_s.has_border || close_s.has_border_width)) {
	        int bw = close_s.has_border_width ? close_s.border_width : 1;
	        CogitoColor bc = close_s.has_border ? close_s.border : fallback_border;
	        if (bw > 0) {
	          cogito_draw_rect_lines(close_lx, lights_y, light_d, light_d, bc, light_rad, bw);
	        }
	      }
	      if (n->appbar_btn_min_x >= 0 && (min_s.has_border || min_s.has_border_width)) {
	        int bw = min_s.has_border_width ? min_s.border_width : 1;
	        CogitoColor bc = min_s.has_border ? min_s.border : fallback_border;
	        if (bw > 0) {
	          cogito_draw_rect_lines(min_lx, lights_y, light_d, light_d, bc, light_rad, bw);
	        }
	      }
	      if (n->appbar_btn_max_x >= 0 && (max_s.has_border || max_s.has_border_width)) {
	        int bw = max_s.has_border_width ? max_s.border_width : 1;
	        CogitoColor bc = max_s.has_border ? max_s.border : fallback_border;
	        if (bw > 0) {
	          cogito_draw_rect_lines(max_lx, lights_y, light_d, light_d, bc, light_rad, bw);
	        }
	      }
      if (n->text) {
        int tsz = cogito_label_class_font_size(COGITO_LABEL_CLASS_TITLE, 24);
        int ty = n->appbar_title_y ? n->appbar_title_y : (n->y + 12);
        CogitoColor tcol = cogito_label_class_color(COGITO_LABEL_CLASS_TITLE, text);
        bool tbold = cogito_label_class_bold(COGITO_LABEL_CLASS_TITLE, true);
        cogito_draw_text_size_node(n, n->x + 16, ty, n->text->data, tcol, tsz, tbold);
      }
      if (n->subtitle) {
        int tsz = cogito_label_class_font_size(COGITO_LABEL_CLASS_TITLE, 24);
        int t_h = cogito_text_height_size(tsz);
        int ssz = cogito_label_class_font_size(COGITO_LABEL_CLASS_SUBTITLE, 18);
        int sy = n->appbar_subtitle_y ? n->appbar_subtitle_y : (n->y + 12 + t_h + 4);
        CogitoColor scol = cogito_label_class_color(COGITO_LABEL_CLASS_SUBTITLE, text);
        bool sbold = cogito_label_class_bold(COGITO_LABEL_CLASS_SUBTITLE, false);
        cogito_draw_text_size_node(n, n->x + 16, sy, n->subtitle->data, scol, ssz, sbold);
      }
      for (size_t i = 0; i < n->len; i++) {
        cogito_draw_node(n->children[i]);
      }
      break;
    }
