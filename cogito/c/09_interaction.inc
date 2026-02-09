static void cogito_invoke_change(CogitoNode* n);
static CogitoNode* cogito_root(CogitoNode* n);
static bool cogito_hit_node(CogitoNode* n, int x, int y);
static void cogito_clear_children(CogitoNode* node);
static void cogito_node_set_class(ErgoVal nodev, ErgoVal classv);

static void cogito_invoke(ErgoFn* fn, int argc, ErgoVal* argv) {
  if (!fn) return;
  static int cogito_debug_invoke = -1;
  if (cogito_debug_invoke < 0) {
    const char* env = getenv("COGITO_DEBUG_HIT");
    cogito_debug_invoke = (env && env[0] && env[0] != '0') ? 1 : 0;
  }
  if (cogito_debug_invoke) {
    fprintf(stderr, "cogito: invoke fn=%p impl=%p env=%p argc=%d\n",
            (void*)fn, (void*)fn->fn, fn->env, argc);
    fflush(stderr);
  }
  ErgoVal ret = fn->fn(fn->env, argc, argv);
  if (cogito_debug_invoke) {
    fprintf(stderr, "cogito: invoke ret tag=%d\n", (int)ret.tag);
    fflush(stderr);
  }
  if (ret.tag != EVT_NULL) {
    ergo_release_val(ret);
  }
  if (cogito_debug_invoke) {
    fprintf(stderr, "cogito: invoke done fn=%p\n", (void*)fn);
    fflush(stderr);
  }
}

static void cogito_invoke_click(CogitoNode* n) {
  if (!n || !n->on_click) return;
  static int cogito_debug_click = -1;
  if (cogito_debug_click < 0) {
    const char* env = getenv("COGITO_DEBUG_HIT");
    cogito_debug_click = (env && env[0] && env[0] != '0') ? 1 : 0;
  }
  if (cogito_debug_click) {
    const char* t = (n->text && n->text->data) ? n->text->data : "";
    CogitoNode* win = cogito_node_window(n);
    fprintf(stderr, "cogito: invoke_click kind=%d text=\"%s\" handler=%p\n",
            (int)n->kind, t, (void*)n->on_click);
    if (win) {
      fprintf(stderr, "cogito: invoke_click window=%p dialog_open=%d dialog=%p dialog_kind=%d\n",
              (void*)win, (int)win->dialog_open, (void*)win->dialog,
              win->dialog ? (int)win->dialog->kind : -1);
    } else {
      fprintf(stderr, "cogito: invoke_click window=NULL\n");
    }
    fflush(stderr);
  }
  ErgoVal arg = EV_OBJ(n);
  cogito_invoke(n->on_click, 1, &arg);
}

static void cogito_invoke_menu(CogitoNode* n, size_t idx) {
  if (!n || idx >= n->menu_len) return;
  if (n->kind == COGITO_DROPDOWN) {
    n->selected = (int)idx;
    cogito_invoke_change(n);
    return;
  }
  ErgoFn* fn = n->menu_handlers[idx];
  if (!fn) return;
  ErgoVal arg = EV_OBJ(n);
  cogito_invoke(fn, 1, &arg);
}

static void cogito_invoke_change(CogitoNode* n) {
  if (!n || !n->on_change) return;
  ErgoVal arg = EV_OBJ(n);
  cogito_invoke(n->on_change, 1, &arg);
}

static void cogito_invoke_select(CogitoNode* n, int idx, bool activate) {
  ErgoVal arg = EV_INT(idx);
  if (n->on_select) cogito_invoke(n->on_select, 1, &arg);
  if (activate && n->on_activate) cogito_invoke(n->on_activate, 1, &arg);
}


static void cogito_dialog_slot_show(ErgoVal slotv, ErgoVal dialogv);
static void cogito_dialog_slot_clear(ErgoVal slotv);
static void cogito_dialog_close(ErgoVal dialogv);
static void cogito_dialog_remove(ErgoVal dialogv);
static ErgoVal cogito_node_parent(ErgoVal nodev);
static void cogito_container_remove_child(CogitoNode* parent, CogitoNode* child);

