static CogitoNode* cogito_buttongroup_new_obj(void) {
  CogitoNode* s = cogito_node_new(COGITO_BUTTON_GROUP);
  s->selected = 0;
  s->gap = 2;
  return s;
}
static ErgoVal cogito_buttongroup_new(void) {
  return EV_OBJ(cogito_buttongroup_new_obj());
}
static void cogito_buttongroup_on_select(ErgoVal nodev, ErgoVal handler) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.buttongroup_on_select expects buttongroup");
  if (handler.tag != EVT_FN) ergo_trap("cogito.buttongroup_on_select expects function");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->kind != COGITO_BUTTON_GROUP) ergo_trap("cogito.buttongroup_on_select expects buttongroup");
  if (n->on_change) ergo_release_val(EV_FN(n->on_change));
  n->on_change = (ErgoFn*)handler.as.p;
  ergo_retain_val(EV_FN(n->on_change));
}
static void cogito_buttongroup_click(CogitoNode* n, int mx, int my) {
  if (n->len == 0) return;
  (void)my;
  int count = (int)n->len;
  if (n->w <= 0) return;
  int rel_x = mx - n->x;
  int idx = (rel_x * count) / n->w;
  if (idx < 0) idx = 0;
  if (idx >= count) idx = count - 1;
  int prev = n->selected;
  n->selected = idx;
  if (n->selected != prev && n->on_change) {
    ErgoVal arg = EV_INT(idx);
    cogito_invoke(n->on_change, 1, &arg);
  }
}
