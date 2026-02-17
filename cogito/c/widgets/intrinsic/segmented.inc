case COGITO_SEGMENTED: {
      int size = cogito_node_font_size(n);
      const int gap = 2;
      int total_w = 0;
      for (size_t i = 0; i < n->len; i++) {
        CogitoNode* child = n->children[i];
        const char* label = child->text ? child->text->data : "";
        int lw = cogito_text_width_size_node(n, label, size);
        total_w += lw + 32;
        if (i + 1 < n->len) total_w += gap;
      }
      if (total_w < 200) total_w = 200;
      w = total_w;
      h = text_h + 16;
      if (h < 56) h = 56;
      break;
    }
