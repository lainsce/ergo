case COGITO_BOTTOM_SHEET: {
  CogitoStyle s = cogito_theme_resolve_node(n);
  int pad_t = s.has_padding_top ? s.padding_top : 18;
  int pad_b = s.has_padding_bottom ? s.padding_bottom : pad_t;
  int handle_h = 28; // drag handle area
  int gap = n->gap > 0 ? n->gap : 8;
  int title_h = 0;
  if (n->text && n->text->data && n->text->data[0]) {
    int ts = cogito_node_font_size(n);
    title_h = cogito_text_height_size(ts) + gap;
  }
  int content_h = 0;
  for (size_t i = 0; i < n->len; i++) {
    int cw = 0, ch = 0;
    cogito_intrinsic_size(n->children[i], &cw, &ch);
    content_h += ch + n->children[i]->margin_top + n->children[i]->margin_bottom;
    if (i > 0) content_h += gap;
  }
  w = 324;
  h = handle_h + title_h + pad_t + content_h + pad_b;
  if (h < 100) h = 100;
  break;
}
