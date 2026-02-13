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
