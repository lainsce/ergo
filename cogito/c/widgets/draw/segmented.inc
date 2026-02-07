case COGITO_SEGMENTED: {
      bool hover = false, active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);

      Color bg = s.has_bg ? s.bg : n->bg;
      Color text_col = s.has_text ? s.text : n->text_color;
      Color border_col = s.has_border ? s.border : cogito_rgba(200, 200, 200, 255);
      int border_w = s.has_border_width ? s.border_width : 1;
      int radius = s.has_radius ? s.radius : (n->h / 2);
      int size = cogito_node_font_size(n);

      // Animated colors
      cogito_anim_apply(n, &s, &bg, &text_col, &border_col, NULL);

      // Draw outer capsule border
      if (bg.a > 0) {
        cogito_draw_rect(n->x, n->y, n->w, n->h, bg, radius);
      }
      if (border_w > 0) {
        cogito_draw_rect_lines(n->x, n->y, n->w, n->h, border_col, radius, border_w);
      }

      if (n->len == 0) break;

      // Calculate segment widths
      int seg_w = n->w / (int)n->len;
      int sel = n->selected;
      if (sel < 0) sel = 0;
      if (sel >= (int)n->len) sel = (int)n->len - 1;

      // Selection color from theme
      Color sel_bg = s.has_selection ? s.selection : cogito_rgba(103, 80, 164, 255);
      Color sel_text = cogito_on_color(sel_bg);

      // Draw selected segment background (pill shape)
      {
        int sel_x = n->x + sel * seg_w;
        int sel_w = (sel == (int)n->len - 1) ? (n->w - sel * seg_w) : seg_w;
        int inset = border_w;
        cogito_draw_rect(sel_x + inset, n->y + inset, sel_w - inset * 2, n->h - inset * 2, sel_bg, radius > inset ? radius - inset : radius);
      }

      // Draw dividers between unselected segments
      for (int i = 1; i < (int)n->len; i++) {
        if (i == sel || i == sel + 1) continue;
        int div_x = n->x + i * seg_w;
        Color divider = border_col;
        divider.a = (unsigned char)(divider.a * 0.4f);
        DrawLine(div_x, n->y + 8, div_x, n->y + n->h - 8, divider);
      }

      // Draw segment labels
      for (int i = 0; i < (int)n->len; i++) {
        CogitoNode* child = n->children[i];
        const char* label = child->text ? child->text->data : "";
        int label_w = cogito_text_width_size_node(n, label, size);
        int label_h = cogito_text_height_size(size);
        int sx = n->x + i * seg_w;
        int sw = (i == (int)n->len - 1) ? (n->w - i * seg_w) : seg_w;
        int lx = sx + (sw - label_w) / 2;
        int ly = n->y + (n->h - label_h) / 2;

        bool is_sel = (i == sel);
        Color lc = is_sel ? sel_text : text_col;

        // Hover highlight on non-selected segments
        if (!is_sel && hover) {
          int hmx = GetMouseX();
          if (hmx >= sx && hmx < sx + sw) {
            Color hl = text_col;
            hl.a = 15;
            int inset = border_w;
            int seg_radius = (i == 0 || i == (int)n->len - 1) ? (radius > inset ? radius - inset : radius) : 0;
            cogito_draw_rect(sx + inset, n->y + inset, sw - inset * 2, n->h - inset * 2, hl, seg_radius);
          }
        }

        // Draw checkmark icon for selected segment
        if (is_sel) {
          int check_size = 10;
          int check_x = lx - check_size - 4;
          int check_cy = ly + label_h / 2;
          if (check_x > sx + 4) {
            DrawLineEx((Vector2){(float)check_x, (float)check_cy},
                       (Vector2){(float)(check_x + check_size * 3 / 8), (float)(check_cy + check_size * 3 / 8)}, 1.5f, lc);
            DrawLineEx((Vector2){(float)(check_x + check_size * 3 / 8), (float)(check_cy + check_size * 3 / 8)},
                       (Vector2){(float)(check_x + check_size), (float)(check_cy - check_size * 3 / 8)}, 1.5f, lc);
          }
        }

        cogito_draw_text_size_node(n, lx, ly, label, lc, size, false);
      }
      break;
    }

