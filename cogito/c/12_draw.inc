static void cogito_draw_node(CogitoNode* n) {
  if (!n) return;
  if (n->tooltip && n->tooltip->data && n->tooltip->data[0] && !n->disabled) {
    int mx = (int)GetMousePosition().x;
    int my = (int)GetMousePosition().y;
    if (cogito_hit_node(n, mx, my)) {
      cogito_tooltip_hovered = n;
    }
  }
  switch (n->kind) {

    #include "widgets/draw/button.inc"

    #include "widgets/draw/iconbtn.inc"

    #include "widgets/draw/image.inc"

    #include "widgets/draw/label.inc"

    #include "widgets/draw/textfield.inc"

    #include "widgets/draw/textview.inc"

    #include "widgets/draw/dropdown.inc"

    #include "widgets/draw/datepicker.inc"

    #include "widgets/draw/slider.inc"

    #include "widgets/draw/colorpicker.inc"

    #include "widgets/draw/tabs.inc"

    #include "widgets/draw/progress.inc"

    #include "widgets/draw/toast.inc"

    #include "widgets/draw/checkbox.inc"

    #include "widgets/draw/switch.inc"

    #include "widgets/draw/list.inc"

    #include "widgets/draw/grid.inc"

    #include "widgets/draw/appbar.inc"

    #include "widgets/draw/dialog.inc"

    #include "widgets/draw/bottom_toolbar.inc"

    #include "widgets/draw/dialog_slot.inc"

    #include "widgets/draw/view_switcher.inc"

    #include "widgets/draw/toasts.inc"

    #include "widgets/draw/vstack.inc"

    #include "widgets/draw/hstack.inc"

    #include "widgets/draw/zstack.inc"

    #include "widgets/draw/fixed.inc"

    #include "widgets/draw/window.inc"

    #include "widgets/draw/scroller.inc"

    #include "widgets/draw/searchfield.inc"

    #include "widgets/draw/stepper.inc"

    #include "widgets/draw/segmented.inc"

    #include "widgets/draw/treeview.inc"

    #include "widgets/draw/tooltip.inc"

    case COGITO_KIND_COUNT:
      break;
  

}
}

static void cogito_draw_menu(void) {
  if (!cogito_menu.open || !cogito_menu.owner) return;
  Color bg = cogito_theme.menu.has_bg ? cogito_theme.menu.bg : cogito_rgba(250, 250, 250, 255);
  Color border = cogito_theme.menu.has_border ? cogito_theme.menu.border : cogito_rgba(180, 180, 180, 255);
  Color text_color = cogito_theme.menu.has_text ? cogito_theme.menu.text : cogito_rgba(30, 30, 30, 255);
  Color sel = cogito_theme.menu.has_selection ? cogito_theme.menu.selection : cogito_rgba(220, 230, 250, 255);
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
    cogito_draw_shadow(x, y, w, h, radius, shadow_level);
  }
  cogito_draw_rect(x, y, w, h, bg, radius);
  if (has_border && border_w > 0) {
    cogito_draw_rect_lines(x, y, w, h, border, radius, border_w);
  }
  int mx = (int)GetMousePosition().x;
  int my = (int)GetMousePosition().y;
  int hover = cogito_menu_hit(&cogito_menu, mx, my);
  int item_radius = cogito_theme.menu_item.has_radius ? cogito_theme.menu_item.radius : 0;
  for (size_t i = 0; i < cogito_menu.owner->menu_len; i++) {
    int iy = y + cogito_menu.pad_top + (int)i * cogito_menu.item_h;
    if ((int)i == hover) {
      int sel_radius = item_radius;
      if (sel_radius < 0) sel_radius = 0;
      cogito_draw_rect(x, iy, w, cogito_menu.item_h, sel, sel_radius);
    }
    const char* text = cogito_menu.owner->menu_labels[i] ? cogito_menu.owner->menu_labels[i]->data : "";
    int th = cogito_text_height_size(font_size);
    int ty = iy + (cogito_menu.item_h - th) / 2;
    cogito_draw_text_size_node(cogito_menu.owner, x + cogito_menu.pad_x, ty, text, text_color, font_size, false);
  }
}

static void cogito_draw_tooltip(CogitoNode* win, CogitoNode* target) {
  if (!win || !target || !target->tooltip || !target->tooltip->data) return;
  CogitoStyle s = cogito_theme_resolve(COGITO_TOOLTIP);
  Color bg = s.has_bg ? s.bg : cogito_rgba(30, 30, 30, 240);
  Color text = s.has_text ? s.text : cogito_rgba(255, 255, 255, 255);
  Color border = s.has_border ? s.border : cogito_rgba(0, 0, 0, 0);
  int border_w = s.has_border_width ? s.border_width : 0;
  int radius = s.has_radius ? s.radius : 6;
  int font_size = s.has_font_size ? s.font_size : cogito_font_size();
  int pad_x = s.has_padding_left ? s.padding_left : 8;
  int pad_y = s.has_padding_top ? s.padding_top : 6;

  const char* t = target->tooltip->data;
  int tw = cogito_text_width_size_node(target, t, font_size);
  int th = cogito_text_height_size(font_size);
  int w = tw + pad_x * 2;
  int h = th + pad_y * 2;

  Vector2 mp = GetMousePosition();
  int x = (int)mp.x + 12;
  int y = (int)mp.y + 16;
  if (x < 4) x = 4;
  if (x + w > win->w - 4) x = win->w - 4 - w;
  if (y + h > win->h - 4) y = win->h - 4 - h;
  if (y < 4) y = 4;

  cogito_draw_rect(x, y, w, h, bg, radius);
  if (border_w > 0) {
    cogito_draw_rect_lines(x, y, w, h, border, radius, border_w);
  }
  cogito_draw_text_size_node(target, x + pad_x, y + pad_y, t, text, font_size, false);
}
