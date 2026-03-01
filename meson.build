project('yis', 'c',
  version : '0.1.0',
  default_options : ['warning_level=2', 'c_std=c11']
)

add_project_arguments('-O3', '-march=native', language : 'c')

yis_c_sources = files(
  'src/bootstrap/main.c',
  'src/bootstrap/lexer.c',
  'src/bootstrap/parser.c',
  'src/bootstrap/project.c',
  'src/bootstrap/sum_validate.c',
  'src/bootstrap/codegen.c',
  'src/bootstrap/typecheck.c',
  'src/bootstrap/file.c',
  'src/bootstrap/platform.c',
  'src/bootstrap/arena.c',
  'src/bootstrap/str.c',
  'src/bootstrap/diag.c',
)

yis_c_includes = include_directories('src', 'src/bootstrap')

yis_bootstrap = executable(
  'yis-bootstrap',
  yis_c_sources,
  include_directories : yis_c_includes,
  install : false
)

yis_target = custom_target(
  'yis',
  input : files('src/init.yi', 'src/lexer.yi', 'src/parser.yi'),
  output : 'yis',
  command : [
    'sh',
    '-c',
    'ROOT="$3" && rm -f "$ROOT/init" "$ROOT/init.c" "$2" && cd "$ROOT" && YIS_STDLIB="$ROOT/src/stdlib" "$1" run "$ROOT/src/init.yi" -- "$ROOT/src/init.yi" && mv "$ROOT/init" "$2" && rm -f "$ROOT/init" "$ROOT/init.c"',
    'sh',
    yis_bootstrap,
    meson.current_build_dir() / 'yis',
    meson.project_source_root()
  ],
  build_by_default : true,
  depends : yis_bootstrap,
  install : true,
  install_dir : get_option('bindir')
)

install_data(
  'src/runtime.inc',
  'src/stdlib/stdr.yi',
  'src/stdlib/math.yi',
  install_dir : get_option('datadir') / 'yis'
)

install_data(
  'src/stdlib/stdr.yi',
  'src/stdlib/math.yi',
  install_dir : get_option('datadir') / 'yis' / 'stdlib'
)
