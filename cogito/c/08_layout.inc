static void cogito_layout_node(CogitoNode* n, int x, int y, int w, int h);
static void cogito_window_relayout(CogitoNode* win);

static void cogito_layout_vstack(CogitoNode* n, int x, int y) {
  int pad = n->gap;
  int avail_w = n->w - n->padding_left - n->padding_right;
  int avail_h = n->h - n->padding_top - n->padding_bottom;
  if (avail_w < 0) avail_w = 0;
  if (avail_h < 0) avail_h = 0;

  // Try both locations
  FILE* dbg = fopen("/tmp/cogito_vstack_debug.log", "a");
  if (!dbg) dbg = fopen("cogito_vstack_debug.log", "a");
  if (dbg) {
    fprintf(dbg, "\n[DEBUG VSTACK] Container: w=%d h=%d, avail_h=%d, children=%zu\n", n->w, n->h, avail_h, n->len);
    fflush(dbg);
  }

  // First pass: calculate total height of non-expanding children and count expanding ones
  int total_intrinsic = 0;
  int expand_count = 0;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    if (dbg) { fprintf(dbg, "  Child %zu: vexpand=%d\n", i, c->vexpand); fflush(dbg); }
    if (c->vexpand) {
      expand_count++;
    } else {
      int ch = 0;
      cogito_intrinsic_size(c, NULL, &ch);
      if (dbg) { fprintf(dbg, "  Child %zu intrinsic height: %d\n", i, ch); fflush(dbg); }
      total_intrinsic += ch + c->margin_top + c->margin_bottom;
    }
  }

  // Calculate gaps
  int total_gaps = (n->len > 0) ? (int)(n->len - 1) * pad : 0;
  int remaining_h = avail_h - total_intrinsic - total_gaps;
  if (remaining_h < 0) remaining_h = 0;

  // Height for each expanding child
  int expand_h = expand_count > 0 ? remaining_h / expand_count : 0;
  if (dbg) {
    fprintf(dbg, "  total_intrinsic=%d, gaps=%d, remaining=%d, expand_count=%d, expand_h=%d\n",
           total_intrinsic, total_gaps, remaining_h, expand_count, expand_h);
    fflush(dbg);
  }

  // Second pass: layout children
  int cy = y + n->padding_top;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    int cw = 0;
    int ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);

    // Check if child wants to expand vertically
    if (c->vexpand) {
      ch = expand_h - c->margin_top - c->margin_bottom;
      if (ch < 0) ch = 0;
      if (dbg) {
        fprintf(dbg, "  Child %zu EXPANDING: expand_h=%d, margins=%d/%d, final ch=%d\n",
               i, expand_h, c->margin_top, c->margin_bottom, ch);
        fflush(dbg);
      }
    } else {
      if (dbg) { fprintf(dbg, "  Child %zu NOT expanding: ch=%d\n", i, ch); fflush(dbg); }
    }

    // Horizontal expansion: if hexpand is set, or if it's a chrome widget (appbar, toolbar, etc.)
    bool is_chrome = (c->kind == COGITO_APPBAR || c->kind == COGITO_TOOLBAR ||
                      c->kind == COGITO_NAV_RAIL || c->kind == COGITO_BOTTOM_NAV);
    if (c->hexpand || is_chrome) {
      int target_w = avail_w - c->margin_left - c->margin_right;
      if (target_w < 0) target_w = 0;
      cw = target_w;

      // Recalculate height for text wrap (but not if vexpand is set)
      if (c->kind == COGITO_LABEL && c->text_wrap && !c->vexpand) {
        ch = cogito_text_wrap_height(c, c->text ? c->text->data : "", cogito_node_font_size(c), target_w);
        if (ch <= 0) ch = cogito_text_height_size(cogito_node_font_size(c));
      }
    }

    // Determine child's horizontal alignment for positioning (0=left, 1=center, 2=right)
    int child_col = c->align % 3;
    int cx = x + n->padding_left;
    int ox = 0;
    if (child_col == 1) {
      ox = (avail_w - cw) / 2;
    } else if (child_col == 2) {
      ox = avail_w - cw;
    }
    if (ox < 0) ox = 0;
    cogito_layout_node(c, cx + ox + c->margin_left, cy + c->margin_top, cw, ch);
    if (dbg) {
      fprintf(dbg, "  Child %zu final layout: x=%d y=%d w=%d h=%d\n",
             i, cx + ox + c->margin_left, cy + c->margin_top, cw, ch);
      fflush(dbg);
    }
    cy += ch + c->margin_top + c->margin_bottom;
    // Add gap only between children, not after the last one
    if (i < n->len - 1) cy += pad;
  }

  if (dbg) { fclose(dbg); }
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
  int inner_x = x + n->padding_left;
  int inner_y = y + n->padding_top;
  int inner_w = n->w - n->padding_left - n->padding_right;
  int inner_h = n->h - n->padding_top - n->padding_bottom;
  if (inner_w < 0) inner_w = 0;
  if (inner_h < 0) inner_h = 0;
  int gap = n->gap;
  int cy = inner_y + inner_h;
  for (size_t idx = n->len; idx-- > 0;) {
    CogitoNode* c = n->children[idx];
    int cw = 0;
    int ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    int cx = inner_x + inner_w - cw - c->margin_right;
    int cy_child = cy - ch - c->margin_bottom;
    if (cx < inner_x) cx = inner_x;
    if (cy_child < inner_y) cy_child = inner_y;
    cogito_layout_node(c, cx + c->margin_left, cy_child + c->margin_top, cw, ch);
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

  // First pass: calculate total width of non-expanding children and count expanding ones
  int total_intrinsic = 0;
  int expand_count = 0;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    if (c->hexpand) {
      expand_count++;
    } else {
      int cw = 0;
      cogito_intrinsic_size(c, &cw, NULL);
      total_intrinsic += cw + c->margin_left + c->margin_right;
    }
  }

  // Calculate gaps
  int total_gaps = (n->len > 0) ? (int)(n->len - 1) * pad : 0;
  int remaining_w = avail_w - total_intrinsic - total_gaps;
  if (remaining_w < 0) remaining_w = 0;

  // Width for each expanding child
  int expand_w = expand_count > 0 ? remaining_w / expand_count : 0;

  // Second pass: layout children
  int cx = x + n->padding_left;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    int cw = 0;
    int ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);

    // Check if child wants to expand horizontally
    if (c->hexpand) {
      cw = expand_w - c->margin_left - c->margin_right;
      if (cw < 0) cw = 0;
    }

    // Determine child's vertical alignment (0=top, 1=middle, 2=bottom)
    int child_row = c->align / 3;

    // Expand to fill available height if vexpand is set, or if it's a chrome widget
    bool is_chrome = (c->kind == COGITO_APPBAR || c->kind == COGITO_TOOLBAR ||
                      c->kind == COGITO_NAV_RAIL || c->kind == COGITO_BOTTOM_NAV);
    if (c->vexpand || is_chrome) {
      int target_h = avail_h - c->margin_top - c->margin_bottom;
      if (target_h < 0) target_h = 0;
      ch = target_h;
    }

    int cy = y + n->padding_top;
    int oy = 0;
    if (child_row == 1) {
      oy = (avail_h - ch) / 2;
    } else if (child_row == 2) {
      oy = avail_h - ch;
    }
    if (oy < 0) oy = 0;
    cogito_layout_node(c, cx + c->margin_left, cy + oy + c->margin_top, cw, ch);
    cx += cw + c->margin_left + c->margin_right;
    // Add gap only between children, not after the last one
    if (i < n->len - 1) cx += pad;
  }
}

