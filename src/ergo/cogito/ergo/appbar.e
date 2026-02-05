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

