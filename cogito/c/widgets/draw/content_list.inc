case COGITO_CONTENT_LIST: {
  CogitoStyle s = cogito_theme_resolve_node(n);
  CogitoColor bg = s.has_bg ? s.bg : n->bg;
  CogitoColor border_color = s.has_border ? s.border : n->border_color;
  int border_w = s.has_border_width ? s.border_width : (n->border_width_set ? n->border_width : 1);

  // Draw overall background with base radius
  int r = s.has_radius ? s.radius : 12;
  if (s.has_bg) {
    cogito_draw_rect(n->x, n->y, n->w, n->h, bg, r);
  }
  if (border_w > 0 && (s.has_border || n->border_width_set)) {
    cogito_draw_rect_lines(n->x, n->y, n->w, n->h, border_color, r, border_w);
  }

  // Draw children with proper rounded corners based on position
  for (size_t i = 0; i < n->len; i++) {
    CogitoNode* child = n->children[i];
    
    // Determine corner radii based on position
    int tl = 0, tr = 0, br = 0, bl = 0;
    
    if (n->len == 1) {
      // Single item: all corners 12px
      tl = tr = br = bl = 12;
    } else if (i == 0) {
      // First item: 12px top-left/top-right, 8px bottom-left/bottom-right
      tl = tr = 12;
      bl = br = 8;
    } else if (i == n->len - 1) {
      // Last item: 8px top-left/top-right, 12px bottom-left/bottom-right
      tl = tr = 8;
      bl = br = 12;
    } else {
      // Middle items: all corners 8px
      tl = tr = br = bl = 8;
    }
    
    // Draw child background with position-specific rounded corners
    // Use the list's bg color for the item backgrounds
    if (bg.a > 0) {
      cogito_draw_rect_radii(child->x, child->y, child->w, child->h, bg, tl, tr, br, bl);
    }
    
    // Draw the child
    cogito_draw_node(child);
    
    // Draw divider between items (not after last)
    if (i + 1 < n->len) {
      CogitoColor div_color = border_color;
      div_color.a = (uint8_t)(div_color.a * 0.3f);
      int dy = child->y + child->h;
      cogito_draw_rect(n->x + 16, dy, n->w - 32, 1, div_color, 0);
    }
  }
  break;
}
