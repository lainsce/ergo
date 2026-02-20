case COGITO_LIST: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      CogitoColor bg = n->bg;
      CogitoColor border = n->border_color;
      int border_w = n->border_width;
      int radius = 0;
      if (s.has_bg) bg = s.bg;
      if (s.has_border) border = s.border;
      if (s.has_border_width) border_w = s.border_width;
      if (s.has_radius) radius = s.radius;
      CogitoColor sel_bg = s.has_selection ? s.selection : n->selection_color;
      cogito_anim_apply(n, &s, &bg, NULL, &border, &sel_bg);
      if (n->bg_set || s.has_bg || border_w > 0) {
        if (n->shadow_set && n->shadow_level > 0) {
          cogito_draw_shadow_node_level(n, &s, n->x, n->y, n->w, n->h, radius, n->shadow_level);
    }
        cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);
        if (border_w > 0) {
          cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, border, radius, border_w);
    }
  }
      bool has_selection = n->selected >= 0 && (size_t)n->selected < n->len;
      if (has_selection) {
        CogitoNode* sel = n->children[n->selected
    ];
        if (cogito_rect_visible(sel->x, sel->y, sel->w, sel->h, n->x, n->y, n->w, n->h))
          cogito_draw_rect(sel->x, sel->y, sel->w, sel->h, sel_bg, radius);
  }
      for (size_t i = 0; i < n->len; i++) {
        CogitoNode* c = n->children[i
    ];
        if (!cogito_rect_visible(c->x, c->y, c->w, c->h, n->x, n->y, n->w, n->h)) continue;
        if (has_selection && (size_t)n->selected == i) {
          cogito_has_selection_text = true;
          cogito_selection_text = cogito_on_color(sel_bg);
          cogito_draw_node(c);
          cogito_has_selection_text = false;
    } else {
          cogito_draw_node(c);
    }
  }
      break;
}