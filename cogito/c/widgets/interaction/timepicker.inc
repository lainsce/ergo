static CogitoNode* cogito_timepicker_new_obj(void) {
  CogitoNode* n = cogito_node_new(COGITO_TIMEPICKER);
  cogito_timepicker_ensure_time(n);
  return n;
}

static ErgoVal cogito_timepicker_new(void) {
  return EV_OBJ(cogito_timepicker_new_obj());
}

static void cogito_timepicker_on_change(ErgoVal nodev, ErgoVal handler) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.timepicker_on_change expects timepicker");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->kind != COGITO_TIMEPICKER) ergo_trap("cogito.timepicker_on_change expects timepicker");
  if (n->on_change) ergo_release_val(EV_FN(n->on_change));
  if (handler.tag == EVT_FN) {
    n->on_change = (ErgoFn*)handler.as.p;
    ergo_retain_val(EV_FN(n->on_change));
  } else {
    n->on_change = NULL;
  }
}

static ErgoVal cogito_timepicker_get_hour(ErgoVal nodev) {
  if (nodev.tag != EVT_OBJ) return EV_INT(0);
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  return EV_INT(n->time_hour);
}

static ErgoVal cogito_timepicker_get_minute(ErgoVal nodev) {
  if (nodev.tag != EVT_OBJ) return EV_INT(0);
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  return EV_INT(n->time_minute);
}

static void cogito_timepicker_set_time(ErgoVal nodev, ErgoVal hourv, ErgoVal minv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.timepicker_set_time expects timepicker");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  int h = 0, m = 0;
  if (hourv.tag == EVT_INT) h = (int)hourv.as.i;
  else if (hourv.tag == EVT_FLOAT) h = (int)hourv.as.f;
  if (minv.tag == EVT_INT) m = (int)minv.as.i;
  else if (minv.tag == EVT_FLOAT) m = (int)minv.as.f;
  if (h < 0) h = 0; if (h > 23) h = 23;
  if (m < 0) m = 0; if (m > 59) m = 59;
  n->time_hour = h;
  n->time_minute = m;
  n->time_ampm = (h >= 12);
  cogito_window_relayout(cogito_node_window(n));
}
