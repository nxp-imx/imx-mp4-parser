
src_list_cmd = run_command('sh', '-c', 'ls lib/src/*.c', check : true)
srcs = files(src_list_cmd.stdout().strip().split())

inc_priv = include_directories('lib/inc')
inc_pub  = include_directories('../../parser/ghdr/common')
inc_pub_api  = include_directories('../../parser/ghdr')

lib_mp4 = shared_library(
  'mp4_parser_arm_elinux',
  srcs,
  include_directories : [inc_pub, inc_pub_api, inc_priv],
  name_prefix         : 'lib_',
  soversion           : '3.1',
  dependencies        : [h264_parser_dep, id3_parser_dep],
  c_args              : ['-DCORE_PARSER_PARSE_H264_DEC_SPECIFIC_INFO', '-D_POSIX_C_SOURCE=200809L'],
  install             : true,
  install_dir         : [libs_install_dir],
)