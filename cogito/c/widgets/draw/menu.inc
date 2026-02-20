// Helper function to get tertiary colors for vibrant menus
static void cogito_menu_theme_tertiary(CogitoColor* out_color, CogitoColor* out_container, CogitoColor* on_container, CogitoColor* on_color) {
  if (!out_color && !out_container && !on_container && !on_color) return;
  double h = 0.0, c = 0.0, t = 0.0;
  cogito_rgb_to_hct(cogito_theme_accent, &h, &c, &t);
  double tert_h = fmod(h + 60.0,
  360.0);
  if (tert_h < 0.0) tert_h += 360.0;
  double tert_c = 24.0;
  CogitoColor tertiary = cogito_hct_to_rgb(tert_h, tert_c, cogito_theme_dark ? 80.0 : 40.0);
  CogitoColor tertiary_container = cogito_hct_to_rgb(tert_h, tert_c, cogito_theme_dark ? 30.0 : 90.0);
  CogitoColor on_tertiary_container = cogito_hct_to_rgb(tert_h, tert_c, cogito_theme_dark ? 90.0 : 30.0);
  CogitoColor on_tertiary = cogito_theme_dark ? cogito_hct_to_rgb(tert_h, tert_c,
  20.0) : cogito_rgba(255,
  255,
  255,
  255);
  if (out_color) *out_color = tertiary;
  if (out_container) *out_container = tertiary_container;
  if (on_container) *on_container = on_tertiary_container;
  if (on_color) *on_color = on_tertiary;
}
// Helper to count sections and find which section an item belongs to
static int cogito_menu_count_sections(CogitoNode* owner, int* section_of_item) {
  if (!owner || owner->menu_len == 0) return 0;
  bool use_section = owner->menu_section_after != NULL;
  if (!use_section) {
    for (size_t i = 0; i < owner->menu_len; i++) section_of_item[i
    ] = 0;
    return 1; // Single section
  }
  int section_count = 1;
  section_of_item[
    0
  ] = 0;
  for (size_t i = 1; i < owner->menu_len; i++) {
    if (owner->menu_section_after[i - 1
    ]) section_count++;
    section_of_item[i
    ] = section_count - 1;
  }
  return section_count;
}
// Helper to find first and last item indices for each section
static void cogito_menu_section_bounds(CogitoNode* owner, int section_count, int* first_item, int* last_item) {
  if (!owner || owner->menu_len == 0) return;
  bool use_section = owner->menu_section_after != NULL;
  for (int s = 0; s < section_count; s++) {
    first_item[s
    ] = -1;
    last_item[s
    ] = -1;
  }
  int current_section = 0;
  for (size_t i = 0; i < owner->menu_len; i++) {
    if (first_item[current_section
    ] < 0) first_item[current_section
    ] = (int)i;
    last_item[current_section
    ] = (int)i;
    if (use_section && owner->menu_section_after[i
    ] && i < owner->menu_len - 1) {
      current_section++;
    }
  }
}

