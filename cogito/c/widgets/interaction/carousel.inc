static CogitoNode* cogito_carousel_new_obj(void) {
  CogitoNode* cr = cogito_node_new(COGITO_CAROUSEL);
  cr->carousel_dragging = false;
  cr->carousel_scroll_x = 0;
  cr->carousel_drag_start_x = 0;
  cr->carousel_drag_start_scroll = 0;
  cr->carousel_active_index = 0;
  cr->carousel_item_count = 0;
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
  cogito_window_relayout(cogito_node_window(n));
}

static ErgoVal cogito_carousel_get_active_index(ErgoVal nodev) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.carousel_get_active_index expects carousel");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  return EV_INT((int64_t)n->carousel_active_index);
}

static CogitoNode* cogito_find_carousel(CogitoNode* n, int x, int y) {
  if (!n) return NULL;
  if (n->kind == COGITO_CAROUSEL && !cogito_hit_rect(x, y, n->x, n->y, n->w, n->h)) return NULL;
  if (n->kind == COGITO_VIEWSWITCHER) {
    CogitoNode* active = cogito_view_switcher_active_child(n);
    if (active) {
      CogitoNode* child = cogito_find_carousel(active, x, y);
      if (child) return child;
    }
  } else {
    for (size_t i = 0; i < n->len; i++) {
      CogitoNode* child = cogito_find_carousel(n->children[i], x, y);
      if (child) return child;
    }
  }
  if (n->kind == COGITO_CAROUSEL && cogito_hit_rect(x, y, n->x, n->y, n->w, n->h)) return n;
  return NULL;
}
