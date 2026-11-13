sbox_inc = include_directories('include')

# The LFI backend marshals calls for x86-64 and AArch64 only.
sbox_lfi_supported = cpu in ['x86_64', 'aarch64']

if sbox_lfi_supported
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
endif

sbox_passthrough_dep = declare_dependency(
  include_directories: sbox_inc,
  dependencies: [meson.get_compiler('cpp').find_library('dl')],
)

subdir('test')
