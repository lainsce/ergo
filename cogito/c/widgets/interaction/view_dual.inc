static CogitoNode* cogito_view_dual_new_obj(void) {
  CogitoNode* n = cogito_node_new(COGITO_VIEW_DUAL);
  n->hexpand = true;
  n->vexpand = true;
  return n;
}
static ErgoVal cogito_view_dual_new(void) {
  CogitoNode* n = cogito_view_dual_new_obj();
  return EV_OBJ(n);
}
static void cogito_view_dual_set_ratio(ErgoVal vdv, ErgoVal ratiov) {
  if (vdv.tag != EVT_OBJ) ergo_trap("cogito.view_dual_set_ratio expects view_dual");
  CogitoNode* n = (CogitoNode*)vdv.as.p;
  float r = (float)ergo_as_float(ratiov);
  if (r < 0.1f) r = 0.1f;
  if (r > 0.9f) r = 0.9f;
  n->view_dual.ratio = r;
  cogito_window_relayout(cogito_node_window(n));
}
