bring stdr
bring cogito

fun on_button(btn = cogito.Button) (( -- )) {
    btn.set_text(@"Clicked!")
}

fun on_check(cb = cogito.Checkbox) (( -- )) {
    if cb.checked() {
        writef(@"Checkbox: on\n")
    } else {
        writef(@"Checkbox: off\n")
    }
}

fun on_switch(sw = cogito.Switch) (( -- )) {
    if sw.checked() {
        writef(@"Switch: on\n")
    } else {
        writef(@"Switch: off\n")
    }
}

fun on_list_select(idx = num) (( -- )) {
    writef(@"List select: {idx}\n")
}

fun on_list_activate(idx = num) (( -- )) {
    writef(@"List activate: {idx}\n")
}

fun on_grid_select(idx = num) (( -- )) {
    writef(@"Grid select: {idx}\n")
}

fun on_grid_activate(idx = num) (( -- )) {
    writef(@"Grid activate: {idx}\n")
}

fun build_ui(win = cogito.Window) (( -- )) {
    let root = cogito.vstack()
    root.align_begin ()
    let row1 = cogito.hstack()
    let row2 = cogito.hstack()
    let row3 = cogito.hstack()
    let row4 = cogito.hstack()

    let label = cogito.label(@"Label")
    let btn = cogito.button(@"Button")
    btn.on_click(on_button)
    row1.add(label)
    row1.add(btn)

    let group = cogito.checkbox(@"Group", null)
    let cb1 = cogito.checkbox(@"Choice A", group)
    let cb2 = cogito.checkbox(@"Choice B", group)
    cb1.on_change(on_check)
    cb2.on_change(on_check)
    row2.add(cb1)
    row2.add(cb2)

    let sw = cogito.switch(@"Switch")
    sw.on_change(on_switch)
    row3.add(sw)

    let list = cogito.list()
    list.on_select(on_list_select)
    list.on_activate(on_list_activate)
    let li1 = cogito.label(@"Item 1")
    let li2 = cogito.label(@"Item 2")
    let li3 = cogito.label(@"Item 3")
    list.add(li1)
    list.add(li2)
    list.add(li3)
    row4.add(list)

    let grid = cogito.grid(2)
    grid.on_select(on_grid_select)
    grid.on_activate(on_grid_activate)
    grid.add(cogito.label(@"A1"))
    grid.add(cogito.label(@"A2"))
    grid.add(cogito.label(@"B1"))
    grid.add(cogito.label(@"B2"))
    row4.add(grid)

    root.add(row1)
    root.add(row2)
    root.add(row3)
    root.add(row4)
    win.add(root)
}

entry () (( -- )) {
    let app = cogito.app()
    let win = cogito.window_title("The Kitchensink").build(build_ui)
    app.run(win)
}
