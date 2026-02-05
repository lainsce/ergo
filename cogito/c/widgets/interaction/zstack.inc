static CogitoNode* cogito_zstack_new_obj(void) {
  return cogito_node_new(COGITO_ZSTACK);
}
static COGITO_UNUSED ErgoVal cogito_zstack_new(void) {
  return EV_OBJ(cogito_zstack_new_obj());
}
