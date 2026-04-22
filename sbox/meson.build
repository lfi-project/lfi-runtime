sbox_inc = include_directories('include')

libsbox_lfi = static_library('sbox_lfi',
  'lfi.cpp',
  include_directories: sbox_inc,
  dependencies: [lfi_linux],
  install: false,
)

sbox_lfi_dep = declare_dependency(
  link_with: libsbox_lfi,
  include_directories: sbox_inc,
  dependencies: [lfi_linux],
)

sbox_passthrough_dep = declare_dependency(
  include_directories: sbox_inc,
  dependencies: [meson.get_compiler('cpp').find_library('dl')],
)

subdir('test')
