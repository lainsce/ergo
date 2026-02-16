case COGITO_TIMEPICKER: {
  cogito_timepicker_ensure_time(n);
  bool hover = false, active = false;
  cogito_node_hover_state(n, &hover, &active);
  CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);

  CogitoColor bg = s.has_bg ? s.bg : n->bg;
  CogitoColor text_color = s.has_text ? s.text : n->text_color;
  CogitoColor border_color = s.has_border ? s.border : n->border_color;
  int border_w = s.has_border_width ? s.border_width : (n->border_width_set ? n->border_width : 0);
  int radius = s.has_radius ? s.radius : (n->radius_set ? n->border_radius : 18);
  CogitoColor sel = s.has_selection ? s.selection : (CogitoColor){114, 222, 194, 255};

  // Background
  cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);
  if (border_w > 0) {
    cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, border_color, radius, border_w);
  }

  int pad_l = s.has_padding_left ? s.padding_left : 12;
  int pad_t = s.has_padding_top ? s.padding_top : 12;
  int size = cogito_node_font_size(n);
  int th = cogito_text_height_size(size);
  int big_size = size + 8;
  int big_th = cogito_text_height_size(big_size);

  int mx = 0, my = 0;
  bool down = false;
  if (cogito_backend && cogito_backend->get_mouse_position)
    cogito_backend->get_mouse_position(&mx, &my);
  if (cogito_backend && cogito_backend->is_mouse_button_down)
    down = cogito_backend->is_mouse_button_down(0);

  // Header: "Select time"
  int header_y = n->y + pad_t;
  cogito_draw_text_size_node(n, n->x + pad_l, header_y, "Select time", text_color, size, false);

  // Time display area
  int display_y = header_y + th + 12 + 32;
  int col_w = 64;
  int colon_w = 20;
  int total_w = col_w * 2 + colon_w;
  int start_x = n->x + (n->w - total_w) / 2;

  // Hour box
  int hour_x = start_x;
  int box_h = big_th + 20;
  int box_r = 8;
  CogitoColor hour_bg = sel;
  hour_bg.a = 40;
  cogito_draw_rect(hour_x, display_y, col_w, box_h, hour_bg, box_r);
  {
    int display_hour = n->timepicker.hour;
    if (n->timepicker.ampm) {
      display_hour = n->timepicker.hour % 12;
      if (display_hour == 0) display_hour = 12;
    }
    char hbuf[8];
    snprintf(hbuf, sizeof(hbuf), "%02d", display_hour);
    int hw = cogito_text_width_size_node(n, hbuf, big_size);
    int hx = hour_x + (col_w - hw) / 2;
    int hy = display_y + (box_h - big_th) / 2;
    cogito_draw_text_size_node(n, hx, hy, hbuf, text_color, big_size, true);
  }

  // Colon
  {
    int cx = start_x + col_w;
    int cw = cogito_text_width_size_node(n, ":", big_size);
    int cy_colon = display_y + (box_h - big_th) / 2;
    cogito_draw_text_size_node(n, cx + (colon_w - cw) / 2, cy_colon, ":", text_color, big_size, true);
  }

  // Minute box
  int min_x = start_x + col_w + colon_w;
  CogitoColor min_bg = sel;
  min_bg.a = 40;
  cogito_draw_rect(min_x, display_y, col_w, box_h, min_bg, box_r);
  {
    char mbuf[8];
    snprintf(mbuf, sizeof(mbuf), "%02d", n->timepicker.minute);
    int mw = cogito_text_width_size_node(n, mbuf, big_size);
    int mmx = min_x + (col_w - mw) / 2;
    int mmy = display_y + (box_h - big_th) / 2;
    cogito_draw_text_size_node(n, mmx, mmy, mbuf, text_color, big_size, true);
  }

  // Up/Down buttons for hour
  int btn_size = 28;
  int btn_r = btn_size / 2;
  CogitoColor btn_bg = text_color;
  {
    // Hour up
    int bx = hour_x + (col_w - btn_size) / 2;
    int by_up = display_y - btn_size - 4;
    CogitoColor up_bg = btn_bg;
    up_bg.a = cogito_hit_rect(mx, my, bx, by_up, btn_size, btn_size) ? (down ? 80 : 50) : 30;
    cogito_draw_rect(bx, by_up, btn_size, btn_size, up_bg, btn_r);
    cogito_draw_icon_fallback(n, "chevron.up", bx + 6, by_up + 6, btn_size - 12, text_color);

    // Hour down
    int by_dn = display_y + box_h + 4;
    CogitoColor dn_bg = btn_bg;
    dn_bg.a = cogito_hit_rect(mx, my, bx, by_dn, btn_size, btn_size) ? (down ? 80 : 50) : 30;
    cogito_draw_rect(bx, by_dn, btn_size, btn_size, dn_bg, btn_r);
    cogito_draw_icon_fallback(n, "chevron.down", bx + 6, by_dn + 6, btn_size - 12, text_color);
  }

  // Up/Down buttons for minute
  {
    int bx = min_x + (col_w - btn_size) / 2;
    int by_up = display_y - btn_size - 4;
    CogitoColor up_bg = btn_bg;
    up_bg.a = cogito_hit_rect(mx, my, bx, by_up, btn_size, btn_size) ? (down ? 80 : 50) : 30;
    cogito_draw_rect(bx, by_up, btn_size, btn_size, up_bg, btn_r);
    cogito_draw_icon_fallback(n, "chevron.up", bx + 6, by_up + 6, btn_size - 12, text_color);

    int by_dn = display_y + box_h + 4;
    CogitoColor dn_bg = btn_bg;
    dn_bg.a = cogito_hit_rect(mx, my, bx, by_dn, btn_size, btn_size) ? (down ? 80 : 50) : 30;
    cogito_draw_rect(bx, by_dn, btn_size, btn_size, dn_bg, btn_r);
    cogito_draw_icon_fallback(n, "chevron.down", bx + 6, by_dn + 6, btn_size - 12, text_color);
  }

  // AM/PM toggle
  int ampm_y = display_y + box_h + btn_size + 16;
  int ampm_w = 48;
  int ampm_h_box = th + 12;
  int ampm_total = ampm_w * 2;
  int ampm_x = n->x + (n->w - ampm_total) / 2;
  int ampm_r = 6;

  // AM button
  {
    bool is_am = !n->timepicker.ampm;
    CogitoColor am_bg = is_am ? sel : bg;
    if (!is_am) am_bg.a = 60;
    cogito_draw_rect(ampm_x, ampm_y, ampm_w, ampm_h_box, am_bg, ampm_r);
    CogitoColor am_text = is_am ? cogito_on_color(sel) : text_color;
    int aw = cogito_text_width_size_node(n, "AM", size);
    cogito_draw_text_size_node(n, ampm_x + (ampm_w - aw) / 2,
                               ampm_y + (ampm_h_box - th) / 2, "AM", am_text, size, true);
  }
  // PM button
  {
    bool is_pm = n->timepicker.ampm;
    CogitoColor pm_bg = is_pm ? sel : bg;
    if (!is_pm) pm_bg.a = 60;
    cogito_draw_rect(ampm_x + ampm_w, ampm_y, ampm_w, ampm_h_box, pm_bg, ampm_r);
    CogitoColor pm_text = is_pm ? cogito_on_color(sel) : text_color;
    int pw = cogito_text_width_size_node(n, "PM", size);
    cogito_draw_text_size_node(n, ampm_x + ampm_w + (ampm_w - pw) / 2,
                               ampm_y + (ampm_h_box - th) / 2, "PM", pm_text, size, true);
  }

  break;
}
