libcallback = shared_library('callback', 'lib_callback.c',
  c_args: example_lib_args, install: false)

executable('callback_passthrough',
  'main.cpp',
  dependencies: [sbox_passthrough_dep],
  install: false,
)

if have_lficc
  lib_callback_lfi = custom_target('lib_callback.lfi',
    input: 'lib_callback.c',
    output: 'lib_callback.lfi',
    command: [lficc, '-static-pie', '-o', '@OUTPUT@', '@INPUT@',
              '-lboxrt', '-Wl,--export-dynamic', '-O2'],
  )
  executable('callback_lfi',
    'main.cpp',
    cpp_args: ['-DSBOX_LFI'],
    dependencies: [sbox_lfi_dep],
    install: false,
  )
endif
