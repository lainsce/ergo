typedef struct {
  char name[96];
  int size;
  int min_size;
  int max_size;
  int threshold;
  int type;
} CogitoIconDir;

typedef struct {
  char base[PATH_MAX];
  char theme[64];
  CogitoIconDir dirs[128];
  int dir_count;
  char inherits[8][96];
  int inherit_count;
} CogitoIconTheme;

typedef struct {
  char name[128];
  char theme[64];
  int size;
  Texture2D tex;
  int w;
  int h;
  bool used;
} CogitoIconCacheEntry;

static CogitoIconCacheEntry cogito_icon_cache[128];
static int cogito_icon_cache_len = 0;
static char cogito_icon_theme_name[64] = {0};
static bool cogito_icon_theme_init = false;

static bool cogito_file_exists(const char* path) {
  struct stat st;
  return path && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static const char* cogito_default_font_path(void) {
#if defined(__APPLE__)
  const char* candidates[] = {
    "/System/Library/Fonts/SFNS.ttf",
    "/System/Library/Fonts/SFNSText.ttf",
    "/System/Library/Fonts/SFNSDisplay.ttf",
    "/System/Library/Fonts/Helvetica.ttc"
  };
#elif defined(_WIN32)
  const char* candidates[] = {
    "C:\\\\Windows\\\\Fonts\\\\segoeui.ttf"
  };
#else
  const char* candidates[] = {
    "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf"
  };
#endif
  for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
    if (cogito_file_exists(candidates[i])) return candidates[i];
  }
  return NULL;
}

static void cogito_trim(char* s) {
  if (!s) return;
  size_t len = strlen(s);
  size_t start = 0;
  while (start < len && isspace((unsigned char)s[start])) start++;
  size_t end = len;
  while (end > start && isspace((unsigned char)s[end - 1])) end--;
  if (start > 0) memmove(s, s + start, end - start);
  s[end - start] = 0;
}

static bool cogito_read_kv(const char* line, char* key, size_t key_cap, char* val, size_t val_cap) {
  const char* eq = strchr(line, '=');
  if (!eq) return false;
  size_t klen = (size_t)(eq - line);
  size_t vlen = strlen(eq + 1);
  if (klen >= key_cap) klen = key_cap - 1;
  if (vlen >= val_cap) vlen = val_cap - 1;
  strncpy(key, line, klen);
  key[klen] = 0;
  strncpy(val, eq + 1, vlen);
  val[vlen] = 0;
  cogito_trim(key);
  cogito_trim(val);
  return key[0] != 0;
}

static int cogito_icon_dir_index(CogitoIconTheme* theme, const char* name) {
  for (int i = 0; i < theme->dir_count; i++) {
    if (strcmp(theme->dirs[i].name, name) == 0) return i;
  }
  return -1;
}

static int cogito_parse_list(const char* src, char out[][96], int max) {
  int count = 0;
  const char* p = src;
  while (*p && count < max) {
    while (*p && (*p == ',' || isspace((unsigned char)*p))) p++;
    if (!*p) break;
    char buf[96];
    size_t i = 0;
    while (*p && *p != ',' && i + 1 < sizeof(buf)) {
      buf[i++] = *p++;
    }
    buf[i] = 0;
    cogito_trim(buf);
    if (buf[0]) {
      strncpy(out[count], buf, 95);
      out[count][95] = 0;
      count++;
    }
  }
  return count;
}

static int cogito_icon_base_dirs(char out[][PATH_MAX], int max) {
  int count = 0;
  const char* home = getenv("HOME");
  if (home && count < max) {
    snprintf(out[count++], PATH_MAX, "%s/.icons", home);
  }
  if (home && count < max) {
    snprintf(out[count++], PATH_MAX, "%s/.local/share/icons", home);
  }
  if (count < max) {
    snprintf(out[count++], PATH_MAX, "/opt/homebrew/share/icons");
  }
  const char* xdg = getenv("XDG_DATA_DIRS");
  if (!xdg || !*xdg) xdg = "/usr/local/share:/usr/share";
  const char* p = xdg;
  while (*p && count < max) {
    char buf[PATH_MAX];
    size_t i = 0;
    while (*p && *p != ':' && i + 1 < sizeof(buf)) buf[i++] = *p++;
    buf[i] = 0;
    if (*p == ':') p++;
    cogito_trim(buf);
    if (buf[0]) {
      snprintf(out[count++], PATH_MAX, "%s/icons", buf);
    }
  }
  return count;
}

