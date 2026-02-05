case COGITO_SWITCH: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      int text_w = 0;
      if (n->text) text_w = cogito_text_width(n->text->data);
      int track_w = n->w - (n->text ? (text_w + 8) : 0);
      if (track_w < 24) track_w = 24;
      if (n->min_w_set && track_w < n->min_w) track_w = n->min_w;
      if (n->max_w_set && n->max_w > 0 && track_w > n->max_w) track_w = n->max_w;
      int h = n->h > 0 ? n->h : 18;
      if (n->min_h_set && h < n->min_h) h = n->min_h;
      if (n->max_h_set && n->max_h > 0 && h > n->max_h) h = n->max_h;
      if (h > track_w / 2) h = track_w / 2;
      if (h < 14) h = 14;
      int w = track_w;
      int cy = n->y + (n->h - h) / 2;
      int cx = n->x;
      bool over = hover;
      bool down = active;
      CogitoStyle track_s = cogito_switch_track_style(n->checked, over, down);
      CogitoStyle knob_s = cogito_switch_knob_style(n->checked, over, down);
      Color track_off = n->track_set ? n->track_color : cogito_rgba(170, 170, 170, 255);
      Color track_on = n->track_on_set ? n->track_on_color : cogito_rgba(90, 160, 110, 255);
      if (track_s.has_bg) {
        track_off = track_s.bg;
        track_on = track_s.bg;
      }
      if (track_s.has_track_on) track_on = track_s.track_on;
      Color track = n->checked ? track_on : track_off;
      cogito_draw_rect(cx, cy, w, h, track, h / 2);
      int knob_w = h - 4;
      int knob_h = h - 4;
      if (knob_s.has_min_w && knob_w < knob_s.min_w) knob_w = knob_s.min_w;
      if (knob_s.has_max_w && knob_s.max_w > 0 && knob_w > knob_s.max_w) knob_w = knob_s.max_w;
      if (knob_s.has_min_h && knob_h < knob_s.min_h) knob_h = knob_s.min_h;
      if (knob_s.has_max_h && knob_s.max_h > 0 && knob_h > knob_s.max_h) knob_h = knob_s.max_h;
      if (knob_w < 4) knob_w = 4;
      if (knob_h < 4) knob_h = 4;
      if (knob_w > w - 4) knob_w = w - 4;
      if (knob_h > h - 2) knob_h = h - 2;
      int knob = knob_w;
      int kx = n->checked ? (cx + w - knob - 2) : (cx + 2);
      int ky = cy + (h - knob_h) / 2;
      Color knob_col = n->knob_set ? n->knob_color : cogito_rgba(250, 250, 250, 255);
      if (knob_s.has_bg) knob_col = knob_s.bg;
      else if (knob_s.has_knob) knob_col = knob_s.knob;
      cogito_draw_rect(kx, ky, knob_w, knob_h, knob_col, knob_h / 2);
      if (n->text) {
        int th = cogito_text_height();
        int ty = n->y + (n->h - th) / 2;
        cogito_draw_text(n->x + w + 8, ty, n->text->data, n->text_color);
      }
      break;
    }
    
