static void cogito_appbar_window_action(CogitoNode* win, int action) {
  (void)win;
  if (action == 0) {
    if (win) win->should_close = true;
  } else if (action == 1) {
    MinimizeWindow();
  } else if (action == 2) {
    if (IsWindowMaximized()) {
      RestoreWindow();
    } else {
      MaximizeWindow();
    }
  }
}
static CogitoNode* cogito_appbar_new_obj(ErgoStr* title, ErgoStr* subtitle) {
  CogitoNode* ab = cogito_node_new(COGITO_APPBAR);
  cogito_node_set_text(ab, title);
  cogito_node_set_subtitle(ab, subtitle);
  ab->appbar_btn_close_x = -1;
  ab->appbar_btn_min_x = -1;
  ab->appbar_btn_max_x = -1;
  strncpy(ab->appbar_controls, "CIM|", sizeof(ab->appbar_controls) - 1);
  ab->appbar_controls[sizeof(ab->appbar_controls) - 1] = 0;
  return ab;
}
static ErgoVal cogito_appbar_new(ErgoVal title, ErgoVal subtitle) {
  ErgoStr* ts = stdr_to_string(title);
  ErgoStr* ss = stdr_to_string(subtitle);
  CogitoNode* ab = cogito_appbar_new_obj(ts, ss);
  return EV_OBJ(ab);
}
static ErgoVal cogito_appbar_add_button(ErgoVal appv, ErgoVal text, ErgoVal handler) {
  if (appv.tag != EVT_OBJ) ergo_trap("cogito.appbar_add_button expects appbar");
  CogitoNode* ab = (CogitoNode*)appv.as.p;
  ErgoStr* ts = stdr_to_string(text);
  CogitoNode* btn = cogito_iconbtn_new_obj(ts);
  if (handler.tag == EVT_FN) {
    btn->on_click = (ErgoFn*)handler.as.p;
    ergo_retain_val(EV_FN(btn->on_click));
  }
  cogito_children_add(ab, btn);
  return EV_OBJ(btn);
}
static void cogito_appbar_set_controls(ErgoVal appv, ErgoVal layout) {
  if (appv.tag != EVT_OBJ) ergo_trap("cogito.appbar_set_controls expects appbar");
  if (layout.tag != EVT_STR) ergo_trap("cogito.appbar_set_controls expects string");
  CogitoNode* ab = (CogitoNode*)appv.as.p;
  ErgoStr* ls = (ErgoStr*)layout.as.p;
  const char* s = ls ? ls->data : "";
  char buf[16];
  size_t j = 0;
  bool has_split = false;
  for (size_t i = 0; s[i] && j + 1 < sizeof(buf); i++) {
    char c = s[i];
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    if (c == 'C' || c == 'M' || c == 'I') {
      buf[j++] = c;
    } else if (c == '|') {
      if (!has_split) {
        buf[j++] = c;
        has_split = true;
      }
    }
  }
  buf[j] = 0;
  if (j == 0) {
    strncpy(ab->appbar_controls, "CIM|", sizeof(ab->appbar_controls) - 1);
    ab->appbar_controls[sizeof(ab->appbar_controls) - 1] = 0;
  } else {
    strncpy(ab->appbar_controls, buf, sizeof(ab->appbar_controls) - 1);
    ab->appbar_controls[sizeof(ab->appbar_controls) - 1] = 0;
  }
  CogitoNode* win = cogito_root(ab);
  if (win && win->kind == COGITO_WINDOW) {
    cogito_window_relayout(win);
  }
}
static CogitoNode* cogito_find_appbar(CogitoNode* win) {
  if (!win) return NULL;
  for (size_t i = 0; i < win->len; i++) {
    if (win->children[i]->kind == COGITO_APPBAR) return win->children[i];
  }
  return NULL;
}
static bool cogito_point_over_appbar_buttons(CogitoNode* app, int x, int y) {
  if (!app) return false;
  if (app->appbar_btn_close_x >= 0 &&
      cogito_hit_rect(x, y, app->appbar_btn_close_x, app->appbar_btn_y, app->appbar_btn_size, app->appbar_btn_size)) return true;
  if (app->appbar_btn_min_x >= 0 &&
      cogito_hit_rect(x, y, app->appbar_btn_min_x, app->appbar_btn_y, app->appbar_btn_size, app->appbar_btn_size)) return true;
  if (app->appbar_btn_max_x >= 0 &&
      cogito_hit_rect(x, y, app->appbar_btn_max_x, app->appbar_btn_y, app->appbar_btn_size, app->appbar_btn_size)) return true;
  for (size_t i = 0; i < app->len; i++) {
    CogitoNode* c = app->children[i];
    if (cogito_hit_rect(x, y, c->x, c->y, c->w, c->h)) return true;
  }
  return false;
}
