case COGITO_DIALOG: {
      CogitoStyle s = cogito_theme_resolve_node(n);
      int pad_l = 24;
      int pad_t = 24;
      int pad_r = 24;
      int pad_b = 24;
      if (n->padding_set) {
        pad_l = n->padding_left;
        pad_t = n->padding_top;
        pad_r = n->padding_right;
        pad_b = n->padding_bottom;
      } else {
        if (s.has_padding) {
          pad_l = s.padding_left;
          pad_t = s.padding_top;
          pad_r = s.padding_right;
          pad_b = s.padding_bottom;
        }
        if (s.has_padding_left) pad_l = s.padding_left;
        if (s.has_padding_top) pad_t = s.padding_top;
        if (s.has_padding_right) pad_r = s.padding_right;
        if (s.has_padding_bottom) pad_b = s.padding_bottom;
      }
      int title_h = cogito_text_height_size(20);
      int cx = 0, cy = 0, cw = 0, ch = 0;
      cogito_dialog_close_rect(n, &cx, &cy, &cw, &ch);
      int header_h = title_h > ch ? title_h : ch;
      int header_gap = 16;
      int section_gap = 24;
      int content_top = pad_t + header_h + header_gap;
      int avail_w = n->w - pad_l - pad_r;
      int avail_h = n->h - content_top - pad_b;
      if (avail_w < 0) avail_w = 0;
      if (avail_h < 0) avail_h = 0;

      size_t count = n->len;
      if (count == 0) break;
      int* intrinsic_w = (int*)calloc(count, sizeof(int));
      int* intrinsic_h = (int*)calloc(count, sizeof(int));
      int* assigned_h = (int*)calloc(count, sizeof(int));
      bool* expand = (bool*)calloc(count, sizeof(bool));
      if (!intrinsic_w || !intrinsic_h || !assigned_h || !expand) {
        free(intrinsic_w);
        free(intrinsic_h);
        free(assigned_h);
        free(expand);
        break;
      }

      int total_margins = 0;
      for (size_t i = 0; i < count; i++) {
        CogitoNode* c = n->children[i];
        int iw = 0;
        int ih = 0;
        cogito_intrinsic_size(c, &iw, &ih);
        if (iw < 0) iw = 0;
        if (ih < 0) ih = 0;
        intrinsic_w[i] = iw;
        intrinsic_h[i] = ih;
        assigned_h[i] = ih;
        expand[i] = c->vexpand;
        total_margins += c->margin_top + c->margin_bottom;
      }

      int total_gaps = (count > 1) ? (int)(count - 1) * section_gap : 0;
      int avail_content_h = avail_h - total_margins - total_gaps;
      if (avail_content_h < 0) avail_content_h = 0;
      cogito_distribute_axis_sizes(count, intrinsic_h, expand, avail_content_h, assigned_h);

      int child_y = y + content_top;
      for (size_t i = 0; i < count; i++) {
        CogitoNode* c = n->children[i];
        int child_w = intrinsic_w[i];
        int child_h = assigned_h[i];
        int slot_w = avail_w - c->margin_left - c->margin_right;
        if (slot_w < 0) slot_w = 0;

        bool is_chrome = (c->kind == COGITO_APPBAR || c->kind == COGITO_TOOLBAR ||
                          c->kind == COGITO_BOTTOM_NAV);
        if (c->hexpand || is_chrome) {
          child_w = slot_w;
          if (c->kind == COGITO_LABEL && c->text_wrap && !c->vexpand) {
            child_h = cogito_text_wrap_height(c, c->text ? c->text->data : "", cogito_node_font_size(c), slot_w);
            if (child_h <= 0) child_h = cogito_text_height_size(cogito_node_font_size(c));
          }
        } else if (child_w > slot_w) {
          child_w = slot_w;
        }

        int child_col = c->align % 3;
        if (child_col < 0 || child_col > 2) child_col = 0;
        int child_x = x + pad_l;
        int ox = 0;
        if (child_col == 1) ox = (slot_w - child_w) / 2;
        else if (child_col == 2) ox = slot_w - child_w;
        if (ox < 0) ox = 0;

        cogito_layout_node(c, child_x + ox + c->margin_left, child_y + c->margin_top, child_w, child_h);
        child_y += child_h + c->margin_top + c->margin_bottom;
        if (i + 1 < count) child_y += section_gap;
      }

      free(intrinsic_w);
      free(intrinsic_h);
      free(assigned_h);
      free(expand);
      break;
    }
    
