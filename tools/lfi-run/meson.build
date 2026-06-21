link_args = []
if get_option('static_lfi_run')
  link_args = ['-static']
endif

defines = []
if get_option('large_sandbox')
  defines += '-DLARGE_SANDBOX'
  defines += '-DLARGE_SANDBOX_BITS=@0@'.format(get_option('large_sandbox_bits'))
endif

executable(
  'lfi-run',
  files('main.c'),
  c_args: c_args_warnings + defines,
  dependencies: [lfi_linux],
  link_args: link_args,
  install: true,
)
