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

