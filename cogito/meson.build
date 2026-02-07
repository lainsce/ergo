project('cogito', 'c', version : '0.1.0')

cogito_inc = include_directories('include', 'src')

cogito_deps = [
  dependency('raylib', required : true),
  dependency('freetype2', required : true),
]

cogito_src = files(
  'src/cogito.c',
  'src/ergo_compat.c',
)

# List of Ergo source files in the order they should be concatenated
ergo_source_files = [
  'ergo/_intrinsics.ergo',
  'ergo/app.ergo',
  'ergo/window.ergo',
  'ergo/appbar.ergo',
  'ergo/image.ergo',
  'ergo/dialog.ergo',
  'ergo/dialog_slot.ergo',
  'ergo/vstack.ergo',
  'ergo/hstack.ergo',
  'ergo/zstack.ergo',
  'ergo/fixed.ergo',
  'ergo/scroller.ergo',
  'ergo/list.ergo',
  'ergo/grid.ergo',
  'ergo/label.ergo',
  'ergo/button.ergo',
  'ergo/checkbox.ergo',
  'ergo/switch.ergo',
  'ergo/searchfield.ergo',
  'ergo/textfield.ergo',
  'ergo/textview.ergo',
  'ergo/datepicker.ergo',
  'ergo/stepper.ergo',
  'ergo/dropdown.ergo',
  'ergo/slider.ergo',
  'ergo/tabs.ergo',
  'ergo/segmented.ergo',
  'ergo/view_switcher.ergo',
  'ergo/progress.ergo',
  'ergo/treeview.ergo',
  'ergo/colorpicker.ergo',
  'ergo/toasts.ergo',
  'ergo/toast.ergo',
  'ergo/bottom_toolbar.ergo',
  'ergo/chip.ergo',
  'ergo/fab.ergo',
  'ergo/nav_rail.ergo',
  'ergo/bottom_nav.ergo',
  'ergo/state.ergo',
]

# Custom target to concatenate all Ergo files into cogito.ergo
cogito_ergo = custom_target(
  'cogito.ergo',
  input : files(ergo_source_files),
  output : 'cogito.ergo',
  command : ['sh', '-c', 'echo "bring stdr\n" && cat "$@"', '--', '@INPUT@'],
  capture : true,
  build_by_default : true,
)

libcogito = shared_library(
  'cogito',
  cogito_src,
  include_directories : cogito_inc,
  dependencies : cogito_deps,
  link_args : ['-framework', 'Cocoa', '-framework', 'CoreGraphics', '-framework', 'Foundation', '-framework', 'AppKit'],
  install : false,
)

libcogito_dep = declare_dependency(
  include_directories : cogito_inc,
  link_with : libcogito,
)
