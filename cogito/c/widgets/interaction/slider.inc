static CogitoNode* cogito_slider_new_obj(double min, double max, double value) {
  CogitoNode* sl = cogito_node_new(COGITO_SLIDER);
  sl->slider.min = min;
  sl->slider.max = max;
  if (sl->slider.max < sl->slider.min) {
    double tmp = sl->slider.min;
    sl->slider.min = sl->slider.max;
    sl->slider.max = tmp;
  }
  if (value < sl->slider.min) value = sl->slider.min;
  if (value > sl->slider.max) value = sl->slider.max;
  sl->slider.value = value;
  return sl;
}
static ErgoVal cogito_slider_new(ErgoVal minv, ErgoVal maxv, ErgoVal valv) {
  double min = (double)ergo_as_int(minv);
  double max = (double)ergo_as_int(maxv);
  double val = (double)ergo_as_int(valv);
  return EV_OBJ(cogito_slider_new_obj(min, max, val));
}
static void cogito_slider_set_value(ErgoVal nodev, ErgoVal valv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.slider_set_value expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  double v = (double)ergo_as_int(valv);
  if (v < n->slider.min) v = n->slider.min;
  if (v > n->slider.max) v = n->slider.max;
  n->slider.value = v;
  cogito_window_relayout(cogito_node_window(n));
}
static ErgoVal cogito_slider_get_value(ErgoVal nodev) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.slider_get_value expects slider");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  return EV_INT((int64_t)lround(n->slider.value));
}
static void cogito_slider_on_change(ErgoVal nodev, ErgoVal handler) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.slider_on_change expects slider");
  if (handler.tag != EVT_FN) ergo_trap("cogito.slider_on_change expects function");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->on_change) ergo_release_val(EV_FN(n->on_change));
  n->on_change = (ErgoFn*)handler.as.p;
  ergo_retain_val(EV_FN(n->on_change));
}
