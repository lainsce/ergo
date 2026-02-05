static int cogito_text_width(const char* s) {
  if (!s) return 0;
  if (!cogito_raylib_ready || !cogito_font_ready) return (int)strlen(s) * 8;
  Vector2 size = MeasureTextEx(cogito_font, s, (float)cogito_font_size(), 1.0f);
  return (int)lround(size.x);
}

static int cogito_text_width_size(const char* s, int size) {
  if (!s) return 0;
  if (size <= 0) size = cogito_font_size();
  if (!cogito_raylib_ready || !cogito_font_ready) return (int)strlen(s) * (size / 2);
  Vector2 sz = MeasureTextEx(cogito_font, s, (float)size, 1.0f);
  return (int)lround(sz.x);
}

static int cogito_text_height(void) {
  if (!cogito_raylib_ready || !cogito_font_ready) return 18;
  return cogito_font.baseSize + 2;
}

static int cogito_text_height_size(int size) {
  if (size <= 0) size = cogito_font_size();
  if (!cogito_raylib_ready || !cogito_font_ready) return size + 2;
  return size + 2;
}

static void cogito_node_set_text(CogitoNode* n, ErgoStr* s);

static int cogito_text_width_size_n(const char* s, int len, int size);

static int cogito_text_width_size_font(Font* f, const char* s, int size) {
  if (!s) return 0;
  if (size <= 0) size = cogito_font_size();
  if (!cogito_raylib_ready || !cogito_font_ready || !f) {
    return MeasureText(s, size);
  }
  Vector2 v = MeasureTextEx(*f, s, (float)size, 1.0f);
  return (int)lroundf(v.x);
}

static int cogito_text_width_size_node(const CogitoNode* n, const char* s, int size) {
  Font* f = cogito_font_for_node(n);
  return cogito_text_width_size_font(f, s, size);
}

static int cogito_text_width_size_n_font(Font* f, const char* s, int len, int size) {
  if (!s || len <= 0) return 0;
  if (len > 1023) len = 1023;
  char buf[1024];
  memcpy(buf, s, (size_t)len);
  buf[len] = '\0';
  return cogito_text_width_size_font(f, buf, size);
}

static int cogito_text_width_size_n_node(const CogitoNode* n, const char* s, int len, int size) {
  Font* f = cogito_font_for_node(n);
  return cogito_text_width_size_n_font(f, s, len, size);
}

static void cogito_text_set_from_buf(CogitoNode* n, const char* buf, size_t len) {
  if (!n) return;
  ErgoStr* s = stdr_str_from_slice(buf, len);
  cogito_node_set_text(n, s);
  if (s) ergo_release_val(EV_STR(s));
}

static void cogito_text_delete_range(CogitoNode* n, size_t start, size_t end) {
  if (!n || !n->text) return;
  if (start > end) { size_t tmp = start; start = end; end = tmp; }
  size_t len = n->text->len;
  if (start > len) start = len;
  if (end > len) end = len;
  if (start == end) return;
  size_t new_len = len - (end - start);
  char* buf = (char*)malloc(new_len + 1);
  if (!buf) return;
  if (start > 0) memcpy(buf, n->text->data, start);
  if (end < len) memcpy(buf + start, n->text->data + end, len - end);
  buf[new_len] = '\0';
  cogito_text_set_from_buf(n, buf, new_len);
  free(buf);
}

