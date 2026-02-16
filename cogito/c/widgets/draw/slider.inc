case COGITO_SLIDER: {
      // Slider component with disconnected rectangular thumb
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      CogitoColor inactive_track = s.has_bg ? s.bg : (s.has_track ? s.track : n->bg);
      CogitoColor active_track = s.has_text ? s.text : (s.has_knob ? s.knob : n->text_color);
      cogito_anim_apply(n, &s, &inactive_track, &active_track, NULL, NULL);

      int track_h = n->track_height_set ? n->track_height : (s.has_track_height ? s.track_height : 4);
      int thumb_w = 8;
      int thumb_h = 32;
      int thumb_r = 4;  // corner radius for rounded rect thumb
      int gap = 6;      // gap between thumb and track on all sides
      int pad = 16;
      int cy = n->y + n->h / 2;
      int x0 = n->x + pad;
      int x1 = n->x + n->w - pad;
      if (x1 < x0) x1 = x0;

      // Calculate thumb position
      double t = 0.0;
      if (n->slider.max > n->slider.min) {
        t = (n->slider.value - n->slider.min) / (n->slider.max - n->slider.min);
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;
      }
      int kx = x0 + (int)((x1 - x0) * t);

      // Thumb edges
      int thumb_left = kx - thumb_w / 2;
      int thumb_right = kx + thumb_w / 2;

      // Draw inactive track (right of thumb + gap) with rounded ends
      int inactive_start = thumb_right + gap;
      if (inactive_start < x1) {
        int track_r = track_h / 2;
        cogito_draw_rect(inactive_start, cy - track_h / 2, x1 - inactive_start, track_h, inactive_track, track_r);
      }

      // Draw active track (left of thumb - gap) with rounded ends
      int active_end = thumb_left - gap;
      if (active_end > x0) {
        int track_r = track_h / 2;
        cogito_draw_rect(x0, cy - track_h / 2, active_end - x0, track_h, active_track, track_r);
      }

      // State layer for hover/press
      if (hover || active) {
        CogitoColor state_layer = active_track;
        state_layer.a = active ? 30 : 20;
        // TODO: Replace with backend circle drawing
        // DrawCircle(kx, cy, 20, state_layer);
        cogito_draw_rect(kx - 20, cy - 20, 40, 40, state_layer, 20);
      }

      // Draw rectangular thumb with rounded corners
      cogito_draw_rect(thumb_left, cy - thumb_h / 2, thumb_w, thumb_h, active_track, thumb_r);
      break;
    }
