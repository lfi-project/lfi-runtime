# LFI Runtime

The LFI runtime is responsible for loading sandboxes and handling host calls. It is split into two sub-components:

* `core`: provides functionality for reserving virtual address space, mapping
  sandbox memory, transferring control to/from the sandbox. It does not impose
  any runtime API on the sandbox beyond the minimum necessary.
* `linux`: provides a Linux emulation layer on top of the core library.

# Installation

```
meson setup build
cd build
ninja
```

This produces `liblfi`, along with the `lfi-run` tool that can be used to run
binaries at the command-line.

See `core/include/lfi_core.h` and `linux/include/lfi_linux.h` for the API
provided by `liblfi`.

# Platform Support

The runtime currently only targets Linux. The core (but not the Linux emulator)
can also be built for macOS.
