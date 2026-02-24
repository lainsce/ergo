case COGITO_SHAPE: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      CogitoColor fill = s.has_bg ? s.bg : (n->bg_set ? n->bg : cogito_color(24, 24, 33, 255));
      CogitoColor stroke = s.has_border ? s.border : (n->border_color_set ? n->border_color : fill);
      int stroke_w = s.has_border_width ? s.border_width : (n->border_width_set ? n->border_width : 0);

      float px[8];
      float py[8];
      for (int i = 0; i < 8; i++) {
        float vx = n->shape.vertices[i * 2 + 0];
        float vy = n->shape.vertices[i * 2 + 1];
        if (vx < 0.0f) vx = 0.0f;
        if (vx > 1.0f) vx = 1.0f;
        if (vy < 0.0f) vy = 0.0f;
        if (vy > 1.0f) vy = 1.0f;
        px[i] = (float)n->x + vx * (float)(n->w - 1);
        py[i] = (float)n->y + vy * (float)(n->h - 1);
      }

      cogito_draw_shape_polygon(px, py, fill, stroke, stroke_w);
      break;
    }
