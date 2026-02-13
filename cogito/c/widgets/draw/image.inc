case COGITO_IMAGE: {
      bool hover = false;
      bool active = false;
      cogito_node_hover_state(n, &hover, &active);
      CogitoStyle s = cogito_theme_resolve_node_state(n, hover, active);
      
      // Read style values into locals without mutating node state
      CogitoColor bg = s.has_bg ? s.bg : n->bg;
      bool has_bg = s.has_bg || n->bg_set;
      CogitoColor border = s.has_border ? s.border : n->border_color;
      int border_w = s.has_border_width ? s.border_width : n->border_width;
      // User-set radius takes priority over theme
      int radius = n->radius_set ? n->border_radius : (s.has_radius ? s.radius : n->border_radius);
      if (radius > n->w / 2) radius = n->w / 2;
      if (radius > n->h / 2) radius = n->h / 2;
      
      // Load image from file path if not yet loaded (fallback if layout didn't load it)
      if (n->icon && n->icon->data && n->icon->data[0] && !n->image_loaded) {
        CogitoTexture* tex = NULL;
        int tw = 0, th = 0;
        if (cogito_image_load_file_texture(n->icon->data, &tex, &tw, &th, &n->image_avg_lum)) {
          n->image_texture = tex;
          n->image_w = tw;
          n->image_h = th;
          n->image_loaded = true;
          CogitoNode* win = cogito_node_window(n);
          if (win) {
            cogito_window_relayout(win);
          }
        } else {
          n->image_loaded = true; // Mark as attempted to avoid retrying
        }
      }
      
      // Draw background and shadow
      if (has_bg || border_w > 0) {
        if (n->shadow_set && n->shadow_level > 0) {
          cogito_draw_shadow_node_level(n, &s, n->x, n->y, n->w, n->h, radius, n->shadow_level);
        }
        cogito_draw_rect_node(n, &s, n->x, n->y, n->w, n->h, bg, radius);
        if (border_w > 0) {
          cogito_draw_rect_lines_node(n, &s, n->x, n->y, n->w, n->h, border, radius, border_w);
        }
      }
      
      // Draw the loaded image texture
      if (n->image_texture && n->image_w > 0 && n->image_h > 0) {
        // Calculate draw dimensions - fit the image to the widget size (cover mode)
        int draw_w = n->w;
        int draw_h = n->h;
        int draw_x = n->x;
        int draw_y = n->y;
        
        // Scale to fill while maintaining aspect ratio (cover mode)
        float img_aspect = (float)n->image_w / (float)n->image_h;
        float widget_aspect = (n->h > 0) ? (float)n->w / (float)n->h : 1.0f;
        int src_x = 0, src_y = 0, src_w = n->image_w, src_h = n->image_h;
        
        if (img_aspect > widget_aspect && widget_aspect > 0) {
          // Image is wider - crop sides
          src_w = (int)(n->image_h * widget_aspect);
          src_x = (n->image_w - src_w) / 2;
        } else if (img_aspect < widget_aspect && img_aspect > 0) {
          // Image is taller - crop top/bottom
          src_h = (int)(n->image_w / widget_aspect);
          src_y = (n->image_h - src_h) / 2;
        }
        
        // --- Parallax effect for images inside carousel items ---
        // Shift the source rect horizontally so the rounded item acts as a
        // peephole into a wider scene.  The shift is proportional to the
        // item's offset from the carousel center.
        if (n->parent && n->parent->kind == COGITO_CAROUSEL_ITEM) {
          CogitoNode* ci = n->parent;          // carousel item
          CogitoNode* carousel = ci->parent;   // carousel
          if (carousel && carousel->kind == COGITO_CAROUSEL && carousel->w > 0) {
            int item_cx = ci->x + ci->w / 2;
            int car_cx  = carousel->x + carousel->w / 2;
            float ratio = (float)(item_cx - car_cx) / (float)(carousel->w / 2);
            // Strength: shift up to 20% of the visible source width
            float strength = 0.20f * (float)src_w;
            int shift = (int)(ratio * strength);
            src_x += shift;
            // Clamp so we don't read outside the texture
            if (src_x < 0) src_x = 0;
            if (src_x + src_w > n->image_w) src_x = n->image_w - src_w;
          }
        }

        CogitoRect src_rect = {src_x, src_y, src_w, src_h};
        CogitoRect dst_rect = {draw_x, draw_y, draw_w, draw_h};
        CogitoColor white = {255, 255, 255, 255};
        
        // Draw the texture
        if (cogito_backend && cogito_backend->draw_texture) {
          cogito_backend->draw_texture(n->image_texture, src_rect, dst_rect, white);
        }
        
        // Apply rounded corner masks if needed
        if (radius > 0) {
          // For mask color, prefer the explicitly set node bg, then theme bg,
          // then fall back to white as a last resort so masks are visible.
          CogitoColor mask_color;
          if (n->bg_set) {
            mask_color = n->bg;
          } else if (s.has_bg) {
            mask_color = s.bg;
          } else {
            // Walk up the tree to find nearest parent with a background
            CogitoColor fallback = {255, 255, 255, 255};
            CogitoNode* p = n->parent;
            while (p) {
              if (p->bg_set) { fallback = p->bg; break; }
              p = p->parent;
            }
            mask_color = fallback;
          }
          // Use small rectangles to mask corners with AA fringe pixels.
          // AA coverage curve matches backend: powf(cov, 0.7f)
          // For each scanline we compute the exact float inset, draw a solid
          // mask for the integer portion, then a 1px AA pixel at the edge.
          #define COGITO_IMG_AA_COV(c) ((c) <= 0.05f ? 0.0f : ((c) >= 1.0f ? 1.0f : powf((c), 0.7f)))
          float r_f = (float)radius;
          // Top-left corner
          for (int dy = 0; dy < radius; dy++) {
            float y_dist = r_f - ((float)dy + 0.5f);
            float inside = r_f * r_f - y_dist * y_dist;
            if (inside < 0.0f) inside = 0.0f;
            float x_exact = r_f - sqrtf(inside);
            int solid_w = (int)x_exact;
            float frac = x_exact - (float)solid_w;
            if (solid_w > 0) {
              cogito_draw_rect(draw_x, draw_y + dy, solid_w, 1, mask_color, 0);
            }
            if (frac > 0.01f && draw_x + solid_w < draw_x + draw_w) {
              float cov = COGITO_IMG_AA_COV(frac);
              CogitoColor aa = mask_color;
              aa.a = (uint8_t)(mask_color.a * cov + 0.5f);
              if (aa.a > 0) cogito_draw_rect(draw_x + solid_w, draw_y + dy, 1, 1, aa, 0);
            }
          }
          // Top-right corner
          for (int dy = 0; dy < radius; dy++) {
            float y_dist = r_f - ((float)dy + 0.5f);
            float inside = r_f * r_f - y_dist * y_dist;
            if (inside < 0.0f) inside = 0.0f;
            float x_exact = r_f - sqrtf(inside);
            int solid_w = (int)x_exact;
            float frac = x_exact - (float)solid_w;
            if (solid_w > 0) {
              cogito_draw_rect(draw_x + draw_w - solid_w, draw_y + dy, solid_w, 1, mask_color, 0);
            }
            if (frac > 0.01f && solid_w < draw_w) {
              float cov = COGITO_IMG_AA_COV(frac);
              CogitoColor aa = mask_color;
              aa.a = (uint8_t)(mask_color.a * cov + 0.5f);
              if (aa.a > 0) cogito_draw_rect(draw_x + draw_w - solid_w - 1, draw_y + dy, 1, 1, aa, 0);
            }
          }
          // Bottom-left corner
          for (int dy = 0; dy < radius; dy++) {
            float y_dist = (float)(dy + 1) - 0.5f;
            float inside = r_f * r_f - y_dist * y_dist;
            if (inside < 0.0f) inside = 0.0f;
            float x_exact = r_f - sqrtf(inside);
            int solid_w = (int)x_exact;
            float frac = x_exact - (float)solid_w;
            if (solid_w > 0) {
              cogito_draw_rect(draw_x, draw_y + draw_h - radius + dy, solid_w, 1, mask_color, 0);
            }
            if (frac > 0.01f && draw_x + solid_w < draw_x + draw_w) {
              float cov = COGITO_IMG_AA_COV(frac);
              CogitoColor aa = mask_color;
              aa.a = (uint8_t)(mask_color.a * cov + 0.5f);
              if (aa.a > 0) cogito_draw_rect(draw_x + solid_w, draw_y + draw_h - radius + dy, 1, 1, aa, 0);
            }
          }
          // Bottom-right corner
          for (int dy = 0; dy < radius; dy++) {
            float y_dist = (float)(dy + 1) - 0.5f;
            float inside = r_f * r_f - y_dist * y_dist;
            if (inside < 0.0f) inside = 0.0f;
            float x_exact = r_f - sqrtf(inside);
            int solid_w = (int)x_exact;
            float frac = x_exact - (float)solid_w;
            if (solid_w > 0) {
              cogito_draw_rect(draw_x + draw_w - solid_w, draw_y + draw_h - radius + dy, solid_w, 1, mask_color, 0);
            }
            if (frac > 0.01f && solid_w < draw_w) {
              float cov = COGITO_IMG_AA_COV(frac);
              CogitoColor aa = mask_color;
              aa.a = (uint8_t)(mask_color.a * cov + 0.5f);
              if (aa.a > 0) cogito_draw_rect(draw_x + draw_w - solid_w - 1, draw_y + draw_h - radius + dy, 1, 1, aa, 0);
            }
          }
          #undef COGITO_IMG_AA_COV
        }
      } else if (n->icon && n->icon->data) {
        // Fallback to icon rendering if image failed to load
        CogitoColor icon_color = cogito_has_selection_text ? cogito_selection_text : (s.has_text ? s.text : cogito_on_color(n->selection_color));
        int target = s.has_icon_size ? s.icon_size : (n->w < n->h ? n->w : n->h);
        if (target <= 0) target = 16;
        int min_dim = n->w < n->h ? n->w : n->h;
        if (min_dim > 0 && target > min_dim) target = min_dim;
        int ix = n->x + (n->w - target) / 2;
        int iy = n->y + (n->h - target) / 2;
        cogito_draw_icon_fallback(n, n->icon->data, ix, iy, target, icon_color);
      }
      break;
    }
