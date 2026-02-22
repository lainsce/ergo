case COGITO_FAB: {
  bool nav_rail_expanded = (n->parent &&
    n->parent->kind == COGITO_NAV_RAIL &&
    n->parent->nav_rail.expanded);
  bool effective_extended = n->fab.extended || nav_rail_expanded;
  const char* label = (n->subtitle && n->subtitle->data && n->subtitle->data[0])
    ? n->subtitle->data
    : "Create";

  if (effective_extended) {
    // Extended FAB: icon + label + padding (always M height)
    int font_size = cogito_node_font_size(n);
    int label_w = cogito_text_width_size_node(n, label, font_size);
    w = 16 + 24 + 12 + label_w + 16;  // padding + icon + gap + label + padding
    h = 56;
  } else {
    // Standard circular FAB: S=42, M=56 (default), L=96
    int dim = (n->fab.size == 0) ? 42 : (n->fab.size == 2) ? 96 : 56;
    w = dim;
    h = dim;
  }
  break;
}
