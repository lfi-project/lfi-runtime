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
    suite: ['sbox'],
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
    suite: ['sbox'],
  )
endforeach

# Passthrough static mode, with the test library linked in directly.
test_passthrough_static = executable('test_passthrough_static',
  'test_passthrough_static.cpp', 'testlib.c',
  include_directories: [test_inc],
  dependencies: [sbox_passthrough_dep],
  install: false,
)
test('passthrough_static', test_passthrough_static,
  protocol: 'tap',
  suite: ['sbox'],
)

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
  suite: ['sbox'],
)

test_passthrough_callback_cross_sandbox = executable(
  'test_passthrough_callback_cross_sandbox',
  'test_passthrough_callback_cross_sandbox.cpp',
  include_directories: [test_inc],
  dependencies: [sbox_passthrough_dep],
  install: false,
)
test('passthrough_callback_cross_sandbox', test_passthrough_callback_cross_sandbox,
  workdir: meson.current_build_dir(),
  depends: [testlib_so, testlib2_so],
  protocol: 'tap',
  suite: ['sbox'],
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
  suite: ['sbox'],
)

# LFI tests - only if the LFI cross-compiler is available.
lficc_name = cpu + '_lfi-linux-musl-clang'
lficc = find_program(lficc_name, required: false)

if lficc.found() and sbox_lfi_supported
  # The minimal runtime provides no mmap, so the sandboxed test libraries use
  # minialloc, a brk-based allocator, in place of the libc allocator. The full
  # configuration has mmap and uses the libc allocator.
  testlib_extra_srcs = []
  if not get_option('linux_full')
    minialloc = subproject('minialloc')
    testlib_extra_srcs = minialloc.get_variable('minialloc_srcs')
  endif

  testlib_lfi = custom_target('testlib.lfi',
    input: ['testlib.c'] + testlib_extra_srcs,
    output: 'testlib.lfi',
    command: [lficc, '-static-pie', '-o', '@OUTPUT@', '@INPUT@',
              '-lboxrt', '-Wl,--export-dynamic', '-O2'],
  )
  testlib2_lfi = custom_target('testlib2.lfi',
    input: ['testlib2.c'] + testlib_extra_srcs,
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
      suite: ['sbox'],
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
    suite: ['sbox'],
  )

  test_lfi_threads = executable('test_lfi_threads',
    'test_lfi_threads.cpp',
    include_directories: [test_inc],
    dependencies: [sbox_lfi_dep, thread_dep],
    install: false,
  )
  test('lfi_threads', test_lfi_threads,
    workdir: meson.current_build_dir(),
    depends: [testlib_lfi],
    protocol: 'tap',
    suite: ['sbox'],
  )

  test_lfi_detach = executable('test_lfi_detach',
    'test_lfi_detach.cpp',
    include_directories: [test_inc],
    dependencies: [sbox_lfi_dep, thread_dep],
    install: false,
  )
  test('lfi_detach', test_lfi_detach,
    workdir: meson.current_build_dir(),
    depends: [testlib_lfi],
    protocol: 'tap',
    suite: ['sbox'],
  )
endif

# Compile-failure tests (verify that type errors are caught at compile time)
compile_fail_script = find_program('test_compile_fail.sh')
cxx_cmd = meson.get_compiler('cpp').cmd_array()

ptr_arg_error = 'Pointer arguments must be sbox<T*> or sbox_safe<T*>'

compile_fail_tests = [
  ['wrong_callback_type', ptr_arg_error],
  ['wrong_data_ptr_type', ptr_arg_error],
  ['raw_pointer_arg', ptr_arg_error],
  ['raw_callback_arg', ptr_arg_error],
  ['lfi_struct_by_value', 'LFI call arguments must be scalars'],
  ['lfi_narrow_callback_arg', 'LFI callback integer parameters'],
  ['raw_pointer_callback_param', 'Callback pointer and reference parameters must be'],
  ['static_raw_pointer_arg', ptr_arg_error],
  ['sig_with_sbox_fn', 'no matching'],
  ['raw_pointer_fn_handle', ptr_arg_error],
  ['lfi_callback_wrong_backend', 'Callback Sandbox parameter must match'],
  ['ctx_out_raw_pointer', 'out requires a trivially-copyable type that is not'],
  ['lfi_reference_param', 'parameters cannot be references'],
  ['alloc_sbox_safe', 'sbox_safe cannot be stored in sandbox memory'],
  ['alloc_raw_pointer', 'Pointers in sandbox memory must be held as sbox<T*>'],
  ['verify_polymorphic', 'Polymorphic types cannot be stored in sandbox memory'],
  ['copy_from_pointer_dest', 'copy_from destination must be a trivially-copyable'],
  ['verify_bool', 'bool values in sandbox memory must be accessed as'],
  ['verify_unscoped_enum', 'Enums in sandbox memory must be scoped'],
]

fs = import('fs')

foreach t : compile_fail_tests
  src = 'compile_fail' / (t[0] + '.cpp')
  if not sbox_lfi_supported and fs.read(src).contains('sbox/lfi.h')
    continue
  endif
  test('compile_fail_' + t[0], compile_fail_script,
    args: [t[1]] + cxx_cmd + [
      '-std=c++17',
      '-I' + meson.current_source_dir() / '..' / 'include',
      '-I' + meson.project_source_root() / 'core' / 'include',
      '-I' + meson.project_source_root() / 'linux' / 'include',
      '-c',
      meson.current_source_dir() / src,
      '-o', '/dev/null',
    ],
    suite: ['sbox'],
  )
endforeach
