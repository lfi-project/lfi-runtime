tests = [
  'test_init.c',
  'test_box.c',
  'test_ctx.c',
  'test_trampoline.c',
  'test_trampoline_clone.c',
]

foreach t : tests
  test(t, executable(
    t + '.elf',
    t,
    dependencies: [lfi_core],
  ), suite: ['core'])
endforeach
