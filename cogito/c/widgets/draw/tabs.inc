case COGITO_TABS: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      CogitoColor bg = n->bg;
      if (s.has_bg) bg = s.bg;
      CogitoColor text = s.has_text ? s.text : n->text_color;
      CogitoColor sel = s.has_selection ? s.selection : n->selection_color;
      cogito_anim_apply(n, &s, &bg, &text, NULL, &sel);
      if (n->bg_set || s.has_bg) {
        cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, n->border_radius);
      }
      if (n->tab_len == 0) break;

      const int slot_count = 4;
      const int icon_size = 24;
      const int icon_gap = 4;
      const int indicator_h = 3;
      const int indicator_radius = 2;
      int size = cogito_node_font_size(n);
      int label_h = cogito_text_height_size(size);
      int indicator_target_x = 0;
      int indicator_target_w = 0;

      for (size_t i = 0; i < n->tab_len; i++) {
        int sx0 = n->x + (n->w * (int)i) / slot_count;
        int sx1 = n->x + (n->w * ((int)i + 1)) / slot_count;
        int sw = sx1 - sx0;
        if (sw < 1) continue;

        const char* t = n->tab_labels[i] ? n->tab_labels[i]->data : "";
        const char* icon = NULL;
        if (n->tab_icons && n->tab_icons[i] && n->tab_icons[i]->data && n->tab_icons[i]->data[0]) {
          icon = n->tab_icons[i]->data;
        }
        bool has_icon = icon != NULL;
        int block_h = label_h + (has_icon ? (icon_size + icon_gap) : 0);
        int content_top = n->y + (n->h - block_h) / 2;
        int label_y = content_top + (has_icon ? (icon_size + icon_gap) : 0);
        bool is_sel = (n->selected == (int)i);
        CogitoColor item_col = is_sel ? sel : text;

        if (has_icon) {
          int icon_x = sx0 + (sw - icon_size) / 2;
          cogito_draw_icon_fallback(n, icon, icon_x, content_top, icon_size, item_col);
        }

        int tw = cogito_text_width_size_node(n, t, size);
        int tx = sx0 + (sw - tw) / 2;
        cogito_draw_text_size_node(n, tx, label_y, t, item_col, size, false);

        if (is_sel) {
          int inset = 2;
          if (tw <= inset * 2) inset = 0;
          int ind_w = tw - inset * 2;
          if (ind_w < 1) ind_w = 1;
          indicator_target_w = ind_w;
          indicator_target_x = tx + inset;
        }
      }

      if (n->selected >= 0 && (size_t)n->selected < n->tab_len && indicator_target_w > 0) {
        int indicator_y = n->y + n->h - indicator_h;
        double now = 0.0;
        if (cogito_backend && cogito_backend->get_time) now = cogito_backend->get_time();
        if (!n->anim_layout_active) {
          n->anim_layout_active = true;
          n->anim_x = n->anim_x_from = n->anim_x_target = indicator_target_x;
          n->anim_x_start = now;
          n->anim_w = n->anim_w_from = n->anim_w_target = indicator_target_w;
          n->anim_w_start = now;
        }
        int prev_anim_ms = n->anim_ms;
        int prev_anim_ease = n->anim_ease;
        if (n->anim_ms <= 0) n->anim_ms = 180;
        if (n->anim_ease <= 0) n->anim_ease = 4; // ease-in-out fallback
        int ind_x = cogito_anim_int(n, indicator_target_x,
                                    &n->anim_x, &n->anim_x_from, &n->anim_x_target,
                                    &n->anim_x_start, now);
        int ind_w = cogito_anim_int(n, indicator_target_w,
                                    &n->anim_w, &n->anim_w_from, &n->anim_w_target,
                                    &n->anim_w_start, now);
        n->anim_ms = prev_anim_ms;
        n->anim_ease = prev_anim_ease;
        if (ind_w < 1) ind_w = 1;
        if (ind_x < n->x) ind_x = n->x;
        if (ind_x + ind_w > n->x + n->w) ind_x = n->x + n->w - ind_w;
        if (ind_x < n->x) ind_x = n->x;
        cogito_draw_rect_radii(ind_x, indicator_y, ind_w, indicator_h, sel,
                               indicator_radius, indicator_radius, 0, 0);
      }
      break;
    }
