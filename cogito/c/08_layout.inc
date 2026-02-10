static void cogito_layout_node(CogitoNode* n, int x, int y, int w, int h);
static void cogito_window_relayout(CogitoNode* win);
static void cogito_dialog_close_rect(CogitoNode* n, int* out_x, int* out_y, int* out_w, int* out_h);

typedef struct CogitoTreeRow {
  CogitoNode* node;
  int depth;
} CogitoTreeRow;

static void cogito_distribute_axis_sizes(size_t count, const int* min_sizes, const bool* expand_flags, int available, int* out_sizes) {
  if (!out_sizes || !min_sizes || count == 0) return;
  if (available < 0) available = 0;

  int sum = 0;
  for (size_t i = 0; i < count; i++) {
    int v = min_sizes[i];
    if (v < 0) v = 0;
    out_sizes[i] = v;
    sum += v;
  }

  if (sum == available) return;

  if (sum < available) {
    int extra = available - sum;
    size_t expand_count = 0;
    for (size_t i = 0; i < count; i++) {
      if (expand_flags && expand_flags[i]) expand_count++;
    }
    if (expand_count == 0) return;
    while (extra > 0) {
      bool changed = false;
      for (size_t i = 0; i < count && extra > 0; i++) {
        if (expand_flags && expand_flags[i]) {
          out_sizes[i]++;
          extra--;
          changed = true;
        }
      }
      if (!changed) break;
    }
    return;
  }

  int overflow = sum - available;
  bool has_expand = false;
  for (size_t i = 0; i < count; i++) {
    if (expand_flags && expand_flags[i]) {
      has_expand = true;
      break;
    }
  }
  if (has_expand) {
    while (overflow > 0) {
      bool changed = false;
      for (size_t i = 0; i < count && overflow > 0; i++) {
        if (expand_flags && expand_flags[i] && out_sizes[i] > 0) {
          out_sizes[i]--;
          overflow--;
          changed = true;
        }
      }
      if (!changed) break;
    }
  }
  while (overflow > 0) {
    bool changed = false;
    for (size_t i = 0; i < count && overflow > 0; i++) {
      if (out_sizes[i] > 0) {
        out_sizes[i]--;
        overflow--;
        changed = true;
      }
    }
    if (!changed) break;
  }
}

static void cogito_treeview_metrics(int* out_row_h, int* out_pad, int* out_indent_step,
                                    int* out_chevron_w, int* out_chevron_h, int* out_chevron_gap) {
  if (out_row_h) *out_row_h = 22;
  if (out_pad) *out_pad = 6;
  if (out_indent_step) *out_indent_step = 12;
  if (out_chevron_w) *out_chevron_w = 10;
  if (out_chevron_h) *out_chevron_h = 12;
  if (out_chevron_gap) *out_chevron_gap = 2;
}

static void cogito_treeview_inner_rect(CogitoNode* n, int* out_x, int* out_y, int* out_w, int* out_h) {
  int inner_x = n->x + n->padding_left;
  int inner_y = n->y + n->padding_top;
  int inner_w = n->w - n->padding_left - n->padding_right;
  int inner_h = n->h - n->padding_top - n->padding_bottom;
  if (inner_w < 0) inner_w = 0;
  if (inner_h < 0) inner_h = 0;
  if (out_x) *out_x = inner_x;
  if (out_y) *out_y = inner_y;
  if (out_w) *out_w = inner_w;
  if (out_h) *out_h = inner_h;
}

static void cogito_treeview_node_label(CogitoNode* n, char* out, size_t cap) {
  if (!out || cap == 0) return;
  if (!n) {
    snprintf(out, cap, "<null>");
    return;
  }
  if (n->text && n->text->data && n->text->data[0]) {
    snprintf(out, cap, "%s", n->text->data);
    return;
  }
  const char* kind = cogito_kind_name(n->kind);
  const char* cls = (n->class_name && n->class_name->data && n->class_name->data[0]) ? n->class_name->data : NULL;
  if (cls) snprintf(out, cap, "%s.%s", kind, cls);
  else snprintf(out, cap, "%s", kind);
}

