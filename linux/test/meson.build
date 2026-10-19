# Simple tests that directly use the lfi_linux API.
tests = [
  'test_init.c',
]

foreach t : tests
  test(t, executable(
    t + '.elf',
    t,
    dependencies: [lfi_linux],
  ), suite: ['linux'])
endforeach

if not lficc.found()
  warning('lficc not found: cannot test liblfi')
  subdir_done()
endif

# The minimal runtime provides no mmap, so sandboxed libraries use minialloc,
# a brk-based allocator, in place of the libc allocator. The full
# configuration has mmap and uses the libc allocator.
lib_extra_srcs = []
if not get_option('linux_full')
  minialloc = subproject('minialloc')
  lib_extra_srcs = minialloc.get_variable('minialloc_srcs')
endif
lib = custom_target('lib.lfi',
  output: 'lib.lfi',
  input: ['library/add.c'] + lib_extra_srcs,
  command: [lficc, '-O2', '-o', '@OUTPUT@', '@INPUT@', '-static-pie',
            '-lboxrt', '-Wl,--export-dynamic'],
)

test_detach = executable(
  'test_detach.elf',
  'test_detach.c',
  dependencies: [lfi_linux],
  c_args: c_args_warnings + linux_defines,
)

test('detach.lfi',
  test_detach, args: [lib],
  suite: ['linux'],
)

if host_machine.system() == 'linux'
  subdir('syscalls')
endif
