static CogitoNode* cogito_datepicker_new_obj(void) {
  CogitoNode* dp = cogito_node_new(COGITO_DATEPICKER);
  cogito_datepicker_ensure_date(dp);
  return dp;
}
static ErgoVal cogito_datepicker_new(void) {
  return EV_OBJ(cogito_datepicker_new_obj());
}
static void cogito_datepicker_on_change(ErgoVal nodev, ErgoVal handler) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.datepicker_on_change expects datepicker");
  if (handler.tag != EVT_FN) ergo_trap("cogito.datepicker_on_change expects function");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->kind != COGITO_DATEPICKER) ergo_trap("cogito.datepicker_on_change expects datepicker");
  if (n->on_change) ergo_release_val(EV_FN(n->on_change));
  n->on_change = (ErgoFn*)handler.as.p;
  ergo_retain_val(EV_FN(n->on_change));
}
