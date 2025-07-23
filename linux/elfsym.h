#pragma once

#include "linux.h"

// Find the offset of the dynamic section and place it in 'o_dynamic'.
bool
elf_dynamic(uint8_t *elfdat, size_t elfsize, uintptr_t *o_dynamic);

// Initialize the dynsym and dynstr sections in the proc so that symbols from
// the dynamic symbol table can be loaded with lfi_proc_sym.
bool
elf_loadsyms(struct LFILinuxProc *proc, uint8_t *elfdat, size_t elfsize);
