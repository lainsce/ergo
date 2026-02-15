case COGITO_TIMEPICKER: {
  CogitoStyle s = cogito_theme_resolve_node(n);
  int pad_l = s.has_padding_left ? s.padding_left : 18;
  int pad_t = s.has_padding_top ? s.padding_top : 18;
  int pad_r = s.has_padding_right ? s.padding_right : pad_l;
  int pad_b = s.has_padding_bottom ? s.padding_bottom : pad_t;
  int size = cogito_node_font_size(n);
  int th = cogito_text_height_size(size);
  int header_h = th + 8;
  // Two columns (hour, minute) with up/down buttons
  int col_h = th * 3 + 24; // 3 rows visible + button padding
  int ampm_h = th + 16;
  w = pad_l + pad_r + 180;
  h = pad_t + header_h + col_h + ampm_h + pad_b;
  if (w < 180) w = 180;
  if (h < 180) h = 180;
  break;
}
