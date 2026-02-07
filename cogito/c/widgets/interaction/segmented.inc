static CogitoNode* cogito_segmented_new_obj(void) {
  CogitoNode* s = cogito_node_new(COGITO_SEGMENTED);
  s->selected = 0;
  return s;
}
static ErgoVal cogito_segmented_new(void) {
  return EV_OBJ(cogito_segmented_new_obj());
}
static void cogito_segmented_on_select(ErgoVal nodev, ErgoVal handler) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.segmented_on_select expects segmented");
  if (handler.tag != EVT_FN) ergo_trap("cogito.segmented_on_select expects function");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->kind != COGITO_SEGMENTED) ergo_trap("cogito.segmented_on_select expects segmented");
  if (n->on_change) ergo_release_val(EV_FN(n->on_change));
  n->on_change = (ErgoFn*)handler.as.p;
  ergo_retain_val(EV_FN(n->on_change));
}
static void cogito_segmented_click(CogitoNode* n, int mx, int my) {
  if (n->len == 0) return;
  (void)my;
  int seg_w = n->w / (int)n->len;
  int idx = (mx - n->x) / seg_w;
  if (idx < 0) idx = 0;
  if (idx >= (int)n->len) idx = (int)n->len - 1;
  int prev = n->selected;
  n->selected = idx;
  if (n->selected != prev && n->on_change) {
    ErgoVal arg = EV_INT(idx);
    cogito_invoke(n->on_change, 1, &arg);
  }
}
