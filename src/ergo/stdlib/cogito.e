-- Cogito GUI module (minimal, container-based)

-- Internal intrinsics
fun __cogito_app() (( App )) { }
fun __cogito_window(title = string, w = num, h = num) (( Window )) { }
fun __cogito_button(text = string) (( Button )) { }
fun __cogito_label(text = string) (( Label )) { }
fun __cogito_label_set_class(label = Label, cls = string) (( -- )) { }
fun __cogito_node_set_class(node = any, cls = string) (( -- )) { }
fun __cogito_node_set_a11y_label(node = any, label = string) (( -- )) { }
fun __cogito_node_set_a11y_role(node = any, role = string) (( -- )) { }
fun __cogito_node_set_tooltip(node = any, text = string) (( -- )) { }
fun __cogito_pointer_capture(node = any) (( -- )) { }
fun __cogito_pointer_release() (( -- )) { }
fun __cogito_label_set_wrap(label = Label, on = bool) (( -- )) { }
fun __cogito_label_set_ellipsis(label = Label, on = bool) (( -- )) { }
fun __cogito_label_set_align(label = Label, align = num) (( -- )) { }
fun __cogito_image(icon = string) (( Image )) { }
fun __cogito_image_set_icon(img = Image, icon = string) (( -- )) { }
fun __cogito_dialog(title = string) (( Dialog )) { }
fun __cogito_dialog_slot() (( DialogSlot )) { }
fun __cogito_dialog_slot_show(slot = DialogSlot, dialog = Dialog) (( -- )) { }
fun __cogito_dialog_slot_clear(slot = DialogSlot) (( -- )) { }
fun __cogito_window_set_dialog(win = Window, dialog = Dialog) (( -- )) { }
fun __cogito_window_clear_dialog(win = Window) (( -- )) { }
fun __cogito_node_window(node = any) (( Window )) { }
fun __cogito_checkbox(text = string, group = any) (( Checkbox )) { }
fun __cogito_switch(text = string) (( Switch )) { }
fun __cogito_textfield(text = string) (( TextField )) { }
fun __cogito_searchfield_set_text(sf = SearchField, text = string) (( -- )) { }
fun __cogito_searchfield_get_text(sf = SearchField) (( string )) { }
fun __cogito_searchfield_on_change(sf = SearchField, handler = any) (( -- )) { }
fun __cogito_textview(text = string) (( TextView )) { }
fun __cogito_searchfield(text = string) (( SearchField )) { }
fun __cogito_dropdown() (( Dropdown )) { }
fun __cogito_datepicker() (( DatePicker )) { }
fun __cogito_datepicker_on_change(dp = DatePicker, handler = any) (( -- )) { }
fun __cogito_stepper(min = num, max = num, value = num, step = num) (( Stepper )) { }
fun __cogito_slider(min = num, max = num, value = num) (( Slider )) { }
fun __cogito_tabs() (( Tabs )) { }
fun __cogito_segmented() (( SegmentedControl )) { }
fun __cogito_view_switcher() (( ViewSwitcher )) { }
fun __cogito_progress(value = num) (( Progress )) { }
fun __cogito_treeview() (( TreeView )) { }
fun __cogito_colorpicker() (( ColorPicker )) { }
fun __cogito_colorpicker_on_change(cp = ColorPicker, handler = any) (( -- )) { }
fun __cogito_toasts() (( Toasts )) { }
fun __cogito_toast(text = string) (( Toast )) { }
fun __cogito_toolbar() (( BottomToolbar )) { }
fun __cogito_vstack() (( VStack )) { }
fun __cogito_hstack() (( HStack )) { }
fun __cogito_zstack() (( ZStack )) { }
fun __cogito_fixed() (( Fixed )) { }
fun __cogito_scroller() (( Scroller )) { }
fun __cogito_list() (( List )) { }
fun __cogito_grid(cols = num) (( Grid )) { }
fun __cogito_container_add(parent = any, child = any) (( -- )) { }
fun __cogito_container_set_margins(node = any, left = num, top = num, right = num, bottom = num) (( -- )) { }
fun __cogito_build(node = any, builder = any) (( -- )) { }
fun __cogito_window_set_builder(win = Window, builder = any) (( -- )) { }
fun __cogito_state_new(value = any) (( State )) { }
fun __cogito_state_get(state = State) (( any )) { }
fun __cogito_state_set(state = State, value = any) (( -- )) { }
fun __cogito_container_set_align(node = any, align = num) (( -- )) { }
fun __cogito_container_set_halign(node = any, align = num) (( -- )) { }
fun __cogito_container_set_valign(node = any, align = num) (( -- )) { }
fun __cogito_container_set_padding(node = any, left = num, top = num, right = num, bottom = num) (( -- )) { }
fun __cogito_fixed_set_pos(fixed = Fixed, child = any, x = num, y = num) (( -- )) { }
fun __cogito_scroller_set_axes(scroller = Scroller, h = bool, v = bool) (( -- )) { }
fun __cogito_grid_set_gap(grid = Grid, x = num, y = num) (( -- )) { }
fun __cogito_grid_set_span(child = any, col_span = num, row_span = num) (( -- )) { }
fun __cogito_grid_set_align(child = any, halign = num, valign = num) (( -- )) { }
fun __cogito_node_set_disabled(node = any, on = bool) (( -- )) { }
fun __cogito_node_set_id(node = any, id = string) (( -- )) { }
fun __cogito_window_set_autosize(win = Window, on = bool) (( -- )) { }
fun __cogito_window_set_resizable(win = Window, on = bool) (( -- )) { }
fun __cogito_appbar(title = string, subtitle = string) (( AppBar )) { }
fun __cogito_appbar_add_button(appbar = AppBar, icon = string, handler = any) (( Button )) { }
fun __cogito_appbar_set_controls(appbar = AppBar, layout = string) (( -- )) { }
fun __cogito_button_set_text(btn = Button, text = string) (( -- )) { }
fun __cogito_button_add_menu(btn = Button, label = string, handler = any) (( -- )) { }
fun __cogito_iconbtn_add_menu(btn = Button, label = string, handler = any) (( -- )) { }
fun __cogito_checkbox_set_checked(cb = Checkbox, checked = bool) (( -- )) { }
fun __cogito_checkbox_get_checked(cb = Checkbox) (( bool )) { }
fun __cogito_switch_set_checked(sw = Switch, checked = bool) (( -- )) { }
fun __cogito_switch_get_checked(sw = Switch) (( bool )) { }
fun __cogito_checkbox_on_change(cb = Checkbox, handler = any) (( -- )) { }
fun __cogito_switch_on_change(sw = Switch, handler = any) (( -- )) { }
fun __cogito_textfield_set_text(tf = TextField, text = string) (( -- )) { }
fun __cogito_textfield_get_text(tf = TextField) (( string )) { }
fun __cogito_textfield_on_change(tf = TextField, handler = any) (( -- )) { }
fun __cogito_textview_set_text(tv = TextView, text = string) (( -- )) { }
fun __cogito_textview_get_text(tv = TextView) (( string )) { }
fun __cogito_textview_on_change(tv = TextView, handler = any) (( -- )) { }
fun __cogito_dropdown_set_items(dd = Dropdown, items = [any]) (( -- )) { }
fun __cogito_dropdown_set_selected(dd = Dropdown, idx = num) (( -- )) { }
fun __cogito_dropdown_get_selected(dd = Dropdown) (( num )) { }
fun __cogito_dropdown_on_change(dd = Dropdown, handler = any) (( -- )) { }
fun __cogito_slider_set_value(sl = Slider, value = num) (( -- )) { }
fun __cogito_slider_get_value(sl = Slider) (( num )) { }
fun __cogito_slider_on_change(sl = Slider, handler = any) (( -- )) { }
fun __cogito_tabs_set_items(tabs = Tabs, items = [any]) (( -- )) { }
fun __cogito_tabs_set_ids(tabs = Tabs, ids = [any]) (( -- )) { }
fun __cogito_tabs_set_selected(tabs = Tabs, idx = num) (( -- )) { }
fun __cogito_tabs_get_selected(tabs = Tabs) (( num )) { }
fun __cogito_tabs_on_change(tabs = Tabs, handler = any) (( -- )) { }
fun __cogito_tabs_bind(tabs = Tabs, view = ViewSwitcher) (( -- )) { }
fun __cogito_view_switcher_set_active(view = ViewSwitcher, id = string) (( -- )) { }
fun __cogito_progress_set_value(p = Progress, value = num) (( -- )) { }
fun __cogito_progress_get_value(p = Progress) (( num )) { }
fun __cogito_toast_set_text(t = Toast, text = string) (( -- )) { }
fun __cogito_toast_on_click(t = Toast, handler = any) (( -- )) { }
fun __cogito_list_on_select(list = List, handler = any) (( -- )) { }
fun __cogito_list_on_activate(list = List, handler = any) (( -- )) { }
fun __cogito_grid_on_select(grid = Grid, handler = any) (( -- )) { }
fun __cogito_grid_on_activate(grid = Grid, handler = any) (( -- )) { }
fun __cogito_button_on_click(btn = Button, handler = any) (( -- )) { }
fun __cogito_run(app = App, win = Window) (( -- )) { }
fun __cogito_load_css(path = string) (( -- )) { }

