#pragma once

#include "proc.h"

#include <stddef.h>
#include <stdint.h>

struct ELFLoadInfo {
    lfiptr lastva;
    lfiptr elfentry;
    lfiptr ldentry;
    lfiptr elfbase;
    lfiptr ldbase;
    uint64_t elfphoff;
    uint16_t elfphnum;
    uint16_t elfphentsize;
};

// Returns the name of the ELF interpreter for this ELF file.
char *
elf_interp(uint8_t *prog, size_t prog_size);

// Load the given ELF program and dynamic interpreter into proc's address
// space. Returns true if successful. The interp may be NULL if the program is
// statically linked or has no dynamic interpreter.
bool
elf_load(struct LFILinuxProc *proc, uint8_t *prog, size_t prog_size,
    uint8_t *interp, size_t interp_size, bool perform_map,
    struct ELFLoadInfo *info);
