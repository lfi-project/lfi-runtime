#pragma once

#include "linux.h"

// Find the offset of the dynamic section and place it in 'o_dynamic'.
bool
elf_dynamic(const uint8_t *elfdat, size_t elfsize, uintptr_t *o_dynamic);

// Initialize the dynsym and dynstr sections in the proc so that symbols from
// the dynamic symbol table can be loaded with lfi_proc_sym.
bool
elf_loadsyms(struct LFILinuxProc *proc, const uint8_t *elfdat, size_t elfsize);

// Looks up a symbol with lfi_proc_sym and places its address in 'p' if it is
// a valid, aligned sandbox code address.
bool
elf_loadsym(struct LFILinuxProc *proc, lfiptr *p, const char *sym);
