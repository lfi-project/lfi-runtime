# LFI Runtime

The LFI runtime is responsible for loading sandboxes and handling host calls. It is split into two sub-components:

* `core`: provides functionality for reserving virtual address space, mapping
  sandbox memory, transferring control to/from the sandbox. It does not impose
  any runtime API on the sandbox beyond the minimum necessary.
* `linux`: provides a Linux emulation layer on top of the core library.
* `sbox`: a C++ library for calling sandboxed functions with a lightweight
  tainting API. See [sbox/README.md](sbox/README.md) for details.

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
experimental support for RISC-V that is in-progress.

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

# Threat Model

LFI is currently being used for library isolation, where the library has been
written by an honest party. In addition, LFI is only currently being used in
`SYS_MINIMAL` mode, which vastly reduces the number of system calls available
to sandboxed programs.

Work to harden the runtime for non-`SYS_MINIMAL` usage and for arbitrarily
malicious programs is ongoing.

Sandboxed programs are able to cause faults or cause the runtime to
fault/abort. Work to allow the runtime to gracefully recover by handling the
signal is ongoing.
