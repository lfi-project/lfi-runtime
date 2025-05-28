project(
  'lfi-verifier',
  ['c'],
  default_options: ['warning_level=1', 'buildtype=release']
)

c_args_hardened = [
  '-fno-strict-aliasing',
  '-fno-delete-null-pointer-checks',
  '-fno-strict-overflow',
  '-Werror=implicit',
  '-Werror=incompatible-pointer-types',
  '-Werror=int-conversion',
]
compiler = meson.get_compiler('c')
has_auto_var_init = compiler.has_argument('-ftrivial-auto-var-init=zero')
if has_auto_var_init
  c_args_hardened += '-ftrivial-auto-var-init=zero'
endif

c_args_warnings = [
  '-Wno-unused-parameter',
  '-Wno-gnu-zero-variadic-macro-arguments',
]

arch_raw = get_option('arch')
arch = arch_raw.split(',')

dependencies = []
srcs = []
c_defines = []

if 'arm64' in arch
  dependencies += subproject('disarm').get_variable('disarm64').as_link_whole()
  srcs += files(
    'src/arm64/verify.c',
  )
  c_defines += '-DARCH_ARM64'
endif
if 'x64' in arch
  dependencies += subproject('fadec').get_variable('fadec').as_link_whole()
  srcs += files(
    'src/x64/verify.c',
    'src/x64/x86-all.bdd.c',
  )
  c_defines += '-DARCH_X64'
endif

liblfiv = static_library(
  'lfiv',
  srcs,
  c_args: c_args_hardened + c_args_warnings + c_defines,
  dependencies: dependencies,
  include_directories: include_directories('src/include'),
)

lfiv = declare_dependency(link_with: liblfiv, include_directories: include_directories('src/include'))