static size_t cogito_treeview_count_subtree_nodes(const CogitoNode* n) {
  if (!n) return 0;
  size_t total = 1;
  for (size_t i = 0; i < n->len; i++) {
    total += cogito_treeview_count_subtree_nodes(n->children[i]);
  }
  return total;
}

static size_t cogito_treeview_visible_row_capacity(const CogitoNode* tree) {
  if (!tree) return 0;
  size_t total = 0;
  for (size_t i = 0; i < tree->len; i++) {
    total += cogito_treeview_count_subtree_nodes(tree->children[i]);
  }
  return total;
}

static size_t cogito_treeview_collect_rows_recursive(CogitoNode* n, int depth, CogitoTreeRow* rows, size_t cap) {
  if (!n || !rows || cap == 0) return 0;
  rows[0].node = n;
  rows[0].depth = depth;
  size_t used = 1;
  if (n->tree_collapsed) return used;
  for (size_t i = 0; i < n->len && used < cap; i++) {
    used += cogito_treeview_collect_rows_recursive(n->children[i], depth + 1, rows + used, cap - used);
  }
  return used;
}

static size_t cogito_treeview_collect_visible_rows(CogitoNode* tree, CogitoTreeRow* rows, size_t cap) {
  if (!tree || !rows || cap == 0) return 0;
  size_t used = 0;
  for (size_t i = 0; i < tree->len && used < cap; i++) {
    used += cogito_treeview_collect_rows_recursive(tree->children[i], 0, rows + used, cap - used);
  }
  return used;
}

static void cogito_treeview_hide_subtree(CogitoNode* n) {
  if (!n) return;
  n->x = 0;
  n->y = 0;
  n->w = 0;
  n->h = 0;
  for (size_t i = 0; i < n->len; i++) {
    cogito_treeview_hide_subtree(n->children[i]);
  }
}

static void cogito_layout_treeview(CogitoNode* n, int x, int y) {
  (void)x;
  (void)y;
  int inner_x = 0, inner_y = 0, inner_w = 0, inner_h = 0;
  cogito_treeview_inner_rect(n, &inner_x, &inner_y, &inner_w, &inner_h);
  for (size_t i = 0; i < n->len; i++) {
    cogito_treeview_hide_subtree(n->children[i]);
  }
  if (inner_w <= 0 || inner_h <= 0 || n->len == 0) return;

  size_t cap = cogito_treeview_visible_row_capacity(n);
  if (cap == 0) return;
  CogitoTreeRow* rows = (CogitoTreeRow*)calloc(cap, sizeof(CogitoTreeRow));
  if (!rows) return;
  size_t row_count = cogito_treeview_collect_visible_rows(n, rows, cap);
  int row_h = 0, pad = 0, indent_step = 0, chevron_w = 0, chevron_h = 0, chevron_gap = 0;
  cogito_treeview_metrics(&row_h, &pad, &indent_step, &chevron_w, &chevron_h, &chevron_gap);
  int y_cursor = inner_y;
  int max_y = inner_y + inner_h;
  for (size_t i = 0; i < row_count; i++) {
    if (y_cursor >= max_y) break;
    CogitoNode* c = rows[i].node;
    if (!c) continue;
    int depth = rows[i].depth;
    int indent_x = inner_x + pad + depth * indent_step;
    int label_x = indent_x + (c->len > 0 ? (chevron_w + chevron_gap) : 0);
    int row_w = inner_x + inner_w - pad - label_x;
    if (row_w < 0) row_w = 0;
    c->x = label_x;
    c->y = y_cursor;
    c->w = row_w;
    c->h = row_h;
    y_cursor += row_h;
  }
  free(rows);
}

