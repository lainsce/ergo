class App {
    fun run(this, win = Window) (( -- )) {
        __cogito_run(this, win)
    }
    fun set_appid(this, id = string) (( -- )) {
        __cogito_app_set_appid(this, id)
    }
    fun set_accent_color(this, color = string, override_color = bool) (( -- )) {
        __cogito_app_set_accent_color(this, color, override_color)
    }
}

