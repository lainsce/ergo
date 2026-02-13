case COGITO_CAROUSEL: {
      if (n->len > 0) {
        // Draw background first
        CogitoStyle s = cogito_theme_resolve_node_state(n, false, false);
        CogitoColor bg = s.has_bg ? s.bg : n->bg;
        if (s.has_bg || n->bg_set) {
          int radius = s.has_radius ? s.radius : 0;
          cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);
        }
        
        // Apply scissor clipping for carousel content
        // Clip to carousel's exact bounds - this ensures items don't render outside
        if (cogito_backend && cogito_backend->begin_scissor) {
          // Clamp scissor to positive coordinates to avoid SDL issues
          int scissor_x = n->x < 0 ? 0 : n->x;
          int scissor_y = n->y < 0 ? 0 : n->y;
          int scissor_w = n->w;
          int scissor_h = n->h;
          cogito_backend->begin_scissor(scissor_x, scissor_y, scissor_w, scissor_h);
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
