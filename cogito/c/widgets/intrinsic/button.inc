case COGITO_BUTTON:
      {
        int btn_h = 40;
        int pad_x = 16;
        cogito_button_size_metrics((int)n->button.size, &btn_h, &pad_x, NULL, NULL);
        w = text_w + pad_x * 2;
        h = btn_h;
        int min_text_h = text_h + 8;
        if (h < min_text_h) h = min_text_h;
      }
      break;
    
