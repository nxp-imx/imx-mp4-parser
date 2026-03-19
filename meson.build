project(
  'imx-mp4-parser',
  'c',
  version : '4.11.0',
  meson_version : '>=0.60.0',
  default_options : [
    'c_std=c11',
    'warning_level=2',
    'buildtype=release',
    'werror=true',
  ]
)

libs_install_dir = join_paths(get_option('libdir'), 'imx-mm/parser')

subdir('src/mp4_parser')
