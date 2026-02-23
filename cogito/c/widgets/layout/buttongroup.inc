case COGITO_BUTTON_GROUP: {
      // ButtonGroup is a layout container for Button/IconButton children
      // Arranges children horizontally with optional connected styling
      
      // Use buttongroup size for gap (spec values)
      int btn_size = (n->buttongroup.size >= 0 && n->buttongroup.size <= 4) ? n->buttongroup.size : 2;
      int buttongroup_gaps[] = {12, 12, 8, 8, 8};      // XS, S, M, L, XL (unconnected)
      int size_gap = buttongroup_gaps[btn_size];
      
      // Use size-based gap (ButtonGroup controls child spacing)
      int pad = size_gap;
      
      int avail_w = n->w - n->padding_left - n->padding_right;
      int avail_h = n->h - n->padding_top - n->padding_bottom;
      if (avail_w < 0) avail_w = 0;
      if (avail_h < 0) avail_h = 0;
      
      size_t count = n->len;
      if (count == 0) return;
      
      // Calculate intrinsic sizes for all children
      int* intrinsic_w = (int*)calloc(count, sizeof(int));
      int* intrinsic_h = (int*)calloc(count, sizeof(int));
      int* assigned_w = (int*)calloc(count, sizeof(int));
      if (!intrinsic_w || !intrinsic_h || !assigned_w) {
        free(intrinsic_w);
        free(intrinsic_h);
        free(assigned_w);
        return;
      }
      
      int total_margins = 0;
      for (size_t i = 0; i < count; i++) {
        CogitoNode* c = n->children[i];
        
        // Propagate ButtonGroup size to children (Button/IconButton)
        if (c->kind == COGITO_BUTTON || c->kind == COGITO_ICONBTN) {
          c->button.size = n->buttongroup.size;
        }
        
        int cw = 0, ch = 0;
        cogito_intrinsic_size(c, &cw, &ch);
        if (cw < 0) cw = 0;
        if (ch < 0) ch = 0;
        intrinsic_w[i] = cw;
        intrinsic_h[i] = ch;
        assigned_w[i] = cw;
        total_margins += c->margin_left + c->margin_right;
      }
      
      // For connected mode, use smaller gap; otherwise use standard gap
      bool is_connected = (n->buttongroup.connected == 1);
      int actual_gap = is_connected ? 2 : pad;
      
      int total_gaps = (count > 1) ? (int)(count - 1) * actual_gap : 0;
      int avail_content_w = avail_w - total_margins - total_gaps;
      if (avail_content_w < 0) avail_content_w = 0;
      
      // Distribute widths - buttons can expand if they have hexpand set
      bool* expand = (bool*)calloc(count, sizeof(bool));
      if (expand) {
        for (size_t i = 0; i < count; i++) {
          expand[i] = n->children[i]->hexpand;
        }
        cogito_distribute_axis_sizes(count, intrinsic_w, expand, avail_content_w, assigned_w);
        free(expand);
      }
      
      // ---- Press animation: expand pressed button by 15%, shrink adjacent ----
      // NOTE: Uses child's spring_w for width multiplier (1.0-1.15). This is a semantic
      // change from typical pixel-width animation (e.g., tabs.inc uses spring_w for pixels).
      // If a Button/IconButton is used in multiple animated contexts, consider a dedicated field.
      // IMPORTANT: Spring is ticked during draw phase (see buttongroup draw), not here.
      // Layout just reads the current spring value.
      
      // Read current spring values for width multipliers
      double* width_mult = (double*)calloc(count, sizeof(double));
      if (width_mult) {
        for (size_t i = 0; i < count; i++) {
          CogitoNode* c = n->children[i];
          // Use current spring value (ticked during draw phase)
          width_mult[i] = c->spring_w.inited ? c->spring_w.x : 1.0;
        }
        
        // Apply width multipliers
        for (size_t i = 0; i < count; i++) {
          assigned_w[i] = (int)(assigned_w[i] * width_mult[i] + 0.5);
        }
        free(width_mult);
      }
      
      // Layout children horizontally
      int cx = x + n->padding_left;
      for (size_t i = 0; i < count; i++) {
        CogitoNode* c = n->children[i];
        int cw = assigned_w[i];
        int ch = intrinsic_h[i];
        int slot_h = avail_h - c->margin_top - c->margin_bottom;
        if (slot_h < 0) slot_h = 0;
        
        // Vertical centering
        if (c->vexpand) {
          ch = slot_h;
        } else if (ch > slot_h) {
          ch = slot_h;
        }
        
        int cy = y + n->padding_top + c->margin_top;
        int oy = (slot_h - ch) / 2;
        if (oy < 0) oy = 0;
        
        cogito_layout_node(c, cx + c->margin_left, cy + oy, cw, ch);
        cx += cw + c->margin_left + c->margin_right;
        if (i < count - 1) cx += actual_gap;
      }
      
      free(intrinsic_w);
      free(intrinsic_h);
      free(assigned_w);
      break;
    }