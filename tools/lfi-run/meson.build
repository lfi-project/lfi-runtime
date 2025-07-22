argtable = subproject('argtable3').get_variable('argtable_dep')

link_args = []
if get_option('static_lfi_run')
  link_args = ['-static']
endif

executable(
  'lfi-run',
  files('main.c'),
  c_args: c_args_warnings,
  dependencies: [argtable, lfi_linux],
  link_args: link_args,
  install: true,
)
