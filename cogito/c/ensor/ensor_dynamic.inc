// Ensor dynamic scheme — DynamicScheme-based color computation
// Faithfully ported from Ensor's DynamicColor.vala, DynamicScheme.vala,
// Scheme.vala, and all variant files (Default, Vibrant, Muted, Monochromatic, Salad, Content).
// Platform = DESKTOP, contrast_level = 0.0 throughout.

// ---- Hue classification (from HCT.vala) ----
static bool cogito_hue_is_yellow(double h) { int fh = (int)floor(h); return fh >= 105 && fh < 125; }
static bool cogito_hue_is_cyan(double h)   { int fh = (int)floor(h); return fh >= 170 && fh < 207; }
static bool cogito_hue_is_blue(double h)   { int fh = (int)floor(h); return fh >= 250 && fh < 270; }

// ---- Contrast utilities (from Contrast.vala) ----
static double cogito_dyn_ratio_of_ys(double y1, double y2) {
  double lighter = y1 > y2 ? y1 : y2;
  double darker  = y1 > y2 ? y2 : y1;
  return (lighter + 5.0) / fmax(1e-10, darker + 5.0);
}

static double cogito_dyn_ratio_of_tones(double t1, double t2) {
  return cogito_dyn_ratio_of_ys(cogito_y_from_lstar(t1), cogito_y_from_lstar(t2));
}

// Returns lighter tone achieving ratio, or -1.0 if impossible.
static double cogito_dyn_lighter_safe(double tone, double ratio) {
  if (tone < 0.0 || tone > 100.0) return -1.0;
  double dark_y  = cogito_y_from_lstar(tone);
  double light_y = ratio * (dark_y + 5.0) - 5.0;
  if (light_y < 0.0 || light_y > 100.0) return -1.0;
  double real = cogito_dyn_ratio_of_ys(light_y, dark_y);
  if (real < ratio && fabs(real - ratio) > 0.04) return -1.0;
  double rv = cogito_lstar_from_y(light_y) + 0.4;
  return (rv < 0.0 || rv > 100.0) ? -1.0 : rv;
}

static double cogito_dyn_darker_safe(double tone, double ratio) {
  if (tone < 0.0 || tone > 100.0) return -1.0;
  double light_y = cogito_y_from_lstar(tone);
  double dark_y  = (light_y + 5.0) / fmax(1e-10, ratio) - 5.0;
  if (dark_y < 0.0 || dark_y > 100.0) return -1.0;
  double real = cogito_dyn_ratio_of_ys(light_y, dark_y);
  if (real < ratio && fabs(real - ratio) > 0.04) return -1.0;
  double rv = cogito_lstar_from_y(dark_y) - 0.4;
  return (rv < 0.0 || rv > 100.0) ? -1.0 : rv;
}

static double cogito_dyn_lighter_unsafe(double tone, double ratio) {
  double t = cogito_dyn_lighter_safe(tone, ratio);
  return t < 0.0 ? 100.0 : t;
}

static double cogito_dyn_darker_unsafe(double tone, double ratio) {
  double t = cogito_dyn_darker_safe(tone, ratio);
  return t < 0.0 ? 0.0 : t;
}

static bool cogito_dyn_prefers_light_fg(double tone) {
  return round(tone) < 60.0;
}

// From DynamicColor.vala foreground_tone
static double cogito_dyn_foreground_tone(double bg, double ratio) {
  double lighter_t = cogito_dyn_lighter_unsafe(bg, ratio);
  double darker_t  = cogito_dyn_darker_unsafe(bg, ratio);
  double lighter_r = cogito_dyn_ratio_of_tones(lighter_t, bg);
  double darker_r  = cogito_dyn_ratio_of_tones(darker_t, bg);
  bool prefer_light = cogito_dyn_prefers_light_fg(bg);

  if (lighter_t > 100.0) lighter_t = 100.0;
  if (lighter_t < 0.0) lighter_t = 0.0;
  if (darker_t > 100.0) darker_t = 100.0;
  if (darker_t < 0.0) darker_t = 0.0;

  if (ratio < 1.5) {
    return prefer_light ? lighter_t : darker_t;
  }

  if (prefer_light) {
    bool neg = fabs(lighter_r - darker_r) < 0.1 && lighter_r < ratio && darker_r < ratio;
    return (lighter_r >= ratio || lighter_r >= darker_r || neg) ? lighter_t : darker_t;
  } else {
    return (darker_r >= ratio || darker_r >= lighter_r) ? darker_t : lighter_t;
  }
}

// ---- ContrastCurve (from ContrastCurve.vala) ----
static double cogito_lerp(double a, double b, double t) {
  return a + t * (b - a);
}

typedef struct {
  double low;
  double normal;
  double medium;
  double high;
} CogitoContrastCurve;

static double cogito_contrast_curve_get(CogitoContrastCurve c, double contrast) {
  if (contrast <= -1.0) {
    return c.low;
  } else if (contrast < 0.0) {
    return cogito_lerp(c.low, c.normal, contrast - (-1.0));
  } else if (contrast < 0.5) {
    return cogito_lerp(c.normal, c.medium, contrast / 0.5);
  } else if (contrast < 1.0) {
    return cogito_lerp(c.medium, c.high, (contrast - 0.5) / 0.5);
  } else {
    return c.high;
  }
}

// ---- Standard contrast curves per role ----
// ContrastCurve(low, normal, medium, high) — interpolated by contrast_level
static const CogitoContrastCurve CC_ACCENT      = { 3.0, 4.5, 7.0, 7.0 };
static const CogitoContrastCurve CC_ON_ACCENT   = { 4.5, 7.0, 11.0, 21.0 };
static const CogitoContrastCurve CC_CONTAINER   = { 1.0, 1.0, 3.0, 4.5 };
static const CogitoContrastCurve CC_ON_CONT     = { 3.0, 4.5, 7.0, 11.0 };
static const CogitoContrastCurve CC_ON_SURFACE  = { 4.5, 7.0, 11.0, 21.0 };
static const CogitoContrastCurve CC_ON_SURF_VAR = { 3.0, 4.5, 7.0, 11.0 };
static const CogitoContrastCurve CC_OUTLINE     = { 1.5, 3.0, 4.5, 7.0 };
static const CogitoContrastCurve CC_OUTLINE_VAR = { 1.0, 1.0, 3.0, 4.5 };
static const CogitoContrastCurve CC_INV_ON_SURF = { 4.5, 7.0, 11.0, 21.0 };
static const CogitoContrastCurve CC_INV_PRIMARY = { 3.0, 4.5, 7.0, 7.0 };

