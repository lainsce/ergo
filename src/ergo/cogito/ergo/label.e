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

