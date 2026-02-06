static void cogito_datepicker_ensure_date(CogitoNode* n) {
  if (!n || n->kind != COGITO_DATEPICKER) return;
  if (n->date_year <= 0 || n->date_month <= 0) {
    time_t now = time(NULL);
    struct tm* lt = localtime(&now);
    if (lt) {
      n->date_year = lt->tm_year + 1900;
      n->date_month = lt->tm_mon + 1;
      n->date_day = lt->tm_mday;
    } else {
      n->date_year = 2025;
      n->date_month = 1;
      n->date_day = 1;
    }
  }
  if (n->date_month < 1) n->date_month = 1;
  if (n->date_month > 12) n->date_month = 12;
  int dim = cogito_days_in_month(n->date_year, n->date_month);
  if (n->date_day < 1) n->date_day = 1;
  if (n->date_day > dim) n->date_day = dim;
}

static void cogito_datepicker_shift_month(CogitoNode* n, int delta) {
  if (!n) return;
  cogito_datepicker_ensure_date(n);
  int m = n->date_month + delta;
  int y = n->date_year;
  while (m < 1) { m += 12; y -= 1; }
  while (m > 12) { m -= 12; y += 1; }
  n->date_month = m;
  n->date_year = y;
  int dim = cogito_days_in_month(n->date_year, n->date_month);
  if (n->date_day > dim) n->date_day = dim;
}

static void cogito_datepicker_layout(CogitoNode* n, int* out_pad_l, int* out_pad_t, int* out_pad_r, int* out_pad_b,
                                     int* out_header_h, int* out_grid_x, int* out_grid_y, int* out_cell_w, int* out_cell_h) {
  CogitoStyle s = cogito_theme_resolve_node(n);
  int pad_l = s.has_padding_left ? s.padding_left : 10;
  int pad_t = s.has_padding_top ? s.padding_top : 10;
  int pad_r = s.has_padding_right ? s.padding_right : pad_l;
  int pad_b = s.has_padding_bottom ? s.padding_bottom : pad_t;
  int size = cogito_node_font_size(n);
  int th = cogito_text_height_size(size);
  int header_h = th + 8;
  int grid_x = n->x + pad_l;
  int grid_y = n->y + pad_t + header_h + 6;
  int grid_w = n->w - pad_l - pad_r;
  int grid_h = n->h - pad_t - pad_b - header_h - 6;
  int cell_w = grid_w / 7;
  int cell_h = grid_h / 6;
  if (cell_w < 1) cell_w = 1;
  if (cell_h < 1) cell_h = 1;
  if (out_pad_l) *out_pad_l = pad_l;
  if (out_pad_t) *out_pad_t = pad_t;
  if (out_pad_r) *out_pad_r = pad_r;
  if (out_pad_b) *out_pad_b = pad_b;
  if (out_header_h) *out_header_h = header_h;
  if (out_grid_x) *out_grid_x = grid_x;
  if (out_grid_y) *out_grid_y = grid_y;
  if (out_cell_w) *out_cell_w = cell_w;
  if (out_cell_h) *out_cell_h = cell_h;
}

static int cogito_datepicker_hit_day(CogitoNode* n, int mx, int my) {
  int grid_x = 0, grid_y = 0, cell_w = 0, cell_h = 0;
  cogito_datepicker_layout(n, NULL, NULL, NULL, NULL, NULL, &grid_x, &grid_y, &cell_w, &cell_h);
  int grid_w = cell_w * 7;
  int grid_h = cell_h * 6;
  if (!cogito_hit_rect(mx, my, grid_x, grid_y, grid_w, grid_h)) return -1;
  int col = (mx - grid_x) / cell_w;
  int row = (my - grid_y) / cell_h;
  if (col < 0 || col > 6 || row < 0 || row > 5) return -1;
  int first = cogito_weekday(n->date_year, n->date_month, 1);
  int idx = row * 7 + col;
  int day = idx - first + 1;
  int dim = cogito_days_in_month(n->date_year, n->date_month);
  if (day < 1 || day > dim) return -1;
  return day;
}

static int cogito_datepicker_hit_header(CogitoNode* n, int mx, int my) {
  int pad_l = 0, pad_t = 0, pad_r = 0, header_h = 0;
  cogito_datepicker_layout(n, &pad_l, &pad_t, &pad_r, NULL, &header_h, NULL, NULL, NULL, NULL);
  int left_x = n->x + pad_l;
  int right_x = n->x + n->w - pad_r - header_h;
  int y = n->y + pad_t;
  if (cogito_hit_rect(mx, my, left_x, y, header_h, header_h)) return -1;
  if (cogito_hit_rect(mx, my, right_x, y, header_h, header_h)) return 1;
  return 0;
}