#include "widgets/interaction/button.inc"
#include "widgets/interaction/checkbox.inc"
#include "widgets/interaction/chip.inc"
#include "widgets/interaction/colorpicker.inc"
#include "widgets/interaction/datepicker.inc"
#include "widgets/interaction/dialog.inc"
#include "widgets/interaction/popover.inc"
#include "widgets/interaction/dropdown.inc"
#include "widgets/interaction/fab.inc"
#include "widgets/interaction/fixed.inc"
#include "widgets/interaction/grid.inc"
#include "widgets/interaction/hstack.inc"
#include "widgets/interaction/iconbtn.inc"
#include "widgets/interaction/image.inc"
#include "widgets/interaction/label.inc"
#include "widgets/interaction/list.inc"
#include "widgets/interaction/nav_rail.inc"
#include "widgets/interaction/bottom_nav.inc"
#include "widgets/interaction/progress.inc"
#include "widgets/interaction/scroller.inc"
#include "widgets/interaction/searchfield.inc"
#include "widgets/interaction/segmented.inc"
#include "widgets/interaction/slider.inc"
#include "widgets/interaction/stepper.inc"
#include "widgets/interaction/switch.inc"
#include "widgets/interaction/tabs.inc"
#include "widgets/interaction/textfield.inc"
#include "widgets/interaction/textview.inc"
#include "widgets/interaction/toast.inc"
#include "widgets/interaction/toasts.inc"
#include "widgets/interaction/bottom_toolbar.inc"
#include "widgets/interaction/treeview.inc"
#include "widgets/interaction/tooltip.inc"
#include "widgets/interaction/view_switcher.inc"
#include "widgets/interaction/vstack.inc"
#include "widgets/interaction/window.inc"
#include "widgets/interaction/zstack.inc"
#include "widgets/interaction/appbar.inc"



static void cogito_state_drop(ErgoObj* o) {
  CogitoState* s = (CogitoState*)o;
  if (s->value.tag != EVT_NULL) {
    ergo_release_val(s->value);
    s->value = EV_NULLV;
  }
}

static CogitoState* cogito_state_new_obj(ErgoVal initial) {
  CogitoState* s = (CogitoState*)ergo_obj_new(sizeof(CogitoState), cogito_state_drop);
  s->value = initial;
  if (initial.tag != EVT_NULL) ergo_retain_val(initial);
  return s;
}

static CogitoNode* cogito_root(CogitoNode* n) {
  CogitoNode* cur = n;
  while (cur && cur->parent) cur = cur->parent;
  return cur;
}

static CogitoNode* cogito_node_window(CogitoNode* n) {
  CogitoNode* root = cogito_root(n);
  if (root && root->kind == COGITO_WINDOW) return root;
  return NULL;
}

static CogitoNode* cogito_node_get_parent_internal(CogitoNode* n) {
  return n ? n->parent : NULL;
}

static size_t cogito_node_get_child_count_internal(CogitoNode* n) {
  return n ? n->len : 0;
}

static CogitoNode* cogito_node_get_child_internal(CogitoNode* n, size_t index) {
  if (!n || index >= n->len) return NULL;
  return n->children[index];
}

static bool cogito_hit_rect(int x, int y, int rx, int ry, int rw, int rh) {
  return x >= rx && y >= ry && x <= rx + rw && y <= ry + rh;
}

static bool cogito_hit_node(CogitoNode* n, int x, int y) {
  return cogito_hit_rect(x, y, n->x, n->y, n->w, n->h);
}

