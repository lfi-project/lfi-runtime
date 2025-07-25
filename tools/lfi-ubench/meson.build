if not lficc.found()
  warning('lficc not found: cannot create lfi-ubench')
  subdir_done()
endif

libbench = custom_target('libbench.lfi',
  output: 'libbench.lfi',
  input: files('lib.c'),
  command: [lficc, '-static-pie', '@INPUT@', '-o', '@OUTPUT@', '-lboxrt', '-Wl,--export-dynamic', '-O2', '-g'])

executable('lfi-ubench',
  files('main.c'), libbench,
  c_args: c_args_warnings,
  dependencies: [lfi_linux],
  install: false)
