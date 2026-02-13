static void cogito_layout_window(CogitoNode* win) {
  win->x = 0;
  win->y = 0;
  int cx = win->padding_left;
  int cy = win->padding_top;
  int appbar_h = 0;
  int toolbar_h = 0;
  int nav_rail_w = 0;
  int bottom_nav_h = 0;
  CogitoNode* appbar = NULL;
  CogitoNode* toolbar = NULL;
  CogitoNode* nav_rail = NULL;
  CogitoNode* bottom_nav = NULL;
  for (size_t i = 0; i < win->len; i++) {
    if (win->children[i]->kind == COGITO_APPBAR) {
      appbar = win->children[i];
      break;
    }
  }
  for (size_t i = 0; i < win->len; i++) {
    if (win->children[i]->kind == COGITO_TOOLBAR) {
      toolbar = win->children[i];
      break;
    }
  }
  for (size_t i = 0; i < win->len; i++) {
    if (win->children[i]->kind == COGITO_NAV_RAIL) {
      nav_rail = win->children[i];
      break;
    }
  }
  for (size_t i = 0; i < win->len; i++) {
    if (win->children[i]->kind == COGITO_BOTTOM_NAV) {
      bottom_nav = win->children[i];
      break;
    }
  }
  if (appbar) {
    cogito_intrinsic_size(appbar, NULL, &appbar_h);
    cogito_layout_node(appbar, 0, 0, win->w, appbar_h);
    cy += appbar_h;
  }
  if (toolbar) {
    cogito_intrinsic_size(toolbar, NULL, &toolbar_h);
    int ty = win->h - toolbar_h;
    if (ty < 0) ty = 0;
    cogito_layout_node(toolbar, 0, ty, win->w, toolbar_h);
  }
  if (nav_rail) {
    cogito_intrinsic_size(nav_rail, &nav_rail_w, NULL);
    int rail_y = appbar ? appbar_h : 0;
    int rail_h = win->h - rail_y;
    if (toolbar) rail_h -= toolbar_h;
    if (bottom_nav) rail_h -= 80;  // Reserve space for bottom nav
    if (rail_h < 0) rail_h = 0;
    cogito_layout_node(nav_rail, 0, rail_y, nav_rail_w, rail_h);
    cx += nav_rail_w;
  }
  if (bottom_nav) {
    cogito_intrinsic_size(bottom_nav, NULL, &bottom_nav_h);
    int nav_y = win->h - bottom_nav_h;
    if (nav_y < 0) nav_y = 0;
    cogito_layout_node(bottom_nav, 0, nav_y, win->w, bottom_nav_h);
  }
  int appbar_gap = appbar ? 6 : 0;
  int toolbar_gap = toolbar ? 6 : 0;

  // Calculate available space for expanding widgets
  int avail_w = win->w - win->padding_left - win->padding_right;
  int avail_h = win->h - win->padding_top - win->padding_bottom;
  if (appbar) avail_h -= appbar_h + appbar_gap;
  if (toolbar) avail_h -= toolbar_h + toolbar_gap;
  if (bottom_nav) avail_h -= bottom_nav_h;
  if (nav_rail) avail_w -= nav_rail_w;
  if (avail_w < 0) avail_w = 0;
  if (avail_h < 0) avail_h = 0;
  int content_x = win->padding_left + (nav_rail ? nav_rail_w : 0);
  int content_y = win->padding_top + appbar_h + appbar_gap;

  for (size_t i = 0; i < win->len; i++) {
    CogitoNode* c = win->children[i];
    if (c->kind == COGITO_APPBAR || c->kind == COGITO_TOOLBAR || c->kind == COGITO_NAV_RAIL || c->kind == COGITO_BOTTOM_NAV) continue;

    int cw = 0;
    int ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    int ch_total = ch + c->margin_top + c->margin_bottom;
    int slot_w = avail_w - c->margin_left - c->margin_right;
    if (slot_w < 0) slot_w = 0;

    // Check if widget should expand (only if explicit hexpand/vexpand)
    bool should_hexpand = c->hexpand;
    bool should_vexpand = c->vexpand;

    if (should_hexpand || should_vexpand) {
      int target_w = should_hexpand ? avail_w : cw;
      int target_h = should_vexpand ? avail_h : ch;
      cogito_layout_node(c, content_x, content_y, target_w, target_h);
      cy = content_y + target_h;
    } else {
      if (cw > slot_w) cw = slot_w;
      cogito_layout_node(c, cx + c->margin_left, cy + c->margin_top, cw, ch);
      cy += ch_total;
    }
  }
  if (win->dialog) {
    if (win->dialog->kind == COGITO_DIALOG_SLOT) {
      if (win->dialog->dialog_open && win->dialog->dialog) {
        cogito_layout_node(win->dialog, 0, 0, win->w, win->h);
      }
    } else if (win->dialog_open) {
      int dw = 0;
      int dh = 0;
      cogito_intrinsic_size(win->dialog, &dw, &dh);
      if (dw <= 0) dw = 320;
      if (dh <= 0) dh = 200;
      int dx = (win->w - dw) / 2;
      int dy = (win->h - dh) / 2;
      cogito_layout_node(win->dialog, dx, dy, dw, dh);
    }
  }
}

