// Cogito Text Rendering - Phase 1 Stub
// Full SDL_ttf implementation in Phase 2

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MULTIPLE_MASTERS_H

static bool cogito_node_bold(const CogitoNode* n);

static int cogito_font_parse_int_env(const char* name, int fallback) {
  if (!name) return fallback;
  const char* env = getenv(name);
  if (!env || !env[0]) return fallback;
  char* end = NULL;
  long v = strtol(env, &end, 10);
  if (end == env || *end != '\0') return fallback;
  if (v < 1) return fallback;
  return (int)v;
}

static int cogito_font_weight_normal(void) {
  return cogito_font_parse_int_env("COGITO_FONT_WEIGHT", 400);
}

static int cogito_font_weight_bold(void) {
  return cogito_font_parse_int_env("COGITO_FONT_BOLD_WEIGHT", 700);
}

static int cogito_default_codepoints(int* out, int cap) {
  int count = 0;
  if (!out || cap <= 0) return 0;
  for (int cp = 32; cp <= 126 && count < cap; cp++) out[count++] = cp;
  for (int cp = 160; cp <= 255 && count < cap; cp++) out[count++] = cp;
  return count;
}

static bool cogito_freetype_set_weight(FT_Library lib, FT_Face face, int weight, bool* out_applied) {
  if (!face || weight <= 0) return false;
  if (!(face->face_flags & FT_FACE_FLAG_MULTIPLE_MASTERS)) return false;
  FT_MM_Var* mm = NULL;
  if (FT_Get_MM_Var(face, &mm) != 0 || !mm) return false;
  int axis_index = -1;
  for (FT_UInt i = 0; i < mm->num_axis; i++) {
    FT_Var_Axis* axis = &mm->axis[i];
    if (axis->tag == FT_MAKE_TAG('w', 'g', 'h', 't') || axis->tag == FT_MAKE_TAG('W', 'G', 'H', 'T')) {
      axis_index = (int)i;
      break;
    }
  }
  if (axis_index < 0) {
    FT_Done_MM_Var(lib, mm);
    return false;
  }
  FT_Fixed* coords = (FT_Fixed*)malloc(sizeof(FT_Fixed) * mm->num_axis);
  if (!coords) {
    FT_Done_MM_Var(lib, mm);
    return false;
  }
  for (FT_UInt i = 0; i < mm->num_axis; i++) coords[i] = mm->axis[i].def;
  FT_Fixed min = mm->axis[axis_index].minimum;
  FT_Fixed max = mm->axis[axis_index].maximum;
  FT_Fixed desired = (FT_Fixed)((long)weight * 65536);
  if (desired < min) desired = min;
  if (desired > max) desired = max;
  coords[axis_index] = desired;
  if (out_applied) *out_applied = true;
  FT_Set_Var_Design_Coordinates(face, mm->num_axis, coords);
  free(coords);
  FT_Done_MM_Var(lib, mm);
  return true;
}

// Stub: Font loading via backend
static bool cogito_load_font_file_ex(CogitoFont** out, const char* path, int size, int weight, bool* out_weight_applied) {
  if (out_weight_applied) *out_weight_applied = false;
  if (!out || !path || !path[0]) return false;
  
  // Phase 2: Use backend font loading
  if (cogito_backend && cogito_backend->font_load) {
    *out = cogito_backend->font_load(path, size);
    return *out != NULL;
  }
  
  return false;
}

static bool cogito_load_font_file(CogitoFont** out, const char* path, int size, int weight) {
  return cogito_load_font_file_ex(out, path, size, weight, NULL);
}

static bool cogito_load_font_file_tabular(CogitoFont** out, const char* path, int size, int weight) {
  // Phase 2: Implement tabular digit font loading
  return cogito_load_font_file(out, path, size, weight);
}

static CogitoFont* cogito_font_cache_get(CogitoFontCacheEntry* cache, int cap, const char* path, int size, int weight);

static CogitoFont* cogito_font_resolve_size(CogitoFont* f, int size) {
  if (!f) return NULL;
  if (size <= 0) size = cogito_font_size();

  if (f == cogito_font) {
    if (!cogito_font_from_env || !cogito_font_path_active || !cogito_font_path_active[0]) return f;
    if (cogito_font_loaded_size == size) return f;
    CogitoFont* cached = cogito_font_cache_get(
      cogito_font_cache,
      (int)(sizeof(cogito_font_cache) / sizeof(cogito_font_cache[0])),
      cogito_font_path_active,
      size,
      cogito_font_weight_normal()
    );
    return cached ? cached : f;
  }

  if (f == cogito_font_bold) {
    if (!cogito_font_bold_from_env || !cogito_font_bold_path_active || !cogito_font_bold_path_active[0]) return f;
    if (cogito_font_bold_loaded_size == size) return f;
    CogitoFont* cached = cogito_font_cache_get(
      cogito_font_bold_cache,
      (int)(sizeof(cogito_font_bold_cache) / sizeof(cogito_font_bold_cache[0])),
      cogito_font_bold_path_active,
      size,
      cogito_font_weight_bold()
    );
    return cached ? cached : f;
  }

  return f;
}

