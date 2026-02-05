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

