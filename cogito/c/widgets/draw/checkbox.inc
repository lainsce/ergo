case COGITO_CHECKBOX: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      int text_w = 0;
      if (n->text) text_w = cogito_text_width(n->text->data);
      // Use intrinsic width unless hexpand is set
      int intrinsic_w = text_w + 32;
      int use_w = (n->hexpand && n->w > intrinsic_w) ? n->w : intrinsic_w;
      int max_box = n->h > 0 ? n->h : 16;
      int avail = use_w - (n->text ? (text_w + 6) : 0);
      if (avail < 12) avail = 12;
      int box = max_box;
      if (box > avail) box = avail;
      if (box < 12) box = 12;
      int cy = n->y + (n->h - box) / 2;
      bool over = hover;
      bool down = active;
      bool is_radio = (n->group.tag != EVT_NULL);
      CogitoStyle check_style = cogito_checkbox_check_style(is_radio, n->checked, over, down);
      CogitoStyle box_style = cogito_checkbox_box_style(is_radio, n->checked, over, down);
      Color resolved_text = s.has_text ? s.text : n->text_color;
      Color check = cogito_on_color(box_style.bg);
      bool has_border = n->border_width_set || n->border_color_set;
      if (s.has_bg) { n->bg = s.bg; n->bg_set = true; }
      if (s.has_border) { n->border_color = s.border; n->border_color_set = true; }
      if (s.has_border_width) { n->border_width = s.border_width; n->border_width_set = true; }
      if (is_radio) {
        if (n->bg_set) {
          DrawCircle(n->x + box / 2, cy + box / 2, box / 2, n->bg);
        }
        if (box_style.has_bg) {
          DrawCircle(n->x + box / 2, cy + box / 2, box / 2, box_style.bg);
        }
        if (has_border) {
          DrawCircleLines(n->x + box / 2, cy + box / 2, box / 2, n->border_color.a ? n->border_color : resolved_text);
        }
        if (n->checked) {
          int r = 4;
          int maxr = box / 2;
          if (r > maxr) r = maxr;
          DrawCircle(n->x + box / 2, cy + box / 2, r, check);
        }
      } else {
        int box_radius = box_style.has_radius ? box_style.radius : 4;
        if (n->bg_set) {
          cogito_draw_rect(n->x, cy, box, box, n->bg, box_radius);
        }
        if (box_style.has_bg) {
          cogito_draw_rect(n->x, cy, box, box, box_style.bg, box_radius);
        }
        if (has_border) {
          cogito_draw_rect_lines(n->x, cy, box, box, n->border_color.a ? n->border_color : resolved_text, box_radius, 1);
        }
        if (n->checked) {
          int mark = 16;
          if (mark > box) mark = box;
          int x0 = n->x + (box - mark) / 2;
          int y0 = cy + (box - mark) / 2;
          int x1 = x0 + (mark * 7) / 16;
          int y1 = y0 + (mark * 12) / 16;
          int x2 = x0 + (mark * 13) / 16;
          int y2 = y0 + (mark * 4) / 16;
          int sx = x0 + (mark * 3) / 16;
          int sy = y0 + (mark * 8) / 16;
          DrawLineEx((Vector2){(float)sx, (float)sy}, (Vector2){(float)x1, (float)y1}, 2.0f, check);
          DrawLineEx((Vector2){(float)x1, (float)y1}, (Vector2){(float)x2, (float)y2}, 2.0f, check);
        }
      }
      if (n->text) {
        int th = cogito_text_height();
        int ty = n->y + (n->h - th) / 2;
        Color label_text = n->text_color;
        if (cogito_has_selection_text) label_text = cogito_selection_text;
        else if (s.has_text) label_text = s.text;
        cogito_draw_text_node(n, n->x + box + 6, ty, n->text->data, label_text);
      }
      break;
    }
    
