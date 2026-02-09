project('cogito', 'c', version : '0.1.0')

cogito_inc = include_directories('src')

cogito_deps = [
  dependency('raylib', required : true),
  dependency('freetype2', required : true),
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
  install : true,
)

# Install header files
install_headers('src/cogito.h', subdir : 'cogito')

libcogito_dep = declare_dependency(
  include_directories : cogito_inc,
  link_with : libcogito,
)
