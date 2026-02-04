bring stdr
bring cogito

def dialog_slot = cogito.dialog_slot()

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

fun on_close_dialog(btn = cogito.Button) (( -- )) {
    dialog_slot.clear()
}

fun on_appbar_settings(btn = cogito.Button) (( -- )) {
    let dlg = cogito.dialog(@"Preferences")
    dlg.add(cogito.label(@"Preferences"))
    let close_btn = cogito.button(@"Done")
    close_btn.on_click(on_close_dialog)
    dlg.add(close_btn)
    dialog_slot.show(dlg)
}

fun on_appbar_help(btn = cogito.Button) (( -- )) {
    let dlg = cogito.dialog(@"About")
    dlg.add(cogito.label(@"Cogito UI Gallery"))
    let close_btn = cogito.button(@"Close")
    close_btn.on_click(on_close_dialog)
    dlg.add(close_btn)
    dialog_slot.show(dlg)
}

fun build_ui(win = cogito.Window) (( -- )) {
    let bar = cogito.appbar(@"The Kitchensink", @"Cogito UI Gallery")
    let settings_btn = bar.add_button(@"sf:gearshape", on_appbar_settings)
    settings_btn.add_menu(@"Preferences", on_appbar_settings)
    settings_btn.add_menu(@"About", on_appbar_help)
    let help_btn = bar.add_button(@"sf:questionmark.circle", on_appbar_help)
    win.add(bar)

    let root = cogito.zstack()
    let content = cogito.vstack()
    root.add(content)
    root.add(dialog_slot)
    let row1 = cogito.hstack()
    let row2 = cogito.hstack()
    let row3 = cogito.hstack()
    let row4 = cogito.hstack()
    let row5 = cogito.hstack()

    let label = cogito.label(@"Label")
    let btn = cogito.button(@"Button")
    btn.on_click((b = cogito.Button) => { b.set_text(@"Clicked!") })
    row1.add(label)
    row1.add(btn)

    let cb1 = cogito.checkbox(@"Checking In", null)
    cb1.on_change(on_check)
    row2.add(cb1)

    let r1 = cogito.checkbox(@"Choice A", @"group1")
    let r2 = cogito.checkbox(@"Choice B", @"group1")
    r1.on_change(on_check)
    r2.on_change(on_check)
    row2.add(r1)
    row2.add(r2)

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

    let zs = cogito.zstack()
    zs.halign(1)
    zs.valign(1)
    let zbase = cogito.button(@"ZStack Base")
    let ztop = cogito.label(@"Overlay")
    ztop.set_class(@"subtitle")
    zs.add(zbase)
    zs.add(ztop)
    row5.add(zs)

    content.add(row1)
    content.add(row2)
    content.add(row3)
    content.add(row4)
    content.add(row5)
    win.add(root)
}

entry () (( -- )) {
    let app = cogito.app()
    let win = cogito.window_title(@"The Kitchensink").build(build_ui)
    win.set_autosize(true)
    app.run(win)
}
