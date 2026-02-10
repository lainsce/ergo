case COGITO_FAB: {
      CogitoNode* p = n->parent;
      int fw = 0, fh = 0;
      cogito_intrinsic_size(n, &fw, &fh);
      if (fw > 0) n->w = fw;
      if (fh > 0) n->h = fh;
      if (!p || n->w <= 0 || n->h <= 0) break;

      if (p->kind == COGITO_NAV_RAIL) {
        n->x = p->x + (p->w - n->w) / 2 + n->margin_left - n->margin_right;
        n->y = p->y + 21 + n->margin_top;
        break;
      }
      if (p->kind == COGITO_BOTTOM_NAV) {
        n->x = p->x + p->w - 18 - n->w - n->margin_right;
        n->y = p->y + (p->h - n->h) / 2 + n->margin_top - n->margin_bottom;
        break;
      }

      int col = n->align % 3;
      if (col < 0 || col > 2) col = 2;  // FAB defaults to right lane.
      if (col == 0) {
        n->x = p->x + 18 + n->margin_left;
      } else if (col == 1) {
        n->x = p->x + (p->w - n->w) / 2 + n->margin_left - n->margin_right;
      } else {
        n->x = p->x + p->w - 18 - n->w - n->margin_right;
      }
      n->y = p->y + p->h - 18 - n->h - n->margin_bottom;
      break;
    }
