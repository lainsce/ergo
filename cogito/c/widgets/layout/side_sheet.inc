case COGITO_SIDE_SHEET: {
  // SideSheet uses HStack-like layout after the title
  int pad_l = n->padding_left;
  int pad_t = n->padding_top;
  int pad_b = n->padding_bottom;
  int handle_w = 28;
  int gap = n->gap > 0 ? n->gap : 8;
  int avail_h = n->h - pad_t - pad_b;
  if (avail_h < 0) avail_h = 0;

  int cx = x + handle_w + pad_l;
  // Title
  if (n->text && n->text->data && n->text->data[0]) {
    int ts = cogito_node_font_size(n);
    int tw = cogito_text_width_size_node(n, n->text->data, ts);
    cx += tw + gap;
  }

  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    int cw = 0, ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    int slot_h = avail_h - c->margin_top - c->margin_bottom;
    if (slot_h < 0) slot_h = 0;
    if (c->vexpand) ch = slot_h;
    else if (ch > slot_h) ch = slot_h;
    cogito_layout_node(c, cx + c->margin_left, y + pad_t + c->margin_top, cw, ch);
    cx += cw + c->margin_left + c->margin_right;
    if (i < n->len - 1) cx += gap;
  }
  break;
}
