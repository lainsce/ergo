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

