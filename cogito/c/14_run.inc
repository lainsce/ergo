#include <SDL3/SDL.h>

static CogitoNode* cogito_find_stepper_hit(CogitoNode* n, int x, int y);

static void cogito_run(ErgoVal appv, ErgoVal winv) {
  if (appv.tag != EVT_OBJ) ergo_trap("cogito.run expects app");
  if (winv.tag != EVT_OBJ) ergo_trap("cogito.run expects window");
  CogitoApp* app = (CogitoApp*)appv.as.p;
  CogitoNode* win = (CogitoNode*)winv.as.p;
  cogito_active_window = win;
  cogito_active_app = app;
  cogito_update_system_theme(app);
  cogito_app_update_theme(app);
  cogito_apply_style_tree(win);
  
  // Initialize SDL3 backend
  if (!cogito_backend) {
    cogito_backend_sdl3_init();
  }
  if (!cogito_backend || !cogito_backend->init()) {
    ergo_trap("Failed to initialize SDL3 backend");
  }
  
  if (win->auto_size) {
    int dw = 0;
    int dh = 0;
    cogito_window_desired_size(win, &dw, &dh);
    if (dw > 0 && dh > 0) {
      win->w = dw;
      win->h = dh;
    }
  }
  
  const char* title = win->text ? win->text->data : "Cogito";
  CogitoNode* appbar = cogito_find_appbar(win);
  
  // Create window via backend
  bool borderless = (appbar != NULL);
#if defined(__APPLE__)
  borderless = false;  // macOS uses native decorations
#endif
  CogitoWindow* window = cogito_backend->window_create(title, win->w, win->h, win->resizable, borderless);
  if (!window) {
    ergo_trap("Failed to create window");
  }
  
  cogito_backend_ready = true;
  
#if defined(__APPLE__)
  cogito_configure_macos_window(appbar != NULL);
  if (win->a11y_label) {
    cogito_macos_set_window_a11y_label(win->a11y_label->data);
  }
#endif

  // Font loading (stubbed for Phase 2)
  const char* font_path = getenv("COGITO_FONT");
  if (!font_path || !font_path[0]) {
    font_path = cogito_default_font_path();
  }
  const char* font_size_env = getenv("COGITO_FONT_SIZE");
  if (font_size_env && font_size_env[0]) {
    int sz = atoi(font_size_env);
    if (sz > 6) cogito_font_size_value = sz;
  }
  
  // Phase 2: Load fonts via backend
  if (font_path && font_path[0]) {
    cogito_font = cogito_backend->font_load(font_path, cogito_font_size());
    if (cogito_font) {
      cogito_font_ready = true;
      cogito_font_loaded_size = cogito_font_size();
      cogito_font_from_env = true;
      cogito_font_path_active = font_path;
    }
  }
  
  if (!cogito_font_ready) {
    // Use stub font - Phase 2 will implement default font
    cogito_font_ready = true;
    cogito_font_loaded_size = cogito_font_size();
    cogito_font_from_env = false;
    cogito_font_path_active = NULL;
  }

  // Mono font
  const char* mono_font_path = getenv("COGITO_FONT_MONO");
  if (!mono_font_path || !mono_font_path[0]) {
    mono_font_path = cogito_guess_mono_font_path(font_path);
  }
  if (mono_font_path && mono_font_path[0]) {
    cogito_font_mono = cogito_backend->font_load(mono_font_path, cogito_font_size());
    if (cogito_font_mono) {
      cogito_font_mono_ready = true;
      cogito_font_mono_loaded_size = cogito_font_size();
      cogito_font_mono_from_env = true;
      cogito_font_mono_path_active = mono_font_path;
    }
  }

  // Bold font
  const char* bold_font_path = getenv("COGITO_FONT_BOLD");
  bool bold_from_regular = false;
  if (!bold_font_path || !bold_font_path[0]) {
    bold_font_path = cogito_guess_bold_font_path(font_path);
    if (!bold_font_path && font_path && font_path[0]) {
      bold_font_path = font_path;
      bold_from_regular = true;
    }
  }
  if (bold_font_path && bold_font_path[0]) {
    cogito_font_bold = cogito_backend->font_load(bold_font_path, cogito_font_size());
    if (cogito_font_bold) {
      cogito_font_bold_ready = true;
      cogito_font_bold_loaded_size = cogito_font_size();
      cogito_font_bold_from_env = true;
      cogito_font_bold_path_active = bold_font_path;
      cogito_font_bold_variable = bold_from_regular;
    }
  }

  // Show window
  cogito_backend->window_show(window);
  
  if (win->auto_size) {
    int dw = 0;
    int dh = 0;
    cogito_window_desired_size(win, &dw, &dh);
    if (dw > 0 && dh > 0) {
      win->w = dw;
      win->h = dh;
      cogito_backend->window_set_size(window, dw, dh);
    }
  }
  cogito_layout_window(win);

  // Main event loop
  while (!cogito_backend->window_should_close(window) && !win->should_close) {
    // Poll events
    cogito_backend->poll_events();
    
    if (cogito_active_app) {
      cogito_update_system_theme(cogito_active_app);
    }
    
    // Get window size
    int sw, sh;
    cogito_backend->window_get_size(window, &sw, &sh);
    if (sw != win->w || sh != win->h) {
      win->w = sw;
      win->h = sh;
      cogito_window_relayout(win);
    }
    
    // Get mouse position
    int mx, my;
    cogito_backend->get_mouse_position(&mx, &my);
    
    // Update stepper hit states for hover effects
    CogitoNode* stepper_hit = cogito_find_stepper_hit(win, mx, my);
    if (stepper_hit) {
      cogito_stepper_hit_test(stepper_hit, mx, my);
    }
    
    float wheel = cogito_backend->get_mouse_wheel_move();
    bool click = cogito_backend->is_mouse_button_pressed(0);  // Left button
    if (cogito_pointer_capture && cogito_backend->is_mouse_button_released(0)) {
      cogito_pointer_capture = NULL;
    }
    
    // Escape key closes menu
    if (cogito_backend->is_key_pressed(SDL_SCANCODE_ESCAPE) && cogito_menu.open) {
      cogito_menu_close();
    }
    
    // Drag handling (non-macOS)
#if !defined(__APPLE__)
    if (cogito_drag_pending) {
      if (cogito_backend->is_mouse_button_down(0)) {
        int dx = mx - (int)cogito_drag_pending_mouse.x;
        int dy = my - (int)cogito_drag_pending_mouse.y;
        if (abs(dx) > 2 || abs(dy) > 2) {
          cogito_drag_active = true;
          cogito_drag_pending = false;
          cogito_drag_start_mouse = cogito_drag_pending_mouse;
          int wx, wy;
          cogito_backend->window_get_position(window, &wx, &wy);
          cogito_drag_start_win.x = (float)wx;
          cogito_drag_start_win.y = (float)wy;
        }
      } else {
        cogito_drag_pending = false;
      }
    }
    if (cogito_drag_active) {
      if (cogito_backend->is_mouse_button_down(0)) {
        float delta_x = (float)mx - cogito_drag_start_mouse.x;
        float delta_y = (float)my - cogito_drag_start_mouse.y;
        double now = cogito_backend->get_time();
        if (now - cogito_drag_last_move > 0.016) {
          int new_x = (int)(cogito_drag_start_win.x + delta_x);
          int new_y = (int)(cogito_drag_start_win.y + delta_y);
          cogito_backend->window_set_position(window, new_x, new_y);
          cogito_drag_last_move = now;
        }
      } else {
        cogito_drag_active = false;
      }
    }
#endif

    // Mouse wheel scrolling
    if (wheel != 0.0f) {
      CogitoNode* slot = (win->dialog && win->dialog->kind == COGITO_DIALOG_SLOT && win->dialog->dialog_open && win->dialog->dialog)
        ? win->dialog
        : cogito_find_dialog_slot(win);
      CogitoNode* root = (slot && slot->dialog) ? slot->dialog : win;
      CogitoNode* scroller = cogito_find_scroller(root, mx, my);
      if (scroller) {
        int delta = (int)(wheel * 32.0f);
        bool horiz = scroller->scroll_horz && (cogito_backend->is_key_down(SDL_SCANCODE_LSHIFT) || 
                                               cogito_backend->is_key_down(SDL_SCANCODE_RSHIFT) || 
                                               !scroller->scroll_vert);
        if (horiz) scroller->scroll_x -= delta;
        else scroller->scroll_y -= delta;
        cogito_window_relayout(win);
      }
    }

    // Tab key navigation
    if (cogito_backend->is_key_pressed(SDL_SCANCODE_TAB)) {
      CogitoNode* slot = (win->dialog && win->dialog->kind == COGITO_DIALOG_SLOT && win->dialog->dialog_open && win->dialog->dialog)
        ? win->dialog
        : cogito_find_dialog_slot(win);
      CogitoNode* root = (slot && slot->dialog) ? slot->dialog : win;
      bool found = false;
      CogitoNode* next = cogito_focus_next(root, cogito_focused, &found);
      if (!next) next = cogito_focus_first(root);
      cogito_focused = next;
    }

    // Text input handling
    if (cogito_focused && !cogito_focused->disabled &&
        (cogito_focused->kind == COGITO_TEXTFIELD || cogito_focused->kind == COGITO_TEXTVIEW ||
         cogito_focused->kind == COGITO_SEARCHFIELD ||
         (cogito_focused->kind == COGITO_COLORPICKER && cogito_focused->color_hex_editing))) {
      bool changed = false;
      int ch = cogito_backend->get_char_pressed();
      while (ch > 0) {
        if ((cogito_focused->kind == COGITO_TEXTFIELD || cogito_focused->kind == COGITO_SEARCHFIELD) && (ch == '\n' || ch == '\r')) {
          ch = cogito_backend->get_char_pressed();
          continue;
        }
        if (cogito_focused->kind == COGITO_COLORPICKER) {
          char c = (char)ch;
          bool ok = (c == '#') || isxdigit((unsigned char)c);
          size_t len = cogito_focused->text ? cogito_focused->text->len : 0;
          size_t sel = 0;
          if (cogito_focused->sel_start != cogito_focused->sel_end) {
            int a = cogito_focused->sel_start;
            int b = cogito_focused->sel_end;
            if (a > b) { int tmp = a; a = b; b = tmp; }
            sel = (size_t)(b - a);
          }
          size_t new_len = len - sel + 1;
          if (ok && new_len <= 7) {
            if (c == '#' && cogito_focused->caret != 0) ok = false;
            if (ok) {
              cogito_text_append_codepoint(cogito_focused, ch);
              changed = true;
            }
          }
        } else {
          cogito_text_append_codepoint(cogito_focused, ch);
          changed = true;
        }
        ch = cogito_backend->get_char_pressed();
      }
      if (cogito_backend->is_key_pressed(SDL_SCANCODE_BACKSPACE)) {
        cogito_text_backspace(cogito_focused);
        changed = true;
      }
      if (changed) {
        if (cogito_focused->kind == COGITO_COLORPICKER) {
          CogitoColor col = {0};
          const char* hex = cogito_focused->text ? cogito_focused->text->data : "";
          if (cogito_hex_to_color(hex, &col)) {
            cogito_rgb_to_hct(col, &cogito_focused->color_h, &cogito_focused->color_c, &cogito_focused->color_t);
          }
        }
        cogito_invoke_change(cogito_focused);
        cogito_window_relayout(win);
      }
    }

    // Slider active handling
    if (cogito_slider_active) {
      if (cogito_backend->is_mouse_button_down(0)) {
        CogitoNode* sld = cogito_slider_active;
        int pad = 8;
        int x0 = sld->x + pad;
        int x1 = sld->x + sld->w - pad;
        if (x1 < x0) x1 = x0;
        double t = (x1 > x0) ? ((double)(mx - x0) / (double)(x1 - x0)) : 0.0;
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;
        double v = sld->slider_min + t * (sld->slider_max - sld->slider_min);
        if (v != sld->slider_value) {
          sld->slider_value = v;
          cogito_invoke_change(sld);
          cogito_window_relayout(win);
        }
      } else {
        if (cogito_pointer_capture == cogito_slider_active) {
          cogito_pointer_capture = NULL;
        }
        cogito_slider_active = NULL;
      }
    }

    // Color picker active handling
    if (cogito_colorpicker_active) {
      if (cogito_backend->is_mouse_button_down(0)) {
        CogitoNode* cp = cogito_colorpicker_active;
        int chan = cogito_colorpicker_channel;
        if (chan >= 0) {
          if (cogito_colorpicker_set_from_mouse(cp, chan, mx)) {
            cogito_invoke_change(cp);
            cogito_window_relayout(win);
          }
        }
      } else {
        if (cogito_pointer_capture == cogito_colorpicker_active) {
          cogito_pointer_capture = NULL;
        }
        cogito_colorpicker_active = NULL;
        cogito_colorpicker_channel = -1;
      }
    }

    // Text selection handling
    if (cogito_focused && !cogito_focused->disabled &&
        (cogito_focused->kind == COGITO_TEXTFIELD || cogito_focused->kind == COGITO_TEXTVIEW ||
         cogito_focused->kind == COGITO_SEARCHFIELD ||
         (cogito_focused->kind == COGITO_COLORPICKER && cogito_focused->color_hex_editing))) {
      if (cogito_backend->is_mouse_button_down(0) && cogito_focused->selecting) {
        int pad = 8;
        int size = cogito_node_font_size(cogito_focused);
        const char* t = cogito_focused->text ? cogito_focused->text->data : "";
        if (cogito_focused->kind == COGITO_TEXTFIELD || cogito_focused->kind == COGITO_SEARCHFIELD) {
          int sf_pad = cogito_focused->kind == COGITO_SEARCHFIELD ? (12 + 16 + 8) : pad;
          int rel_x = mx - (cogito_focused->x + sf_pad);
          if (rel_x < 0) rel_x = 0;
          int idx = cogito_text_index_from_x(cogito_focused, t, size, rel_x);
          cogito_focused->sel_end = idx;
          cogito_focused->caret = idx;
        } else {
          if (cogito_focused->kind == COGITO_TEXTVIEW) {
            int rel_x = mx - (cogito_focused->x + pad);
            if (rel_x < 0) rel_x = 0;
            int line_h = cogito_text_height_size(size);
            int idx = cogito_text_line_index_from_pos(cogito_focused, t, size, cogito_focused->w - pad * 2, cogito_focused->y + pad, line_h, rel_x, my);
            cogito_focused->sel_end = idx;
            cogito_focused->caret = idx;
          } else if (cogito_focused->kind == COGITO_COLORPICKER) {
            int hx = 0, hy = 0, hw = 0, hh = 0;
            cogito_colorpicker_hex_rect(cogito_focused, &hx, &hy, &hw, &hh);
            int rel_x = mx - (hx + pad);
            if (rel_x < 0) rel_x = 0;
            int idx = cogito_text_index_from_x(cogito_focused, t, size, rel_x);
            cogito_focused->sel_end = idx;
            cogito_focused->caret = idx;
          }
        }
      }
      if (cogito_backend->is_mouse_button_released(0)) {
        cogito_focused->selecting = false;
      }
    }

    // Click handling
    if (click) {
      static int cogito_debug_hit = -1;
      if (cogito_debug_hit < 0) {
        const char* env = getenv("COGITO_DEBUG_HIT");
        cogito_debug_hit = (env && env[0] && env[0] != '0') ? 1 : 0;
      }
      if (cogito_menu.open) {
        int idx = cogito_menu_hit(&cogito_menu, mx, my);
        if (idx >= 0 && cogito_menu.owner) {
          cogito_invoke_menu(cogito_menu.owner, (size_t)idx);
        }
        cogito_menu_close();
      } else {
        bool handled = false;
        // Check for traffic light clicks on the first appbar
        if (appbar) {
          if (appbar->appbar_btn_close_x >= 0 &&
              cogito_hit_rect(mx, my, appbar->appbar_btn_close_x, appbar->appbar_btn_y, appbar->appbar_btn_size, appbar->appbar_btn_size)) {
            cogito_appbar_window_action(win, 0);
            handled = true;
          } else if (appbar->appbar_btn_min_x >= 0 &&
                     cogito_hit_rect(mx, my, appbar->appbar_btn_min_x, appbar->appbar_btn_y, appbar->appbar_btn_size, appbar->appbar_btn_size)) {
            cogito_appbar_window_action(win, 1);
            handled = true;
          } else if (appbar->appbar_btn_max_x >= 0 &&
                     cogito_hit_rect(mx, my, appbar->appbar_btn_max_x, appbar->appbar_btn_y, appbar->appbar_btn_size, appbar->appbar_btn_size)) {
            cogito_appbar_window_action(win, 2);
            handled = true;
          }
        }
        // Check if click is on ANY appbar for dragging (only if we didn't handle a button click)
        if (!handled) {
          CogitoNode* clicked_appbar = cogito_find_appbar_at_point(win, mx, my);
          if (clicked_appbar && !cogito_point_over_appbar_buttons(clicked_appbar, mx, my)) {
#if defined(__APPLE__)
            cogito_macos_drag_window();
#else
            cogito_drag_pending = true;
            cogito_drag_pending_mouse.x = (float)mx;
            cogito_drag_pending_mouse.y = (float)my;
            cogito_drag_last_move = 0.0;
#endif
            handled = true;  // Mark as handled to skip clickable search
          }
        }
        CogitoNode* slot = (win->dialog && win->dialog->kind == COGITO_DIALOG_SLOT && win->dialog->dialog_open && win->dialog->dialog)
          ? win->dialog
          : cogito_find_dialog_slot(win);
        CogitoNode* hit = NULL;
        if (!handled) {
          hit = (slot && slot->dialog) ? cogito_find_clickable(slot->dialog, mx, my)
                                       : (win->dialog_open && win->dialog ? cogito_find_clickable(win->dialog, mx, my)
                                                                         : cogito_find_clickable(win, mx, my));
        }
        if (cogito_debug_hit) {
          if (slot && slot->dialog) {
            fprintf(stderr, "cogito: click (%d,%d) dialog rect (%d,%d %dx%d) hit=%p kind=%d\n",
                    mx, my, slot->dialog->x, slot->dialog->y, slot->dialog->w, slot->dialog->h,
                    (void*)hit, hit ? (int)hit->kind : -1);
          } else if (win->dialog_open && win->dialog) {
            fprintf(stderr, "cogito: click (%d,%d) legacy dialog rect (%d,%d %dx%d) hit=%p kind=%d\n",
                    mx, my, win->dialog->x, win->dialog->y, win->dialog->w, win->dialog->h,
                    (void*)hit, hit ? (int)hit->kind : -1);
          } else {
            fprintf(stderr, "cogito: click (%d,%d) no dialog hit=%p kind=%d\n",
                    mx, my, (void*)hit, hit ? (int)hit->kind : -1);
          }
          if (hit) {
            const char* t = (hit->text && hit->text->data) ? hit->text->data : "";
            fprintf(stderr, "cogito:   hit rect (%d,%d %dx%d) text=\"%s\"\n",
                    hit->x, hit->y, hit->w, hit->h, t);
          }
          fflush(stderr);
        }
        if (hit && hit->disabled) hit = NULL;
        CogitoNode* root = (slot && slot->dialog) ? slot->dialog : (win->dialog_open && win->dialog ? win->dialog : win);
        CogitoNode* focus_hit = cogito_find_text_input(root, mx, my);
        if (focus_hit && focus_hit->disabled) focus_hit = NULL;
        if (focus_hit) {
          if (cogito_focused && cogito_focused != focus_hit && cogito_focused->kind == COGITO_COLORPICKER) {
            cogito_focused->color_hex_editing = false;
          }
          cogito_focused = focus_hit;
          int pad = 8;
          int size = cogito_node_font_size(focus_hit);
          const char* t = focus_hit->text ? focus_hit->text->data : "";
          if (focus_hit->kind == COGITO_TEXTFIELD || focus_hit->kind == COGITO_SEARCHFIELD) {
            int sf_pad = focus_hit->kind == COGITO_SEARCHFIELD ? (12 + 16 + 8) : pad;
            int rel_x = mx - (focus_hit->x + sf_pad);
            if (rel_x < 0) rel_x = 0;
            focus_hit->caret = cogito_text_index_from_x(focus_hit, t, size, rel_x);
            focus_hit->sel_start = focus_hit->caret;
            focus_hit->sel_end = focus_hit->caret;
            focus_hit->selecting = true;
          } else if (focus_hit->kind == COGITO_TEXTVIEW) {
            int rel_x = mx - (focus_hit->x + pad);
            if (rel_x < 0) rel_x = 0;
            int line_h = cogito_text_height_size(size);
            focus_hit->caret = cogito_text_line_index_from_pos(focus_hit, t, size, focus_hit->w - pad * 2, focus_hit->y + pad, line_h, rel_x, my);
            focus_hit->sel_start = focus_hit->caret;
            focus_hit->sel_end = focus_hit->caret;
            focus_hit->selecting = true;
          } else if (focus_hit->kind == COGITO_COLORPICKER) {
            int hx = 0, hy = 0, hw = 0, hh = 0;
            cogito_colorpicker_hex_rect(focus_hit, &hx, &hy, &hw, &hh);
            focus_hit->color_hex_editing = cogito_hit_rect(mx, my, hx, hy, hw, hh);
            int rel_x = mx - (hx + pad);
            if (rel_x < 0) rel_x = 0;
            focus_hit->caret = cogito_text_index_from_x(focus_hit, t, size, rel_x);
            focus_hit->sel_start = focus_hit->caret;
            focus_hit->sel_end = focus_hit->caret;
            focus_hit->selecting = true;
          }
        } else if (hit && cogito_node_focusable(hit)) {
          if (cogito_focused && cogito_focused != hit && cogito_focused->kind == COGITO_COLORPICKER) {
            cogito_focused->color_hex_editing = false;
          }
          cogito_focused = hit;
        } else {
          if (cogito_focused && cogito_focused->kind == COGITO_COLORPICKER) {
            cogito_focused->color_hex_editing = false;
          }
          cogito_focused = NULL;
        }
        if (hit) {
          if (hit->kind == COGITO_BUTTON || hit->kind == COGITO_ICONBTN) {
            if (hit->menu_len > 0) {
              cogito_menu_open_for(hit);
            } else {
              cogito_invoke_click(hit);
            }
          } else if (hit->kind == COGITO_CHECKBOX || hit->kind == COGITO_SWITCH) {
            bool prev = hit->checked;
            if (hit->kind == COGITO_CHECKBOX && hit->group.tag != EVT_NULL) {
              hit->checked = true;
              cogito_uncheck_group(win, hit, hit->group);
            } else {
              hit->checked = !hit->checked;
            }
            if (hit->checked && hit->group.tag != EVT_NULL) {
              cogito_uncheck_group(win, hit, hit->group);
            }
            if (!hit->checked) {
              hit->bg_set = false;
              hit->border_color_set = false;
              hit->border_width_set = false;
            }
            if (hit->checked != prev) {
              cogito_invoke_change(hit);
            }
          } else if (hit->kind == COGITO_DROPDOWN) {
            cogito_menu_open_for(hit);
          } else if (hit->kind == COGITO_SLIDER) {
            cogito_slider_active = hit;
            cogito_pointer_capture = hit;
          } else if (hit->kind == COGITO_TABS) {
            int idx = cogito_tabs_hit_index(hit, mx, my);
            if (idx >= 0) {
              hit->selected = idx;
              if (hit->view_switcher && hit->tab_ids && (size_t)idx < hit->tab_len) {
                CogitoNode* vs = hit->view_switcher;
                ErgoStr* id = hit->tab_ids[idx];
                if (id) {
                  if (vs->view_active_id) ergo_release_val(EV_STR(vs->view_active_id));
                  vs->view_active_id = id;
                  ergo_retain_val(EV_STR(vs->view_active_id));
                }
              }
              cogito_invoke_change(hit);
              cogito_window_relayout(win);
            }
          } else if (hit->kind == COGITO_DATEPICKER) {
            int header_hit = cogito_datepicker_hit_header(hit, mx, my);
            if (header_hit < 0) {
              cogito_datepicker_shift_month(hit, -1);
            } else if (header_hit > 0) {
              cogito_datepicker_shift_month(hit, 1);
            } else {
              int day = cogito_datepicker_hit_day(hit, mx, my);
              if (day > 0) hit->date_day = day;
            }
            cogito_invoke_change(hit);
            cogito_window_relayout(win);
          } else if (hit->kind == COGITO_COLORPICKER) {
            int chan = cogito_colorpicker_hit_slider(hit, mx, my);
            if (chan >= 0) {
              cogito_colorpicker_active = hit;
              cogito_colorpicker_channel = chan;
              cogito_pointer_capture = hit;
              if (cogito_colorpicker_set_from_mouse(hit, chan, mx)) {
                cogito_invoke_change(hit);
                cogito_window_relayout(win);
              }
            }
          } else if (hit->kind == COGITO_TOAST) {
            int cx = 0, cy = 0, cw = 0, ch = 0;
            cogito_toast_close_rect(hit, &cx, &cy, &cw, &ch);
            if (cw > 0 && ch > 0 && cogito_hit_rect(mx, my, cx, cy, cw, ch)) {
              CogitoNode* parent = hit->parent;
              if (parent) {
                cogito_container_remove_child(parent, hit);
                cogito_window_relayout(win);
              }
            } else if (hit->action_text && hit->action_button_w > 0 &&
                      cogito_hit_rect(mx, my, hit->action_button_x, hit->action_button_y,
                                     hit->action_button_w, hit->action_button_h)) {
              if (hit->on_action) {
                ErgoVal arg = EV_OBJ(hit);
                cogito_invoke(hit->on_action, 1, &arg);
              }
            } else {
              cogito_invoke_click(hit);
            }
          } else if (hit->kind == COGITO_CHIP) {
            int cx = 0, cy = 0, cw = 0, ch = 0;
            cogito_chip_close_rect(hit, &cx, &cy, &cw, &ch);
            if (cw > 0 && ch > 0 && cogito_hit_rect(mx, my, cx, cy, cw, ch)) {
              // Clicked on close button
              if (hit->on_action) {
                ErgoVal arg = EV_OBJ(hit);
                cogito_invoke(hit->on_action, 1, &arg);
              }
            } else {
              // Clicked on chip body - toggle selected state
              hit->chip_selected = !hit->chip_selected;
              cogito_invoke_click(hit);
              cogito_window_relayout(win);
            }
          } else if (hit->kind == COGITO_FAB) {
            cogito_invoke_click(hit);
          } else if (hit->kind == COGITO_STEPPER) {
            cogito_stepper_click(hit, mx, my);
          } else if (hit->kind == COGITO_SEGMENTED) {
            cogito_segmented_click(hit, mx, my);
            cogito_window_relayout(win);
          } else if (hit->kind == COGITO_NAV_RAIL) {
            int idx = cogito_nav_rail_hit_index(hit, mx, my);
            if (idx >= 0) {
              cogito_handle_nav_rail_select(hit, idx);
              cogito_window_relayout(win);
            }
          } else if (hit->kind == COGITO_BOTTOM_NAV) {
            int idx = cogito_bottom_nav_hit_index(hit, mx, my);
            if (idx >= 0) {
              cogito_handle_bottom_nav_select(hit, idx);
              cogito_window_relayout(win);
            }
          }
        }
        if (slot && slot->dialog) {
          cogito_handle_click(slot->dialog, mx, my, true);
        } else if (win->dialog_open && win->dialog) {
          cogito_handle_click(win->dialog, mx, my, true);
        } else {
          cogito_handle_click(win, mx, my, true);
        }
      }
    }

    // Rendering
    if (cogito_drag_active) {
      cogito_backend->begin_frame(window);
      cogito_backend->end_frame(window);
      cogito_backend->present(window);
      continue;
    }
    
    cogito_backend->begin_frame(window);
    
    CogitoColor win_bg = win->bg_set ? win->bg : cogito_rgba(245, 245, 245, 255);
    cogito_backend->clear(win_bg);
    
    cogito_tooltip_hovered = NULL;
    cogito_draw_node(win);
    if (cogito_menu.open) cogito_draw_menu();
    if (!cogito_menu.open) {
      CogitoNode* slot = (win->dialog && win->dialog->kind == COGITO_DIALOG_SLOT && win->dialog->dialog_open && win->dialog->dialog)
        ? win->dialog
        : cogito_find_dialog_slot(win);
      CogitoNode* root = (slot && slot->dialog) ? slot->dialog : win;
      if (root != win) {
        // Modal dialog open: don't show tooltips from background layers.
        cogito_tooltip_hovered = NULL;
      }
      if (cogito_tooltip_hovered) {
        cogito_draw_tooltip(win, cogito_tooltip_hovered);
      } else {
        CogitoNode* tip = cogito_find_tooltip_node(root, mx, my);
        if (tip) {
          cogito_draw_tooltip(win, tip);
        }
      }
    }
    
    cogito_backend->end_frame(window);
    cogito_backend->present(window);
  }
  
  // Cleanup
  cogito_font_cache_clear();
  if (cogito_font_ready && cogito_font_from_env && cogito_font) {
    cogito_backend->font_unload(cogito_font);
  }
  if (cogito_font_mono_ready && cogito_font_mono_from_env && cogito_font_mono) {
    cogito_backend->font_unload(cogito_font_mono);
  }
  if (cogito_font_bold_ready && cogito_font_bold_from_env && cogito_font_bold) {
    cogito_backend->font_unload(cogito_font_bold);
  }
  
  cogito_backend->window_destroy(window);
  cogito_backend_ready = false;
  cogito_menu_close();
}
