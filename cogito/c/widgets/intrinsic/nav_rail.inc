case COGITO_NAV_RAIL: {
  int rail_w = COGITO_NAV_RAIL_WIDTH;
  if (n->nav_rail.expanded) {
    int parent_w = 0;
    if (n->parent) {
      parent_w = n->parent->w - n->parent->padding_left - n->parent->padding_right;
      if (parent_w < 0) parent_w = 0;
    }
    int expanded_w = parent_w > 0 ? parent_w / 3 : COGITO_NAV_RAIL_EXPANDED_MIN_WIDTH;
    if (expanded_w < COGITO_NAV_RAIL_EXPANDED_MIN_WIDTH) expanded_w = COGITO_NAV_RAIL_EXPANDED_MIN_WIDTH;
    if (expanded_w > COGITO_NAV_RAIL_EXPANDED_MAX_WIDTH) expanded_w = COGITO_NAV_RAIL_EXPANDED_MAX_WIDTH;
    rail_w = expanded_w;
  }
  int fab_h = 0;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    if (!c || c->kind != COGITO_FAB) continue;
    int fw = 0;
    cogito_intrinsic_size(c, &fw, &fab_h);
    if (fab_h < 0) fab_h = 0;
    break;
  }
  int top_space = COGITO_NAV_RAIL_TOP_OFFSET + COGITO_NAV_RAIL_SECTION_GAP;
  if (n->nav_rail.show_toggle) {
    top_space += COGITO_NAV_RAIL_TOGGLE_BUTTON_SIZE;
  }
  if (fab_h > 0) top_space += fab_h + COGITO_NAV_RAIL_SECTION_GAP;
  int item_step = n->nav_rail.expanded ? COGITO_NAV_RAIL_ITEM_HEIGHT : COGITO_NAV_RAIL_COLLAPSED_ITEM_STEP;
  w = rail_w;
  h = n->padding_top + n->padding_bottom + top_space + (item_step * n->nav_rail.len);
  if (h < 200) h = 200;  // Minimum height
  break;
}