static CogitoNode* cogito_find_clickable(CogitoNode* n, int x, int y) {
  if (!n) return NULL;
  if (n->kind == COGITO_DIALOG_SLOT) {
    if (n->dialog && n->dialog_open) {
      return cogito_find_clickable(n->dialog, x, y);
    }
    return NULL;
  }
  if (n->kind == COGITO_DIALOG && !cogito_hit_node(n, x, y)) return NULL;
  if (n->kind == COGITO_SCROLLER && !cogito_hit_rect(x, y, n->x, n->y, n->w, n->h)) return NULL;
  if (n->kind == COGITO_VIEWSWITCHER) {
    CogitoNode* active = cogito_view_switcher_active_child(n);
    if (active) {
      CogitoNode* child = cogito_find_clickable(active, x, y);
      if (child) return child;
    }
  } else {
    for (size_t i = n->len; i > 0; i--) {
      CogitoNode* child = cogito_find_clickable(n->children[i - 1], x, y);
      if (child) return child;
    }
  }
  if (n->kind == COGITO_BUTTON || n->kind == COGITO_ICONBTN || n->kind == COGITO_CHECKBOX ||
      n->kind == COGITO_SWITCH || n->kind == COGITO_LIST || n->kind == COGITO_GRID ||
      n->kind == COGITO_TEXTFIELD || n->kind == COGITO_TEXTVIEW ||
      n->kind == COGITO_DROPDOWN || n->kind == COGITO_SLIDER ||
      n->kind == COGITO_TABS || n->kind == COGITO_TOAST ||
      n->kind == COGITO_DATEPICKER || n->kind == COGITO_COLORPICKER ||
      n->kind == COGITO_CHIP || n->kind == COGITO_FAB ||
      n->kind == COGITO_STEPPER || n->kind == COGITO_SEGMENTED ||
      n->kind == COGITO_SEARCHFIELD) {
    if (cogito_hit_node(n, x, y)) return n;
  }
  return NULL;
}


static bool cogito_node_focusable(const CogitoNode* n) {
  if (!n || n->disabled) return false;
  switch (n->kind) {
    #include "widgets/interaction/focusable/textfield.inc"
    #include "widgets/interaction/focusable/textview.inc"
    #include "widgets/interaction/focusable/dropdown.inc"
    #include "widgets/interaction/focusable/slider.inc"
    #include "widgets/interaction/focusable/button.inc"
    #include "widgets/interaction/focusable/iconbtn.inc"
    #include "widgets/interaction/focusable/checkbox.inc"
    #include "widgets/interaction/focusable/chip.inc"
    #include "widgets/interaction/focusable/switch.inc"
    #include "widgets/interaction/focusable/colorpicker.inc"
    #include "widgets/interaction/focusable/datepicker.inc"
    #include "widgets/interaction/focusable/fab.inc"
    #include "widgets/interaction/focusable/nav_rail.inc"
    #include "widgets/interaction/focusable/bottom_nav.inc"
    #include "widgets/interaction/focusable/stepper.inc"
    #include "widgets/interaction/focusable/segmented.inc"
    #include "widgets/interaction/focusable/searchfield.inc"

    default:
      return false;

}
}

static CogitoNode* cogito_focus_next(CogitoNode* n, CogitoNode* current, bool* found_current) {
  if (!n) return NULL;
  if (n == current) *found_current = true;
  else if (*found_current && cogito_node_focusable(n)) return n;
  if (n->kind == COGITO_VIEWSWITCHER) {
    CogitoNode* active = cogito_view_switcher_active_child(n);
    if (active) {
      CogitoNode* hit = cogito_focus_next(active, current, found_current);
      if (hit) return hit;
    }
  } else {
    for (size_t i = 0; i < n->len; i++) {
      CogitoNode* hit = cogito_focus_next(n->children[i], current, found_current);
      if (hit) return hit;
    }
  }
  return NULL;
}

static CogitoNode* cogito_focus_first(CogitoNode* n) {
  if (!n) return NULL;
  if (cogito_node_focusable(n)) return n;
  if (n->kind == COGITO_VIEWSWITCHER) {
    CogitoNode* active = cogito_view_switcher_active_child(n);
    if (active) return cogito_focus_first(active);
  } else {
    for (size_t i = 0; i < n->len; i++) {
      CogitoNode* hit = cogito_focus_first(n->children[i]);
      if (hit) return hit;
    }
  }
  return NULL;
}

static void cogito_container_remove_child(CogitoNode* parent, CogitoNode* child) {
  if (!parent || !child || !parent->children) return;
  size_t idx = parent->len;
  for (size_t i = 0; i < parent->len; i++) {
    if (parent->children[i] == child) { idx = i; break; }
  }
  if (idx >= parent->len) return;
  ergo_release_val(EV_OBJ(parent->children[idx]));
  for (size_t i = idx + 1; i < parent->len; i++) {
    parent->children[i - 1] = parent->children[i];
  }
  parent->len--;
}

static void cogito_clear_children(CogitoNode* node) {
  if (!node || !node->children) return;
  for (size_t i = 0; i < node->len; i++) {
    ergo_release_val(EV_OBJ(node->children[i]));
  }
  free(node->children);
  node->children = NULL;
  node->len = 0;
  node->cap = 0;
}



