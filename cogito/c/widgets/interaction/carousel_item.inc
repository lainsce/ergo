static CogitoNode* cogito_carousel_item_new_obj(void) {
  CogitoNode* item = cogito_node_new(COGITO_CAROUSEL_ITEM);
  // Default border radius for carousel items (will be overridden by theme)
  item->border_radius = 16;
  
  // Add a built-in label as a child
  CogitoNode* label = cogito_node_new(COGITO_LABEL);
  // Leave text empty initially - user can set it via add() or set_text()
  label->text_align = 0;  // left alignment
  
  // Add label as first child using existing helper function
  cogito_children_add(item, label);
  
  return item;
}

static ErgoVal cogito_carousel_item_new(void) {
  return EV_OBJ(cogito_carousel_item_new_obj());
}

// Set text on the built-in label
static void cogito_carousel_item_set_text(ErgoVal itemv, ErgoVal textv) {
  if (itemv.tag != EVT_OBJ) ergo_trap("cogito.carousel_item_set_text expects CarouselItem");
  CogitoNode* n = (CogitoNode*)itemv.as.p;
  // Get the first child (the built-in label)
  if (n->len > 0 && n->children[0]->kind == COGITO_LABEL) {
    CogitoNode* label = n->children[0];
    ErgoStr* ts = stdr_to_string(textv);
    cogito_node_set_text(label, ts);
    cogito_window_relayout(cogito_node_window(n));
  }
}

// Set horizontal alignment on the built-in label
static void cogito_carousel_item_set_halign(ErgoVal itemv, ErgoVal alignv) {
  if (itemv.tag != EVT_OBJ) ergo_trap("cogito.carousel_item_set_halign expects CarouselItem");
  CogitoNode* n = (CogitoNode*)itemv.as.p;
  // Get the first child (the built-in label)
  if (n->len > 0 && n->children[0]->kind == COGITO_LABEL) {
    CogitoNode* label = n->children[0];
    label->text_align = (int)ergo_as_int(alignv);
    if (label->text_align < 0) label->text_align = 0;
    if (label->text_align > 2) label->text_align = 2;
    cogito_window_relayout(cogito_node_window(n));
  }
}

// Set vertical alignment on the built-in label
static void cogito_carousel_item_set_valign(ErgoVal itemv, ErgoVal alignv) {
  if (itemv.tag != EVT_OBJ) ergo_trap("cogito.carousel_item_set_valign expects CarouselItem");
  CogitoNode* n = (CogitoNode*)itemv.as.p;
  // For now, use the same text_align for vertical (could add a separate field if needed)
  // This sets vertical position within the carousel item
  (void)alignv;  // TODO: implement vertical alignment
}
