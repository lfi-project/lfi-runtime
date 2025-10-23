#pragma once

#include "proc.h"

#include <stddef.h>
#include <stdint.h>

// Returns the name of the ELF interpreter for this ELF file.
char *
elf_interp(uint8_t *prog, size_t prog_size);

// Load the given ELF program and dynamic interpreter into proc's address
// space. Returns true if successful. The interp may be NULL if the program is
// statically linked or has no dynamic interpreter.
bool
elf_load(struct LFILinuxProc *proc, const char *prog_path, int prog_fd,
    uint8_t *prog, size_t prog_size, const char *interp_path, int interp_fd,
    uint8_t *interp, size_t interp_size, bool perform_map, bool reload,
    struct ELFLoadInfo *info);

// Output a perf map file listing all the symbols from the ELF file. This
// allows perf to map the symbols in the dynamically mapped code for the
// sandbox to recognizable symbols.
bool
perf_output_jit_interface_file(uint8_t *elf_data, size_t size,
    uintptr_t offset);
