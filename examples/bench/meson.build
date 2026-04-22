libbench = shared_library('bench', 'lib_bench.c',
  c_args: example_lib_args, install: false)

executable('bench_passthrough',
  'main.cpp',
  dependencies: [sbox_passthrough_dep, thread_dep],
  install: false,
)

if have_lficc
  lib_bench_lfi = custom_target('lib_bench.lfi',
    input: 'lib_bench.c',
    output: 'lib_bench.lfi',
    command: [lficc, '-static-pie', '-o', '@OUTPUT@', '@INPUT@',
              '-lboxrt', '-Wl,--export-dynamic', '-O2'],
  )
  executable('bench_lfi',
    'main.cpp',
    cpp_args: ['-DSBOX_LFI'],
    dependencies: [sbox_lfi_dep, thread_dep],
    install: false,
  )
endif
