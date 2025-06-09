#pragma once

#include <stddef.h>
#include <stdint.h>

// Returns the name of the ELF interpreter for this ELF file.
char *
elf_interp(uint8_t *prog, size_t prog_size);
