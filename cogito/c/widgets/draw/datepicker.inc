case COGITO_DATEPICKER: {
      cogito_datepicker_ensure_date(n);
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
        cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);
        if (border_w > 0) {
          cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, border, radius, border_w);
        }
      }
      int pad_l = 0, pad_t = 0, pad_r = 0, pad_b = 0;
      int header_h = 0, grid_x = 0, grid_y = 0, cell_w = 0, cell_h = 0;
      cogito_datepicker_layout(n, &pad_l, &pad_t, &pad_r, &pad_b, &header_h, &grid_x, &grid_y, &cell_w, &cell_h);
      int size = cogito_node_font_size(n);
      CogitoColor text = s.has_text ? s.text : n->text_color;
      static const char* months[] = {"January","February","March","April","May","June","July","August","September","October","November","December"};
      const char* mname = (n->datepicker.month >= 1 && n->datepicker.month <= 12) ? months[n->datepicker.month - 1] : "";
      char header[64];
      snprintf(header, sizeof(header), "%s %d", mname, n->datepicker.year);
      int header_w = n->w - pad_l - pad_r;
      int tw = cogito_text_width_size_node(n, header, size);
      int tx = n->x + pad_l + (header_w - tw) / 2;
      int ty = n->y + pad_t + (header_h - cogito_text_height_size(size)) / 2;
      cogito_draw_text_size_node(n, tx, ty, header, text, size, false);
      int left_x = n->x + pad_l;
      int right_x = n->x + n->w - pad_r - header_h;
      int arrow_y = n->y + pad_t + (header_h - cogito_text_height_size(size)) / 2;
      int btn_y = n->y + pad_t;
      int mx = 0, my = 0;
      bool down = false;
      if (cogito_backend && cogito_backend->get_mouse_position) {
        cogito_backend->get_mouse_position(&mx, &my);
      }
      if (cogito_backend && cogito_backend->is_mouse_button_down) {
        down = cogito_backend->is_mouse_button_down(0);
      }
      bool over_left = cogito_hit_rect(mx, my, left_x, btn_y, header_h, header_h);
      bool over_right = cogito_hit_rect(mx, my, right_x, btn_y, header_h, header_h);
      CogitoColor btn_bg = text;
      CogitoColor btn_border = text;
      btn_border.a = 90;
      CogitoColor btn_bg_left = btn_bg;
      CogitoColor btn_bg_right = btn_bg;
      btn_bg_left.a = over_left ? (down ? 96 : 68) : 42;
      btn_bg_right.a = over_right ? (down ? 96 : 68) : 42;
      int btn_r = header_h / 2;
      if (btn_r < 4) btn_r = 4;
      cogito_draw_rect(left_x, btn_y, header_h, header_h, btn_bg_left, btn_r);
      cogito_draw_rect(right_x, btn_y, header_h, header_h, btn_bg_right, btn_r);
      cogito_draw_rect_lines(left_x, btn_y, header_h, header_h, btn_border, btn_r, 1);
      cogito_draw_rect_lines(right_x, btn_y, header_h, header_h, btn_border, btn_r, 1);
      cogito_draw_text_size_node(n, left_x + (header_h / 2) - 4, arrow_y, "<", text, size, false);
      cogito_draw_text_size_node(n, right_x + (header_h / 2) - 4, arrow_y, ">", text, size, false);
      int first = cogito_weekday(n->datepicker.year, n->datepicker.month, 1);
      int dim = cogito_days_in_month(n->datepicker.year, n->datepicker.month);
      CogitoColor sel = s.has_selection ? s.selection : n->selection_color;
      for (int day = 1; day <= dim; day++) {
        int idx = first + (day - 1);
        int row = idx / 7;
        int col = idx % 7;
        int cx = grid_x + col * cell_w;
        int cy = grid_y + row * cell_h;
        if (day == n->datepicker.day) {
          int r = cell_h / 2;
          if (r < 2) r = 2;
          int inset = 2;
          cogito_draw_rect(cx + inset, cy + inset, cell_w - inset * 2, cell_h - inset * 2, sel, r);
        }
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", day);
        int dw = cogito_text_width_size_node(n, buf, size);
        int dh = cogito_text_height_size(size);
        int dx = cx + (cell_w - dw) / 2;
        int dy = cy + (cell_h - dh) / 2;
        CogitoColor day_text = (day == n->datepicker.day) ? cogito_on_color(sel) : text;
        cogito_draw_text_size_node(n, dx, dy, buf, day_text, size, false);
      }
      break;
    }
