case COGITO_PROGRESS: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      Color track = s.has_bg ? s.bg : (n->bg_set ? n->bg : (s.has_track ? s.track : n->bg));
      Color fill = s.has_selection ? s.selection : (s.has_text ? s.text : (s.has_knob ? s.knob : n->text_color));
      int radius = s.has_radius ? s.radius : n->border_radius;
      double t = n->progress_value;
      if (t < 0.0) t = 0.0;
      if (t > 1.0) t = 1.0;
      cogito_draw_rect(n->x, n->y, n->w, n->h, track, radius);
      int fw = (int)(n->w * t);
      if (fw > 0) {
        cogito_draw_rect(n->x, n->y, fw, n->h, fill, radius);
      }
      break;
    }