// ---- Tone finders (faithful port of find_best_tone_for_chroma from Scheme.vala) ----
static double cogito_dyn_find_best_tone(double hue, double chroma, double start, bool by_decreasing) {
  double answer = start;
  CogitoColor rgb = cogito_hct_to_rgb(hue, chroma, answer);
  double rh, rc, rt;
  cogito_rgb_to_hct(rgb, &rh, &rc, &rt);
  double best_c = rc;
  double tone = start;
  while (best_c < chroma) {
    if (tone < 0.0 || tone > 100.0) break;
    tone += by_decreasing ? -1.0 : 1.0;
    if (tone < 0.0 || tone > 100.0) break;
    CogitoColor cand = cogito_hct_to_rgb(hue, chroma, tone);
    double ch, cc, ct;
    cogito_rgb_to_hct(cand, &ch, &cc, &ct);
    if (cc > best_c) {
      best_c = cc;
      answer = tone;
    }
  }
  return answer;
}

// t_max_c: starts at 100, decrements → highest tone achieving target chroma
static double cogito_dyn_tmax_c_m(double hue, double chroma, double lo, double hi, double cmult) {
  double target = chroma * cmult;
  lo = lo < 0.0 ? 0.0 : (lo > 100.0 ? 100.0 : lo);
  hi = hi < lo ? lo : (hi > 100.0 ? 100.0 : hi);
  double answer = cogito_dyn_find_best_tone(hue, target, 100.0, true);
  if (answer < lo) answer = lo;
  if (answer > hi) answer = hi;
  return answer;
}

static double cogito_dyn_tmax_c(double hue, double chroma, double lo, double hi) {
  return cogito_dyn_tmax_c_m(hue, chroma, lo, hi, 1.0);
}

// t_min_c: starts at 0, increments → lowest tone achieving target chroma
static double cogito_dyn_tmin_c(double hue, double chroma, double lo, double hi) {
  lo = lo < 0.0 ? 0.0 : (lo > 100.0 ? 100.0 : lo);
  hi = hi < lo ? lo : (hi > 100.0 ? 100.0 : hi);
  double answer = cogito_dyn_find_best_tone(hue, chroma, 0.0, false);
  if (answer < lo) answer = lo;
  if (answer > hi) answer = hi;
  return answer;
}

// ---- fix_disliked (from HCT.vala) ----
// Hues in the yellowish range with moderate chroma at dark tones look "muddy".
// Fix by pushing tone to 70.
static void cogito_dyn_fix_disliked(double* h, double* c, double* t) {
  int rh = (int)round(*h);
  if (rh >= 90 && rh <= 111 && round(*c) > 16.0 && round(*t) < 65.0) {
    CogitoColor fixed = cogito_hct_to_rgb(*h, *c, 70.0);
    cogito_rgb_to_hct(fixed, h, c, t);
  }
}

// ---- Palette struct ----
typedef struct { double hue, chroma; } CogitoEnsorPal;

// ---- Dynamic scheme context ----
typedef struct {
  CogitoEnsorPal primary, secondary, tertiary, neutral, neutral_var, error;
  double src_h, src_c, src_t;
  bool dark;
  double contrast_level;
  CogitoEnsorVariant variant;
} CogitoEnsorDyn;

// ---- Correct rotation tables from Vala sources ----

// Vibrant secondary (Vibrant.vala)
static const double dyn_vib_sec_hues[] = { 0, 21, 51, 121, 158, 191, 272, 306, 360 };
static const double dyn_vib_sec_rots[] = { -160, 155, -100, 96, -96, -156, -165, -160 };
// Vibrant tertiary
static const double dyn_vib_tert_hues[] = { 0, 105, 140, 204, 253, 278, 300, 333, 360 };
static const double dyn_vib_tert_rots[] = { -165, 160, -105, 101, -101, -160, -170, -165 };
// Vibrant neutral
static const double dyn_vib_neut_hues[] = { 0, 71, 124, 253, 278, 300, 360 };
static const double dyn_vib_neut_rots[] = { 10, 0, 10, 0, 10, 0 };

// Muted tertiary (Muted.vala)
static const double dyn_mut_tert_hues[] = { 0, 38, 105, 161, 204, 278, 333, 360 };
static const double dyn_mut_tert_rots[] = { -32, 26, 10, -39, 24, -15, -32 };

// Default tertiary (Default.vala)
static const double dyn_def_tert_hues[] = { 0.0, 20.0, 71.0, 161.0, 333.0, 360.0 };
static const double dyn_def_tert_rots[] = { -40.0, 48.0, -32.0, 29.5, -32.0 };

// Error palette hue table (DynamicScheme.vala)
static const double dyn_err_hue_bps[] = { 0, 3, 13, 23, 33, 43, 153, 273, 360 };
static const double dyn_err_hue_vals[] = { 12, 22, 32, 12, 22, 32, 22, 12 };

// ---- Error palette hue from source hue ----
static double cogito_dyn_error_hue(double src_h) {
  return cogito_ensor_piecewise_val(src_h,
    dyn_err_hue_bps, sizeof(dyn_err_hue_bps)/sizeof(dyn_err_hue_bps[0]),
    dyn_err_hue_vals, sizeof(dyn_err_hue_vals)/sizeof(dyn_err_hue_vals[0]));
}

// ---- Rotated hue helper ----
static double cogito_dyn_rotated(double src_h, const double* hbps, size_t nh,
                                  const double* rots, size_t nr) {
  return cogito_ensor_rotated_hue(src_h, hbps, nh, rots, nr);
}

#define DYN_ROT(src, hbps, rots) cogito_dyn_rotated(src, hbps, sizeof(hbps)/sizeof(hbps[0]), rots, sizeof(rots)/sizeof(rots[0]))

