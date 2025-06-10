argp = subproject('argp').get_variable('argp_dep')

executable(
  'lfi-run',
  files('main.c'),
  c_args: c_args + c_args_warnings,
  dependencies: [argp, lfi_linux],
)
