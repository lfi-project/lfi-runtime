if not lficc.found()
  warning('lficc not found: cannot create lfi-scale')
  subdir_done()
endif

libscale = custom_target('libscale.lfi',
  output: 'libscale.lfi',
  input: files('lib.c'),
  command: [lficc, '-static-pie', '@INPUT@', '-o', '@OUTPUT@', '-lboxrt', '-Wl,--export-dynamic', '-O2'])

link_args = []
if get_option('static_lfi_run')
  link_args = ['-static']
endif

defines = []
if get_option('large_sandbox')
  defines += '-DLARGE_SANDBOX'
  defines += '-DLARGE_SANDBOX_BITS=@0@'.format(get_option('large_sandbox_bits'))
endif

executable('lfi-scale',
  files('main.c'), libscale,
  c_args: c_args_warnings + defines,
  dependencies: [lfi_linux],
  link_args: link_args,
  install: false)
