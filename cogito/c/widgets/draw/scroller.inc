case COGITO_SCROLLER: {
      bool hover = false, active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      CogitoColor bg = s.has_bg ? s.bg : n->bg;

      if (n->len > 0) {
        cogito_anim_apply(n, &s, &bg, NULL, NULL, NULL);
        cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, 0);

        if (cogito_backend && cogito_backend->begin_scissor) {
          cogito_backend->begin_scissor(n->x, n->y, n->w, n->h);
        }
        for (size_t i = 0; i < n->len; i++) {
          cogito_draw_node(n->children[i]);
        }
        if (cogito_backend && cogito_backend->end_scissor) {
          cogito_backend->end_scissor();
        }
      }
      break;
    }
