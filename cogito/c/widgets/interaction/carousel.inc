static CogitoNode* cogito_carousel_new_obj(void) {
  CogitoNode* cr = cogito_node_new(COGITO_CAROUSEL);
  cr->carousel_dragging = false;
  cr->carousel_scroll_x = 0;
  cr->carousel_drag_start_x = 0;
  cr->carousel_drag_start_scroll = 0;
  cr->carousel_active_index = 0;
  cr->carousel_item_count = 0;
  cr->carousel_velocity = 0.0f;
  cr->carousel_scroll_xf = 0.0;
  cr->carousel_anim_start = 0.0;
  cr->carousel_anim_from = 0.0;
  cr->carousel_anim_to = 0.0;
  cr->carousel_animating = false;
  cr->carousel_drag_last_time = 0.0;
  cr->carousel_drag_last_xf = 0.0;
  return cr;
}

static ErgoVal cogito_carousel_new(void) {
  return EV_OBJ(cogito_carousel_new_obj());
}

static void cogito_carousel_set_active_index(ErgoVal nodev, ErgoVal idxv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.carousel_set_active_index expects carousel");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  int idx = (int)ergo_as_int(idxv);
  if (idx < 0) idx = 0;
  if (idx >= (int)n->len) idx = (int)n->len - 1;
  if (idx < 0) idx = 0;
  n->carousel_active_index = idx;
  
  // Also update scroll_x so the layout function computes the same active index.
  // scroll_x = (idx / (count-1)) * max_scroll
  // Replicate the max_scroll formula from cogito_layout_carousel / 14_run.inc.
  int count = (int)n->len;
  if (count > 1) {
    int inner_w = n->w - n->padding_left - n->padding_right;
    if (inner_w < 0) inner_w = 0;
    int gaps = (count >= 3) ? 2 : (count - 1);
    if (gaps < 0) gaps = 0;
    int avail = inner_w - (gaps * 8);
    if (avail < 0) avail = 0;
    int est_ratio_sum = (count >= 3) ? (count + 5) : 7;
    int est_content = est_ratio_sum * avail / 7 + (count - 1) * 8;
    int max_scroll = est_content - inner_w;
    if (max_scroll < 0) max_scroll = 0;
    n->carousel_scroll_x = (int)((double)idx / (double)(count - 1) * (double)max_scroll + 0.5);
    if (n->carousel_scroll_x < 0) n->carousel_scroll_x = 0;
    if (n->carousel_scroll_x > max_scroll) n->carousel_scroll_x = max_scroll;
    n->carousel_scroll_xf = (double)n->carousel_scroll_x;
  } else {
    n->carousel_scroll_x = 0;
    n->carousel_scroll_xf = 0.0;
  }
  n->carousel_animating = false;
  
  cogito_window_relayout(cogito_node_window(n));
}

static ErgoVal cogito_carousel_get_active_index(ErgoVal nodev) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.carousel_get_active_index expects carousel");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  return EV_INT((int64_t)n->carousel_active_index);
}