// ---- Build DynScheme per variant (from each variant's .vala file) ----
static CogitoEnsorDyn cogito_dyn_make(double h, double c, double t, bool dark, double contrast_level, CogitoEnsorVariant variant) {
  CogitoEnsorDyn s;
  memset(&s, 0, sizeof(s));
  s.src_h = h; s.src_c = c; s.src_t = t;
  s.dark = dark; s.contrast_level = contrast_level; s.variant = variant;

  // Force monochrome if source chroma ≤ 5 (except monochrome itself, always mono)
  bool force_mono = (c <= 5.0 && variant != COGITO_ENSOR_MONOCHROME && variant != COGITO_ENSOR_CONTENT);
  if (force_mono) variant = COGITO_ENSOR_MONOCHROME;
  s.variant = variant;

  switch (variant) {
    case COGITO_ENSOR_VIBRANT: {
      s.primary   = (CogitoEnsorPal){ h, dark ? 36.0 : 48.0 };
      double sh   = DYN_ROT(h, dyn_vib_sec_hues, dyn_vib_sec_rots);
      s.secondary = (CogitoEnsorPal){ sh, dark ? 16.0 : 24.0 };
      double th2  = DYN_ROT(h, dyn_vib_tert_hues, dyn_vib_tert_rots);
      s.tertiary  = (CogitoEnsorPal){ th2, 48.0 };
      double nh2  = DYN_ROT(h, dyn_vib_neut_hues, dyn_vib_neut_rots);
      s.neutral   = (CogitoEnsorPal){ nh2, dark ? (cogito_hue_is_yellow(h) ? 6.0 : 14.0) : 18.0 };
      s.neutral_var = (CogitoEnsorPal){ h, (h >= 105.0 && h < 125.0) ? 16.0 : 23.0 };
    } break;

    case COGITO_ENSOR_MUTED: {
      bool blue = cogito_hue_is_blue(h);
      s.primary   = (CogitoEnsorPal){ h, blue ? 12.0 : 8.0 };
      s.secondary = (CogitoEnsorPal){ h, blue ? 6.0 : 4.0 };
      double th2  = DYN_ROT(h, dyn_mut_tert_hues, dyn_mut_tert_rots);
      s.tertiary  = (CogitoEnsorPal){ th2, 20.0 };
      s.neutral   = (CogitoEnsorPal){ h, 1.0 };
      s.neutral_var = (CogitoEnsorPal){ h, 2.2 };
    } break;

    case COGITO_ENSOR_MONOCHROME:
      s.primary     = (CogitoEnsorPal){ h, 0.0 };
      s.secondary   = (CogitoEnsorPal){ h, 0.0 };
      s.tertiary    = (CogitoEnsorPal){ h, 0.0 };
      s.neutral     = (CogitoEnsorPal){ h, 0.0 };
      s.neutral_var = (CogitoEnsorPal){ h, 0.0 };
      break;

    case COGITO_ENSOR_SALAD: {
      double sh = cogito_ensor_sanitize_deg(h - 50.0);
      s.primary   = (CogitoEnsorPal){ sh, 48.0 };
      s.secondary = (CogitoEnsorPal){ sh, 24.0 };
      s.tertiary  = (CogitoEnsorPal){ h, 36.0 };
      s.neutral   = (CogitoEnsorPal){ h, 10.0 };
      s.neutral_var = (CogitoEnsorPal){ h, 16.0 };
    } break;

    case COGITO_ENSOR_CONTENT: {
      s.primary   = (CogitoEnsorPal){ h, c };
      s.secondary = (CogitoEnsorPal){ h, fmax(c - 32.0, c * 0.5) };
      // Tertiary: analogous(3, 6)[2] → from_params(h + 120, c, t), then fix_disliked
      double th2 = cogito_ensor_sanitize_deg(h + 120.0);
      double tc2 = c, tt2 = t;
      // Gamut-map at the analogous hue to get actual h/c
      CogitoColor tmp = cogito_hct_to_rgb(th2, tc2, tt2);
      cogito_rgb_to_hct(tmp, &th2, &tc2, &tt2);
      cogito_dyn_fix_disliked(&th2, &tc2, &tt2);
      s.tertiary  = (CogitoEnsorPal){ th2, tc2 };
      s.neutral   = (CogitoEnsorPal){ h, c / 8.0 };
      s.neutral_var = (CogitoEnsorPal){ h, c / 8.0 + 4.0 };
    } break;

    default: { // COGITO_ENSOR_DEFAULT
      s.primary   = (CogitoEnsorPal){ h, dark ? 26.0 : 40.0 };
      double sh   = cogito_ensor_sanitize_deg(h - 14.0);
      s.secondary = (CogitoEnsorPal){ sh, 16.0 };
      double th2  = DYN_ROT(h, dyn_def_tert_hues, dyn_def_tert_rots);
      s.tertiary  = (CogitoEnsorPal){ th2, dark ? 28.0 : 48.0 };
      s.neutral   = (CogitoEnsorPal){ h, 2.0 };
      s.neutral_var = (CogitoEnsorPal){ h, 2.7 };
    } break;
  }

  // Error palette (common to all variants)
  double err_h = cogito_dyn_error_hue(h);
  s.error = (CogitoEnsorPal){ err_h, fmax(c, 60.0) };

  return s;
}

// ---- Surface tones (DESKTOP, from Scheme.vala) ----
// All surface roles use the neutral palette.

static double cogito_dyn_surface_tone(const CogitoEnsorDyn* s) {
  if (s->dark) return 4.0;
  if (cogito_hue_is_yellow(s->neutral.hue)) return 99.0;
  if (s->variant == COGITO_ENSOR_VIBRANT) return 97.0;
  return 98.0;
}

static double cogito_dyn_surface_bright_tone(const CogitoEnsorDyn* s) {
  if (s->dark) return 18.0;
  if (cogito_hue_is_yellow(s->neutral.hue)) return 99.0;
  if (s->variant == COGITO_ENSOR_VIBRANT) return 97.0;
  return 98.0;
}

static double cogito_dyn_surface_dim_tone(const CogitoEnsorDyn* s) {
  if (s->dark) return 4.0;
  if (cogito_hue_is_yellow(s->neutral.hue)) return 90.0;
  if (s->variant == COGITO_ENSOR_VIBRANT) return 85.0;
  return 87.0;
}

static double cogito_dyn_sc_lowest_tone(const CogitoEnsorDyn* s) {
  return s->dark ? 0.0 : 100.0;
}

static double cogito_dyn_sc_low_tone(const CogitoEnsorDyn* s) {
  if (s->dark) return 6.0;
  if (cogito_hue_is_yellow(s->neutral.hue)) return 98.0;
  if (s->variant == COGITO_ENSOR_VIBRANT) return 95.0;
  return 96.0;
}

