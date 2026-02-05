static CogitoNode* cogito_segmented_new_obj(void) {
  CogitoNode* s = cogito_node_new(COGITO_SEGMENTED);
  s->selected = 0;
  return s;
}
static ErgoVal cogito_segmented_new(void) {
  return EV_OBJ(cogito_segmented_new_obj());
}
