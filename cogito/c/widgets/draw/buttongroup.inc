case COGITO_BUTTON_GROUP: {
      if (n->len == 0) break;
      for (size_t i = 0; i < n->len; i++) {
        cogito_draw_node(n->children[i]);
      }
      break;
    }
