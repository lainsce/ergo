static void cogito_draw_toasts_overlay(CogitoNode* n) {
  if (!n) return;
  if (n->kind == COGITO_TOASTS) {
    cogito_toasts_force_draw = true;
    cogito_draw_node(n);
    cogito_toasts_force_draw = false;
    return;
  }
  for (size_t i = 0; i < n->len; i++) {
    cogito_draw_toasts_overlay(n->children[i]);
  }
  if ((n->kind == COGITO_WINDOW || n->kind == COGITO_DIALOG_SLOT) &&
      n->dialog && n->dialog_open) {
    cogito_draw_toasts_overlay(n->dialog);
  }
}
