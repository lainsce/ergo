case COGITO_CARD: {
  CogitoStyle s = cogito_theme_resolve_node(n);
  int pad_l = s.has_padding_left ? s.padding_left : 16;
  int pad_t = s.has_padding_top ? s.padding_top : 16;
  int pad_r = s.has_padding_right ? s.padding_right : pad_l;
  int pad_b = s.has_padding_bottom ? s.padding_bottom : pad_t;
  int gap = n->gap;
  int title_h = 0;
  if (n->text && n->text->data && n->text->data[0]) {
    int ts = cogito_node_font_size(n);
    title_h = cogito_text_height_size(ts) + gap;
  }
  int content_h = 0;
  int max_cw = 0;
  for (size_t i = 0; i < n->len; i++) {
    int cw = 0, ch = 0;
    cogito_intrinsic_size(n->children[i], &cw, &ch);
    if (cw > max_cw) max_cw = cw;
    content_h += ch + n->children[i]->margin_top + n->children[i]->margin_bottom;
    if (i > 0) content_h += gap;
  }
  w = pad_l + pad_r + max_cw;
  h = pad_t + pad_b + title_h + content_h;
  if (w < 200) w = 200;
  if (h < 60) h = 60;
  break;
}
