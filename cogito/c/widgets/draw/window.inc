case COGITO_WINDOW:
      for (size_t i = 0; i < n->len; i++) {
        cogito_draw_node(n->children[i]);
      }
      if (n->kind == COGITO_WINDOW && n->dialog) {
        if (n->dialog->kind == COGITO_DIALOG_SLOT) {
          cogito_draw_node(n->dialog);
        } else if (n->dialog_open) {
          DrawRectangle(0, 0, n->w, n->h, cogito_rgba(0, 0, 0, 82));
          cogito_draw_node(n->dialog);
        }
      }
      // Draw popover on top if open
      if (n->kind == COGITO_WINDOW && n->popover_open && n->popover) {
        // Lighter scrim for popovers
        DrawRectangle(0, 0, n->w, n->h, cogito_rgba(0, 0, 0, 32));
        cogito_draw_node(n->popover);
      }
      break;
    
