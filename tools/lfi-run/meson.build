argtable = subproject('argtable3').get_variable('argtable_dep')

executable(
  'lfi-run',
  files('main.c'),
  c_args: c_args_warnings,
  dependencies: [argtable, lfi_linux],
  link_args: ['-static'],
  install: true,
)
