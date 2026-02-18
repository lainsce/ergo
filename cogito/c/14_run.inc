#include <SDL3/SDL.h>

static CogitoNode* cogito_find_stepper_hit(CogitoNode* n, int x, int y);

#include "widgets/util/text_editing.inc"

static void cogito_run(ErgoVal appv, ErgoVal winv) {
  if (appv.tag != EVT_OBJ) ergo_trap("cogito.run expects app");
  if (winv.tag != EVT_OBJ) ergo_trap("cogito.run expects window");
  CogitoApp* app = (CogitoApp*)appv.as.p;
  CogitoNode* win = (CogitoNode*)winv.as.p;
  const char* title = (win->text && win->text->data && win->text->data[0]) ? win->text->data : COGITO_USER_VERSION;
  const char* app_id = (app && app->app_id && app->app_id->data && app->app_id->data[0]) ? app->app_id->data : NULL;
  const char* app_name = (app && app->app_name && app->app_name->data && app->app_name->data[0]) ? app->app_name->data : title;
  cogito_active_window = win;
  cogito_active_app = app;
  cogito_update_system_theme(app);
  cogito_app_update_theme(app);
  cogito_apply_style_tree(win);
  if (cogito_debug_style_enabled_internal()) {
    cogito_style_dump_tree_internal(win, 0);
  }
  (void)SDL_SetHint(SDL_HINT_APP_NAME, app_name);
  if (app_id && app_id[0]) {
    (void)SDL_SetHint(SDL_HINT_APP_ID, app_id);
  }
  (void)SDL_SetAppMetadata(app_name, NULL, app_id ? app_id : app_name);
#if defined(__APPLE__)
  cogito_macos_set_process_name(app_name);
#endif
  
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
  
  CogitoNode* appbar = cogito_find_appbar(win);
  
  // Create window via backend
  bool borderless = (appbar != NULL);
#if defined(__APPLE__)
  borderless = false;  // macOS uses native decorations
#endif
  CogitoWindow* window = cogito_backend->window_create(title, win->w, win->h, win->resizable, borderless, true);
  if (!window) {
    ergo_trap("Failed to create window");
  }
  cogito_active_backend_window = window;

  // Set window icon: explicit app icon or auto-detect from script dir
  if (cogito_backend->window_set_icon) {
    const char* icon_path = cogito_app_get_icon_path(app);
    char resolved_icon[PATH_MAX];
    bool icon_set = false;
    if (icon_path) {
      if (cogito_image_resolve_path(icon_path, resolved_icon, sizeof(resolved_icon))) {
        icon_set = cogito_backend->window_set_icon(window, resolved_icon);
      }
    }
    if (!icon_set && cogito_script_dir[0]) {
      // Auto-detect icon file in script directory
      static const char* icon_names[] = {"icon.png", "icon.svg", "icon.jpg", "icon.bmp", NULL};
      for (int i = 0; icon_names[i]; i++) {
        if (cogito_image_resolve_path(icon_names[i], resolved_icon, sizeof(resolved_icon))) {
          if (cogito_backend->window_set_icon(window, resolved_icon)) break;
        }
      }
    }
  }

  cogito_backend_ready = true;
  {
    const char* env_inspector = getenv("COGITO_INSPECTOR");
    cogito_inspector_enabled = true;
    cogito_inspector_open = env_inspector && env_inspector[0] && env_inspector[0] != '0';
    cogito_inspector_selected = win;
    cogito_inspector_view_x = 0;
    cogito_inspector_view_y = 0;
    cogito_inspector_view_w = 0;
    cogito_inspector_view_h = 0;
  }
  
#if defined(__APPLE__)
  cogito_configure_macos_window(appbar != NULL);
  bool macos_app_name_pending = !cogito_macos_apply_app_name(app_name);
  if (win->a11y_label) {
    cogito_macos_set_window_a11y_label(win->a11y_label->data);
  }
#else
  bool macos_app_name_pending = false;
#endif

  const char* font_path = getenv("COGITO_FONT");
  if (!font_path || !font_path[0]) {
    font_path = cogito_default_font_path();
  }
  const char* font_size_env = getenv("COGITO_FONT_SIZE");
  if (font_size_env && font_size_env[0]) {
    int sz = atoi(font_size_env);
    if (sz > 6) cogito_font_size_value = sz;
  }
  
  if (font_path && font_path[0]) {
    if (cogito_load_font_file(&cogito_font, font_path, cogito_font_size(), cogito_font_weight_normal())) {
      cogito_font_ready = true;
      cogito_font_loaded_size = cogito_font_size();
      cogito_font_from_env = true;
      cogito_font_path_active = font_path;
    }
  }
  
  if (!cogito_font_ready) {
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
    if (cogito_load_font_file(&cogito_font_mono, mono_font_path, cogito_font_size(), cogito_font_weight_normal())) {
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
    bool weight_applied = false;
    int bold_weight = bold_from_regular ? cogito_font_weight_bold() : 0;
    if (cogito_load_font_file_ex(&cogito_font_bold, bold_font_path, cogito_font_size(), bold_weight, &weight_applied)) {
      if (bold_from_regular && !weight_applied) {
        // Variable font weight not applied -- fall back to faux bold
        cogito_font_bold_variable = true;
      } else {
        cogito_font_bold_variable = false;
      }
      cogito_font_bold_ready = true;
      cogito_font_bold_loaded_size = cogito_font_size();
      cogito_font_bold_from_env = true;
      cogito_font_bold_path_active = bold_font_path;
    }
  }

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
  // Second layout pass: images loaded during the first pass now have their
  // real dimensions available for intrinsic sizing.  This replaces the old
  // recursive cogito_window_relayout calls that used to happen inside the
  // layout pass (one per image), collapsing O(N) nested passes into two.
  cogito_layout_window(win);
  CogitoWindow* inspector_window = NULL;
  const int inspector_w = 520;
  const int inspector_h = 680;
  const int inspector_appbar_h = 52;
  const int inspector_corner_r = 18;
#if defined(__APPLE__)
  const int inspector_shadow_margin = 0;
#else
  const int inspector_shadow_margin = 18;
#endif
  int inspector_traffic_btn = 14;
  int inspector_traffic_gap = 9;
  int inspector_traffic_pad_top = 9;
  int inspector_traffic_pad_x = 9;
  cogito_appbar_button_metrics(&inspector_traffic_btn, &inspector_traffic_gap, &inspector_traffic_pad_top, &inspector_traffic_pad_x);
  if (inspector_traffic_btn < 2) inspector_traffic_btn = 2;
  if (inspector_traffic_gap < 0) inspector_traffic_gap = 0;
  if (inspector_traffic_pad_top < 0) inspector_traffic_pad_top = 0;
  if (inspector_traffic_pad_x < 0) inspector_traffic_pad_x = 0;
  int* inspector_hit_test_params = (int*)malloc(6 * sizeof(int));
  if (inspector_hit_test_params) {
    inspector_hit_test_params[0] = inspector_appbar_h;
    inspector_hit_test_params[1] = inspector_shadow_margin;
    inspector_hit_test_params[2] = inspector_traffic_btn;
    inspector_hit_test_params[3] = inspector_traffic_gap;
    inspector_hit_test_params[4] = inspector_traffic_pad_x;
    inspector_hit_test_params[5] = inspector_traffic_pad_top;
  }
  bool inspector_native_drag = false;
  bool inspector_drag_pending = false;
  bool inspector_drag_active = false;
  int inspector_drag_start_gx = 0;
  int inspector_drag_start_gy = 0;
  int inspector_drag_start_wx = 0;
  int inspector_drag_start_wy = 0;
  if (cogito_inspector_open) {
    inspector_window = cogito_backend->window_create(
      "Inspector",
      inspector_w + inspector_shadow_margin * 2,
      inspector_h + inspector_shadow_margin * 2,
      true,
      true,
      false
    );
    if (inspector_window) {
      cogito_backend->window_show(inspector_window);
      if (cogito_backend->window_set_hit_test_callback) {
        cogito_backend->window_set_hit_test_callback(inspector_window, cogito_inspector_hit_test, (void*)inspector_hit_test_params);
        inspector_native_drag = true;
      }
#if defined(__APPLE__)
      cogito_macos_set_window_shadow(inspector_window, true);
#endif
    } else {
      cogito_inspector_open = false;
    }
  }

  #include "widgets/util/carousel_anim.inc"

  // Render the first frame before showing the window so the user never
  // sees a bare black framebuffer while layout/setup is finishing.
  {
    CogitoColor win_bg = win->bg_set ? win->bg : cogito_rgba(245, 245, 245, 255);
    cogito_backend->begin_frame(window);
    cogito_icon_cache_frame_start();  // Initialize icon cache frame counter
    cogito_backend->clear(win_bg);
    cogito_draw_node(win);
    cogito_backend->end_frame(window);
    cogito_backend->present(window);
  }
  cogito_backend->window_show(window);

  bool prev_main_pointer_valid = false;
  int prev_main_mx = 0;
  int prev_main_my = 0;
  bool prev_main_mouse_inside = false;
  bool prev_route_main_pointer = false;
  bool prev_inspector_pointer_known = false;
  int prev_inspector_mx = 0;
  int prev_inspector_my = 0;
  bool prev_inspector_mouse_inside = false;
  bool prev_inspector_mouse_in_frame = false;
  CogitoNode* prev_hover_node = NULL;

  // Main event loop
  while (!cogito_backend->window_should_close(window) && !win->should_close) {
    // On macOS, wrap the entire loop body in an autoreleasepool.
    // SDL3's Metal backend and Cocoa calls create ObjC objects that get
    // autoreleased. Without a pool, they accumulate and leak every iteration.
#if defined(__APPLE__)
    extern void cogito_autorelease_push(void);
    extern void cogito_autorelease_pop(void);
    cogito_autorelease_push();
#endif
    // Poll events (true if any event was processed; used to decide if we need to redraw)
    bool had_events = cogito_backend->poll_events();
#if defined(__APPLE__)
    if (macos_app_name_pending) {
      macos_app_name_pending = !cogito_macos_apply_app_name(app_name);
    }
#endif
    
    if (cogito_active_app) {
      cogito_update_system_theme(cogito_active_app);
    }
    // Only update inspector button metrics when the inspector is actually open
    if (cogito_inspector_open && inspector_window) {
      cogito_appbar_button_metrics(&inspector_traffic_btn, &inspector_traffic_gap, &inspector_traffic_pad_top, &inspector_traffic_pad_x);
      if (inspector_traffic_btn < 2) inspector_traffic_btn = 2;
      if (inspector_traffic_gap < 0) inspector_traffic_gap = 0;
      if (inspector_traffic_pad_top < 0) inspector_traffic_pad_top = 0;
      if (inspector_traffic_pad_x < 0) inspector_traffic_pad_x = 0;
      if (inspector_hit_test_params) {
        inspector_hit_test_params[2] = inspector_traffic_btn;
        inspector_hit_test_params[3] = inspector_traffic_gap;
        inspector_hit_test_params[4] = inspector_traffic_pad_x;
        inspector_hit_test_params[5] = inspector_traffic_pad_top;
      }
    }
    if (inspector_window && cogito_backend->window_should_close(inspector_window)) {
      cogito_backend->window_destroy(inspector_window);
      inspector_window = NULL;
      cogito_inspector_open = false;
      if (inspector_hit_test_params) {
        free(inspector_hit_test_params);
        inspector_hit_test_params = NULL;
      }
      inspector_native_drag = false;
      inspector_drag_pending = false;
      inspector_drag_active = false;
    }
    if (cogito_inspector_enabled && cogito_debug_inspector_toggle_pressed(cogito_backend)) {
      cogito_inspector_open = !cogito_inspector_open;
      if (cogito_inspector_open) {
        if (!inspector_window) {
          inspector_window = cogito_backend->window_create(
            "Inspector",
            inspector_w + inspector_shadow_margin * 2,
            inspector_h + inspector_shadow_margin * 2,
            true,
            true,
            false
          );
          if (inspector_window) {
            cogito_backend->window_show(inspector_window);
            if (cogito_backend->window_set_hit_test_callback) {
              cogito_backend->window_set_hit_test_callback(inspector_window, cogito_inspector_hit_test, (void*)inspector_hit_test_params);
              inspector_native_drag = true;
            }
#if defined(__APPLE__)
            cogito_macos_set_window_shadow(inspector_window, true);
#endif
          } else {
            cogito_inspector_open = false;
          }
        }
        if (cogito_inspector_open && !cogito_inspector_selected) cogito_inspector_selected = win;
      } else {
        if (inspector_window) {
          cogito_backend->window_destroy(inspector_window);
          inspector_window = NULL;
        }
        if (inspector_hit_test_params) {
          free(inspector_hit_test_params);
          inspector_hit_test_params = NULL;
        }
        inspector_native_drag = false;
        inspector_drag_pending = false;
        inspector_drag_active = false;
      }
    }
    
    // Get window size
    int sw, sh;
    bool window_resized = false;
    cogito_backend->window_get_size(window, &sw, &sh);
    if (sw != win->w || sh != win->h) {
      win->w = sw;
      win->h = sh;
      cogito_window_relayout(win);
      window_resized = true;
    }
    // GTK-style selective invalidation: only redraw when something visual changed.
    // Mouse motion alone doesn't trigger redraw — hover-change detection below handles it.
    extern bool cogito_last_poll_had_non_motion;
    if (window_resized) cogito_queue_draw(win);
    if (had_events && cogito_last_poll_had_non_motion) cogito_queue_draw(win);

    // Get mouse position in the main window coordinate space.
    int mx = 0;
    int my = 0;
    bool main_mouse_inside = false;
    if (cogito_backend->get_mouse_position_in_window) {
      cogito_backend->get_mouse_position_in_window(window, &mx, &my);
      main_mouse_inside = (mx >= 0 && my >= 0 && mx < sw && my < sh);
    } else {
      cogito_backend->get_mouse_position(&mx, &my);
      main_mouse_inside = (mx >= 0 && my >= 0 && mx < sw && my < sh);
    }
    
    float wheel = cogito_backend->get_mouse_wheel_move();
    bool left_down = cogito_backend->is_mouse_button_down(0);
    bool left_pressed = cogito_backend->is_mouse_button_pressed(0);
    bool click = left_pressed;  // Left button
    if (cogito_pointer_capture && cogito_backend->is_mouse_button_released(0)) {
      cogito_pointer_capture = NULL;
    }

    int imx = 0, imy = 0, iw = 0, ih = 0, iwx = 0, iwy = 0;
    bool inspector_mouse_known = false;
    bool inspector_mouse_inside = false;
    int inspector_gx = 0;
    int inspector_gy = 0;
    if (inspector_window && cogito_backend->get_mouse_position_in_window) {
      cogito_backend->get_mouse_position_in_window(inspector_window, &imx, &imy);
      cogito_backend->window_get_size(inspector_window, &iw, &ih);
      cogito_backend->window_get_position(inspector_window, &iwx, &iwy);
      inspector_gx = iwx + imx;
      inspector_gy = iwy + imy;
      inspector_mouse_known = true;
      inspector_mouse_inside = (imx >= 0 && imy >= 0 && imx < iw && imy < ih);
    }
    int inspector_frame_x = inspector_shadow_margin;
    int inspector_frame_y = inspector_shadow_margin;
    int inspector_frame_w = iw - inspector_shadow_margin * 2;
    int inspector_frame_h = ih - inspector_shadow_margin * 2;
    if (inspector_frame_w < 1) inspector_frame_w = 1;
    if (inspector_frame_h < 1) inspector_frame_h = 1;
    bool inspector_mouse_in_frame = inspector_mouse_inside &&
      imx >= inspector_frame_x &&
      imy >= inspector_frame_y &&
      imx < inspector_frame_x + inspector_frame_w &&
      imy < inspector_frame_y + inspector_frame_h;
    bool route_main_pointer = main_mouse_inside && !inspector_mouse_in_frame;
    cogito_main_pointer_x = mx;
    cogito_main_pointer_y = my;
    cogito_main_pointer_valid = main_mouse_inside;
    cogito_main_pointer_routed = route_main_pointer;
    cogito_main_pointer_down = left_down;
    cogito_main_pointer_pressed = left_pressed;

    bool main_pointer_changed = !prev_main_pointer_valid ||
      mx != prev_main_mx || my != prev_main_my ||
      main_mouse_inside != prev_main_mouse_inside ||
      route_main_pointer != prev_route_main_pointer;
    bool inspector_pointer_changed = false;
    if (inspector_window) {
      inspector_pointer_changed = !prev_inspector_pointer_known ||
        inspector_mouse_known != prev_inspector_pointer_known ||
        imx != prev_inspector_mx || imy != prev_inspector_my ||
        inspector_mouse_inside != prev_inspector_mouse_inside ||
        inspector_mouse_in_frame != prev_inspector_mouse_in_frame;
    } else if (prev_inspector_pointer_known || prev_inspector_mouse_inside || prev_inspector_mouse_in_frame) {
      inspector_pointer_changed = true;
    }

    // Update stepper hit states for hover effects only when pointer routing/position changes.
    if (main_pointer_changed && route_main_pointer) {
      CogitoNode* stepper_hit = cogito_find_stepper_hit(win, mx, my);
      if (stepper_hit) {
        cogito_stepper_hit_test(stepper_hit, mx, my);
      }
    }

    // GTK-style hover-change detection: only redraw on mouse motion if the
    // hovered interactive widget changed (enter/leave a button, slider, etc.)
    if (main_pointer_changed) {
      CogitoNode* cur_hover = route_main_pointer
          ? cogito_find_clickable(win, mx, my) : NULL;
      if (cur_hover != prev_hover_node) {
        prev_hover_node = cur_hover;
        cogito_queue_draw(win);
      }
      // Menu hover highlight follows pointer even when underlying clickable node doesn't change.
      if (cogito_menu.open) {
        cogito_queue_draw(win);
      }
      // Overlay inspector (single-window mode) tracks pointer in the main window.
      if (cogito_inspector_open && !inspector_window) {
        cogito_queue_draw(win);
      }
    }
    if (inspector_pointer_changed && cogito_inspector_open && inspector_window) {
      cogito_queue_draw(win);
    }

    prev_main_pointer_valid = true;
    prev_main_mx = mx;
    prev_main_my = my;
    prev_main_mouse_inside = main_mouse_inside;
    prev_route_main_pointer = route_main_pointer;
    if (inspector_window) {
      prev_inspector_pointer_known = inspector_mouse_known;
      prev_inspector_mx = imx;
      prev_inspector_my = imy;
      prev_inspector_mouse_inside = inspector_mouse_inside;
      prev_inspector_mouse_in_frame = inspector_mouse_in_frame;
    } else {
      prev_inspector_pointer_known = false;
      prev_inspector_mouse_inside = false;
      prev_inspector_mouse_in_frame = false;
    }

    int inspector_btn_close_x = -1;
    int inspector_btn_min_x = -1;
    int inspector_btn_max_x = -1;
    int inspector_btn_y = 0;
    CogitoTrafficLightsState inspector_lights = (CogitoTrafficLightsState){0};
    if (inspector_window) {
      int appbar_h = inspector_appbar_h;
      if (appbar_h > inspector_frame_h) appbar_h = inspector_frame_h;
      inspector_btn_y = inspector_frame_y + inspector_traffic_pad_top;
      if (inspector_btn_y + inspector_traffic_btn > inspector_frame_y + appbar_h) {
        inspector_btn_y = inspector_frame_y + (appbar_h - inspector_traffic_btn) / 2;
      }
      inspector_btn_close_x = inspector_frame_x + inspector_traffic_pad_x;
      inspector_btn_min_x = inspector_btn_close_x + inspector_traffic_btn + inspector_traffic_gap;
      inspector_btn_max_x = inspector_btn_min_x + inspector_traffic_btn + inspector_traffic_gap;
      bool down = left_down;
      cogito_traffic_lights_compute(
        inspector_btn_close_x, inspector_btn_min_x, inspector_btn_max_x, inspector_btn_y, inspector_traffic_btn,
        imx, imy, down, &inspector_lights
      );
    }

    if (!inspector_native_drag && inspector_drag_pending) {
      if (cogito_backend->is_mouse_button_down(0)) {
        if (inspector_mouse_known) {
          int dx = inspector_gx - inspector_drag_start_gx;
          int dy = inspector_gy - inspector_drag_start_gy;
          if (abs(dx) > 2 || abs(dy) > 2) {
            inspector_drag_active = true;
            inspector_drag_pending = false;
          }
        }
      } else {
        inspector_drag_pending = false;
      }
    }
    if (!inspector_native_drag && inspector_drag_active) {
      if (cogito_backend->is_mouse_button_down(0)) {
        if (inspector_mouse_known) {
          int dx = inspector_gx - inspector_drag_start_gx;
          int dy = inspector_gy - inspector_drag_start_gy;
          cogito_backend->window_set_position(inspector_window, inspector_drag_start_wx + dx, inspector_drag_start_wy + dy);
        }
      } else {
        inspector_drag_active = false;
      }
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
    if (wheel != 0.0f && route_main_pointer) {
      if (win->popover_open && win->popover) {
        CogitoNode* pop_hit = cogito_find_clickable(win->popover, mx, my);
        if (pop_hit && pop_hit->kind == COGITO_DATEPICKER && pop_hit->datepicker.popover_panel) {
          if (cogito_datepicker_wheel(pop_hit, mx, my, wheel)) {
            cogito_window_relayout(win);
            wheel = 0.0f;
          }
        }
      }

      if (wheel != 0.0f) {
        CogitoNode* slot = (win->dialog && win->dialog->kind == COGITO_DIALOG_SLOT && win->dialog->dialog_open && win->dialog->dialog)
          ? win->dialog
          : cogito_find_dialog_slot(win);
        CogitoNode* root = (slot && slot->dialog) ? slot->dialog : win;
        CogitoNode* scroller = cogito_find_scroller(root, mx, my);
        if (scroller) {
          int delta = (int)(wheel * 32.0f);
          bool horiz = scroller->scroller.horz && (cogito_backend->is_key_down(SDL_SCANCODE_LSHIFT) || 
                                                 cogito_backend->is_key_down(SDL_SCANCODE_RSHIFT) || 
                                                 !scroller->scroller.vert);
          if (horiz) scroller->scroller.scroll_x -= delta;
          else scroller->scroller.scroll_y -= delta;
          cogito_window_relayout(win);
        } else {
          // Check if mouse is over a carousel — scroll it horizontally
          CogitoNode* carousel = cogito_find_carousel(root, mx, my);
          if (carousel && (int)carousel->len > 1) {
            int max_scroll = 0;
            CAROUSEL_MAX_SCROLL(carousel, max_scroll);
            
            // Calculate unit width to check if leftmost item is wide enough for snapping
            int inner_w = carousel->w - carousel->padding_left - carousel->padding_right;
            if (inner_w < 0) inner_w = 0;
            int gaps = ((int)carousel->len >= 3) ? 2 : ((int)carousel->len - 1);
            if (gaps < 0) gaps = 0;
            int avail = inner_w - (gaps * 8);
            if (avail < 0) avail = 0;
            int unit_w = avail / 7;
            
            // Only snap if leftmost (tertiary) item width >= 56px
            bool should_snap = unit_w >= 56;
            
            // Use wheel delta to scroll (horizontal or vertical wheel both work)
            double target_xf = carousel->carousel.scroll_xf - (double)(wheel * 80.0f);
            if (target_xf < 0.0) target_xf = 0.0;
            if (target_xf > (double)max_scroll) target_xf = (double)max_scroll;
            
            int snap_idx = 0;
            int snap_sx = 0;
            if (should_snap) {
              // Snap to nearest item
              CAROUSEL_NEAREST_INDEX(carousel, target_xf, max_scroll, snap_idx);
              CAROUSEL_SNAP_SCROLL(carousel, snap_idx, max_scroll, snap_sx);
            } else {
              // Without snapping: scroll freely to target position
              snap_sx = (int)(target_xf + 0.5);
              // Calculate active index from scroll progress (reverse of CAROUSEL_SNAP_SCROLL)
              if (max_scroll > 0) {
                snap_idx = (int)((target_xf / (double)max_scroll) * (double)((int)carousel->len - 1) + 0.5);
              } else {
                snap_idx = 0;
              }
              if (snap_idx < 0) snap_idx = 0;
              if (snap_idx >= (int)carousel->len) snap_idx = (int)carousel->len - 1;
            }
            
            // Start snap animation only if position actually changes
            carousel->carousel.anim_from = carousel->carousel.scroll_xf;
            carousel->carousel.anim_to = (double)snap_sx;
            if (carousel->carousel.anim_from != carousel->carousel.anim_to) {
              carousel->carousel.anim_start = cogito_backend->get_time();
              if (!carousel->carousel.animating) cogito_carousel_animating_count++;
              carousel->carousel.animating = true;
            } else {
              carousel->carousel.animating = false;
              carousel->carousel.scroll_xf = carousel->carousel.anim_to;
            }
            carousel->carousel.active_index = snap_idx;
          }
        }
      }
    }

    /* Inspector window: wheel scrolls widget tree when cursor over panel */
    if (inspector_window && wheel != 0.0f && inspector_mouse_in_frame && imy >= inspector_frame_y + inspector_appbar_h) {
      int appbar_h = inspector_appbar_h;
      if (appbar_h > inspector_frame_h) appbar_h = inspector_frame_h;
      cogito_inspector_view_x = inspector_frame_x;
      cogito_inspector_view_y = inspector_frame_y + appbar_h;
      cogito_inspector_view_w = inspector_frame_w;
      cogito_inspector_view_h = inspector_frame_h - appbar_h - inspector_corner_r;
      if (cogito_inspector_view_h < 0) cogito_inspector_view_h = 0;
      if (cogito_inspector_handle_wheel(win, imx, imy, wheel)) {
        wheel = 0.0f;
        cogito_queue_draw(win);
      }
      cogito_inspector_view_x = 0;
      cogito_inspector_view_y = 0;
      cogito_inspector_view_w = 0;
      cogito_inspector_view_h = 0;
    }

    // Tab key navigation
    if (cogito_backend->is_key_pressed(SDL_SCANCODE_TAB)) {
      CogitoNode* slot = (win->dialog && win->dialog->kind == COGITO_DIALOG_SLOT && win->dialog->dialog_open && win->dialog->dialog)
        ? win->dialog
        : cogito_find_dialog_slot(win);
      CogitoNode* root = (slot && slot->dialog) ? slot->dialog : win;
      bool reverse = cogito_backend->is_key_down(SDL_SCANCODE_LSHIFT) || cogito_backend->is_key_down(SDL_SCANCODE_RSHIFT);
      bool found = false;
      CogitoNode* next = reverse
        ? cogito_focus_prev(root, cogito_focused, &found)
        : cogito_focus_next(root, cogito_focused, &found);
      if (!next) next = reverse ? cogito_focus_last(root) : cogito_focus_first(root);
      if (cogito_focused && cogito_focused != next && cogito_focused->kind == COGITO_COLORPICKER) {
        cogito_focused->colorpicker.hex_editing = false;
      }
      cogito_focused = next;
    }

    // Text input handling
    bool focused_text_editable = cogito_focused &&
      (cogito_focused->kind == COGITO_TEXTFIELD || cogito_focused->kind == COGITO_TEXTVIEW || cogito_focused->kind == COGITO_SEARCHFIELD) &&
      cogito_focused->editable;
    bool focused_color_hex_edit = cogito_focused &&
      cogito_focused->kind == COGITO_COLORPICKER && cogito_focused->colorpicker.hex_editing;
    if (cogito_focused && !cogito_focused->disabled && (focused_text_editable || focused_color_hex_edit)) {
      bool changed = false;
      bool caret_changed = false;
      int ch = cogito_backend->get_char_pressed();
      while (ch > 0) {
        if (ch == '\t') {
          ch = cogito_backend->get_char_pressed();
          continue;
        }
        if ((cogito_focused->kind == COGITO_TEXTFIELD || cogito_focused->kind == COGITO_SEARCHFIELD) && (ch == '\n' || ch == '\r')) {
          ch = cogito_backend->get_char_pressed();
          continue;
        }
        if (cogito_focused->kind == COGITO_TEXTVIEW && (ch == '\n' || ch == '\r')) {
          ch = cogito_backend->get_char_pressed();
          continue;
        }
        if (cogito_focused->kind == COGITO_COLORPICKER) {
          char c = (char)ch;
          bool ok = (c == '#') || isxdigit((unsigned char)c);
          size_t len = cogito_focused->text ? cogito_focused->text->len : 0;
          size_t sel = 0;
          if (cogito_focused->text_input.sel_start != cogito_focused->text_input.sel_end) {
            int a = cogito_focused->text_input.sel_start;
            int b = cogito_focused->text_input.sel_end;
            if (a > b) { int tmp = a; a = b; b = tmp; }
            sel = (size_t)(b - a);
          }
          size_t new_len = len - sel + 1;
          if (ok && new_len <= 7) {
            if (c == '#' && cogito_focused->text_input.caret != 0) ok = false;
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
      if (cogito_backend->is_key_pressed(SDL_SCANCODE_LEFT)) {
        if (cogito_text_move_caret_h(cogito_focused, -1)) caret_changed = true;
      }
      if (cogito_backend->is_key_pressed(SDL_SCANCODE_RIGHT)) {
        if (cogito_text_move_caret_h(cogito_focused, +1)) caret_changed = true;
      }
      if (cogito_focused->kind == COGITO_TEXTVIEW && cogito_backend->is_key_pressed(SDL_SCANCODE_UP)) {
        if (cogito_textview_move_caret_v(cogito_focused, -1)) caret_changed = true;
      }
      if (cogito_focused->kind == COGITO_TEXTVIEW && cogito_backend->is_key_pressed(SDL_SCANCODE_DOWN)) {
        if (cogito_textview_move_caret_v(cogito_focused, +1)) caret_changed = true;
      }
      if (cogito_focused->kind == COGITO_TEXTVIEW &&
          (cogito_backend->is_key_pressed(SDL_SCANCODE_RETURN) ||
           cogito_backend->is_key_pressed(SDL_SCANCODE_KP_ENTER))) {
        cogito_text_append_codepoint(cogito_focused, '\n');
        changed = true;
      }
      if (changed || caret_changed) {
        if (cogito_focused->kind == COGITO_COLORPICKER) {
          CogitoColor col = {0};
          const char* hex = cogito_focused->text ? cogito_focused->text->data : "";
          if (cogito_hex_to_color(hex, &col)) {
            cogito_rgb_to_hct(col, &cogito_focused->colorpicker.h, &cogito_focused->colorpicker.c, &cogito_focused->colorpicker.t);
          }
        }
        if (changed) {
          cogito_invoke_change(cogito_focused);
        }
        cogito_window_relayout(win);
      }
    }

    // Slider active handling
    if (cogito_slider_active) {
      if (cogito_backend->is_mouse_button_down(0)) {
        CogitoNode* sld = cogito_slider_active;
        int rail_h = 16;
        if (sld->slider.size == COGITO_SLIDER_SIZE_S) rail_h = 24;
        if (sld->slider.size == COGITO_SLIDER_SIZE_M) rail_h = 40;
        if (sld->slider.size == COGITO_SLIDER_SIZE_L) rail_h = 56;
        if (sld->slider.size == COGITO_SLIDER_SIZE_XL) rail_h = 96;
        int pad = rail_h / 2 + 7;
        int x0 = sld->x + pad;
        int x1 = sld->x + sld->w - pad;
        if (x1 < x0) x1 = x0;
        double t = (x1 > x0) ? ((double)(mx - x0) / (double)(x1 - x0)) : 0.0;
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;
        double v = sld->slider.min + t * (sld->slider.max - sld->slider.min);
        bool changed = false;
        if (sld->slider.mode == COGITO_SLIDER_MODE_RANGE) {
          double prev0 = sld->slider.value;
          double prev1 = sld->slider.value2;
          if (sld->slider.active_handle == 2) {
            sld->slider.value2 = v;
            if (sld->slider.value2 < sld->slider.value) sld->slider.value2 = sld->slider.value;
          } else {
            sld->slider.value = v;
            if (sld->slider.value > sld->slider.value2) sld->slider.value = sld->slider.value2;
          }
          changed = (sld->slider.value != prev0) || (sld->slider.value2 != prev1);
        } else {
          if (v != sld->slider.value) {
            sld->slider.value = v;
            sld->slider.value2 = v;
            changed = true;
          }
        }
        if (changed) {
          cogito_invoke_change(sld);
          cogito_window_relayout(win);
        }
      } else {
        if (cogito_pointer_capture == cogito_slider_active) {
          cogito_pointer_capture = NULL;
        }
        cogito_slider_active->slider.dragging = false;
        cogito_slider_active = NULL;
      }
    }

    // Carousel active handling (drag-to-scroll with velocity tracking)
    if (cogito_carousel_active) {
      if (cogito_backend->is_mouse_button_down(0)) {
        CogitoNode* cr = cogito_carousel_active;
        int dx = mx - cr->carousel.drag_start_x;
        double new_xf = (double)cr->carousel.drag_start_scroll - (double)dx;
        
        int max_scroll = 0;
        CAROUSEL_MAX_SCROLL(cr, max_scroll);
        
        if (new_xf < 0.0) new_xf = 0.0;
        if (new_xf > (double)max_scroll) new_xf = (double)max_scroll;
        
        // Track velocity: compare with last sample
        double now = cogito_backend->get_time();
        double dt = now - cr->carousel.drag_last_time;
        if (dt > 0.001) {
          double dxf = new_xf - cr->carousel.drag_last_xf;
          // Exponential smoothing of velocity (px/sec)
          float inst_vel = (float)(dxf / dt);
          cr->carousel.velocity = cr->carousel.velocity * 0.6f + inst_vel * 0.4f;
          cr->carousel.drag_last_time = now;
          cr->carousel.drag_last_xf = new_xf;
        }
        
        cr->carousel.scroll_xf = new_xf;
        int new_scroll = (int)(new_xf + 0.5);
        if (new_scroll != cr->carousel.scroll_x) {
          cr->carousel.scroll_x = new_scroll;
          cogito_window_relayout(win);
        }
        
        if (cogito_backend && cogito_backend->set_cursor) {
          cogito_backend->set_cursor(COGITO_CURSOR_GRABBING);
        }
      } else {
        // --- Release: start momentum + snap animation ---
        CogitoNode* cr = cogito_carousel_active;
        int max_scroll = 0;
        CAROUSEL_MAX_SCROLL(cr, max_scroll);
        
        // Calculate unit width to check if leftmost item is wide enough for snapping
        int inner_w = cr->w - cr->padding_left - cr->padding_right;
        if (inner_w < 0) inner_w = 0;
        int gaps = ((int)cr->len >= 3) ? 2 : ((int)cr->len - 1);
        if (gaps < 0) gaps = 0;
        int avail = inner_w - (gaps * 8);
        if (avail < 0) avail = 0;
        int unit_w = avail / 7;
        
        // Only snap if leftmost (tertiary) item width >= 56px
        bool should_snap = unit_w >= 56;
        
        // Project where momentum would carry us (velocity * decay_time)
        double projected = cr->carousel.scroll_xf + (double)cr->carousel.velocity * 0.4;
        if (projected < 0.0) projected = 0.0;
        if (projected > (double)max_scroll) projected = (double)max_scroll;
        
        int snap_idx = 0;
        int snap_sx = 0;
        if (should_snap) {
          // Snap to nearest item
          CAROUSEL_NEAREST_INDEX(cr, projected, max_scroll, snap_idx);
          CAROUSEL_SNAP_SCROLL(cr, snap_idx, max_scroll, snap_sx);
        } else {
          // Without snapping: momentum carries us to projected position
          snap_sx = (int)(projected + 0.5);
          // Calculate active index from scroll progress (reverse of CAROUSEL_SNAP_SCROLL)
          if (max_scroll > 0) {
            snap_idx = (int)((projected / (double)max_scroll) * (double)((int)cr->len - 1) + 0.5);
          } else {
            snap_idx = 0;
          }
          if (snap_idx < 0) snap_idx = 0;
          if (snap_idx >= (int)cr->len) snap_idx = (int)cr->len - 1;
        }
        
        // Start animation from current position to snap target (only if position changes)
        cr->carousel.anim_from = cr->carousel.scroll_xf;
        cr->carousel.anim_to = (double)snap_sx;
        if (cr->carousel.anim_from != cr->carousel.anim_to) {
          cr->carousel.anim_start = cogito_backend->get_time();
          if (!cr->carousel.animating) cogito_carousel_animating_count++;
          cr->carousel.animating = true;
        } else {
          cr->carousel.animating = false;
          cr->carousel.scroll_xf = cr->carousel.anim_to;
        }
        cr->carousel.active_index = snap_idx;
        
        if (cogito_pointer_capture == cr) {
          cogito_pointer_capture = NULL;
        }
        cr->carousel.dragging = false;
        cogito_carousel_active = NULL;
        
        if (cogito_backend && cogito_backend->set_cursor) {
          cogito_backend->set_cursor(COGITO_CURSOR_DEFAULT);
        }
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

    // Time picker dial drag handling
    if (cogito_timepicker_active) {
      if (cogito_backend->is_mouse_button_down(0)) {
        if (cogito_timepicker_drag_update(cogito_timepicker_active, mx, my)) {
          cogito_window_relayout(win);
        }
      } else {
        if (cogito_pointer_capture == cogito_timepicker_active) {
          cogito_pointer_capture = NULL;
        }
        cogito_timepicker_active = NULL;
      }
    }

    // ViewDual drag handling
    if (cogito_pointer_capture && cogito_pointer_capture->kind == COGITO_VIEW_DUAL) {
      CogitoNode* vd = cogito_pointer_capture;
      if (cogito_backend->is_mouse_button_down(0)) {
        int handle_w = 4;
        const int min_item_w = 218;
        if (vd->w > 0) {
          int available_w = vd->w - handle_w;
          if (available_w < 0) available_w = 0;
          int new_handle_x = mx - vd->view_dual.drag_offset;
          int new_left_w = new_handle_x - vd->x;
          if (available_w >= min_item_w * 2) {
            int max_left = available_w - min_item_w;
            if (new_left_w < min_item_w) new_left_w = min_item_w;
            if (new_left_w > max_left) new_left_w = max_left;
          } else {
            new_left_w = available_w / 2;
          }
          if (new_left_w < 0) new_left_w = 0;
          if (new_left_w > available_w) new_left_w = available_w;
          float new_ratio = (float)(new_left_w + handle_w / 2) / (float)vd->w;
          if (new_ratio < 0.0f) new_ratio = 0.0f;
          if (new_ratio > 1.0f) new_ratio = 1.0f;
          if (new_ratio != vd->view_dual.ratio) {
            vd->view_dual.ratio = new_ratio;
            cogito_window_relayout(win);
          }
        }
      } else {
        vd->view_dual.dragging = false;
        cogito_pointer_capture = NULL;
      }
    }

    // Text selection handling
    if (cogito_focused && !cogito_focused->disabled && (focused_text_editable || focused_color_hex_edit)) {
      if (cogito_backend->is_mouse_button_down(0) && cogito_focused->text_input.selecting) {
        int pad = 8;
        int size = cogito_node_font_size(cogito_focused);
        const char* t = cogito_focused->text ? cogito_focused->text->data : "";
        if (cogito_focused->kind == COGITO_TEXTFIELD || cogito_focused->kind == COGITO_SEARCHFIELD) {
          int sf_pad = cogito_focused->kind == COGITO_SEARCHFIELD ? (12 + 16 + 8) : pad;
          int rel_x = mx - (cogito_focused->x + sf_pad);
          if (rel_x < 0) rel_x = 0;
          int idx = cogito_text_index_from_x(cogito_focused, t, size, rel_x);
          cogito_focused->text_input.sel_end = idx;
          cogito_focused->text_input.caret = idx;
        } else {
          if (cogito_focused->kind == COGITO_TEXTVIEW) {
            int rel_x = mx - (cogito_focused->x + pad);
            if (rel_x < 0) rel_x = 0;
            int line_h = cogito_text_height_size(size);
            int idx = cogito_text_line_index_from_pos(cogito_focused, t, size, cogito_focused->w - pad * 2, cogito_focused->y + pad, line_h, rel_x, my);
            cogito_focused->text_input.sel_end = idx;
            cogito_focused->text_input.caret = idx;
          } else if (cogito_focused->kind == COGITO_COLORPICKER) {
            int hx = 0, hy = 0, hw = 0, hh = 0;
            cogito_colorpicker_hex_rect(cogito_focused, &hx, &hy, &hw, &hh);
            int rel_x = mx - (hx + pad);
            if (rel_x < 0) rel_x = 0;
            int idx = cogito_text_index_from_x(cogito_focused, t, size, rel_x);
            cogito_focused->text_input.sel_end = idx;
            cogito_focused->text_input.caret = idx;
          }
        }
      }
      if (cogito_backend->is_mouse_button_released(0)) {
        cogito_focused->text_input.selecting = false;
      }
    }

    /* Scrollbar thumb drag and track click (overlay scrollbars) */
    if (cogito_scrollbar_drag_scroller) {
      if (!left_down) {
        cogito_scrollbar_drag_scroller = NULL;
      } else {
        CogitoNode* sc = cogito_scrollbar_drag_scroller;
        int cur = cogito_scrollbar_drag_vert ? my : mx;
        int delta = cur - cogito_scrollbar_drag_start_mouse;
        int new_scroll = cogito_scrollbar_drag_start_scroll + (int)((long)delta * (long)cogito_scrollbar_drag_max / (long)cogito_scrollbar_drag_travel);
        if (new_scroll < 0) new_scroll = 0;
        if (new_scroll > cogito_scrollbar_drag_max) new_scroll = cogito_scrollbar_drag_max;
        if (cogito_scrollbar_drag_vert) sc->scroller.scroll_y = new_scroll;
        else sc->scroller.scroll_x = new_scroll;
        cogito_window_relayout(win);
        cogito_queue_draw(win);
      }
    } else if (left_pressed && route_main_pointer) {
      CogitoNode* slot = (win->dialog && win->dialog->kind == COGITO_DIALOG_SLOT && win->dialog->dialog_open && win->dialog->dialog)
        ? win->dialog
        : cogito_find_dialog_slot(win);
      CogitoNode* root = (slot && slot->dialog) ? slot->dialog : win;
      CogitoNode* sc = cogito_find_scroller(root, mx, my);
      if (sc) {
        CogitoScrollbarHit hit = cogito_scroller_hit_scrollbar(sc, mx, my, COGITO_SCROLLBAR_WIDTH_WIDE, COGITO_SCROLLBAR_WIDTH_WIDE);
        if (hit == COGITO_SCROLLBAR_HIT_VERT_THUMB) {
          int max_s = 0, travel = 1;
          if (cogito_scroller_vert_drag_params(sc, &max_s, &travel)) {
            cogito_scrollbar_drag_scroller = sc;
            cogito_scrollbar_drag_vert = true;
            cogito_scrollbar_drag_start_mouse = my;
            cogito_scrollbar_drag_start_scroll = sc->scroller.scroll_y;
            cogito_scrollbar_drag_max = max_s;
            cogito_scrollbar_drag_travel = travel;
            click = false;
          }
        } else if (hit == COGITO_SCROLLBAR_HIT_HORZ_THUMB) {
          int max_s = 0, travel = 1;
          if (cogito_scroller_horz_drag_params(sc, &max_s, &travel)) {
            cogito_scrollbar_drag_scroller = sc;
            cogito_scrollbar_drag_vert = false;
            cogito_scrollbar_drag_start_mouse = mx;
            cogito_scrollbar_drag_start_scroll = sc->scroller.scroll_x;
            cogito_scrollbar_drag_max = max_s;
            cogito_scrollbar_drag_travel = travel;
            click = false;
          }
        } else if (hit == COGITO_SCROLLBAR_HIT_VERT_TRACK) {
          int vtx, vty, vtw, vth, vux, vuy, vuw, vuh;
          if (cogito_scroller_vert_scrollbar_rects(sc, COGITO_SCROLLBAR_WIDTH_WIDE, &vtx, &vty, &vtw, &vth, &vux, &vuy, &vuw, &vuh)) {
            int max_s = 0, travel = 1;
            if (cogito_scroller_vert_drag_params(sc, &max_s, &travel)) {
              sc->scroller.scroll_y = cogito_scroller_track_to_scroll(my, vty, vth, vuh, max_s);
              if (sc->scroller.scroll_y < 0) sc->scroller.scroll_y = 0;
              if (sc->scroller.scroll_y > max_s) sc->scroller.scroll_y = max_s;
              cogito_window_relayout(win);
              cogito_queue_draw(win);
              click = false;
            }
          }
        } else if (hit == COGITO_SCROLLBAR_HIT_HORZ_TRACK) {
          int htx, hty, htw, hth, hux, huy, huw, huh;
          if (cogito_scroller_horz_scrollbar_rects(sc, COGITO_SCROLLBAR_WIDTH_WIDE, &htx, &hty, &htw, &hth, &hux, &huy, &huw, &huh)) {
            int max_s = 0, travel = 1;
            if (cogito_scroller_horz_drag_params(sc, &max_s, &travel)) {
              sc->scroller.scroll_x = cogito_scroller_track_to_scroll(mx, htx, htw, huw, max_s);
              if (sc->scroller.scroll_x < 0) sc->scroller.scroll_x = 0;
              if (sc->scroller.scroll_x > max_s) sc->scroller.scroll_x = max_s;
              cogito_window_relayout(win);
              cogito_queue_draw(win);
              click = false;
            }
          }
        }
      }
    }

    // Click handling
    if (click) {
      if (cogito_inspector_open) {
        bool consumed = false;
        if (inspector_window) {
          if (inspector_mouse_in_frame) {
            int appbar_h = inspector_appbar_h;
            if (appbar_h > inspector_frame_h) appbar_h = inspector_frame_h;
            if (imy < inspector_frame_y + appbar_h) {
              if (inspector_lights.over_close) {
                cogito_backend->window_destroy(inspector_window);
                inspector_window = NULL;
                cogito_inspector_open = false;
                inspector_native_drag = false;
                inspector_drag_pending = false;
                inspector_drag_active = false;
              } else if (inspector_lights.over_min) {
                if (cogito_backend->window_minimize) {
                  cogito_backend->window_minimize(inspector_window);
                }
              } else if (inspector_lights.over_max) {
                if (cogito_backend->window_is_maximized && cogito_backend->window_restore && cogito_backend->window_maximize) {
                  if (cogito_backend->window_is_maximized(inspector_window)) cogito_backend->window_restore(inspector_window);
                  else cogito_backend->window_maximize(inspector_window);
                } else if (cogito_backend->window_maximize) {
                  cogito_backend->window_maximize(inspector_window);
                }
              } else if (!inspector_native_drag) {
                inspector_drag_pending = true;
                inspector_drag_active = false;
                inspector_drag_start_gx = inspector_gx;
                inspector_drag_start_gy = inspector_gy;
                inspector_drag_start_wx = iwx;
                inspector_drag_start_wy = iwy;
              }
              consumed = true;
            } else {
              cogito_inspector_view_x = inspector_frame_x;
              cogito_inspector_view_y = inspector_frame_y + appbar_h;
              cogito_inspector_view_w = inspector_frame_w;
              if (cogito_inspector_view_w < 0) cogito_inspector_view_w = 0;
              cogito_inspector_view_h = inspector_frame_h - appbar_h - inspector_corner_r;
              if (cogito_inspector_view_h < 0) cogito_inspector_view_h = 0;
              (void)cogito_inspector_handle_click(win, imx, imy);
              consumed = true;
              cogito_inspector_view_x = 0;
              cogito_inspector_view_y = 0;
              cogito_inspector_view_w = 0;
              cogito_inspector_view_h = 0;
            }
          } else if (inspector_mouse_inside) {
            consumed = true;
          }
        } else {
          if (cogito_inspector_handle_click(win, mx, my)) {
            consumed = true;
          }
        }
        if (consumed) {
          click = false;
        } else {
          CogitoNode* picked = cogito_inspector_pick_node_recursive(win, mx, my);
          if (picked) cogito_inspector_selected = picked;
        }
      }
    }
    if (click && !route_main_pointer) {
      click = false;
    }
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
        CogitoNode* slot = (win->dialog && win->dialog->kind == COGITO_DIALOG_SLOT && win->dialog->dialog_open && win->dialog->dialog)
          ? win->dialog
          : cogito_find_dialog_slot(win);
        CogitoNode* active_dialog = (slot && slot->dialog)
          ? slot->dialog
          : ((win->dialog_open && win->dialog && win->dialog->kind == COGITO_DIALOG) ? win->dialog : NULL);
        if (win->popover_open && win->popover) {
          bool inside_popover = cogito_hit_node(win->popover, mx, my);
          bool inside_anchor = (win->popover_anchor && cogito_hit_node(win->popover_anchor, mx, my));
          if (!inside_popover && !inside_anchor) {
            cogito_popover_close(EV_OBJ(win));
          }
        }
        // Check for traffic light clicks on the first appbar
        if (!active_dialog && appbar) {
          if (appbar->appbar.btn_close_x >= 0 &&
              cogito_hit_rect(mx, my, appbar->appbar.btn_close_x, appbar->appbar.btn_y, appbar->appbar.btn_size, appbar->appbar.btn_size)) {
            cogito_appbar_window_action(win, 0);
            handled = true;
          } else if (appbar->appbar.btn_min_x >= 0 &&
                     cogito_hit_rect(mx, my, appbar->appbar.btn_min_x, appbar->appbar.btn_y, appbar->appbar.btn_size, appbar->appbar.btn_size)) {
            cogito_appbar_window_action(win, 1);
            handled = true;
          } else if (appbar->appbar.btn_max_x >= 0 &&
                     cogito_hit_rect(mx, my, appbar->appbar.btn_max_x, appbar->appbar.btn_y, appbar->appbar.btn_size, appbar->appbar.btn_size)) {
            cogito_appbar_window_action(win, 2);
            handled = true;
          }
        }
        // Check if click is on ANY appbar for dragging (only if we didn't handle a button click)
        if (!handled && !active_dialog) {
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
        if (!handled && active_dialog) {
          int dx = 0, dy = 0, dw = 0, dh = 0;
          cogito_dialog_close_rect(active_dialog, &dx, &dy, &dw, &dh);
          if (dw > 0 && dh > 0 && cogito_hit_rect(mx, my, dx, dy, dw, dh)) {
            if (slot && slot->kind == COGITO_DIALOG_SLOT) {
              cogito_dialog_slot_clear(EV_OBJ(slot));
            } else {
              cogito_dialog_close(EV_OBJ(active_dialog));
            }
            handled = true;
          }
        }
        CogitoNode* hit = NULL;
        if (!handled) {
          CogitoNode* root = (slot && slot->dialog_open && slot->dialog) ? slot->dialog : (win->dialog_open && win->dialog ? win->dialog : win);
          hit = cogito_find_overlay_toast(root, mx, my);
          if (!hit && win->popover_open && win->popover) {
            hit = cogito_find_clickable(win->popover, mx, my);
          }
          if (!hit) {
            hit = (slot && slot->dialog_open && slot->dialog) ? cogito_find_clickable(slot->dialog, mx, my)
                                         : (win->dialog_open && win->dialog ? cogito_find_clickable(win->dialog, mx, my)
                                                                           : cogito_find_clickable(win, mx, my));
          }
        }
        if (cogito_debug_hit) {
          if (slot && slot->dialog_open && slot->dialog) {
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
        CogitoNode* root = (slot && slot->dialog_open && slot->dialog) ? slot->dialog : (win->dialog_open && win->dialog ? win->dialog : win);
        CogitoNode* focus_hit = NULL;
        if (win->popover_open && win->popover) {
          focus_hit = cogito_find_text_input(win->popover, mx, my);
        }
        if (!focus_hit) {
          focus_hit = cogito_find_text_input(root, mx, my);
        }
        if (focus_hit && focus_hit->disabled) focus_hit = NULL;
        if (focus_hit) {
          if (cogito_focused && cogito_focused != focus_hit && cogito_focused->kind == COGITO_COLORPICKER) {
            cogito_focused->colorpicker.hex_editing = false;
          }
          cogito_focused = focus_hit;
          int pad = 8;
          int size = cogito_node_font_size(focus_hit);
          const char* t = focus_hit->text ? focus_hit->text->data : "";
          if (focus_hit->kind == COGITO_TEXTFIELD || focus_hit->kind == COGITO_SEARCHFIELD) {
            int sf_pad = focus_hit->kind == COGITO_SEARCHFIELD ? (12 + 16 + 8) : pad;
            int rel_x = mx - (focus_hit->x + sf_pad);
            if (rel_x < 0) rel_x = 0;
            focus_hit->text_input.caret = cogito_text_index_from_x(focus_hit, t, size, rel_x);
            focus_hit->text_input.sel_start = focus_hit->text_input.caret;
            focus_hit->text_input.sel_end = focus_hit->text_input.caret;
            focus_hit->text_input.selecting = true;
          } else if (focus_hit->kind == COGITO_TEXTVIEW) {
            int rel_x = mx - (focus_hit->x + pad);
            if (rel_x < 0) rel_x = 0;
            int line_h = cogito_text_height_size(size);
            focus_hit->text_input.caret = cogito_text_line_index_from_pos(focus_hit, t, size, focus_hit->w - pad * 2, focus_hit->y + pad, line_h, rel_x, my);
            focus_hit->text_input.sel_start = focus_hit->text_input.caret;
            focus_hit->text_input.sel_end = focus_hit->text_input.caret;
            focus_hit->text_input.selecting = true;
          } else if (focus_hit->kind == COGITO_COLORPICKER) {
            int hx = 0, hy = 0, hw = 0, hh = 0;
            cogito_colorpicker_hex_rect(focus_hit, &hx, &hy, &hw, &hh);
            focus_hit->colorpicker.hex_editing = cogito_hit_rect(mx, my, hx, hy, hw, hh);
            int rel_x = mx - (hx + pad);
            if (rel_x < 0) rel_x = 0;
            focus_hit->text_input.caret = cogito_text_index_from_x(focus_hit, t, size, rel_x);
            focus_hit->text_input.sel_start = focus_hit->text_input.caret;
            focus_hit->text_input.sel_end = focus_hit->text_input.caret;
            focus_hit->text_input.selecting = true;
          }
        } else if (hit && cogito_node_focusable(hit)) {
          if (cogito_focused && cogito_focused != hit && cogito_focused->kind == COGITO_COLORPICKER) {
            cogito_focused->colorpicker.hex_editing = false;
          }
          cogito_focused = hit;
        } else {
          if (cogito_focused && cogito_focused->kind == COGITO_COLORPICKER) {
            cogito_focused->colorpicker.hex_editing = false;
          }
          cogito_focused = NULL;
        }
        if (hit) {
          // Trigger ripple for Primary-colored interactables
          if (hit->kind == COGITO_BUTTON || hit->kind == COGITO_FAB || hit->kind == COGITO_ICONBTN) {
            cogito_ripple_start(hit, mx, my);
          }
          if (hit->kind == COGITO_BUTTON || hit->kind == COGITO_ICONBTN) {
            if (hit->menu_len > 0) {
              cogito_menu_open_for(hit);
            } else {
              cogito_invoke_click(hit);
            }
          } else if (hit->kind == COGITO_SPLIT_BUTTON) {
            int arrow_x = hit->split_btn.arrow_x;
            if (mx >= arrow_x && hit->menu_len > 0) {
              cogito_menu_open_for(hit);
            } else {
              cogito_invoke_click(hit);
            }
          } else if (hit->kind == COGITO_VIEW_CHOOSER) {
            cogito_menu_open_for(hit);
          } else if (hit->kind == COGITO_CHECKBOX || hit->kind == COGITO_SWITCH || hit->kind == COGITO_SWITCHBAR) {
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
            hit->slider.dragging = true;
            if (hit->slider.mode == COGITO_SLIDER_MODE_RANGE) {
              double span = hit->slider.max - hit->slider.min;
              int rail_h = 16;
              if (hit->slider.size == COGITO_SLIDER_SIZE_S) rail_h = 24;
              if (hit->slider.size == COGITO_SLIDER_SIZE_M) rail_h = 40;
              if (hit->slider.size == COGITO_SLIDER_SIZE_L) rail_h = 56;
              if (hit->slider.size == COGITO_SLIDER_SIZE_XL) rail_h = 96;
              int pad = rail_h / 2 + 7;
              int x0 = hit->x + pad;
              int x1 = hit->x + hit->w - pad;
              if (x1 < x0) x1 = x0;
              double t0 = 0.0;
              double t1 = 0.0;
              if (span > 0.0) {
                t0 = (hit->slider.value - hit->slider.min) / span;
                t1 = (hit->slider.value2 - hit->slider.min) / span;
              }
              if (t0 < 0.0) t0 = 0.0;
              if (t0 > 1.0) t0 = 1.0;
              if (t1 < 0.0) t1 = 0.0;
              if (t1 > 1.0) t1 = 1.0;
              int k0 = x0 + (int)lround((double)(x1 - x0) * t0);
              int k1 = x0 + (int)lround((double)(x1 - x0) * t1);
              int d0 = abs(mx - k0);
              int d1 = abs(mx - k1);
              if (d1 < d0) {
                hit->slider.active_handle = 2;
              } else if (d0 < d1) {
                hit->slider.active_handle = 1;
              } else {
                hit->slider.active_handle = (mx >= (k0 + k1) / 2) ? 2 : 1;
              }
            } else {
              hit->slider.active_handle = 0;
            }
          } else if (hit->kind == COGITO_CAROUSEL_ITEM) {
            // Click on a carousel item: scroll to it and make it active
            CogitoNode* item = hit;
            CogitoNode* carousel = item->parent;
            if (carousel && carousel->kind == COGITO_CAROUSEL) {
              int max_scroll = 0;
              CAROUSEL_MAX_SCROLL(carousel, max_scroll);
              
              // Find the index of the clicked item
              int item_idx = -1;
              for (size_t i = 0; i < carousel->len; i++) {
                if (carousel->children[i] == item) {
                  item_idx = (int)i;
                  break;
                }
              }
              
              if (item_idx >= 0) {
                // Calculate target scroll position to make this item active
                int snap_sx = 0;
                CAROUSEL_SNAP_SCROLL(carousel, item_idx, max_scroll, snap_sx);
                
                // Start animation to scroll to this item
                carousel->carousel.anim_from = carousel->carousel.scroll_xf;
                carousel->carousel.anim_to = (double)snap_sx;
                if (carousel->carousel.anim_from != carousel->carousel.anim_to) {
                  carousel->carousel.anim_start = cogito_backend->get_time();
                  if (!carousel->carousel.animating) cogito_carousel_animating_count++;
                  carousel->carousel.animating = true;
                }
                carousel->carousel.active_index = item_idx;
                cogito_window_relayout(win);
              }
            }
          } else if (hit->kind == COGITO_CAROUSEL) {
            cogito_carousel_active = hit;
            cogito_pointer_capture = hit;
            hit->carousel.dragging = true;
            hit->carousel.animating = false;
            hit->carousel.drag_start_x = mx;
            hit->carousel.drag_start_scroll = (int)hit->carousel.scroll_xf;
            hit->carousel.velocity = 0.0f;
            hit->carousel.drag_last_time = cogito_backend->get_time();
            hit->carousel.drag_last_xf = hit->carousel.scroll_xf;
            // Set cursor to grab
            if (cogito_backend && cogito_backend->set_cursor) {
              cogito_backend->set_cursor(COGITO_CURSOR_GRAB);
            }
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
            cogito_datepicker_click(hit, mx, my);
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
              hit->chip.selected = !hit->chip.selected;
              cogito_invoke_click(hit);
              cogito_window_relayout(win);
            }
          } else if (hit->kind == COGITO_FAB) {
            cogito_invoke_click(hit);
          } else if (hit->kind == COGITO_STEPPER) {
            cogito_stepper_click(hit, mx, my);
          } else if (hit->kind == COGITO_TIMEPICKER) {
            cogito_timepicker_click(hit, mx, my);
          } else if (hit->kind == COGITO_SEGMENTED) {
            cogito_segmented_click(hit, mx, my);
            cogito_window_relayout(win);
          } else if (hit->kind == COGITO_TREEVIEW) {
            if (cogito_treeview_click(hit, mx, my)) {
              cogito_window_relayout(win);
            }
          } else if (hit->kind == COGITO_NAV_RAIL) {
            if (cogito_nav_rail_hit_toggle(hit, mx, my)) {
              hit->nav_rail.expanded = !hit->nav_rail.expanded;
              cogito_window_relayout(win);
            } else {
              int idx = cogito_nav_rail_hit_index(hit, mx, my);
              if (idx >= 0) {
                cogito_handle_nav_rail_select(hit, idx);
                cogito_window_relayout(win);
              }
            }
          } else if (hit->kind == COGITO_BOTTOM_NAV) {
            int idx = cogito_bottom_nav_hit_index(hit, mx, my);
            if (idx >= 0) {
              cogito_handle_bottom_nav_select(hit, idx);
              cogito_window_relayout(win);
            }
          } else if (hit->kind == COGITO_SETTINGS_WINDOW) {
            int sidebar_w = 200;
            if (mx >= hit->x && mx < hit->x + sidebar_w && my >= hit->y && my < hit->y + hit->h) {
              int item_h = 40;
              int idx = (my - hit->y - 48) / item_h;
              if (idx >= 0 && idx < (int)hit->len) {
                hit->selected = idx;
                cogito_window_relayout(win);
              }
            }
          } else if (hit->kind == COGITO_EMPTY_PAGE || hit->kind == COGITO_WELCOME_SCREEN || hit->kind == COGITO_ABOUT_WINDOW) {
            if (hit->action_text && hit->action_text->data && hit->action_text->data[0]) {
              if (hit->action_button_w > 0 && cogito_hit_rect(mx, my, hit->action_button_x, hit->action_button_y,
                          hit->action_button_w, hit->action_button_h)) {
                if (hit->on_action) {
                  ErgoVal arg = EV_OBJ(hit);
                  cogito_invoke(hit->on_action, 1, &arg);
                } else if (hit->kind == COGITO_ABOUT_WINDOW && cogito_backend && cogito_backend->open_url) {
                  cogito_backend->open_url(hit->action_text->data);
                }
              }
            }
          } else if (hit->kind == COGITO_VIEW_DUAL) {
            float ratio = hit->view_dual.ratio;
            if (ratio <= 0.0f) ratio = 0.5f;
            if (ratio > 1.0f) ratio = 1.0f;
            int handle_w = 4;
            const int min_item_w = 218;
            int available_w = hit->w - handle_w;
            if (available_w < 0) available_w = 0;
            int left_w = (int)(hit->w * ratio) - handle_w / 2;
            if (available_w >= min_item_w * 2) {
              int max_left = available_w - min_item_w;
              if (left_w < min_item_w) left_w = min_item_w;
              if (left_w > max_left) left_w = max_left;
            } else {
              left_w = available_w / 2;
            }
            if (left_w < 0) left_w = 0;
            if (left_w > available_w) left_w = available_w;
            int handle_x = hit->x + left_w;
            if (cogito_hit_rect(mx, my, handle_x - 4, hit->y, handle_w + 8, hit->h)) {
              hit->view_dual.dragging = true;
              hit->view_dual.drag_offset = mx - handle_x;
              cogito_pointer_capture = hit;
            }
          }
        }
        if (slot && slot->dialog_open && slot->dialog) {
          cogito_handle_click(slot->dialog, mx, my, true);
        } else if (win->dialog_open && win->dialog) {
          cogito_handle_click(win->dialog, mx, my, true);
        } else {
          cogito_handle_click(win, mx, my, true);
        }
      }
    }

    // --- Carousel animation tick ---
    // Only traverse the tree when at least one carousel is animating.
    if (cogito_carousel_animating_count > 0) {
      CogitoNode* _stk[64];
      int _sp = 0;
      _stk[_sp++] = win;
      bool _carousel_did_relayout = false;
      double _now = cogito_backend->get_time();
      while (_sp > 0) {
        CogitoNode* _cn = _stk[--_sp];
        if (!_cn) continue;
        if (_cn->kind == COGITO_CAROUSEL && _cn->carousel.animating) {
          double t = (_now - _cn->carousel.anim_start) / CAROUSEL_ANIM_DURATION;
          if (t >= 1.0) {
            t = 1.0;
            _cn->carousel.animating = false;
            cogito_carousel_animating_count--;
          }
          // ease-out quad: t * (2 - t)
          double et = t * (2.0 - t);
          double sx = _cn->carousel.anim_from + (_cn->carousel.anim_to - _cn->carousel.anim_from) * et;
          _cn->carousel.scroll_xf = sx;
          int isx = (int)(sx + 0.5);
          if (isx != _cn->carousel.scroll_x) {
            _cn->carousel.scroll_x = isx;
            _carousel_did_relayout = true;
          }
        }
        for (size_t _ci = 0; _ci < _cn->len && _sp < 63; _ci++) {
          _stk[_sp++] = _cn->children[_ci];
        }
      }
      if (_carousel_did_relayout) {
        cogito_window_relayout(win);
      }
      // Safety: clamp to zero if counter drifted
      if (cogito_carousel_animating_count < 0) cogito_carousel_animating_count = 0;
    }

    // Only redraw when something requested it (queue_draw) or continuous updates needed.
    // Do not redraw during window drag — we only move the window; no present() needed, so we wait for events and avoid CPU spike.
    bool window_being_dragged = cogito_drag_active || cogito_drag_pending 
        || inspector_drag_active || inspector_drag_pending;
    double now = cogito_backend->get_time();
    bool anim_running = now < cogito_anim_end_time;
    bool need_redraw = !window_being_dragged && (cogito_draw_pending(win) || anim_running
        || (cogito_carousel_animating_count > 0));

    // Debug redraw: set COGITO_DEBUG_REDRAW=1 to see why each frame redraws
    {
      static int _dbg_init = -1;
      if (_dbg_init < 0) { const char* _e = getenv("COGITO_DEBUG_REDRAW"); _dbg_init = (_e && _e[0] && _e[0] != '0') ? 1 : 0; }
      if (_dbg_init) {
        static int _dbg_frame = 0;
        static int _dbg_redraw_streak = 0;
        _dbg_frame++;
        if (need_redraw) {
          _dbg_redraw_streak++;
          if (_dbg_redraw_streak <= 3 || _dbg_redraw_streak % 60 == 0) {
            fprintf(stderr, "[redraw] frame=%d streak=%d reason:", _dbg_frame, _dbg_redraw_streak);
            if (cogito_draw_pending(win)) fprintf(stderr, " draw_pending");
            if (anim_running) fprintf(stderr, " anim(%.0fms)", (cogito_anim_end_time - now) * 1000.0);
            if (cogito_carousel_animating_count > 0) fprintf(stderr, " carousel(%d)", cogito_carousel_animating_count);
            fprintf(stderr, "\n");
          }
        } else {
          if (_dbg_redraw_streak > 3) {
            fprintf(stderr, "[redraw] streak ended after %d frames\n", _dbg_redraw_streak);
          }
          _dbg_redraw_streak = 0;
        }
      }
    }

    if (need_redraw) {
    // Rendering
    cogito_backend->begin_frame(window);
    cogito_icon_cache_frame_start();  // Evict old icon textures

    CogitoColor win_bg = win->bg_set ? win->bg : cogito_rgba(245, 245, 245, 255);
    cogito_backend->clear(win_bg);

    cogito_tooltip_hovered = NULL;
    cogito_draw_node(win);
    if (cogito_menu.open) cogito_draw_menu();
    if (cogito_inspector_open) {
      cogito_draw_inspector_highlight();
      if (!inspector_window) {
        cogito_draw_inspector_panel(win);
      }
    }
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
      } else if (route_main_pointer) {
        CogitoNode* tip = cogito_find_tooltip_node(root, mx, my);
        if (tip) {
          cogito_draw_tooltip(win, tip);
        }
      }
    }
    
    cogito_backend->end_frame(window);
    cogito_backend->present(window);
    cogito_draw_clear_pending(win);
    // Animation deadline is time-based, no decrement needed

    if (cogito_inspector_open && inspector_window) {
      int iw = 0;
      int ih = 0;
      int appbar_h = inspector_appbar_h;
      cogito_backend->window_get_size(inspector_window, &iw, &ih);
      int frame_x = inspector_shadow_margin;
      int frame_y = inspector_shadow_margin;
      int frame_w = iw - inspector_shadow_margin * 2;
      int frame_h = ih - inspector_shadow_margin * 2;
      if (frame_w < 1) frame_w = 1;
      if (frame_h < 1) frame_h = 1;
      if (appbar_h > frame_h) appbar_h = frame_h;
      cogito_backend->begin_frame(inspector_window);
      cogito_backend->clear(cogito_rgba(0, 0, 0, 0));
      CogitoColor frame_bg = cogito_rgba(18, 21, 26, 245);
      CogitoColor appbar_bg = frame_bg;
      CogitoColor appbar_fg = cogito_rgba(220, 226, 235, 255);
      CogitoColor appbar_div = cogito_rgba(255, 255, 255, 34);
      CogitoStyle inspector_appbar_style = cogito_theme.per_kind[COGITO_APPBAR];
#if !defined(__APPLE__)
      cogito_draw_shadow_radii_level(frame_x, frame_y, frame_w, frame_h, inspector_corner_r, inspector_corner_r, inspector_corner_r, inspector_corner_r, 4);
#endif
      cogito_draw_rect(frame_x, frame_y, frame_w, frame_h, frame_bg, inspector_corner_r);
      cogito_draw_rect(frame_x, frame_y, frame_w, appbar_h + inspector_corner_r, appbar_bg, inspector_corner_r);
      if (appbar_h > 0) {
        cogito_draw_line(frame_x, frame_y + appbar_h - 1, frame_x + frame_w, frame_y + appbar_h - 1, appbar_div);
      }
      cogito_draw_rect_lines(frame_x, frame_y, frame_w, frame_h, appbar_div, inspector_corner_r, 1);
      CogitoColor inspector_fallback_btn = inspector_appbar_style.has_selection
        ? inspector_appbar_style.selection
        : cogito_rgba(184, 184, 184, 255);
      CogitoColor inspector_fallback_border = appbar_fg;
      if (inspector_fallback_border.a > 80) inspector_fallback_border.a = 80;
      cogito_draw_traffic_lights_styled(
        &inspector_appbar_style, inspector_fallback_btn, inspector_fallback_border,
        inspector_btn_close_x, inspector_btn_min_x, inspector_btn_max_x,
        &inspector_lights
      );
      int title_size = 20;
      int title_y = (appbar_h - cogito_text_height_size(title_size)) / 2;
      if (title_y < 0) title_y = 0;
      int title_x = inspector_btn_max_x + inspector_traffic_btn + 12;
      cogito_draw_text_size_node(win, title_x, frame_y + title_y, "Inspector", appbar_fg, title_size, true);
      cogito_inspector_view_x = frame_x;
      cogito_inspector_view_y = frame_y + appbar_h;
      cogito_inspector_view_w = frame_w;
      if (cogito_inspector_view_w < 0) cogito_inspector_view_w = 0;
      cogito_inspector_view_h = frame_h - appbar_h - inspector_corner_r;
      if (cogito_inspector_view_h < 0) cogito_inspector_view_h = 0;
      cogito_draw_inspector_panel(win);
      cogito_inspector_view_x = 0;
      cogito_inspector_view_y = 0;
      cogito_inspector_view_w = 0;
      cogito_inspector_view_h = 0;
      cogito_backend->end_frame(inspector_window);
      cogito_backend->present(inspector_window);
    }
    } else {
      // Idle: wait for an event or timeout.
      // SDL_WaitEventTimeout on macOS/Metal may not truly block (compositor
      // events wake it immediately), so always follow with a short sleep.
      cogito_backend->wait_event_timeout(100);
      struct timespec ts = {0, 8000000}; // 8ms (~120Hz check rate)
      nanosleep(&ts, NULL);
    }
#if defined(__APPLE__)
    cogito_autorelease_pop();
#endif
  }
  
  // Cleanup
  cogito_font_cache_clear();
  cogito_ft_library_done();
  cogito_icon_cache_clear();
  cogito_hit_test_cleanup();
  if (cogito_font_ready && cogito_font_from_env && cogito_font) {
    cogito_backend->font_unload(cogito_font);
  }
  if (cogito_font_mono_ready && cogito_font_mono_from_env && cogito_font_mono) {
    cogito_backend->font_unload(cogito_font_mono);
  }
  if (cogito_font_bold_ready && cogito_font_bold_from_env && cogito_font_bold) {
    cogito_backend->font_unload(cogito_font_bold);
  }
  
  if (inspector_window) {
    cogito_backend->window_destroy(inspector_window);
    inspector_window = NULL;
  }
  if (inspector_hit_test_params) {
    free(inspector_hit_test_params);
    inspector_hit_test_params = NULL;
  }
  cogito_inspector_view_x = 0;
  cogito_inspector_view_y = 0;
  cogito_inspector_view_w = 0;
  cogito_inspector_view_h = 0;
  cogito_backend->window_destroy(window);
  cogito_active_backend_window = NULL;
  cogito_backend_ready = false;
  cogito_menu_close();
  if (cogito_user_sum) {
    free(cogito_user_sum);
    cogito_user_sum = NULL;
  }
}