static void cogito_app_drop(ErgoObj* o) {
  CogitoApp* app = (CogitoApp*)o;
  if (app->app_id) {
    ergo_release_val(EV_STR(app->app_id));
    app->app_id = NULL;
  }
}

static CogitoApp* cogito_app_new_obj(void) {
  CogitoApp* app = (CogitoApp*)ergo_obj_new(sizeof(CogitoApp), cogito_app_drop);
  app->native = NULL;
  app->app_id = NULL;
  app->accent_override = false;
  app->accent_color = cogito_rgba(114, 222, 194, 255);
  app->accent_set = false;
  app->system_dark = false;
  app->system_accent = cogito_rgba(114, 222, 194, 255);
  app->system_accent_set = false;
  return app;
}

static CogitoColor cogito_app_effective_accent(const CogitoApp* app) {
  if (app->accent_override && app->accent_set) return app->accent_color;
  if (app->system_accent_set) return app->system_accent;
  if (app->accent_set) return app->accent_color;
  return cogito_rgba(114, 222, 194, 255);
}

static void cogito_app_update_theme(CogitoApp* app) {
  bool dark = app ? app->system_dark : false;
  CogitoColor accent = app ? cogito_app_effective_accent(app) : cogito_rgba(114, 222, 194, 255);
  if (dark != cogito_theme_dark ||
      accent.r != cogito_theme_accent.r || accent.g != cogito_theme_accent.g ||
      accent.b != cogito_theme_accent.b) {
    cogito_apply_theme(dark, accent);
  }
}

static void cogito_update_system_theme(CogitoApp* app) {
  if (!app) return;
  bool dark = cogito_system_is_dark();
  CogitoColor acc = {0};
  bool has_acc = cogito_system_accent(&acc);
  bool changed = false;
  if (dark != app->system_dark) {
    app->system_dark = dark;
    changed = true;
  }
  if (has_acc && (!app->system_accent_set ||
      acc.r != app->system_accent.r || acc.g != app->system_accent.g || acc.b != app->system_accent.b)) {
    app->system_accent = acc;
    app->system_accent_set = true;
    if (!app->accent_override) changed = true;
  }
  if (changed) cogito_app_update_theme(app);
}

static void cogito_app_set_appid(ErgoVal appv, ErgoVal idv) {
  if (appv.tag != EVT_OBJ) ergo_trap("cogito.app_set_appid expects app");
  CogitoApp* app = (CogitoApp*)appv.as.p;
  ErgoStr* s = NULL;
  bool temp = false;
  if (idv.tag == EVT_STR) {
    s = (ErgoStr*)idv.as.p;
    if (s) ergo_retain_val(EV_STR(s));
  } else if (idv.tag != EVT_NULL) {
    s = stdr_to_string(idv);
    temp = true;
  }
  if (app->app_id) ergo_release_val(EV_STR(app->app_id));
  app->app_id = s;
  if (temp && s) {
    // app now owns temp string (ref=1)
  }
}

static void cogito_app_set_accent_color(ErgoVal appv, ErgoVal colorv, ErgoVal overridev) {
  if (appv.tag != EVT_OBJ) ergo_trap("cogito.app_set_accent_color expects app");
  CogitoApp* app = (CogitoApp*)appv.as.p;
  bool override = ergo_as_bool(overridev);
  app->accent_override = override;
  if (colorv.tag != EVT_NULL) {
    ErgoStr* s = NULL;
    bool temp = false;
    if (colorv.tag == EVT_STR) {
      s = (ErgoStr*)colorv.as.p;
    } else {
      s = stdr_to_string(colorv);
      temp = true;
    }
    CogitoColor c = {0};
    if (s && s->data && cogito_hex_to_color(s->data, &c)) {
      app->accent_color = c;
      app->accent_set = true;
    }
    if (temp && s) ergo_release_val(EV_STR(s));
  }
  cogito_app_update_theme(app);
}



































































static ErgoVal cogito_app_new(void) {
  if (cogito_debug_enabled()) {
    fprintf(stderr, "cogito: app_new\n");
    fflush(stderr);
  }
  return EV_OBJ(cogito_app_new_obj());
}













































