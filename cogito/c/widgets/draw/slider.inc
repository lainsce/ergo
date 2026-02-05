case COGITO_SLIDER: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      Color track = s.has_bg ? s.bg : cogito_rgba(190, 190, 190, 255);
      Color knob = s.has_text ? s.text : cogito_rgba(80, 80, 80, 255);
      int pad = 8;
      int cy = n->y + n->h / 2;
      int x0 = n->x + pad;
      int x1 = n->x + n->w - pad;
      if (x1 < x0) x1 = x0;
      DrawLine(x0, cy, x1, cy, track);
      double t = 0.0;
      if (n->slider_max > n->slider_min) {
        t = (n->slider_value - n->slider_min) / (n->slider_max - n->slider_min);
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;
      }
      int kx = x0 + (int)((x1 - x0) * t);
      DrawCircle(kx, cy, 6, knob);
      break;
    }
    
