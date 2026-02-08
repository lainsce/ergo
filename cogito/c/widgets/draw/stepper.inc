case COGITO_STEPPER: {
      bool hover = false, active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);

      Color bg = s.has_bg ? s.bg : n->bg;
      Color text_col = s.has_text ? s.text : n->text_color;
      Color border_col = s.has_border ? s.border : n->border_color;
      int border_w = s.has_border_width ? s.border_width : 1;
      int radius = s.has_radius ? s.radius : (n->h / 2);
      int size = cogito_node_font_size(n);

      // Animated colors
      // cogito_anim_apply(n, &s, &bg, &text_col, &border_col, NULL);

      // Draw outer capsule
      cogito_draw_rect(n->x, n->y, n->w, n->h, bg, radius);
      if (border_w > 0) {
        cogito_draw_rect_lines(n->x, n->y, n->w, n->h, border_col, radius, border_w);
      }

      // Layout: [ - | value | + ]
      int btn_w = n->h;  // Square buttons at each end
      int center_w = n->w - btn_w * 2;

      // Draw dividers
      Color divider = border_col;
      divider.a = (unsigned char)(divider.a * 0.5f);
      DrawLine(n->x + btn_w, n->y + 6, n->x + btn_w, n->y + n->h - 6, divider);
      DrawLine(n->x + btn_w + center_w, n->y + 6, n->x + btn_w + center_w, n->y + n->h - 6, divider);

      // Draw minus button
      int minus_hover = hover && n->stepper_hit_minus;
      Color minus_col = text_col;
      if (n->stepper_value <= n->stepper_min) {
        minus_col.a = 80;  // Dimmed when at minimum
      }
      if (minus_hover) {
        Color hl = text_col;
        hl.a = 20;
        cogito_draw_rect(n->x + border_w, n->y + border_w, btn_w - border_w, n->h - border_w * 2, hl, radius);
      }
      int icon_len = 12;
      int mx = n->x + (btn_w - icon_len) / 2;
      int my = n->y + n->h / 2;
      DrawLineEx((Vector2){(float)mx, (float)my}, (Vector2){(float)(mx + icon_len), (float)my}, 2.0f, minus_col);

      // Draw plus button
      int plus_hover = hover && n->stepper_hit_plus;
      Color plus_col = text_col;
      if (n->stepper_value >= n->stepper_max) {
        plus_col.a = 80;  // Dimmed when at maximum
      }
      if (plus_hover) {
        Color hl = text_col;
        hl.a = 20;
        cogito_draw_rect(n->x + btn_w + center_w, n->y + border_w, btn_w - border_w, n->h - border_w * 2, hl, radius);
      }
      int px = n->x + btn_w + center_w + (btn_w - icon_len) / 2;
      int py = n->y + n->h / 2;
      DrawLineEx((Vector2){(float)px, (float)py}, (Vector2){(float)(px + icon_len), (float)py}, 2.0f, plus_col);
      DrawLineEx((Vector2){(float)(px + icon_len / 2), (float)(py - icon_len / 2)}, (Vector2){(float)(px + icon_len / 2), (float)(py + icon_len / 2)}, 2.0f, plus_col);

      // Draw value text centered
      char val_buf[32];
      if (n->stepper_step >= 1.0 && n->stepper_value == (int)n->stepper_value) {
        snprintf(val_buf, sizeof(val_buf), "%d", (int)n->stepper_value);
      } else {
        snprintf(val_buf, sizeof(val_buf), "%.1f", n->stepper_value);
      }
      int text_w = cogito_text_width_size_node(n, val_buf, size);
      int text_h = cogito_text_height_size(size);
      int tx = n->x + btn_w + (center_w - text_w) / 2;
      int ty = n->y + (n->h - text_h) / 2;
      cogito_draw_text_size_node(n, tx, ty, val_buf, text_col, size, false);
      break;
    }