static bool cogito_load_theme_file(const char* path, CogitoIconTheme* theme) {
  FILE* f = fopen(path, "rb");
  if (!f) return false;
  char line[512];
  char section[128] = {0};
  while (fgets(line, sizeof(line), f)) {
    char* s = line;
    while (*s && (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')) s++;
    if (*s == 0 || *s == '#' || *s == ';') continue;
    if (*s == '[') {
      char* end = strchr(s, ']');
      if (!end) continue;
      size_t len = (size_t)(end - (s + 1));
      if (len >= sizeof(section)) len = sizeof(section) - 1;
      strncpy(section, s + 1, len);
      section[len] = 0;
      cogito_trim(section);
      continue;
    }
    char key[96], val[384];
    if (!cogito_read_kv(s, key, sizeof(key), val, sizeof(val))) continue;
    if (strcmp(section, "Icon Theme") == 0) {
      if (strcmp(key, "Inherits") == 0) {
        theme->inherit_count = cogito_parse_list(val, theme->inherits, (int)(sizeof(theme->inherits) / sizeof(theme->inherits[0])));
      } else if (strcmp(key, "Directories") == 0) {
        char dirs[128][96];
        int n = cogito_parse_list(val, dirs, 128);
        for (int i = 0; i < n && theme->dir_count < 128; i++) {
          strncpy(theme->dirs[theme->dir_count].name, dirs[i], sizeof(theme->dirs[theme->dir_count].name) - 1);
          theme->dirs[theme->dir_count].name[sizeof(theme->dirs[theme->dir_count].name) - 1] = 0;
          theme->dirs[theme->dir_count].size = 0;
          theme->dirs[theme->dir_count].min_size = 0;
          theme->dirs[theme->dir_count].max_size = 0;
          theme->dirs[theme->dir_count].threshold = 2;
          theme->dirs[theme->dir_count].type = 0;
          theme->dir_count++;
        }
      }
    } else if (section[0]) {
      int idx = cogito_icon_dir_index(theme, section);
      if (idx >= 0) {
        CogitoIconDir* d = &theme->dirs[idx];
        if (strcmp(key, "Size") == 0) d->size = atoi(val);
        else if (strcmp(key, "MinSize") == 0) d->min_size = atoi(val);
        else if (strcmp(key, "MaxSize") == 0) d->max_size = atoi(val);
        else if (strcmp(key, "Threshold") == 0) d->threshold = atoi(val);
        else if (strcmp(key, "Type") == 0) {
          if (strcmp(val, "Scalable") == 0) d->type = 1;
          else if (strcmp(val, "Threshold") == 0) d->type = 2;
          else d->type = 0;
        }
      }
    }
  }
  fclose(f);
  return true;
}

static bool cogito_icon_theme_load(const char* theme_name, CogitoIconTheme* out) {
  if (!theme_name || !out) return false;
  memset(out, 0, sizeof(*out));
  strncpy(out->theme, theme_name, sizeof(out->theme) - 1);
  out->theme[sizeof(out->theme) - 1] = 0;
  char bases[16][PATH_MAX];
  int base_count = cogito_icon_base_dirs(bases, 16);
  for (int i = 0; i < base_count; i++) {
    char index_path[PATH_MAX];
    snprintf(index_path, sizeof(index_path), "%s/%s/index.theme", bases[i], theme_name);
    if (cogito_file_exists(index_path)) {
      snprintf(out->base, sizeof(out->base), "%s/%s", bases[i], theme_name);
      if (cogito_load_theme_file(index_path, out)) return true;
    }
  }
  return false;
}

static int cogito_icon_match_score(const CogitoIconDir* d, int size) {
  if (!d) return 100000;
  if (d->type == 1) {
    int minv = d->min_size ? d->min_size : d->size;
    int maxv = d->max_size ? d->max_size : d->size;
    if (size >= minv && size <= maxv) return 0;
    if (size < minv) return minv - size;
    return size - maxv;
  }
  if (d->type == 2) {
    int thr = d->threshold > 0 ? d->threshold : 2;
    int diff = abs(size - d->size);
    return diff <= thr ? 0 : diff - thr;
  }
  return abs(size - d->size);
}

static bool cogito_icon_find_in_theme(const char* theme_name, const char* icon, int size, char* out_path, size_t cap, bool* out_svg) {
  CogitoIconTheme theme;
  if (!cogito_icon_theme_load(theme_name, &theme)) return false;
  int best_score = 100000;
  bool best_svg = false;
  char best_path[PATH_MAX] = {0};
  for (int i = 0; i < theme.dir_count; i++) {
    CogitoIconDir* d = &theme.dirs[i];
    int score = cogito_icon_match_score(d, size);
    char svg_path[PATH_MAX];
    char png_path[PATH_MAX];
    snprintf(svg_path, sizeof(svg_path), "%s/%s/%s.svg", theme.base, d->name, icon);
    snprintf(png_path, sizeof(png_path), "%s/%s/%s.png", theme.base, d->name, icon);
    bool has_svg = cogito_file_exists(svg_path);
    bool has_png = cogito_file_exists(png_path);
    if (!has_svg && !has_png) continue;
    bool choose_svg = has_svg;
    if (score < best_score || (score == best_score && choose_svg && !best_svg)) {
      best_score = score;
      best_svg = choose_svg;
      strncpy(best_path, choose_svg ? svg_path : png_path, sizeof(best_path) - 1);
      best_path[sizeof(best_path) - 1] = 0;
    }
  }
  if (best_path[0]) {
    if (out_path && cap) {
      strncpy(out_path, best_path, cap - 1);
      out_path[cap - 1] = 0;
    }
    if (out_svg) *out_svg = best_svg;
    return true;
  }
  return false;
}

static bool cogito_icon_find_file(const char* theme_name, const char* icon, int size, char* out_path, size_t cap, bool* out_svg) {
  if (cogito_icon_find_in_theme(theme_name, icon, size, out_path, cap, out_svg)) return true;
  CogitoIconTheme theme;
  if (!cogito_icon_theme_load(theme_name, &theme)) return false;
  for (int i = 0; i < theme.inherit_count; i++) {
    if (cogito_icon_find_in_theme(theme.inherits[i], icon, size, out_path, cap, out_svg)) return true;
  }
  if (strcmp(theme_name, "hicolor") != 0) {
    if (cogito_icon_find_in_theme("hicolor", icon, size, out_path, cap, out_svg)) return true;
  }
  return false;
}

static const char* cogito_icon_theme_get(void) {
  if (!cogito_icon_theme_init) {
    const char* env = getenv("COGITO_ICON_THEME");
    if (env && *env) {
      strncpy(cogito_icon_theme_name, env, sizeof(cogito_icon_theme_name) - 1);
    } else {
      strncpy(cogito_icon_theme_name, "hicolor", sizeof(cogito_icon_theme_name) - 1);
    }
    cogito_icon_theme_name[sizeof(cogito_icon_theme_name) - 1] = 0;
    cogito_icon_theme_init = true;
  }
  return cogito_icon_theme_name;
}

static bool cogito_icon_is_sf(const char* name) {
  return name && strncmp(name, "sf:", 3) == 0;
}

#if defined(__APPLE__)
static bool cogito_sf_symbol_load(const char* symbol, int size, Texture2D* out, int* out_w, int* out_h) {
  if (!symbol || !symbol[0] || size <= 0) return false;
  id nsimage = (id)objc_getClass("NSImage");
  if (!nsimage) return false;
  SEL sel_image = sel_registerName("imageWithSystemSymbolName:accessibilityDescription:");
  id name = cogito_nsstring(symbol);
  id img = ((id (*)(id, SEL, id, id))objc_msgSend)(nsimage, sel_image, name, (id)0);
  if (!img) return false;
  // Avoid configuration calls that can abort for invalid weights/scales.
  SEL sel_cg = sel_registerName("CGImageForProposedRect:context:hints:");
  CGRect rect = CGRectMake(0, 0, (CGFloat)size, (CGFloat)size);
  CGImageRef cg = (CGImageRef)((void* (*)(id, SEL, CGRect*, id, id))objc_msgSend)(img, sel_cg, &rect, (id)0, (id)0);
  if (!cg) return false;
  size_t w = (size_t)CGRectGetWidth(rect);
  size_t h = (size_t)CGRectGetHeight(rect);
  if (w == 0 || h == 0) { w = (size_t)size; h = (size_t)size; }
  CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
  if (!cs) return false;
  size_t bpr = w * 4;
  CGContextRef ctx = CGBitmapContextCreate(NULL, w, h, 8, bpr, cs, kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
  if (!ctx) { CGColorSpaceRelease(cs); return false; }
  CGContextClearRect(ctx, CGRectMake(0, 0, (CGFloat)w, (CGFloat)h));
  CGContextDrawImage(ctx, CGRectMake(0, 0, (CGFloat)w, (CGFloat)h), cg);
  unsigned char* data = (unsigned char*)CGBitmapContextGetData(ctx);
  if (!data) { CGContextRelease(ctx); CGColorSpaceRelease(cs); return false; }
  size_t len = w * h * 4;
  unsigned char* copy = (unsigned char*)malloc(len);
  if (!copy) { CGContextRelease(ctx); CGColorSpaceRelease(cs); return false; }
  memcpy(copy, data, len);
  Image imgdata = {0};
  imgdata.data = copy;
  imgdata.width = (int)w;
  imgdata.height = (int)h;
  imgdata.mipmaps = 1;
  imgdata.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
  // Convert to white mask so tinting works for icons.
  unsigned char* px = (unsigned char*)imgdata.data;
  size_t pxlen = (size_t)imgdata.width * (size_t)imgdata.height * 4;
  for (size_t i = 0; i + 3 < pxlen; i += 4) {
    px[i + 0] = 255;
    px[i + 1] = 255;
    px[i + 2] = 255;
  }
  Texture2D tex = LoadTextureFromImage(imgdata);
  UnloadImage(imgdata);
  CGContextRelease(ctx);
  CGColorSpaceRelease(cs);
  if (tex.id == 0) return false;
  SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
  if (out) *out = tex;
  if (out_w) *out_w = (int)w;
  if (out_h) *out_h = (int)h;
  return true;
}
#endif

static bool cogito_icon_load_texture(const char* name, int size, Texture2D* out, int* out_w, int* out_h) {
  if (!name || !name[0] || size <= 0) return false;
  const char* theme = cogito_icon_is_sf(name) ? "sf" : cogito_icon_theme_get();
  for (int i = 0; i < cogito_icon_cache_len; i++) {
    CogitoIconCacheEntry* e = &cogito_icon_cache[i];
    if (e->used && e->size == size && strcmp(e->name, name) == 0 && strcmp(e->theme, theme) == 0) {
      if (out) *out = e->tex;
      if (out_w) *out_w = e->w;
      if (out_h) *out_h = e->h;
      return true;
    }
  }
  if (cogito_icon_is_sf(name)) {
#if defined(__APPLE__)
    Texture2D tex = {0};
    int tw = 0;
    int th = 0;
    if (cogito_sf_symbol_load(name + 3, size, &tex, &tw, &th)) {
      SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
      if (cogito_icon_cache_len < (int)(sizeof(cogito_icon_cache) / sizeof(cogito_icon_cache[0]))) {
        CogitoIconCacheEntry* e = &cogito_icon_cache[cogito_icon_cache_len++];
        memset(e, 0, sizeof(*e));
        strncpy(e->name, name, sizeof(e->name) - 1);
        strncpy(e->theme, theme, sizeof(e->theme) - 1);
        e->size = size;
        e->tex = tex;
        e->w = tw;
        e->h = th;
        e->used = true;
      }
      if (out) *out = tex;
      if (out_w) *out_w = tw;
      if (out_h) *out_h = th;
      return true;
    }
#endif
    return false;
  }
  char path[PATH_MAX];
  bool is_svg = false;
  if (!cogito_icon_find_file(theme, name, size, path, sizeof(path), &is_svg)) return false;
  Texture2D tex = {0};
  int tw = 0;
  int th = 0;
  (void)is_svg;
  Image img = LoadImage(path);
  if (img.data) {
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    unsigned char* px = (unsigned char*)img.data;
    size_t pxlen = (size_t)img.width * (size_t)img.height * 4;
    for (size_t i = 0; i + 3 < pxlen; i += 4) {
      px[i + 0] = 255;
      px[i + 1] = 255;
      px[i + 2] = 255;
    }
    int maxdim = img.width > img.height ? img.width : img.height;
    if (size > 0 && maxdim > 0 && maxdim != size) {
      float scale = (float)size / (float)maxdim;
      int nw = (int)lroundf((float)img.width * scale);
      int nh = (int)lroundf((float)img.height * scale);
      if (nw < 1) nw = 1;
      if (nh < 1) nh = 1;
      ImageResize(&img, nw, nh);
    }
    tw = img.width;
    th = img.height;
    tex = LoadTextureFromImage(img);
    UnloadImage(img);
  }
  if (tex.id == 0) return false;
  if (cogito_icon_cache_len < (int)(sizeof(cogito_icon_cache) / sizeof(cogito_icon_cache[0]))) {
    CogitoIconCacheEntry* e = &cogito_icon_cache[cogito_icon_cache_len++];
    memset(e, 0, sizeof(*e));
    strncpy(e->name, name, sizeof(e->name) - 1);
    strncpy(e->theme, theme, sizeof(e->theme) - 1);
    e->size = size;
    e->tex = tex;
    e->w = tw;
    e->h = th;
    e->used = true;
  }
  if (out) *out = tex;
  if (out_w) *out_w = tw;
  if (out_h) *out_h = th;
  return true;
}

static void cogito_load_css_file(const char* path);