class App {
    fun run(this, win = Window) (( -- )) {
        __cogito_run(this, win)
    }
}

class Window {
    fun add(this, child = any) (( -- )) {
        __cogito_container_add(this, child)
    }
    fun set_margins(this, left = num, top = num, right = num, bottom = num) (( -- )) {
        __cogito_container_set_margins(this, left, top, right, bottom)
    }
    fun set_padding(this, left = num, top = num, right = num, bottom = num) (( -- )) {
        __cogito_container_set_padding(this, left, top, right, bottom)
    }
    fun set_autosize(this, on = bool) (( -- )) {
        __cogito_window_set_autosize(this, on)
    }
    fun set_resizable(this, on = bool) (( -- )) {
        __cogito_window_set_resizable(this, on)
    }
    fun set_a11y_label(this, label = string) (( -- )) {
        __cogito_node_set_a11y_label(this, label)
    }
    fun set_disabled(this, on = bool) (( -- )) {
        __cogito_node_set_disabled(this, on)
    }
    fun set_dialog(this, dialog = Dialog) (( -- )) {
        __cogito_window_set_dialog(this, dialog)
    }
    fun clear_dialog(this) (( -- )) {
        __cogito_window_clear_dialog(this)
    }
    fun build(this, builder = any) (( Window )) {
        __cogito_window_set_builder(this, builder)
        __cogito_build(this, builder)
        return this
    }
}

