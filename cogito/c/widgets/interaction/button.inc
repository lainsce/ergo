static CogitoNode* cogito_button_new_obj(ErgoStr* label) {
  CogitoNode* btn = cogito_node_new(COGITO_BUTTON);
  cogito_node_set_text(btn, label);
  return btn;
}
static ErgoVal cogito_button_new(ErgoVal text) {
  if (cogito_debug_enabled()) {
    fprintf(stderr, "cogito: button_new\n");
    fflush(stderr);
  }
  ErgoStr* ts = stdr_to_string(text);
  CogitoNode* btn = cogito_button_new_obj(ts);
  return EV_OBJ(btn);
}
static void cogito_button_set_text(ErgoVal btnv, ErgoVal text) {
  if (btnv.tag != EVT_OBJ) ergo_trap("cogito.button_set_text expects button");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  ErgoStr* ts = stdr_to_string(text);
  cogito_node_set_text(btn, ts);
}
static void cogito_button_on_click(ErgoVal btnv, ErgoVal handler) {
  if (btnv.tag != EVT_OBJ) ergo_trap("cogito.button_on_click expects button");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (btn->on_click) ergo_release_val(EV_FN(btn->on_click));
  btn->on_click = (ErgoFn*)handler.as.p;
  ergo_retain_val(EV_FN(btn->on_click));
  static int cogito_debug_hit = -1;
  if (cogito_debug_hit < 0) {
    const char* env = getenv("COGITO_DEBUG_HIT");
    cogito_debug_hit = (env && env[0] && env[0] != '0') ? 1 : 0;
  }
  if (cogito_debug_hit) {
    const char* t = (btn->text && btn->text->data) ? btn->text->data : "";
    fprintf(stderr, "cogito: set_on_click text=\"%s\" handler=%p\n", t, (void*)btn->on_click);
    fflush(stderr);
  }
}
static void cogito_button_add_menu(ErgoVal btnv, ErgoVal label, ErgoVal handler) {
  if (btnv.tag != EVT_OBJ) ergo_trap("cogito.button_add_menu expects button");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  ErgoStr* ts = stdr_to_string(label);
  ErgoFn* fn = handler.tag == EVT_FN ? (ErgoFn*)handler.as.p : NULL;
  cogito_node_add_menu(btn, ts, fn);
}
