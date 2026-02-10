case COGITO_TOASTS:
      if (!cogito_toasts_force_draw) {
        CogitoNode* win = cogito_node_window(n);
        if (win) break;
      }
      for (size_t i = 0; i < n->len; i++) {
        cogito_draw_node(n->children[i]);
      }
      break;
