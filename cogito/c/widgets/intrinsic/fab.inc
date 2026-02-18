case COGITO_FAB: {
  bool nav_rail_expanded = (n->parent &&
    n->parent->kind == COGITO_NAV_RAIL &&
    n->parent->nav_rail.expanded);
  bool effective_extended = n->fab.extended || nav_rail_expanded;
  const char* label = (n->subtitle && n->subtitle->data && n->subtitle->data[0])
    ? n->subtitle->data
    : "Create";

  if (effective_extended) {
    // Extended FAB: icon + label + padding
    int size = cogito_node_font_size(n);
    int label_w = cogito_text_width_size_node(n, label, size);
    w = 16 + 24 + 12 + label_w + 16;  // padding + icon + gap + label + padding
    h = 56;
  } else {
    // Standard FAB: circular
    w = 56;
    h = 56;
  }
  break;
}