// Stub: Font cache using backend fonts
static CogitoFont* cogito_font_cache_get(CogitoFontCacheEntry* cache, int cap, const char* path, int size, int weight) {
  if (!cache || cap <= 0 || !path || !path[0] || size <= 0) return NULL;
  
  for (int i = 0; i < cap; i++) {
    if (cache[i].ready && cache[i].size == size) return cache[i].font;
  }
  
  int slot = -1;
  for (int i = 0; i < cap; i++) {
    if (!cache[i].ready) { slot = i; break; }
  }
  if (slot < 0) slot = 0;
  
  if (cache[slot].ready) {
    if (cogito_backend && cogito_backend->font_unload) {
      cogito_backend->font_unload(cache[slot].font);
    }
    cache[slot].ready = false;
    cache[slot].size = 0;
  }
  
  if (!cogito_load_font_file_ex(&cache[slot].font, path, size, weight, NULL)) {
    return NULL;
  }
  
  cache[slot].size = size;
  cache[slot].ready = true;
  return cache[slot].font;
}

static CogitoFont* cogito_font_tabular_cache_get(const char* path, int size, int weight) {
  if (!path || !path[0] || size <= 0) return NULL;
  int cap = (int)(sizeof(cogito_font_tabular_cache) / sizeof(cogito_font_tabular_cache[0]));
  
  for (int i = 0; i < cap; i++) {
    if (cogito_font_tabular_cache[i].ready && cogito_font_tabular_cache[i].size == size) {
      return cogito_font_tabular_cache[i].font;
    }
  }
  
  int slot = -1;
  for (int i = 0; i < cap; i++) {
    if (!cogito_font_tabular_cache[i].ready) { slot = i; break; }
  }
  if (slot < 0) slot = 0;
  
  if (cogito_font_tabular_cache[slot].ready) {
    if (cogito_backend && cogito_backend->font_unload) {
      cogito_backend->font_unload(cogito_font_tabular_cache[slot].font);
    }
    cogito_font_tabular_cache[slot].ready = false;
    cogito_font_tabular_cache[slot].size = 0;
  }
  
  if (!cogito_load_font_file_tabular(&cogito_font_tabular_cache[slot].font, path, size, weight)) {
    return NULL;
  }
  
  cogito_font_tabular_cache[slot].size = size;
  cogito_font_tabular_cache[slot].ready = true;
  return cogito_font_tabular_cache[slot].font;
}

static CogitoFont* cogito_font_for_size(CogitoFont* f, int size) {
  if (!f) return f;
  if (size <= 0) size = cogito_font_size();
  
  // Phase 2: Implement font scaling
  // For now, return the same font
  return f;
}

static CogitoFont* cogito_font_tabular_for(CogitoFont* f, int size) {
  if (!f) return f;
  if (size <= 0) size = cogito_font_size();
  
  if (f == cogito_font) {
    if (!cogito_font_from_env || !cogito_font_path_active) return f;
    CogitoFont* cached = cogito_font_tabular_cache_get(cogito_font_path_active, size, cogito_font_weight_normal());
    return cached ? cached : f;
  }
  if (f == cogito_font_bold) {
    if (!cogito_font_bold_from_env || !cogito_font_bold_path_active) return f;
    CogitoFont* cached = cogito_font_tabular_cache_get(cogito_font_bold_path_active, size, cogito_font_weight_bold());
    return cached ? cached : f;
  }
  return f;
}

static void cogito_font_cache_clear(void) {
  for (size_t i = 0; i < sizeof(cogito_font_cache) / sizeof(cogito_font_cache[0]); i++) {
    if (cogito_font_cache[i].ready) {
      if (cogito_backend && cogito_backend->font_unload) {
        cogito_backend->font_unload(cogito_font_cache[i].font);
      }
      cogito_font_cache[i].ready = false;
      cogito_font_cache[i].size = 0;
    }
  }
  for (size_t i = 0; i < sizeof(cogito_font_bold_cache) / sizeof(cogito_font_bold_cache[0]); i++) {
    if (cogito_font_bold_cache[i].ready) {
      if (cogito_backend && cogito_backend->font_unload) {
        cogito_backend->font_unload(cogito_font_bold_cache[i].font);
      }
      cogito_font_bold_cache[i].ready = false;
      cogito_font_bold_cache[i].size = 0;
    }
  }
  for (size_t i = 0; i < sizeof(cogito_font_tabular_cache) / sizeof(cogito_font_tabular_cache[0]); i++) {
    if (cogito_font_tabular_cache[i].ready) {
      if (cogito_backend && cogito_backend->font_unload) {
        cogito_backend->font_unload(cogito_font_tabular_cache[i].font);
      }
      cogito_font_tabular_cache[i].ready = false;
      cogito_font_tabular_cache[i].size = 0;
    }
  }
}

