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

ln = find_program('ln', required: true)
parser_link = meson.project_source_root() / 'parser'

fs = import('fs')
if not fs.exists(parser_link)
  run_command(ln, '-s', '../imx-parser', parser_link, check: true)
endif

parser_dir = 'parser'

libs_install_dir = join_paths(get_option('libdir'), 'imx-mm/parser')

subdir(parser_dir / 'src/parser_libs/id3_parser')
subdir(parser_dir / 'src/parser_libs/h264_parser')

subdir('src/mp4_parser')
