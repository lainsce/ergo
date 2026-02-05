case COGITO_TOAST: {
      int size = cogito_node_font_size(n);
      int th = cogito_text_height_size(size);
      int pad_x = 12;
      int pad_y = 8;
      const char* t = n->text ? n->text->data : "";
      int tw = cogito_text_width_size_node(n, t, size);
      CogitoStyle s = cogito_theme_resolve_node(n);
      int icon = s.has_icon_size ? s.icon_size : 12;
      int close_box = icon + 8;
      w = tw + pad_x * 2 + close_box;
      h = th + pad_y * 2;
      break;
    }
    
