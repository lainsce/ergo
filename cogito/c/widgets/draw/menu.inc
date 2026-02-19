static void cogito_draw_menu(void) {
  if (!cogito_menu.open || !cogito_menu.owner) return;
  CogitoColor bg = cogito_theme.menu.has_bg ? cogito_theme.menu.bg : cogito_rgba(250, 250, 250, 255);
  CogitoColor border = cogito_theme.menu.has_border ? cogito_theme.menu.border : cogito_rgba(180, 180, 180, 255);
  CogitoColor text_color = cogito_theme.menu.has_text ? cogito_theme.menu.text : cogito_rgba(30, 30, 30, 255);
  CogitoColor sel = cogito_theme.menu.has_selection ? cogito_theme.menu.selection : cogito_rgba(220, 230, 250, 255);
  int radius = cogito_theme.menu.has_radius ? cogito_theme.menu.radius : 4;
  bool has_border = cogito_theme.menu.has_border || cogito_theme.menu.has_border_width;
  int border_w = cogito_theme.menu.has_border_width ? cogito_theme.menu.border_width : 1;
  int shadow_level = cogito_theme.menu.has_shadow ? cogito_theme.menu.shadow_level : 0;
  int font_size = cogito_font_size();
  if (cogito_theme.menu.has_font_size && cogito_theme.menu.font_size > 0) {
    font_size = cogito_theme.menu.font_size;
  }
  int x = cogito_menu.x;
  int y = cogito_menu.y;
  int w = cogito_menu.w;
  int h = cogito_menu.h;
  if (shadow_level > 0) {
    int tl = radius, tr = radius, br = radius, bl = radius;
    cogito_resolve_node_radii(NULL, &cogito_theme.menu, radius, w, h, &tl, &tr, &br, &bl);
    cogito_draw_shadow_radii_level(x, y, w, h, tl, tr, br, bl, shadow_level);
  }
  cogito_draw_rect_node(NULL, &cogito_theme.menu, x, y, w, h, bg, radius);
  if (has_border && border_w > 0) {
    cogito_draw_rect_lines_node(NULL, &cogito_theme.menu, x, y, w, h, border, radius, border_w);
  }
  if (cogito_menu.owner->kind == COGITO_DROPDOWN && w > 2) {
    int seam_y = -1;
    if (y == cogito_menu.owner->y + cogito_menu.owner->h) seam_y = y;
    if (y + h == cogito_menu.owner->y) seam_y = y + h - 1;
    if (seam_y >= y && seam_y < y + h) {
      cogito_draw_line(x + 1, seam_y, x + w - 2, seam_y, border);
    }
  }
  int mx = 0, my = 0;
  if (cogito_backend && cogito_backend->get_mouse_position) {
    cogito_backend->get_mouse_position(&mx, &my);
  }
  int hover = cogito_menu_hit(&cogito_menu, mx, my);
  int item_radius = cogito_theme.menu_item.has_radius ? cogito_theme.menu_item.radius : 0;
  // Use section dividers if available, otherwise fall back to menu_divider
  bool use_section = cogito_menu.owner->menu_section_after != NULL;
  bool use_divider = use_section || cogito_menu.owner->button.menu_divider;
  int item_gap = (cogito_menu.owner->button.menu_item_gap > 0) ? cogito_menu.owner->button.menu_item_gap : 
                 (cogito_theme.menu.has_item_gap ? cogito_theme.menu.item_gap : (use_divider ? 0 : 4));
  int divider_h = use_divider ? 1 : 0;
  int current_y = y + cogito_menu.pad_top;
  
  for (size_t i = 0; i < cogito_menu.owner->menu_len; i++) {
    if ((int)i == hover) {
      int sel_radius = item_radius;
      if (sel_radius < 0) sel_radius = 0;
      cogito_draw_rect(x, current_y, w, cogito_menu.item_h, sel, sel_radius);
    }
    const char* text = cogito_menu.owner->menu_labels[i] ? cogito_menu.owner->menu_labels[i]->data : "";
    int th = cogito_text_height_size(font_size);
    int ty = current_y + (cogito_menu.item_h - th) / 2;
    cogito_draw_text_size_node(cogito_menu.owner, x + cogito_menu.pad_x, ty, text, text_color, font_size, false);
    
    // Draw divider if needed (check per-item section divider)
    bool has_divider_after = false;
    if (use_section) {
      has_divider_after = cogito_menu.owner->menu_section_after[i];
    } else if (cogito_menu.owner->button.menu_divider) {
      has_divider_after = true;
    }
    if (has_divider_after && i < cogito_menu.owner->menu_len - 1) {
      int divider_y = current_y + cogito_menu.item_h + item_gap;
      CogitoColor divider_color = cogito_theme.menu.has_border ? cogito_theme.menu.border : cogito_rgba(180, 180, 180, 255);
      int divider_inset = 16; // Inset from left and right
      cogito_draw_line(x + divider_inset, divider_y, x + w - divider_inset, divider_y, divider_color);
    }
    
    // Move to next item position
    current_y += cogito_menu.item_h + item_gap;
    if (has_divider_after && i < cogito_menu.owner->menu_len - 1) {
      current_y += divider_h;
    }
  }
}
