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

cpu = target_machine.cpu_family()
archmap = {
  'aarch64': 'arm64',
  'x86_64': 'x64',
}
if not archmap.has_key(cpu)
  warning('unsupported architecture: ' + cpu)
  subdir_done()
endif

defines = []
if compiler.has_function('getdents64', prefix : '''#define _GNU_SOURCE
#include <dirent.h>''')
  defines += '-DHAVE_GETDENTS64'
endif
if compiler.compiles('''#include <sys/syscall.h>
#ifndef SYS_getdents64
#error "no SYS_getdents64"
#endif
''')
  defines += '-DHAVE_SYS_GETDENTS64'
endif
if compiler.has_function('memfd_create', prefix : '''#define _GNU_SOURCE
#include <sys/mman.h>''')
  defines += '-DHAVE_MEMFD_CREATE'
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

subdir('core')
subdir('linux')
subdir('tools')
