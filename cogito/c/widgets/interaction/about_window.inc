static CogitoNode* cogito_about_window_new_obj(ErgoStr* app_name, ErgoStr* version) {
  CogitoNode* n = cogito_node_new(COGITO_ABOUT_WINDOW);
  cogito_node_set_text(n, app_name);
  n->subtitle = version;
  if (version) ergo_retain_val(EV_STR(version));
  n->hexpand = true;
  n->vexpand = true;
  return n;
}
static ErgoVal cogito_about_window_new(ErgoVal namev, ErgoVal versionv) {
  ErgoStr* ns = stdr_to_string(namev);
  ErgoStr* vs = stdr_to_string(versionv);
  CogitoNode* n = cogito_about_window_new_obj(ns, vs);
  return EV_OBJ(n);
}
static void cogito_about_window_set_icon(ErgoVal awv, ErgoVal iconv) {
  if (awv.tag != EVT_OBJ) ergo_trap("cogito.about_window_set_icon expects about_window");
  CogitoNode* n = (CogitoNode*)awv.as.p;
  ErgoStr* s = stdr_to_string(iconv);
  if (n->icon) ergo_release_val(EV_STR(n->icon));
  n->icon = s;
  ergo_retain_val(EV_STR(s));
  cogito_window_relayout(cogito_node_window(n));
}
static void cogito_about_window_set_description(ErgoVal awv, ErgoVal descv) {
  if (awv.tag != EVT_OBJ) ergo_trap("cogito.about_window_set_description expects about_window");
  CogitoNode* n = (CogitoNode*)awv.as.p;
  ErgoStr* s = stdr_to_string(descv);
  if (n->tooltip) ergo_release_val(EV_STR(n->tooltip));
  n->tooltip = s;
  ergo_retain_val(EV_STR(s));
  cogito_window_relayout(cogito_node_window(n));
}
static void cogito_about_window_set_website(ErgoVal awv, ErgoVal urlv) {
  if (awv.tag != EVT_OBJ) ergo_trap("cogito.about_window_set_website expects about_window");
  CogitoNode* n = (CogitoNode*)awv.as.p;
  ErgoStr* s = stdr_to_string(urlv);
  if (n->action_text) ergo_release_val(EV_STR(n->action_text));
  n->action_text = s;
  ergo_retain_val(EV_STR(s));
  cogito_window_relayout(cogito_node_window(n));
}
