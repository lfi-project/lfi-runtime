tests = [
  'test_init.c',
  'test_box.c',
  'test_ctx.c',
  'test_trampoline.c',
]

foreach t : tests
  test(t, executable(
    t + '.elf',
    t,
    dependencies: [lfi_core],
  ), suite: ['core'])
endforeach
