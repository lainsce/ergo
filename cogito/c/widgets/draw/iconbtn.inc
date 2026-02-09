case COGITO_ICONBTN: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      if (n->parent && n->parent->kind == COGITO_APPBAR) {
        int mx = 0, my = 0;
        if (cogito_backend && cogito_backend->get_mouse_position) {
          cogito_backend->get_mouse_position(&mx, &my);
        }
        bool over = cogito_hit_rect(mx, my, n->x, n->y, n->w, n->h);
        bool down = over && (cogito_backend && cogito_backend->is_mouse_button_down(0));
        if (down) {
          s = cogito_style_merge(&s, &cogito_theme.appbar_child_iconbtn_active);
        } else if (over) {
          s = cogito_style_merge(&s, &cogito_theme.appbar_child_iconbtn_hover);
        }
      }
      CogitoColor bg = n->bg;
      CogitoColor border = n->border_color;
      int border_w = n->border_width;
      int radius = n->border_radius;
      if (s.has_bg) bg = s.bg;
      if (n->parent && n->parent->kind == COGITO_APPBAR) {
        int mx = 0, my = 0;
        if (cogito_backend && cogito_backend->get_mouse_position) {
          cogito_backend->get_mouse_position(&mx, &my);
        }
        bool over = cogito_hit_rect(mx, my, n->x, n->y, n->w, n->h);
        bool down = over && (cogito_backend && cogito_backend->is_mouse_button_down(0));
        if (over || down) {
          CogitoStyle overlay = down ? cogito_theme.appbar_child_iconbtn_active : cogito_theme.appbar_child_iconbtn_hover;
          if (overlay.has_bg) {
            bg = cogito_color_blend(bg, overlay.bg);
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
      CogitoColor icon_color = cogito_has_selection_text ? cogito_selection_text : (s.has_text ? s.text : cogito_on_color(n->selection_color));
      if (n->text) {
        int btn_min = n->w < n->h ? n->w : n->h;
        int target = 24;
        if (btn_min > 0 && btn_min < target) target = btn_min;
        if (target < 8) target = 8;
        // TODO: Replace with backend texture drawing
        // Texture2D tex = {0};
        // int tw = 0;
        // int th = 0;
        // if (cogito_icon_load_texture(n->text->data, target, &tex, &tw, &th)) {
        //   cogito_backend->draw_texture(...);
        // } else {
        int text_w = cogito_text_width(n->text->data);
        int text_h = cogito_text_height();
        int tx = n->x + (n->w - text_w) / 2;
        int ty = n->y + (n->h - text_h) / 2;
        CogitoColor text = n->text_color;
        if (cogito_has_selection_text) text = cogito_selection_text;
        else if (s.has_text) text = s.text;
        cogito_draw_text_node(n, tx, ty, n->text->data, text);
        // }
      }
      break;
    }
