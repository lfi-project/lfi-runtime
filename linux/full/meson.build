# The full Linux configuration. Everything here is compiled only when
# -Dlinux_full=true, and reaches the minimal runtime through the hooks
# declared in linux/hooks.h.
linux_defines += '-DLINUX_FULL'

srcs += files(
  'proc.c',
  'thread.c',
  'sys.c',
)

subdir('sys')
