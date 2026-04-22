libinout = shared_library('inout', 'lib_inout.c',
  c_args: example_lib_args, install: false)

executable('inout_passthrough',
  'main.cpp',
  dependencies: [sbox_passthrough_dep],
  install: false,
)

if have_lficc
  lib_inout_lfi = custom_target('lib_inout.lfi',
    input: 'lib_inout.c',
    output: 'lib_inout.lfi',
    command: [lficc, '-static-pie', '-o', '@OUTPUT@', '@INPUT@',
              '-lboxrt', '-Wl,--export-dynamic', '-O2'],
  )
  executable('inout_lfi',
    'main.cpp',
    cpp_args: ['-DSBOX_LFI'],
    dependencies: [sbox_lfi_dep],
    install: false,
  )
endif
