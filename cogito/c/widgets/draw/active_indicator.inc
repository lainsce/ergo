case COGITO_ACTIVE_INDICATOR: {
  CogitoStyle s = cogito_theme_resolve_node(n);
  CogitoColor color = s.has_text ? s.text : n->text_color;

  double now = 0.0;
  if (cogito_backend && cogito_backend->get_time) {
    now = cogito_backend->get_time();
  }
  if (n->indicator.last_time == 0.0) n->indicator.last_time = now;
  // Rotate at ~1.5 revolutions per second
  double dt = now - n->indicator.last_time;
  n->indicator.angle += dt * 540.0; // degrees per second
  if (n->indicator.angle > 360.0) n->indicator.angle -= 360.0;
  n->indicator.last_time = now;

  int cx = n->x + n->w / 2;
  int cy = n->y + n->h / 2;
  int num_bars = 12;
  int bar_w = 3;
  int bar_h = 9;
  int radius = (n->w < n->h ? n->w : n->h) / 2 - bar_h;
  if (radius < 4) radius = 4;

  for (int i = 0; i < num_bars; i++) {
    double angle_deg = (360.0 / num_bars) * i + n->indicator.angle;
    double angle_rad = angle_deg * 3.14159265358979 / 180.0;
    // Opacity fades around the circle
    float alpha = (float)(num_bars - i) / (float)num_bars;
    alpha = alpha * alpha; // ease
    CogitoColor bar_color = color;
    bar_color.a = (uint8_t)(color.a * alpha);
    if (bar_color.a < 10) bar_color.a = 10;

    int bx = cx + (int)(cos(angle_rad) * radius) - bar_w / 2;
    int by = cy + (int)(sin(angle_rad) * radius) - bar_h / 2;
    cogito_draw_rect(bx, by, bar_w, bar_h, bar_color, bar_w / 2);
  }
  // Request continuous redraw for animation
  if (cogito_anim_frames_remaining < 2) cogito_anim_frames_remaining = 2;
  break;
}
