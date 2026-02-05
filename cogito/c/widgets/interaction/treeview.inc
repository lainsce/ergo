static CogitoNode* cogito_treeview_new_obj(void) {
  return cogito_node_new(COGITO_TREEVIEW);
}
static ErgoVal cogito_treeview_new(void) {
  return EV_OBJ(cogito_treeview_new_obj());
}
