case COGITO_COLORPICKER: {
      CogitoStyle cs = cogito_theme_resolve_node(n);
      int cp_size = cogito_node_font_size(n);
      int cp_pad = cs.has_padding_top ? cs.padding_top : 12;
      int cp_row_h = cp_size + 10;
      int cp_gap = 8;
      h = cp_pad * 2 + cp_row_h * 4 + cp_gap * 3;
      w = 240;
      break;
    }