static double cogito_dyn_sc_tone(const CogitoEnsorDyn* s) {
  if (s->dark) return 9.0;
  if (cogito_hue_is_yellow(s->neutral.hue)) return 96.0;
  if (s->variant == COGITO_ENSOR_VIBRANT) return 92.0;
  return 94.0;
}

static double cogito_dyn_sc_high_tone(const CogitoEnsorDyn* s) {
  if (s->dark) return 12.0;
  if (cogito_hue_is_yellow(s->neutral.hue)) return 94.0;
  if (s->variant == COGITO_ENSOR_VIBRANT) return 90.0;
  return 92.0;
}

static double cogito_dyn_sc_highest_tone(const CogitoEnsorDyn* s) {
  if (s->dark) return 15.0;
  if (cogito_hue_is_yellow(s->neutral.hue)) return 92.0;
  if (s->variant == COGITO_ENSOR_VIBRANT) return 88.0;
  return 90.0;
}

// ---- Neutral chromamult helpers (DESKTOP, from Scheme.vala) ----
// surface_bright: chromamult only in dark mode
static double cogito_dyn_sbright_cmult(const CogitoEnsorDyn* s) {
  if (s->dark) {
    if (s->variant == COGITO_ENSOR_MUTED) return 2.5;
    if (s->variant == COGITO_ENSOR_DEFAULT) return 1.7;
    if (s->variant == COGITO_ENSOR_SALAD)
      return cogito_hue_is_yellow(s->neutral.hue) ? 2.7 : 1.75;
    if (s->variant == COGITO_ENSOR_VIBRANT) return 1.36;
  }
  return 1.0;
}

// surface_dim: chromamult only in light mode
static double cogito_dyn_sdim_cmult(const CogitoEnsorDyn* s) {
  if (!s->dark) {
    if (s->variant == COGITO_ENSOR_MUTED) return 2.5;
    if (s->variant == COGITO_ENSOR_DEFAULT) return 1.7;
    if (s->variant == COGITO_ENSOR_SALAD)
      return cogito_hue_is_yellow(s->neutral.hue) ? 2.7 : 1.75;
    if (s->variant == COGITO_ENSOR_VIBRANT) return 1.36;
  }
  return 1.0;
}

// surface_container_low
static double cogito_dyn_scl_cmult(const CogitoEnsorDyn* s) {
  if (s->variant == COGITO_ENSOR_MUTED) return 1.3;
  if (s->variant == COGITO_ENSOR_DEFAULT) return 1.25;
  if (s->variant == COGITO_ENSOR_SALAD)
    return cogito_hue_is_yellow(s->neutral.hue) ? 1.3 : 1.15;
  if (s->variant == COGITO_ENSOR_VIBRANT) return 1.08;
  return 1.0;
}

// surface_container
static double cogito_dyn_sc_cmult(const CogitoEnsorDyn* s) {
  if (s->variant == COGITO_ENSOR_MUTED) return 1.6;
  if (s->variant == COGITO_ENSOR_DEFAULT) return 1.4;
  if (s->variant == COGITO_ENSOR_SALAD)
    return cogito_hue_is_yellow(s->neutral.hue) ? 1.6 : 1.3;
  if (s->variant == COGITO_ENSOR_VIBRANT) return 1.15;
  return 1.0;
}

// surface_container_high
static double cogito_dyn_sch_cmult(const CogitoEnsorDyn* s) {
  if (s->variant == COGITO_ENSOR_MUTED) return 1.9;
  if (s->variant == COGITO_ENSOR_DEFAULT) return 1.5;
  if (s->variant == COGITO_ENSOR_SALAD)
    return cogito_hue_is_yellow(s->neutral.hue) ? 1.95 : 1.45;
  if (s->variant == COGITO_ENSOR_VIBRANT) return 1.22;
  return 1.0;
}

// surface_container_highest (same as on_surface/outline/on_surface_variant neutral mult)
static double cogito_dyn_schi_cmult(const CogitoEnsorDyn* s) {
  if (s->variant == COGITO_ENSOR_MUTED) return 2.2;
  if (s->variant == COGITO_ENSOR_DEFAULT) return 1.7;
  if (s->variant == COGITO_ENSOR_SALAD)
    return cogito_hue_is_yellow(s->neutral.hue) ? 2.3 : 1.6;
  if (s->variant == COGITO_ENSOR_VIBRANT) return 1.29;
  return 1.0;
}

// on_surface, on_surface_variant, outline, outline_variant neutral chromamult
static double cogito_dyn_neut_cmult(const CogitoEnsorDyn* s) {
  if (s->variant == COGITO_ENSOR_MUTED) return 2.2;
  if (s->variant == COGITO_ENSOR_DEFAULT) return 1.7;
  if (s->variant == COGITO_ENSOR_SALAD)
    return cogito_hue_is_yellow(s->neutral.hue) ? (s->dark ? 3.0 : 2.3) : 1.6;
  return 1.0;
}

// ---- Render color from palette with chromamult ----
static CogitoColor cogito_dyn_pal_color(CogitoEnsorPal p, double tone, double cmult) {
  return cogito_hct_to_rgb(p.hue, p.chroma * cmult, tone);
}

// ---- Snap background tones (from DynamicColor.vala) ----
// Avoids tones 50-64 for is_background colors.
static double cogito_dyn_snap_bg(double tone) {
  if (tone >= 57.0) {
    return tone < 65.0 ? 65.0 : tone;
  } else {
    return tone > 49.0 ? 49.0 : tone;
  }
}

// ---- Resolve contrast against a background ----
// If current ratio is insufficient, use foreground_tone.
static double cogito_dyn_ensure_contrast(double self_tone, double bg_tone, double ratio) {
  if (cogito_dyn_ratio_of_tones(bg_tone, self_tone) >= ratio) return self_tone;
  return cogito_dyn_foreground_tone(bg_tone, ratio);
}

// ---- finalize_tone_for_light_on_roles (from DynamicColor.vala) ----
// Forces tone=100 for on_primary, on_secondary, on_tertiary, on_error,
// inverse_on_surface in light mode.
// Caller passes is_forced=true for those roles.
static double cogito_dyn_finalize_light_on(double tone, bool dark, bool is_forced) {
  if (!dark && is_forced) return 100.0;
  return tone;
}

