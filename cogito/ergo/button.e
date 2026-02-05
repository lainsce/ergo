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

