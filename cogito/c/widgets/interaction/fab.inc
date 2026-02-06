static CogitoNode* cogito_fab_new_obj(ErgoStr* icon) {
  CogitoNode* n = cogito_node_new(COGITO_FAB);
  cogito_node_set_text(n, icon);  // Icon name
  return n;
}
static ErgoVal cogito_fab_new(ErgoVal icon) {
  ErgoStr* ts = stdr_to_string(icon);
  CogitoNode* n = cogito_fab_new_obj(ts);
  return EV_OBJ(n);
}
static void cogito_fab_set_extended(ErgoVal fabv, ErgoVal extended, ErgoVal label) {
  if (fabv.tag != EVT_OBJ) ergo_trap("cogito.fab_set_extended expects fab");
  CogitoNode* n = (CogitoNode*)fabv.as.p;
  n->fab_extended = ergo_as_bool(extended);
  if (n->fab_extended) {
    ErgoStr* lbl = stdr_to_string(label);
    if (n->subtitle) ergo_release_val(EV_STR(n->subtitle));
    n->subtitle = lbl;
    ergo_retain_val(EV_STR(n->subtitle));
  }
  cogito_window_relayout(cogito_node_window(n));
}
