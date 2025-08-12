#pragma once

#include "proc.h"

#include <stddef.h>
#include <stdint.h>

#define ELF_NO_MAP 0
#define ELF_MAP    1
#define ELF_REMAP  2

// Returns the name of the ELF interpreter for this ELF file.
char *
elf_interp(const uint8_t *prog, size_t prog_size);

// Load the given ELF program and dynamic interpreter into proc's address
// space. Returns true if successful. The interp may be NULL if the program is
// statically linked or has no dynamic interpreter.
bool
elf_load(struct LFILinuxProc *proc, const char *prog_path, int prog_fd,
    const uint8_t *prog, size_t prog_size, const char *interp_path,
    int interp_fd, const uint8_t *interp, size_t interp_size, int map_op,
    bool reload, struct ELFLoadInfo *info);

// Output a perf map file listing all the symbols from the ELF file. This
// allows perf to map the symbols in the dynamically mapped code for the
// sandbox to recognizable symbols.
bool
perf_output_jit_interface_file(const uint8_t *elf_data, size_t size,
    uintptr_t offset);
