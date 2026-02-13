case COGITO_IMAGE:
      // Respect an explicit size first; otherwise fall back to the
      // intrinsic image dimensions, and finally a reasonable default.
      if (n->min_w_set && n->min_h_set) {
        w = n->min_w;
        h = n->min_h;
      } else if (n->image_loaded && n->image_w > 0 && n->image_h > 0) {
        w = n->image_w;
        h = n->image_h;
      } else {
        w = 100;
        h = 100;
      }
      break;
    
