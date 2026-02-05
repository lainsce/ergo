case COGITO_TABS: {
      int size = cogito_node_font_size(n);
      int th = cogito_text_height_size(size);
      int pad_x = 12;
      int pad_y = 8;
      int gap = 6;
      int tw = 0;
      if (n->tab_len == 0) {
        w = 160;
        h = th + pad_y * 2;
        break;
      }
      for (size_t i = 0; i < n->tab_len; i++) {
        const char* t = n->tab_labels[i] ? n->tab_labels[i]->data : "";
        int lw = cogito_text_width_size_node(n, t, size);
        tw += lw + pad_x * 2;
        if (i + 1 < n->tab_len) tw += gap;
      }
      w = tw;
      h = th + pad_y * 2;
      break;
    }
    