static void cogito_text_append_codepoint(CogitoNode* n, int cp) {
  if (!n || cp <= 0) return;
  if (n->sel_start != n->sel_end) {
    size_t start = (size_t)n->sel_start;
    size_t end = (size_t)n->sel_end;
    cogito_text_delete_range(n, start, end);
    n->caret = (int)((start < end) ? start : end);
    n->sel_start = n->caret;
    n->sel_end = n->caret;
  }
  char utf8[5] = {0, 0, 0, 0, 0};
  int ulen = 0;
  if (cp <= 0x7F) {
    utf8[0] = (char)cp;
    ulen = 1;
  } else if (cp <= 0x7FF) {
    utf8[0] = (char)(0xC0 | ((cp >> 6) & 0x1F));
    utf8[1] = (char)(0x80 | (cp & 0x3F));
    ulen = 2;
  } else if (cp <= 0xFFFF) {
    utf8[0] = (char)(0xE0 | ((cp >> 12) & 0x0F));
    utf8[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
    utf8[2] = (char)(0x80 | (cp & 0x3F));
    ulen = 3;
  } else {
    utf8[0] = (char)(0xF0 | ((cp >> 18) & 0x07));
    utf8[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    utf8[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    utf8[3] = (char)(0x80 | (cp & 0x3F));
    ulen = 4;
  }
  size_t base_len = n->text ? n->text->len : 0;
  size_t insert_at = (size_t)n->caret;
  if (insert_at > base_len) insert_at = base_len;
  size_t out_len = base_len + (size_t)ulen;
  char* buf = (char*)malloc(out_len + 1);
  if (!buf) return;
  if (insert_at > 0) memcpy(buf, n->text->data, insert_at);
  memcpy(buf + insert_at, utf8, (size_t)ulen);
  if (base_len > insert_at) memcpy(buf + insert_at + (size_t)ulen, n->text->data + insert_at, base_len - insert_at);
  buf[out_len] = '\0';
  cogito_text_set_from_buf(n, buf, out_len);
  free(buf);
  n->caret = (int)(insert_at + (size_t)ulen);
  n->sel_start = n->caret;
  n->sel_end = n->caret;
}

static void cogito_text_backspace(CogitoNode* n) {
  if (!n || !n->text || n->text->len == 0) return;
  if (n->sel_start != n->sel_end) {
    size_t start = (size_t)n->sel_start;
    size_t end = (size_t)n->sel_end;
    cogito_text_delete_range(n, start, end);
    n->caret = (int)((start < end) ? start : end);
    n->sel_start = n->caret;
    n->sel_end = n->caret;
    return;
  }
  size_t i = (size_t)n->caret;
  if (i == 0) return;
  i--;
  while (i > 0 && ((unsigned char)n->text->data[i] & 0xC0) == 0x80) {
    i--;
  }
  size_t start = i;
  size_t end = (size_t)n->caret;
  cogito_text_delete_range(n, start, end);
  n->caret = (int)start;
  n->sel_start = n->caret;
  n->sel_end = n->caret;
}

static int cogito_text_index_from_x(const CogitoNode* n, const char* text, int size, int target_x) {
  if (!text || target_x <= 0) return 0;
  int len = (int)strlen(text);
  int last = 0;
  for (int i = 1; i <= len; i++) {
    int w = cogito_text_width_size_n_node(n, text, i, size);
    if (w >= target_x) return i;
    last = i;
  }
  return last;
}

static int cogito_text_line_index_from_pos(const CogitoNode* n, const char* text, int size, int max_w, int line_y, int line_h, int x, int y) {
  if (!text) return 0;
  if (max_w <= 0) max_w = 1;
  int len = (int)strlen(text);
  int idx = 0;
  int cur_y = line_y;
  while (idx < len) {
    int line_start = idx;
    int line_len = 0;
    int last_space = -1;
    for (int i = idx; i < len; i++) {
      char c = text[i];
      if (c == '\n') {
        line_len = i - idx;
        idx = i + 1;
        break;
      }
      int w = cogito_text_width_size_n_node(n, text + idx, i - idx + 1, size);
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
    if (line_len < 0) line_len = 0;
    if (y >= cur_y && y <= cur_y + line_h) {
      int local_x = x;
      if (local_x < 0) local_x = 0;
      int caret = cogito_text_index_from_x(n, text + line_start, size, local_x);
      if (caret > line_len) caret = line_len;
      return line_start + caret;
    }
    cur_y += line_h;
  }
  return len;
}

static int cogito_text_width_size_n(const char* s, int len, int size) {
  return cogito_text_width_size_n_font(&cogito_font, s, len, size);
}

static bool cogito_is_leap_year(int y) {
  return ((y % 4) == 0 && (y % 100) != 0) || (y % 400) == 0;
}

static int cogito_days_in_month(int y, int m) {
  static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (m < 1) m = 1;
  if (m > 12) m = 12;
  int d = days[m - 1];
  if (m == 2 && cogito_is_leap_year(y)) d = 29;
  return d;
}

static int cogito_weekday(int y, int m, int d) {
  struct tm t;
  memset(&t, 0, sizeof(t));
  t.tm_year = y - 1900;
  t.tm_mon = m - 1;
  t.tm_mday = d;
  mktime(&t);
  return t.tm_wday;
}
