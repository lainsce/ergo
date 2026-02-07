case COGITO_TOAST: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      if (s.has_bg) { n->bg = s.bg; n->bg_set = true; }
      if (s.has_border) { n->border_color = s.border; n->border_color_set = true; }
      if (s.has_border_width) { n->border_width = s.border_width; n->border_width_set = true; }
      if (s.has_radius) n->border_radius = s.radius;
      if (n->bg_set) {
        cogito_draw_rect(n->x, n->y, n->w, n->h, n->bg, n->border_radius);
        if (n->border_width_set && n->border_width > 0) {
          cogito_draw_rect_lines(n->x, n->y, n->w, n->h, n->border_color, n->border_radius, n->border_width);
        }
      }
      int size = cogito_node_font_size(n);
      Color text = cogito_has_selection_text ? cogito_selection_text : (s.has_text ? s.text : n->text_color);
      const char* t = n->text ? n->text->data : "";
      int th = cogito_text_height_size(size);
      int ty = n->y + (n->h - th) / 2;
      int pad_x = 12;
      int cx = 0;
      int cy = 0;
      int cw = 0;
      int ch = 0;
      cogito_toast_close_rect(n, &cx, &cy, &cw, &ch);
      int action_w = 0;
      if (n->action_text && n->action_text->data[0]) {
        action_w = cogito_text_width_size_node(n, n->action_text->data, size);
      }
      int text_w = (cw > 0) ? (cx - (n->x + pad_x) - 6) : (n->w - pad_x * 2);
      if (action_w > 0) {
        text_w -= (action_w + 24);
      }
      if (text_w < 0) text_w = 0;
      cogito_draw_text_ellipsis(n, n->x + pad_x, ty, text_w, t, text, size, cogito_node_bold(n), 0);
      if (action_w > 0) {
        Color action_color = s.has_selection ? s.selection : (s.has_text ? s.text : n->selection_color);
        int action_x = n->x + pad_x + text_w + 24;
        int action_y = ty;
        cogito_draw_text_size_node(n, action_x, action_y, n->action_text->data, action_color, size, false);
        n->action_button_x = action_x;
        n->action_button_y = action_y;
        n->action_button_w = action_w;
        n->action_button_h = th;
      }
      if (cw > 0 && ch > 0) {
        int icon_size = s.has_icon_size ? s.icon_size : 12;
        if (icon_size < 8) icon_size = 8;
        int ix = cx + (cw - icon_size) / 2;
        int iy = cy + (ch - icon_size) / 2;
        Texture2D tex = {0};
        int tw = 0;
        int th_i = 0;
        Color tint = s.has_icon_color ? s.icon_color : text;
        if (cogito_icon_load_texture("x", icon_size, &tex, &tw, &th_i)) {
          float dw = (float)icon_size;
          float dh = (float)icon_size;
          if (tw > 0 && th_i > 0) {
            float sx = (float)icon_size / (float)tw;
            float sy = (float)icon_size / (float)th_i;
            float s = sx < sy ? sx : sy;
            dw = (float)tw * s;
            dh = (float)th_i * s;
          }
          float dx = ix + ((float)icon_size - dw) * 0.5f;
          float dy = iy + ((float)icon_size - dh) * 0.5f;
          DrawTexturePro(tex, (Rectangle){0, 0, (float)tw, (float)th_i},
                         (Rectangle){dx, dy, dw, dh},
                         (Vector2){0, 0}, 0.0f, tint);
        } else {
          DrawLine(ix, iy, ix + icon_size, iy + icon_size, tint);
          DrawLine(ix + icon_size, iy, ix, iy + icon_size, tint);
        }
      }
      break;
    }
