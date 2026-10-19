srcs = files(
  'linux.c',
  'proc.c',
  'elfload.c',
  'elfsym.c',
  'thread.c',
  'host.c',
  'perf.c',
  'trampoline.c',
  'debugger.c',
  'sys.c',
)

# Extra defines for liblfi-linux. The full configuration appends to this.
linux_defines = []

subdir('arch/' + archmap[cpu])
subdir('sys')

if get_option('linux_full')
  subdir('full')
endif

libelf_dep = []
if not have_elf_h
  libelf_dep = [subproject('libelf').get_variable('libelf_dep').as_link_whole()]
endif

liblfi_linux = static_library(
  'lfi-linux',
  srcs,
  c_args: c_args_warnings + defines + linux_defines,
  dependencies: [lfi_core.as_link_whole()] + libelf_dep,
  install: true,
  include_directories: include_directories('include', 'arch/' + archmap[cpu]),
)

lfi_linux = declare_dependency(link_with: liblfi_linux, include_directories: include_directories('include', '../core/include'))

install_headers('include/lfi_linux.h')

subdir('test')
