libadd = shared_library('add', 'lib_add.c',
  c_args: example_lib_args, install: false)

executable('add_passthrough',
  'main.cpp',
  include_directories: include_directories('.'),
  dependencies: [sbox_passthrough_dep],
  install: false,
)

if have_lficc
  lib_add_lfi = custom_target('lib_add.lfi',
    input: 'lib_add.c',
    output: 'lib_add.lfi',
    command: [lficc, '-static-pie', '-o', '@OUTPUT@', '@INPUT@',
              '-lboxrt', '-Wl,--export-dynamic', '-O2'],
  )
  executable('add_lfi',
    'main.cpp',
    include_directories: include_directories('.'),
    cpp_args: ['-DSBOX_LFI'],
    dependencies: [sbox_lfi_dep],
    install: false,
  )
endif