static void cogito_dialog_slot_show(ErgoVal slotv, ErgoVal dialogv);
static void cogito_dialog_slot_clear(ErgoVal slotv);
static void cogito_dialog_close(ErgoVal dialogv);
static void cogito_dialog_remove(ErgoVal dialogv);

















static void cogito_container_add(ErgoVal parentv, ErgoVal childv) {
  if (parentv.tag != EVT_OBJ || childv.tag != EVT_OBJ) ergo_trap("cogito.container_add expects objects");
  CogitoNode* parent = (CogitoNode*)parentv.as.p;
  CogitoNode* child = (CogitoNode*)childv.as.p;
  cogito_children_add(parent, child);
}

static void cogito_container_set_margins(ErgoVal nodev, ErgoVal left, ErgoVal top, ErgoVal right, ErgoVal bottom) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.container_set_margins expects object");
  CogitoNode* node = (CogitoNode*)nodev.as.p;
  node->margin_left = (int)ergo_as_int(left);
  node->margin_top = (int)ergo_as_int(top);
  node->margin_right = (int)ergo_as_int(right);
  node->margin_bottom = (int)ergo_as_int(bottom);
  node->margin_set = true;
}

static void cogito_container_set_align(ErgoVal nodev, ErgoVal alignv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.container_set_align expects object");
  CogitoNode* node = (CogitoNode*)nodev.as.p;
  node->align = (int)ergo_as_int(alignv);
}

static void cogito_container_set_halign(ErgoVal nodev, ErgoVal alignv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.container_set_halign expects object");
  CogitoNode* node = (CogitoNode*)nodev.as.p;
  int col = (int)ergo_as_int(alignv);
  if (col < 0) col = 0;
  if (col > 2) col = 2;
  int row = node->align / 3;
  node->align = row * 3 + col;
}

static void cogito_container_set_valign(ErgoVal nodev, ErgoVal alignv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.container_set_valign expects object");
  CogitoNode* node = (CogitoNode*)nodev.as.p;
  int row = (int)ergo_as_int(alignv);
  if (row < 0) row = 0;
  if (row > 2) row = 2;
  int col = node->align % 3;
  node->align = row * 3 + col;
}

static void cogito_container_set_hexpand(ErgoVal nodev, ErgoVal expandv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.container_set_hexpand expects object");
  CogitoNode* node = (CogitoNode*)nodev.as.p;
  node->hexpand = ergo_as_bool(expandv);
}

static void cogito_container_set_vexpand(ErgoVal nodev, ErgoVal expandv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.container_set_vexpand expects object");
  CogitoNode* node = (CogitoNode*)nodev.as.p;
  node->vexpand = ergo_as_bool(expandv);
}

static void cogito_container_set_gap(ErgoVal nodev, ErgoVal gapv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.container_set_gap expects object");
  CogitoNode* node = (CogitoNode*)nodev.as.p;

  node->gap = (int)ergo_as_int(gapv);
  cogito_window_relayout(cogito_node_window(node));
}


static void cogito_container_set_padding(ErgoVal nodev, ErgoVal left, ErgoVal top, ErgoVal right, ErgoVal bottom) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.container_set_padding expects object");
  CogitoNode* node = (CogitoNode*)nodev.as.p;
  node->padding_left = (int)ergo_as_int(left);
  node->padding_top = (int)ergo_as_int(top);
  node->padding_right = (int)ergo_as_int(right);
  node->padding_bottom = (int)ergo_as_int(bottom);
  node->padding_set = true;
}

















static void cogito_node_set_disabled(ErgoVal nodev, ErgoVal onv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.node_set_disabled expects object");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  n->disabled = ergo_as_bool(onv);
  if (n == cogito_focused && n->disabled) cogito_focused = NULL;
  cogito_window_relayout(cogito_node_window(n));
}





































static void cogito_node_set_id(ErgoVal nodev, ErgoVal idv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.node_set_id expects object");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  ErgoStr* s = NULL;
  if (idv.tag == EVT_STR) {
    s = (ErgoStr*)idv.as.p;
    if (s) ergo_retain_val(EV_STR(s));
  } else if (idv.tag != EVT_NULL) {
    s = stdr_to_string(idv);
  }
  if (n->view_id) ergo_release_val(EV_STR(n->view_id));
  n->view_id = s;
  cogito_window_relayout(cogito_node_window(n));
}





















