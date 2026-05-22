srcs = files(
  'list.c',
  'linux.c',
  'fd.c',
  'proc.c',
  'buf.c',
  'elfload.c',
  'elfsym.c',
  'thread.c',
  'host.c',
  'perf.c',
  'path.c',
  'trampoline.c',
  'debugger.c',
  'cwalk.c',
)

if get_option('sys_minimal')
  srcs += files('sys_minimal.c')
else
  srcs += files('sys.c')

  # Pick a futex backend. The Linux backend uses the futex(2) syscall
  # directly; the macOS backend uses os_sync_wait_on_address (requires macOS
  # 14.4+); otherwise we fall back to a portable pthread-based pool.
  if host_machine.system() == 'darwin' and compiler.has_header('os/os_sync_wait_on_address.h')
    srcs += files('futex_macos.c')
  elif host_machine.system() == 'linux'
    srcs += files('futex_linux.c')
  else
    srcs += files('futex_pthread.c')
  endif
endif

subdir('arch/' + archmap[cpu])
subdir('sys')

config_h = configure_file(
  output: 'config.h',
  configuration: {
    'CONFIG_MAX_FUTEXES': get_option('max_futexes'),
    'CONFIG_ENABLE_DYLD': get_option('enable_dyld'),
  }
)

libelf_dep = []
if not have_elf_h
  libelf_dep = [subproject('libelf').get_variable('libelf_dep').as_link_whole()]
endif

liblfi_linux = static_library(
  'lfi-linux',
  srcs + [config_h],
  c_args: c_args_warnings + defines,
  dependencies: [lfi_core.as_link_whole()] + libelf_dep,
  install: true,
  include_directories: include_directories('include', 'arch/' + archmap[cpu]),
)

lfi_linux = declare_dependency(link_with: liblfi_linux, include_directories: include_directories('include', '../core/include'))

install_headers('include/lfi_linux.h')

subdir('test')
