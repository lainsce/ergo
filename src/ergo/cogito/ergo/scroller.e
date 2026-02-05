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

