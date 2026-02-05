static CogitoNode* cogito_toolbar_new_obj(void) {
  return cogito_node_new(COGITO_TOOLBAR);
}
static ErgoVal cogito_toolbar_new(void) {
  return EV_OBJ(cogito_toolbar_new_obj());
}
