case COGITO_CHIP: {
  int size = cogito_node_font_size(n);
  const char* t = n->text ? n->text->data : "";
  int text_w = cogito_text_width_size_node(n, t, size);
  w = text_w + 32;  // 16px padding each side
  if (n->chip.has_close) {
    w += 24;  // Space for close icon + gap
  }
  h = 32;  // Standard chip height
  break;
}
