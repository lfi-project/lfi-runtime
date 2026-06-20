if not lficc.found()
  warning('lficc not found: cannot create lfi-ubench')
  subdir_done()
endif

libbench = custom_target('libbench.lfi',
  output: 'libbench.lfi',
  input: files('lib.c'),
  command: [lficc, '-static-pie', '@INPUT@', '-o', '@OUTPUT@', '-lboxrt', '-Wl,--export-dynamic', '-O2'])

ubench_c_args = c_args_warnings
if get_option('enable_ubench_baselines')
  # Build the extra non-LFI host baseline benchmarks (native_call, host_syscall,
  # host_ctxswitch) for comparison against the sandboxed ones.
  ubench_c_args += '-DUBENCH_BASELINES'
endif

executable('lfi-ubench',
  files('main.c'), libbench,
  c_args: ubench_c_args,
  dependencies: [lfi_linux],
  install: false)
