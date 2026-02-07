static CogitoNode* cogito_dialog_new_obj(ErgoStr* title) {
  CogitoNode* dlg = cogito_node_new(COGITO_DIALOG);
  cogito_node_set_text(dlg, title);
  return dlg;
}
static CogitoNode* cogito_dialog_slot_new_obj(void) {
  return cogito_node_new(COGITO_DIALOG_SLOT);
}
static ErgoVal cogito_dialog_new(ErgoVal title) {
  ErgoStr* ts = stdr_to_string(title);
  CogitoNode* dlg = cogito_dialog_new_obj(ts);
  return EV_OBJ(dlg);
}
static ErgoVal cogito_dialog_slot_new(void) {
  return EV_OBJ(cogito_dialog_slot_new_obj());
}
static void cogito_dialog_slot_show(ErgoVal slotv, ErgoVal dialogv) {
  if (slotv.tag != EVT_OBJ) ergo_trap("cogito.dialog_slot_show expects dialog slot");
  if (dialogv.tag != EVT_OBJ) ergo_trap("cogito.dialog_slot_show expects dialog");
  CogitoNode* slot = (CogitoNode*)slotv.as.p;
  CogitoNode* dlg = (CogitoNode*)dialogv.as.p;
  if (slot->dialog) ergo_release_val(EV_OBJ(slot->dialog));
  slot->dialog = dlg;
  slot->dialog_open = true;
  dlg->parent = slot;
  ergo_retain_val(EV_OBJ(dlg));
  cogito_apply_style_tree(dlg);
  cogito_window_relayout(cogito_node_window(slot));
}
static void cogito_dialog_slot_clear(ErgoVal slotv) {
  if (slotv.tag != EVT_OBJ) ergo_trap("cogito.dialog_slot_clear expects dialog slot");
  CogitoNode* slot = (CogitoNode*)slotv.as.p;
  slot->dialog_open = false;
  cogito_window_relayout(cogito_node_window(slot));
}
static void cogito_dialog_close(ErgoVal dialogv) {
  if (dialogv.tag != EVT_OBJ) ergo_trap("cogito.dialog_close expects dialog");
  CogitoNode* dlg = (CogitoNode*)dialogv.as.p;
  if (dlg->parent && dlg->parent->kind == COGITO_DIALOG_SLOT) {
    dlg->parent->dialog_open = false;
    cogito_window_relayout(cogito_node_window(dlg->parent));
  }
}
static void cogito_dialog_remove(ErgoVal dialogv) {
  if (dialogv.tag != EVT_OBJ) ergo_trap("cogito.dialog_remove expects dialog");
  CogitoNode* dlg = (CogitoNode*)dialogv.as.p;
  CogitoNode* parent = dlg->parent;
  if (parent) {
    cogito_container_remove_child(parent, dlg);
    cogito_window_relayout(cogito_node_window(parent));
  }
}
static ErgoVal cogito_node_parent(ErgoVal nodev) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.node_parent expects object");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->parent) return EV_OBJ(n->parent);
  return EV_NULLV;
}
static CogitoNode* cogito_find_dialog_slot(CogitoNode* n) {
  if (!n) return NULL;
  if (n->kind == COGITO_DIALOG_SLOT && n->dialog_open && n->dialog) return n;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* hit = cogito_find_dialog_slot(n->children[i]);
    if (hit) return hit;
  }
  return NULL;
}