static void cogito_pointer_capture_set(ErgoVal nodev) {
  if (nodev.tag == EVT_NULL) {
    cogito_pointer_capture = NULL;
    return;
  }
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.pointer_capture expects node or null");
  cogito_pointer_capture = (CogitoNode*)nodev.as.p;
}

static void cogito_pointer_capture_clear(void) {
  cogito_pointer_capture = NULL;
}

static void cogito_node_set_class(ErgoVal nodev, ErgoVal classv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.node_set_class expects node");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  ErgoStr* cs = NULL;
  bool temp = false;
  if (classv.tag == EVT_STR) {
    cs = (ErgoStr*)classv.as.p;
  } else if (classv.tag != EVT_NULL) {
    cs = stdr_to_string(classv);
    temp = true;
  }
  const char* s = cs ? cs->data : "";
  
  // Clear old class name
  if (n->class_name) {
    ergo_release_val(EV_STR(n->class_name));
    n->class_name = NULL;
  }
  
  n->font_mono = false;
  n->font_tabular = false;
  n->button_outlined = false;
  n->button_text = false;
  if (n->kind == COGITO_LABEL) n->label_class = COGITO_LABEL_CLASS_NONE;
  if (s && s[0]) {
    if (strcmp(s, "title") == 0) {
      n->label_class = COGITO_LABEL_CLASS_TITLE;
    } else if (strcmp(s, "subtitle") == 0 || strcmp(s, "sub-title") == 0) {
      n->label_class = COGITO_LABEL_CLASS_SUBTITLE;
    } else if (strcmp(s, "body") == 0) {
      n->label_class = COGITO_LABEL_CLASS_BODY;
    } else if (strcmp(s, "monospace") == 0 || strcmp(s, "mono") == 0) {
      n->font_mono = true;
    } else if (strcmp(s, "tabular") == 0 || strcmp(s, "tnum") == 0) {
      n->font_tabular = true;
    } else if (strcmp(s, "outlined") == 0) {
      n->button_outlined = true;
    } else if (strcmp(s, "text") == 0) {
      n->button_text = true;
    } else {
      // Store custom class name
      if (cs) {
        n->class_name = cs;
        ergo_retain_val(EV_STR(cs));
        temp = false; // Don't release if we retained it
      }
    }
  }
  if (temp && cs) ergo_release_val(EV_STR(cs));
  cogito_apply_style_to_node(n);
  extern CogitoNode* cogito_active_window;
  if (cogito_active_window) {
    cogito_window_relayout(cogito_active_window);
  }
}

static void cogito_node_set_a11y_label(ErgoVal nodev, ErgoVal labelv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.node_set_a11y_label expects node");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->a11y_label) {
    ergo_release_val(EV_STR(n->a11y_label));
    n->a11y_label = NULL;
  }
  if (labelv.tag == EVT_STR) {
    ErgoStr* ls = (ErgoStr*)labelv.as.p;
    if (ls) {
      n->a11y_label = ls;
      ergo_retain_val(EV_STR(ls));
    }
  } else if (labelv.tag != EVT_NULL) {
    n->a11y_label = stdr_to_string(labelv);
  }
#if defined(__APPLE__)
  if (n->kind == COGITO_WINDOW && cogito_backend_ready) {
    const char* s = n->a11y_label ? n->a11y_label->data : "";
    cogito_macos_set_window_a11y_label(s);
  }
#endif
}

static void cogito_node_set_a11y_role(ErgoVal nodev, ErgoVal rolev) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.node_set_a11y_role expects node");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->a11y_role) {
    ergo_release_val(EV_STR(n->a11y_role));
    n->a11y_role = NULL;
  }
  if (rolev.tag == EVT_STR) {
    ErgoStr* rs = (ErgoStr*)rolev.as.p;
    if (rs) {
      n->a11y_role = rs;
      ergo_retain_val(EV_STR(rs));
    }
  } else if (rolev.tag != EVT_NULL) {
    n->a11y_role = stdr_to_string(rolev);
  }
}

