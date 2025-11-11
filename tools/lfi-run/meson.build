link_args = []
if get_option('static_lfi_run')
  link_args = ['-static']
endif

executable(
  'lfi-run',
  files('main.c', 'argtable3.c'),
  c_args: c_args_warnings,
  dependencies: [lfi_linux],
  link_args: link_args,
  install: true,
)
