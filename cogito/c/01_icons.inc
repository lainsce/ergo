// ---- Cogito GUI (SDL3) ----

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
  CogitoTexture* tex;
  int w;
  int h;
  bool used;
} CogitoIconCacheEntry;

static CogitoIconCacheEntry cogito_icon_cache[128];
static int cogito_icon_cache_len = 0;
static char cogito_icon_theme_name[64] = {0};
static bool cogito_icon_theme_init = false;

#if defined(COGITO_HAS_SDL3_IMAGE)
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_surface.h>
#include <SDL3_image/SDL_image.h>
#endif

#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

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

static const char* cogito_replace_once(const char* path, const char* find, const char* repl, char* out, size_t cap) {
  if (!path || !find || !repl || !out || cap == 0) return NULL;
  const char* pos = strstr(path, find);
  if (!pos) return NULL;
  size_t prefix = (size_t)(pos - path);
  size_t find_len = strlen(find);
  size_t repl_len = strlen(repl);
  size_t tail_len = strlen(pos + find_len);
  if (prefix + repl_len + tail_len + 1 > cap) return NULL;
  memcpy(out, path, prefix);
  memcpy(out + prefix, repl, repl_len);
  memcpy(out + prefix + repl_len, pos + find_len, tail_len + 1);
  return out;
}

static const char* cogito_guess_bold_font_path(const char* regular_path) {
  static char buf[PATH_MAX];
  bool has_regular = regular_path && regular_path[0];
  if (has_regular) {
    if (strstr(regular_path, "Bold") || strstr(regular_path, "bold")) {
      if (cogito_file_exists(regular_path)) return regular_path;
    }
    const struct { const char* find; const char* repl; } swaps[] = {
      {"-Regular", "-Bold"}, {"_Regular", "_Bold"}, {" Regular", " Bold"}, {"Regular", "Bold"},
      {"-Roman", "-Bold"}, {"_Roman", "_Bold"}, {" Roman", " Bold"}, {"Roman", "Bold"},
      {"-Book", "-Bold"}, {"_Book", "_Bold"}, {" Book", " Bold"}, {"Book", "Bold"},
      {"-Medium", "-Bold"}, {"_Medium", "_Bold"}, {" Medium", " Bold"}, {"Medium", "Bold"},
      {"-Light", "-Bold"}, {"_Light", "_Bold"}, {" Light", " Bold"}, {"Light", "Bold"},
      {"-Text", "-Bold"}, {"_Text", "_Bold"}, {" Text", " Bold"}, {"Text", "Bold"},
      {"-regular", "-bold"}, {"_regular", "_bold"}, {" regular", " bold"}, {"regular", "bold"},
      {"-roman", "-bold"}, {"_roman", "_bold"}, {" roman", " bold"}, {"roman", "bold"},
      {"-book", "-bold"}, {"_book", "_bold"}, {" book", " bold"}, {"book", "bold"},
      {"-medium", "-bold"}, {"_medium", "_bold"}, {" medium", " bold"}, {"medium", "bold"},
      {"-light", "-bold"}, {"_light", "_bold"}, {" light", " bold"}, {"light", "bold"},
      {"-text", "-bold"}, {"_text", "_bold"}, {" text", " bold"}, {"text", "bold"}
    };
    for (size_t i = 0; i < sizeof(swaps) / sizeof(swaps[0]); i++) {
      if (cogito_replace_once(regular_path, swaps[i].find, swaps[i].repl, buf, sizeof(buf))) {
        if (cogito_file_exists(buf)) return buf;
      }
    }
    const char* dot = strrchr(regular_path, '.');
    if (dot && dot != regular_path) {
      size_t base_len = (size_t)(dot - regular_path);
      if (base_len + 6 + strlen(dot) + 1 < sizeof(buf)) {
        memcpy(buf, regular_path, base_len);
        memcpy(buf + base_len, "-Bold", 5);
        strcpy(buf + base_len + 5, dot);
        if (cogito_file_exists(buf)) return buf;
      }
      if (base_len + 5 + strlen(dot) + 1 < sizeof(buf)) {
        memcpy(buf, regular_path, base_len);
        memcpy(buf + base_len, "Bold", 4);
        strcpy(buf + base_len + 4, dot);
        if (cogito_file_exists(buf)) return buf;
      }
    }
  }
  if (has_regular) {
    // Avoid mixing unrelated font families when we already have a base font.
    return NULL;
  }
#if defined(__APPLE__)
  const char* candidates[] = {
    "/System/Library/Fonts/HelveticaNeue.ttc",
    "/System/Library/Fonts/Helvetica.ttc"
  };
#elif defined(_WIN32)
  const char* candidates[] = {
    "C:\\\\Windows\\\\Fonts\\\\segoeuib.ttf"
  };
#else
  const char* candidates[] = {
    "/usr/share/fonts/truetype/noto/NotoSans-Bold.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf"
  };
#endif
  for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
    if (cogito_file_exists(candidates[i])) return candidates[i];
  }
  return NULL;
}

