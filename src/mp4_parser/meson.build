
src_list_cmd = run_command('sh', '-c', 'ls lib/src/*.c', check : true)
srcs = files(src_list_cmd.stdout().strip().split())

inc_priv = include_directories('lib/inc')

sysroot = meson.get_external_property('sys_root', '')

add_project_arguments(
  '-I' + sysroot + '/usr/include/imx-mm/parser',
  language: 'c'
)

imx_parser_dep = dependency('imx-parser', required: true)

lib_mp4 = shared_library(
  'mp4_parser_arm_elinux',
  srcs,
  include_directories : inc_priv,
  name_prefix         : 'lib_',
  soversion           : '3.1',
  dependencies        : imx_parser_dep,
  c_args              : ['-DCORE_PARSER_PARSE_H264_DEC_SPECIFIC_INFO',
                         '-D_POSIX_C_SOURCE=200809L',
                          '-I' + sysroot + '/usr/include/imx-mm/parser'],
  install             : true,
  install_dir         : libs_install_dir,
)