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
