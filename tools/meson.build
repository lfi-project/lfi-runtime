argp = subproject('argp').get_variable('argp_dep')

executable(
  'lfi-run',
  files('lfi_run.c'),
  c_args: c_args_hardened + c_args_warnings,
  dependencies: [argp, lfi_linux],
)
