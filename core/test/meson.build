tests = [
  'test_init.c',
  'test_box.c',
  'test_ctx.c',
  'test_pku.c',
]

# Add architecture-specific tests
if cpu != 'riscv64'
  tests += ['test_trampoline.c', 'test_trampoline_clone.c']
endif

foreach t : tests
  test(t, executable(
    t + '.elf',
    t,
    c_args: defines,
    dependencies: [lfi_core],
  ), suite: ['core'])
endforeach
