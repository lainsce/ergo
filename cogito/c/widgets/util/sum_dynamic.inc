// Dynamic theme generator
// Builds SUM source from accent color and dark/light mode.

static void cogito_sum_append(char* dst, size_t dst_cap, size_t* dst_off, const char* fmt, ...) {
  if (!dst || !dst_off || *dst_off >= dst_cap) return;
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(dst + *dst_off, dst_cap - *dst_off, fmt, ap);
  va_end(ap);
  if (n < 0) return;
  if ((size_t)n >= dst_cap - *dst_off) {
    *dst_off = dst_cap - 1;
  } else {
    *dst_off += (size_t)n;
  }
}

static double cogito_sum_sanitize_degrees(double deg) {
  deg = fmod(deg,
  360.0);
  if (deg < 0.0) deg += 360.0;
  return deg;
}

static double cogito_sum_piecewise_val(double src_h,
                                       const double* huebps, size_t huebps_len,
                                       const double* vals, size_t vals_len) {
  if (!huebps || !vals || huebps_len < 2 || vals_len < 1) {
    return cogito_sum_sanitize_degrees(src_h);
  }

  size_t size = huebps_len - 1;
  if (vals_len < size) size = vals_len;
  for (size_t i = 0; i < size; i++) {
    if (src_h >= huebps[i
    ] && src_h < huebps[i + 1
    ]) {
      return cogito_sum_sanitize_degrees(vals[i
      ]);
    }
  }
  return cogito_sum_sanitize_degrees(src_h);
}

static double cogito_sum_get_rotated_hue(double src_h,
                                         const double* huebps, size_t huebps_len,
                                         const double* rotations, size_t rotations_len) {
  double piece = cogito_sum_piecewise_val(src_h, huebps, huebps_len, rotations, rotations_len);
  return cogito_sum_sanitize_degrees(piece + src_h);
}

