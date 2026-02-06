case COGITO_ICONBTN: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      if (n->parent && n->parent->kind == COGITO_APPBAR) {
        int mx = (int)GetMousePosition().x;
        int my = (int)GetMousePosition().y;
        bool over = cogito_hit_rect(mx, my, n->x, n->y, n->w, n->h);
        bool down = over && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        if (down) {
          s = cogito_style_merge(&s, &cogito_theme.appbar_child_iconbtn_active);
        } else if (over) {
          s = cogito_style_merge(&s, &cogito_theme.appbar_child_iconbtn_hover);
        }
      }
      Color bg = n->bg;
      Color border = n->border_color;
      int border_w = n->border_width;
      int radius = n->border_radius;
      if (s.has_bg) bg = s.bg;
      if (n->parent && n->parent->kind == COGITO_APPBAR) {
        int mx = (int)GetMousePosition().x;
        int my = (int)GetMousePosition().y;
        bool over = cogito_hit_rect(mx, my, n->x, n->y, n->w, n->h);
        bool down = over && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        if (over || down) {
          CogitoStyle overlay = down ? cogito_theme.appbar_child_iconbtn_active : cogito_theme.appbar_child_iconbtn_hover;
          if (overlay.has_bg) {
            bg = cogito_blend(bg, overlay.bg);
          }
        }
      }
      if (s.has_border) border = s.border;
      if (s.has_border_width) border_w = s.border_width;
      if (s.has_radius) radius = s.radius;
      if (n->bg_set || border_w > 0) {
        if (n->shadow_set && n->shadow_level > 0) {
          cogito_draw_shadow(n->x, n->y, n->w, n->h, radius, n->shadow_level);
        }
        cogito_draw_rect(n->x, n->y, n->w, n->h, bg, radius);
        if (border_w > 0) {
          cogito_draw_rect_lines(n->x, n->y, n->w, n->h, border, radius, border_w);
        }
      }
      Color icon_color = cogito_has_selection_text ? cogito_selection_text : (s.has_text ? s.text : cogito_on_color(n->selection_color));
      if (n->text) {
        int btn_min = n->w < n->h ? n->w : n->h;
        int target = 24;
        if (btn_min > 0 && btn_min < target) target = btn_min;
        if (target < 8) target = 8;
        Texture2D tex = {0};
        int tw = 0;
        int th = 0;
        if (cogito_icon_load_texture(n->text->data, target, &tex, &tw, &th)) {
          float scale = 1.0f;
          if (tw > 0 && th > 0) {
            float sx = (float)n->w / (float)tw;
            float sy = (float)n->h / (float)th;
            scale = sx < sy ? sx : sy;
            if (scale > 1.0f) scale = 1.0f;
            if (scale <= 0.0f) scale = 1.0f;
          }
          float dw = tw * scale;
          float dh = th * scale;
          float dx = n->x + (n->w - dw) * 0.5f;
          float dy = n->y + (n->h - dh) * 0.5f;
          DrawTexturePro(tex, (Rectangle){0, 0, (float)tw, (float)th},
                         (Rectangle){dx, dy, dw, dh}, (Vector2){0, 0}, 0.0f, icon_color);
        } else {
          int text_w = cogito_text_width(n->text->data);
          int text_h = cogito_text_height();
          int tx = n->x + (n->w - text_w) / 2;
          int ty = n->y + (n->h - text_h) / 2;
          Color text = n->text_color;
          if (cogito_has_selection_text) text = cogito_selection_text;
          else if (s.has_text) text = s.text;
          cogito_draw_text_node(n, tx, ty, n->text->data, text);
        }
      }
      break;
    }
    
