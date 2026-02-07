project('ergo', 'c',
  version : '0.1.0',
  default_options : ['warning_level=2', 'c_std=c11']
)

ergo_c_sources = files(
  'src/ergo/main.c',
  'src/ergo/lexer.c',
  'src/ergo/parser.c',
  'src/ergo/project.c',
  'src/ergo/codegen.c',
  'src/ergo/typecheck.c',
  'src/ergo/file.c',
  'src/ergo/platform.c',
  'src/ergo/arena.c',
  'src/ergo/str.c',
  'src/ergo/diag.c',
)

ergo_c_includes = include_directories('src/ergo')

ergo_exe = executable(
  'ergo',
  ergo_c_sources,
  include_directories : ergo_c_includes,
  install : true
)

# Install stdlib files
install_data(
  'src/ergo/stdlib/stdr.ergo',
  'src/ergo/stdlib/math.ergo',
  'src/ergo/stdlib/cogito.ergo',
  install_dir : get_option('datadir') / 'ergo' / 'stdlib'
)