class AppBar {
    fun add_button(this, icon = string, handler = any) (( Button )) {
        return __cogito_appbar_add_button(this, icon, handler)
    }
    fun set_window_controls(this, layout = string) (( -- )) {
        __cogito_appbar_set_controls(this, layout)
    }
    fun set_disabled(this, on = bool) (( -- )) {
        __cogito_node_set_disabled(this, on)
    }
}

class Image {
    fun set_icon(this, icon = string) (( -- )) {
        __cogito_image_set_icon(this, icon)
    }
    fun set_disabled(this, on = bool) (( -- )) {
        __cogito_node_set_disabled(this, on)
    }
}

class Dialog {
    fun add(this, child = any) (( -- )) {
        __cogito_container_add(this, child)
    }
    fun set_padding(this, left = num, top = num, right = num, bottom = num) (( -- )) {
        __cogito_container_set_padding(this, left, top, right, bottom)
    }
    fun set_margins(this, left = num, top = num, right = num, bottom = num) (( -- )) {
        __cogito_container_set_margins(this, left, top, right, bottom)
    }
    fun build(this, builder = any) (( Dialog )) {
        __cogito_build(this, builder)
        return this
    }
    fun window(this) (( Window )) {
        return __cogito_node_window(this)
    }
    fun set_disabled(this, on = bool) (( -- )) {
        __cogito_node_set_disabled(this, on)
    }
}

class DialogSlot {
    fun show(this, dialog = Dialog) (( -- )) {
        __cogito_dialog_slot_show(this, dialog)
    }
    fun clear(this) (( -- )) {
        __cogito_dialog_slot_clear(this)
    }
    fun set_disabled(this, on = bool) (( -- )) {
        __cogito_node_set_disabled(this, on)
    }
}

class VStack {
    fun add(this, child = any) (( -- )) {
        __cogito_container_add(this, child)
    }
    fun set_margins(this, left = num, top = num, right = num, bottom = num) (( -- )) {
        __cogito_container_set_margins(this, left, top, right, bottom)
    }
    fun set_padding(this, left = num, top = num, right = num, bottom = num) (( -- )) {
        __cogito_container_set_padding(this, left, top, right, bottom)
    }
    fun set_align(this, align = num) (( -- )) {
        __cogito_container_set_align(this, align)
    }
    fun halign(this, align = num) (( -- )) {
        __cogito_container_set_halign(this, align)
    }
    fun valign(this, align = num) (( -- )) {
        __cogito_container_set_valign(this, align)
    }
    fun align_begin(this) (( -- )) {
        __cogito_container_set_align(this, 0)
    }
    fun align_center(this) (( -- )) {
        __cogito_container_set_align(this, 1)
    }
    fun align_end(this) (( -- )) {
        __cogito_container_set_align(this, 2)
    }
    fun build(this, builder = any) (( VStack )) {
        __cogito_build(this, builder)
        return this
    }
    fun set_disabled(this, on = bool) (( -- )) {
        __cogito_node_set_disabled(this, on)
    }
}

class HStack {
    fun add(this, child = any) (( -- )) {
        __cogito_container_add(this, child)
    }
    fun set_margins(this, left = num, top = num, right = num, bottom = num) (( -- )) {
        __cogito_container_set_margins(this, left, top, right, bottom)
    }
    fun set_padding(this, left = num, top = num, right = num, bottom = num) (( -- )) {
        __cogito_container_set_padding(this, left, top, right, bottom)
    }
    fun set_align(this, align = num) (( -- )) {
        __cogito_container_set_align(this, align)
    }
    fun halign(this, align = num) (( -- )) {
        __cogito_container_set_halign(this, align)
    }
    fun valign(this, align = num) (( -- )) {
        __cogito_container_set_valign(this, align)
    }
    fun align_begin(this) (( -- )) {
        __cogito_container_set_align(this, 0)
    }
    fun align_center(this) (( -- )) {
        __cogito_container_set_align(this, 1)
    }
    fun align_end(this) (( -- )) {
        __cogito_container_set_align(this, 2)
    }
    fun build(this, builder = any) (( HStack )) {
        __cogito_build(this, builder)
        return this
    }
    fun set_disabled(this, on = bool) (( -- )) {
        __cogito_node_set_disabled(this, on)
    }
}

class ZStack {
    fun add(this, child = any) (( -- )) {
        __cogito_container_add(this, child)
    }
    fun set_margins(this, left = num, top = num, right = num, bottom = num) (( -- )) {
        __cogito_container_set_margins(this, left, top, right, bottom)
    }
    fun set_padding(this, left = num, top = num, right = num, bottom = num) (( -- )) {
        __cogito_container_set_padding(this, left, top, right, bottom)
    }
    fun set_align(this, align = num) (( -- )) {
        __cogito_container_set_align(this, align)
    }
    fun halign(this, align = num) (( -- )) {
        __cogito_container_set_halign(this, align)
    }
    fun valign(this, align = num) (( -- )) {
        __cogito_container_set_valign(this, align)
    }
    fun align_begin(this) (( -- )) {
        __cogito_container_set_align(this, 0)
    }
    fun align_center(this) (( -- )) {
        __cogito_container_set_align(this, 1)
    }
    fun align_end(this) (( -- )) {
        __cogito_container_set_align(this, 2)
    }
    fun build(this, builder = any) (( ZStack )) {
        __cogito_build(this, builder)
        return this
    }
    fun set_disabled(this, on = bool) (( -- )) {
        __cogito_node_set_disabled(this, on)
    }
}

