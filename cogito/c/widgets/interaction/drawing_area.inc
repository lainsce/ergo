static CogitoNode* cogito_drawing_area_new_obj(void) {
  CogitoNode* n = cogito_node_new(COGITO_DRAWING_AREA);
  n->drawing_area.pressed = false;
  n->drawing_area.dragging = false;
  n->drawing_area.pointer_x = 0;
  n->drawing_area.pointer_y = 0;
  n->drawing_area.press_x = 0;
  n->drawing_area.press_y = 0;
  return n;
}

static ErgoVal cogito_drawing_area_new(void) {
  return EV_OBJ(cogito_drawing_area_new_obj());
}

static ErgoVal cogito_drawing_area_get_x(ErgoVal dav) {
  if (dav.tag != EVT_OBJ) ergo_trap("cogito.drawing_area_get_x expects drawing_area");
  CogitoNode* n = (CogitoNode*)dav.as.p;
  return EV_INT(n->drawing_area.pointer_x);
}

static ErgoVal cogito_drawing_area_get_y(ErgoVal dav) {
  if (dav.tag != EVT_OBJ) ergo_trap("cogito.drawing_area_get_y expects drawing_area");
  CogitoNode* n = (CogitoNode*)dav.as.p;
  return EV_INT(n->drawing_area.pointer_y);
}

static ErgoVal cogito_drawing_area_get_pressed(ErgoVal dav) {
  if (dav.tag != EVT_OBJ) ergo_trap("cogito.drawing_area_get_pressed expects drawing_area");
  CogitoNode* n = (CogitoNode*)dav.as.p;
  return EV_BOOL(n->drawing_area.pressed);
}
