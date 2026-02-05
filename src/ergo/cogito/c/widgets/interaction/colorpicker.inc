static CogitoNode* cogito_colorpicker_new_obj(void) {
  CogitoNode* cp = cogito_node_new(COGITO_COLORPICKER);
  cp->color_h = 200.0;
  cp->color_c = 20.0;
  cp->color_t = 90.0;
  cogito_colorpicker_sync_hex(cp);
  return cp;
}
static ErgoVal cogito_colorpicker_new(void) {
  return EV_OBJ(cogito_colorpicker_new_obj());
}
static void cogito_colorpicker_on_change(ErgoVal nodev, ErgoVal handler) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.colorpicker_on_change expects colorpicker");
  if (handler.tag != EVT_FN) ergo_trap("cogito.colorpicker_on_change expects function");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->kind != COGITO_COLORPICKER) ergo_trap("cogito.colorpicker_on_change expects colorpicker");
  if (n->on_change) ergo_release_val(EV_FN(n->on_change));
  n->on_change = (ErgoFn*)handler.as.p;
  ergo_retain_val(EV_FN(n->on_change));
}