static void cogito_layout_list(CogitoNode* n, int x, int y) {
  int pad = n->gap;
  int cy = y + n->padding_top;
  int item_inset = 0;  // Horizontal inset for items (set to 0 for now, controlled by list padding)
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    int cw = n->w - n->padding_left - n->padding_right - item_inset * 2;
    int ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    if (c->kind == COGITO_LABEL && c->text_wrap) {
      int target_w = cw - c->margin_left - c->margin_right;
      if (target_w < 0) target_w = 0;
      cw = target_w;
      ch = cogito_text_wrap_height(c, c->text ? c->text->data : "", cogito_node_font_size(c), target_w);
      if (ch <= 0) ch = cogito_text_height_size(cogito_node_font_size(c));
    }
    // Enforce 42px minimum height for list items
    if (ch < 42) ch = 42;
    // Items fill available width/height within list padding
    cogito_layout_node(c, x + n->padding_left + item_inset, cy, cw, ch);
    cy += ch;
    // Add gap only between items, not after the last one
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
  if (!row_heights) {
    free(rows); free(cols_out); free(span_x); free(span_y); free(occ);
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
  int *row_y = (int*)calloc((size_t)row_count, sizeof(int));
  if (!row_y) {
    free(rows); free(cols_out); free(span_x); free(span_y); free(occ); free(row_heights);
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
    int cw = 0;
    int ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    if (cw > cell_w_span) cw = cell_w_span;
    if (ch > cell_h_span) ch = cell_h_span;
    int ax = c->grid_align_x >= 0 ? c->grid_align_x : n->grid_align_x;
    int ay = c->grid_align_y >= 0 ? c->grid_align_y : n->grid_align_y;
    int ox = 0;
    int oy = 0;
    if (ax == 1) ox = (cell_w_span - cw) / 2;
    else if (ax == 2) ox = cell_w_span - cw;
    if (ay == 1) oy = (cell_h_span - ch) / 2;
    else if (ay == 2) oy = cell_h_span - ch;
    if (ox < 0) ox = 0;
    if (oy < 0) oy = 0;
    cogito_layout_node(c, cell_x + ox, cell_y + oy, cw, ch);
  }
  free(rows);
  free(cols_out);
  free(span_x);
  free(span_y);
  free(occ);
  free(row_heights);
  free(row_y);
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

    case COGITO_KIND_COUNT:
      break;
  

}
}

static void cogito_layout_window(CogitoNode* win) {
  FILE* wdbg = fopen("/tmp/cogito_window_debug.log", "a");
  if (!wdbg) wdbg = fopen("cogito_window_debug.log", "a");
  if (wdbg) {
    fprintf(wdbg, "\n[WINDOW LAYOUT] w=%d h=%d\n", win->w, win->h);
    fflush(wdbg);
    fclose(wdbg);
  }

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

    // Check if widget should expand (only if explicit hexpand/vexpand)
    bool should_hexpand = c->hexpand;
    bool should_vexpand = c->vexpand;

    if (should_hexpand || should_vexpand) {
      int target_w = should_hexpand ? avail_w : cw;
      int target_h = should_vexpand ? avail_h : ch;
      cogito_layout_node(c, content_x, content_y, target_w, target_h);
      cy = content_y + target_h;
    } else {
      cogito_layout_node(c, cx + c->margin_left, cy + c->margin_top, cw, ch);
      cy += ch_total;
    }
  }
  if (win->dialog) {
    if (win->dialog->kind == COGITO_DIALOG_SLOT) {
      cogito_layout_node(win->dialog, 0, 0, win->w, win->h);
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
