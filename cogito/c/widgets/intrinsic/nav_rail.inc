case COGITO_NAV_RAIL: {
  int item_h = 56;  // Standard item height
  w = 80;  // Standard nav rail width
  h = n->padding_top + n->padding_bottom + (item_h * n->nav_rail.len);
  if (h < 200) h = 200;  // Minimum height
  break;
}
