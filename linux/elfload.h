#pragma once

#include "proc.h"

#include <stddef.h>
#include <stdint.h>

#define ELF_NO_MAP 0
#define ELF_MAP    1
#define ELF_REMAP  2

// Load the given static-pie ELF program into proc's address space. Returns
// true if successful.
bool
elf_load(struct LFILinuxProc *proc, const char *prog_path, int prog_fd,
    const uint8_t *prog, size_t prog_size, int map_op,
    struct ELFLoadInfo *info);

// Output a perf map file listing all the symbols from the ELF file. This
// allows perf to map the symbols in the dynamically mapped code for the
// sandbox to recognizable symbols.
bool
perf_output_jit_interface_file(const uint8_t *elf_data, size_t size,
    uintptr_t offset);