static void cogito_layout_vstack(CogitoNode* n, int x, int y) {
  int pad = n->gap;
  int avail_w = n->w - n->padding_left - n->padding_right;
  int avail_h = n->h - n->padding_top - n->padding_bottom;
  if (avail_w < 0) avail_w = 0;
  if (avail_h < 0) avail_h = 0;
  size_t count = n->len;
  if (count == 0) return;
  int* intrinsic_w = (int*)calloc(count, sizeof(int));
  int* intrinsic_h = (int*)calloc(count, sizeof(int));
  int* assigned_h = (int*)calloc(count, sizeof(int));
  bool* expand = (bool*)calloc(count, sizeof(bool));
  if (!intrinsic_w || !intrinsic_h || !assigned_h || !expand) {
    free(intrinsic_w);
    free(intrinsic_h);
    free(assigned_h);
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
    assigned_h[i] = ch;
    expand[i] = c->vexpand;
    total_margins += c->margin_top + c->margin_bottom;
  }
  int total_gaps = (count > 0) ? (int)(count - 1) * pad : 0;
  int avail_content_h = avail_h - total_margins - total_gaps;
  if (avail_content_h < 0) avail_content_h = 0;
  cogito_distribute_axis_sizes(count, intrinsic_h, expand, avail_content_h, assigned_h);

  int cy = y + n->padding_top;
  for (size_t i = 0; i < count; i++) {
    CogitoNode* c = n->children[i];
    int cw = intrinsic_w[i];
    int ch = assigned_h[i];
    int slot_w = avail_w - c->margin_left - c->margin_right;
    if (slot_w < 0) slot_w = 0;

    bool is_chrome = (c->kind == COGITO_APPBAR || c->kind == COGITO_TOOLBAR ||
                      c->kind == COGITO_NAV_RAIL || c->kind == COGITO_BOTTOM_NAV);
    if (c->hexpand || is_chrome) {
      cw = slot_w;
      if (c->kind == COGITO_LABEL && c->text_wrap && !c->vexpand) {
        ch = cogito_text_wrap_height(c, c->text ? c->text->data : "", cogito_node_font_size(c), slot_w);
        if (ch <= 0) ch = cogito_text_height_size(cogito_node_font_size(c));
      }
    } else if (cw > slot_w) {
      cw = slot_w;
    }

    int child_col = c->align % 3;
    if (child_col < 0 || child_col > 2) child_col = 0;
    int cx = x + n->padding_left;
    int ox = 0;
    if (child_col == 1) {
      ox = (slot_w - cw) / 2;
    } else if (child_col == 2) {
      ox = slot_w - cw;
    }
    if (ox < 0) ox = 0;
    cogito_layout_node(c, cx + ox + c->margin_left, cy + c->margin_top, cw, ch);
    cy += ch + c->margin_top + c->margin_bottom;
    if (i < count - 1) cy += pad;
  }
  free(intrinsic_w);
  free(intrinsic_h);
  free(assigned_h);
  free(expand);
}

static void cogito_layout_zstack(CogitoNode* n, int x, int y) {
  int inner_x = x + n->padding_left;
  int inner_y = y + n->padding_top;
  int inner_w = n->w - n->padding_left - n->padding_right;
  int inner_h = n->h - n->padding_top - n->padding_bottom;
  if (inner_w < 0) inner_w = 0;
  if (inner_h < 0) inner_h = 0;
  int align = n->align;
  if (align < 0 || align > 8) align = 0;
  int row = align / 3;
  int col = align % 3;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    int cw = 0;
    int ch = 0;
    int ox = 0;
    int oy = 0;
    int avail_w = inner_w - c->margin_left - c->margin_right;
    int avail_h = inner_h - c->margin_top - c->margin_bottom;
    if (avail_w < 0) avail_w = 0;
    if (avail_h < 0) avail_h = 0;
    if (c->kind == COGITO_DIALOG_SLOT) {
      cw = avail_w;
      ch = avail_h;
    } else {
      cogito_intrinsic_size(c, &cw, &ch);
    }
    int col_use = col;
    int row_use = row;
    if (c->kind == COGITO_TOASTS) {
      col_use = 2;
      row_use = 2;
    }
    if (col_use == 1) ox = (avail_w - cw) / 2;
    else if (col_use == 2) ox = avail_w - cw;
    if (row_use == 1) oy = (avail_h - ch) / 2;
    else if (row_use == 2) oy = avail_h - ch;
    if (ox < 0) ox = 0;
    if (oy < 0) oy = 0;
    cogito_layout_node(c, inner_x + c->margin_left + ox, inner_y + c->margin_top + oy, cw, ch);
  }
}

