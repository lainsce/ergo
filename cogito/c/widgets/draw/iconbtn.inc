case COGITO_ICONBTN: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      bool has_custom_class = n->class_name && n->class_name->data && n->class_name->data[0];
      bool in_toolbar = n->parent && n->parent->kind == COGITO_TOOLBAR;
      bool toolbar_floating = in_toolbar ? cogito_toolbar_is_floating(n->parent) : false;
      bool toolbar_compact = in_toolbar ? cogito_toolbar_is_compact(n->parent) : false;
      if (n->parent && n->parent->kind == COGITO_APPBAR) {
        int mx = 0, my = 0;
        if (cogito_backend && cogito_backend->get_mouse_position) {
          cogito_backend->get_mouse_position(&mx, &my);
        }
        bool over = cogito_hit_rect(mx, my, n->x, n->y, n->w, n->h);
        bool down = over && (cogito_backend && cogito_backend->is_mouse_button_down(0));
        if (down) {
          cogito_style_merge_into(&s, &cogito_theme.appbar_child_iconbtn_active, &s);
        } else if (over) {
          cogito_style_merge_into(&s, &cogito_theme.appbar_child_iconbtn_hover, &s);
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
      if (n->parent && n->parent->kind == COGITO_APPBAR && !n->radius_set) {
        // AppBar IconBtn is always fully rounded (circle) by default
        int min_dim = n->w < n->h ? n->w : n->h;
        radius = min_dim / 2; // fully rounded = circle
      } else if (s.has_radius) {
        radius = s.radius;
      } else {
        int min_dim = n->w < n->h ? n->w : n->h;
        if (n->button.icon_shape == 1) {
          if (n->button.size == COGITO_BUTTON_SIZE_XS) {
            radius = 12;
          } else if (n->button.size == COGITO_BUTTON_SIZE_S) {
            radius = 12;
          } else if (n->button.size == COGITO_BUTTON_SIZE_M) {
            radius = 16;
          } else if (n->button.size == COGITO_BUTTON_SIZE_L) {
            radius = 28;
          } else if (n->button.size == COGITO_BUTTON_SIZE_XL) {
            radius = 28;
          } else {
            radius = 16;
          }
        } else {
          radius = min_dim / 2; // fully rounded = circle
        }
      }
      if (in_toolbar && (!toolbar_floating || toolbar_compact) && !has_custom_class) {
        bg = cogito_rgba(0, 0, 0, 0);
        border_w = 0;
        if (!n->radius_set) radius = 0;
      }
      if (s.has_text) icon_color = s.text;
      else if (cogito_has_selection_text) icon_color = cogito_selection_text;
      cogito_anim_apply(n, &s, &bg, &icon_color, &border, NULL);
      bool iconbtn_has_bg = n->bg_set || s.has_bg || (bg.a > 0);
      if (iconbtn_has_bg || border_w > 0) {
        if (n->shadow_set && n->shadow_level > 0) {
          cogito_draw_shadow_node_level(n, &s, n->x, n->y, n->w, n->h, radius, n->shadow_level);
        }
        cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);
        // Draw ripple effect when iconbtn is active (showing Primary color; clipped to widget bounds)
        if (cogito_ripple_update(n)) {
          float r_radius, r_alpha;
          cogito_ripple_get(n, &r_radius, &r_alpha);
          if (r_radius > 0 && r_alpha > 0) {
            CogitoColor ripple_color = icon_color;
            ripple_color.a = (uint8_t)(255 * r_alpha);
            int cx = n->x + n->ripple_x;
            int cy = n->y + n->ripple_y;
            cogito_draw_circle_clipped_node(n, &s, radius, cx, cy, r_radius, ripple_color);
          }
        }
        if (border_w > 0 || n->button.outlined) {
          int draw_border_w = border_w;
          if (n->button.outlined && draw_border_w == 0) {
            draw_border_w = 1; // Default border width for outlined icon buttons
          }
          cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, border, radius, draw_border_w);
        }
      }
      if (n->text) {
        int btn_min = n->w < n->h ? n->w : n->h;
        int default_icon = 24;
        cogito_button_size_metrics((int)n->button.size, n->kind, NULL, NULL, &default_icon, NULL);
        // Icon sizes per size tier (from spec): XS=20, S=24, M=24, L=32, XL=40
        // cogito_button_size_metrics already returns these correctly via out_icon
        int target = s.has_icon_size ? s.icon_size : default_icon;
        if (target < 8) target = 8;
        if (btn_min > 0 && target > btn_min) target = btn_min;
        int ix = n->x + (n->w - target) / 2;
        int iy = n->y + (n->h - target) / 2;
        cogito_draw_icon_fallback(n, n->text->data, ix, iy, target, icon_color);
      }
      break;
    }