// ---- TDP FARTHER RELATIVE_LIGHTER for accent roles ----
// Given accent's initial tone and container's initial tone, apply TDP.
// Returns adjusted accent tone (container tone is NOT adjusted by TDP).
static double cogito_dyn_tdp_accent(double accent_init, double container_init, double delta, bool dark) {
  // absolute_delta = dark ? -delta : +delta (RELATIVE_LIGHTER)
  // For accent (role_b), relative_delta = absolute_delta * -1
  double rel = dark ? delta : -delta;
  if (rel > 0) {
    // FARTHER: self_tone = max(container_init + rel, accent_init)
    double bound = container_init + rel;
    return accent_init > bound ? accent_init : bound;
  } else {
    // FARTHER: self_tone = min(container_init + rel, accent_init)
    double bound = container_init + rel;
    return accent_init < bound ? accent_init : bound;
  }
}

// ---- Primary role tones (from Scheme.vala) ----

static double cogito_dyn_primary_container_init(const CogitoEnsorDyn* s) {
  const CogitoEnsorPal* p = &s->primary;
  switch (s->variant) {
    case COGITO_ENSOR_CONTENT: return s->src_t;
    case COGITO_ENSOR_MONOCHROME: return s->dark ? 85.0 : 25.0;
    case COGITO_ENSOR_MUTED: return s->dark ? 30.0 : 90.0;
    case COGITO_ENSOR_DEFAULT:
      return s->dark ? 25.0 : cogito_dyn_tmax_c(p->hue, p->chroma, 0, 100);
    case COGITO_ENSOR_SALAD:
      return s->dark ? cogito_dyn_tmax_c(p->hue, p->chroma, 30, 93)
                     : cogito_dyn_tmax_c(p->hue, p->chroma, 78,
                         cogito_hue_is_cyan(p->hue) ? 88 : 90);
    default: // VIBRANT + others
      return s->dark ? cogito_dyn_tmin_c(p->hue, p->chroma, 66, 93)
                     : cogito_dyn_tmax_c(p->hue, p->chroma, 66,
                         cogito_hue_is_cyan(p->hue) ? 88 : 93);
  }
}

static double cogito_dyn_primary_init(const CogitoEnsorDyn* s) {
  const CogitoEnsorPal* p = &s->primary;
  switch (s->variant) {
    case COGITO_ENSOR_MONOCHROME: return s->dark ? 100.0 : 0.0;
    case COGITO_ENSOR_MUTED: return s->dark ? 80.0 : 40.0;
    case COGITO_ENSOR_DEFAULT: return s->dark ? 80.0 : 40.0;
    case COGITO_ENSOR_SALAD:
      return cogito_dyn_tmax_c(p->hue, p->chroma, 0,
        cogito_hue_is_yellow(p->hue) ? 25 : (cogito_hue_is_cyan(p->hue) ? 88 : 98));
    default: // VIBRANT + others
      return cogito_dyn_tmax_c(p->hue, p->chroma, 0,
        cogito_hue_is_cyan(p->hue) ? 88 : 98);
  }
}

static double cogito_dyn_on_primary_init(const CogitoEnsorDyn* s) {
  if (s->variant == COGITO_ENSOR_MONOCHROME) return s->dark ? 10.0 : 90.0;
  return s->dark ? 10.0 : 100.0;
}

static double cogito_dyn_on_primary_container_init(const CogitoEnsorDyn* s) {
  if (s->variant == COGITO_ENSOR_MONOCHROME) return s->dark ? 0.0 : 100.0;
  return s->dark ? 90.0 : 30.0;
}

// ---- Secondary role tones ----

static double cogito_dyn_secondary_container_init(const CogitoEnsorDyn* s) {
  const CogitoEnsorPal* p = &s->secondary;
  switch (s->variant) {
    case COGITO_ENSOR_MONOCHROME: return s->dark ? 30.0 : 85.0;
    case COGITO_ENSOR_CONTENT: {
      double init = s->dark ? 30.0 : 90.0;
      return cogito_dyn_find_best_tone(p->hue, p->chroma, init, !s->dark);
    }
    case COGITO_ENSOR_DEFAULT: return s->dark ? 25.0 : 90.0;
    case COGITO_ENSOR_MUTED:
      return s->dark ? cogito_dyn_tmax_c(p->hue, p->chroma, 20, 30)
                     : cogito_dyn_tmin_c(p->hue, p->chroma, 85, 95);
    case COGITO_ENSOR_VIBRANT:
      return s->dark ? cogito_dyn_tmin_c(p->hue, p->chroma, 30, 40)
                     : cogito_dyn_tmax_c(p->hue, p->chroma, 84, 90);
    case COGITO_ENSOR_SALAD:
      return s->dark ? 15.0 : cogito_dyn_tmax_c(p->hue, p->chroma, 90, 95);
    default: return s->dark ? 25.0 : 90.0;
  }
}

static double cogito_dyn_secondary_init(const CogitoEnsorDyn* s) {
  const CogitoEnsorPal* p = &s->secondary;
  switch (s->variant) {
    case COGITO_ENSOR_MONOCHROME: return s->dark ? 80.0 : 40.0;
    case COGITO_ENSOR_DEFAULT:
      return s->dark ? 80.0 : cogito_dyn_tmax_c(p->hue, p->chroma, 0, 100);
    case COGITO_ENSOR_MUTED:
      return s->dark ? cogito_dyn_tmin_c(p->hue, p->chroma, 0, 98)
                     : cogito_dyn_tmax_c(p->hue, p->chroma, 0, 100);
    case COGITO_ENSOR_SALAD:
      return cogito_dyn_tmax_c(p->hue, p->chroma, 0, s->dark ? 90 : 98);
    default: // VIBRANT + others
      return s->dark ? cogito_dyn_tmax_c(p->hue, p->chroma, 75, 85)
                     : cogito_dyn_tmax_c(p->hue, p->chroma, 35, 45);
  }
}

static double cogito_dyn_on_secondary_init(const CogitoEnsorDyn* s) {
  (void)s;
  return s->dark ? 10.0 : 100.0;
}

static double cogito_dyn_on_secondary_container_init(const CogitoEnsorDyn* s) {
  if (s->variant == COGITO_ENSOR_MONOCHROME) return s->dark ? 90.0 : 10.0;
  return s->dark ? 90.0 : 30.0;
}

// ---- Tertiary role tones ----

