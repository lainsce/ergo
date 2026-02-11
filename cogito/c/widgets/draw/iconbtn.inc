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
      CogitoColor icon_color = n->text_color;
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
	      if (n->parent && n->parent->kind == COGITO_APPBAR && !s.has_radius && !n->radius_set) {
	        int min_dim = n->w < n->h ? n->w : n->h;
	        if (min_dim > 0) radius = min_dim / 2;
	      }
	      if (s.has_text) icon_color = s.text;
	      else if (cogito_has_selection_text) icon_color = cogito_selection_text;
	      cogito_anim_apply(n, &s, &bg, &icon_color, &border, NULL);
	      if (n->bg_set || border_w > 0) {
	        if (n->shadow_set && n->shadow_level > 0) {
	          cogito_draw_shadow_node_level(n, &s, n->x, n->y, n->w, n->h, radius, n->shadow_level);
	        }
        cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);
        if (border_w > 0) {
          cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, border, radius, border_w);
        }
      }
      if (n->text) {
        int btn_min = n->w < n->h ? n->w : n->h;
        int target = s.has_icon_size ? s.icon_size : 24;
        if (target < 8) target = 8;
        if (btn_min > 0 && target > btn_min) target = btn_min;
        int ix = n->x + (n->w - target) / 2;
        int iy = n->y + (n->h - target) / 2;
        cogito_draw_icon_fallback(n, n->text->data, ix, iy, target, icon_color);
      }
      break;
    }
