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