static void cogito_window_desired_size(CogitoNode* win, int* out_w, int* out_h) {
  int max_w = win->padding_left;
  int cy = win->padding_top;
  int appbar_h = 0;
  int toolbar_h = 0;
  for (size_t i = 0; i < win->len; i++) {
    CogitoNode* c = win->children[i];
    if (c->kind == COGITO_APPBAR) {
      cogito_intrinsic_size(c, NULL, &appbar_h);
      int row_w = win->padding_left + win->w;
      if (row_w > max_w) max_w = row_w;
      break;
    }
  }
  for (size_t i = 0; i < win->len; i++) {
    CogitoNode* c = win->children[i];
    if (c->kind == COGITO_TOOLBAR) {
      cogito_intrinsic_size(c, NULL, &toolbar_h);
      int row_w = win->padding_left + win->w;
      if (row_w > max_w) max_w = row_w;
      break;
    }
  }
  if (appbar_h > 0) cy += appbar_h;
  if (toolbar_h > 0) cy += toolbar_h;
  // Check if any child wants to expand
  bool has_vexpanding_child = false;
  for (size_t i = 0; i < win->len; i++) {
    CogitoNode* c = win->children[i];
    if (c->kind == COGITO_APPBAR || c->kind == COGITO_TOOLBAR) continue;
    if (c->vexpand) {
      has_vexpanding_child = true;
      break;
    }
  }

  // Calculate intrinsic sizes for all children
  for (size_t i = 0; i < win->len; i++) {
    CogitoNode* c = win->children[i];
    if (c->kind == COGITO_APPBAR || c->kind == COGITO_TOOLBAR) continue;
    int cw = 0;
    int ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    int cw_total = cw + c->margin_left + c->margin_right;
    int ch_total = ch + c->margin_top + c->margin_bottom;
    int row_w = win->padding_left + cw_total;
    if (row_w > max_w) max_w = row_w;
    cy += ch_total;
  }
  int dw = max_w + win->padding_right;
  // Calculate minimum height needed for intrinsic content
  int min_h = cy + win->padding_bottom;
  // If there's an expanding child, use max of minimum height and current window height
  // Otherwise, use the calculated minimum height for auto-sizing
  int dh = has_vexpanding_child ? (win->h > min_h ? win->h : min_h) : min_h;
  cogito_apply_size_constraints(win, &dw, &dh);
  if (out_w) *out_w = dw;
  if (out_h) *out_h = dh;
}

static void cogito_window_relayout(CogitoNode* win) {
  if (!win) return;
  cogito_layout_window(win);
}
