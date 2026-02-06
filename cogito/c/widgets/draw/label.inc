case COGITO_LABEL: {
      if (n->text) {
        bool hover = false;
        bool active = false;
        cogito_node_hover_state(n, &hover, &active);
        int size = cogito_label_font_size(n);
        Color text = n->text_color;
        CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
        if (cogito_has_selection_text) text = cogito_selection_text;
        else if (s.has_text) text = s.text;
        int align = n->text_align;
        if (n->text_wrap) {
          cogito_draw_text_wrapped(n, n->x, n->y, n->w, n->text->data, text, size, cogito_label_bold(n), align, true);
        } else if (n->text_ellipsis) {
          int th = cogito_text_height_size(size);
          int ty = n->y + (n->h - th) / 2;
          cogito_draw_text_ellipsis(n, n->x, ty, n->w, n->text->data, text, size, cogito_label_bold(n), align);
        } else {
          int tw = cogito_text_width_size_node(n, n->text->data, size);
          int th = cogito_text_height_size(size);
          int ty = n->y + (n->h - th) / 2;
          int tx = n->x;
          if (align == 1) tx = n->x + (n->w - tw) / 2;
          else if (align == 2) tx = n->x + (n->w - tw);
          cogito_draw_text_size_node(n, tx, ty, n->text->data, text, size, cogito_label_bold(n));
        }
      }
      break;
    }
    
