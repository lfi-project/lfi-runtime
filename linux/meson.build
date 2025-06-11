srcs = files(
  'linux.c',
  'fd.c',
  'file.c',
  'sys.c',
  'proc.c',
  'buf.c',
  'elfload.c',
  'thread.c',
  'host.c',
  'perf.c',
  'path.c',
)

subdir('arch/' + archmap[cpu])
subdir('sys')

config_h = configure_file(
  output: 'config.h',
  configuration: {
    'CONFIG_MAX_FUTEXES': get_option('max_futexes'),
    'CONFIG_ENABLE_THREADS': get_option('enable_threads'),
    'CONFIG_ENABLE_DYLD': get_option('enable_dyld'),
  }
)

cwalk = subproject('cwalk').get_variable('cwalk_dep')

liblfi_linux = static_library(
  'lfi-linux',
  srcs + [config_h],
  c_args: c_args + c_args_warnings + defines,
  dependencies: [lfi_core.as_link_whole(), cwalk.as_link_whole()],
  install: true,
  include_directories: include_directories('include', 'arch/' + archmap[cpu]),
)

lfi_linux = declare_dependency(link_with: liblfi_linux, include_directories: include_directories('include', '../core/include'))

install_headers('include/lfi_linux.h')

subdir('test')