static CogitoNode* cogito_view_switcher_active_child(CogitoNode* n) {
  if (!n || n->len == 0) return NULL;
  if (n->view_active_id) {
    for (size_t i = 0; i < n->len; i++) {
      CogitoNode* c = n->children[i];
      if (c->view_id && cogito_str_eq(c->view_id, n->view_active_id)) return c;
    }
  }
  return n->children[0];
}

static void cogito_layout_view_switcher(CogitoNode* n, int x, int y) {
  int inner_x = x + n->padding_left;
  int inner_y = y + n->padding_top;
  int inner_w = n->w - n->padding_left - n->padding_right;
  int inner_h = n->h - n->padding_top - n->padding_bottom;
  if (inner_w < 0) inner_w = 0;
  if (inner_h < 0) inner_h = 0;
  CogitoNode* active = cogito_view_switcher_active_child(n);
  int align = n->align;
  if (align < 0 || align > 8) align = 0;
  int row = align / 3;
  int col = align % 3;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    if (c != active) {
      cogito_layout_node(c, inner_x, inner_y, 0, 0);
      continue;
    }
    int cw = 0;
    int ch = 0;
    int ox = 0;
    int oy = 0;
    int avail_w = inner_w - c->margin_left - c->margin_right;
    int avail_h = inner_h - c->margin_top - c->margin_bottom;
    if (avail_w < 0) avail_w = 0;
    if (avail_h < 0) avail_h = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    if (c->hexpand) cw = avail_w;
    if (c->vexpand) ch = avail_h;
    if (col == 1) ox = (avail_w - cw) / 2;
    else if (col == 2) ox = avail_w - cw;
    if (row == 1) oy = (avail_h - ch) / 2;
    else if (row == 2) oy = avail_h - ch;
    if (ox < 0) ox = 0;
    if (oy < 0) oy = 0;
    cogito_layout_node(c, inner_x + c->margin_left + ox, inner_y + c->margin_top + oy, cw, ch);
  }
}

static void cogito_layout_toasts(CogitoNode* n, int x, int y) {
  CogitoNode* win = cogito_node_window(n);
  int base_x = win ? 0 : x;
  int base_y = win ? 0 : y;
  int base_w = win ? win->w : n->w;
  int base_h = win ? win->h : n->h;
  if (base_w < 0) base_w = 0;
  if (base_h < 0) base_h = 0;
  if (win) {
    n->x = base_x;
    n->y = base_y;
    n->w = base_w;
    n->h = base_h;
  }
  int inner_x = base_x + n->padding_left;
  int inner_y = base_y + n->padding_top;
  int inner_w = base_w - n->padding_left - n->padding_right;
  int inner_h = base_h - n->padding_top - n->padding_bottom;
  if (inner_w < 0) inner_w = 0;
  if (inner_h < 0) inner_h = 0;
  int gap = n->gap > 0 ? n->gap : 6;
  int cy = inner_y + inner_h;
  for (size_t idx = n->len; idx-- > 0;) {
    CogitoNode* c = n->children[idx];
    int cw = 0;
    int ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    int slot_w = inner_w - c->margin_left - c->margin_right;
    if (slot_w < 0) slot_w = 0;
    if (cw > slot_w) cw = slot_w;
    cy -= c->margin_bottom + ch;
    int cy_child = cy;
    int min_y = inner_y + c->margin_top;
    if (cy_child < min_y) cy_child = min_y;
    int cx = inner_x + c->margin_left + (slot_w - cw) / 2;
    if (cx < inner_x + c->margin_left) cx = inner_x + c->margin_left;
    cogito_layout_node(c, cx, cy_child, cw, ch);
    cy = cy_child - c->margin_top;
    // Add gap only between items, not before the first one (we're iterating backwards)
    if (idx > 0) cy -= gap;
  }
}

