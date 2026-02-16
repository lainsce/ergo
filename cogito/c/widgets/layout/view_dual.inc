case COGITO_VIEW_DUAL: {
  float ratio = n->view_dual.ratio;
  if (ratio <= 0.0f) ratio = 0.5f;
  if (ratio > 1.0f) ratio = 1.0f;
  int handle_w = 4;
  int left_w = (int)(n->w * ratio) - handle_w / 2;
  if (left_w < 0) left_w = 0;
  int right_x = x + left_w + handle_w;
  int right_w = n->w - left_w - handle_w;
  if (right_w < 0) right_w = 0;

  if (n->len > 0) {
    CogitoNode* left = n->children[0];
    int lw = 0, lh = 0;
    cogito_intrinsic_size(left, &lw, &lh);
    cogito_layout_node(left, x, y, left_w, n->h);
  }
  if (n->len > 1) {
    CogitoNode* right = n->children[1];
    int rw = 0, rh = 0;
    cogito_intrinsic_size(right, &rw, &rh);
    cogito_layout_node(right, right_x, y, right_w, n->h);
  }
  break;
}
