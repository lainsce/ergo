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

