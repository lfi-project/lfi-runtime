project('libboxmap', ['c'], default_options: ['warning_level=3', 'buildtype=debugoptimized'], meson_version: '>= 0.56.0')

srcs = files(
  'boxmap.c',
  'extalloc.c',
  'err.c',
)

install = not meson.is_subproject()

libboxmap = static_library('boxmap', srcs, install: install)
boxmap = declare_dependency(link_with: libboxmap, include_directories: include_directories('.'))

if install
  install_headers('boxmap.h')
endif

subdir('test')
