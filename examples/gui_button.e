bring stdr
bring cogito

fun on_click(btn = cogito.Button) (( -- )) {
    btn.set_text(@"Clicked!")
}

fun build_root(root = cogito.VStack) (( -- )) {
    let btn = cogito.button(@"Click me")
    root.add(btn)
    btn.on_click(on_click)
}

entry () (( -- )) {
    cogito.load_css(@"examples/cogito_default.css")
    let app = cogito.app()
    let win = cogito.window()
    win.set_autosize(true)
    let root = cogito.build(cogito.vstack(), build_root)
    win.add(root)
    app.run(win)
}
