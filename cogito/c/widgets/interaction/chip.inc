static CogitoNode* cogito_chip_new_obj(ErgoStr* label) {
  CogitoNode* n = cogito_node_new(COGITO_CHIP);
  cogito_node_set_text(n, label);
  return n;
}
static ErgoVal cogito_chip_new(ErgoVal text) {
  ErgoStr* ts = stdr_to_string(text);
  CogitoNode* n = cogito_chip_new_obj(ts);
  return EV_OBJ(n);
}
static void cogito_chip_set_selected(ErgoVal chipv, ErgoVal sel) {
  if (chipv.tag != EVT_OBJ) ergo_trap("cogito.chip_set_selected expects chip");
  CogitoNode* n = (CogitoNode*)chipv.as.p;
  n->chip_selected = ergo_as_bool(sel);
  cogito_window_relayout(cogito_node_window(n));
}
static ErgoVal cogito_chip_get_selected(ErgoVal chipv) {
  if (chipv.tag != EVT_OBJ) ergo_trap("cogito.chip_get_selected expects chip");
  CogitoNode* n = (CogitoNode*)chipv.as.p;
  return EV_BOOL(n->chip_selected);
}
static void cogito_chip_set_closable(ErgoVal chipv, ErgoVal closable) {
  if (chipv.tag != EVT_OBJ) ergo_trap("cogito.chip_set_closable expects chip");
  CogitoNode* n = (CogitoNode*)chipv.as.p;
  n->chip_has_close = ergo_as_bool(closable);
  cogito_window_relayout(cogito_node_window(n));
}
static void cogito_chip_on_close(ErgoVal chipv, ErgoVal handler) {
  if (chipv.tag != EVT_OBJ) ergo_trap("cogito.chip_on_close expects chip");
  CogitoNode* n = (CogitoNode*)chipv.as.p;
  if (n->on_action) ergo_release_val(EV_FN(n->on_action));
  if (handler.tag == EVT_FN) {
    n->on_action = (ErgoFn*)handler.as.p;
    ergo_retain_val(EV_FN(n->on_action));
  } else {
    n->on_action = NULL;
  }
}