static double cogito_dyn_tertiary_container_init(const CogitoEnsorDyn* s) {
  const CogitoEnsorPal* p = &s->tertiary;
  switch (s->variant) {
    case COGITO_ENSOR_MONOCHROME: return s->dark ? 60.0 : 49.0;
    case COGITO_ENSOR_CONTENT: {
      // fix_disliked on tertiary palette at source tone
      double th = p->hue, tc = p->chroma, tt = s->src_t;
      CogitoColor tmp = cogito_hct_to_rgb(th, tc, tt);
      cogito_rgb_to_hct(tmp, &th, &tc, &tt);
      cogito_dyn_fix_disliked(&th, &tc, &tt);
      return tt < 1.0 ? (s->dark ? 30.0 : 90.0) : tt;
    }
    case COGITO_ENSOR_DEFAULT: return s->dark ? 25.0 : 81.0;
    case COGITO_ENSOR_MUTED:
      return s->dark ? cogito_dyn_tmax_c(p->hue, p->chroma, 0, 93)
                     : cogito_dyn_tmax_c(p->hue, p->chroma, 0, 96);
    case COGITO_ENSOR_SALAD:
      return cogito_dyn_tmax_c(p->hue, p->chroma, 75,
        cogito_hue_is_cyan(p->hue) ? 88 : (s->dark ? 93 : 100));
    default: // VIBRANT
      return s->dark ? cogito_dyn_tmax_c(p->hue, p->chroma, 0, 93)
                     : cogito_dyn_tmax_c(p->hue, p->chroma, 72, 100);
  }
}

static double cogito_dyn_tertiary_init(const CogitoEnsorDyn* s) {
  const CogitoEnsorPal* p = &s->tertiary;
  switch (s->variant) {
    case COGITO_ENSOR_MONOCHROME: return s->dark ? 90.0 : 25.0;
    case COGITO_ENSOR_CONTENT: return s->dark ? 80.0 : 40.0;
    case COGITO_ENSOR_DEFAULT:
      return s->dark ? cogito_dyn_tmax_c(p->hue, p->chroma, 0, 98)
                     : cogito_dyn_tmax_c(p->hue, p->chroma, 0, 100);
    case COGITO_ENSOR_MUTED:
    case COGITO_ENSOR_SALAD:
      return cogito_dyn_tmax_c(p->hue, p->chroma, 0,
        cogito_hue_is_cyan(p->hue) ? 88 : (s->dark ? 98 : 100));
    default: // VIBRANT
      return s->dark ? cogito_dyn_tmax_c(p->hue, p->chroma, 75, 85)
                     : cogito_dyn_tmax_c(p->hue, p->chroma, 35, 45);
  }
}

static double cogito_dyn_on_tertiary_init(const CogitoEnsorDyn* s) {
  if (s->variant == COGITO_ENSOR_MONOCHROME) return s->dark ? 10.0 : 90.0;
  return s->dark ? 20.0 : 100.0;
}

static double cogito_dyn_on_tertiary_container_init(const CogitoEnsorDyn* s) {
  if (s->variant == COGITO_ENSOR_MONOCHROME) return s->dark ? 0.0 : 100.0;
  return s->dark ? 90.0 : 30.0;
}

// ---- Error role tones ----

static double cogito_dyn_error_init(const CogitoEnsorDyn* s) {
  const CogitoEnsorPal* p = &s->error;
  return s->dark ? cogito_dyn_tmin_c(p->hue, p->chroma, 0, 98)
                 : cogito_dyn_tmax_c(p->hue, p->chroma, 0, 100);
}

static double cogito_dyn_error_container_init(const CogitoEnsorDyn* s) {
  const CogitoEnsorPal* p = &s->error;
  if (s->dark) return cogito_dyn_tmin_c(p->hue, p->chroma, 30, 93);
  if (s->variant == COGITO_ENSOR_DEFAULT) return 90.0;
  return cogito_dyn_tmax_c(p->hue, p->chroma, 0, 90);
}

static double cogito_dyn_on_error_init(const CogitoEnsorDyn* s) {
  return s->dark ? 10.0 : 100.0;
}

// ---- Full accent role resolution ----
// Applies TDP + contrast + snap + finalize for primary/secondary/tertiary/error.
// bg_tone = surface_bright (dark) or surface_dim (light).

typedef struct {
  double accent, container, on_accent, on_container;
} CogitoEnsorRoleTones;

static CogitoEnsorRoleTones cogito_dyn_resolve_accent(
    double accent_init, double container_init,
    double on_accent_init, double on_container_init,
    double bg_tone, bool dark,
    double container_ratio, double accent_ratio,
    double on_accent_ratio, double on_container_ratio,
    bool on_accent_forced_white)
{
  CogitoEnsorRoleTones r;

  // --- Container: snap for is_background, contrast if ratio > 1 ---
  r.container = cogito_dyn_snap_bg(container_init);
  if (container_ratio > 1.0)
    r.container = cogito_dyn_ensure_contrast(r.container, bg_tone, container_ratio);

  // --- Accent: TDP(container, accent, 5, RELATIVE_LIGHTER, FARTHER) ---
  double acc = cogito_dyn_tdp_accent(accent_init, container_init, 5.0, dark);
  acc = cogito_dyn_ensure_contrast(acc, bg_tone, accent_ratio);
  acc = cogito_dyn_snap_bg(acc);
  r.accent = acc;

  // --- on_accent: contrast against accent ---
  double on_acc = on_accent_init;
  on_acc = cogito_dyn_ensure_contrast(on_acc, r.accent, on_accent_ratio);
  on_acc = cogito_dyn_finalize_light_on(on_acc, dark, on_accent_forced_white);
  r.on_accent = on_acc;

  // --- on_container: contrast against container ---
  double on_cont = on_container_init;
  on_cont = cogito_dyn_ensure_contrast(on_cont, r.container, on_container_ratio);
  r.on_container = on_cont;

  return r;
}

