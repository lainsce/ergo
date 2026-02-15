static void cogito_timepicker_ensure_time(CogitoNode* n) {
  if (!n || n->kind != COGITO_TIMEPICKER) return;
  // Default: if all zero, set to current time
  if (n->time_hour == 0 && n->time_minute == 0 && !n->time_ampm) {
    time_t now = time(NULL);
    struct tm* lt = localtime(&now);
    if (lt) {
      n->time_hour = lt->tm_hour;
      n->time_minute = lt->tm_min;
      n->time_ampm = (n->time_hour >= 12);
    }
  }
}

static bool cogito_timepicker_hit(CogitoNode* n, int mx, int my, bool clicked) {
  if (!n || n->kind != COGITO_TIMEPICKER || !clicked) return false;
  CogitoStyle s = cogito_theme_resolve_node(n);
  int pad_l = s.has_padding_left ? s.padding_left : 12;
  int pad_t = s.has_padding_top ? s.padding_top : 12;
  (void)pad_l;
  int size = cogito_node_font_size(n);
  int th = cogito_text_height_size(size);
  int big_size = size + 8;
  int big_th = cogito_text_height_size(big_size);

  int header_y = n->y + pad_t;
  int display_y = header_y + th + 12;
  int col_w = 64;
  int colon_w = 20;
  int total_w = col_w * 2 + colon_w;
  int start_x = n->x + (n->w - total_w) / 2;
  int box_h = big_th + 20;
  int btn_size = 28;

  int hour_x = start_x;
  int min_x = start_x + col_w + colon_w;

  // Hour up button
  {
    int bx = hour_x + (col_w - btn_size) / 2;
    int by = display_y - btn_size - 4;
    if (cogito_hit_rect(mx, my, bx, by, btn_size, btn_size)) {
      n->time_hour = (n->time_hour + 1) % 24;
      n->time_ampm = (n->time_hour >= 12);
      return true;
    }
  }
  // Hour down button
  {
    int bx = hour_x + (col_w - btn_size) / 2;
    int by = display_y + box_h + 4;
    if (cogito_hit_rect(mx, my, bx, by, btn_size, btn_size)) {
      n->time_hour = (n->time_hour + 23) % 24;
      n->time_ampm = (n->time_hour >= 12);
      return true;
    }
  }
  // Minute up button
  {
    int bx = min_x + (col_w - btn_size) / 2;
    int by = display_y - btn_size - 4;
    if (cogito_hit_rect(mx, my, bx, by, btn_size, btn_size)) {
      n->time_minute = (n->time_minute + 1) % 60;
      return true;
    }
  }
  // Minute down button
  {
    int bx = min_x + (col_w - btn_size) / 2;
    int by = display_y + box_h + 4;
    if (cogito_hit_rect(mx, my, bx, by, btn_size, btn_size)) {
      n->time_minute = (n->time_minute + 59) % 60;
      return true;
    }
  }

  // AM/PM toggle
  int ampm_y = display_y + box_h + btn_size + 16;
  int ampm_w = 48;
  int ampm_h_box = th + 12;
  int ampm_total = ampm_w * 2;
  int ampm_x = n->x + (n->w - ampm_total) / 2;

  if (cogito_hit_rect(mx, my, ampm_x, ampm_y, ampm_w, ampm_h_box)) {
    // Clicked AM
    if (n->time_ampm) {
      n->time_hour -= 12;
      if (n->time_hour < 0) n->time_hour += 24;
      n->time_ampm = false;
      return true;
    }
  } else if (cogito_hit_rect(mx, my, ampm_x + ampm_w, ampm_y, ampm_w, ampm_h_box)) {
    // Clicked PM
    if (!n->time_ampm) {
      n->time_hour += 12;
      if (n->time_hour >= 24) n->time_hour -= 24;
      n->time_ampm = true;
      return true;
    }
  }
  return false;
}
