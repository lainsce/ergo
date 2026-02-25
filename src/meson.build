project('yis', 'c',
  version : '0.1.0',
  default_options : ['warning_level=2', 'c_std=c11']
)

yis_c_sources = files(
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

yis_c_includes = include_directories('.')

yis_exe = executable(
  'yis',
  yis_c_sources,
  include_directories : yis_c_includes,
  install : true
)

# Install stdlib files
install_data(
  'stdlib/stdr.yi',
  'stdlib/math.yi',
  install_dir : get_option('datadir') / 'yis' / 'stdlib'
)