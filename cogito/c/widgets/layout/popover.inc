case COGITO_POPOVER: {
      // Popover positions itself relative to anchor
      CogitoNode* win = cogito_node_window(n);
      if (win && win->popover_anchor) {
        CogitoNode* anchor = win->popover_anchor;
        // Position popover below anchor with slight offset
        int pop_x = anchor->x;
        int pop_y = anchor->y + anchor->h + 8;

        // Keep popover within window bounds
        if (pop_x + n->w > win->x + win->w) {
          pop_x = win->x + win->w - n->w - 8;
        }
        if (pop_x < win->x + 8) {
          pop_x = win->x + 8;
        }
        if (pop_y + n->h > win->y + win->h) {
          // Position above anchor if no room below
          pop_y = anchor->y - n->h - 8;
        }

        if (n->x != pop_x || n->y != pop_y) {
          n->x = pop_x;
          n->y = pop_y;
          cogito_layout_mark_changed();
        }
        cogito_layout_vstack(n, pop_x, pop_y);
      } else {
        if (n->x != x || n->y != y) {
          n->x = x;
          n->y = y;
          cogito_layout_mark_changed();
        }
        cogito_layout_vstack(n, x, y);
      }
      break;
    }