// Special version for Content variant where on_container uses foreground_tone
static CogitoEnsorRoleTones cogito_dyn_resolve_accent_content(
    double accent_init, double container_init,
    double on_accent_init,
    double bg_tone, bool dark,
    double container_ratio, double accent_ratio,
    double on_accent_ratio, double on_container_ratio,
    bool on_accent_forced_white)
{
  CogitoEnsorRoleTones r;

  // Container: snap, contrast if ratio > 1
  r.container = cogito_dyn_snap_bg(container_init);
  if (container_ratio > 1.0)
    r.container = cogito_dyn_ensure_contrast(r.container, bg_tone, container_ratio);

  // Accent with TDP
  double acc = cogito_dyn_tdp_accent(accent_init, container_init, 5.0, dark);
  acc = cogito_dyn_ensure_contrast(acc, bg_tone, accent_ratio);
  acc = cogito_dyn_snap_bg(acc);
  r.accent = acc;

  // on_accent
  double on_acc = on_accent_init;
  on_acc = cogito_dyn_ensure_contrast(on_acc, r.accent, on_accent_ratio);
  on_acc = cogito_dyn_finalize_light_on(on_acc, dark, on_accent_forced_white);
  r.on_accent = on_acc;

  // on_container: foreground_tone against container
  r.on_container = cogito_dyn_foreground_tone(r.container, on_container_ratio);

  return r;
}

// ---- Main build function ----

