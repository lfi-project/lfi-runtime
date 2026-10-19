# Differential syscall tests: each case runs natively and in the sandbox and
# the results are compared. Only meaningful on a Linux host.

cases_lfi = custom_target('syscall_cases.lfi',
  output: 'syscall_cases.lfi',
  input: ['cases.c'] + lib_extra_srcs,
  command: [lficc, '-O2', '-o', '@OUTPUT@', '@INPUT@', '-static-pie',
            '-lboxrt', '-Wl,--export-dynamic'],
)

exit_prog_lfi = custom_target('syscall_exit_prog.lfi',
  output: 'syscall_exit_prog.lfi',
  input: 'exit_prog.c',
  command: [lficc, '-O2', '-o', '@OUTPUT@', '@INPUT@', '-static-pie'],
)

test_syscalls = executable(
  'test_syscalls.elf',
  ['test_syscalls.c', 'cases.c', 'exit_prog.c'],
  dependencies: [lfi_linux],
  include_directories: include_directories('..'),
  c_args: c_args_warnings + linux_defines + ['-DHOST_TEST'],
)

test('syscalls',
  test_syscalls, args: [cases_lfi, exit_prog_lfi],
  protocol: 'tap',
  suite: ['linux'],
)