// Stub: Text measurement using backend
static int cogito_text_width(const char* s) {
  if (!s) return 0;
  if (!cogito_backend_ready || !cogito_font_ready) return (int)strlen(s) * 8;
  
  if (cogito_backend && cogito_backend->text_measure_width) {
    CogitoFont* f = cogito_font_resolve_size(cogito_font, cogito_font_size());
    return cogito_backend->text_measure_width(f ? f : cogito_font, s, cogito_font_size());
  }
  
  return (int)strlen(s) * 8;  // Fallback estimate
}

static int cogito_text_width_size(const char* s, int size) {
  if (!s) return 0;
  if (size <= 0) size = cogito_font_size();
  if (!cogito_backend_ready || !cogito_font_ready) return (int)strlen(s) * (size / 2);
  
  if (cogito_backend && cogito_backend->text_measure_width) {
    CogitoFont* f = cogito_font_resolve_size(cogito_font, size);
    return cogito_backend->text_measure_width(f ? f : cogito_font, s, size);
  }
  
  return (int)strlen(s) * (size / 2);
}

static int cogito_text_height(void) {
  if (!cogito_backend_ready || !cogito_font_ready) return 18;
  
  if (cogito_backend && cogito_backend->text_measure_height) {
    int size = cogito_font_size();
    CogitoFont* f = cogito_font_resolve_size(cogito_font, size);
    return cogito_backend->text_measure_height(f ? f : cogito_font, size);
  }
  
  return cogito_font_size() + 2;
}

static int cogito_text_height_size(int size) {
  if (size <= 0) size = cogito_font_size();
  if (!cogito_backend_ready || !cogito_font_ready) return size + 2;
  
  if (cogito_backend && cogito_backend->text_measure_height) {
    CogitoFont* f = cogito_font_resolve_size(cogito_font, size);
    return cogito_backend->text_measure_height(f ? f : cogito_font, size);
  }
  
  return size + 2;
}

static void cogito_node_set_text(CogitoNode* n, ErgoStr* s);

static int cogito_text_width_size_n(const char* s, int len, int size);

static int cogito_text_width_size_font_spaced(CogitoFont* f, const char* s, int size, float spacing) {
  (void)spacing;  // Phase 2: Implement letter spacing
  if (!s) return 0;
  if (size <= 0) size = cogito_font_size();
  f = cogito_font_resolve_size(f, size);
  if (!cogito_backend_ready || !cogito_font_ready || !f) {
    return (int)strlen(s) * (size / 2);
  }
  
  if (cogito_backend && cogito_backend->text_measure_width) {
    return cogito_backend->text_measure_width(f, s, size);
  }
  
  return (int)strlen(s) * (size / 2);
}

static int cogito_text_width_size_font(CogitoFont* f, const char* s, int size) {
  return cogito_text_width_size_font_spaced(f, s, size, 0.0f);
}

static int cogito_text_width_size_node(const CogitoNode* n, const char* s, int size) {
  CogitoFont* f = cogito_font_for_node(n);
  if (n && cogito_node_bold(n) && f == cogito_font && cogito_font_bold_ready) {
    f = cogito_font_bold;
  }
  if (n && n->font_tabular) {
    f = cogito_font_tabular_for(f, size);
  }
  float spacing = (n && n->letter_spacing_set) ? n->letter_spacing : 0.0f;
  return cogito_text_width_size_font_spaced(f, s, size, spacing);
}

static int cogito_text_width_size_n_font(CogitoFont* f, const char* s, int len, int size) {
  if (!s || len <= 0) return 0;
  if (len > 1023) len = 1023;
  char buf[1024];
  memcpy(buf, s, (size_t)len);
  buf[len] = '\0';
  return cogito_text_width_size_font(f, buf, size);
}

static int cogito_text_width_size_n_node(const CogitoNode* n, const char* s, int len, int size) {
  CogitoFont* f = cogito_font_for_node(n);
  if (n && cogito_node_bold(n) && f == cogito_font && cogito_font_bold_ready) {
    f = cogito_font_bold;
  }
  if (n && n->font_tabular) {
    f = cogito_font_tabular_for(f, size);
  }
  float spacing = (n && n->letter_spacing_set) ? n->letter_spacing : 0.0f;
  if (!s || len <= 0) return 0;
  if (len > 1023) len = 1023;
  char buf[1024];
  memcpy(buf, s, (size_t)len);
  buf[len] = '\0';
  return cogito_text_width_size_font_spaced(f, buf, size, spacing);
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

__attribute__((unused))
static int cogito_text_width_size_n(const char* s, int len, int size) {
  return cogito_text_width_size_n_font(cogito_font, s, len, size);
}
