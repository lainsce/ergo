bring cogito

def toast_layer = cogito.toasts()

fun on_toast(btn = cogito.Button) (( -- )) {
    let t = cogito.toast(@"Hello from Cogito")
    cogito.set_tooltip(t, @"Click x to dismiss")
    toast_layer.add(t)
}

fun on_datepicker(dp = cogito.DatePicker) (( -- )) {
    writef(@"Date picker changed\n")
}

fun on_colorpicker(cp = cogito.ColorPicker) (( -- )) {
    writef(@"Color picker changed\n")
}

fun build_ui(win = cogito.Window) (( -- )) {
    let bar = cogito.appbar(@"Hello", @"Cogito")
    let info_btn = bar.add_button(@"sf:info.circle", on_toast)
    cogito.set_tooltip(info_btn, @"Show toast")
    win.add(bar)

    let root = cogito.zstack()
    let content = cogito.vstack()
    root.add(content)
    root.add(toast_layer)

    let title = cogito.label(@"Hello, Cogito")
    title.set_class(@"title")
    let subtitle = cogito.label(@"A tiny GUI demo")
    subtitle.set_class(@"subtitle")
    let body = cogito.label(@"Dropdown, tooltip, toast, and tabular label.")
    body.set_class(@"body")
    let tab = cogito.label(@"Tabular 0011223344")
    tab.set_class(@"tabular")

    let dd = cogito.dropdown()
    dd.set_items([@"Option A", @"Option B", @"Option C"])
    cogito.set_tooltip(dd, @"Choose an option")

    let btn = cogito.button(@"Show Toast")
    btn.on_click(on_toast)
    cogito.set_tooltip(btn, @"Adds a toast")

    let dp = cogito.datepicker()
    dp.on_change(on_datepicker)
    dp.set_a11y_label(@"Choose date")
    dp.set_a11y_role(@"datepicker")
    cogito.set_tooltip(dp, @"Pick a date")

    let cp = cogito.colorpicker()
    cp.on_change(on_colorpicker)
    cp.set_a11y_label(@"Choose color")
    cp.set_a11y_role(@"colorpicker")
    cogito.set_tooltip(cp, @"Pick a color")

    content.add(title)
    content.add(subtitle)
    content.add(body)
    content.add(tab)
    content.add(dd)
    content.add(btn)
    content.add(dp)
    content.add(cp)

    win.add(root)
}

entry () (( -- )) {
    let app = cogito.app()
    let win = cogito.window_title(@"Hello").build(build_ui)
    win.set_a11y_label(@"Hello Window")
    win.set_resizable(false)
    app.run(win)
}
