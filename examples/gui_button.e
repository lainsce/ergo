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
    let app = cogito.app()
    let win = cogito.window()
    let root = cogito.build(cogito.vstack(), build_root)
    win.add(root)
    app.run(win)
}
