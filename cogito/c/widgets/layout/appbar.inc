case COGITO_APPBAR: {
      int btn_size = 12;
      int btn_gap = 6;
      int btn_pad_top = 9;
      int btn_pad_right = 9;
      cogito_appbar_button_metrics(&btn_size, &btn_gap, &btn_pad_top, &btn_pad_right);
      int buttons_y = y + btn_pad_top;
      n->appbar_btn_size = btn_size;
      n->appbar_btn_y = buttons_y;
      CogitoNode* win = cogito_node_window(n);
      bool allow_max = !win || win->resizable;
      // Only show traffic lights on the first (leftmost) appbar
      if (cogito_is_first_appbar(win, n)) {
        cogito_appbar_controls_positions(n, x, w, btn_size, btn_gap, btn_pad_right, allow_max,
                                         &n->appbar_btn_close_x, &n->appbar_btn_min_x, &n->appbar_btn_max_x);
      } else {
        // Other appbars don't show traffic lights
        n->appbar_btn_close_x = -1;
        n->appbar_btn_min_x = -1;
        n->appbar_btn_max_x = -1;
      }
      int title_y = 0;
      int subtitle_y = 0;
      int child_y = 0;
      int appbar_h = 0;
      cogito_appbar_row_positions(n, &buttons_y, &title_y, &subtitle_y, &child_y, &appbar_h);
      n->appbar_btn_y = buttons_y;
      n->appbar_title_y = title_y;
      n->appbar_subtitle_y = subtitle_y;
      int icon_row_h = appbar_h - (child_y - y) - 6;
      if (icon_row_h < btn_size) icon_row_h = btn_size;
      int cx = x + w - 12;
      for (size_t i = 0; i < n->len; i++) {
        CogitoNode* c = n->children[i];
        int cw = 0;
        int ch = 0;
        cogito_intrinsic_size(c, &cw, &ch);
        cx -= cw;
        int cy = child_y + (icon_row_h - ch) / 2;
        cogito_layout_node(c, cx, cy, cw, ch);
        cx -= btn_gap;
      }
      break;
    }
    
