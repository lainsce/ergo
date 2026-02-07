static CogitoNode* cogito_stepper_new_obj(double min, double max, double value, double step) {
  CogitoNode* st = cogito_node_new(COGITO_STEPPER);
  st->stepper_min = min;
  st->stepper_max = max;
  if (st->stepper_max < st->stepper_min) {
    double tmp = st->stepper_min;
    st->stepper_min = st->stepper_max;
    st->stepper_max = tmp;
  }
  if (value < st->stepper_min) value = st->stepper_min;
  if (value > st->stepper_max) value = st->stepper_max;
  st->stepper_value = value;
  st->stepper_step = step;
  return st;
}
static ErgoVal cogito_stepper_new(ErgoVal minv, ErgoVal maxv, ErgoVal valv, ErgoVal stepv) {
  double min = ergo_as_float(minv);
  double max = ergo_as_float(maxv);
  double val = ergo_as_float(valv);
  double step = ergo_as_float(stepv);
  return EV_OBJ(cogito_stepper_new_obj(min, max, val, step));
}
static void cogito_stepper_on_change(ErgoVal nodev, ErgoVal handler) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.stepper_on_change expects stepper");
  if (handler.tag != EVT_FN) ergo_trap("cogito.stepper_on_change expects function");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->kind != COGITO_STEPPER) ergo_trap("cogito.stepper_on_change expects stepper");
  if (n->on_change) ergo_release_val(EV_FN(n->on_change));
  n->on_change = (ErgoFn*)handler.as.p;
  ergo_retain_val(EV_FN(n->on_change));
}
static ErgoVal cogito_stepper_get_value(ErgoVal nodev) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.stepper_value expects stepper");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->kind != COGITO_STEPPER) ergo_trap("cogito.stepper_value expects stepper");
  return EV_FLOAT(n->stepper_value);
}
static void cogito_stepper_hit_test(CogitoNode* n, int mx, int my) {
  int btn_w = n->h;
  n->stepper_hit_minus = cogito_hit_rect(mx, my, n->x, n->y, btn_w, n->h);
  n->stepper_hit_plus = cogito_hit_rect(mx, my, n->x + n->w - btn_w, n->y, btn_w, n->h);
}
static void cogito_stepper_click(CogitoNode* n, int mx, int my) {
  int btn_w = n->h;
  bool hit_minus = cogito_hit_rect(mx, my, n->x, n->y, btn_w, n->h);
  bool hit_plus = cogito_hit_rect(mx, my, n->x + n->w - btn_w, n->y, btn_w, n->h);
  double prev = n->stepper_value;
  if (hit_minus && n->stepper_value > n->stepper_min) {
    n->stepper_value -= n->stepper_step;
    if (n->stepper_value < n->stepper_min) n->stepper_value = n->stepper_min;
  } else if (hit_plus && n->stepper_value < n->stepper_max) {
    n->stepper_value += n->stepper_step;
    if (n->stepper_value > n->stepper_max) n->stepper_value = n->stepper_max;
  }
  if (n->stepper_value != prev) {
    cogito_invoke_change(n);
  }
}
