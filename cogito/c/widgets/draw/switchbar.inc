case COGITO_SWITCHBAR: {
  bool hover = false, active = false;
  cogito_node_hover_state(n, &hover, &active);
  CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);

  CogitoColor bg = s.has_bg ? s.bg : n->bg;
  CogitoColor text_color = s.has_text ? s.text : n->text_color;
  int radius = s.has_radius ? s.radius : 8;
  int bar_h = 48;

  cogito_anim_apply(n, &s, &bg, &text_color, NULL, NULL);

  // Draw bar background
  cogito_draw_rect(n->x, n->y, n->w, bar_h, bg, radius);

  // Draw label
  if (n->text && n->text->data && n->text->data[0]) {
    int ts = cogito_node_font_size(n);
    int th = cogito_text_height_size(ts);
    int tx = n->x + 16;
    int ty = n->y + (bar_h - th) / 2;
    cogito_draw_text_size_node(n, tx, ty, n->text->data, text_color, ts, false);
  }

  // Draw switch on right side
  {
    int sw_w = 52;
    int sw_h = 28;
    int sw_x = n->x + n->w - sw_w - 16;
    int sw_y = n->y + (bar_h - sw_h) / 2;

    CogitoColor track_off = s.has_track ? s.track : cogito_rgba(170, 170, 170, 255);
    CogitoColor track_on = s.has_track_on ? s.track_on : (s.has_selection ? s.selection : cogito_rgba(90, 160, 110, 255));
    CogitoColor track = n->checked ? track_on : track_off;
    CogitoColor knob = s.has_knob ? s.knob : cogito_rgba(255, 255, 255, 255);

    cogito_draw_rect(sw_x, sw_y, sw_w, sw_h, track, sw_h / 2);

    int knob_w = 24;
    int knob_h = 24;
    int knob_x = n->checked ? (sw_x + sw_w - knob_w - 2) : (sw_x + 2);
    int knob_y = sw_y + (sw_h - knob_h) / 2;
    cogito_draw_rect(knob_x, knob_y, knob_w, knob_h, knob, knob_h / 2);
  }

  // Draw children (only if checked/enabled)
  if (n->checked) {
    for (size_t i = 0; i < n->len; i++) {
      cogito_draw_node(n->children[i]);
    }
  }
  break;
}
