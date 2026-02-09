project('cogito', 'c', version : '0.1.0')

cogito_inc = include_directories('src', 'c/backend')

# SDL3 dependency (pkg-config name first, fallback to CMake name)
sdl3_dep = dependency('sdl3', required : false)
if not sdl3_dep.found()
  sdl3_dep = dependency('SDL3', required : true)
endif

# SDL3_ttf for text rendering
sdl3_ttf_dep = dependency('sdl3-ttf', required : false)
if not sdl3_ttf_dep.found()
  sdl3_ttf_dep = dependency('SDL3_ttf', required : false)
endif

cogito_deps = [
  sdl3_dep,
  sdl3_ttf_dep,
  dependency('freetype2', required : true),
]

cogito_src = files(
  'src/cogito.c',
  'src/ergo_compat.c',
  'c/backend/backend_sdl3.c',
  'c/backend/csd_sdl3.c',
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
