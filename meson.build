project(
  'imx-mp4-parser',
  'c',
  version : '5.0.0',
  meson_version : '>=0.60.0',
  default_options : [
    'c_std=c11',
    'warning_level=2',
    'buildtype=release',
    'werror=true',
  ]
)

#inc_dir = include_directories(join_paths(get_option('includedir'), 'imx-mm/parser'))

libs_install_dir = join_paths(get_option('libdir'), 'imx-mm/parser')

subdir('src/mp4_parser')
