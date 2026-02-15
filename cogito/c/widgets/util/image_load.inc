// Image loading (preserves original colors, no white mask conversion)

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

// --- Image texture cache: deduplicate loads of the same file path ---
typedef struct {
  char* path;
  CogitoTexture* texture;
  int w, h;
  float avg_lum;
} CogitoImageCacheEntry;

#define COGITO_IMAGE_CACHE_MAX 64
static CogitoImageCacheEntry cogito_image_cache[COGITO_IMAGE_CACHE_MAX];
static int cogito_image_cache_count = 0;

static CogitoImageCacheEntry* cogito_image_cache_find(const char* resolved) {
  for (int i = 0; i < cogito_image_cache_count; i++) {
    if (cogito_image_cache[i].path && strcmp(cogito_image_cache[i].path, resolved) == 0) {
      return &cogito_image_cache[i];
    }
  }
  return NULL;
}

static void cogito_image_cache_add(const char* resolved, CogitoTexture* tex, int w, int h, float avg_lum) {
  if (cogito_image_cache_count >= COGITO_IMAGE_CACHE_MAX) return;
  CogitoImageCacheEntry* e = &cogito_image_cache[cogito_image_cache_count++];
  e->path = strdup(resolved);
  e->texture = tex;
  e->w = w;
  e->h = h;
  e->avg_lum = avg_lum;
}

__attribute__((unused))
static void cogito_image_cache_clear(void) {
  for (int i = 0; i < cogito_image_cache_count; i++) {
    free(cogito_image_cache[i].path);
    cogito_image_cache[i].path = NULL;
    // Textures are owned by the backend; nodes still reference them.
    // Only call this at shutdown when all nodes are gone.
  }
  cogito_image_cache_count = 0;
}

static bool cogito_image_load_file_texture(const char* path, CogitoTexture** out, int* out_w, int* out_h, float* out_avg_lum) {
  if (!path || !path[0] || !cogito_backend || !cogito_backend->texture_create) return false;

  // Resolve the path
  char resolved_path[PATH_MAX];
  if (!cogito_image_resolve_path(path, resolved_path, sizeof(resolved_path))) {
    return false;
  }

  // Check cache first
  CogitoImageCacheEntry* cached = cogito_image_cache_find(resolved_path);
  if (cached && cached->texture) {
    if (out) *out = cached->texture;
    if (out_w) *out_w = cached->w;
    if (out_h) *out_h = cached->h;
    if (out_avg_lum) *out_avg_lum = cached->avg_lum;
    return true;
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
  // Cap texture dimensions — no point uploading > 2048px per side when the
  // display is smaller. Halve repeatedly until within budget. This turns a
  // 4096×4096 (64 MB) texture into 2048×2048 (16 MB).
  #define COGITO_IMAGE_MAX_DIM 2048
  if (rgba->w > COGITO_IMAGE_MAX_DIM || rgba->h > COGITO_IMAGE_MAX_DIM) {
    int dw = rgba->w, dh = rgba->h;
    while (dw > COGITO_IMAGE_MAX_DIM || dh > COGITO_IMAGE_MAX_DIM) {
      dw /= 2; dh /= 2;
    }
    if (dw < 1) dw = 1;
    if (dh < 1) dh = 1;
    SDL_Surface* scaled = SDL_CreateSurface(dw, dh, SDL_PIXELFORMAT_RGBA32);
    if (scaled) {
      SDL_BlitSurfaceScaled(rgba, NULL, scaled, NULL, SDL_SCALEMODE_LINEAR);
      SDL_DestroySurface(rgba);
      rgba = scaled;
    }
  }

  int w = rgba->w;
  int h = rgba->h;
  size_t row = (size_t)w * 4u;
  // Compute average luminance directly from the SDL surface (no extra copy).
  float avg_lum = 0.5f;
  {
    const uint8_t* px_base = (const uint8_t*)rgba->pixels;
    int pitch = rgba->pitch;
    double total = 0.0;
    int count = 0;
    int step = (w > 64 || h > 64) ? 8 : 2;
    for (int sy = 0; sy < h; sy += step) {
      for (int sx = 0; sx < w; sx += step) {
        const uint8_t* px = px_base + (size_t)sy * (size_t)pitch + (size_t)sx * 4u;
        total += 0.299 * px[0] + 0.587 * px[1] + 0.114 * px[2];
        count++;
      }
    }
    avg_lum = (count > 0) ? (float)(total / (count * 255.0)) : 0.5f;
  }
  // If pitch matches expected row stride, pass surface pixels directly to
  // texture_create (channels==4 path avoids another copy). Otherwise copy
  // row-by-row into a contiguous buffer.
  CogitoTexture* tex = NULL;
  if ((size_t)rgba->pitch == row) {
    tex = cogito_backend->texture_create(w, h, (const uint8_t*)rgba->pixels, 4);
  } else {
    uint8_t* packed = (uint8_t*)malloc(row * (size_t)h);
    if (packed) {
      const uint8_t* sp = (const uint8_t*)rgba->pixels;
      for (int y = 0; y < h; y++) {
        memcpy(packed + (size_t)y * row, sp + (size_t)y * (size_t)rgba->pitch, row);
      }
      tex = cogito_backend->texture_create(w, h, packed, 4);
      free(packed);
    }
  }
  SDL_DestroySurface(rgba);
  if (!tex) return false;

  // After large image decode, nudge malloc to return freed pages to the OS.
  // Without this, the decode buffers (potentially 64 MB+) stay mapped even
  // after free(), inflating RSS.
#if defined(__APPLE__)
  extern size_t malloc_zone_pressure_relief(void*, size_t);
  malloc_zone_pressure_relief(NULL, 0);
#endif

  // Add to cache
  cogito_image_cache_add(resolved_path, tex, w, h, avg_lum);

  if (out) *out = tex;
  if (out_w) *out_w = w;
  if (out_h) *out_h = h;
  if (out_avg_lum) *out_avg_lum = avg_lum;
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
