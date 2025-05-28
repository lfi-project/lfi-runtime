lfiv = subproject('lfiv').get_variable('lfiv').as_link_whole()
mmap = subproject('libmmap').get_variable('mmap').as_link_whole()
boxmap = subproject('libboxmap').get_variable('boxmap').as_link_whole()

srcs = files(
  'engine.c',
)

liblfi_core = static_library(
  'lfi-core',
  srcs,
  c_args: c_args_hardened + c_args_warnings,
  dependencies: [lfiv, mmap, boxmap],
  install: true,
  include_directories: include_directories('include'),
)

lfi_core = declare_dependency(link_with: liblfi_core, include_directories: include_directories('include'))

install_headers('include/lfi_core.h')
