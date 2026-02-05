static CogitoNode* cogito_progress_new_obj(double value) {
  CogitoNode* p = cogito_node_new(COGITO_PROGRESS);
  if (value < 0.0) value = 0.0;
  if (value > 1.0) value = 1.0;
  p->progress_value = value;
  return p;
}
static ErgoVal cogito_progress_new(ErgoVal valv) {
  double v = ergo_as_float(valv);
  return EV_OBJ(cogito_progress_new_obj(v));
}
static void cogito_progress_set_value(ErgoVal pv, ErgoVal valv) {
  if (pv.tag != EVT_OBJ) ergo_trap("cogito.progress_set_value expects progress");
  CogitoNode* p = (CogitoNode*)pv.as.p;
  double v = ergo_as_float(valv);
  if (v < 0.0) v = 0.0;
  if (v > 1.0) v = 1.0;
  p->progress_value = v;
  cogito_window_relayout(cogito_node_window(p));
}
static ErgoVal cogito_progress_get_value(ErgoVal pv) {
  if (pv.tag != EVT_OBJ) ergo_trap("cogito.progress_get_value expects progress");
  CogitoNode* p = (CogitoNode*)pv.as.p;
  return EV_FLOAT(p->progress_value);
}
