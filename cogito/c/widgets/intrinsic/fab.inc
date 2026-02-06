case COGITO_FAB: {
  if (n->fab_extended) {
    // Extended FAB: icon + label + padding
    int size = cogito_node_font_size(n);
    int label_w = n->subtitle ? cogito_text_width_size_node(n, n->subtitle->data, size) : 0;
    w = 16 + 24 + 12 + label_w + 16;  // padding + icon + gap + label + padding
    h = 56;
  } else {
    // Standard FAB: circular
    w = 56;
    h = 56;
  }
  break;
}
