case COGITO_BUTTON_GROUP: {
      bool hover = false, active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);

      CogitoColor bg = s.has_bg ? s.bg : n->bg;
      CogitoColor text_col = s.has_text ? s.text : n->text_color;
      int radius = s.has_radius ? s.radius : (n->h / 2);
      int size = cogito_node_font_size(n);
      const int gap = 2;
      int inner_radius = 4;

      // Animated colors
      cogito_anim_apply(n, &s, &bg, &text_col, NULL, NULL);

      if (n->len == 0) break;

      int count = (int)n->len;

      int sel = n->selected;
      if (sel < 0) sel = 0;
      if (sel >= count) sel = count - 1;

      // Selection color from theme
      CogitoColor sel_bg = s.has_selection ? s.selection : n->selection_color;
      CogitoColor sel_text = cogito_on_color(sel_bg);

      int mx = 0;
      int my = 0;
      if (hover && cogito_backend && cogito_backend->get_mouse_position) {
        cogito_backend->get_mouse_position(&mx, &my);
      }

      // Draw segment labels
      for (int i = 0; i < count; i++) {
        int slot_l = n->x + (n->w * i) / count;
        int slot_r = n->x + (n->w * (i + 1)) / count;
        int sw = slot_r - slot_l;
        if (sw < 1) sw = 1;
        int inset_l = (i == 0) ? 0 : (gap / 2);
        int inset_r = (i == count - 1) ? 0 : (gap - (gap / 2));
        int sx = slot_l + inset_l;
        sw -= (inset_l + inset_r);
        if (sw < 1) {
          sx = slot_l;
          sw = 1;
        }
        int local_outer = radius;
        if (local_outer > n->h / 2) local_outer = n->h / 2;
        if (local_outer > sw / 2) local_outer = sw / 2;
        int local_inner = inner_radius;
        if (local_inner > local_outer) local_inner = local_outer;

        int rtl = local_inner;
        int rtr = local_inner;
        int rbr = local_inner;
        int rbl = local_inner;
        if (count == 1) {
          rtl = rtr = rbr = rbl = local_outer;
        } else if (i == 0) {
          rtl = local_outer;
          rtr = local_inner;
          rbr = local_inner;
          rbl = local_outer;
        } else if (i == count - 1) {
          rtl = local_inner;
          rtr = local_outer;
          rbr = local_outer;
          rbl = local_inner;
        }

        bool is_sel = (i == sel);
        bool is_hovered = (!is_sel && hover &&
                           mx >= sx && mx < sx + sw &&
                           my >= n->y && my < n->y + n->h);
        CogitoColor seg_bg = is_sel ? sel_bg : bg;
        cogito_draw_rect_radii_aa(sx, n->y, sw, n->h, seg_bg, rtl, rtr, rbr, rbl);
        if (is_hovered) {
          // Draw hover as a smooth overlay so edge segments don't show stitched corners.
          CogitoColor hover_col = text_col;
          hover_col.a = (uint8_t)(hover_col.a * 0.16f);
          int hover_radius = local_inner;
          if (count == 1 || i == 0 || i == count - 1) {
            hover_radius = local_outer;
          }
          cogito_draw_rect(sx, n->y, sw, n->h, hover_col, hover_radius);
        }

        CogitoNode* child = n->children[i];
        const char* label = child->text ? child->text->data : "";
        int label_w = cogito_text_width_size_node(n, label, size);
        int label_h = cogito_text_height_size(size);
        int lx = sx + (sw - label_w) / 2;
        int ly = n->y + (n->h - label_h) / 2;
        CogitoColor lc = is_sel ? sel_text : text_col;
        cogito_draw_text_size_node(n, lx, ly, label, lc, size, is_sel || is_hovered);
      }
      break;
    }
