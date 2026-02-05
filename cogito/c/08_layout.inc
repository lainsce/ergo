static void cogito_layout_node(CogitoNode* n, int x, int y, int w, int h);
static void cogito_window_relayout(CogitoNode* win);

static void cogito_layout_vstack(CogitoNode* n, int x, int y) {
  int pad = 6;
  int col = n->align % 3;
  int cy = y + n->padding_top;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    int cw = 0;
    int ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    int avail_w = n->w - n->padding_left - n->padding_right;
    if (avail_w < 0) avail_w = 0;
    if (c->kind == COGITO_LABEL && c->text_wrap) {
      int target_w = avail_w - c->margin_left - c->margin_right;
      if (target_w < 0) target_w = 0;
      cw = target_w;
      ch = cogito_text_wrap_height(c, c->text ? c->text->data : "", cogito_node_font_size(c), target_w);
      if (ch <= 0) ch = cogito_text_height_size(cogito_node_font_size(c));
    }
    int cx = x + n->padding_left;
    int ox = 0;
    if (col == 1) {
      ox = (n->w - cw) / 2;
    } else if (col == 2) {
      ox = n->w - cw - n->padding_right;
    }
    if (ox < 0) ox = 0;
    if (col == 1) {
      cogito_layout_node(c, cx + ox + c->margin_left, cy + c->margin_top, cw, ch);
    } else if (col == 2) {
      cogito_layout_node(c, cx + ox + c->margin_left, cy + c->margin_top, cw, ch);
    } else {
      cogito_layout_node(c, cx + c->margin_left, cy + c->margin_top, cw, ch);
    }
    cy += ch + c->margin_top + c->margin_bottom + pad;
  }
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
  int gap = 6;
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
    cy = cy_child - c->margin_top - gap;
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
  int pad = 6;
  int row = n->align / 3;
  int cx = x + n->padding_left;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    int cw = 0;
    int ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    int cy = y + n->padding_top;
    int oy = 0;
    if (row == 1) {
      oy = (n->h - ch) / 2;
    } else if (row == 2) {
      oy = n->h - ch - n->padding_bottom;
    }
    if (oy < 0) oy = 0;
    if (row == 1) {
      cogito_layout_node(c, cx + c->margin_left, cy + oy + c->margin_top, cw, ch);
    } else if (row == 2) {
      cogito_layout_node(c, cx + c->margin_left, cy + oy + c->margin_top, cw, ch);
    } else {
      cogito_layout_node(c, cx + c->margin_left, cy + c->margin_top, cw, ch);
    }
    cx += cw + c->margin_left + c->margin_right + pad;
  }
}

static void cogito_layout_list(CogitoNode* n, int x, int y) {
  int pad = 6;
  int cy = y + n->padding_top;
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* c = n->children[i];
    int cw = n->w - n->padding_left - n->padding_right;
    int ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    if (c->kind == COGITO_LABEL && c->text_wrap) {
      int target_w = cw - c->margin_left - c->margin_right;
      if (target_w < 0) target_w = 0;
      cw = target_w;
      ch = cogito_text_wrap_height(c, c->text ? c->text->data : "", cogito_node_font_size(c), target_w);
      if (ch <= 0) ch = cogito_text_height_size(cogito_node_font_size(c));
    }
    cogito_layout_node(c, x + n->padding_left + pad, cy, cw, ch);
    cy += ch + pad;
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

    #include "widgets/layout/searchfield.inc"

    #include "widgets/layout/datepicker.inc"

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

    #include "widgets/layout/checkbox.inc"

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
  int pad = 12;
  win->x = 0;
  win->y = 0;
  int cx = pad + win->padding_left;
  int cy = pad + win->padding_top;
  int appbar_h = 0;
  int toolbar_h = 0;
  CogitoNode* appbar = NULL;
  CogitoNode* toolbar = NULL;
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
  if (appbar) {
    cogito_intrinsic_size(appbar, NULL, &appbar_h);
    cogito_layout_node(appbar, 0, 0, win->w, appbar_h);
    cy += appbar_h + 6;
  }
  if (toolbar) {
    cogito_intrinsic_size(toolbar, NULL, &toolbar_h);
    int ty = win->h - toolbar_h;
    if (ty < 0) ty = 0;
    cogito_layout_node(toolbar, 0, ty, win->w, toolbar_h);
  }
  int appbar_gap = appbar ? 6 : 0;
  int toolbar_gap = toolbar ? 6 : 0;
  for (size_t i = 0; i < win->len; i++) {
    CogitoNode* c = win->children[i];
    if (c->kind == COGITO_APPBAR || c->kind == COGITO_TOOLBAR) continue;
    int cw = 0;
    int ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    int ch_total = ch + c->margin_top + c->margin_bottom;
    if (c->kind == COGITO_VSTACK || c->kind == COGITO_HSTACK) {
      int avail_w = win->w - win->padding_left - win->padding_right;
      int avail_h = win->h - win->padding_top - win->padding_bottom;
      if (appbar) avail_h -= appbar_h + appbar_gap;
      if (toolbar) avail_h -= toolbar_h + toolbar_gap;
      if (avail_w < 0) avail_w = 0;
      if (avail_h < 0) avail_h = 0;
      cogito_layout_node(c, win->padding_left, win->padding_top + appbar_h + appbar_gap, avail_w, avail_h);
      cy += avail_h + pad + appbar_gap;
    } else {
      cogito_layout_node(c, cx + c->margin_left, cy + c->margin_top, cw, ch);
      cy += ch_total + pad;
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
  int pad = 12;
  int max_w = pad + win->padding_left;
  int cy = pad + win->padding_top;
  int appbar_h = 0;
  int toolbar_h = 0;
  for (size_t i = 0; i < win->len; i++) {
    CogitoNode* c = win->children[i];
    if (c->kind == COGITO_APPBAR) {
      cogito_intrinsic_size(c, NULL, &appbar_h);
      int row_w = pad + win->padding_left + win->w;
      if (row_w > max_w) max_w = row_w;
      break;
    }
  }
  for (size_t i = 0; i < win->len; i++) {
    CogitoNode* c = win->children[i];
    if (c->kind == COGITO_TOOLBAR) {
      cogito_intrinsic_size(c, NULL, &toolbar_h);
      int row_w = pad + win->padding_left + win->w;
      if (row_w > max_w) max_w = row_w;
      break;
    }
  }
  if (appbar_h > 0) cy += appbar_h + 6;
  if (toolbar_h > 0) cy += toolbar_h + 6;
  for (size_t i = 0; i < win->len; i++) {
    CogitoNode* c = win->children[i];
    if (c->kind == COGITO_APPBAR || c->kind == COGITO_TOOLBAR) continue;
    int cw = 0;
    int ch = 0;
    cogito_intrinsic_size(c, &cw, &ch);
    int cw_total = cw + c->margin_left + c->margin_right;
    int ch_total = ch + c->margin_top + c->margin_bottom;
    int row_w = pad + win->padding_left + cw_total;
    if (row_w > max_w) max_w = row_w;
    cy += ch_total + pad;
  }
  int dw = max_w + win->padding_right + pad;
  int dh = cy + win->padding_bottom + pad;
  cogito_apply_size_constraints(win, &dw, &dh);
  if (out_w) *out_w = dw;
  if (out_h) *out_h = dh;
}

static void cogito_window_relayout(CogitoNode* win) {
  if (!win) return;
  cogito_layout_window(win);
}
