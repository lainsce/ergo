project('ergo', 'c',
  version : '0.1.0',
  default_options : ['warning_level=2', 'c_std=c11']
)

ergo_c_sources = files(
  'main.c',
  'lexer.c',
  'parser.c',
  'project.c',
  'sum_validate.c',
  'codegen.c',
  'typecheck.c',
  'file.c',
  'platform.c',
  'arena.c',
  'str.c',
  'diag.c',
)

ergo_c_includes = include_directories('.')

ergo_exe = executable(
  'ergo',
  ergo_c_sources,
  include_directories : ergo_c_includes,
  install : true
)

# Install stdlib files
install_data(
  'stdlib/stdr.ergo',
  'stdlib/math.ergo',
  install_dir : get_option('datadir') / 'ergo' / 'stdlib'
)