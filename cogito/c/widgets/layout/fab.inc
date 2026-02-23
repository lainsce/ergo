case COGITO_FAB: {
      CogitoNode* p = n->parent;
      int fw = 0, fh = 0;
      cogito_intrinsic_size(n, &fw, &fh);
      if (fw > 0) n->w = fw;
      if (fh > 0) n->h = fh;
      if (!p || n->w <= 0 || n->h <= 0) break;

      if (p->kind == COGITO_NAV_RAIL) {
        int rail_w = p->w > 0 ? p->w : COGITO_NAV_RAIL_COLLAPSED_WIDTH;
        int top_offset = p->nav_rail.expanded
          ? COGITO_NAV_RAIL_EXPANDED_TOP_OFFSET
          : COGITO_NAV_RAIL_COLLAPSED_TOP_OFFSET;
        int section_gap = p->nav_rail.expanded
          ? COGITO_NAV_RAIL_EXPANDED_SECTION_GAP
          : COGITO_NAV_RAIL_COLLAPSED_SECTION_GAP;
        if (p->nav_rail.expanded) {
          int indicator_w = rail_w - (COGITO_NAV_RAIL_EXPANDED_SIDE_PADDING * 2);
          if (indicator_w < COGITO_NAV_RAIL_EXPANDED_INDICATOR_MIN_WIDTH) {
            indicator_w = COGITO_NAV_RAIL_EXPANDED_INDICATOR_MIN_WIDTH;
          }
          if (n->w > indicator_w) n->w = indicator_w;
          int bg_x = p->x + (rail_w - indicator_w) / 2;
          // Keep FAB on the same lane start as destination indicators.
          // FAB's internal left padding then aligns icon with destination icons.
          n->x = bg_x + n->margin_left - n->margin_right;
        } else {
          n->x = p->x + (rail_w - n->w) / 2 + n->margin_left - n->margin_right;
        }
        int y = p->y + p->padding_top + top_offset + n->margin_top;
        if (p->nav_rail.show_toggle) {
          y += COGITO_NAV_RAIL_SHARED_TOGGLE_BUTTON_SIZE + section_gap;
        }
        n->y = y;
        break;
      }
      if (p->kind == COGITO_BOTTOM_NAV) {
        n->x = p->x + p->w - 18 - n->w - n->margin_right;
        n->y = p->y + (p->h - n->h) / 2 + n->margin_top - n->margin_bottom;
        break;
      }

      // Position FAB relative to its parent container, floating at the bottom.
      CogitoNode* ctr = p;
      if (!ctr || ctr->w <= 0 || ctr->h <= 0) {
        ctr = cogito_node_window(n);
      }
      if (!ctr) break;

      // Check for a sibling floating FAB already laid out in this container.
      // If found, this becomes the secondary (S-size) FAB to its left.
      CogitoNode* primary = cogito_find_other_floating_fab(ctr, n);
      if (primary && primary->y > 0) {
        // Force S size (42x42) for the secondary FAB
        n->w = 42;
        n->h = 42;
        n->y = ctr->y + ctr->h - 18 - n->h - n->margin_bottom;
        n->x = primary->x - 8 - n->w;
        break;
      }

      int col = n->align % 3;
      if (col < 0 || col > 2) col = 2;  // FAB defaults to right lane.
      if (col == 0) {
        n->x = ctr->x + 18 + n->margin_left;
      } else if (col == 1) {
        n->x = ctr->x + (ctr->w - n->w) / 2 + n->margin_left - n->margin_right;
      } else {
        n->x = ctr->x + ctr->w - 16 - n->w - n->margin_right;
      }
      n->y = ctr->y + ctr->h - 18 - n->h - n->margin_bottom;
      break;
    }
