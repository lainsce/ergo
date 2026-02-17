// Timepicker hit testing - handles clicks on hour/minute up/down buttons and AM/PM toggle
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
  int display_y = header_y + th + 44;  // th + 12 + 32 (extra spacing in draw code)
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
      n->timepicker.hour = (n->timepicker.hour + 1) % 24;
      n->timepicker.ampm = (n->timepicker.hour >= 12);
      return true;
    }
  }
  // Hour down button
  {
    int bx = hour_x + (col_w - btn_size) / 2;
    int by = display_y + box_h + 4;
    if (cogito_hit_rect(mx, my, bx, by, btn_size, btn_size)) {
      n->timepicker.hour = (n->timepicker.hour + 23) % 24;
      n->timepicker.ampm = (n->timepicker.hour >= 12);
      return true;
    }
  }
  // Minute up button
  {
    int bx = min_x + (col_w - btn_size) / 2;
    int by = display_y - btn_size - 4;
    if (cogito_hit_rect(mx, my, bx, by, btn_size, btn_size)) {
      n->timepicker.minute = (n->timepicker.minute + 1) % 60;
      return true;
    }
  }
  // Minute down button
  {
    int bx = min_x + (col_w - btn_size) / 2;
    int by = display_y + box_h + 4;
    if (cogito_hit_rect(mx, my, bx, by, btn_size, btn_size)) {
      n->timepicker.minute = (n->timepicker.minute + 59) % 60;
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
    if (n->timepicker.ampm) {
      n->timepicker.hour -= 12;
      if (n->timepicker.hour < 0) n->timepicker.hour += 24;
      n->timepicker.ampm = false;
      return true;
    }
  } else if (cogito_hit_rect(mx, my, ampm_x + ampm_w, ampm_y, ampm_w, ampm_h_box)) {
    // Clicked PM
    if (!n->timepicker.ampm) {
      n->timepicker.hour += 12;
      if (n->timepicker.hour >= 24) n->timepicker.hour -= 24;
      n->timepicker.ampm = true;
      return true;
    }
  }
  return false;
}

static CogitoNode* cogito_timepicker_new_obj(void) {
  CogitoNode* n = cogito_node_new(COGITO_TIMEPICKER);
  cogito_timepicker_ensure_time(n);
  return n;
}

static ErgoVal cogito_timepicker_new(void) {
  return EV_OBJ(cogito_timepicker_new_obj());
}

static void cogito_timepicker_on_change(ErgoVal nodev, ErgoVal handler) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.timepicker_on_change expects timepicker");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->kind != COGITO_TIMEPICKER) ergo_trap("cogito.timepicker_on_change expects timepicker");
  if (n->on_change) ergo_release_val(EV_FN(n->on_change));
  if (handler.tag == EVT_FN) {
    n->on_change = (ErgoFn*)handler.as.p;
    ergo_retain_val(EV_FN(n->on_change));
  } else {
    n->on_change = NULL;
  }
}

static ErgoVal cogito_timepicker_get_hour(ErgoVal nodev) {
  if (nodev.tag != EVT_OBJ) return EV_INT(0);
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  return EV_INT(n->timepicker.hour);
}

static ErgoVal cogito_timepicker_get_minute(ErgoVal nodev) {
  if (nodev.tag != EVT_OBJ) return EV_INT(0);
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  return EV_INT(n->timepicker.minute);
}

static void cogito_timepicker_set_time(ErgoVal nodev, ErgoVal hourv, ErgoVal minv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.timepicker_set_time expects timepicker");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  int h = 0, m = 0;
  if (hourv.tag == EVT_INT) h = (int)hourv.as.i;
  else if (hourv.tag == EVT_FLOAT) h = (int)hourv.as.f;
  if (minv.tag == EVT_INT) m = (int)minv.as.i;
  else if (minv.tag == EVT_FLOAT) m = (int)minv.as.f;
  if (h < 0) h = 0; if (h > 23) h = 23;
  if (m < 0) m = 0; if (m > 59) m = 59;
  n->timepicker.hour = h;
  n->timepicker.minute = m;
  n->timepicker.ampm = (h >= 12);
  cogito_window_relayout(cogito_node_window(n));
}

static void cogito_timepicker_click(CogitoNode* n, int mx, int my) {
  if (cogito_timepicker_hit(n, mx, my, true)) {
    cogito_invoke_change(n);
  }
}
