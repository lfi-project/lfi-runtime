project(
  'lfi-runtime',
  ['c'],
  default_options: ['warning_level=1', 'buildtype=release']
)

c_args = [
  '-fno-strict-aliasing',
  '-fno-delete-null-pointer-checks',
  '-fno-strict-overflow',
  '-Werror=implicit',
  '-Werror=incompatible-pointer-types',
  '-Werror=int-conversion',
  '-fvisibility=hidden',
]
compiler = meson.get_compiler('c')
has_auto_var_init = compiler.has_argument('-ftrivial-auto-var-init=zero')
if has_auto_var_init
  c_args += '-ftrivial-auto-var-init=zero'
endif
c_args_warnings = [
  '-Wno-unused-parameter',
  '-Wno-gnu-zero-variadic-macro-arguments',
]

if get_option('enable_analyzer')
  if compiler.has_argument('-fanalyzer')
    c_args_warnings += '-fanalyzer'
  else
    error('compiler does not support -fanalyzer')
  endif
endif

cpu = target_machine.cpu_family()
archmap = {
  'aarch64': 'arm64',
  'x86_64': 'x64',
  'riscv64': 'riscv64'
}
if not archmap.has_key(cpu)
  warning('unsupported architecture: ' + cpu)
  subdir_done()
endif

if cpu == 'riscv64'
  add_project_arguments('-march=rv64gc_zba', language: 'c')
  add_project_arguments('-march=rv64gc_zba', language: 'cpp')
endif

defines = []
if compiler.has_function('getdents64', prefix : '''#define _GNU_SOURCE
#include <dirent.h>''')
  defines += '-DHAVE_GETDENTS64'
endif
if compiler.has_function('getauxval', prefix : '''#include <sys/auxv.h>''')
  defines += '-DHAVE_GETAUXVAL'
endif
if compiler.has_function('sysinfo', prefix : '''#include <sys/sysinfo.h>''')
  defines += '-DHAVE_SYSINFO'
endif
if compiler.compiles('''#include <sys/syscall.h>
#ifndef SYS_getdents64
#error "no SYS_getdents64"
#endif
''')
  defines += '-DHAVE_SYS_GETDENTS64'
endif
if compiler.compiles('''#include <sys/syscall.h>
#ifndef SYS_futex
#error "no SYS_futex"
#endif
''')
  defines += '-DHAVE_SYS_FUTEX'
endif
if compiler.has_function('memfd_create', prefix : '''#define _GNU_SOURCE
#include <sys/mman.h>''')
  defines += '-DHAVE_MEMFD_CREATE'
endif
if get_option('enable_pku') and compiler.has_function('pkey_alloc', prefix : '''#define _GNU_SOURCE
#include <sys/mman.h>''')
  defines += '-DHAVE_PKU'
endif
if compiler.compiles('''#include <sys/syscall.h>
#ifndef SYS_memfd_create
#error "no SYS_memfd_create"
#endif
''')
  defines += '-DHAVE_SYS_MEMFD_CREATE'
endif
if compiler.has_function('getentropy', prefix : '#include <unistd.h>')
  defines += '-DHAVE_GETENTROPY'
endif
if compiler.has_function('getrandom', prefix : '#include <sys/random.h>')
  defines += '-DHAVE_GETRANDOM'
endif
if compiler.has_function('arc4random_buf', prefix : '#include <stdlib.h>')
  defines += '-DHAVE_ARC4RANDOM_BUF'
endif
r_debug_code = '''
extern char _r_debug;
int main() {
  return (int) &_r_debug;
}
'''
if compiler.links(r_debug_code, name: 'has _r_debug')
  defines += '-DHAVE_R_DEBUG'
endif
have_elf_h = compiler.has_header('elf.h')
if not have_elf_h
  defines += '-DUSE_GELF_H'
endif

if get_option('enable_tlsdesc') and cpu == 'x86_64'
  defines += '-DENABLE_TLS_DESC'
  c_args += '-mtls-dialect=gnu2'
endif
if get_option('enable_tlsexec')
  defines += '-DENABLE_TLS_EXEC'
endif
if get_option('enable_segue')
  defines += '-DENABLE_SEGUE'
endif
if get_option('segue_single_sandbox')
  defines += '-DSEGUE_SINGLE_SANDBOX'
endif
if get_option('sys_minimal')
  defines += '-DSYS_MINIMAL'
endif
if get_option('ctxreg')
  defines += '-DCTXREG'
endif

if not meson.is_subproject()
  add_global_arguments(c_args, language: 'c')
endif

subdir('core')

if get_option('enable_linux')
  lficc = find_program(cpu + '_lfi-linux-musl-clang', required: false)

  subdir('linux')
  subdir('tools')

  liblfi_so = shared_library(
    'lfi',
    dependencies: lfi_linux.as_link_whole(),
    install: true,
  )
  liblfi_a = static_library(
    'lfi',
    dependencies: lfi_linux.as_link_whole(),
    install: true,
  )

  pkg = import('pkgconfig')
  pkg.generate(
      name: 'lfi',
      filebase: 'lfi',
      description: 'LFI runtime',
      libraries: [liblfi_so, liblfi_a],
  )
endif
