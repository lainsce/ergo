case COGITO_SWITCH: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      int text_w = 0;
      if (n->text) text_w = cogito_text_width(n->text->data);
      int intrinsic_w = text_w + 40;
      int use_w = (n->hexpand && n->w > intrinsic_w) ? n->w : intrinsic_w;
      int track_w = use_w - (n->text ? (text_w + 8) : 0);
      if (track_w < 24) track_w = 24;
      if (n->min_w_set && track_w < n->min_w) track_w = n->min_w;
      if (n->max_w_set && n->max_w > 0 && track_w > n->max_w) track_w = n->max_w;
      int text_h = n->text ? cogito_text_height() : 0;
      int intrinsic_h = text_h + 4;
      if (intrinsic_h < 18) intrinsic_h = 18;
      int h = (n->vexpand && n->h > intrinsic_h) ? n->h : intrinsic_h;
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

      // Animate track color
      cogito_anim_apply(n, &track_s, &track, NULL, NULL, NULL);

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

      // Animate knob position
      int kx_target = n->checked ? (cx + w - knob_w - 2) : (cx + 2);
      int kx = cogito_anim_int(n, kx_target, &n->anim_x, &n->anim_x_from, &n->anim_x_target, GetTime());

      int ky = cy + (h - knob_h) / 2;
      Color knob_col = n->knob_set ? n->knob_color : cogito_rgba(250, 250, 250, 255);
      if (knob_s.has_bg) knob_col = knob_s.bg;
      else if (knob_s.has_knob) knob_col = knob_s.knob;
      cogito_draw_rect(kx, ky, knob_w, knob_h, knob_col, knob_h / 2);
      if (n->text) {
        CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
        int th = cogito_text_height();
        int ty = n->y + (n->h - th) / 2;
        Color label_text = n->text_color;
        if (cogito_has_selection_text) label_text = cogito_selection_text;
        else if (s.has_text) label_text = s.text;
        cogito_draw_text_node(n, n->x + w + 8, ty, n->text->data, label_text);
      }
      break;
    }

