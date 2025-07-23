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

The runtime currently targets Linux for Arm64 and x86-64. There is also
experimental support for macOS (futexes are not yet supported), and support for
RISC-V is in-progress.

# Usage

```
Usage: lfi-run [OPTION...] INPUT...

  -h, --help                show help
  -V, --verbose             verbose output
  --perf                    enable perf support
  -v, --verify              enable verification
  -p, --sys-passthrough     pass most system calls directly to the host
  --pagesize=<int>          system page size
  --env=<var=val>           set environment variable
  --dir=<box=host>          map sandbox path to host directory
  --wd=<dir>                working directory within sandbox
  -r, --restricted          apply --dir and --wd flags (default is --dir /=/ --wd $PWD for testing)
  <input>                   input command
```
