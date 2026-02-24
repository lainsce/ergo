project('ergo', 'c',
  version : '0.1.0',
  default_options : ['warning_level=2', 'c_std=c11']
)

ergo_c_sources = files(
  'src/main.c',
  'src/lexer.c',
  'src/parser.c',
  'src/project.c',
  'src/sum_validate.c',
  'src/codegen.c',
  'src/typecheck.c',
  'src/file.c',
  'src/platform.c',
  'src/arena.c',
  'src/str.c',
  'src/diag.c',
)

ergo_c_includes = include_directories('src')

ergo_exe = executable(
  'ergo',
  ergo_c_sources,
  include_directories : ergo_c_includes,
  install : true
)

# Install stdlib files
install_data(
  'src/stdlib/stdr.ergo',
  'src/stdlib/math.ergo',
  install_dir : get_option('datadir') / 'ergo' / 'stdlib'
)
