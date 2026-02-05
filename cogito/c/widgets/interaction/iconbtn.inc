static CogitoNode* cogito_iconbtn_new_obj(ErgoStr* label) {
  CogitoNode* btn = cogito_node_new(COGITO_ICONBTN);
  cogito_node_set_text(btn, label);
  return btn;
}
static ErgoVal cogito_iconbtn_new(ErgoVal text) {
  ErgoStr* ts = stdr_to_string(text);
  CogitoNode* btn = cogito_iconbtn_new_obj(ts);
  return EV_OBJ(btn);
}
static void cogito_iconbtn_set_text(ErgoVal btnv, ErgoVal text) {
  if (btnv.tag != EVT_OBJ) ergo_trap("cogito.iconbtn_set_text expects button");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  ErgoStr* ts = stdr_to_string(text);
  cogito_node_set_text(btn, ts);
}
static void cogito_iconbtn_on_click(ErgoVal btnv, ErgoVal handler) {
  if (btnv.tag != EVT_OBJ) ergo_trap("cogito.iconbtn_on_click expects button");
  CogitoNode* btn = (CogitoNode*)btnv.as.p;
  if (btn->on_click) ergo_release_val(EV_FN(btn->on_click));
  btn->on_click = (ErgoFn*)handler.as.p;
  ergo_retain_val(EV_FN(btn->on_click));
}
static void cogito_iconbtn_add_menu(ErgoVal btnv, ErgoVal label, ErgoVal handler) {
  cogito_button_add_menu(btnv, label, handler);
}
