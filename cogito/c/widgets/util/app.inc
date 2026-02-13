// CogitoApp lifecycle management
// App object creation, accent color, system theme detection.

static void cogito_app_drop(ErgoObj* o) {
  CogitoApp* app = (CogitoApp*)o;
  if (app->app_id) {
    ergo_release_val(EV_STR(app->app_id));
    app->app_id = NULL;
  }
  if (app->app_name) {
    ergo_release_val(EV_STR(app->app_name));
    app->app_name = NULL;
  }
}

static CogitoApp* cogito_app_new_obj(void) {
  CogitoApp* app = (CogitoApp*)ergo_obj_new(sizeof(CogitoApp), cogito_app_drop);
  app->native = NULL;
  app->app_id = NULL;
  app->app_name = NULL;
  app->accent_override = false;
  app->accent_color = cogito_rgba(114, 222, 194, 255);
  app->accent_set = false;
  app->system_dark = false;
  app->system_accent = cogito_rgba(114, 222, 194, 255);
  app->system_accent_set = false;
  return app;
}

static CogitoColor cogito_app_effective_accent(const CogitoApp* app) {
  if (app->accent_override && app->accent_set) return app->accent_color;
  if (app->system_accent_set) return app->system_accent;
  if (app->accent_set) return app->accent_color;
  return cogito_rgba(114, 222, 194, 255);
}

static void cogito_app_update_theme(CogitoApp* app) {
  bool dark = app ? app->system_dark : false;
  CogitoColor accent = app ? cogito_app_effective_accent(app) : cogito_rgba(114, 222, 194, 255);
  if (dark != cogito_theme_dark ||
      accent.r != cogito_theme_accent.r || accent.g != cogito_theme_accent.g ||
      accent.b != cogito_theme_accent.b) {
    cogito_apply_theme(dark, accent);
  }
}

static void cogito_update_system_theme(CogitoApp* app) {
  if (!app) return;
  bool dark = cogito_system_is_dark();
  CogitoColor acc = {0};
  bool has_acc = cogito_system_accent(&acc);
  bool changed = false;
  if (dark != app->system_dark) {
    app->system_dark = dark;
    changed = true;
  }
  if (has_acc && (!app->system_accent_set ||
      acc.r != app->system_accent.r || acc.g != app->system_accent.g || acc.b != app->system_accent.b)) {
    app->system_accent = acc;
    app->system_accent_set = true;
    if (!app->accent_override) changed = true;
  }
  if (changed) cogito_app_update_theme(app);
}

static void cogito_app_set_appid(ErgoVal appv, ErgoVal idv) {
  if (appv.tag != EVT_OBJ) ergo_trap("cogito.app_set_appid expects app");
  CogitoApp* app = (CogitoApp*)appv.as.p;
  ErgoStr* s = NULL;
  bool temp = false;
  if (idv.tag == EVT_STR) {
    s = (ErgoStr*)idv.as.p;
    if (s) ergo_retain_val(EV_STR(s));
  } else if (idv.tag != EVT_NULL) {
    s = stdr_to_string(idv);
    temp = true;
  }
  if (app->app_id) ergo_release_val(EV_STR(app->app_id));
  app->app_id = s;
  if (temp && s) {
    // app now owns temp string (ref=1)
  }
}

static void cogito_app_set_app_name(ErgoVal appv, ErgoVal namev) {
  if (appv.tag != EVT_OBJ) ergo_trap("cogito.app_set_app_name expects app");
  CogitoApp* app = (CogitoApp*)appv.as.p;
  ErgoStr* s = NULL;
  bool temp = false;
  if (namev.tag == EVT_STR) {
    s = (ErgoStr*)namev.as.p;
    if (s) ergo_retain_val(EV_STR(s));
  } else if (namev.tag != EVT_NULL) {
    s = stdr_to_string(namev);
    temp = true;
  }
  if (app->app_name) ergo_release_val(EV_STR(app->app_name));
  app->app_name = s;
  if (temp && s) {
    // app now owns temp string (ref=1)
  }
}

static void cogito_app_set_accent_color(ErgoVal appv, ErgoVal colorv, ErgoVal follow_systemv) {
  if (appv.tag != EVT_OBJ) ergo_trap("cogito.app_set_accent_color expects app");
  CogitoApp* app = (CogitoApp*)appv.as.p;
  bool follow_system = ergo_as_bool(follow_systemv);
  app->accent_override = !follow_system;
  if (colorv.tag != EVT_NULL) {
    ErgoStr* s = NULL;
    bool temp = false;
    if (colorv.tag == EVT_STR) {
      s = (ErgoStr*)colorv.as.p;
    } else {
      s = stdr_to_string(colorv);
      temp = true;
    }
    CogitoColor c = {0};
    if (s && s->data && cogito_hex_to_color(s->data, &c)) {
      app->accent_color = c;
      app->accent_set = true;
    }
    if (temp && s) ergo_release_val(EV_STR(s));
  }
  cogito_app_update_theme(app);
}

static ErgoVal cogito_app_new(void) {
  if (cogito_debug_enabled()) {
    fprintf(stderr, "cogito: app_new\n");
    fflush(stderr);
  }
  return EV_OBJ(cogito_app_new_obj());
}
