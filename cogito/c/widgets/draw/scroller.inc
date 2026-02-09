case COGITO_SCROLLER: {
      if (n->len > 0) {
        // TODO: Replace with backend scissor API
        // BeginScissorMode(n->x, n->y, n->w, n->h);
        if (cogito_backend && cogito_backend->begin_scissor) {
          cogito_backend->begin_scissor(n->x, n->y, n->w, n->h);
        }
        for (size_t i = 0; i < n->len; i++) {
          cogito_draw_node(n->children[i]);
        }
        // EndScissorMode();
        if (cogito_backend && cogito_backend->end_scissor) {
          cogito_backend->end_scissor();
        }
      }
      break;
    }
