case COGITO_TOOLBAR: {
      const int max_slots = 6;
      const int docked_gap = 32;
      const int floating_gap = 8;
      const int floating_compact_gap = 4;
      const int outer_pad = 16;
      const int compact_side_pad = 8;

      const char* cls = (n->class_name && n->class_name->data) ? n->class_name->data : NULL;
      bool floating = cls && strstr(cls, "floating") != NULL;
      bool compact = floating && cls && strstr(cls, "compact") != NULL;

      int slots = (int)n->len;
      if (slots > max_slots) slots = max_slots;
      if (slots < 0) slots = 0;

      int gap = floating ? floating_gap : docked_gap;
      if (compact) gap = floating_compact_gap;
      int side = compact ? compact_side_pad : outer_pad;

      int max_item_w = 0;
      int sum_item_w = 0;
      for (int i = 0; i < slots; i++) {
        CogitoNode* c = n->children[i];
        if (!c) continue;
        int cw = 0;
        int ch = 0;
        cogito_intrinsic_size(c, &cw, &ch);
        if (cw < 1) cw = 1;
        if (c->kind == COGITO_BUTTON && !c->hexpand) {
          int font_px = cogito_node_font_size(c);
          int text_w = 0;
          if (c->text && c->text->data && c->text->data[0]) {
            text_w = cogito_text_width_size_node(c, c->text->data, font_px);
          }
          int pad_x = c->padding_left + c->padding_right;
          if (pad_x <= 0) pad_x = 24;
          int compact_w = text_w + pad_x;
          int min_tap = ch > 0 ? ch : 40;
          if (min_tap < 32) min_tap = 32;
          if (compact_w < min_tap) compact_w = min_tap;
          cw = compact_w;
        }
        if (cw > max_item_w) max_item_w = cw;
        sum_item_w += cw;
      }

      if (max_item_w < 48) max_item_w = 48;
      h = 64;

      if (compact) {
        if (slots <= 0) {
          w = side * 2;
        } else {
          int total_gap = (slots > 1) ? gap * (slots - 1) : 0;
          w = side * 2 + sum_item_w + total_gap;
        }
      } else {
        int fixed_slots = max_slots;
        int total_gap = (fixed_slots > 1) ? gap * (fixed_slots - 1) : 0;
        w = side * 2 + (max_item_w * fixed_slots) + total_gap;
      }
      break;
    }
    
