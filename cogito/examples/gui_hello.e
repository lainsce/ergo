bring stdr
bring cogito

def label_state = cogito.state(@"Hello")

fun build_ui(win = cogito.Window) (( -- )) {
    let root = cogito.zstack()
    root.halign(1)
    root.valign(1)

    let content = cogito.vstack()
    let title = cogito.label(label_state.get())
    title.set_class(@"title")
    let btn = cogito.button(@"Change Title")
    btn.on_click((b = cogito.Button) => {
        label_state.set(@"Title Updated")
    })
    content.add(title)
    content.add(btn)
    root.add(content)
    win.add(root)
}

entry () (( -- )) {
    let app = cogito.app()
    let win = cogito.window_title(@"Hello").build(build_ui)
    app.run(win)
}
