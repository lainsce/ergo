case COGITO_SLIDER: {
      // Slider component
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      Color inactive_track = s.has_bg ? s.bg : cogito_rgba(121, 116, 126, 255);
      Color active_track = s.has_text ? s.text : cogito_rgba(103, 80, 164, 255);

      int pad = 16;
      int cy = n->y + n->h / 2;
      int x0 = n->x + pad;
      int x1 = n->x + n->w - pad;
      if (x1 < x0) x1 = x0;

      // Calculate thumb position
      double t = 0.0;
      if (n->slider_max > n->slider_min) {
        t = (n->slider_value - n->slider_min) / (n->slider_max - n->slider_min);
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;
      }
      int kx = x0 + (int)((x1 - x0) * t);

      // Draw inactive track (right side)
      DrawRectangle(kx, cy - 2, x1 - kx, 4, inactive_track);

      // Draw active track (left side)
      DrawRectangle(x0, cy - 2, kx - x0, 4, active_track);

      // Draw thumb
      int thumb_radius = active || n->slider_dragging ? 10 : 8;
      if (hover) thumb_radius = 10;

      // State layer for hover/press
      if (hover || active) {
        Color state_layer = active_track;
        state_layer.a = active ? 30 : 20;
        DrawCircle(kx, cy, 20, state_layer);
      }

      DrawCircle(kx, cy, thumb_radius, active_track);
      break;
    }
    
