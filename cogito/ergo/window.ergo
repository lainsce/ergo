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

