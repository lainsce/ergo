case COGITO_DATEPICKER: {
      CogitoStyle s = cogito_theme_resolve_node(n);
      int pad_l = s.has_padding_left ? s.padding_left : 10;
      int pad_t = s.has_padding_top ? s.padding_top : 10;
      int pad_r = s.has_padding_right ? s.padding_right : pad_l;
      int pad_b = s.has_padding_bottom ? s.padding_bottom : pad_t;
      int size = cogito_node_font_size(n);
      int th = cogito_text_height_size(size);
      int header_h = th + 8;
      int cell_h = th + 10;
      if (cell_h < 28) cell_h = 28;
      w = 240;
      h = pad_t + header_h + 6 + cell_h * 6 + pad_b;
      if (h < 260) h = 260;
      if (w < pad_l + pad_r + 7 * 24) w = pad_l + pad_r + 7 * 24;
      break;
    }