class Fixed {
    fun add(this, child = any) (( -- )) {
        __cogito_container_add(this, child)
    }
    fun set_pos(this, child = any, x = num, y = num) (( -- )) {
        __cogito_fixed_set_pos(this, child, x, y)
    }
    fun set_padding(this, left = num, top = num, right = num, bottom = num) (( -- )) {
        __cogito_container_set_padding(this, left, top, right, bottom)
    }
    fun build(this, builder = any) (( Fixed )) {
        __cogito_build(this, builder)
        return this
    }
    fun set_disabled(this, on = bool) (( -- )) {
        __cogito_node_set_disabled(this, on)
    }
}

class Scroller {
    fun add(this, child = any) (( -- )) {
        __cogito_container_add(this, child)
    }
    fun set_axes(this, h = bool, v = bool) (( -- )) {
        __cogito_scroller_set_axes(this, h, v)
    }
    fun set_padding(this, left = num, top = num, right = num, bottom = num) (( -- )) {
        __cogito_container_set_padding(this, left, top, right, bottom)
    }
    fun build(this, builder = any) (( Scroller )) {
        __cogito_build(this, builder)
        return this
    }
    fun set_disabled(this, on = bool) (( -- )) {
        __cogito_node_set_disabled(this, on)
    }
}

class List {
    fun add(this, child = any) (( -- )) {
        __cogito_container_add(this, child)
    }
    fun set_margins(this, left = num, top = num, right = num, bottom = num) (( -- )) {
        __cogito_container_set_margins(this, left, top, right, bottom)
    }
    fun set_padding(this, left = num, top = num, right = num, bottom = num) (( -- )) {
        __cogito_container_set_padding(this, left, top, right, bottom)
    }
    fun set_align(this, align = num) (( -- )) {
        __cogito_container_set_align(this, align)
    }
    fun halign(this, align = num) (( -- )) {
        __cogito_container_set_halign(this, align)
    }
    fun valign(this, align = num) (( -- )) {
        __cogito_container_set_valign(this, align)
    }
    fun align_begin(this) (( -- )) {
        __cogito_container_set_align(this, 0)
    }
    fun align_center(this) (( -- )) {
        __cogito_container_set_align(this, 1)
    }
    fun align_end(this) (( -- )) {
        __cogito_container_set_align(this, 2)
    }
    fun on_select(this, handler = any) (( -- )) {
        __cogito_list_on_select(this, handler)
    }
    fun on_activate(this, handler = any) (( -- )) {
        __cogito_list_on_activate(this, handler)
    }
    fun build(this, builder = any) (( List )) {
        __cogito_build(this, builder)
        return this
    }
    fun set_disabled(this, on = bool) (( -- )) {
        __cogito_node_set_disabled(this, on)
    }
}

class Grid {
    fun add(this, child = any) (( -- )) {
        __cogito_container_add(this, child)
    }
    fun set_margins(this, left = num, top = num, right = num, bottom = num) (( -- )) {
        __cogito_container_set_margins(this, left, top, right, bottom)
    }
    fun set_padding(this, left = num, top = num, right = num, bottom = num) (( -- )) {
        __cogito_container_set_padding(this, left, top, right, bottom)
    }
    fun set_gap(this, x = num, y = num) (( -- )) {
        __cogito_grid_set_gap(this, x, y)
    }
    fun set_span(this, child = any, col_span = num, row_span = num) (( -- )) {
        __cogito_grid_set_span(child, col_span, row_span)
    }
    fun set_cell_align(this, child = any, halign = num, valign = num) (( -- )) {
        __cogito_grid_set_align(child, halign, valign)
    }
    fun set_align(this, align = num) (( -- )) {
        __cogito_container_set_align(this, align)
    }
    fun halign(this, align = num) (( -- )) {
        __cogito_container_set_halign(this, align)
    }
    fun valign(this, align = num) (( -- )) {
        __cogito_container_set_valign(this, align)
    }
    fun align_begin(this) (( -- )) {
        __cogito_container_set_align(this, 0)
    }
    fun align_center(this) (( -- )) {
        __cogito_container_set_align(this, 1)
    }
    fun align_end(this) (( -- )) {
        __cogito_container_set_align(this, 2)
    }
    fun on_select(this, handler = any) (( -- )) {
        __cogito_grid_on_select(this, handler)
    }
    fun on_activate(this, handler = any) (( -- )) {
        __cogito_grid_on_activate(this, handler)
    }
    fun build(this, builder = any) (( Grid )) {
        __cogito_build(this, builder)
        return this
    }
    fun set_disabled(this, on = bool) (( -- )) {
        __cogito_node_set_disabled(this, on)
    }
}

