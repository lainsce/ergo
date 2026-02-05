project('cogito', 'c', version : '0.1.0')

cogito_inc = include_directories('include', 'src')

cogito_deps = [
  dependency('raylib', required : true),
]

cogito_src = files(
  'src/cogito.c',
  'src/ergo_compat.c',
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
