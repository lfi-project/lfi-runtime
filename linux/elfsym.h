#pragma once

#include "linux.h"

// Initialize the dynsym and dynstr sections in the proc so that symbols from
// the dynamic symbol table can be loaded with lfi_proc_sym.
bool
elf_loadsyms(struct LFILinuxProc *proc, uint8_t *elfdat, size_t elfsize);