static void cogito_layout_fixed(CogitoNode* n, int x, int y) {
  int base_x = x + n->padding_left;
  int base_y = y + n->padding_top;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    int cw = 0;
    int ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    int cx = base_x + (c->fixed_pos_set ? c->fixed_x : 0);
    int cy = base_y + (c->fixed_pos_set ? c->fixed_y : 0);
    cogito_layout_node(c, cx + c->margin_left, cy + c->margin_top, cw, ch);
  }
}

static void cogito_layout_scroller(CogitoNode* n, int x, int y) {
  int inner_x = x + n->padding_left;
  int inner_y = y + n->padding_top;
  int inner_w = n->w - n->padding_left - n->padding_right;
  int inner_h = n->h - n->padding_top - n->padding_bottom;
  if (inner_w < 0) inner_w = 0;
  if (inner_h < 0) inner_h = 0;
  if (n->len == 0) return;
  CogitoNode* c = n->children[0];
  int cw = 0;
  int ch = 0;
  cogito_intrinsic_size(c, &cw, &ch);
  if (!n->scroll_horz) {
    cw = inner_w;
  } else if (cw < inner_w) {
    cw = inner_w;
  }
  if (!n->scroll_vert) {
    ch = inner_h;
  } else if (ch < inner_h) {
    ch = inner_h;
  }
  n->scroll_content_w = cw;
  n->scroll_content_h = ch;
  int max_x = cw - inner_w;
  int max_y = ch - inner_h;
  if (max_x < 0) max_x = 0;
  if (max_y < 0) max_y = 0;
  if (n->scroll_x < 0) n->scroll_x = 0;
  if (n->scroll_y < 0) n->scroll_y = 0;
  if (n->scroll_x > max_x) n->scroll_x = max_x;
  if (n->scroll_y > max_y) n->scroll_y = max_y;
  cogito_layout_node(c, inner_x - n->scroll_x, inner_y - n->scroll_y, cw, ch);
  for (size_t i = 1; i < n->len; i++) {
    cogito_layout_node(n->children[i], inner_x, inner_y, 0, 0);
  }
}

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

static void cogito_layout_list(CogitoNode* n, int x, int y) {
  int pad = n->gap;
  int cy = y + n->padding_top;
  int item_pad_x = 6;
  int item_pad_y = 6;
  int avail_w = n->w - n->padding_left - n->padding_right;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    // Apply item padding to children that don't already have explicit padding
    if (!c->padding_left) c->padding_left = item_pad_x;
    if (!c->padding_right) c->padding_right = item_pad_x;
    if (!c->padding_top) c->padding_top = item_pad_y;
    if (!c->padding_bottom) c->padding_bottom = item_pad_y;
    int cw = 0;
    int ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    int slot_w = avail_w - c->margin_left - c->margin_right;
    if (slot_w < 0) slot_w = 0;
    // List items always fill the available width
    cw = slot_w;
    if (c->kind == COGITO_LABEL && c->text_wrap) {
      ch = cogito_text_wrap_height(c, c->text ? c->text->data : "", cogito_node_font_size(c), slot_w);
      if (ch <= 0) ch = cogito_text_height_size(cogito_node_font_size(c));
    }
    // Enforce 42px minimum height for list items
    if (ch < 42) ch = 42;
    cy += c->margin_top;
    cogito_layout_node(c, x + n->padding_left + c->margin_left, cy, cw, ch);
    cy += ch + c->margin_bottom;
    if (i < n->len - 1) cy += pad;
  }
}

