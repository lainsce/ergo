static CogitoNode* cogito_bottom_sheet_new_obj(ErgoStr* title) {
  CogitoNode* n = cogito_node_new(COGITO_BOTTOM_SHEET);
  if (title && title->data && title->data[0]) {
    cogito_node_set_text(n, title);
  }
  n->hexpand = true;
  n->gap = 8;
  return n;
}

static ErgoVal cogito_bottom_sheet_new(ErgoVal titlev) {
  ErgoStr* ts = NULL;
  if (titlev.tag == EVT_STR) {
    ts = (ErgoStr*)titlev.as.p;
  } else if (titlev.tag != EVT_NULL) {
    ts = stdr_to_string(titlev);
  }
  CogitoNode* n = cogito_bottom_sheet_new_obj(ts);
  if (ts && titlev.tag != EVT_STR) ergo_release_val(EV_STR(ts));
  return EV_OBJ(n);
}
