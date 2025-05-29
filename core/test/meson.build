tests = [
  'init.c',
]

foreach t : tests
  test(t, executable(
    t + '.elf',
    t,
    dependencies: [lfi_core],
  ), suite: ['core'])
endforeach
