case COGITO_SCROLLER: {
      if (n->len > 0) {
        BeginScissorMode(n->x, n->y, n->w, n->h);
        for (size_t i = 0; i < n->len; i++) {
          cogito_draw_node(n->children[i]);
        }
        EndScissorMode();
      }
      break;
    }
    
