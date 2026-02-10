case COGITO_DIALOG:
      w = 320;
      h = 200;
      {
        CogitoStyle ds = cogito_theme_resolve_node(n);
        int pad_l = n->padding_left > 0 ? n->padding_left : (ds.has_padding_left ? ds.padding_left : 24);
        int pad_t = n->padding_top > 0 ? n->padding_top : (ds.has_padding_top ? ds.padding_top : 24);
        int pad_r = n->padding_right > 0 ? n->padding_right : (ds.has_padding_right ? ds.padding_right : 24);
        int pad_b = n->padding_bottom > 0 ? n->padding_bottom : (ds.has_padding_bottom ? ds.padding_bottom : 24);
        int title_size = 20;
        int title_h = cogito_text_height_size(title_size);
        int title_w = (n->text && n->text->data && n->text->data[0]) ? cogito_text_width_size_node(n, n->text->data, title_size) : 0;
        int icon = ds.has_icon_size ? ds.icon_size : 14;
        if (icon < 10) icon = 10;
        int close_box = icon + 10;
        int header_h = title_h > close_box ? title_h : close_box;
        int header_gap = 12;
        int cw = 0;
        int ch = 0;
        for (size_t i = 0; i < n->len; i++) {
          int iw = 0;
          int ih = 0;
          cogito_intrinsic_size(n->children[i], &iw, &ih);
          int tw = iw + n->children[i]->margin_left + n->children[i]->margin_right;
          int th = ih + n->children[i]->margin_top + n->children[i]->margin_bottom;
          if (tw > cw) cw = tw;
          ch += th;
          if (i + 1 < n->len) ch += n->gap;
        }
        int min_header_w = title_w + close_box + 12;
        if (cw < min_header_w) cw = min_header_w;
        w = pad_l + cw + pad_r;
        h = pad_t + header_h + header_gap + ch + pad_b;
        if (w < 320) w = 320;
        if (h < 200) h = 200;
      }
      break;
    