static void cogito_layout_grid(CogitoNode* n, int x, int y) {
  int cols = n->grid_cols > 0 ? n->grid_cols : 1;
  int gap_x = n->grid_gap_x;
  int gap_y = n->grid_gap_y;
  int inner_w = n->w - n->padding_left - n->padding_right;
  if (inner_w < 0) inner_w = 0;
  int cell_w = cols > 0 ? (inner_w - (cols - 1) * gap_x) / cols : 0;
  if (cell_w < 0) cell_w = 0;
  size_t count = n->len;
  if (count == 0) return;
  int *rows = (int*)calloc(count, sizeof(int));
  int *cols_out = (int*)calloc(count, sizeof(int));
  int *span_x = (int*)calloc(count, sizeof(int));
  int *span_y = (int*)calloc(count, sizeof(int));
  if (!rows || !cols_out || !span_x || !span_y) {
    free(rows); free(cols_out); free(span_x); free(span_y);
    return;
  }
  int row_cap = 4;
  int row_count = 0;
  bool *occ = (bool*)calloc((size_t)row_cap * (size_t)cols, sizeof(bool));
  if (!occ) {
    free(rows); free(cols_out); free(span_x); free(span_y);
    return;
  }
  for (size_t i = 0; i < count; i++) {
    CogitoNode* c = n->children[i];
    int cs = c->grid_span_x > 0 ? c->grid_span_x : 1;
    int rs = c->grid_span_y > 0 ? c->grid_span_y : 1;
    if (cs > cols) cs = cols;
    int r = 0;
    int ccol = 0;
    bool placed = false;
    while (!placed) {
      if (r >= row_cap) {
        int new_cap = row_cap * 2;
        bool *next = (bool*)realloc(occ, (size_t)new_cap * (size_t)cols * sizeof(bool));
        if (!next) break;
        memset(next + (size_t)row_cap * (size_t)cols, 0, (size_t)(new_cap - row_cap) * (size_t)cols * sizeof(bool));
        occ = next;
        row_cap = new_cap;
      }
      for (ccol = 0; ccol + cs <= cols; ccol++) {
        bool ok = true;
        for (int rr = 0; rr < rs; rr++) {
          int row_idx = r + rr;
          if (row_idx >= row_cap) { ok = false; break; }
          for (int cc = 0; cc < cs; cc++) {
            if (occ[row_idx * cols + (ccol + cc)]) { ok = false; break; }
          }
          if (!ok) break;
        }
        if (ok) {
          for (int rr = 0; rr < rs; rr++) {
            int row_idx = r + rr;
            if (row_idx >= row_count) row_count = row_idx + 1;
            for (int cc = 0; cc < cs; cc++) {
              occ[row_idx * cols + (ccol + cc)] = true;
            }
          }
          placed = true;
          break;
        }
      }
      if (!placed) r++;
    }
    rows[i] = r;
    cols_out[i] = ccol;
    span_x[i] = cs;
    span_y[i] = rs;
  }
  int *row_heights = (int*)calloc((size_t)row_count, sizeof(int));
  bool *row_expand = (bool*)calloc((size_t)row_count, sizeof(bool));
  if (!row_heights) {
    free(rows); free(cols_out); free(span_x); free(span_y); free(occ);
    free(row_expand);
    return;
  }
  for (size_t i = 0; i < count; i++) {
    CogitoNode* c = n->children[i];
    int ch = 0;
    cogito_intrinsic_size(c, NULL, &ch);
    int rs = span_y[i] > 0 ? span_y[i] : 1;
    int per_row = rs > 0 ? (ch + rs - 1) / rs : ch;
    for (int rr = 0; rr < rs; rr++) {
      int row_idx = rows[i] + rr;
      if (row_idx >= row_count) break;
      if (per_row > row_heights[row_idx]) row_heights[row_idx] = per_row;
    }
  }
  if (row_expand) {
    for (int r = 0; r < row_count; r++) row_expand[r] = true;
    int avail_h = n->h - n->padding_top - n->padding_bottom;
    if (avail_h < 0) avail_h = 0;
    int total_gap_h = row_count > 0 ? (row_count - 1) * gap_y : 0;
    int avail_row_h = avail_h - total_gap_h;
    if (avail_row_h < 0) avail_row_h = 0;
    int* row_out = (int*)calloc((size_t)row_count, sizeof(int));
    if (row_out) {
      cogito_distribute_axis_sizes((size_t)row_count, row_heights, row_expand, avail_row_h, row_out);
      for (int r = 0; r < row_count; r++) row_heights[r] = row_out[r];
      free(row_out);
    }
  }
  int *row_y = (int*)calloc((size_t)row_count, sizeof(int));
  if (!row_y) {
    free(rows); free(cols_out); free(span_x); free(span_y); free(occ); free(row_heights); free(row_expand);
    return;
  }
  int cy = y + n->padding_top;
  for (int r = 0; r < row_count; r++) {
    row_y[r] = cy;
    cy += row_heights[r] + gap_y;
  }
  int base_x = x + n->padding_left;
  for (size_t i = 0; i < count; i++) {
    CogitoNode* c = n->children[i];
    int r = rows[i];
    int ccol = cols_out[i];
    int cs = span_x[i] > 0 ? span_x[i] : 1;
    int rs = span_y[i] > 0 ? span_y[i] : 1;
    int cell_x = base_x + ccol * (cell_w + gap_x);
    int cell_y = row_y[r];
    int cell_w_span = cell_w * cs + gap_x * (cs - 1);
    int cell_h_span = 0;
    for (int rr = 0; rr < rs; rr++) {
      int row_idx = r + rr;
      if (row_idx >= row_count) break;
      cell_h_span += row_heights[row_idx];
      if (rr + 1 < rs) cell_h_span += gap_y;
    }
    int cw = cell_w_span - c->margin_left - c->margin_right;
    int ch = cell_h_span - c->margin_top - c->margin_bottom;
    if (cw < 0) cw = 0;
    if (ch < 0) ch = 0;
    cogito_layout_node(c, cell_x + c->margin_left, cell_y + c->margin_top, cw, ch);
  }
  free(rows);
  free(cols_out);
  free(span_x);
  free(span_y);
  free(occ);
  free(row_heights);
  free(row_y);
  free(row_expand);
}

