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

static void sum_build_dynamic(bool dark, CogitoColor accent, char* out, size_t cap) {
  size_t off = 0;
  if (cap == 0) return;
  out[0] = 0;

  double h = 0.0, c = 0.0, t = 0.0;
  cogito_rgb_to_hct(accent, &h, &c, &t);

  // === Full Color Token System ===

  // Primary palette
  double primary_c = c < 48.0 ? 48.0 : c;
  CogitoColor primary = cogito_hct_to_rgb(h, primary_c, dark ? 80.0 : 40.0);
  CogitoColor on_primary = dark
    ? cogito_hct_to_rgb(h, primary_c, 20.0)
    : cogito_rgba(255, 255, 255, 255);
  CogitoColor primary_container = cogito_hct_to_rgb(h, primary_c, dark ? 30.0 : 90.0);
  CogitoColor on_primary_container = cogito_hct_to_rgb(h, primary_c, dark ? 90.0 : 10.0);

  // Secondary palette
  double secondary_c = 16.0;
  CogitoColor secondary = cogito_hct_to_rgb(h, secondary_c, dark ? 80.0 : 40.0);
  CogitoColor on_secondary = dark ? cogito_hct_to_rgb(h, secondary_c, 20.0) : cogito_rgba(255, 255, 255, 255);
  CogitoColor secondary_container = cogito_hct_to_rgb(h, secondary_c, dark ? 30.0 : 90.0);
  CogitoColor on_secondary_container = cogito_hct_to_rgb(h, secondary_c, dark ? 90.0 : 10.0);

  // Tertiary palette
  double tert_h = fmod(h + 60.0, 360.0);
  double tert_c = 24.0;
  CogitoColor tertiary = cogito_hct_to_rgb(tert_h, tert_c, dark ? 80.0 : 40.0);
  CogitoColor on_tertiary = dark ? cogito_hct_to_rgb(h, tert_c, 20.0) : cogito_rgba(255, 255, 255, 255);
  CogitoColor tertiary_container = cogito_hct_to_rgb(tert_h, tert_c, dark ? 30.0 : 90.0);
  CogitoColor on_tertiary_container = cogito_hct_to_rgb(tert_h, tert_c, dark ? 90.0 : 10.0);

  // Error palette
  CogitoColor error = cogito_hct_to_rgb(20.0, 60.0, dark ? 80.0 : 40.0);
  CogitoColor on_error = dark ? cogito_hct_to_rgb(20.0, 60.0, 20.0) : cogito_rgba(255, 255, 255, 255);
  CogitoColor error_container = cogito_hct_to_rgb(20.0, 60.0, dark ? 30.0 : 90.0);
  CogitoColor on_error_container = cogito_hct_to_rgb(20.0, 60.0, dark ? 90.0 : 10.0);

  // Neutral palette
  double neutral_c = 4.0;
  CogitoColor background = cogito_hct_to_rgb(h, neutral_c, dark ? 14.0 : 98.0);
  CogitoColor on_background = cogito_hct_to_rgb(h, neutral_c, dark ? 90.0 : 10.0);
  CogitoColor surface = cogito_hct_to_rgb(h, neutral_c, dark ? 14.0 : 98.0);
  CogitoColor on_surface = cogito_hct_to_rgb(h, neutral_c, dark ? 90.0 : 10.0);
  CogitoColor surface_variant = cogito_hct_to_rgb(h, neutral_c + 4.0, dark ? 30.0 : 90.0);
  CogitoColor on_surface_variant = cogito_hct_to_rgb(h, neutral_c + 4.0, dark ? 80.0 : 30.0);
  CogitoColor surface_tint = cogito_mix(primary, surface, 0.05f);

  // Surface containers
  CogitoColor surface_dim = cogito_hct_to_rgb(h, neutral_c, dark ? 4.0 : 87.0);
  CogitoColor surface_bright = cogito_hct_to_rgb(h, neutral_c, dark ? 24.0 : 98.0);
  CogitoColor surface_cont_lowest = cogito_hct_to_rgb(h, neutral_c, dark ? 4.0 : 100.0);
  CogitoColor surface_cont_low = cogito_hct_to_rgb(h, neutral_c, dark ? 10.0 : 96.0);
  CogitoColor surface_cont = cogito_hct_to_rgb(h, neutral_c, dark ? 12.0 : 94.0);
  CogitoColor surface_cont_high = cogito_hct_to_rgb(h, neutral_c, dark ? 17.0 : 92.0);
  CogitoColor surface_cont_highest = cogito_hct_to_rgb(h, neutral_c, dark ? 22.0 : 90.0);

  // Outline
  CogitoColor outline = cogito_hct_to_rgb(h, neutral_c, dark ? 60.0 : 50.0);
  CogitoColor outline_variant = cogito_hct_to_rgb(h, neutral_c, dark ? 30.0 : 80.0);

  // Inverse colors
  CogitoColor inverse_surface = cogito_hct_to_rgb(h, neutral_c, dark ? 90.0 : 20.0);
  CogitoColor inverse_on_surface = cogito_hct_to_rgb(h, neutral_c, dark ? 20.0 : 95.0);
  CogitoColor inverse_primary = cogito_hct_to_rgb(h, primary_c, dark ? 40.0 : 80.0);

  // Shadow & Scrim
  CogitoColor shadow = cogito_hct_to_rgb(h, 0.0, 0.0);
  CogitoColor scrim = cogito_hct_to_rgb(h, 0.0, 0.0);

  // === Hex conversions ===
  char p[8], op[8], pc[8], opc[8];
  char sec[8], osec[8], secc[8], osecc[8];
  char tert[8], otert[8], tertc[8], otertc[8];
  char err[8], oerr[8], errc[8], oerrc[8];
  char bg[8], obg[8], surf[8], surft[8], ons[8];
  char outl[8], outlv[8];
  char surfvar[8], osurfvar[8];
  char surfdim[8], surfbrt[8];
  char scl[8], sclo[8], sc[8], sch[8], schi[8];
  char invsrf[8], invons[8], invp[8];
  char shad[8], scrm[8];

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

  // === Component aliases from color tokens only ===
  const char* ph = pc;
  const char* pa = p;
  const char* surfth = surfvar;
  const char* navbg = sc;
  const char* navsel = secc;
  const char* bottombg = sc;
  const char* bottomsel = secc;
  const char* tabsbg = sc;
  const char* tabssel = pc;
  const char* progbg = outlv;
  const char* toastbg = invsrf;
  const char* toastfg = invons;
  const char* iconbg = sclo;
  const char* tooltipbg = invsrf;
  const char* tooltipfg = invons;
  const char* checkhov = osurfvar;
  const char* checkact = p;
  const char* strack = outlv;
  const char* strackh = outl;
  const char* knob = surf;
  const char* knobh = surfvar;
  const char* knoba = ons;
  const char* appiconbg = sc;
  const char* chiphov = sclo;
  const char* chipact = sc;
  const char* chipsel = secc;

  // === SUM output ===
  cogito_sum_append(out, cap, &off, "*\n  color: %s\n  font-size: 14\n", ons);

  cogito_sum_append(out, cap, &off, ".surface\n  background: %s\n", surf);
  cogito_sum_append(out, cap, &off, ".surface-container-high\n  background: %s\n", sch);
  cogito_sum_append(out, cap, &off, ".surface-container-highest\n  background: %s\n", schi);
  cogito_sum_append(out, cap, &off, ".surface-container-low\n  background: %s\n", sclo);
  cogito_sum_append(out, cap, &off, ".surface-container-lowest\n  background: %s\n", scl);
  cogito_sum_append(out, cap, &off, ".surface-container\n  background: %s\n", sc);
  cogito_sum_append(out, cap, &off, ".surface-bright\n  background: %s\n", surfbrt);
  cogito_sum_append(out, cap, &off, ".surface-dim\n  background: %s\n", surfdim);
  cogito_sum_append(out, cap, &off, ".surface-variant\n  background: %s\n  color: %s\n", surfvar, osurfvar);

  cogito_sum_append(out, cap, &off, ".inverse-primary\n  color: %s\n", invp);
  cogito_sum_append(out, cap, &off, ".inverse-surface\n  background: %s\n  color: %s\n", invsrf, invons);

  cogito_sum_append(out, cap, &off, ".error\n  background: %s\n  color: %s\n", err, oerr);
  cogito_sum_append(out, cap, &off, ".error-container\n  background: %s\n  color: %s\n", errc, oerrc);

  cogito_sum_append(out, cap, &off, ".primary\n  background: %s\n  color: %s\n", p, op);
  cogito_sum_append(out, cap, &off, ".primary-container\n  background: %s\n  color: %s\n", pc, opc);
  cogito_sum_append(out, cap, &off, ".secondary\n  background: %s\n  color: %s\n", sec, osec);
  cogito_sum_append(out, cap, &off, ".secondary-container\n  background: %s\n  color: %s\n", secc, osecc);
  cogito_sum_append(out, cap, &off, ".tertiary\n  background: %s\n  color: %s\n", tert, otert);
  cogito_sum_append(out, cap, &off, ".tertiary-container\n  background: %s\n  color: %s\n", tertc, otertc);

  cogito_sum_append(out, cap, &off, ".outline\n  border-color: %s\n", outl);
  cogito_sum_append(out, cap, &off, ".outline-variant\n  border-color: %s\n", outlv);

  cogito_sum_append(out, cap, &off, ".scrim\n  background: %s\n", scrm);
  cogito_sum_append(out, cap, &off, ".shadow\n  background: %s\n", scrm);

  cogito_sum_append(out, cap, &off, ".title\n  color: %s\n  font-size: 32\n  font-weight: normal\n", ons);
  cogito_sum_append(out, cap, &off, ".subtitle\n  color: %s\n  font-size: 20\n  font-weight: normal\n", ons);
  cogito_sum_append(out, cap, &off, ".body\n  color: %s\n  font-size: 14\n  font-weight: normal\n", ons);
  cogito_sum_append(out, cap, &off, ".caption\n  color: %s\n  font-size: 11\n  font-weight: normal\n", ons);
  cogito_sum_append(out, cap, &off, ".tabular\n  font-size: 13\n  font-weight: normal\n");
  cogito_sum_append(out, cap, &off, ".monospace\n  font-size: 13\n  font-weight: normal\n");

  cogito_sum_append(out, cap, &off, "about_window\n  background: %s\n  color: %s\n  border-radius: 26\n  padding: 24\n", surf, ons);
  cogito_sum_append(out, cap, &off, "active_indicator\n  color: %s\n", p);
  cogito_sum_append(out, cap, &off, "appbar .button:active\n  background: %s\n", outl);
  cogito_sum_append(out, cap, &off, "appbar .button:hover\n  background: %s\n", surf);
  cogito_sum_append(out, cap, &off, "appbar .iconbtn:active\n  background: %s\n", bg);
  cogito_sum_append(out, cap, &off, "appbar .iconbtn:hover\n  background: %s\n", outl);
  cogito_sum_append(out, cap, &off, "appbar .iconbtn\n  background: %s\n", appiconbg);
  cogito_sum_append(out, cap, &off, "appbar .window-close:active\n  background: %s\n", secc);
  cogito_sum_append(out, cap, &off, "appbar .window-close:hover\n  background: %s\n", secc);
  cogito_sum_append(out, cap, &off, "appbar .window-close\n  background: %s\n", secc);
  cogito_sum_append(out, cap, &off, "appbar .window-max:active\n  background: %s\n", secc);
  cogito_sum_append(out, cap, &off, "appbar .window-max:hover\n  background: %s\n", secc);
  cogito_sum_append(out, cap, &off, "appbar .window-max\n  background: %s\n", secc);
  cogito_sum_append(out, cap, &off, "appbar .window-min:active\n  background: %s\n", secc);
  cogito_sum_append(out, cap, &off, "appbar .window-min:hover\n  background: %s\n", secc);
  cogito_sum_append(out, cap, &off, "appbar .window-min\n  background: %s\n", secc);
  cogito_sum_append(out, cap, &off, "appbar.sidebar\n  background: %s\n  color: %s\n", sc, ons);
  cogito_sum_append(out, cap, &off, "appbar.view\n  background: %s\n  color: %s\n", bg, ons);
  cogito_sum_append(out, cap, &off, "appbar\n  background: %s\n  color: %s\n", bg, ons);
  cogito_sum_append(out, cap, &off, "avatar\n  background: %s\n  color: %s\n  min-width: 64\n  min-height: 64\n", secc, osecc);
  cogito_sum_append(out, cap, &off, "badge\n  background: %s\n  color: %s\n  font-size: 11\n  min-width: 10\n  min-height: 10\n", err, oerr);
  cogito_sum_append(out, cap, &off, "banner:selection\n  color: %s\n", p);
  cogito_sum_append(out, cap, &off, "banner\n  background: %s\n  color: %s\n  padding: 16\n  min-height: 52\n  transition: 200 standard\n", secc, osecc);
  cogito_sum_append(out, cap, &off, "bottom_sheet\n  background: %s\n  color: %s\n  border-radius: 28 28 0 0\n  padding: 16\n  transition: 300 emphasized\n", surf, ons);
  cogito_sum_append(out, cap, &off, "bottom_toolbar\n  background: %s\n  color: %s\n  min-height: 56\n  padding: 8 16\n", surf, ons);
  cogito_sum_append(out, cap, &off, "bottom-nav:selection\n  background: %s\n", bottomsel);
  cogito_sum_append(out, cap, &off, "bottom-nav\n  background: %s\n  color: %s\n  min-height: 80\n  padding: 0 8\n  transition: 220 standard\n", bottombg, ons);
  cogito_sum_append(out, cap, &off, "bottom-sheet\n  background: %s\n  color: %s\n  border-radius: 28 28 0 0\n  padding: 16\n  transition: 300 emphasized\n", surf, ons);
  cogito_sum_append(out, cap, &off, "button:active\n  background: %s\n  color: %s\n", pa, op);
  cogito_sum_append(out, cap, &off, "button:focus, iconbtn:focus, checkbox:focus, switch:focus, slider:focus, dropdown:focus, chip:focus, segmented:focus, stepper:focus, fab:focus, nav_rail:focus, bottom_nav:focus, datepicker:focus, colorpicker:focus, timepicker:focus\n  border-color: %s\n  border-width: 2\n", p);
  cogito_sum_append(out, cap, &off, "button:hover\n  background: %s\n  color: %s\n", ph, op);
  cogito_sum_append(out, cap, &off, "button.outlined:active\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off, "button.outlined:disabled\n  color: %s61\n  border-color: %s12\n", ons, ons);
  cogito_sum_append(out, cap, &off, "button.outlined:hover\n  background: %s\n  border-color: %s\n", p, p);
  cogito_sum_append(out, cap, &off, "button.outlined\n  background: transparent\n  color: %s\n  border-radius: 100\n  min-height: 56\n  border: 1 solid %s\n", p, outl);
  cogito_sum_append(out, cap, &off, "button.text:active\n  background: %s1f\n", p);
  cogito_sum_append(out, cap, &off, "button.text:disabled\n  color: %s61\n", ons);
  cogito_sum_append(out, cap, &off, "button.text:hover\n  background: %s12\n  color: %s\n", p, ons);
  cogito_sum_append(out, cap, &off, "button.text\n  background: transparent\n  color: %s\n  border-radius: 100\n  min-height: 56\n  box-shadow: none\n  padding: 10 12\n", p);
  cogito_sum_append(out, cap, &off, "button\n  background: %s\n  color: %s\n  border-radius: 100\n  font-weight: bold\n  min-width: 120\n  min-height: 56\n  padding: 10 24\n  transition: 180 standard\n", p, op);
  cogito_sum_append(out, cap, &off, "card:hover\n  box-shadow: [ 0 2 8 0 #0000002a ]\n");
  cogito_sum_append(out, cap, &off, "card\n  background: %s\n  color: %s\n  border-radius: 12\n  padding: 16\n  transition: 200 standard\n", sc, ons);
  cogito_sum_append(out, cap, &off, "carousel_item\n  background: %s\n  color: %s\n  border-radius: 26\n  padding: 16\n", sc, ons);
  cogito_sum_append(out, cap, &off, "carousel_item\n  background: %s\n", surf);
  cogito_sum_append(out, cap, &off, "carousel\n  background: %s\n  color: %s\n  min-height: 240\n  min-width: 400\n  border-radius: 26\n", surf, ons);
  cogito_sum_append(out, cap, &off, "checkbox .box:active\n  border-color: %s\n", p);
  cogito_sum_append(out, cap, &off, "checkbox .box:hover\n  border-color: %s\n", ons);
  cogito_sum_append(out, cap, &off, "checkbox .box\n  background: transparent\n  border-radius: 4\n  border: 2 solid %s\n", outl);
  cogito_sum_append(out, cap, &off, "checkbox .box\n  background: transparent\n  border-radius: 4\n  border: 2 solid %s\n", outl);
  cogito_sum_append(out, cap, &off, "checkbox .check:active\n  color: %s\n", checkact);
  cogito_sum_append(out, cap, &off, "checkbox .check:hover\n  color: %s\n", checkhov);
  cogito_sum_append(out, cap, &off, "checkbox .check\n  color: %s\n", ons);
  cogito_sum_append(out, cap, &off, "checkbox:checked .box\n  background: %s\n  border: 0 none %s\n", p, p);
  cogito_sum_append(out, cap, &off, "checkbox:checked .check\n  color: %s\n", ons);
  cogito_sum_append(out, cap, &off, "checkbox:checked\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off, "checkbox\n  color: %s\n  min-height: 24\n  min-width: 24\n  transition: 120 standard\n", ons);
  cogito_sum_append(out, cap, &off, "checkbox\n  color: %s\n  min-height: 24\n  min-width: 24\n  transition: 120 standard\n", ons);
  cogito_sum_append(out, cap, &off, "chip:active\n  background: %s\n", chipact);
  cogito_sum_append(out, cap, &off, "chip:checked\n  background: %s\n  color: %s\n  border: 0 none %s\n", chipsel, ons, chipsel);
  cogito_sum_append(out, cap, &off, "chip:hover\n  background: %s\n  border-color: %s\n", chiphov, ons);
  cogito_sum_append(out, cap, &off, "chip\n  background: transparent\n  color: %s\n  border-radius: 100\n  min-height: 32\n  padding: 0 16\n  border: 1 solid %s\n  transition: 150 standard\n", ons, outl);
  cogito_sum_append(out, cap, &off, "colorpicker:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off, "colorpicker\n  background: %s\n  color: %s\n  border-radius: 18\n  min-height: 140\n  padding: 12\n  track-height: 4\n", sc, ons);
  cogito_sum_append(out, cap, &off, "content_list label\n  padding: 0 16\n  min-height: 80\n  background: transparent\n");
  cogito_sum_append(out, cap, &off, "content_list\n  background: %s\n  color: %s\n  border-radius: 12\n", surfvar, ons);
  cogito_sum_append(out, cap, &off, "content_list=first\n  border-radius: 12 12 8 8\n");
  cogito_sum_append(out, cap, &off, "content_list=last\n  border-radius: 8 8 12 12\n");
  cogito_sum_append(out, cap, &off, "content_list=middle\n  border-radius: 8\n");
  cogito_sum_append(out, cap, &off, "content_list=single\n  border-radius: 12\n");
  cogito_sum_append(out, cap, &off, "datepicker:hover, .datepicker:hover\n  background: %s\n", surfth);
  cogito_sum_append(out, cap, &off, "datepicker, .datepicker\n  background: %s\n  color: %s\n  border: 1 solid %s\n  border-radius: 8\n  min-height: 56\n  min-width: 220\n  transition: 180 standard\n", sc, ons, outl);
  cogito_sum_append(out, cap, &off, "datepicker:selection, .datepicker:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off, "dialog\n  background: %s\n  color: %s\n  border-radius: 26\n  min-width: 280\n  min-height: 160\n  padding: 24\n", surf, ons);
  cogito_sum_append(out, cap, &off, "dialogslot\n  background: %s66\n  color: %s\n", scrm, ons);
  cogito_sum_append(out, cap, &off, "divider\n  background: %s\n  min-height: 1\n", outlv);
  cogito_sum_append(out, cap, &off, "dropdown:hover\n  background: %s\n", surfth);
  cogito_sum_append(out, cap, &off, "dropdown\n  background: %s\n  color: %s\n  border-radius: 4 4 0 0\n  min-height: 56\n  padding: 8 16\n  transition: 180 standard\n", sc, ons);
  cogito_sum_append(out, cap, &off, "empty_page\n  background: transparent\n  color: %s\n", ons);
  cogito_sum_append(out, cap, &off, "fab:active\n  background: %s\n  color: %s\n", pa, op);
  cogito_sum_append(out, cap, &off, "fab:hover\n  background: %s\n  color: %s\n", ph, op);
  cogito_sum_append(out, cap, &off, "fab\n  background: %s\n  color: %s\n  border-radius: 100\n  min-width: 56\n  min-height: 56\n  transition: 180 standard\n", p, op);
  cogito_sum_append(out, cap, &off, "fixed\n  background: transparent\n  color: %s\n", ons);
  cogito_sum_append(out, cap, &off, "grid:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off, "grid\n  background: %s\n  color: %s\n  border-radius: 12\n", sc, ons);
  cogito_sum_append(out, cap, &off, "hstack\n  background: %s\n  color: %s\n", bg, ons);
  cogito_sum_append(out, cap, &off, "iconbtn:active\n  background: %s\n  border-radius: 99\n  min-width: 48\n  min-height: 48\n  color: %s\n", p, op);
  cogito_sum_append(out, cap, &off, "iconbtn:hover\n  background: %s\n  border-radius: 99\n  min-width: 48\n  min-height: 48\n  color: %s\n", outl, ons);
  cogito_sum_append(out, cap, &off, "iconbtn\n  background: %s\n  border-radius: 99\n  min-width: 48\n  min-height: 48\n  color: %s\n  transition: 150 standard\n", iconbg, ons);
  cogito_sum_append(out, cap, &off, "image\n  background: transparent\n");
  cogito_sum_append(out, cap, &off, "label.carousel-item-label\n  font-size: 21\n  font-weight: bold\n  color: #ffffff\n");
  cogito_sum_append(out, cap, &off, "label\n  color: %s\n", ons);
  cogito_sum_append(out, cap, &off, "list:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off, "list\n  background: transparent\n  color: %s\n", ons);
  cogito_sum_append(out, cap, &off, "menu item\n  border-radius: 0\n  padding: 8 12\n  min-height: 42\n");
  cogito_sum_append(out, cap, &off, "menu_button\n  background: transparent\n  color: %s\n  padding: 8 12\n", ons);
  cogito_sum_append(out, cap, &off, "menu:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off, "menu\n  background: %s\n  border-radius: 4\n  color: %s\n  box-shadow: 2\n  font-size: 13\n", sc, ons);
  cogito_sum_append(out, cap, &off, "nav-rail:selection\n  background: %s\n", navsel);
  cogito_sum_append(out, cap, &off, "nav-rail\n  background: %s\n  color: %s\n  min-width: 80\n  padding: 4 0\n  transition: 220 standard\n", navbg, ons);
  cogito_sum_append(out, cap, &off, "popover\n  background: %s\n  border-radius: 12\n  padding: 16\n  box-shadow: [ 0 2 8 0 #0000001f ]\n", sc);
  cogito_sum_append(out, cap, &off, "progress:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off, "progress\n  background: %s\n  border-radius: 100\n  track-height: 4\n  min-height: 12\n  min-width: 120\n  transition: 200 standard\n", progbg);
  cogito_sum_append(out, cap, &off, "radio .box:active\n  border-color: %s\n", p);
  cogito_sum_append(out, cap, &off, "radio .box:hover\n  border-color: %s\n", ons);
  cogito_sum_append(out, cap, &off, "radio .box\n  background: transparent\n  border-radius: 100\n  border: 2 solid %s\n", outl);
  cogito_sum_append(out, cap, &off, "radio .box\n  background: transparent\n  border-radius: 100\n  border: 2 solid %s\n", outl);
  cogito_sum_append(out, cap, &off, "radio .check:active\n  color: %s\n", checkact);
  cogito_sum_append(out, cap, &off, "radio .check:hover\n  color: %s\n", checkhov);
  cogito_sum_append(out, cap, &off, "radio .check\n  color: %s\n", ons);
  cogito_sum_append(out, cap, &off, "radio:checked .box\n  background: %s\n  border: 0 none %s\n", p, p);
  cogito_sum_append(out, cap, &off, "radio:checked .check\n  color: %s\n", ons);
  cogito_sum_append(out, cap, &off, "radio:checked\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off, "radio\n  color: %s\n  min-height: 24\n  min-width: 24\n  transition: 120 standard\n", ons);
  cogito_sum_append(out, cap, &off, "scroller.sidebar\n  background: %s\n  color: %s\n", sc, ons);
  cogito_sum_append(out, cap, &off, "scroller\n  background: %s\n  color: %s\n", bg, ons);
  cogito_sum_append(out, cap, &off, "searchfield:active\n  border-color: %s\n  border-width: 2\n", p);
  cogito_sum_append(out, cap, &off, "searchfield:focus\n  border: 2 solid %s\n", p);
  cogito_sum_append(out, cap, &off, "searchfield:hover\n  background: %s\n  border-color: %s\n", surfth, ons);
  cogito_sum_append(out, cap, &off, "searchfield:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off, "searchfield\n  background: %s\n  color: %s\n  border-radius: 18\n  min-height: 42\n  min-width: 180\n  padding: 6 12\n  transition: 180 standard\n", sc, ons);
  cogito_sum_append(out, cap, &off, "segmented:selection\n  background: %s\n", secc);
  cogito_sum_append(out, cap, &off, "segmented\n  background: %s\n  color: %s\n  border-radius: 100\n  min-height: 56\n  border: 0 none #000000\n  transition: 180 standard\n", sec, osec);
  cogito_sum_append(out, cap, &off, "settings_list\n  background: %s\n  color: %s\n  border-radius: 12\n", surf, ons);
  cogito_sum_append(out, cap, &off, "settings_page\n  background: %s\n  color: %s\n", bg, ons);
  cogito_sum_append(out, cap, &off, "settings_row\n  background: transparent\n  color: %s\n  min-height: 56\n  padding: 8 16\n", ons);
  cogito_sum_append(out, cap, &off, "settings_window\n  background: %s\n  color: %s\n", bg, ons);
  cogito_sum_append(out, cap, &off, "slider:active\n  color: %s\n", pa);
  cogito_sum_append(out, cap, &off, "slider:hover\n  color: %s\n", ph);
  cogito_sum_append(out, cap, &off, "slider\n  background: %s\n  color: %s\n  track-height: 4\n  min-height: 48\n  min-width: 200\n  transition: 120 standard\n", secc, p);
  cogito_sum_append(out, cap, &off, "split_button\n  background: %s\n  color: %s\n  border-radius: 20\n", p, op);
  cogito_sum_append(out, cap, &off, "stepper\n  background: %s\n  color: %s\n  border-radius: 100\n  min-height: 42\n  min-width: 120\n  border: 1 solid %s\n  padding: 4 8\n  transition: 150 standard\n", surf, ons, outl);
  cogito_sum_append(out, cap, &off, "switch .knob:active\n  background: %s\n", knoba);
  cogito_sum_append(out, cap, &off, "switch .knob:hover\n  background: %s\n", knobh);
  cogito_sum_append(out, cap, &off, "switch .knob\n  background: %s\n  min-width: 32\n  min-height: 24\n", knob);
  cogito_sum_append(out, cap, &off, "switch .track:active\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off, "switch .track:hover\n  background: %s\n", strackh);
  cogito_sum_append(out, cap, &off, "switch .track\n  background: %s\n", strack);
  cogito_sum_append(out, cap, &off, "switch knob:active\n  background: %s\n", knoba);
  cogito_sum_append(out, cap, &off, "switch knob:hover\n  background: %s\n", knobh);
  cogito_sum_append(out, cap, &off, "switch knob\n  background: %s\n  min-width: 32\n  min-height: 24\n", knob);
  cogito_sum_append(out, cap, &off, "switch track:active\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off, "switch track:hover\n  background: %s\n", strackh);
  cogito_sum_append(out, cap, &off, "switch track\n  background: %s\n", strack);
  cogito_sum_append(out, cap, &off, "switch:checked .track\n  background: %s\n  border-radius: 100\n", p);
  cogito_sum_append(out, cap, &off, "switch:checked track\n  background: %s\n  border-radius: 100\n", p);
  cogito_sum_append(out, cap, &off, "switch\n  color: %s\n  min-height: 28\n  min-width: 64\n  transition: 100 standard\n", ons);
  cogito_sum_append(out, cap, &off, "switchbar:active\n  background: %s\n  color: %s\n  border-color: %s\n", pc, opc, pc);
  cogito_sum_append(out, cap, &off, "switchbar\n  background: %s\n  color: %s\n  border: 1 solid %s\n  border-radius: 12\n  min-height: 56\n", surf, ons, outl);
  cogito_sum_append(out, cap, &off, "tabs:selection\n  background: %s\n", tabssel);
  cogito_sum_append(out, cap, &off, "tabs\n  background: %s\n  color: %s\n  border-radius: 10\n  min-height: 42\n  transition: 180 standard\n", tabsbg, ons);
  cogito_sum_append(out, cap, &off, "textfield:focus\n  border: 2 solid %s\n", p);
  cogito_sum_append(out, cap, &off, "textfield:hover\n  background: %s\n", surfth);
  cogito_sum_append(out, cap, &off, "textfield:selection, .textfield:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off, "textfield\n  background: %s\n  color: %s\n  border-radius: 4 4 0 0\n  border: 0 none #000000\n  min-height: 56\n  padding: 8 16\n  transition: 180 standard\n", sc, ons);
  cogito_sum_append(out, cap, &off, "textview:focus\n  border: 2 solid %s\n", p);
  cogito_sum_append(out, cap, &off, "textview:selection, .textview:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off, "textview\n  background: %s\n  color: %s\n  border-radius: 8\n  min-height: 96\n  transition: 180 standard\n", sc, ons);
  cogito_sum_append(out, cap, &off, "timepicker:selection\n  background: %s\n", p);
  cogito_sum_append(out, cap, &off, "timepicker:hover\n  background: %s\n", surfth);
  cogito_sum_append(out, cap, &off, "timepicker\n  background: %s\n  color: %s\n  border: 1 solid %s\n  border-radius: 8\n  min-height: 56\n  min-width: 220\n  transition: 180 standard\n", sc, ons, outl);
  cogito_sum_append(out, cap, &off, "tip_view\n  background: %s\n  color: %s\n  border-radius: 8\n  padding: 12\n", secc, osecc);
  cogito_sum_append(out, cap, &off, "toast:selection\n  background: %s\n", invp);
  cogito_sum_append(out, cap, &off, "toast\n  background: %s\n  color: %s\n  border-radius: 10\n  min-height: 34\n  padding: 0 12\n  transition: 220 standard\n", toastbg, toastfg);
  cogito_sum_append(out, cap, &off, "toasts\n  padding: 12\n");
  cogito_sum_append(out, cap, &off, "toolbar\n  background: %s\n  color: %s\n  min-height: 44\n  padding: 8 12\n", surf, ons);
  cogito_sum_append(out, cap, &off, "tooltip\n  background: %s\n  color: %s\n  border-radius: 6\n  font-size: 13\n  padding: 6 8\n", tooltipbg, tooltipfg);
  cogito_sum_append(out, cap, &off, "treeview\n  background: %s\n  color: %s\n  border-radius: 8\n  min-height: 180\n  min-width: 200\n  padding: 8\n", sc, ons);
  cogito_sum_append(out, cap, &off, "view_chooser\n  background: %s\n  color: %s\n  border-radius: 8\n  min-height: 48\n", surf, ons);
  cogito_sum_append(out, cap, &off, "view_dual\n  background: %s\n", surf);
  cogito_sum_append(out, cap, &off, "view_switcher\n  background: %s\n  color: %s\n", surf, ons);
  cogito_sum_append(out, cap, &off, "viewswitcher\n  background: %s\n  color: %s\n", surf, ons);
  cogito_sum_append(out, cap, &off, "vstack.sidebar\n  background: %s\n  color: %s\n", sc, ons);
  cogito_sum_append(out, cap, &off, "vstack\n  background: %s\n  color: %s\n", bg, ons);
  cogito_sum_append(out, cap, &off, "welcome_screen\n  background: %s\n  color: %s\n", bg, ons);
  cogito_sum_append(out, cap, &off, "window\n  background: %s\n", surf);
  cogito_sum_append(out, cap, &off, "zstack\n  background: %s\n  color: %s\n", bg, ons);
}