class Label {
    fun set_margins(this, left = num, top = num, right = num, bottom = num) (( -- )) {
        __cogito_container_set_margins(this, left, top, right, bottom)
    }
    fun set_padding(this, left = num, top = num, right = num, bottom = num) (( -- )) {
        __cogito_container_set_padding(this, left, top, right, bottom)
    }
    fun set_align(this, align = num) (( -- )) {
        __cogito_container_set_align(this, align)
    }
    fun align_begin(this) (( -- )) {
        __cogito_container_set_align(this, 0)
    }
    fun align_center(this) (( -- )) {
        __cogito_container_set_align(this, 1)
    }
    fun align_end(this) (( -- )) {
        __cogito_container_set_align(this, 2)
    }
    fun set_class(this, cls = string) (( -- )) {
        __cogito_label_set_class(this, cls)
    }
    fun set_wrap(this, on = bool) (( -- )) {
        __cogito_label_set_wrap(this, on)
    }
    fun set_ellipsis(this, on = bool) (( -- )) {
        __cogito_label_set_ellipsis(this, on)
    }
    fun set_text_align(this, align = num) (( -- )) {
        __cogito_label_set_align(this, align)
    }
    fun set_disabled(this, on = bool) (( -- )) {
        __cogito_node_set_disabled(this, on)
    }
}

class Button {
    fun set_margins(this, left = num, top = num, right = num, bottom = num) (( -- )) {
        __cogito_container_set_margins(this, left, top, right, bottom)
    }
    fun set_padding(this, left = num, top = num, right = num, bottom = num) (( -- )) {
        __cogito_container_set_padding(this, left, top, right, bottom)
    }
    fun set_align(this, align = num) (( -- )) {
        __cogito_container_set_align(this, align)
    }
    fun align_begin(this) (( -- )) {
        __cogito_container_set_align(this, 0)
    }
    fun align_center(this) (( -- )) {
        __cogito_container_set_align(this, 1)
    }
    fun align_end(this) (( -- )) {
        __cogito_container_set_align(this, 2)
    }
    fun set_text(this, text = string) (( -- )) {
        __cogito_button_set_text(this, text)
    }
    fun on_click(this, handler = any) (( -- )) {
        __cogito_button_on_click(this, handler)
    }
    fun add_menu(this, label = string, handler = any) (( -- )) {
        __cogito_button_add_menu(this, label, handler)
    }
    fun window(this) (( Window )) {
        return __cogito_node_window(this)
    }
    fun set_disabled(this, on = bool) (( -- )) {
        __cogito_node_set_disabled(this, on)
    }
}

class Checkbox {
    fun set_margins(this, left = num, top = num, right = num, bottom = num) (( -- )) {
        __cogito_container_set_margins(this, left, top, right, bottom)
    }
    fun set_padding(this, left = num, top = num, right = num, bottom = num) (( -- )) {
        __cogito_container_set_padding(this, left, top, right, bottom)
    }
    fun set_align(this, align = num) (( -- )) {
        __cogito_container_set_align(this, align)
    }
    fun align_begin(this) (( -- )) {
        __cogito_container_set_align(this, 0)
    }
    fun align_center(this) (( -- )) {
        __cogito_container_set_align(this, 1)
    }
    fun align_end(this) (( -- )) {
        __cogito_container_set_align(this, 2)
    }
    fun set_checked(this, checked = bool) (( -- )) {
        __cogito_checkbox_set_checked(this, checked)
    }
    fun checked(this) (( bool )) {
        return __cogito_checkbox_get_checked(this)
    }
    fun on_change(this, handler = any) (( -- )) {
        __cogito_checkbox_on_change(this, handler)
    }
    fun window(this) (( Window )) {
        return __cogito_node_window(this)
    }
    fun set_disabled(this, on = bool) (( -- )) {
        __cogito_node_set_disabled(this, on)
    }
}

class Switch {
    fun set_margins(this, left = num, top = num, right = num, bottom = num) (( -- )) {
        __cogito_container_set_margins(this, left, top, right, bottom)
    }
    fun set_padding(this, left = num, top = num, right = num, bottom = num) (( -- )) {
        __cogito_container_set_padding(this, left, top, right, bottom)
    }
    fun set_align(this, align = num) (( -- )) {
        __cogito_container_set_align(this, align)
    }
    fun align_begin(this) (( -- )) {
        __cogito_container_set_align(this, 0)
    }
    fun align_center(this) (( -- )) {
        __cogito_container_set_align(this, 1)
    }
    fun align_end(this) (( -- )) {
        __cogito_container_set_align(this, 2)
    }
    fun set_checked(this, checked = bool) (( -- )) {
        __cogito_switch_set_checked(this, checked)
    }
    fun checked(this) (( bool )) {
        return __cogito_switch_get_checked(this)
    }
    fun on_change(this, handler = any) (( -- )) {
        __cogito_switch_on_change(this, handler)
    }
    fun window(this) (( Window )) {
        return __cogito_node_window(this)
    }
    fun set_disabled(this, on = bool) (( -- )) {
        __cogito_node_set_disabled(this, on)
    }
}

