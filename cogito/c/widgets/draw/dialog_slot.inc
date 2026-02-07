case COGITO_DIALOG_SLOT:
      if (n->dialog && n->dialog_open) {
        CogitoNode* win = cogito_node_window(n);
        CogitoStyle s = cogito_theme_resolve_node(n);
        Color scrim = s.has_bg ? s.bg : n->bg;
        if (win) {
          DrawRectangle(0, 0, win->w, win->h, scrim);
        } else {
          DrawRectangle(n->x, n->y, n->w, n->h, scrim);
        }
        cogito_draw_node(n->dialog);
      }
      break;