static CogitoEnsorScheme cogito_ensor_build_dynamic_scheme(
    bool dark, double h, double c, double t,
    CogitoEnsorVariant variant, double contrast_level)
{
  CogitoEnsorScheme out;
  memset(&out, 0, sizeof(out));
  out.dark = dark;

  CogitoEnsorDyn s = cogito_dyn_make(h, c, t, dark, contrast_level, variant);
  double cl = s.contrast_level;

  // Evaluate contrast curves once
  double r_container  = cogito_contrast_curve_get(CC_CONTAINER, cl);
  double r_accent     = cogito_contrast_curve_get(CC_ACCENT, cl);
  double r_on_accent  = cogito_contrast_curve_get(CC_ON_ACCENT, cl);
  double r_on_cont    = cogito_contrast_curve_get(CC_ON_CONT, cl);
  double r_on_surface = cogito_contrast_curve_get(CC_ON_SURFACE, cl);
  double r_on_sv      = cogito_contrast_curve_get(CC_ON_SURF_VAR, cl);
  double r_outline    = cogito_contrast_curve_get(CC_OUTLINE, cl);
  double r_outline_v  = cogito_contrast_curve_get(CC_OUTLINE_VAR, cl);
  double r_inv_on_s   = cogito_contrast_curve_get(CC_INV_ON_SURF, cl);
  double r_inv_pri    = cogito_contrast_curve_get(CC_INV_PRIMARY, cl);

  // Background tone for contrast enforcement (DESKTOP):
  // accent roles use surface_bright (dark) / surface_dim (light)
  double bg_tone = dark ? cogito_dyn_surface_bright_tone(&s)
                        : cogito_dyn_surface_dim_tone(&s);

  // ---- Primary ----
  {
    double pc_init  = cogito_dyn_primary_container_init(&s);
    double p_init   = cogito_dyn_primary_init(&s);
    double op_init  = cogito_dyn_on_primary_init(&s);
    double opc_init = cogito_dyn_on_primary_container_init(&s);

    CogitoEnsorRoleTones rt;
    if (s.variant == COGITO_ENSOR_CONTENT) {
      rt = cogito_dyn_resolve_accent_content(p_init, pc_init, op_init,
        bg_tone, dark, r_container, r_accent, r_on_accent, r_on_cont, true);
    } else {
      rt = cogito_dyn_resolve_accent(p_init, pc_init, op_init, opc_init,
        bg_tone, dark, r_container, r_accent, r_on_accent, r_on_cont, true);
    }

    out.primary              = cogito_dyn_pal_color(s.primary, rt.accent, 1.0);
    out.primary_container    = cogito_dyn_pal_color(s.primary, rt.container, 1.0);
    out.on_primary           = cogito_dyn_pal_color(s.primary, rt.on_accent, 1.0);
    out.on_primary_container = cogito_dyn_pal_color(s.primary, rt.on_container, 1.0);
  }

  // ---- Secondary ----
  {
    double sc_init  = cogito_dyn_secondary_container_init(&s);
    double se_init  = cogito_dyn_secondary_init(&s);
    double os_init  = cogito_dyn_on_secondary_init(&s);
    double osc_init = cogito_dyn_on_secondary_container_init(&s);

    CogitoEnsorRoleTones rt;
    if (s.variant == COGITO_ENSOR_CONTENT) {
      rt = cogito_dyn_resolve_accent_content(se_init, sc_init, os_init,
        bg_tone, dark, r_container, r_accent, r_on_accent, r_on_cont, true);
    } else {
      rt = cogito_dyn_resolve_accent(se_init, sc_init, os_init, osc_init,
        bg_tone, dark, r_container, r_accent, r_on_accent, r_on_cont, true);
    }

    out.secondary              = cogito_dyn_pal_color(s.secondary, rt.accent, 1.0);
    out.secondary_container    = cogito_dyn_pal_color(s.secondary, rt.container, 1.0);
    out.on_secondary           = cogito_dyn_pal_color(s.secondary, rt.on_accent, 1.0);
    out.on_secondary_container = cogito_dyn_pal_color(s.secondary, rt.on_container, 1.0);
  }

  // ---- Tertiary ----
  {
    double tc_init  = cogito_dyn_tertiary_container_init(&s);
    double te_init  = cogito_dyn_tertiary_init(&s);
    double ot_init  = cogito_dyn_on_tertiary_init(&s);
    double otc_init = cogito_dyn_on_tertiary_container_init(&s);

    CogitoEnsorRoleTones rt;
    if (s.variant == COGITO_ENSOR_CONTENT) {
      rt = cogito_dyn_resolve_accent_content(te_init, tc_init, ot_init,
        bg_tone, dark, r_container, r_accent, r_on_accent, r_on_cont, true);
    } else {
      rt = cogito_dyn_resolve_accent(te_init, tc_init, ot_init, otc_init,
        bg_tone, dark, r_container, r_accent, r_on_accent, r_on_cont, true);
    }

    out.tertiary              = cogito_dyn_pal_color(s.tertiary, rt.accent, 1.0);
    out.tertiary_container    = cogito_dyn_pal_color(s.tertiary, rt.container, 1.0);
    out.on_tertiary           = cogito_dyn_pal_color(s.tertiary, rt.on_accent, 1.0);
    out.on_tertiary_container = cogito_dyn_pal_color(s.tertiary, rt.on_container, 1.0);
  }

  // ---- Error ----
  {
    double ec_init = cogito_dyn_error_container_init(&s);
    double er_init = cogito_dyn_error_init(&s);
    double oe_init = cogito_dyn_on_error_init(&s);

    // error_container: snap + contrast if ratio > 1
    double ec_tone = cogito_dyn_snap_bg(ec_init);
    if (r_container > 1.0)
      ec_tone = cogito_dyn_ensure_contrast(ec_tone, bg_tone, r_container);

    // error: TDP(error_container, error, 5, RELATIVE_LIGHTER, FARTHER)
    double er_tone = cogito_dyn_tdp_accent(er_init, ec_init, 5.0, dark);
    er_tone = cogito_dyn_ensure_contrast(er_tone, bg_tone, r_accent);
    er_tone = cogito_dyn_snap_bg(er_tone);

    // on_error: contrast against error
    double oe_tone = oe_init;
    oe_tone = cogito_dyn_ensure_contrast(oe_tone, er_tone, r_on_accent);
    oe_tone = cogito_dyn_finalize_light_on(oe_tone, dark, true);

    // on_error_container: foreground_tone against error_container
    double oec_tone = cogito_dyn_foreground_tone(ec_tone, r_on_cont);

    out.error              = cogito_dyn_pal_color(s.error, er_tone, 1.0);
    out.error_container    = cogito_dyn_pal_color(s.error, ec_tone, 1.0);
    out.on_error           = cogito_dyn_pal_color(s.error, oe_tone, 1.0);
    out.on_error_container = cogito_dyn_pal_color(s.error, oec_tone, 1.0);
  }

  // ---- Surface roles (all use neutral palette, no contrast curves) ----
  {
    double surf_t     = cogito_dyn_surface_tone(&s);
    double sbright_t  = cogito_dyn_surface_bright_tone(&s);
    double sdim_t     = cogito_dyn_surface_dim_tone(&s);
    double scl_t      = cogito_dyn_sc_lowest_tone(&s);
    double sclo_t     = cogito_dyn_sc_low_tone(&s);
    double sc_t       = cogito_dyn_sc_tone(&s);
    double sch_t      = cogito_dyn_sc_high_tone(&s);
    double schi_t     = cogito_dyn_sc_highest_tone(&s);

    out.surface                  = cogito_dyn_pal_color(s.neutral, surf_t, 1.0);
    out.surface_bright           = cogito_dyn_pal_color(s.neutral, sbright_t, cogito_dyn_sbright_cmult(&s));
    out.surface_dim              = cogito_dyn_pal_color(s.neutral, sdim_t, cogito_dyn_sdim_cmult(&s));
    out.surface_container_lowest = cogito_dyn_pal_color(s.neutral, scl_t, 1.0);
    out.surface_container_low    = cogito_dyn_pal_color(s.neutral, sclo_t, cogito_dyn_scl_cmult(&s));
    out.surface_container        = cogito_dyn_pal_color(s.neutral, sc_t, cogito_dyn_sc_cmult(&s));
    out.surface_container_high   = cogito_dyn_pal_color(s.neutral, sch_t, cogito_dyn_sch_cmult(&s));
    out.surface_container_highest= cogito_dyn_pal_color(s.neutral, schi_t, cogito_dyn_schi_cmult(&s));

    out.background    = out.surface;
    out.surface_variant = out.surface_container_highest;
  }

  // ---- on_surface (neutral palette, contrast against surface_bright/dim) ----
  {
    double neut_cm = cogito_dyn_neut_cmult(&s);
    double init_tone;
    if (s.variant == COGITO_ENSOR_VIBRANT) {
      init_tone = cogito_dyn_tmax_c_m(s.neutral.hue, s.neutral.chroma, 0, 100, 1.1);
    } else {
      init_tone = bg_tone;
    }
    double ons_tone = cogito_dyn_ensure_contrast(init_tone, bg_tone, r_on_surface);
    out.on_surface = cogito_dyn_pal_color(s.neutral, ons_tone, neut_cm);
    out.on_background = out.on_surface;
  }

  // ---- on_surface_variant ----
  {
    double neut_cm = cogito_dyn_neut_cmult(&s);
    double osv_tone = cogito_dyn_ensure_contrast(bg_tone, bg_tone, r_on_sv);
    out.on_surface_variant = cogito_dyn_pal_color(s.neutral, osv_tone, neut_cm);
  }

  // ---- outline ----
  {
    double neut_cm = cogito_dyn_neut_cmult(&s);
    double out_tone = cogito_dyn_ensure_contrast(bg_tone, bg_tone, r_outline);
    out.outline = cogito_dyn_pal_color(s.neutral, out_tone, neut_cm);
  }

  // ---- outline_variant ----
  {
    double neut_cm = cogito_dyn_neut_cmult(&s);
    double outv_tone = cogito_dyn_ensure_contrast(bg_tone, bg_tone, r_outline_v);
    out.outline_variant = cogito_dyn_pal_color(s.neutral, outv_tone, neut_cm);
  }

  // ---- inverse_surface (neutral, fixed tones, no contrast curve) ----
  {
    double inv_tone = dark ? 98.0 : 4.0;
    out.inverse_surface = cogito_dyn_pal_color(s.neutral, inv_tone, 1.0);
  }

  // ---- inverse_on_surface ----
  {
    double inv_bg = dark ? 98.0 : 4.0;
    double inv_ons = cogito_dyn_ensure_contrast(inv_bg, inv_bg, r_inv_on_s);
    inv_ons = cogito_dyn_finalize_light_on(inv_ons, dark, true);
    out.inverse_on_surface = cogito_dyn_pal_color(s.neutral, inv_ons, 1.0);
  }

  // ---- inverse_primary ----
  {
    double inv_bg = dark ? 98.0 : 4.0;
    double ip_init = cogito_dyn_tmax_c(s.primary.hue, s.primary.chroma, 0, 100);
    double ip_tone = cogito_dyn_ensure_contrast(ip_init, inv_bg, r_inv_pri);
    out.inverse_primary = cogito_dyn_pal_color(s.primary, ip_tone, 1.0);
  }

  // ---- surface_tint = primary color (same as primary) ----
  out.surface_tint = out.primary;

  // ---- shadow / scrim ----
  out.shadow = cogito_hct_to_rgb(s.neutral.hue, 0.0, 0.0);
  out.scrim  = cogito_hct_to_rgb(s.neutral.hue, 0.0, 0.0);

  return out;
}