static void cogito_layout_node(CogitoNode* n, int x, int y, int w, int h) {
  n->x = x;
  n->y = y;
  n->w = w;
  n->h = h;
  cogito_apply_size_constraints(n, &n->w, &n->h);
  switch (n->kind) {

    #include "widgets/layout/appbar.inc"

    #include "widgets/layout/dialog.inc"

    #include "widgets/layout/popover.inc"

    #include "widgets/layout/searchfield.inc"

    #include "widgets/layout/datepicker.inc"

    #include "widgets/layout/fab.inc"

    #include "widgets/layout/stepper.inc"

    #include "widgets/layout/segmented.inc"

    #include "widgets/layout/treeview.inc"

    #include "widgets/layout/colorpicker.inc"

    #include "widgets/layout/tooltip.inc"

    #include "widgets/layout/bottom_toolbar.inc"

    #include "widgets/layout/dialog_slot.inc"

    #include "widgets/layout/vstack.inc"

    #include "widgets/layout/hstack.inc"

    #include "widgets/layout/zstack.inc"

    #include "widgets/layout/view_switcher.inc"

    #include "widgets/layout/fixed.inc"

    #include "widgets/layout/scroller.inc"

    #include "widgets/layout/list.inc"

    #include "widgets/layout/grid.inc"

    #include "widgets/layout/toasts.inc"

    #include "widgets/layout/window.inc"

    #include "widgets/layout/button.inc"

    #include "widgets/layout/iconbtn.inc"

    #include "widgets/layout/image.inc"

    #include "widgets/layout/label.inc"

    #include "widgets/layout/nav_rail.inc"

    #include "widgets/layout/bottom_nav.inc"

    #include "widgets/layout/checkbox.inc"

    #include "widgets/layout/chip.inc"

    #include "widgets/layout/switch.inc"

    #include "widgets/layout/textfield.inc"

    #include "widgets/layout/textview.inc"

    #include "widgets/layout/dropdown.inc"

    #include "widgets/layout/slider.inc"

    #include "widgets/layout/tabs.inc"

    #include "widgets/layout/progress.inc"

    #include "widgets/layout/toast.inc"

    #include "widgets/layout/divider.inc"

    case COGITO_KIND_COUNT:
      break;
  

}
}

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
