static void cogito_menu_open_for(CogitoNode* n) {
  if (!n || n->menu_len == 0) return;
  int font_size = cogito_font_size();
  if (cogito_theme.menu.has_font_size && cogito_theme.menu.font_size > 0) {
    font_size = cogito_theme.menu.font_size;
  }
  CogitoStyle* item_style = &cogito_theme.menu_item;
  int pad_x = item_style->has_padding_left ? item_style->padding_left
              : (cogito_theme.menu.has_padding_left ? cogito_theme.menu.padding_left : 10);
  int pad_y = item_style->has_padding_top ? item_style->padding_top
              : (cogito_theme.menu.has_padding_top ? cogito_theme.menu.padding_top : 6);
  int item_h = font_size + pad_y * 2;
  int min_h = item_style->has_min_h ? item_style->min_h
              : (cogito_theme.menu.has_min_h ? cogito_theme.menu.min_h : 0);
  if (min_h > item_h) item_h = min_h;
  int menu_pad_top = 8;
  int menu_pad_bottom = 8;
  int max_w = 0;
  for (size_t i = 0; i < n->menu_len; i++) {
    const char* text = n->menu_labels[i] ? n->menu_labels[i]->data : "";
    int w = cogito_text_width_size(text, font_size);
    if (w > max_w) max_w = w;
  }
  int w = max_w + pad_x * 2;
  int h = (int)n->menu_len * item_h + menu_pad_top + menu_pad_bottom;
  cogito_menu.open = true;
  cogito_menu.owner = n;
  cogito_menu.item_h = item_h;
  cogito_menu.pad_x = pad_x;
  cogito_menu.pad_y = pad_y;
  cogito_menu.pad_top = menu_pad_top;
  cogito_menu.pad_bottom = menu_pad_bottom;
  cogito_menu.w = w;
  cogito_menu.h = h;
  int x = n->x;
  int y = n->y + n->h + 4;
  int win_w = GetScreenWidth();
  int win_h = GetScreenHeight();
  if (w > win_w - 8) w = win_w - 8;
  if (h > win_h - 8) h = win_h - 8;
  if (x + w > win_w - 4) x = win_w - 4 - w;
  if (x < 4) x = 4;
  if (y + h > win_h - 4) {
    int above = n->y - h - 4;
    if (above >= 4) y = above;
    else y = win_h - 4 - h;
  }
  if (y < 4) y = 4;
  cogito_menu.w = w;
  cogito_menu.h = h;
  cogito_menu.x = x;
  cogito_menu.y = y;
}

static int cogito_menu_hit(const CogitoMenu* menu, int x, int y) {
  if (!menu || !menu->open) return -1;
  if (x < menu->x || y < menu->y || x > menu->x + menu->w || y > menu->y + menu->h) return -1;
  int rel_y = y - (menu->y + menu->pad_top);
  if (rel_y < 0) return -1;
  int idx = rel_y / menu->item_h;
  if (idx < 0) return -1;
  if (!menu->owner) return -1;
  if ((size_t)idx >= menu->owner->menu_len) return -1;
  return idx;
}

static float cogito_roundness_for(int w, int h, int radius) {
  if (radius <= 0) return 0.0f;
  float min = (float)(w < h ? w : h);
  if (min <= 0.0f) return 0.0f;
  float roundness = (float)radius / (min * 0.5f);
  if (roundness > 1.0f) roundness = 1.0f;
  if (roundness < 0.0f) roundness = 0.0f;
  return roundness;
}

static void cogito_draw_rect(int x, int y, int w, int h, Color bg, int radius) {
  if (w <= 0 || h <= 0) return;
  Rectangle r = { (float)x, (float)y, (float)w, (float)h };
  float roundness = cogito_roundness_for(w, h, radius);
  if (roundness > 0.0f) DrawRectangleRounded(r, roundness, 8, bg);
  else DrawRectangleRec(r, bg);
}

static void cogito_draw_rect_lines(int x, int y, int w, int h, Color border, int radius, int thickness) {
  if (w <= 0 || h <= 0 || thickness <= 0) return;
  Rectangle r = { (float)x, (float)y, (float)w, (float)h };
  float roundness = cogito_roundness_for(w, h, radius);
  if (roundness > 0.0f) {
    DrawRectangleRoundedLines(r, roundness, 8, border);
  } else {
    DrawRectangleLinesEx(r, (float)thickness, border);
  }
}

static Color cogito_blend(Color base, Color over) {
  float a = over.a / 255.0f;
  float ia = 1.0f - a;
  Color out;
  out.r = (unsigned char)(base.r * ia + over.r * a);
  out.g = (unsigned char)(base.g * ia + over.g * a);
  out.b = (unsigned char)(base.b * ia + over.b * a);
  out.a = base.a;
  return out;
}

static void cogito_draw_text(int x, int y, const char* text, Color color) {
  if (!text) return;
  if (cogito_font_ready) {
    DrawTextEx(cogito_font, text, (Vector2){(float)x, (float)y}, (float)cogito_font_size(), 1.0f, color);
  } else {
    DrawText(text, x, y, cogito_font_size(), color);
  }
}

static void cogito_draw_text_size_font(Font* f, int x, int y, const char* text, Color color, int size, bool bold) {
  if (!text) return;
  if (size <= 0) size = cogito_font_size();
  if (cogito_font_ready && f) {
    DrawTextEx(*f, text, (Vector2){(float)x, (float)y}, (float)size, 1.0f, color);
    if (bold) {
      DrawTextEx(*f, text, (Vector2){(float)x + 1.0f, (float)y}, (float)size, 1.0f, color);
    }
  } else {
    DrawText(text, x, y, size, color);
    if (bold) DrawText(text, x + 1, y, size, color);
  }
}

static void cogito_draw_text_size(int x, int y, const char* text, Color color, int size, bool bold) {
  cogito_draw_text_size_font(&cogito_font, x, y, text, color, size, bold);
}

