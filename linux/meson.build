srcs = files(
  'linux.c',
)

liblfi_linux = static_library(
  'lfi-linux',
  srcs,
  c_args: c_args_hardened + c_args_warnings,
  dependencies: [lfi_core.as_link_whole()],
  install: true,
  include_directories: include_directories('include'),
)

lfi_linux = declare_dependency(link_with: liblfi_linux, include_directories: include_directories('include'))

install_headers('include/lfi_linux.h')
