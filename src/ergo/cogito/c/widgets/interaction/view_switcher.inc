static CogitoNode* cogito_view_switcher_new_obj(void) {
  return cogito_node_new(COGITO_VIEWSWITCHER);
}
static ErgoVal cogito_view_switcher_new(void) {
  return EV_OBJ(cogito_view_switcher_new_obj());
}
static void cogito_view_switcher_set_active(ErgoVal vsv, ErgoVal idv) {
  if (vsv.tag != EVT_OBJ) ergo_trap("cogito.view_switcher_set_active expects view_switcher");
  CogitoNode* n = (CogitoNode*)vsv.as.p;
  if (n->kind != COGITO_VIEWSWITCHER) ergo_trap("cogito.view_switcher_set_active expects view_switcher");
  ErgoStr* s = stdr_to_string(idv);
  if (n->view_active_id) ergo_release_val(EV_STR(n->view_active_id));
  n->view_active_id = s;
  if (n->view_active_id) ergo_retain_val(EV_STR(n->view_active_id));
  cogito_window_relayout(cogito_node_window(n));
}
