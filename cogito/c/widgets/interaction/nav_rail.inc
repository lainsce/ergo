static CogitoNode* cogito_nav_rail_new_obj(void) {
  CogitoNode* n = cogito_node_new(COGITO_NAV_RAIL);
  n->nav_rail_labels = NULL;
  n->nav_rail_icons = NULL;
  n->nav_rail_len = 0;
  n->nav_rail_cap = 0;
  n->nav_rail_selected = 0;
  return n;
}

static ErgoVal cogito_nav_rail_new(void) {
  CogitoNode* n = cogito_nav_rail_new_obj();
  return EV_OBJ(n);
}

static void cogito_nav_rail_set_items(ErgoVal railv, ErgoVal labelsv, ErgoVal iconsv) {
  if (railv.tag != EVT_OBJ) ergo_trap("cogito.nav_rail_set_items expects nav_rail");
  CogitoNode* n = (CogitoNode*)railv.as.p;

  // Clear existing items
  if (n->nav_rail_labels) {
    for (size_t i = 0; i < n->nav_rail_len; i++) {
      if (n->nav_rail_labels[i]) ergo_release_val(EV_STR(n->nav_rail_labels[i]));
      if (n->nav_rail_icons && n->nav_rail_icons[i]) ergo_release_val(EV_STR(n->nav_rail_icons[i]));
    }
    free(n->nav_rail_labels);
    free(n->nav_rail_icons);
  }

  // Parse labels array
  if (labelsv.tag != EVT_ARR) ergo_trap("cogito.nav_rail_set_items expects array of labels");
  ErgoArr* labels = (ErgoArr*)labelsv.as.p;
  size_t count = labels->len;

  n->nav_rail_len = count;
  n->nav_rail_cap = count;
  n->nav_rail_labels = (ErgoStr**)calloc(count, sizeof(ErgoStr*));
  n->nav_rail_icons = (ErgoStr**)calloc(count, sizeof(ErgoStr*));

  for (size_t i = 0; i < count; i++) {
    ErgoVal lv = labels->items[i];
    ErgoStr* ls = stdr_to_string(lv);
    n->nav_rail_labels[i] = ls;
    ergo_retain_val(EV_STR(ls));
  }

  // Parse icons array if provided
  if (iconsv.tag == EVT_ARR) {
    ErgoArr* icons = (ErgoArr*)iconsv.as.p;
    size_t icon_count = icons->len < count ? icons->len : count;
    for (size_t i = 0; i < icon_count; i++) {
      ErgoVal iv = icons->items[i];
      ErgoStr* is = stdr_to_string(iv);
      n->nav_rail_icons[i] = is;
      ergo_retain_val(EV_STR(is));
    }
  }

  cogito_window_relayout(cogito_node_window(n));
}

static ErgoVal cogito_nav_rail_get_selected(ErgoVal railv) {
  if (railv.tag != EVT_OBJ) ergo_trap("cogito.nav_rail_get_selected expects nav_rail");
  CogitoNode* n = (CogitoNode*)railv.as.p;
  return EV_INT(n->nav_rail_selected);
}

static void cogito_nav_rail_set_selected(ErgoVal railv, ErgoVal idxv) {
  if (railv.tag != EVT_OBJ) ergo_trap("cogito.nav_rail_set_selected expects nav_rail");
  CogitoNode* n = (CogitoNode*)railv.as.p;
  int idx = (int)ergo_as_int(idxv);
  if (idx >= 0 && idx < (int)n->nav_rail_len) {
    n->nav_rail_selected = idx;
  }
}

static void cogito_nav_rail_on_change(ErgoVal railv, ErgoVal handler) {
  if (railv.tag != EVT_OBJ) ergo_trap("cogito.nav_rail_on_change expects nav_rail");
  CogitoNode* n = (CogitoNode*)railv.as.p;
  if (n->on_select) ergo_release_val(EV_FN(n->on_select));
  if (handler.tag == EVT_FN) {
    n->on_select = (ErgoFn*)handler.as.p;
    ergo_retain_val(EV_FN(n->on_select));
  } else {
    n->on_select = NULL;
  }
}

static int cogito_nav_rail_hit_index(CogitoNode* n, int x, int y) {
  if (!n || n->kind != COGITO_NAV_RAIL) return -1;
  if (x < n->x || x >= n->x + n->w || y < n->y || y >= n->y + n->h) return -1;

  int item_h = 56;  // Standard item height
  int item_y = n->y + n->padding_top;

  for (size_t i = 0; i < n->nav_rail_len; i++) {
    if (y >= item_y && y < item_y + item_h) {
      return (int)i;
    }
    item_y += item_h;
  }
  return -1;
}

static CogitoNode* cogito_find_nav_rail_hit(CogitoNode* n, int x, int y, int* out_idx) {
  if (!n) return NULL;
  if (n->kind == COGITO_NAV_RAIL) {
    int idx = cogito_nav_rail_hit_index(n, x, y);
    if (idx >= 0) {
      if (out_idx) *out_idx = idx;
      return n;
    }
  }
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* hit = cogito_find_nav_rail_hit(n->children[i], x, y, out_idx);
    if (hit) return hit;
  }
  return NULL;
}

static void cogito_handle_nav_rail_select(CogitoNode* rail, int idx) {
  rail->nav_rail_selected = idx;
  cogito_invoke_select(rail, idx, false);
}
