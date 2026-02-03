-- Cogito GUI module (minimal, container-based)

-- Internal intrinsics
fun __cogito_app() (( App )) { }
fun __cogito_window(title = string, w = num, h = num) (( Window )) { }
fun __cogito_button(text = string) (( Button )) { }
fun __cogito_label(text = string) (( Label )) { }
fun __cogito_checkbox(text = string, group = any) (( Checkbox )) { }
fun __cogito_switch(text = string) (( Switch )) { }
fun __cogito_vstack() (( VStack )) { }
fun __cogito_hstack() (( HStack )) { }
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
fun __cogito_container_set_padding(node = any, left = num, top = num, right = num, bottom = num) (( -- )) { }
fun __cogito_window_set_autosize(win = Window, on = bool) (( -- )) { }
fun __cogito_button_set_text(btn = Button, text = string) (( -- )) { }
fun __cogito_checkbox_set_checked(cb = Checkbox, checked = bool) (( -- )) { }
fun __cogito_checkbox_get_checked(cb = Checkbox) (( bool )) { }
fun __cogito_switch_set_checked(sw = Switch, checked = bool) (( -- )) { }
fun __cogito_switch_get_checked(sw = Switch) (( bool )) { }
fun __cogito_checkbox_on_change(cb = Checkbox, handler = any) (( -- )) { }
fun __cogito_switch_on_change(sw = Switch, handler = any) (( -- )) { }
fun __cogito_list_on_select(list = List, handler = any) (( -- )) { }
fun __cogito_list_on_activate(list = List, handler = any) (( -- )) { }
fun __cogito_grid_on_select(grid = Grid, handler = any) (( -- )) { }
fun __cogito_grid_on_activate(grid = Grid, handler = any) (( -- )) { }
fun __cogito_button_on_click(btn = Button, handler = any) (( -- )) { }
fun __cogito_run(app = App, win = Window) (( -- )) { }

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
    fun build(this, builder = any) (( Window )) {
        __cogito_window_set_builder(this, builder)
        __cogito_build(this, builder)
        return this
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

fun vstack() (( VStack )) {
    return __cogito_vstack()
}

fun hstack() (( HStack )) {
    return __cogito_hstack()
}

fun list() (( List )) {
    return __cogito_list()
}

fun grid(cols = num) (( Grid )) {
    return __cogito_grid(cols)
}

fun label(text = string) (( Label )) {
    return __cogito_label(text)
}

fun button(text = string) (( Button )) {
    return __cogito_button(text)
}

fun checkbox(text = string, group = any) (( Checkbox )) {
    return __cogito_checkbox(text, group)
}

fun switch(text = string) (( Switch )) {
    return __cogito_switch(text)
}
