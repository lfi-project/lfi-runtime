libthreads = shared_library('threads', 'lib_threads.c',
  c_args: example_lib_args, install: false)

executable('threads_passthrough',
  'main.cpp',
  dependencies: [sbox_passthrough_dep, thread_dep],
  install: false,
)

if have_lficc
  lib_threads_lfi = custom_target('lib_threads.lfi',
    input: 'lib_threads.c',
    output: 'lib_threads.lfi',
    command: [lficc, '-static-pie', '-o', '@OUTPUT@', '@INPUT@',
              '-lboxrt', '-Wl,--export-dynamic', '-O2'],
  )
  executable('threads_lfi',
    'main.cpp',
    cpp_args: ['-DSBOX_LFI'],
    dependencies: [sbox_lfi_dep, thread_dep],
    install: false,
  )
endif
