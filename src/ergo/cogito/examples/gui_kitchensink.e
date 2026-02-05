bring stdr
bring cogito

def main_win = cogito.window_title(@"The Kitchensink")

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

fun on_textfield(tf = cogito.TextField) (( -- )) {
    writef(@"TextField: {tf.text()}\n")
}

fun on_textview(tv = cogito.TextView) (( -- )) {
    writef(@"TextView: {tv.text()}\n")
}

fun on_dropdown(dd = cogito.Dropdown) (( -- )) {
    writef(@"Dropdown: {dd.selected()}\n")
}

fun on_slider(sl = cogito.Slider) (( -- )) {
    writef(@"Slider: {sl.value()}\n")
}

fun on_tabs(t = cogito.Tabs) (( -- )) {
    writef(@"Tabs: {t.selected()}\n")
}

fun on_datepicker(dp = cogito.DatePicker) (( -- )) {
    writef(@"DatePicker changed\n")
}

fun on_colorpicker(cp = cogito.ColorPicker) (( -- )) {
    writef(@"ColorPicker changed\n")
}

fun on_close_dialog(btn = cogito.Button) (( -- )) {
    main_win.clear_dialog()
}

fun on_appbar_settings(btn = cogito.Button) (( -- )) {
    let dlg = cogito.dialog(@"Preferences")
    dlg.add(cogito.label(@"Preferences"))
    let close_btn = cogito.button(@"Done")
    close_btn.on_click(on_close_dialog)
    dlg.add(close_btn)
    main_win.set_dialog(dlg)
}

fun on_appbar_help(btn = cogito.Button) (( -- )) {
    let dlg = cogito.dialog(@"About")
    dlg.add(cogito.label(@"Cogito UI Gallery"))
    let close_btn = cogito.button(@"Close")
    close_btn.on_click(on_close_dialog)
    dlg.add(close_btn)
    main_win.set_dialog(dlg)
}

