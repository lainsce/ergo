case COGITO_COLORPICKER: {
      CogitoStyle cs = cogito_theme_resolve_node(n);
      int cp_size = cogito_node_font_size(n);
      int cp_pad_t = cs.has_padding_top ? cs.padding_top : 12;
      int cp_pad_b = cs.has_padding_bottom ? cs.padding_bottom : cp_pad_t;
      int cp_row_h = cp_size + 10;
      int cp_gap = 8;
      h = cp_pad_t + cp_row_h * 4 + cp_gap * 3 + cp_pad_b;
      if (h < 140) h = 140;
      w = 240;
      break;
    }