static void sum_build_dynamic(bool dark, CogitoColor accent, char* out, size_t cap) {
  size_t off = 0;
  if (cap == 0) return;
  out[
    0
  ] = 0;

  double h = 0.0, c = 0.0, t = 0.0;
  cogito_rgb_to_hct(accent, &h, &c, &t);

  // === Full Color Token System ===
  // Primary palette — MD3 Expressive: chroma 36 dark / 40 light
  double primary_c = dark ? 36.0 : 40.0;
  CogitoColor primary = cogito_hct_to_rgb(h, primary_c, dark ? 80.0 : 40.0);
  CogitoColor on_primary = dark ? cogito_hct_to_rgb(h, primary_c,
  20.0) : cogito_hct_to_rgb(h, primary_c,
  100.0);
  CogitoColor primary_container = cogito_hct_to_rgb(h, primary_c, dark ? 30.0 : 90.0);
  CogitoColor on_primary_container = cogito_hct_to_rgb(h, primary_c, dark ? 90.0 : 30.0);
  CogitoColor surface_tint = cogito_alpha(primary,
  0.1f);

  // Secondary palette — MD3 Expressive: +60° hue, chroma 24 for vibrancy
  double secondary_h = cogito_sum_sanitize_degrees(h + 60.0);
  double secondary_c = 24.0;
  CogitoColor secondary = cogito_hct_to_rgb(secondary_h, secondary_c, dark ? 80.0 : 40.0);
  CogitoColor on_secondary = dark ? cogito_hct_to_rgb(secondary_h, secondary_c,
  20.0) : cogito_hct_to_rgb(secondary_h, secondary_c,
  100.0);
  CogitoColor secondary_container = cogito_hct_to_rgb(secondary_h, secondary_c, dark ? 30.0 : 90.0);
  CogitoColor on_secondary_container = cogito_hct_to_rgb(secondary_h, secondary_c, dark ? 90.0 : 30.0);

  // Tertiary palette
  static const double tertiary_hues[] = {
    0.0,
    20.0,
    71.0,
    161.0,
    333.0,
    360.0
  };
  static const double tertiary_rotations[] = {
    -40.0,
    48.0,
    -32.0,
    29.5,
    -32.0
  };
  double tert_h = cogito_sum_get_rotated_hue(
    h,
    tertiary_hues, sizeof(tertiary_hues) / sizeof(tertiary_hues[
    0
  ]),
    tertiary_rotations, sizeof(tertiary_rotations) / sizeof(tertiary_rotations[
    0
  ])
  );
  double tert_c = dark ? 28.0 : 48.0;
  CogitoColor tertiary = cogito_hct_to_rgb(tert_h, tert_c, dark ? 80.0 : 40.0);
  CogitoColor on_tertiary = dark ? cogito_hct_to_rgb(tert_h, tert_c,
  20.0) : cogito_hct_to_rgb(tert_h, tert_c,
  100.0);
  CogitoColor tertiary_container = cogito_hct_to_rgb(tert_h, tert_c, dark ? 30.0 : 90.0);
  CogitoColor on_tertiary_container = cogito_hct_to_rgb(tert_h, tert_c, dark ? 90.0 : 30.0);

  // Error palette
  CogitoColor error = cogito_hct_to_rgb(25.0,
  84.0, dark ? 80.0 : 40.0);
  CogitoColor on_error = cogito_hct_to_rgb(25.0,
  84.0, dark ? 20.0 : 100.0);
  CogitoColor error_container = cogito_hct_to_rgb(25.0,
  84.0, dark ? 30.0 : 90.0);
  CogitoColor on_error_container = cogito_hct_to_rgb(25.0,
  84.0, dark ? 90.0 : 30.0);

  // Neutral palette — MD3: chroma 6 for tinted neutrals
  double neutral_c = 6.0;
  CogitoColor background = cogito_hct_to_rgb(h, neutral_c, dark ? 6.0 : 98.0);
  CogitoColor on_background = cogito_hct_to_rgb(h, neutral_c, dark ? 90.0 : 10.0);
  CogitoColor surface = cogito_hct_to_rgb(h, neutral_c, dark ? 6.0 : 98.0);
  CogitoColor on_surface = cogito_hct_to_rgb(h, neutral_c, dark ? 90.0 : 10.0);

  // Neutral Variant palette — MD3: chroma 8 for visible outline/surface-variant
  double neutralv_c = 8.0;
  CogitoColor outline = cogito_hct_to_rgb(h, neutralv_c, dark ? 60.0 : 50.0);
  CogitoColor outline_variant = cogito_hct_to_rgb(h, neutralv_c, dark ? 30.0 : 80.0);
  CogitoColor surface_variant = cogito_hct_to_rgb(h, neutralv_c, dark ? 30.0 : 90.0);
  CogitoColor on_surface_variant = cogito_hct_to_rgb(h, neutralv_c, dark ? 80.0 : 30.0);

  // Surface containers
  CogitoColor surface_dim = cogito_hct_to_rgb(h, neutral_c, dark ? 6.0 : 87.0);
  CogitoColor surface_bright = cogito_hct_to_rgb(h, neutral_c, dark ? 24.0 : 98.0);
  CogitoColor surface_cont_lowest = cogito_hct_to_rgb(h, neutral_c, dark ? 4.0 : 100.0);
  CogitoColor surface_cont_low = cogito_hct_to_rgb(h, neutral_c, dark ? 10.0 : 96.0);
  CogitoColor surface_cont = cogito_hct_to_rgb(h, neutral_c, dark ? 12.0 : 94.0);
  CogitoColor surface_cont_high = cogito_hct_to_rgb(h, neutral_c, dark ? 17.0 : 92.0);
  CogitoColor surface_cont_highest = cogito_hct_to_rgb(h, neutral_c, dark ? 22.0 : 90.0);

  // Inverse colors
  CogitoColor inverse_surface = cogito_hct_to_rgb(h, neutral_c, dark ? 90.0 : 20.0);
  CogitoColor inverse_on_surface = cogito_hct_to_rgb(h, neutral_c, dark ? 20.0 : 95.0);
  CogitoColor inverse_primary = cogito_hct_to_rgb(h, primary_c, dark ? 40.0 : 80.0);

  // Shadow & Scrim
  CogitoColor shadow = cogito_hct_to_rgb(h,
  0.0,
  0.0);
  CogitoColor scrim = cogito_hct_to_rgb(h,
  0.0,
  0.0);

  // === Hex conversions ===
  char p[
    8
  ], op[
    8
  ], pc[
    8
  ], opc[
    8
  ];
  char sec[
    8
  ], osec[
    8
  ], secc[
    8
  ], osecc[
    8
  ];
  char tert[
    8
  ], otert[
    8
  ], tertc[
    8
  ], otertc[
    8
  ];
  char err[
    8
  ], oerr[
    8
  ], errc[
    8
  ], oerrc[
    8
  ];
  char bg[
    8
  ], obg[
    8
  ], surf[
    8
  ], surft[
    8
  ], ons[
    8
  ];
  char outl[
    8
  ], outlv[
    8
  ];
  char surfvar[
    8
  ], osurfvar[
    8
  ];
  char surfdim[
    8
  ], surfbrt[
    8
  ];
  char scl[
    8
  ], sclo[
    8
  ], sc[
    8
  ], sch[
    8
  ], schi[
    8
  ];
  char invsrf[
    8
  ], invons[
    8
  ], invp[
    8
  ];
  char shad[
    8
  ], scrm[
    8
  ];

  cogito_color_to_hex(primary, p);
  cogito_color_to_hex(on_primary, op);
  cogito_color_to_hex(primary_container, pc);
  cogito_color_to_hex(on_primary_container, opc);
  cogito_color_to_hex(secondary, sec);
  cogito_color_to_hex(on_secondary, osec);
  cogito_color_to_hex(secondary_container, secc);
  cogito_color_to_hex(on_secondary_container, osecc);
  cogito_color_to_hex(tertiary, tert);
  cogito_color_to_hex(on_tertiary, otert);
  cogito_color_to_hex(tertiary_container, tertc);
  cogito_color_to_hex(on_tertiary_container, otertc);
  cogito_color_to_hex(error, err);
  cogito_color_to_hex(on_error, oerr);
  cogito_color_to_hex(error_container, errc);
  cogito_color_to_hex(on_error_container, oerrc);
  cogito_color_to_hex(background, bg);
  cogito_color_to_hex(on_background, obg);
  cogito_color_to_hex(surface, surf);
  cogito_color_to_hex(surface_tint, surft);
  cogito_color_to_hex(on_surface, ons);
  cogito_color_to_hex(outline, outl);
  cogito_color_to_hex(outline_variant, outlv);
  cogito_color_to_hex(surface_variant, surfvar);
  cogito_color_to_hex(on_surface_variant, osurfvar);
  cogito_color_to_hex(surface_dim, surfdim);
  cogito_color_to_hex(surface_bright, surfbrt);
  cogito_color_to_hex(surface_cont_lowest, scl);
  cogito_color_to_hex(surface_cont_low, sclo);
  cogito_color_to_hex(surface_cont, sc);
  cogito_color_to_hex(surface_cont_high, sch);
  cogito_color_to_hex(surface_cont_highest, schi);
  cogito_color_to_hex(inverse_surface, invsrf);
  cogito_color_to_hex(inverse_on_surface, invons);
  cogito_color_to_hex(inverse_primary, invp);
  cogito_color_to_hex(shadow, shad);
  cogito_color_to_hex(scrim, scrm);

  // === SUM output ===
  cogito_sum_append(out, cap, &off,
  "*\n  color: %s\n  font-size: 14\n", ons);

  cogito_sum_append(out, cap, &off,
  ".surface\n  background: %s\n", surf);
  cogito_sum_append(out, cap, &off,
  ".surface-container-high\n  background: %s\n", sch);
  cogito_sum_append(out, cap, &off,
  ".surface-container-highest\n  background: %s\n", schi);
  cogito_sum_append(out, cap, &off,
  ".surface-container-low\n  background: %s\n", sclo);
  cogito_sum_append(out, cap, &off,
  ".surface-container-lowest\n  background: %s\n", scl);
  cogito_sum_append(out, cap, &off,
  ".surface-container\n  background: %s\n", sc);
  cogito_sum_append(out, cap, &off,
  ".surface-bright\n  background: %s\n", surfbrt);
  cogito_sum_append(out, cap, &off,
  ".surface-dim\n  background: %s\n", surfdim);
  cogito_sum_append(out, cap, &off,
  ".surface-variant\n  background: %s\n  color: %s\n", surfvar, osurfvar);

  cogito_sum_append(out, cap, &off,
  ".inverse-primary\n  color: %s\n", invp);
  cogito_sum_append(out, cap, &off,
  ".inverse-surface\n  background: %s\n  color: %s\n", invsrf, invons);

  cogito_sum_append(out, cap, &off,
  ".error\n  background: %s\n  color: %s\n", err, oerr);
  cogito_sum_append(out, cap, &off,
  ".error-container\n  background: %s\n  color: %s\n", errc, oerrc);

  cogito_sum_append(out, cap, &off,
  ".primary\n  background: %s\n  color: %s\n", p, op);
  cogito_sum_append(out, cap, &off,
  ".primary-container\n  background: %s\n  color: %s\n", pc, opc);
  cogito_sum_append(out, cap, &off,
  ".secondary\n  background: %s\n  color: %s\n", sec, osec);
  cogito_sum_append(out, cap, &off,
  ".secondary-container\n  background: %s\n  color: %s\n", secc, osecc);
  cogito_sum_append(out, cap, &off,
  ".tertiary\n  background: %s\n  color: %s\n", tert, otert);
  cogito_sum_append(out, cap, &off,
  ".tertiary-container\n  background: %s\n  color: %s\n", tertc, otertc);

  cogito_sum_append(out, cap, &off,
  ".outline\n  border-color: %s\n", outl);
  cogito_sum_append(out, cap, &off,
  ".outline-variant\n  border-color: %s\n", outlv);

  cogito_sum_append(out, cap, &off,
  ".scrim\n  background: %s\n", scrm);
  cogito_sum_append(out, cap, &off,
  ".shadow\n  background: %s\n", scrm);

  cogito_sum_append(out, cap, &off,
  ".big-display\n  color: %s\n  font-family: serif\n  font-size: 56\n  font-weight: normal\n", ons);
  cogito_sum_append(out, cap, &off,
  ".display\n  color: %s\n  font-family: serif\n  font-size: 36\n  font-weight: normal\n", ons);
  cogito_sum_append(out, cap, &off,
  ".view-title\n  color: %s\n  font-size: 24\n  font-weight: normal\n", ons);
  cogito_sum_append(out, cap, &off,
  ".view-subtitle\n  color: %s\n  font-size: 18\n  font-weight: bold\n", ons);
  cogito_sum_append(out, cap, &off,
  ".heading\n  color: %s\n  font-size: 16\n  font-weight: normal\n", ons);
  cogito_sum_append(out, cap, &off,
  ".content-title\n  color: %s\n  font-size: 16\n  font-weight: bold\n", ons);
  cogito_sum_append(out, cap, &off,
  ".content-subtitle\n  color: %s\n  font-size: 14\n  font-weight: normal\n", ons);
  cogito_sum_append(out, cap, &off,
  ".title\n  color: %s\n  font-size: 24\n  font-weight: normal\n", ons);
  cogito_sum_append(out, cap, &off,
  ".subtitle\n  color: %s\n  font-size: 18\n  font-weight: bold\n", ons);
  cogito_sum_append(out, cap, &off,
  ".body\n  color: %s\n  font-size: 14\n  font-weight: normal\n", ons);
  cogito_sum_append(out, cap, &off,
  ".caption\n  color: %s\n  font-size: 12\n  font-weight: bold\n", ons);
  cogito_sum_append(out, cap, &off,
  ".tabular\n  font-size: 14\n  font-weight: normal\n  font-variant-numeric: tnum\n");
  cogito_sum_append(out, cap, &off,
  ".monospace\n  font-size: 14\n  font-weight: normal\n");

  cogito_sum_append(out, cap, &off,
  "about_window\n  background: %s\n  color: %s\n  border-radius: 26\n  padding: 24\n", sch, ons);
  cogito_sum_append(out, cap, &off,
  "active_indicator\n  color: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "appbar .button:active\n  background: %s\n", outl);
  cogito_sum_append(out, cap, &off,
  "appbar .button:hover\n  background: %s\n", surf);
  cogito_sum_append(out, cap, &off,
  "appbar .iconbtn:active\n  background: %s\n", bg);
  cogito_sum_append(out, cap, &off,
  "appbar .iconbtn:hover\n  background: %s\n", outl);
  cogito_sum_append(out, cap, &off,
  "appbar .iconbtn\n  background: transparent\n");
  cogito_sum_append(out, cap, &off,
  "appbar .window-close:active\n  background: %s\n", pc);
  cogito_sum_append(out, cap, &off,
  "appbar .window-close:hover\n  background: %s\n", pc);
  cogito_sum_append(out, cap, &off,
  "appbar .window-close\n  background: %s\n", pc);
  cogito_sum_append(out, cap, &off,
  "appbar .window-max:active\n  background: %s\n", pc);
  cogito_sum_append(out, cap, &off,
  "appbar .window-max:hover\n  background: %s\n", pc);
  cogito_sum_append(out, cap, &off,
  "appbar .window-max\n  background: %s\n", pc);
  cogito_sum_append(out, cap, &off,
  "appbar .window-min:active\n  background: %s\n", pc);
  cogito_sum_append(out, cap, &off,
  "appbar .window-min:hover\n  background: %s\n", pc);
  cogito_sum_append(out, cap, &off,
  "appbar .window-min\n  background: %s\n", pc);
  cogito_sum_append(out, cap, &off,
  "appbar.sidebar\n  background: %s\n  color: %s\n", sc, ons);
  cogito_sum_append(out, cap, &off,
  "appbar.view\n  background: %s\n  color: %s\n", bg, ons);
  cogito_sum_append(out, cap, &off,
  "appbar\n  background: %s\n  color: %s\n", bg, ons);
  cogito_sum_append(out, cap, &off,
  "avatar\n  background: %s\n  color: %s\n  min-width: 64\n  min-height: 64\n", secc, osecc);
  cogito_sum_append(out, cap, &off,
  "badge\n  background: %s\n  color: %s\n  font-size: 11\n  min-width: 10\n  min-height: 10\n", err, oerr);
  cogito_sum_append(out, cap, &off,
  "banner:selection\n  color: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "banner\n  background: %s\n  color: %s\n  padding: 16\n  min-height: 52\n  transition: 200 standard\n", secc, osecc);
  cogito_sum_append(out, cap, &off,
  "bottom_sheet\n  background: %s\n  color: %s\n  border-radius: 28 28 0 0\n  padding: 16\n  transition: 300 emphasized\n", sclo, ons);
  cogito_sum_append(out, cap, &off,
  "bottom_toolbar.floating_compact\n  border-radius: 999\n  elevation: 1\n  padding: 8\n");
  cogito_sum_append(out, cap, &off,
  "bottom_toolbar.floating\n  border-radius: 999\n  elevation: 1\n");
  cogito_sum_append(out, cap, &off,
  "bottom_toolbar .iconbtn\n  color: %s\n  background: transparent\n", ons);
  cogito_sum_append(out, cap, &off,
  "bottom_toolbar .iconbtn\n  color: %s\n  background: transparent\n", ons);
  cogito_sum_append(out, cap, &off,
  "bottom_toolbar\n  background: %s\n  color: %s\n  min-height: 64\n  padding: 8 16\n", sc, ons);
  cogito_sum_append(out, cap, &off,
  "bottom-nav:selection\n  background: %s\n  color: %s\n", secc, osecc);
  cogito_sum_append(out, cap, &off,
  "bottom-nav\n  background: %s\n  color: %s\n  min-height: 80\n  padding: 0 8\n  transition: 220 standard\n", sc, ons);
  cogito_sum_append(out, cap, &off,
  "bottom-sheet\n  background: %s\n  color: %s\n  border-radius: 28 28 0 0\n  padding: 16\n  transition: 300 emphasized\n", sclo, ons);
  cogito_sum_append(out, cap, &off,
  "side-sheet\n  background: %s\n  color: %s\n  border-radius: 28 0 0 28\n  padding: 16\n  transition: 300 emphasized\n", sclo, ons);
  cogito_sum_append(out, cap, &off,
  "button:active\n  background: %s\n  color: %s\n", p, op);
  cogito_sum_append(out, cap, &off,
  "button:disabled\n  background: %s29\n  color: %s52\n", ons, ons);
  cogito_sum_append(out, cap, &off,
  "button:focus, iconbtn:focus, checkbox:focus, switch:focus, slider:focus, dropdown:focus, chip:focus, segmented:focus, stepper:focus, fab:focus, nav_rail:focus, bottom_nav:focus, datepicker:focus, colorpicker:focus, timepicker:focus\n  border-color: %s\n  border-width: 2\n", p);
  // MD3: hover stays on primary bg (state layer handles the tint in button.inc)
  cogito_sum_append(out, cap, &off,
  "button:hover\n  background: %s\n  color: %s\n", p, op);
  cogito_sum_append(out, cap, &off,
  "button.outlined:active\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "button.outlined:disabled\n  background: transparent\n  color: %s52\n  border-color: %s52\n", ons, ons, ons);
  cogito_sum_append(out, cap, &off,
  "button.outlined:hover\n  background: %s\n  border-color: %s\n", p, p);
  cogito_sum_append(out, cap, &off,
  "button.outlined\n  background: transparent\n  color: %s\n  border: 1 solid %s\n", p, outl);
  cogito_sum_append(out, cap, &off,
  "button.text:active\n  background: %s1f\n", p);
  cogito_sum_append(out, cap, &off,
  "button.text:disabled\n  background: transparent\n  color: %s52\n", ons, ons);
  cogito_sum_append(out, cap, &off,
  "button.text:hover\n  background: %s12\n  color: %s\n", p, ons);
  cogito_sum_append(out, cap, &off,
  "button.text\n  background: transparent\n  color: %s\n  box-shadow: none\n  padding: 0\n", p);
  cogito_sum_append(out, cap, &off,
  "button.toolbar_format_active\n  background: %s\n  color: %s\n  min-width: 56\n  min-height: 56\n  padding: 0 16\n  box-shadow: none\n", p, op);
  cogito_sum_append(out, cap, &off,
  "button.toolbar_format\n  background: transparent\n  color: %s\n  min-width: 40\n  min-height: 48\n  padding: 0 8\n  box-shadow: none\n", p);
  cogito_sum_append(out, cap, &off,
  "button\n  background: %s\n  color: %s\n  font-weight: bold\n  transition: 180 standard\n", p, op);
  cogito_sum_append(out, cap, &off,
  "card:hover\n  box-shadow: [ 0 2 8 0 #0000002a ]\n");
  // MD3 Expressive Card: L=16dp corner radius
  cogito_sum_append(out, cap, &off,
  "card\n  background: %s\n  color: %s\n  border-radius: 16\n  padding: 16\n  transition: 200 standard\n", sclo, ons);
  cogito_sum_append(out, cap, &off,
  "carousel_item\n  background: %s\n  color: %s\n  border-radius: 26\n  padding: 16\n", sc, ons);
  cogito_sum_append(out, cap, &off,
  "carousel_item\n  background: %s\n", surf);
  cogito_sum_append(out, cap, &off,
  "carousel\n  background: %s\n  color: %s\n  min-height: 240\n  min-width: 400\n  border-radius: 26\n", surf, ons);
  cogito_sum_append(out, cap, &off,
  "checkbox .box:active\n  border-color: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "checkbox .box:hover\n  border-color: %s\n", ons);
  cogito_sum_append(out, cap, &off,
  "checkbox .box\n  background: transparent\n  border-radius: 4\n  border: 2 solid %s\n", osurfvar);
  cogito_sum_append(out, cap, &off,
  "checkbox .box\n  background: transparent\n  border-radius: 4\n  border: 2 solid %s\n", osurfvar);
  cogito_sum_append(out, cap, &off,
  "checkbox .check:active\n  color: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "checkbox .check:hover\n  color: %s\n", osurfvar);
  cogito_sum_append(out, cap, &off,
  "checkbox .check\n  color: %s\n", ons);
  cogito_sum_append(out, cap, &off,
  "checkbox:checked .box\n  background: %s\n  border: 0 none %s\n", p, p);
  // MD3: checkmark on filled primary box must use on_primary, not on_surface
  cogito_sum_append(out, cap, &off,
  "checkbox:checked .check\n  color: %s\n", op);
  cogito_sum_append(out, cap, &off,
  "checkbox:checked\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "checkbox:disabled\n  color: %s52\n", ons);
  cogito_sum_append(out, cap, &off,
  "checkbox\n  color: %s\n  min-height: 24\n  min-width: 24\n  transition: 120 standard\n", ons);
  cogito_sum_append(out, cap, &off,
  "checkbox\n  color: %s\n  min-height: 24\n  min-width: 24\n  transition: 120 standard\n", ons);
  cogito_sum_append(out, cap, &off,
  "chip:active\n  background: %s\n", sc);
  cogito_sum_append(out, cap, &off,
  "chip:checked\n  background: %s\n  color: %s\n  border: 0 none %s\n", secc, osecc, secc);
  cogito_sum_append(out, cap, &off,
  "chip:disabled\n  background: %s29\n  color: %s52\n  border-color: %s52\n", ons, ons, ons);
  cogito_sum_append(out, cap, &off,
  "chip:hover\n  background: %s\n  border-color: %s\n", sclo, ons);
  cogito_sum_append(out, cap, &off,
  "chip\n  background: transparent\n  color: %s\n  border-radius: 100\n  min-height: 32\n  padding: 0 16\n  border: 1 solid %s\n  transition: 150 standard\n", ons, outl);
  cogito_sum_append(out, cap, &off,
  "colorpicker:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "colorpicker\n  background: %s\n  color: %s\n  border-radius: 18\n  min-height: 140\n  padding: 12\n  track-height: 4\n", sc, ons);
  cogito_sum_append(out, cap, &off,
  "content_list label\n  padding: 0 16\n  min-height: 80\n  background: transparent\n");
  cogito_sum_append(out, cap, &off,
  "content_list\n  background: %s\n  color: %s\n  border-radius: 12\n", surfvar, ons);
  cogito_sum_append(out, cap, &off,
  "content_list=first\n  border-radius: 12 12 8 8\n");
  cogito_sum_append(out, cap, &off,
  "content_list=last\n  border-radius: 8 8 12 12\n");
  cogito_sum_append(out, cap, &off,
  "content_list=middle\n  border-radius: 8\n");
  cogito_sum_append(out, cap, &off,
  "content_list=single\n  border-radius: 12\n");
  cogito_sum_append(out, cap, &off,
  "datepicker, .datepicker\n  background: %s\n  color: %s\n  border: 1 solid %s\n  border-radius: 8\n  min-height: 56\n  min-width: 220\n  transition: 180 standard\n", sc, ons, outl);
  cogito_sum_append(out, cap, &off,
  "datepicker:hover, .datepicker:hover\n  background: %s\n", surfvar);
  cogito_sum_append(out, cap, &off,
  "datepicker:selection, .datepicker:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "dialog\n  background: %s\n  color: %s\n  border-radius: 28\n  min-width: 280\n  min-height: 240\n  padding: 24\n", sch, ons);
  cogito_sum_append(out, cap, &off,
  "dialogslot\n  background: %s66\n  color: %s\n", scrm, ons);
  cogito_sum_append(out, cap, &off,
  "divider\n  background: %s\n  min-height: 1\n", outlv);
  cogito_sum_append(out, cap, &off,
  "dropdown:disabled\n  background: %s29\n  color: %s52\n", ons, ons);
  cogito_sum_append(out, cap, &off,
  "dropdown:hover\n  background: %s\n", surfvar);
  cogito_sum_append(out, cap, &off,
  "dropdown\n  background: %s\n  color: %s\n  border-radius: 4 4 0 0\n  min-height: 56\n  padding: 8 16\n  transition: 180 standard\n", sc, ons);
  cogito_sum_append(out, cap, &off,
  "empty_page\n  background: transparent\n  color: %s\n", ons);
  cogito_sum_append(out, cap, &off,
  "fab:active\n  background: %s\n  color: %s\n", p, op);
  cogito_sum_append(out, cap, &off,
  "fab:disabled\n  background: %s29\n  color: %s52\n", ons, ons);
  cogito_sum_append(out, cap, &off,
  "fab:hover\n  background: %s\n  color: %s\n", pc, op);
  cogito_sum_append(out, cap, &off,
  "fab.toolbar_plus\n  background: %s\n  color: %s\n  border-radius: 20\n  min-width: 56\n  min-height: 56\n  elevation: 3\n", secc, osecc);
  cogito_sum_append(out, cap, &off,
  "fab\n  background: %s\n  color: %s\n  border-radius: 100\n  min-width: 56\n  min-height: 56\n  transition: 180 standard\n", p, op);
  cogito_sum_append(out, cap, &off,
  "fixed\n  background: transparent\n  color: %s\n", ons);
  cogito_sum_append(out, cap, &off,
  "grid:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "grid\n  background: %s\n  color: %s\n  border-radius: 12\n", sc, ons);
  cogito_sum_append(out, cap, &off,
  "hstack\n  background: %s\n  color: %s\n", bg, ons);
  cogito_sum_append(out, cap, &off,
  "iconbtn:active\n  background: %s\n  color: %s\n", surfvar, p);
  cogito_sum_append(out, cap, &off,
  "iconbtn:disabled\n  background: %s29\n  color: %s52\n", ons, ons);
  cogito_sum_append(out, cap, &off,
  "iconbtn:hover\n  background: %s\n  color: %s\n", surfvar, osurfvar);
  cogito_sum_append(out, cap, &off,
  "iconbtn.filled:active\n  background: %s\n  color: %s\n", pc, opc);
  cogito_sum_append(out, cap, &off,
  "iconbtn.filled:hover\n  background: %s\n  color: %s\n", pc, opc);
  cogito_sum_append(out, cap, &off,
  "iconbtn.filled\n  background: %s\n  color: %s\n  transition: 150 standard\n", p, op);
  cogito_sum_append(out, cap, &off,
  "iconbtn.iconic:active\n  background: %s\n  color: %s\n", surfvar, p);
  cogito_sum_append(out, cap, &off,
  "iconbtn.iconic:hover\n  background: %s\n  color: %s\n", surfvar, osurfvar);
  cogito_sum_append(out, cap, &off,
  "iconbtn.iconic\n  background: transparent\n  color: %s\n  transition: 150 standard\n", osurfvar);
  cogito_sum_append(out, cap, &off,
  "iconbtn.outlined:active\n  background: %s\n  color: %s\n  border-color: %s\n", p, op, p);
  cogito_sum_append(out, cap, &off,
  "iconbtn.outlined:hover\n  background: %s\n  color: %s\n  border-color: %s\n", surfvar, osurfvar, outl);
  cogito_sum_append(out, cap, &off,
  "iconbtn.outlined\n  background: transparent\n  color: %s\n  border: 1 solid %s\n  transition: 150 standard\n", osurfvar, outl);
  cogito_sum_append(out, cap, &off,
  "iconbtn.tonal:active\n  background: %s\n  color: %s\n", p, op);
  cogito_sum_append(out, cap, &off,
  "iconbtn.tonal:hover\n  background: %s\n  color: %s\n", p, op);
  cogito_sum_append(out, cap, &off,
  "iconbtn.tonal\n  background: %s\n  color: %s\n  transition: 150 standard\n", pc, opc);
  cogito_sum_append(out, cap, &off,
  "iconbtn.toolbar_float_icon\n  background: %s\n  color: %s\n  border-radius: 99\n  min-width: 48\n  min-height: 48\n", sc, ons);
  cogito_sum_append(out, cap, &off,
  "iconbtn.toolbar_plus\n  background: %s\n  color: %s\n  border-radius: 20\n  min-width: 56\n  min-height: 56\n  elevation: 3\n", secc, osecc);
  cogito_sum_append(out, cap, &off,
  "iconbtn\n  background: transparent\n  color: %s\n  transition: 150 standard\n", osurfvar);
  cogito_sum_append(out, cap, &off,
  "image\n  background: transparent\n");
  cogito_sum_append(out, cap, &off,
  "label.carousel-item-label\n  font-size: 21\n  font-weight: bold\n  color: #ffffff\n");
  cogito_sum_append(out, cap, &off,
  "label\n  color: %s\n", ons);
  cogito_sum_append(out, cap, &off,
  "list:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "list\n  background: transparent\n  color: %s\n", ons);
  cogito_sum_append(out, cap, &off,
  "menu item\n  border-radius: 0\n  padding: 8 12\n  min-height: 42\n");
  cogito_sum_append(out, cap, &off,
  "menu_button:hover\n  background: %s\n", surfvar);
  cogito_sum_append(out, cap, &off,
  "menu_button:active\n  background: %s\n  color: %s\n", secc, osecc);
  cogito_sum_append(out, cap, &off,
  "menu_button:checked\n  background: %s\n  color: %s\n", secc, osecc);
  cogito_sum_append(out, cap, &off,
  "menu_button:hover\n  background: %s\n", surfvar);
  cogito_sum_append(out, cap, &off,
  "menu_button:active\n  background: %s\n  color: %s\n", secc, osecc);
  cogito_sum_append(out, cap, &off,
  "menu_button:checked\n  background: %s\n  color: %s\n", secc, osecc);
  cogito_sum_append(out, cap, &off,
  "menu_button\n  background: transparent\n  color: %s\n  padding: 8 12\n", ons);
  cogito_sum_append(out, cap, &off,
  "menu:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "menu\n  background: %s\n  border-radius: 4\n  color: %s\n  box-shadow: 2\n  font-size: 14\n", sc, ons);
  cogito_sum_append(out, cap, &off,
  "nav-rail:selection\n  background: %s\n  color: %s\n", secc, osecc);
  cogito_sum_append(out, cap, &off,
  "nav-rail\n  background: %s\n  color: %s\n  min-width: 96\n  padding: 4 0\n  transition: 220 standard\n", surf, ons);
  cogito_sum_append(out, cap, &off,
  "popover\n  background: %s\n  border-radius: 12\n  padding: 16\n  box-shadow: [ 0 2 8 0 #0000001f ]\n", sc);
  cogito_sum_append(out, cap, &off,
  "progress:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "progress\n  background: %s\n  border-radius: 100\n  track-height: 4\n  min-height: 12\n  min-width: 120\n  transition: 200 standard\n", outlv);
  cogito_sum_append(out, cap, &off,
  "radio .box:active\n  border-color: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "radio .box:hover\n  border-color: %s\n", ons);
  cogito_sum_append(out, cap, &off,
  "radio .box\n  background: transparent\n  border-radius: 100\n  border: 2 solid %s\n", osurfvar);
  cogito_sum_append(out, cap, &off,
  "radio .box\n  background: transparent\n  border-radius: 100\n  border: 2 solid %s\n", osurfvar);
  cogito_sum_append(out, cap, &off,
  "radio .check:active\n  color: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "radio .check:hover\n  color: %s\n", osurfvar);
  cogito_sum_append(out, cap, &off,
  "radio .check\n  color: %s\n", ons);
  cogito_sum_append(out, cap, &off,
  "radio:checked .box\n  background: %s\n  border: 0 none %s\n", p, p);
  cogito_sum_append(out, cap, &off,
  "radio:checked .check\n  color: %s\n", op);
  cogito_sum_append(out, cap, &off,
  "radio:checked\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "radio:disabled\n  color: %s52\n", ons);
  cogito_sum_append(out, cap, &off,
  "radio\n  color: %s\n  min-height: 24\n  min-width: 24\n  transition: 120 standard\n", ons);
  cogito_sum_append(out, cap, &off,
  "scroller.sidebar\n  background: %s\n  color: %s\n", sc, ons);
  cogito_sum_append(out, cap, &off,
  "scroller\n  background: %s\n  color: %s\n", bg, ons);
  cogito_sum_append(out, cap, &off,
  "searchfield:active\n  border-color: %s\n  border-width: 2\n", p);
  cogito_sum_append(out, cap, &off,
  "searchfield:disabled\n  background: %s29\n  color: %s52\n", ons, ons);
  cogito_sum_append(out, cap, &off,
  "searchfield:focus\n  border: 2 solid %s\n", p);
  cogito_sum_append(out, cap, &off,
  "searchfield:hover\n  background: %s\n  border-color: %s\n", surfvar, ons);
  cogito_sum_append(out, cap, &off,
  "searchfield:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "searchfield\n  background: %s\n  color: %s\n  border-radius: 999\n  min-height: 56\n  min-width: 240\n  padding: 6 16\n  transition: 180 standard\n", sch, ons);
  cogito_sum_append(out, cap, &off,
  "segmented:disabled\n  background: %s29\n  color: %s52\n", ons, ons);
  cogito_sum_append(out, cap, &off,
  "segmented:selection\n  background: %s\n  color: %s\n  border: 1 solid %s\n", secc, osecc, secc);
  cogito_sum_append(out, cap, &off,
  "segmented\n  background: transparent\n  color: %s\n  border-radius: 100\n  min-height: 40\n  border: 1 solid %s\n  transition: 180 standard\n", ons, outl);
  cogito_sum_append(out, cap, &off,
  "settings_list\n  background: %s\n  color: %s\n  border-radius: 12\n", surf, ons);
  cogito_sum_append(out, cap, &off,
  "settings_page\n  background: %s\n  color: %s\n", bg, ons);
  cogito_sum_append(out, cap, &off,
  "settings_row\n  background: transparent\n  color: %s\n  min-height: 56\n  padding: 8 16\n", ons);
  cogito_sum_append(out, cap, &off,
  "settings_window\n  background: %s\n  color: %s\n", bg, ons);
  cogito_sum_append(out, cap, &off,
  "slider:active\n  color: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "slider:disabled\n  color: %s52\n", ons);
  cogito_sum_append(out, cap, &off,
  "slider:hover\n  color: %s\n", pc);
  cogito_sum_append(out, cap, &off,
  "slider\n  background: %s\n  color: %s\n  track-height: 4\n  min-height: 48\n  min-width: 200\n  transition: 120 standard\n", surfvar, p);
  cogito_sum_append(out, cap, &off,
  "split_button:hover\n  background: %s\n", pc);
  cogito_sum_append(out, cap, &off,
  "split_button:active\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "split_button:hover\n  background: %s\n", pc);
  cogito_sum_append(out, cap, &off,
  "split_button:active\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "split_button\n  background: %s\n  color: %s\n  border-radius: 20\n", p, op);
  cogito_sum_append(out, cap, &off,
  "stepper:disabled\n  background: %s29\n  color: %s52\n", ons, ons);
  cogito_sum_append(out, cap, &off,
  "stepper\n  background: %s\n  color: %s\n  border-radius: 100\n  min-height: 42\n  min-width: 120\n  border: 1 solid %s\n  padding: 4 8\n  transition: 150 standard\n", surf, ons, outl);
  cogito_sum_append(out, cap, &off,
  "switch .knob:active\n  background: %s\n", op);
  cogito_sum_append(out, cap, &off,
  "switch .knob:hover\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "switch .knob\n  background: %s\n  min-width: 32\n  min-height: 24\n", outl);
  cogito_sum_append(out, cap, &off,
  "switch .track:active\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "switch .track:hover\n  background: %s\n", outl);
  cogito_sum_append(out, cap, &off,
  "switch .track\n  background: %s\n  border: 2 solid %s\n", schi, outl);
  cogito_sum_append(out, cap, &off,
  "switch knob:active\n  background: %s\n", surf);
  cogito_sum_append(out, cap, &off,
  "switch knob:hover\n  background: %s\n", surf);
  cogito_sum_append(out, cap, &off,
  "switch knob\n  background: %s\n  min-width: 24\n  min-height: 24\n  elevation: 1\n", outl);
  cogito_sum_append(out, cap, &off,
  "switch track:active\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "switch track:hover\n  background: %s\n", schi);
  cogito_sum_append(out, cap, &off,
  "switch track\n  background: %s\n  border: 2 solid %s\n", schi, outl);
  cogito_sum_append(out, cap, &off,
  "switch:checked .track\n  background: %s\n  border-radius: 100\n  border: 0 none %s\n", p, p);
  cogito_sum_append(out, cap, &off,
  "switch:checked track\n  background: %s\n  border-radius: 100\n  border: 0 none %s\n", p, p);
  cogito_sum_append(out, cap, &off,
  "switch:disabled knob\n  background: %s52\n", ons);
  cogito_sum_append(out, cap, &off,
  "switch:disabled track\n  background: %s29\n", ons);
  cogito_sum_append(out, cap, &off,
  "switch:disabled\n  color: %s52\n", ons);
  cogito_sum_append(out, cap, &off,
  "switch\n  color: %s\n  min-height: 32\n  min-width: 52\n  transition: 100 standard\n", ons);
  cogito_sum_append(out, cap, &off,
  "switchbar:active\n  background: %s\n  color: %s\n  border-color: %s\n", secc, osecc, secc);
  cogito_sum_append(out, cap, &off,
  "switchbar\n  background: %s\n  color: %s\n  border: 1 solid %s\n  border-radius: 12\n  min-height: 56\n", surf, ons, outl);
  cogito_sum_append(out, cap, &off,
  "tabs:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "tabs\n  background: %s\n  color: %s\n  border-radius: 0\n  min-height: 48\n  transition: 180 standard\n", surf, ons);
  cogito_sum_append(out, cap, &off,
  "textfield:disabled\n  background: %s29\n  color: %s52\n", ons, ons);
  cogito_sum_append(out, cap, &off,
  "textfield:focus\n  border: 2 solid %s\n", p);
  cogito_sum_append(out, cap, &off,
  "textfield:hover\n  background: %s\n", surfvar);
  cogito_sum_append(out, cap, &off,
  "textfield:selection, .textfield:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "textfield\n  background: %s\n  color: %s\n  border-radius: 4 4 0 0\n  border: 0 none #000000\n  min-height: 56\n  padding: 8 16\n  transition: 180 standard\n", sc, ons);
  cogito_sum_append(out, cap, &off,
  "textview:focus\n  border: 2 solid %s\n", p);
  cogito_sum_append(out, cap, &off,
  "textview:selection, .textview:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "textview\n  background: %s\n  color: %s\n  border-radius: 8\n  min-height: 96\n  transition: 180 standard\n", sc, ons);
  cogito_sum_append(out, cap, &off,
  "timepicker:hover\n  background: %s\n", surfvar);
  cogito_sum_append(out, cap, &off,
  "timepicker:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off,
  "timepicker\n  background: %s\n  color: %s\n  border: 1 solid %s\n  border-radius: 8\n  min-height: 56\n  min-width: 220\n  transition: 180 standard\n", sc, ons, outl);
  cogito_sum_append(out, cap, &off,
  "tip_view\n  background: %s\n  color: %s\n  border-radius: 8\n  padding: 12\n", secc, osecc);
  cogito_sum_append(out, cap, &off,
  "toast:selection\n  background: %s\n", invp);
  cogito_sum_append(out, cap, &off,
  "toast\n  background: %s\n  color: %s\n  border-radius: 999\n  min-height: 48\n  padding: 0 16\n  transition: 220 standard\n", invsrf, invons);
  cogito_sum_append(out, cap, &off,
  "toasts\n  padding: 12\n");
  cogito_sum_append(out, cap, &off,
  "toolbar.floating_compact\n  border-radius: 999\n  border: 1 solid %s\n  elevation: 1\n  padding: 8\n", outl);
  cogito_sum_append(out, cap, &off,
  "toolbar.floating\n  border-radius: 999\n  border: 1 solid %s\n  elevation: 1\n", outl);
  cogito_sum_append(out, cap, &off,
  "toolbar\n  background: %s\n  color: %s\n  min-height: 64\n  padding: 8 16\n", sc, ons);
  cogito_sum_append(out, cap, &off,
  "tooltip\n  background: %s\n  color: %s\n  border-radius: 6\n  font-size: 14\n  padding: 6 8\n", invsrf, invons);
  cogito_sum_append(out, cap, &off,
  "treeview\n  background: %s\n  color: %s\n  border-radius: 8\n  min-height: 180\n  min-width: 200\n  padding: 8\n", sc, ons);
  cogito_sum_append(out, cap, &off,
  "view_chooser\n  background: %s\n  color: %s\n  border-radius: 8\n  min-height: 48\n", surf, ons);
  cogito_sum_append(out, cap, &off,
  "view_dual\n  background: %s\n", surf);
  cogito_sum_append(out, cap, &off,
  "view_switcher\n  background: %s\n  color: %s\n", surf, ons);
  cogito_sum_append(out, cap, &off,
  "viewswitcher\n  background: %s\n  color: %s\n", surf, ons);
  cogito_sum_append(out, cap, &off,
  "vstack.sidebar\n  background: %s\n  color: %s\n", sc, ons);
  cogito_sum_append(out, cap, &off,
  "vstack\n  background: %s\n  color: %s\n", bg, ons);
  cogito_sum_append(out, cap, &off,
  "welcome_screen\n  background: %s\n  color: %s\n", bg, ons);
  cogito_sum_append(out, cap, &off,
  "window\n  background: %s\n", surf);
  cogito_sum_append(out, cap, &off,
  "zstack\n  background: %s\n  color: %s\n", bg, ons);

  // --- Selectors that exist in system .sum files but were previously missing ---
  cogito_sum_append(out, cap, &off,
  ".on-error\n  background: %s\n  color: %s\n", err, oerr);
  cogito_sum_append(out, cap, &off,
  ".about-window\n  background: %s\n  color: %s\n", sch, ons);
  cogito_sum_append(out, cap, &off,
  "label.about-window-title\n  color: %s\n  font-size: 28\n  font-weight: bold\n", ons);
  cogito_sum_append(out, cap, &off,
  "label.about-window-license\n  color: %s\n  font-size: 14\n", outl);
  cogito_sum_append(out, cap, &off,
  "hstack.about-window-actions\n  background: transparent\n  padding: 4 0 0 0\n");
  cogito_sum_append(out, cap, &off,
  "image.about-window-icon\n  min-width: 128\n  min-height: 128\n");
  cogito_sum_append(out, cap, &off,
  "menu_section\n  background: transparent\n");
  cogito_sum_append(out, cap, &off,
  "menu_section item\n  border-radius: 0\n  padding: 8 12\n  min-height: 42\n");
  cogito_sum_append(out, cap, &off,
  "switch .knob:checked\n  background: %s\n", surf);
  cogito_sum_append(out, cap, &off,
  "switch knob:checked\n  background: %s\n", surf);
}