static void cogito_draw_menu(void) {
  if (!cogito_menu.open || !cogito_menu.owner) return;
  
  // Check if vibrant mode is enabled
  bool vibrant = cogito_menu.owner->button.menu_vibrant;
  
  // Get tertiary colors (always compute for selection text)
  CogitoColor tertiary = {
    0
  }, tertiary_container = {
    0
  }, on_tertiary_container = {
    0
  }, on_tertiary = {
    0
  };
  cogito_menu_theme_tertiary(&tertiary, &tertiary_container, &on_tertiary_container, &on_tertiary);
  
  // MD3: derive surface/on-surface/outline from theme accent via HCT if theme has no explicit values
  // Surface-container-high: chroma ~4, tone 94 (light) / 22 (dark)
  // On-surface: chroma ~4, tone 10 (light) / 90 (dark)
  // Outline: chroma ~6, tone 50 (light) / 60 (dark)
  double h_acc = 0.0, c_acc = 0.0, t_acc = 0.0;
  cogito_rgb_to_hct(cogito_theme_accent, &h_acc, &c_acc, &t_acc);
  double surf_chroma = c_acc > 4.0 ? 4.0 : c_acc;
  CogitoColor md3_surface   = cogito_hct_to_rgb(h_acc, surf_chroma, cogito_theme_dark ? 22.0 : 94.0);
  CogitoColor md3_on_surf   = cogito_hct_to_rgb(h_acc, surf_chroma, cogito_theme_dark ? 90.0 : 10.0);
  CogitoColor md3_outline   = cogito_hct_to_rgb(h_acc,
  6.0,         cogito_theme_dark ? 60.0 : 50.0);
  CogitoColor md3_sel       = cogito_hct_to_rgb(h_acc,
  24.0,        cogito_theme_dark ? 30.0 : 90.0); // secondary-container

  CogitoColor bg         = cogito_theme.menu.has_bg        ? cogito_theme.menu.bg        : md3_surface;
  CogitoColor border     = cogito_theme.menu.has_border    ? cogito_theme.menu.border    : md3_outline;
  CogitoColor text_color = cogito_theme.menu.has_text      ? cogito_theme.menu.text      : md3_on_surf;
  CogitoColor sel        = cogito_theme.menu.has_selection ? cogito_theme.menu.selection : md3_sel;
  
  // Override with tertiary colors in vibrant mode
  if (vibrant) {
    bg         = tertiary_container;
    text_color = on_tertiary_container;
    sel        = tertiary;
  }
  
  bool has_border = cogito_theme.menu.has_border || cogito_theme.menu.has_border_width;
  int border_w = cogito_theme.menu.has_border_width ? cogito_theme.menu.border_width : 0; // No border by default
  int shadow_level = cogito_theme.menu.has_shadow ? cogito_theme.menu.shadow_level : 1; // Default elevation = 1
  int font_size = cogito_font_size();
  if (cogito_theme.menu.has_font_size && cogito_theme.menu.font_size > 0) {
    font_size = cogito_theme.menu.font_size;
  }
  int x = cogito_menu.x;
  int y = cogito_menu.y;
  int w = cogito_menu.w;
  int h = cogito_menu.h;
  
  // Sections use gap by default; dividers are opt-in via menu_divider flag
  bool use_section = cogito_menu.owner->menu_section_after != NULL;
  bool use_divider = cogito_menu.owner->button.menu_divider;
  int item_gap = (cogito_menu.owner->button.menu_item_gap > 0) ? cogito_menu.owner->button.menu_item_gap : 
                 (cogito_theme.menu.has_item_gap ? cogito_theme.menu.item_gap : 2);
  int section_gap = 2; // Gap between sections
  int divider_h = use_divider ? 1 : 0;
  
  // Count sections and determine section membership for radius scheme
  int max_items = (int)cogito_menu.owner->menu_len;
  int* section_of_item = (int*)malloc(max_items * sizeof(int));
  int* first_item_of_section = (int*)malloc((max_items + 1) * sizeof(int));
  int* last_item_of_section = (int*)malloc((max_items + 1) * sizeof(int));
  int section_count = cogito_menu_count_sections(cogito_menu.owner, section_of_item);
  cogito_menu_section_bounds(cogito_menu.owner, section_count, first_item_of_section, last_item_of_section);
  
  // Calculate section Y positions and heights for section-based backgrounds
  int section_padding = 2; // 2px padding at top and bottom of each section
  int* section_y = (int*)malloc((section_count + 1) * sizeof(int));
  int* section_h = (int*)malloc((section_count + 1) * sizeof(int));
  int current_y_calc = y + cogito_menu.pad_top;
  for (int s = 0; s < section_count; s++) {
    section_y[s
    ] = current_y_calc;
    int items_in_section = last_item_of_section[s
    ] - first_item_of_section[s
    ] + 1;
    int section_height = items_in_section * cogito_menu.item_h;
    // Add gaps between items within section
     if (items_in_section > 1) {
       section_height += (items_in_section - 1) * item_gap;
    }
    // Add 2px padding at top and bottom of section
     section_height += section_padding * 2;
     // Minimum 48px per section for accessibility
     if (section_height < 48) section_height = 48;
     section_h[s
    ] = section_height;
    current_y_calc += section_height;
    // Add section gap or divider space after section (except last)
    if (s < section_count - 1) {
      if (use_divider) {
        current_y_calc += divider_h + item_gap;
      } else {
        current_y_calc += section_gap;
      }
    }
  }
  
  int inner_radius = 4;
  int outer_radius = 12;
  // Draw single shadow for entire menu instead of per-section shadows
  if (shadow_level > 0 && section_count > 0) {
    cogito_draw_shadow_radii_level(x, y + cogito_menu.pad_top, w, h - cogito_menu.pad_top - cogito_menu.pad_bottom, outer_radius, outer_radius, outer_radius, outer_radius, shadow_level);
  }
  // Draw section backgrounds with appropriate radii
  // For bubble style (no dividers): each section is fully rounded
  // For divider style: sections connect with small radii at junctions
  for (int s = 0; s < section_count; s++) {
    int tl = inner_radius, tr = inner_radius, br = inner_radius, bl = inner_radius;
    if (section_count == 1) {
      // Single section or bubble style: use outer radius on all corners
      tl = tr = br = bl = outer_radius;
    } else if (s == 0) {
      // First section: outer radius on top, inner on bottom
      tl = tr = outer_radius;
      br = bl = inner_radius;
    } else if (s == section_count - 1) {
      // Last section: inner radius on top, outer on bottom
      tl = tr = inner_radius;
      br = bl = outer_radius;
    }
    // Middle sections: all inner radius (already set)
    cogito_draw_rect_radii_aa(x, section_y[s
    ], w, section_h[s
    ], bg, tl, tr, br, bl);
  }
  // Draw section borders if needed
  if (has_border && border_w > 0) {
    CogitoColor border_color = vibrant ? tertiary : border;
    for (int s = 0; s < section_count; s++) {
      int tl = inner_radius, tr = inner_radius, br = inner_radius, bl = inner_radius;
      if (section_count == 1) {
        tl = tr = br = bl = outer_radius;
      } else if (s == 0) {
        tl = tr = outer_radius;
        br = bl = inner_radius;
      } else if (s == section_count - 1) {
        tl = tr = inner_radius;
        br = bl = outer_radius;
      }
      cogito_draw_rect_lines_radii(x, section_y[s
      ], w, section_h[s
      ], border_color, tl, tr, br, bl, border_w);
    }
  }
  
  if (cogito_menu.owner->kind == COGITO_DROPDOWN && w > 2) {
    int seam_y = -1;
    if (y == cogito_menu.owner->y + cogito_menu.owner->h) seam_y = y;
    if (y + h == cogito_menu.owner->y) seam_y = y + h - 1;
    if (seam_y >= y && seam_y < y + h) {
      CogitoColor seam_color = vibrant ? tertiary : border;
      cogito_draw_line(x + 1, seam_y, x + w - 2, seam_y, seam_color);
    }
  }
  int mx = 0, my = 0;
  if (cogito_backend && cogito_backend->get_mouse_position) {
    cogito_backend->get_mouse_position(&mx, &my);
  }
  int hover = cogito_menu_hit(&cogito_menu, mx, my);
  
  // Selection styling for sectioned menus
  int sel_inset_edge = 4; // 4px inset from section edges (horizontal)
  int sel_inset_between = 2; // 2px between items (vertical)
  int sel_inset_section_edge = 2; // 2px inset from section top/bottom edges for first/last items
  
  // Calculate item Y positions aligned to sections (accounting for section padding)
  int* item_y = (int*)malloc(max_items * sizeof(int));
  for (int s = 0; s < section_count; s++) {
    int item_idx = first_item_of_section[s
    ];
    int item_y_pos = section_y[s
    ] + section_padding; // Start after section top padding
    int items_count = last_item_of_section[s
    ] - first_item_of_section[s
    ] + 1;
    for (int j = 0; j < items_count; j++) {
      item_y[item_idx + j
      ] = item_y_pos;
      item_y_pos += cogito_menu.item_h + item_gap;
    }
  }
  
  for (size_t i = 0; i < cogito_menu.owner->menu_len; i++) {
    int current_item_y = item_y[i
    ];
    int section_idx = section_of_item[i
    ];
    bool is_first_in_section = (i == (size_t)first_item_of_section[section_idx
    ]);
    bool is_last_in_section = (i == (size_t)last_item_of_section[section_idx
    ]);
    
    if ((int)i == hover) {
      // Calculate selection background with proper insets
      int sel_x = x + sel_inset_edge;
      int sel_w = w - sel_inset_edge * 2;
      int sel_y = current_item_y;
      int sel_h = cogito_menu.item_h;
      
      // Apply insets based on position within section
      if (is_first_in_section) {
        // First item: add 2px padding from section top edge
        sel_y += sel_inset_section_edge;
        sel_h -= sel_inset_section_edge;
      } else {
        // Not first: add 2px inset from item above
        sel_y += sel_inset_between;
        sel_h -= sel_inset_between;
      }
      if (is_last_in_section) {
        // Last item: add 2px padding from section bottom edge
        sel_h -= sel_inset_section_edge;
      } else {
        // Not last: add 2px inset from item below
        sel_h -= sel_inset_between;
      }
      // Calculate selection radius based on position within section
      // Follows section's radius: outer_radius at section edges, inner_radius elsewhere
      int sel_tl = inner_radius, sel_tr = inner_radius, sel_br = inner_radius, sel_bl = inner_radius;
      if (section_count == 1 || !use_divider) {
        // Single section or bubble style: all corners use outer radius
        sel_tl = sel_tr = sel_br = sel_bl = outer_radius;
      } else {
        // Divider style: match section corners at edges
        if (section_idx == 0 && is_first_in_section) {
          sel_tl = sel_tr = outer_radius; // First item in first section
        }
        if (section_idx == section_count - 1 && is_last_in_section) {
          sel_br = sel_bl = outer_radius; // Last item in last section
        }
      }
      // For middle items, use inner_radius (already set)
      
      cogito_draw_rect_radii_aa(sel_x, sel_y, sel_w, sel_h, sel, sel_tl, sel_tr, sel_br, sel_bl);
    }
    const char* text = cogito_menu.owner->menu_labels[i
    ] ? cogito_menu.owner->menu_labels[i
    ]->data : "";
    int th = cogito_text_height_size(font_size);
    int ty = current_item_y + (cogito_menu.item_h - th) / 2;
    // Use on_tertiary for text on selection (always, for both vibrant and non-vibrant)
    CogitoColor item_text_color = ((int)i == hover) ? on_tertiary : text_color;
    cogito_draw_text_size_node(cogito_menu.owner, x + cogito_menu.pad_x, ty, text, item_text_color, font_size, false);
    
    // Check if this item has a section break after it
    bool has_section_after = use_section && cogito_menu.owner->menu_section_after[i
    ];
    
    // Draw divider only if using sections with menu_divider enabled
    if (use_divider && has_section_after && i < cogito_menu.owner->menu_len - 1) {
      int divider_y = current_item_y + cogito_menu.item_h + item_gap;
      CogitoColor divider_color = vibrant ? tertiary : (cogito_theme.menu.has_border ? cogito_theme.menu.border : md3_outline);
      int divider_inset = 12; // Inset from left and right
      cogito_draw_line(x + divider_inset, divider_y, x + w - divider_inset, divider_y, divider_color);
    }
  }
  
  free(item_y);
  
  // Free allocated memory
  free(section_of_item);
  free(first_item_of_section);
  free(last_item_of_section);
  free(section_y);
  free(section_h);
}