case COGITO_SPLIT_BUTTON: {
  int tw = 0;
  if (n->text) tw = cogito_text_width(n->text->data);
  int th = cogito_text_height();
  int pad = 24;
  int gap = 2;
  int main_min_w = 120;
  h = th + 16;
  if (h < 56) h = 56;
  int menu_w = h;
  int main_w = tw + pad * 2;
  if (main_w < main_min_w) main_w = main_min_w;
  w = main_w + menu_w + gap;
  break;
}