__attribute__((unused))
static void cogito_node_set_tooltip_val(ErgoVal nodev, ErgoVal textv) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.node_set_tooltip expects node");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  if (n->tooltip) {
    ergo_release_val(EV_STR(n->tooltip));
    n->tooltip = NULL;
  }
  if (textv.tag == EVT_STR) {
    ErgoStr* ts = (ErgoStr*)textv.as.p;
    if (ts) {
      n->tooltip = ts;
      ergo_retain_val(EV_STR(ts));
    }
  } else if (textv.tag != EVT_NULL) {
    n->tooltip = stdr_to_string(textv);
  }
}







static void cogito_build(ErgoVal nodev, ErgoVal builder) {
  if (builder.tag != EVT_FN) ergo_trap("cogito.build expects function");
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.build expects object");
  ErgoVal arg = nodev;
  ergo_retain_val(arg);
  ErgoVal ret = ergo_call(builder, 1, &arg);
  ergo_release_val(arg);
  ergo_release_val(ret);
}



__attribute__((unused))
static ErgoVal cogito_state_new(ErgoVal initial) {
  CogitoState* s = cogito_state_new_obj(initial);
  return EV_OBJ(s);
}

__attribute__((unused))
static ErgoVal cogito_state_get(ErgoVal sv) {
  if (sv.tag != EVT_OBJ) ergo_trap("cogito.state_get expects state");
  CogitoState* s = (CogitoState*)sv.as.p;
  ErgoVal v = s->value;
  ergo_retain_val(v);
  return v;
}

__attribute__((unused))
static void cogito_state_set(ErgoVal sv, ErgoVal nv) {
  if (sv.tag != EVT_OBJ) ergo_trap("cogito.state_set expects state");
  CogitoState* s = (CogitoState*)sv.as.p;
  if (s->value.tag != EVT_NULL) ergo_release_val(s->value);
  s->value = nv;
  if (nv.tag != EVT_NULL) ergo_retain_val(nv);
  (void)sv;
  extern CogitoNode* cogito_active_window;
  if (cogito_active_window) {
    cogito_window_rebuild(cogito_active_window);
  }
}















static ErgoVal cogito_node_window_val(ErgoVal nodev) {
  if (nodev.tag != EVT_OBJ) ergo_trap("cogito.node_window expects object");
  CogitoNode* n = (CogitoNode*)nodev.as.p;
  CogitoNode* win = cogito_node_window(n);
  if (win) return EV_OBJ(win);
  return EV_NULLV;
}



































static int cogito_css_hex_value(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
  if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
  return -1;
}

static void cogito_css_skip_ws(const char** p) {
  while (**p) {
    if (isspace((unsigned char)**p)) { (*p)++; continue; }
    if ((*p)[0] == '/' && (*p)[1] == '/') {
      (*p) += 2;
      while (**p && **p != '\n') (*p)++;
      continue;
    }
    if ((*p)[0] == '/' && (*p)[1] == '*') {
      (*p) += 2;
      while (**p && !((*p)[0] == '*' && (*p)[1] == '/')) (*p)++;
      if (**p) (*p) += 2;
      continue;
    }
    break;
  }
}

static bool cogito_css_read_ident(const char** p, char* out, size_t cap) {
  size_t len = 0;
  if (!isalpha((unsigned char)**p) && **p != '_' && **p != '-' && **p != '.' && **p != '#') return false;
  while (**p && (isalnum((unsigned char)**p) || **p == '_' || **p == '-' || **p == '.' || **p == '#')) {
    if (len + 1 < cap) out[len++] = **p;
    (*p)++;
  }
  out[len] = 0;
  return len > 0;
}

static bool cogito_css_read_number(const char** p, double* out) {
  char* end = NULL;
  double v = strtod(*p, &end);
  if (end == *p) return false;
  *p = end;
  if ((*p)[0] == 'p' && (*p)[1] == 'x') (*p) += 2;
  *out = v;
  return true;
}

