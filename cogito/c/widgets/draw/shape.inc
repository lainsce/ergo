case COGITO_SHAPE: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      CogitoColor fill = s.has_bg ? s.bg : (n->bg_set ? n->bg : cogito_color(24, 24, 33, 255));
      CogitoColor stroke = s.has_border ? s.border : (n->border_color_set ? n->border_color : fill);
      int stroke_w = s.has_border_width ? s.border_width : (n->border_width_set ? n->border_width : 0);
      if (n->shape.color_style > 0) {
        CogitoColor primary = fill;
        CogitoColor secondary = fill;
        CogitoColor tertiary = fill;
        cogito_theme_get_state_layer_palette(&primary, &secondary, NULL, &tertiary, NULL, NULL,
                                             NULL, NULL, NULL, NULL);
        if (n->shape.color_style == 1) fill = primary;
        else if (n->shape.color_style == 2) fill = secondary;
        else if (n->shape.color_style == 3) fill = tertiary;
        stroke = fill;
      }

      float px[COGITO_SHAPE_SVG_POLY_MAX];
      float py[COGITO_SHAPE_SVG_POLY_MAX];
      int point_count = n->shape.svg_poly_count;
      bool use_svg_poly = (point_count >= 3);
      if (!use_svg_poly) point_count = COGITO_SHAPE_VERTEX_COUNT;
      if (point_count > COGITO_SHAPE_SVG_POLY_MAX) point_count = COGITO_SHAPE_SVG_POLY_MAX;
      for (int i = 0; i < point_count; i++) {
        float vx = use_svg_poly ? n->shape.svg_poly[i * 2 + 0] : n->shape.vertices[i * 2 + 0];
        float vy = use_svg_poly ? n->shape.svg_poly[i * 2 + 1] : n->shape.vertices[i * 2 + 1];
        if (vx < 0.0f) vx = 0.0f;
        if (vx > 1.0f) vx = 1.0f;
        if (vy < 0.0f) vy = 0.0f;
        if (vy > 1.0f) vy = 1.0f;
        px[i] = (float)n->x + vx * (float)(n->w - 1);
        py[i] = (float)n->y + vy * (float)(n->h - 1);
      }

      bool pixel_shape = (n->shape.preset == COGITO_SHAPE_PIXEL_CIRCLE ||
                          n->shape.preset == COGITO_SHAPE_PIXEL_TRIANGLE);
      bool smooth_points = !pixel_shape && !use_svg_poly;
      bool antialias = !pixel_shape;
      if (antialias &&
          stroke_w > 0 &&
          stroke.r == fill.r &&
          stroke.g == fill.g &&
          stroke.b == fill.b &&
          stroke.a == fill.a) {
        // Avoid jagged overdraw from a same-color aliased outline on top of AA fill.
        stroke_w = 0;
      }
      cogito_draw_shape_polygon(px, py, point_count, fill, stroke, stroke_w, smooth_points, antialias);
      break;
    }
