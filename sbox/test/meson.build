test_inc = include_directories('.')

thread_dep = dependency('threads')
dl_dep = meson.get_compiler('cpp').find_library('dl')

# Passthrough test shared libraries (loaded via dlopen). Override the
# project-wide -fvisibility=hidden so the test symbols are exported.
visibility_args = ['-fvisibility=default']
testlib_so = shared_library('testlib', 'testlib.c',
  c_args: visibility_args, install: false)
testlib2_so = shared_library('testlib2', 'testlib2.c',
  c_args: visibility_args, install: false)

# Shared test categories, each built for both passthrough and LFI.
test_categories = [
  'arithmetic', 'fn_handles', 'misc', 'structs',
  'pointers', 'memory', 'strings', 'callbacks',
]

foreach cat : test_categories
  exe = executable('test_passthrough_' + cat,
    'test_passthrough_' + cat + '.cpp',
    include_directories: [test_inc],
    dependencies: [sbox_passthrough_dep],
    install: false,
  )
  test('passthrough_' + cat, exe,
    workdir: meson.current_build_dir(),
    depends: [testlib_so],
    protocol: 'tap',
  )
endforeach

# Passthrough-only extra tests
foreach t : ['multi', 'reentrant']
  exe = executable('test_passthrough_' + t,
    'test_passthrough_' + t + '.cpp',
    include_directories: [test_inc],
    dependencies: [sbox_passthrough_dep],
    install: false,
  )
  test('passthrough_' + t, exe,
    workdir: meson.current_build_dir(),
    depends: [testlib_so],
    protocol: 'tap',
  )
endforeach

# Multi-sandbox (passthrough)
test_passthrough_multi_sandbox = executable('test_passthrough_multi_sandbox',
  'test_passthrough_multi_sandbox.cpp',
  include_directories: [test_inc],
  dependencies: [sbox_passthrough_dep],
  install: false,
)
test('passthrough_multi_sandbox', test_passthrough_multi_sandbox,
  workdir: meson.current_build_dir(),
  depends: [testlib_so, testlib2_so],
  protocol: 'tap',
)

# Threads test (passthrough)
test_threads = executable('test_threads',
  'test_threads.cpp',
  include_directories: [test_inc],
  dependencies: [sbox_passthrough_dep, thread_dep],
  install: false,
)
test('threads', test_threads,
  workdir: meson.current_build_dir(),
  depends: [testlib_so],
)

# LFI tests - only if the LFI cross-compiler is available.
lficc_name = cpu + '_lfi-linux-musl-clang'
lficc = find_program(lficc_name, required: false)

if lficc.found()
  testlib_lfi = custom_target('testlib.lfi',
    input: 'testlib.c',
    output: 'testlib.lfi',
    command: [lficc, '-static-pie', '-o', '@OUTPUT@', '@INPUT@',
              '-lboxrt', '-Wl,--export-dynamic', '-O2'],
  )
  testlib2_lfi = custom_target('testlib2.lfi',
    input: 'testlib2.c',
    output: 'testlib2.lfi',
    command: [lficc, '-static-pie', '-o', '@OUTPUT@', '@INPUT@',
              '-lboxrt', '-Wl,--export-dynamic', '-O2'],
  )

  foreach cat : test_categories
    exe = executable('test_lfi_' + cat,
      'test_lfi_' + cat + '.cpp',
      include_directories: [test_inc],
      dependencies: [sbox_lfi_dep],
      install: false,
    )
    test('lfi_' + cat, exe,
      workdir: meson.current_build_dir(),
      depends: [testlib_lfi],
      protocol: 'tap',
    )
  endforeach

  test_lfi_multi_sandbox = executable('test_lfi_multi_sandbox',
    'test_lfi_multi_sandbox.cpp',
    include_directories: [test_inc],
    dependencies: [sbox_lfi_dep],
    install: false,
  )
  test('lfi_multi_sandbox', test_lfi_multi_sandbox,
    workdir: meson.current_build_dir(),
    depends: [testlib_lfi, testlib2_lfi],
    protocol: 'tap',
  )
endif

# Compile-failure tests (verify that type errors are caught at compile time)
compile_fail_script = find_program('test_compile_fail.sh')
cxx_cmd = meson.get_compiler('cpp').cmd_array()

compile_fail_tests = [
  'wrong_callback_type',
  'wrong_data_ptr_type',
  'raw_pointer_arg',
  'raw_callback_arg',
]

foreach t : compile_fail_tests
  test('compile_fail_' + t, compile_fail_script,
    args: cxx_cmd + [
      '-std=c++17',
      '-I' + meson.current_source_dir() / '..' / 'include',
      '-c',
      meson.current_source_dir() / 'compile_fail' / (t + '.cpp'),
      '-o', '/dev/null',
    ],
  )
endforeach
