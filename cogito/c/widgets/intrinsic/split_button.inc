case COGITO_SPLIT_BUTTON: {
  int tw = 0;
  if (n->text) tw = cogito_text_width(n->text->data);
  int th = cogito_text_height();
  int pad = 24;
  int arrow_w = 32;
  w = tw + pad * 2 + arrow_w;
  h = th + 16;
  if (h < 36) h = 36;
  if (w < 80) w = 80;
  break;
}
