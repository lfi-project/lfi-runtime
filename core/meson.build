lfiv = subproject('lfiv', default_options: ['arch=' + archmap[cpu]]).get_variable('lfiv').as_link_whole()
mmap = subproject('libmmap').get_variable('mmap').as_link_whole()
boxmap = subproject('libboxmap').get_variable('boxmap').as_link_whole()

srcs = files(
  'core.c',
  'error.c',
  'log.c',
  'box.c',
  'ctx.c',
  'trampoline.c',
  'arch_asm.c',
)

if get_option('stores_only_trampoline')
  defines += ['-DSTORES_ONLY_TRAMPOLINE']
endif

subdir('arch/' + archmap[cpu])

liblfi_core = static_library(
  'lfi-core',
  srcs,
  c_args: c_args_warnings + defines,
  dependencies: [lfiv, mmap, boxmap],
  install: true,
  include_directories: include_directories('include', 'arch/' + archmap[cpu]),
)

lfi_core = declare_dependency(link_with: liblfi_core, include_directories: include_directories('include'))

install_headers('include/lfi_core.h')
install_headers('include/lfi_arch.h')

subdir('test')
