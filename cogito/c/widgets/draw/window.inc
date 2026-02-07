case COGITO_WINDOW:
      for (size_t i = 0; i < n->len; i++) {
        cogito_draw_node(n->children[i]);
      }
      if (n->kind == COGITO_WINDOW && n->dialog) {
        if (n->dialog->kind == COGITO_DIALOG_SLOT) {
          cogito_draw_node(n->dialog);
        } else if (n->dialog_open) {
          bool hover = false, active = false;
          cogito_node_hover_state(n, &hover, &active);
          CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
          Color scrim = s.has_bg ? s.bg : n->bg;
          DrawRectangle(0, 0, n->w, n->h, scrim);
          cogito_draw_node(n->dialog);
        }
      }
      // Draw popover on top if open
      if (n->kind == COGITO_WINDOW && n->popover_open && n->popover) {
        cogito_draw_node(n->popover);
      }
      break;