class SearchField {
    fun set_text(this, text = string) (( -- )) {
        __cogito_searchfield_set_text(this, text)
    }
    fun text(this) (( string )) {
        return __cogito_searchfield_get_text(this)
    }
    fun on_change(this, handler = any) (( -- )) {
        __cogito_searchfield_on_change(this, handler)
    }
}

class TextField {
    fun set_margins(this, left = num, top = num, right = num, bottom = num) (( -- )) {
        __cogito_container_set_margins(this, left, top, right, bottom)
    }
    fun set_padding(this, left = num, top = num, right = num, bottom = num) (( -- )) {
        __cogito_container_set_padding(this, left, top, right, bottom)
    }
    fun set_align(this, align = num) (( -- )) {
        __cogito_container_set_align(this, align)
    }
    fun align_begin(this) (( -- )) {
        __cogito_container_set_align(this, 0)
    }
    fun align_center(this) (( -- )) {
        __cogito_container_set_align(this, 1)
    }
    fun align_end(this) (( -- )) {
        __cogito_container_set_align(this, 2)
    }
    fun set_text(this, text = string) (( -- )) {
        __cogito_textfield_set_text(this, text)
    }
    fun text(this) (( string )) {
        return __cogito_textfield_get_text(this)
    }
    fun on_change(this, handler = any) (( -- )) {
        __cogito_textfield_on_change(this, handler)
    }
    fun set_disabled(this, on = bool) (( -- )) {
        __cogito_node_set_disabled(this, on)
    }
}

class TextView {
    fun set_margins(this, left = num, top = num, right = num, bottom = num) (( -- )) {
        __cogito_container_set_margins(this, left, top, right, bottom)
    }
    fun set_padding(this, left = num, top = num, right = num, bottom = num) (( -- )) {
        __cogito_container_set_padding(this, left, top, right, bottom)
    }
    fun set_align(this, align = num) (( -- )) {
        __cogito_container_set_align(this, align)
    }
    fun align_begin(this) (( -- )) {
        __cogito_container_set_align(this, 0)
    }
    fun align_center(this) (( -- )) {
        __cogito_container_set_align(this, 1)
    }
    fun align_end(this) (( -- )) {
        __cogito_container_set_align(this, 2)
    }
    fun set_text(this, text = string) (( -- )) {
        __cogito_textview_set_text(this, text)
    }
    fun text(this) (( string )) {
        return __cogito_textview_get_text(this)
    }
    fun on_change(this, handler = any) (( -- )) {
        __cogito_textview_on_change(this, handler)
    }
    fun set_disabled(this, on = bool) (( -- )) {
        __cogito_node_set_disabled(this, on)
    }
}

class DatePicker {
    fun set_date(this, y = num, m = num, d = num) (( -- )) { }
    fun date(this) (( [any] )) { }
    fun on_change(this, handler = any) (( -- )) {
        __cogito_datepicker_on_change(this, handler)
    }
    fun set_a11y_label(this, label = string) (( -- )) {
        __cogito_node_set_a11y_label(this, label)
    }
    fun set_a11y_role(this, role = string) (( -- )) {
        __cogito_node_set_a11y_role(this, role)
    }
    fun set_disabled(this, on = bool) (( -- )) {
        __cogito_node_set_disabled(this, on)
    }
}

class Stepper {
    fun set_value(this, value = num) (( -- )) { }
    fun value(this) (( num )) { }
    fun on_change(this, handler = any) (( -- )) { }
}

class Dropdown {
    fun set_margins(this, left = num, top = num, right = num, bottom = num) (( -- )) {
        __cogito_container_set_margins(this, left, top, right, bottom)
    }
    fun set_padding(this, left = num, top = num, right = num, bottom = num) (( -- )) {
        __cogito_container_set_padding(this, left, top, right, bottom)
    }
    fun set_align(this, align = num) (( -- )) {
        __cogito_container_set_align(this, align)
    }
    fun align_begin(this) (( -- )) {
        __cogito_container_set_align(this, 0)
    }
    fun align_center(this) (( -- )) {
        __cogito_container_set_align(this, 1)
    }
    fun align_end(this) (( -- )) {
        __cogito_container_set_align(this, 2)
    }
    fun set_items(this, items = [any]) (( -- )) {
        __cogito_dropdown_set_items(this, items)
    }
    fun set_selected(this, idx = num) (( -- )) {
        __cogito_dropdown_set_selected(this, idx)
    }
    fun selected(this) (( num )) {
        return __cogito_dropdown_get_selected(this)
    }
    fun on_change(this, handler = any) (( -- )) {
        __cogito_dropdown_on_change(this, handler)
    }
    fun set_disabled(this, on = bool) (( -- )) {
        __cogito_node_set_disabled(this, on)
    }
}

class Slider {
    fun set_margins(this, left = num, top = num, right = num, bottom = num) (( -- )) {
        __cogito_container_set_margins(this, left, top, right, bottom)
    }
    fun set_padding(this, left = num, top = num, right = num, bottom = num) (( -- )) {
        __cogito_container_set_padding(this, left, top, right, bottom)
    }
    fun set_align(this, align = num) (( -- )) {
        __cogito_container_set_align(this, align)
    }
    fun align_begin(this) (( -- )) {
        __cogito_container_set_align(this, 0)
    }
    fun align_center(this) (( -- )) {
        __cogito_container_set_align(this, 1)
    }
    fun align_end(this) (( -- )) {
        __cogito_container_set_align(this, 2)
    }
    fun set_value(this, value = num) (( -- )) {
        __cogito_slider_set_value(this, value)
    }
    fun value(this) (( num )) {
        return __cogito_slider_get_value(this)
    }
    fun on_change(this, handler = any) (( -- )) {
        __cogito_slider_on_change(this, handler)
    }
    fun set_disabled(this, on = bool) (( -- )) {
        __cogito_node_set_disabled(this, on)
    }
}

