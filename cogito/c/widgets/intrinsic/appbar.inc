case COGITO_APPBAR: {
      int appbar_h = 0;
      cogito_appbar_row_positions(n, NULL, NULL, NULL, NULL, &appbar_h);
      h = appbar_h > 0 ? appbar_h : 48;
      w = n->w;
      break;
    }
    
