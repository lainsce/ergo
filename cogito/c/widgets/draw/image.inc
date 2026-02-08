case COGITO_IMAGE: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      if (s.has_bg) { n->bg = s.bg; n->bg_set = true; }
      else { n->bg_set = false; }
      if (s.has_border) { n->border_color = s.border; n->border_color_set = true; }
      else { n->border_color_set = false; }
      if (s.has_border_width) { n->border_width = s.border_width; n->border_width_set = true; }
      else { n->border_width_set = false; }
      if (s.has_radius) n->border_radius = s.radius;
      if (n->bg_set || n->border_width > 0) {
        if (n->shadow_set && n->shadow_level > 0) {
          cogito_draw_shadow(n->x, n->y, n->w, n->h, n->border_radius, n->shadow_level);
        }
        cogito_draw_rect(n->x, n->y, n->w, n->h, n->bg, n->border_radius);
        if (n->border_width > 0) {
          cogito_draw_rect_lines(n->x, n->y, n->w, n->h, n->border_color, n->border_radius, n->border_width);
        }
      }
      Color icon_color = cogito_has_selection_text ? cogito_selection_text : (s.has_text ? s.text : cogito_on_color(n->selection_color));
      if (n->icon && n->icon->data) {
        int target = n->w < n->h ? n->w : n->h;
        if (target <= 0) target = 16;
        Texture2D tex = {0};
        int tw = 0;
        int th = 0;
        if (cogito_icon_load_texture(n->icon->data, target, &tex, &tw, &th)) {
          float scale = 1.0f;
          if (tw > 0 && th > 0) {
            float sx = (float)n->w / (float)tw;
            float sy = (float)n->h / (float)th;
            scale = sx < sy ? sx : sy;
            if (scale <= 0.0f) scale = 1.0f;
          }
          float dw = tw * scale;
          float dh = th * scale;
          float dx = n->x + (n->w - dw) * 0.5f;
          float dy = n->y + (n->h - dh) * 0.5f;
          DrawTexturePro(tex, (Rectangle){0, 0, (float)tw, (float)th},
                         (Rectangle){dx, dy, dw, dh}, (Vector2){0, 0}, 0.0f, icon_color);
        }
      }
      break;
    }
    