class Tabs {
    fun set_margins(this, left = num, top = num, right = num, bottom = num) (( -- )) {
        __cogito_container_set_margins(this, left, top, right, bottom)
    }
    fun set_padding(this, left = num, top = num, right = num, bottom = num) (( -- )) {
        __cogito_container_set_padding(this, left, top, right, bottom)
    }
    fun set_align(this, align = num) (( -- )) {
        __cogito_container_set_align(this, align)
    }
    fun set_items(this, items = [any]) (( -- )) {
        __cogito_tabs_set_items(this, items)
    }
    fun set_ids(this, ids = [any]) (( -- )) {
        __cogito_tabs_set_ids(this, ids)
    }
    fun set_selected(this, idx = num) (( -- )) {
        __cogito_tabs_set_selected(this, idx)
    }
    fun selected(this) (( num )) {
        return __cogito_tabs_get_selected(this)
    }
    fun on_change(this, handler = any) (( -- )) {
        __cogito_tabs_on_change(this, handler)
    }
    fun bind(this, view = ViewSwitcher) (( -- )) {
        __cogito_tabs_bind(this, view)
    }
    fun set_disabled(this, on = bool) (( -- )) {
        __cogito_node_set_disabled(this, on)
    }
}

class SegmentedControl {
    fun set_items(this, items = [any]) (( -- )) { }
    fun set_selected(this, idx = num) (( -- )) { }
    fun selected(this) (( num )) { }
    fun on_change(this, handler = any) (( -- )) { }
}

class ViewSwitcher {
    fun add(this, child = any) (( -- )) {
        __cogito_container_add(this, child)
    }
    fun set_margins(this, left = num, top = num, right = num, bottom = num) (( -- )) {
        __cogito_container_set_margins(this, left, top, right, bottom)
    }
    fun set_padding(this, left = num, top = num, right = num, bottom = num) (( -- )) {
        __cogito_container_set_padding(this, left, top, right, bottom)
    }
    fun set_align(this, align = num) (( -- )) {
        __cogito_container_set_align(this, align)
    }
    fun halign(this, align = num) (( -- )) {
        __cogito_container_set_halign(this, align)
    }
    fun valign(this, align = num) (( -- )) {
        __cogito_container_set_valign(this, align)
    }
    fun set_active(this, id = string) (( -- )) {
        __cogito_view_switcher_set_active(this, id)
    }
    fun build(this, builder = any) (( ViewSwitcher )) {
        __cogito_build(this, builder)
        return this
    }
    fun set_disabled(this, on = bool) (( -- )) {
        __cogito_node_set_disabled(this, on)
    }
}

class Progress {
    fun set_margins(this, left = num, top = num, right = num, bottom = num) (( -- )) {
        __cogito_container_set_margins(this, left, top, right, bottom)
    }
    fun set_padding(this, left = num, top = num, right = num, bottom = num) (( -- )) {
        __cogito_container_set_padding(this, left, top, right, bottom)
    }
    fun set_align(this, align = num) (( -- )) {
        __cogito_container_set_align(this, align)
    }
    fun set_value(this, value = num) (( -- )) {
        __cogito_progress_set_value(this, value)
    }
    fun value(this) (( num )) {
        return __cogito_progress_get_value(this)
    }
    fun set_disabled(this, on = bool) (( -- )) {
        __cogito_node_set_disabled(this, on)
    }
}

class TreeView {
    fun add(this, child = any) (( -- )) {
        __cogito_container_add(this, child)
    }
}

class ColorPicker {
    fun set_hex(this, text = string) (( -- )) { }
    fun hex(this) (( string )) { }
    fun on_change(this, handler = any) (( -- )) {
        __cogito_colorpicker_on_change(this, handler)
    }
    fun set_a11y_label(this, label = string) (( -- )) {
        __cogito_node_set_a11y_label(this, label)
    }
    fun set_a11y_role(this, role = string) (( -- )) {
        __cogito_node_set_a11y_role(this, role)
    }
    fun set_disabled(this, on = bool) (( -- )) {
        __cogito_node_set_disabled(this, on)
    }
}

class Toasts {
    fun add(this, child = any) (( -- )) {
        __cogito_container_add(this, child)
    }
    fun set_margins(this, left = num, top = num, right = num, bottom = num) (( -- )) {
        __cogito_container_set_margins(this, left, top, right, bottom)
    }
    fun set_padding(this, left = num, top = num, right = num, bottom = num) (( -- )) {
        __cogito_container_set_padding(this, left, top, right, bottom)
    }
    fun set_align(this, align = num) (( -- )) {
        __cogito_container_set_align(this, align)
    }
    fun build(this, builder = any) (( Toasts )) {
        __cogito_build(this, builder)
        return this
    }
    fun set_disabled(this, on = bool) (( -- )) {
        __cogito_node_set_disabled(this, on)
    }
}

