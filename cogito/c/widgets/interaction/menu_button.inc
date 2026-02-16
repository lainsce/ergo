static CogitoNode* cogito_menu_button_new_obj(ErgoStr* icon) {
  CogitoNode* n = cogito_node_new(COGITO_ICONBTN);
  cogito_node_set_text(n, icon);
  return n;
}
static ErgoVal cogito_menu_button_new(ErgoVal iconv) {
  ErgoStr* ts = stdr_to_string(iconv);
  CogitoNode* n = cogito_menu_button_new_obj(ts);
  return EV_OBJ(n);
}
