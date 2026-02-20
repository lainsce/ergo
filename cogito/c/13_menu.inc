static void cogito_menu_close(void) {
  cogito_menu.open = false;
  cogito_menu.owner = NULL;
}

static void cogito_menu_open_for(CogitoNode* n) {
  if (!n || n->menu_len == 0) return;
  int font_size = cogito_font_size();
  if (cogito_theme.menu.has_font_size && cogito_theme.menu.font_size > 0) {
    font_size = cogito_theme.menu.font_size;
  }
  CogitoStyle* item_style = &cogito_theme.menu_item;
  int pad_x = item_style->has_padding_left ? item_style->padding_left
              : (cogito_theme.menu.has_padding_left ? cogito_theme.menu.padding_left : 10);
  int pad_y = item_style->has_padding_top ? item_style->padding_top
              : (cogito_theme.menu.has_padding_top ? cogito_theme.menu.padding_top : 6);
  
  // Sections use gap by default; dividers are opt-in via menu_divider flag
  bool use_section = n->menu_section_after != NULL;
  bool use_divider = n->button.menu_divider;
  
   int item_h = font_size + pad_y * 2;
  int min_h = item_style->has_min_h ? item_style->min_h
              : (cogito_theme.menu.has_min_h ? cogito_theme.menu.min_h : 0);
  if (min_h > item_h) item_h = min_h;
  // Minimum 44px item height for sectioned menus (accessibility)
  if (use_section && item_h < 48) item_h = 48;
  
  int menu_pad_top = 8;
  int menu_pad_bottom = 8;
  int item_gap = (n->button.menu_item_gap > 0) ? n->button.menu_item_gap : 
                 (cogito_theme.menu.has_item_gap ? cogito_theme.menu.item_gap : 2);
  int section_gap = 2; // Gap between sections
  int divider_h = use_divider ? 1 : 0;
  int max_w = 0;
  for (size_t i = 0; i < n->menu_len; i++) {
    const char* text = n->menu_labels[i] ? n->menu_labels[i]->data : "";
    int w = cogito_text_width_size(text, font_size);
    if (w > max_w) max_w = w;
  }
  int w = max_w + pad_x * 2;
  if (n->w > w) w = n->w;
  
  // Minimum menu width for accessibility
  if (w < 112) w = 112;
  
  // Calculate menu height with gaps and potential dividers
  int section_padding = 2; // 2px padding at top and bottom of each section
  int h = menu_pad_top;
  if (use_section) {
    // Calculate per-section heights
    int section_start = 0;
    for (size_t i = 0; i < n->menu_len; i++) {
      bool has_section_after = n->menu_section_after[i];
      if (has_section_after || i == n->menu_len - 1) {
        // Calculate section height
        int section_items = (int)i - section_start + 1;
        int section_h = section_items * item_h;
        // Add item gaps within section
         if (section_items > 1) {
           section_h += (section_items - 1) * item_gap;
         }
         // Add 2px padding at top and bottom of section
         section_h += section_padding * 2;
         // Minimum 48px per section for accessibility
         if (section_h < 48) section_h = 48;

         h += section_h;
        // Add section gap or divider after section (except last)
        if (has_section_after && i < n->menu_len - 1) {
          if (use_divider) {
            h += divider_h;
          } else {
            h += section_gap;
          }
        }
        section_start = (int)i + 1;
      }
    }
  } else {
    // Original calculation for non-sectioned menus
    for (size_t i = 0; i < n->menu_len; i++) {
      h += item_h;
      if (i < n->menu_len - 1) {
        h += item_gap;
      }
    }
  }
  h += menu_pad_bottom;
  
  cogito_menu.open = true;
  cogito_menu.owner = n;
  cogito_menu.item_h = item_h;
  cogito_menu.pad_x = pad_x;
  cogito_menu.pad_y = pad_y;
  cogito_menu.pad_top = menu_pad_top;
  cogito_menu.pad_bottom = menu_pad_bottom;
  cogito_menu.w = w;
  cogito_menu.h = h;
  int x = n->x;
  int y = n->y + n->h;
  
  // Get window size from backend
  int win_w = 800, win_h = 600;  // Default fallback
  if (cogito_backend && cogito_backend_ready && cogito_active_window) {
    // Use window size from active window
    win_w = cogito_active_window->w;
    win_h = cogito_active_window->h;
  }
  
  // Check if menu would go off right edge - try repositioning left
  if (x + w > win_w - 4) {
    int left_pos = n->x - w;
    if (left_pos >= 4) {
      // There's room on the left, use it
      x = left_pos;
    } else {
      // Not enough room on either side, clamp to window
      x = win_w - 4 - w;
      if (x < 4) x = 4;
    }
  }
  
  if (w > win_w - 8) w = win_w - 8;
  if (h > win_h - 8) h = win_h - 8;
  if (x + w > win_w - 4) x = win_w - 4 - w;
  if (x < 4) x = 4;
  
  // Check if menu would go off bottom - try repositioning above
  if (y + h > win_h - 4) {
    int above = n->y - h;
    if (above >= 4) {
      y = above;
    } else {
      // Not enough room above or below, clamp to window
      y = win_h - 4 - h;
      if (y < 4) y = 4;
    }
  }
  if (y < 4) y = 4;
  cogito_menu.w = w;
  cogito_menu.h = h;
  cogito_menu.x = x;
  cogito_menu.y = y;
}

static int cogito_menu_hit(const CogitoMenu* menu, int x, int y) {
  if (!menu || !menu->open) return -1;
  if (x < menu->x || y < menu->y || x > menu->x + menu->w || y > menu->y + menu->h) return -1;
  int rel_y = y - (menu->y + menu->pad_top);
  if (rel_y < 0) return -1;
  
  // Sections use gap by default; dividers are opt-in via menu_divider flag
  bool use_section = menu->owner->menu_section_after != NULL;
  bool use_divider = menu->owner->button.menu_divider;
  int item_gap = (menu->owner->button.menu_item_gap > 0) ? menu->owner->button.menu_item_gap : 
                 (cogito_theme.menu.has_item_gap ? cogito_theme.menu.item_gap : 2);
  int section_gap = 2;
  int divider_h = use_divider ? 1 : 0;
  int section_padding = 2; // 2px padding at top and bottom of each section
  
  int idx = -1;
  int current_y = use_section ? section_padding : 0;  // Start after section padding if using sections
  
  for (size_t i = 0; i < menu->owner->menu_len; i++) {
    if (rel_y >= current_y && rel_y < current_y + menu->item_h) {
      idx = (int)i;
      break;
    }
    current_y += menu->item_h + item_gap;
    // Check if this item has a section break after it
    bool has_section_after = use_section && menu->owner->menu_section_after[i];
    if (has_section_after && i < menu->owner->menu_len - 1) {
      // Add bottom padding of current section, gap/divider, and top padding of next section
      current_y += section_padding;  // Bottom padding of current section
      if (use_divider) {
        current_y += divider_h;
      } else {
        current_y += section_gap;
      }
      current_y += section_padding;  // Top padding of next section
    }
  }
  
  if (idx < 0 || (size_t)idx >= menu->owner->menu_len) {
    return -1;
  }
  
  return idx;
}