fun build_ui(win = cogito.Window) (( -- )) {
    let bar = cogito.appbar(@"The Kitchensink", @"Cogito UI Gallery")
    let settings_btn = bar.add_button(@"sf:gearshape", on_appbar_settings)
    settings_btn.add_menu(@"Preferences", on_appbar_settings)
    settings_btn.add_menu(@"About", on_appbar_help)
    cogito.set_tooltip(settings_btn, @"Preferences and About")
    let help_btn = bar.add_button(@"sf:questionmark.circle", on_appbar_help)
    cogito.set_tooltip(help_btn, @"Help and About")
    win.add(bar)

    let root = cogito.zstack()
    let content = cogito.vstack()
    root.add(content)
    let toast_layer = cogito.toasts()
    root.add(toast_layer)

    let row1 = cogito.hstack()
    let row2 = cogito.hstack()
    let row3 = cogito.hstack()
    let row4 = cogito.hstack()
    let row5 = cogito.hstack()
    let row6 = cogito.hstack()
    let row7 = cogito.hstack()
    let row8 = cogito.hstack()
    let row9 = cogito.hstack()
    let row10 = cogito.hstack()
    let row11 = cogito.hstack()
    let row12 = cogito.hstack()

    let label = cogito.label(@"Label")
    let btn = cogito.button(@"Button")
    btn.on_click((b = cogito.Button) => { b.set_text(@"Clicked!") })
    cogito.set_tooltip(btn, @"Click me")
    row1.add(label)
    row1.add(btn)
    let mono = cogito.label(@"Monospace 0123456789")
    mono.set_class(@"monospace")
    row1.add(mono)
    let tab = cogito.label(@"Tabular 0011223344")
    tab.set_class(@"tabular")
    row1.add(tab)

    let cb1 = cogito.checkbox(@"Checking In", null)
    cb1.on_change(on_check)
    cogito.set_tooltip(cb1, @"Toggle checkbox")
    row2.add(cb1)

    let r1 = cogito.checkbox(@"Choice A", @"group1")
    let r2 = cogito.checkbox(@"Choice B", @"group1")
    r1.on_change(on_check)
    r2.on_change(on_check)
    cogito.set_tooltip(r1, @"Radio A")
    cogito.set_tooltip(r2, @"Radio B")
    row2.add(r1)
    row2.add(r2)

    let sw = cogito.switch(@"Switch")
    sw.on_change(on_switch)
    cogito.set_tooltip(sw, @"Toggle switch")
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

    let tf = cogito.textfield(@"Text field")
    tf.on_change(on_textfield)
    cogito.set_tooltip(tf, @"Type here")
    row6.add(tf)
    let tv = cogito.textview(@"Text view with wrapping text that spans multiple lines.")
    tv.on_change(on_textview)
    cogito.set_tooltip(tv, @"Multi-line input")
    row6.add(tv)
    let sf = cogito.searchfield(@"Search…")
    cogito.set_tooltip(sf, @"Search field")
    row6.add(sf)

    let dd = cogito.dropdown()
    dd.set_items([@"One", @"Two", @"Three"])
    dd.on_change(on_dropdown)
    cogito.set_tooltip(dd, @"Choose an option")
    row7.add(dd)
    let sld = cogito.slider(0, 100, 40)
    sld.on_change(on_slider)
    cogito.set_tooltip(sld, @"Drag to change")
    row7.add(sld)

    let tabs = cogito.tabs()
    tabs.set_items([@"General", @"Advanced"])
    tabs.set_ids([@"general", @"advanced"])
    tabs.on_change(on_tabs)
    cogito.set_tooltip(tabs, @"Switch views")
    row8.add(tabs)

    let vs = cogito.view_switcher()
    let v1 = cogito.label(@"General view")
    let v2 = cogito.label(@"Advanced view")
    cogito.set_id(v1, @"general")
    cogito.set_id(v2, @"advanced")
    vs.add(v1)
    vs.add(v2)
    tabs.bind(vs)
    row8.add(vs)

    let prog = cogito.progress(0.35)
    cogito.set_tooltip(prog, @"Progress 35%")
    row9.add(prog)

    let dp = cogito.datepicker()
    dp.on_change(on_datepicker)
    dp.set_a11y_label(@"Choose date")
    dp.set_a11y_role(@"datepicker")
    cogito.set_tooltip(dp, @"Date picker")
    row10.add(dp)

    let cp = cogito.colorpicker()
    cp.on_change(on_colorpicker)
    cp.set_a11y_label(@"Choose color")
    cp.set_a11y_role(@"colorpicker")
    cogito.set_tooltip(cp, @"Color picker")
    row10.add(cp)

    let sc = cogito.scroller()
    sc.set_axes(true, true)
    let sc_content = cogito.vstack()
    sc_content.add(cogito.label(@"Scrollable item 1"))
    sc_content.add(cogito.label(@"Scrollable item 2"))
    sc_content.add(cogito.label(@"Scrollable item 3"))
    sc_content.add(cogito.label(@"Scrollable item 4"))
    sc_content.add(cogito.label(@"Scrollable item 5"))
    sc_content.add(cogito.label(@"Scrollable item 6"))
    sc_content.add(cogito.label(@"This line is intentionally very long to force horizontal scrolling."))
    sc.add(sc_content)
    row11.add(sc)

    let fixed = cogito.fixed()
    let fx_label = cogito.label(@"Fixed top-left")
    fixed.add(fx_label)
    fixed.set_pos(fx_label, 0, 0)
    let fx_btn = cogito.button(@"Fixed button")
    fixed.add(fx_btn)
    fixed.set_pos(fx_btn, 12, 42)
    let fx_tag = cogito.label(@"Fixed top-right")
    fixed.add(fx_tag)
    fixed.set_pos(fx_tag, 120, 0)
    row12.add(fixed)

    let text_demo = cogito.vstack()
    let wrap = cogito.label(@"This label wraps within a max-width constraint and shows alignment.")
    wrap.set_class(@"body")
    wrap.set_wrap(true)
    wrap.set_text_align(1)
    let ellipsis = cogito.label(@"This label demonstrates ellipsis truncation with a max width.")
    ellipsis.set_class(@"body")
    ellipsis.set_ellipsis(true)
    ellipsis.set_text_align(2)
    text_demo.add(wrap)
    text_demo.add(ellipsis)
    row12.add(text_demo)

    let t1 = cogito.toast(@"Saved")
    let t2 = cogito.toast(@"Export complete")
    cogito.set_tooltip(t1, @"Click x to dismiss")
    cogito.set_tooltip(t2, @"Click x to dismiss")
    toast_layer.add(t1)
    toast_layer.add(t2)

    content.add(row1)
    content.add(row2)
    content.add(row3)
    content.add(row4)
    content.add(row5)
    content.add(row6)
    content.add(row7)
    content.add(row8)
    content.add(row9)
    content.add(row10)
    content.add(row11)
    content.add(row12)

    let toolbar = cogito.bottom_toolbar()
    let tb_label = cogito.label(@"Bottom toolbar")
    tb_label.set_class(@"subtitle")
    toolbar.add(tb_label)
    content.add(toolbar)

    win.add(root)
}

entry () (( -- )) {
    let app = cogito.app()
    cogito.load_css(@"src/ergo/cogito/examples/cogito_examples.css")
    main_win.build(build_ui)
    app.set_appid(@"com.example.CogitoKitchensink")
    app.set_accent_color(@"#007AFF", false)
    main_win.set_a11y_label(@"Cogito Kitchensink")
    main_win.set_resizable (true)
    app.run(main_win)
}
