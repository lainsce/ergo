bring stdr
bring cogito

def button_label = @"Click me"
def ?clicks = 0

fun build_root(root = cogito.VStack) (( -- )) {
    let btn = cogito.button(button_label)
    root.add(btn)
    btn.on_click((b = cogito.Button) => {
        clicks = clicks + 1
        b.set_text(@"Clicked {clicks}")
    })
}

entry () (( -- )) {
    let app = cogito.app()
    let win = cogito.window()
    win.set_autosize(true)
    let root = cogito.build(cogito.vstack(), build_root)
    win.add(root)
    app.run(win)
}
