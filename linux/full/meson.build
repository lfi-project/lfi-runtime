# The full Linux configuration. Everything here is compiled only when
# -Dlinux_full=true, and reaches the minimal runtime through the hooks
# declared in linux/hooks.h.
linux_defines += '-DLINUX_FULL'

srcs += files(
  'proc.c',
  'thread.c',
  'trampoline.c',
  'sys.c',
)

# Pick a futex backend. The Linux backend uses the futex(2) syscall directly;
# the macOS backend uses os_sync_wait_on_address (requires macOS 14.4+);
# elsewhere futexes are not supported.
if host_machine.system() == 'darwin' and compiler.has_header('os/os_sync_wait_on_address.h')
  srcs += files('futex_macos.c')
elif host_machine.system() == 'linux'
  srcs += files('futex_linux.c')
else
  srcs += files('futex_none.c')
endif

subdir('sys')
