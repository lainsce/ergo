case COGITO_DIALOG_SLOT:
      if (n->dialog && n->dialog_open) {
        CogitoNode* win = cogito_node_window(n);
        if (win) {
          DrawRectangle(0, 0, win->w, win->h, cogito_rgba(0, 0, 0, 82));
        } else {
          DrawRectangle(n->x, n->y, n->w, n->h, cogito_rgba(0, 0, 0, 82));
        }
        cogito_draw_node(n->dialog);
      }
      break;
    
