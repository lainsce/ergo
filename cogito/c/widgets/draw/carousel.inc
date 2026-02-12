case COGITO_CAROUSEL: {
      if (n->len > 0) {
        // Apply scissor clipping for carousel content
        if (cogito_backend && cogito_backend->begin_scissor) {
          cogito_backend->begin_scissor(n->x, n->y, n->w, n->h);
        }
        for (size_t i = 0; i < n->len; i++) {
          cogito_draw_node(n->children[i]);
        }
        if (cogito_backend && cogito_backend->end_scissor) {
          cogito_backend->end_scissor();
        }
      }
      break;
    }
