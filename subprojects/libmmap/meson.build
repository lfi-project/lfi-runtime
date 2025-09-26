project('libmmap', ['c'], default_options: ['warning_level=3', 'buildtype=debugoptimized'], meson_version: '>= 0.56.0')

srcs = files(
  'mmap.c',
)

install = not meson.is_subproject()

libmmap = static_library('mmap', srcs, install: install)
mmap = declare_dependency(link_with: libmmap, include_directories: include_directories('.'))

if install
  install_headers('mmap.h')
endif

subdir('test')
