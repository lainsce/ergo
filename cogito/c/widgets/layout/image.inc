case COGITO_IMAGE:
      // Try to load image from file path during layout so intrinsic sizing
      // can use real dimensions.  Only attempt if the backend is ready
      // (renderer available); otherwise the draw pass will handle it.
      if (n->icon && n->icon->data && n->icon->data[0] && !n->image_loaded) {
        if (cogito_backend && cogito_backend->texture_create) {
          CogitoTexture* tex = NULL;
          int tw = 0, th = 0;
          if (cogito_image_load_file_texture(n->icon->data, &tex, &tw, &th, &n->image_avg_lum)) {
            n->image_texture = tex;
            n->image_w = tw;
            n->image_h = th;
            n->image_loaded = true;
            // Dimensions are now set; a subsequent layout pass will pick
            // them up via the intrinsic-size case.  We intentionally do NOT
            // call cogito_window_relayout here — doing so re-enters
            // cogito_layout_window recursively, causing O(N) nested full
            // layout passes when N images load during the same pass.
          }
          // Do NOT mark image_loaded on failure here — the draw pass
          // will retry when the renderer is guaranteed to be active.
        }
      }
      break;
