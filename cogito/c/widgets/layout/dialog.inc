case COGITO_DIALOG: {
      CogitoStyle s = cogito_theme_resolve_node(n);
      int pad_l = n->padding_left > 0 ? n->padding_left : (s.has_padding_left ? s.padding_left : 24);
      int pad_t = n->padding_top > 0 ? n->padding_top : (s.has_padding_top ? s.padding_top : 24);
      int pad_r = n->padding_right > 0 ? n->padding_right : (s.has_padding_right ? s.padding_right : 24);
      int pad_b = n->padding_bottom > 0 ? n->padding_bottom : (s.has_padding_bottom ? s.padding_bottom : 24);
      int title_h = cogito_text_height_size(20);
      int cx = 0, cy = 0, cw = 0, ch = 0;
      cogito_dialog_close_rect(n, &cx, &cy, &cw, &ch);
      int header_h = title_h > ch ? title_h : ch;
      int header_gap = 12;
      int old_pad_l = n->padding_left;
      int old_pad_t = n->padding_top;
      int old_pad_r = n->padding_right;
      int old_pad_b = n->padding_bottom;
      n->padding_left = pad_l;
      n->padding_top = pad_t + header_h + header_gap;
      n->padding_right = pad_r;
      n->padding_bottom = pad_b;
      cogito_layout_vstack(n, x, y);
      n->padding_left = old_pad_l;
      n->padding_top = old_pad_t;
      n->padding_right = old_pad_r;
      n->padding_bottom = old_pad_b;
      break;
    }
    
