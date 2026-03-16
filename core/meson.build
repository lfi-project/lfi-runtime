lfiv_opts = ['arch=' + archmap[cpu]]
if get_option('ctxreg')
  lfiv_opts += 'ctxreg_tp_offset=' + get_option('ctxreg_tp_offset').to_string()
endif
lfiv = subproject('lfi-verifier', default_options: lfiv_opts).get_variable('lfiv').as_link_whole()

srcs = files(
  'core.c',
  'error.c',
  'log.c',
  'box.c',
  'ctx.c',
  'trampoline.c',
  'arch_asm.c',
  'mmap.c',
  'boxmap.c',
  'extalloc.c',
)

if get_option('stores_only')
  defines += ['-DSTORES_ONLY']
endif

subdir('arch/' + archmap[cpu])

liblfi_core = static_library(
  'lfi-core',
  srcs,
  c_args: c_args_warnings + defines,
  dependencies: [lfiv],
  install: true,
  include_directories: include_directories('include', 'arch/' + archmap[cpu]),
)

lfi_core = declare_dependency(link_with: liblfi_core, include_directories: include_directories('include'))

install_headers('include/lfi_core.h')
install_headers('include/lfi_arch.h')

subdir('test')