class Toast {
    fun set_margins(this, left = num, top = num, right = num, bottom = num) (( -- )) {
        __cogito_container_set_margins(this, left, top, right, bottom)
    }
    fun set_padding(this, left = num, top = num, right = num, bottom = num) (( -- )) {
        __cogito_container_set_padding(this, left, top, right, bottom)
    }
    fun set_align(this, align = num) (( -- )) {
        __cogito_container_set_align(this, align)
    }
    fun set_text(this, text = string) (( -- )) {
        __cogito_toast_set_text(this, text)
    }
    fun on_click(this, handler = any) (( -- )) {
        __cogito_toast_on_click(this, handler)
    }
    fun set_disabled(this, on = bool) (( -- )) {
        __cogito_node_set_disabled(this, on)
    }
}

class BottomToolbar {
    fun add(this, child = any) (( -- )) {
        __cogito_container_add(this, child)
    }
}

class State {
    fun get(this) (( any )) {
        return __cogito_state_get(this)
    }
    fun set(this, value = any) (( -- )) {
        __cogito_state_set(this, value)
    }
}

fun app() (( App )) {
    return __cogito_app()
}

fun load_css(path = string) (( -- )) {
    __cogito_load_css(path)
}
fun set_class(node = any, cls = string) (( -- )) {
    __cogito_node_set_class(node, cls)
}
fun set_a11y_label(node = any, label = string) (( -- )) {
    __cogito_node_set_a11y_label(node, label)
}
fun set_a11y_role(node = any, role = string) (( -- )) {
    __cogito_node_set_a11y_role(node, role)
}
fun set_tooltip(node = any, text = string) (( -- )) {
    __cogito_node_set_tooltip(node, text)
}
fun pointer_capture(node = any) (( -- )) {
    __cogito_pointer_capture(node)
}
fun pointer_release() (( -- )) {
    __cogito_pointer_release()
}

fun window() (( Window )) {
    return __cogito_window(@"Cogito", 360, 296)
}

fun window_title(title = string) (( Window )) {
    return __cogito_window(title, 360, 296)
}

fun window_size(title = string, w = num, h = num) (( Window )) {
    return __cogito_window(title, w, h)
}

fun build(node = any, builder = any) (( any )) {
    __cogito_build(node, builder)
    return node
}

fun state(value = any) (( State )) {
    return __cogito_state_new(value)
}

fun set_id(node = any, id = string) (( -- )) {
    __cogito_node_set_id(node, id)
}

fun vstack() (( VStack )) {
    return __cogito_vstack()
}

fun hstack() (( HStack )) {
    return __cogito_hstack()
}

fun zstack() (( ZStack )) {
    return __cogito_zstack()
}

fun fixed() (( Fixed )) {
    return __cogito_fixed()
}

fun scroller() (( Scroller )) {
    return __cogito_scroller()
}

fun list() (( List )) {
    return __cogito_list()
}

fun grid(cols = num) (( Grid )) {
    return __cogito_grid(cols)
}

fun tabs() (( Tabs )) {
    return __cogito_tabs()
}

fun view_switcher() (( ViewSwitcher )) {
    return __cogito_view_switcher()
}

fun progress(value = num) (( Progress )) {
    return __cogito_progress(value)
}

fun toasts() (( Toasts )) {
    return __cogito_toasts()
}

fun toast(text = string) (( Toast )) {
    return __cogito_toast(text)
}

fun label(text = string) (( Label )) {
    return __cogito_label(text)
}

fun image(icon = string) (( Image )) {
    return __cogito_image(icon)
}

fun dialog(title = string) (( Dialog )) {
    return __cogito_dialog(title)
}

fun dialog_slot() (( DialogSlot )) {
    return __cogito_dialog_slot()
}

fun button(text = string) (( Button )) {
    return __cogito_button(text)
}

fun appbar(title = string, subtitle = string) (( AppBar )) {
    return __cogito_appbar(title, subtitle)
}

fun checkbox(text = string, group = any) (( Checkbox )) {
    return __cogito_checkbox(text, group)
}

fun switch(text = string) (( Switch )) {
    return __cogito_switch(text)
}

fun textfield(text = string) (( TextField )) {
    return __cogito_textfield(text)
}

fun searchfield(text = string) (( SearchField )) {
    return __cogito_searchfield(text)
}

fun textview(text = string) (( TextView )) {
    return __cogito_textview(text)
}

fun dropdown() (( Dropdown )) {
    return __cogito_dropdown()
}

fun datepicker() (( DatePicker )) {
    return __cogito_datepicker()
}

fun stepper(min = num, max = num, value = num, step = num) (( Stepper )) {
    return __cogito_stepper(min, max, value, step)
}

fun slider(min = num, max = num, value = num) (( Slider )) {
    return __cogito_slider(min, max, value)
}

fun segmented() (( SegmentedControl )) {
    return __cogito_segmented()
}

fun treeview() (( TreeView )) {
    return __cogito_treeview()
}

fun colorpicker() (( ColorPicker )) {
    return __cogito_colorpicker()
}

fun bottom_toolbar() (( BottomToolbar )) {
    return __cogito_toolbar()
}
