libshm = shared_library('shm', 'lib_shm.c',
  c_args: example_lib_args, install: false)

executable('shm_passthrough',
  'main.cpp',
  dependencies: [sbox_passthrough_dep],
  install: false,
)

if have_lficc
  lib_shm_lfi = custom_target('lib_shm.lfi',
    input: 'lib_shm.c',
    output: 'lib_shm.lfi',
    command: [lficc, '-static-pie', '-o', '@OUTPUT@', '@INPUT@',
              '-lboxrt', '-Wl,--export-dynamic', '-O2'],
  )
  executable('shm_lfi',
    'main.cpp',
    cpp_args: ['-DSBOX_LFI'],
    dependencies: [sbox_lfi_dep],
    install: false,
  )
endif