static const char* cogito_guess_mono_font_path(const char* regular_path) {
  if (regular_path && regular_path[0]) {
    if (strstr(regular_path, "SFNS.ttf")) {
      if (cogito_file_exists("/System/Library/Fonts/SFNSMono.ttf")) return "/System/Library/Fonts/SFNSMono.ttf";
    }
    if (strstr(regular_path, "SFNSItalic.ttf")) {
      if (cogito_file_exists("/System/Library/Fonts/SFNSMonoItalic.ttf")) return "/System/Library/Fonts/SFNSMonoItalic.ttf";
    }
    if (strstr(regular_path, "SFNSRounded.ttf")) {
      if (cogito_file_exists("/System/Library/Fonts/SFNSMono.ttf")) return "/System/Library/Fonts/SFNSMono.ttf";
    }
    if (strstr(regular_path, "SFCompact")) {
      if (cogito_file_exists("/System/Library/Fonts/SFNSMono.ttf")) return "/System/Library/Fonts/SFNSMono.ttf";
    }
  }
#if defined(__APPLE__)
  const char* candidates[] = {
    "/System/Library/Fonts/SFNSMono.ttf",
    "/System/Library/Fonts/Menlo.ttc"
  };
#elif defined(_WIN32)
  const char* candidates[] = {
    "C:\\\\Windows\\\\Fonts\\\\consola.ttf"
  };
#else
  const char* candidates[] = {
    "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
    "/usr/share/fonts/truetype/noto/NotoSansMono-Regular.ttf"
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
#if defined(_WIN32)
  const char* localappdata = getenv("LOCALAPPDATA");
  const char* programdata = getenv("ProgramData");
  if (localappdata && count < max) {
    snprintf(out[count++], PATH_MAX, "%s\\icons", localappdata);
  }
  if (programdata && count < max) {
    snprintf(out[count++], PATH_MAX, "%s\\icons", programdata);
  }
  return count;
#else
  const char* home = getenv("HOME");
  if (home && count < max) {
    snprintf(out[count++], PATH_MAX, "%s/.icons", home);
  }
  if (home && count < max) {
    snprintf(out[count++], PATH_MAX, "%s/.local/share/icons", home);
  }
#if defined(__APPLE__)
  if (count < max) {
    snprintf(out[count++], PATH_MAX, "/opt/homebrew/share/icons");
  }
#endif
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
#endif
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
  if (theme_name && theme_name[0] && cogito_icon_find_in_theme(theme_name, icon, size, out_path, cap, out_svg)) return true;
  CogitoIconTheme theme;
  if (theme_name && theme_name[0] && cogito_icon_theme_load(theme_name, &theme)) {
    for (int i = 0; i < theme.inherit_count; i++) {
      if (cogito_icon_find_in_theme(theme.inherits[i], icon, size, out_path, cap, out_svg)) return true;
    }
  }
  if (!theme_name || strcmp(theme_name, "Adwaita") != 0) {
    if (cogito_icon_find_in_theme("Adwaita", icon, size, out_path, cap, out_svg)) return true;
  }
  if (!theme_name || strcmp(theme_name, "hicolor") != 0) {
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
#if defined(__APPLE__)
      strncpy(cogito_icon_theme_name, "sf", sizeof(cogito_icon_theme_name) - 1);
#elif defined(_WIN32)
      strncpy(cogito_icon_theme_name, "hicolor", sizeof(cogito_icon_theme_name) - 1);
#else
      const char* gtk = getenv("GTK_THEME");
      if (gtk && *gtk) {
        size_t n = 0;
        while (gtk[n] && gtk[n] != ':' && n + 1 < sizeof(cogito_icon_theme_name)) {
          cogito_icon_theme_name[n] = gtk[n];
          n++;
        }
        cogito_icon_theme_name[n] = 0;
      } else {
        strncpy(cogito_icon_theme_name, "Adwaita", sizeof(cogito_icon_theme_name) - 1);
      }
#endif
    }
    if (!cogito_icon_theme_name[0]) {
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

static void cogito_icon_add_candidate(char out[][128], int* count, int max, const char* name) {
  if (!out || !count || !name || !name[0] || *count >= max) return;
  for (int i = 0; i < *count; i++) {
    if (strcmp(out[i], name) == 0) return;
  }
  strncpy(out[*count], name, 127);
  out[*count][127] = 0;
  (*count)++;
}

static const char* cogito_icon_alias_sf(const char* token) {
  if (!token || !token[0]) return token;
  if (strcmp(token, "x") == 0 || strcmp(token, "close") == 0 || strcmp(token, "window-close") == 0) return "xmark";
  if (strcmp(token, "window-min") == 0 || strcmp(token, "minimize") == 0) return "minus";
  if (strcmp(token, "window-max") == 0 || strcmp(token, "maximize") == 0) return "plus.square";
  if (strcmp(token, "search") == 0 || strcmp(token, "system-search") == 0) return "magnifyingglass";
  if (strcmp(token, "chevron-down") == 0 || strcmp(token, "pan-down") == 0) return "chevron.down";
  if (strcmp(token, "chevron-up") == 0 || strcmp(token, "pan-up") == 0) return "chevron.up";
  if (strcmp(token, "chevron-left") == 0 || strcmp(token, "pan-start") == 0) return "chevron.left";
  if (strcmp(token, "chevron-right") == 0 || strcmp(token, "pan-end") == 0) return "chevron.right";
  if (strcmp(token, "add") == 0) return "plus";
  if (strcmp(token, "remove") == 0) return "minus";
  if (strcmp(token, "question") == 0) return "questionmark";
  if (strcmp(token, "check") == 0) return "checkmark";
  return token;
}

#if !defined(__APPLE__)
static const char* cogito_icon_alias_theme(const char* token) {
  if (!token || !token[0]) return token;
  if (strcmp(token, "x") == 0 || strcmp(token, "xmark") == 0 || strcmp(token, "close") == 0 || strcmp(token, "window-close") == 0) return "window-close-symbolic";
  if (strcmp(token, "window-min") == 0 || strcmp(token, "minimize") == 0 || strcmp(token, "minus") == 0) return "window-minimize-symbolic";
  if (strcmp(token, "window-max") == 0 || strcmp(token, "maximize") == 0) return "window-maximize-symbolic";
  if (strcmp(token, "search") == 0 || strcmp(token, "magnifyingglass") == 0 || strcmp(token, "system-search") == 0) return "system-search-symbolic";
  if (strcmp(token, "chevron.down") == 0 || strcmp(token, "chevron-down") == 0 || strcmp(token, "pan-down") == 0) return "pan-down-symbolic";
  if (strcmp(token, "chevron.up") == 0 || strcmp(token, "chevron-up") == 0 || strcmp(token, "pan-up") == 0) return "pan-up-symbolic";
  if (strcmp(token, "chevron.left") == 0 || strcmp(token, "chevron-left") == 0 || strcmp(token, "pan-start") == 0) return "pan-start-symbolic";
  if (strcmp(token, "chevron.right") == 0 || strcmp(token, "chevron-right") == 0 || strcmp(token, "pan-end") == 0) return "pan-end-symbolic";
  if (strcmp(token, "plus") == 0 || strcmp(token, "add") == 0 || strcmp(token, "list-add") == 0) return "list-add-symbolic";
  if (strcmp(token, "minus") == 0 || strcmp(token, "remove") == 0 || strcmp(token, "list-remove") == 0) return "list-remove-symbolic";
  if (strcmp(token, "check") == 0 || strcmp(token, "checkmark") == 0) return "object-select-symbolic";
  if (strcmp(token, "photo") == 0 || strcmp(token, "image") == 0) return "image-x-generic-symbolic";
  if (strcmp(token, "question") == 0 || strcmp(token, "questionmark") == 0 || strcmp(token, "help") == 0) return "help-browser-symbolic";
  return token;
}
#endif

static int cogito_icon_candidates(const char* name, char out[][128], int max) {
  if (!name || !name[0] || !out || max <= 0) return 0;
  int count = 0;
  const char* token = name;
  if (strncmp(name, "sf:", 3) == 0) token = name + 3;
  else if (strncmp(name, "gtk:", 4) == 0) token = name + 4;
  else if (strncmp(name, "win:", 4) == 0) token = name + 4;

#if defined(__APPLE__)
  char sf_name[128];
  const char* sf = cogito_icon_alias_sf(token);
  snprintf(sf_name, sizeof(sf_name), "sf:%s", sf);
  cogito_icon_add_candidate(out, &count, max, sf_name);
#else
  const char* themed = cogito_icon_alias_theme(token);
  cogito_icon_add_candidate(out, &count, max, themed);
  cogito_icon_add_candidate(out, &count, max, token);
  cogito_icon_add_candidate(out, &count, max, name);
#endif
  return count;
}

static int cogito_sf_optimized_size(int size) {
  static const int optimized[] = {16, 24, 32, 48, 64, 128};
  if (size <= 0) return size;
  for (size_t i = 0; i < sizeof(optimized) / sizeof(optimized[0]); i++) {
    if (size <= optimized[i]) return optimized[i];
  }
  return size;
}

#if defined(__APPLE__)
static bool cogito_sf_symbol_load(const char* symbol, int size, CogitoTexture** out, int* out_w, int* out_h) {
  if (!symbol || !symbol[0] || size <= 0) return false;
  id nsimage = (id)objc_getClass("NSImage");
  if (!nsimage) return false;
  SEL sel_image = sel_registerName("imageWithSystemSymbolName:accessibilityDescription:");
  id name = cogito_nsstring(symbol);
  id img = ((id (*)(id, SEL, id, id))objc_msgSend)(nsimage, sel_image, name, (id)0);
  if (!img) return false;
  int raster_size = size * 2;
  if (raster_size < size) raster_size = size;
  if (raster_size > 1024) raster_size = 1024;
  // Configure SF symbol at the requested size so we do not upscale tiny rasters.
  id cfg_cls = (id)objc_getClass("NSImageSymbolConfiguration");
  if (cfg_cls) {
    SEL sel_apply = sel_registerName("imageWithSymbolConfiguration:");
    SEL sel_responds = sel_registerName("respondsToSelector:");
    BOOL can_apply = ((BOOL (*)(id, SEL, SEL))objc_msgSend)(img, sel_responds, sel_apply);
    if (can_apply) {
      id cfg = (id)0;
      SEL sel_cfg3 = sel_registerName("configurationWithPointSize:weight:scale:");
      BOOL can_cfg3 = ((BOOL (*)(id, SEL, SEL))objc_msgSend)(cfg_cls, sel_responds, sel_cfg3);
      if (can_cfg3) {
        // 0.0 = regular weight, 2 = medium symbol scale.
        cfg = ((id (*)(id, SEL, CGFloat, CGFloat, CogitoNSInteger))objc_msgSend)(cfg_cls, sel_cfg3, (CGFloat)raster_size, (CGFloat)0.0, (CogitoNSInteger)2);
      }
      if (!cfg) {
        SEL sel_cfg2 = sel_registerName("configurationWithPointSize:weight:");
        BOOL can_cfg2 = ((BOOL (*)(id, SEL, SEL))objc_msgSend)(cfg_cls, sel_responds, sel_cfg2);
        if (can_cfg2) {
          cfg = ((id (*)(id, SEL, CGFloat, CGFloat))objc_msgSend)(cfg_cls, sel_cfg2, (CGFloat)raster_size, (CGFloat)0.0);
        }
      }
      if (!cfg) {
        SEL sel_cfg1 = sel_registerName("configurationWithPointSize:");
        BOOL can_cfg1 = ((BOOL (*)(id, SEL, SEL))objc_msgSend)(cfg_cls, sel_responds, sel_cfg1);
        if (can_cfg1) {
          cfg = ((id (*)(id, SEL, CGFloat))objc_msgSend)(cfg_cls, sel_cfg1, (CGFloat)raster_size);
        }
      }
      if (cfg) {
        id cfg_img = ((id (*)(id, SEL, id))objc_msgSend)(img, sel_apply, cfg);
        if (cfg_img) img = cfg_img;
      }
    }
  }
  SEL sel_cg = sel_registerName("CGImageForProposedRect:context:hints:");
  CGImageRef cg = (CGImageRef)((void* (*)(id, SEL, CGRect*, id, id))objc_msgSend)(img, sel_cg, (CGRect*)0, (id)0, (id)0);
  if (!cg) return false;
  size_t src_w = (size_t)CGImageGetWidth(cg);
  size_t src_h = (size_t)CGImageGetHeight(cg);
  size_t w = src_w;
  size_t h = src_h;
  if (w == 0 || h == 0) {
    w = (size_t)raster_size;
    h = (size_t)raster_size;
  }
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
  // Convert to white mask so tinting works for icons.
  unsigned char* px = (unsigned char*)copy;
  size_t pxlen = w * h * 4;
  for (size_t i = 0; i + 3 < pxlen; i += 4) {
    px[i + 0] = 255;
    px[i + 1] = 255;
    px[i + 2] = 255;
  }
  CogitoTexture* tex = cogito_backend->texture_create((int)w, (int)h, copy, 4);
  free(copy);
  CGContextRelease(ctx);
  CGColorSpaceRelease(cs);
  if (!tex) return false;
  if (out) *out = tex;
  if (out_w) *out_w = (int)w;
  if (out_h) *out_h = (int)h;
  return true;
}
#endif

static bool cogito_icon_load_file_texture(const char* path, CogitoTexture** out, int* out_w, int* out_h) {
  if (!path || !path[0] || !cogito_backend || !cogito_backend->texture_create) return false;
#if defined(COGITO_HAS_SDL3_IMAGE)
  SDL_Surface* src = IMG_Load(path);
  if (!src) return false;
  SDL_Surface* rgba = SDL_ConvertSurface(src, SDL_PIXELFORMAT_RGBA32);
  SDL_DestroySurface(src);
  if (!rgba || rgba->w <= 0 || rgba->h <= 0 || !rgba->pixels) {
    if (rgba) SDL_DestroySurface(rgba);
    return false;
  }
  int w = rgba->w;
  int h = rgba->h;
  size_t row = (size_t)w * 4u;
  size_t len = row * (size_t)h;
  uint8_t* copy = (uint8_t*)malloc(len);
  if (!copy) {
    SDL_DestroySurface(rgba);
    return false;
  }
  const uint8_t* src_px = (const uint8_t*)rgba->pixels;
  for (int y = 0; y < h; y++) {
    memcpy(copy + (size_t)y * row, src_px + (size_t)y * (size_t)rgba->pitch, row);
  }
  for (size_t i = 0; i + 3 < len; i += 4) {
    copy[i + 0] = 255;
    copy[i + 1] = 255;
    copy[i + 2] = 255;
  }
  CogitoTexture* tex = cogito_backend->texture_create(w, h, copy, 4);
  free(copy);
  SDL_DestroySurface(rgba);
  if (!tex) return false;
  if (out) *out = tex;
  if (out_w) *out_w = w;
  if (out_h) *out_h = h;
  return true;
#else
  (void)out;
  (void)out_w;
  (void)out_h;
  return false;
#endif
}

static bool cogito_icon_load_texture_one(const char* name, const char* theme, int size, CogitoTexture** out, int* out_w, int* out_h) {
  if (!name || !name[0] || size <= 0) return false;
  bool sf_icon = cogito_icon_is_sf(name);
  int cache_size = sf_icon ? cogito_sf_optimized_size(size) : size;
  const char* cache_theme = sf_icon ? "sf" : (theme && theme[0] ? theme : "hicolor");
  for (int i = 0; i < cogito_icon_cache_len; i++) {
    CogitoIconCacheEntry* e = &cogito_icon_cache[i];
    if (e->used && e->size == cache_size && strcmp(e->name, name) == 0 && strcmp(e->theme, cache_theme) == 0) {
      if (out) *out = e->tex;
      if (out_w) *out_w = e->w;
      if (out_h) *out_h = e->h;
      return true;
    }
  }
  if (sf_icon) {
#if defined(__APPLE__)
    CogitoTexture* tex = NULL;
    int tw = 0;
    int th = 0;
    if (cogito_sf_symbol_load(name + 3, cache_size, &tex, &tw, &th)) {
      if (cogito_icon_cache_len < (int)(sizeof(cogito_icon_cache) / sizeof(cogito_icon_cache[0]))) {
        CogitoIconCacheEntry* e = &cogito_icon_cache[cogito_icon_cache_len++];
        memset(e, 0, sizeof(*e));
        strncpy(e->name, name, sizeof(e->name) - 1);
        strncpy(e->theme, cache_theme, sizeof(e->theme) - 1);
        e->size = cache_size;
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
  if (!cogito_icon_find_file(cache_theme, name, size, path, sizeof(path), &is_svg)) return false;
  CogitoTexture* tex = NULL;
  int tw = 0;
  int th = 0;
  (void)is_svg;
  if (!cogito_icon_load_file_texture(path, &tex, &tw, &th)) return false;
  if (cogito_icon_cache_len < (int)(sizeof(cogito_icon_cache) / sizeof(cogito_icon_cache[0]))) {
    CogitoIconCacheEntry* e = &cogito_icon_cache[cogito_icon_cache_len++];
    memset(e, 0, sizeof(*e));
    strncpy(e->name, name, sizeof(e->name) - 1);
    strncpy(e->theme, cache_theme, sizeof(e->theme) - 1);
    e->size = cache_size;
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

static bool cogito_icon_load_texture(const char* name, int size, CogitoTexture** out, int* out_w, int* out_h) {
  if (!name || !name[0] || size <= 0) return false;
  char candidates[8][128];
  int n = cogito_icon_candidates(name, candidates, (int)(sizeof(candidates) / sizeof(candidates[0])));
  const char* theme = cogito_icon_theme_get();
  for (int i = 0; i < n; i++) {
    if (cogito_icon_load_texture_one(candidates[i], theme, size, out, out_w, out_h)) return true;
  }
  return false;
}

static void cogito_icon_cache_clear(void) {
  for (int i = 0; i < cogito_icon_cache_len; i++) {
    CogitoIconCacheEntry* e = &cogito_icon_cache[i];
    if (e->tex && cogito_backend && cogito_backend->texture_destroy) {
      cogito_backend->texture_destroy(e->tex);
      e->tex = NULL;
    }
  }
  cogito_icon_cache_len = 0;
}

// ============================================================================
// Image Loading (preserves original colors, no white mask conversion)
// ============================================================================

static char cogito_script_dir[PATH_MAX] = {0};

void cogito_set_script_dir(const char* dir) {
  if (dir && dir[0]) {
    snprintf(cogito_script_dir, sizeof(cogito_script_dir), "%s", dir);
  } else {
    cogito_script_dir[0] = 0;
  }
}

const char* cogito_get_script_dir(void) {
  return cogito_script_dir[0] ? cogito_script_dir : NULL;
}

static bool cogito_path_is_absolute(const char* path) {
  if (!path || !path[0]) return false;
#if defined(_WIN32)
  if ((isalpha((unsigned char)path[0]) && path[1] == ':') ||
      path[0] == '\\' || path[0] == '/') return true;
  return false;
#else
  return path[0] == '/';
#endif
}

static bool cogito_image_resolve_path(const char* path, char* resolved, size_t resolved_cap) {
  if (!path || !path[0] || !resolved || resolved_cap == 0) return false;
  
  // Check if path is absolute - if so, try it directly first
  if (cogito_path_is_absolute(path)) {
    if (cogito_file_exists(path)) {
      snprintf(resolved, resolved_cap, "%s", path);
      return true;
    }
    return false;
  }
  
  // Skip leading '/' in path and treat as relative
  const char* rel_path = path;
  if (path[0] == '/') rel_path = path + 1;
  
  // Try script directory first
  if (cogito_script_dir[0]) {
    size_t bl = strlen(cogito_script_dir);
    bool has_sep = bl > 0 && (cogito_script_dir[bl - 1] == '/' || cogito_script_dir[bl - 1] == '\\');
    if (has_sep) snprintf(resolved, resolved_cap, "%s%s", cogito_script_dir, rel_path);
    else snprintf(resolved, resolved_cap, "%s/%s", cogito_script_dir, rel_path);
    if (cogito_file_exists(resolved)) return true;
  }
  
  // Try current working directory
  if (getcwd(resolved, resolved_cap)) {
    size_t cwd_len = strlen(resolved);
    if (cwd_len + strlen(rel_path) + 2 < resolved_cap) {
      resolved[cwd_len] = '/';
      strcpy(resolved + cwd_len + 1, rel_path);
      if (cogito_file_exists(resolved)) return true;
    }
  }
  
  // Try as-is (relative to cwd)
  snprintf(resolved, resolved_cap, "%s", rel_path);
  return cogito_file_exists(resolved);
}

static bool cogito_image_load_file_texture(const char* path, CogitoTexture** out, int* out_w, int* out_h, float* out_avg_lum) {
  if (!path || !path[0] || !cogito_backend || !cogito_backend->texture_create) return false;
  
  // Resolve the path
  char resolved_path[PATH_MAX];
  if (!cogito_image_resolve_path(path, resolved_path, sizeof(resolved_path))) {
    return false;
  }
  
#if defined(COGITO_HAS_SDL3_IMAGE)
  SDL_Surface* src = IMG_Load(resolved_path);
  if (!src) return false;
  SDL_Surface* rgba = SDL_ConvertSurface(src, SDL_PIXELFORMAT_RGBA32);
  SDL_DestroySurface(src);
  if (!rgba || rgba->w <= 0 || rgba->h <= 0 || !rgba->pixels) {
    if (rgba) SDL_DestroySurface(rgba);
    return false;
  }
  int w = rgba->w;
  int h = rgba->h;
  size_t row = (size_t)w * 4u;
  size_t len = row * (size_t)h;
  uint8_t* copy = (uint8_t*)malloc(len);
  if (!copy) {
    SDL_DestroySurface(rgba);
    return false;
  }
  const uint8_t* src_px = (const uint8_t*)rgba->pixels;
  for (int y = 0; y < h; y++) {
    memcpy(copy + (size_t)y * row, src_px + (size_t)y * (size_t)rgba->pitch, row);
  }
  // Compute average luminance by sampling a sparse grid of pixels.
  if (out_avg_lum) {
    double total = 0.0;
    int count = 0;
    int step = (w > 64 || h > 64) ? 8 : 2;
    for (int sy = 0; sy < h; sy += step) {
      for (int sx = 0; sx < w; sx += step) {
        const uint8_t* px = copy + ((size_t)sy * row + (size_t)sx * 4u);
        total += 0.299 * px[0] + 0.587 * px[1] + 0.114 * px[2];
        count++;
      }
    }
    *out_avg_lum = (count > 0) ? (float)(total / (count * 255.0)) : 0.5f;
  }
  // Note: We do NOT convert to white mask here - preserve original colors
  CogitoTexture* tex = cogito_backend->texture_create(w, h, copy, 4);
  free(copy);
  SDL_DestroySurface(rgba);
  if (!tex) return false;
  if (out) *out = tex;
  if (out_w) *out_w = w;
  if (out_h) *out_h = h;
  return true;
#else
  (void)out;
  (void)out_w;
  (void)out_h;
  (void)out_avg_lum;
  return false;
#endif
}

static void cogito_load_sum_file(const char* path);