static bool cogito_css_parse_hex_color(const char** p, CogitoColor* out) {
  if (**p != '#') return false;
  const char* s = *p + 1;
  int len = 0;
  while (cogito_css_hex_value(s[len]) >= 0) len++;
  if (len != 3 && len != 4 && len != 6 && len != 8) return false;
  int r = 0, g = 0, b = 0, a = 255;
  if (len == 3 || len == 4) {
    r = cogito_css_hex_value(s[0]) * 17;
    g = cogito_css_hex_value(s[1]) * 17;
    b = cogito_css_hex_value(s[2]) * 17;
    if (len == 4) a = cogito_css_hex_value(s[3]) * 17;
  } else if (len == 6 || len == 8) {
    r = cogito_css_hex_value(s[0]) * 16 + cogito_css_hex_value(s[1]);
    g = cogito_css_hex_value(s[2]) * 16 + cogito_css_hex_value(s[3]);
    b = cogito_css_hex_value(s[4]) * 16 + cogito_css_hex_value(s[5]);
    if (len == 8) a = cogito_css_hex_value(s[6]) * 16 + cogito_css_hex_value(s[7]);
  }
  *out = cogito_rgba(r, g, b, a);
  *p += 1 + len;
  return true;
}

static bool cogito_css_parse_named_color(const char* name, CogitoColor* out) {
  if (!name) return false;
  if (strcmp(name, "transparent") == 0) { *out = cogito_rgba(0, 0, 0, 0); return true; }
  if (strcmp(name, "white") == 0) { *out = cogito_rgba(255, 255, 255, 255); return true; }
  if (strcmp(name, "black") == 0) { *out = cogito_rgba(0, 0, 0, 255); return true; }
  if (strcmp(name, "gray") == 0 || strcmp(name, "grey") == 0) { *out = cogito_rgba(128, 128, 128, 255); return true; }
  if (strcmp(name, "red") == 0) { *out = cogito_rgba(220, 60, 60, 255); return true; }
  if (strcmp(name, "green") == 0) { *out = cogito_rgba(60, 170, 90, 255); return true; }
  if (strcmp(name, "blue") == 0) { *out = cogito_rgba(70, 110, 200, 255); return true; }
  if (strcmp(name, "yellow") == 0) { *out = cogito_rgba(230, 210, 90, 255); return true; }
  if (strcmp(name, "orange") == 0) { *out = cogito_rgba(230, 150, 60, 255); return true; }
  if (strcmp(name, "purple") == 0) { *out = cogito_rgba(160, 100, 210, 255); return true; }
  return false;
}

static bool cogito_css_parse_rgb_color(const char** p, CogitoColor* out) {
  const char* s = *p;
  bool has_alpha = false;
  if (strncmp(s, "rgba", 4) == 0) {
    has_alpha = true;
    s += 4;
  } else if (strncmp(s, "rgb", 3) == 0) {
    s += 3;
  } else {
    return false;
  }
  cogito_css_skip_ws(&s);
  if (*s != '(') return false;
  s++;
  double vals[4] = {0, 0, 0, 1};
  int count = 0;
  while (*s && count < (has_alpha ? 4 : 3)) {
    cogito_css_skip_ws(&s);
    double v = 0;
    if (!cogito_css_read_number(&s, &v)) break;
    vals[count++] = v;
    cogito_css_skip_ws(&s);
    if (*s == ',') s++;
  }
  cogito_css_skip_ws(&s);
  if (*s != ')') return false;
  s++;
  if (count < (has_alpha ? 4 : 3)) return false;
  int r = (int)lround(vals[0]);
  int g = (int)lround(vals[1]);
  int b = (int)lround(vals[2]);
  int a = 255;
  if (has_alpha) {
    if (vals[3] <= 1.0) a = (int)lround(vals[3] * 255.0);
    else a = (int)lround(vals[3]);
  }
  *out = cogito_rgba(r, g, b, a);
  *p = s;
  return true;
}

static bool cogito_css_parse_color(const char** p, CogitoColor* out) {
  cogito_css_skip_ws(p);
  if (**p == '#') return cogito_css_parse_hex_color(p, out);
  if (strncmp(*p, "rgb", 3) == 0) return cogito_css_parse_rgb_color(p, out);
  char name[32];
  const char* s = *p;
  if (!cogito_css_read_ident(&s, name, sizeof(name))) return false;
  if (cogito_css_parse_named_color(name, out)) { *p = s; return true; }
  return false;
}

static int cogito_css_parse_ints(const char** p, int* out, int max) {
  int count = 0;
  while (count < max) {
    cogito_css_skip_ws(p);
    double v = 0;
    if (!cogito_css_read_number(p, &v)) break;
    out[count++] = (int)lround(v);
    cogito_css_skip_ws(p);
    if (**p == ',') (*p)++;
  }
  return count;
}
