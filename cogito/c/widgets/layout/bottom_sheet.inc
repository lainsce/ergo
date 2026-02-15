case COGITO_BOTTOM_SHEET: {
  // BottomSheet uses VStack-like layout below the drag handle
  CogitoStyle s = cogito_theme_resolve_node(n);
  int pad_l = s.has_padding_left ? s.padding_left : 16;
  int pad_t = s.has_padding_top ? s.padding_top : 16;
  int pad_r = s.has_padding_right ? s.padding_right : pad_l;
  int handle_h = 28;
  int gap = n->gap > 0 ? n->gap : 8;
  int avail_w = n->w - pad_l - pad_r;
  if (avail_w < 0) avail_w = 0;

  int cy = y + handle_h + pad_t;
  // Title
  if (n->text && n->text->data && n->text->data[0]) {
    int ts = cogito_node_font_size(n);
    int th = cogito_text_height_size(ts);
    cy += th + gap;
  }

  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    int cw = 0, ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    int slot_w = avail_w - c->margin_left - c->margin_right;
    if (slot_w < 0) slot_w = 0;
    if (c->hexpand) cw = slot_w;
    else if (cw > slot_w) cw = slot_w;
    cogito_layout_node(c, x + pad_l + c->margin_left, cy + c->margin_top, cw, ch);
    cy += ch + c->margin_top + c->margin_bottom;
    if (i < n->len - 1) cy += gap;
  }
  break;
}
