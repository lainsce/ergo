static void cogito_layout_hstack(CogitoNode* n, int x, int y) {
  int pad = n->gap;
  int avail_w = n->w - n->padding_left - n->padding_right;
  int avail_h = n->h - n->padding_top - n->padding_bottom;
  if (avail_w < 0) avail_w = 0;
  if (avail_h < 0) avail_h = 0;
  size_t count = n->len;
  if (count == 0) return;
  int* intrinsic_w = (int*)calloc(count, sizeof(int));
  int* intrinsic_h = (int*)calloc(count, sizeof(int));
  int* assigned_w = (int*)calloc(count, sizeof(int));
  bool* expand = (bool*)calloc(count, sizeof(bool));
  if (!intrinsic_w || !intrinsic_h || !assigned_w || !expand) {
    free(intrinsic_w);
    free(intrinsic_h);
    free(assigned_w);
    free(expand);
    return;
  }
  int total_margins = 0;
  for (size_t i = 0; i < count; i++) {
    CogitoNode* c = n->children[i];
    int cw = 0;
    int ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    if (cw < 0) cw = 0;
    if (ch < 0) ch = 0;
    intrinsic_w[i] = cw;
    intrinsic_h[i] = ch;
    assigned_w[i] = cw;
    expand[i] = c->hexpand;
    total_margins += c->margin_left + c->margin_right;
  }
  int total_gaps = (count > 0) ? (int)(count - 1) * pad : 0;
  int avail_content_w = avail_w - total_margins - total_gaps;
  if (avail_content_w < 0) avail_content_w = 0;
  cogito_distribute_axis_sizes(count, intrinsic_w, expand, avail_content_w, assigned_w);

  int cx = x + n->padding_left;
  for (size_t i = 0; i < count; i++) {
    CogitoNode* c = n->children[i];
    int cw = assigned_w[i];
    int ch = intrinsic_h[i];
    int slot_h = avail_h - c->margin_top - c->margin_bottom;
    if (slot_h < 0) slot_h = 0;

    int child_row = c->align / 3;
    if (child_row < 0 || child_row > 2) child_row = 0;

    bool is_chrome = (c->kind == COGITO_APPBAR || c->kind == COGITO_TOOLBAR ||
                      c->kind == COGITO_NAV_RAIL || c->kind == COGITO_BOTTOM_NAV);
    if (c->vexpand || is_chrome) {
      ch = slot_h;
    } else if (ch > slot_h) {
      ch = slot_h;
    }

    int cy = y + n->padding_top;
    int oy = 0;
    if (child_row == 1) {
      oy = (slot_h - ch) / 2;
    } else if (child_row == 2) {
      oy = slot_h - ch;
    }
    if (oy < 0) oy = 0;
    cogito_layout_node(c, cx + c->margin_left, cy + oy + c->margin_top, cw, ch);
    cx += cw + c->margin_left + c->margin_right;
    if (i < count - 1) cx += pad;
  }
  free(intrinsic_w);
  free(intrinsic_h);
  free(assigned_w);
  free(expand);
}
