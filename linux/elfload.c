#include "elfload.h"
#include "buf.h"

#include <elf.h>

// We do not load ELF files that have more than 64 program headers.
#define PHNUM_MAX 64
// We do not allow interpreter paths longer than 1024 bytes.
#define INTERP_MAX 1024

char *
elf_interp(uint8_t *prog_data, size_t prog_size)
{
    struct Buf prog = (struct Buf) {
        .data = prog_data,
        .size = prog_size,
    };

    Elf64_Ehdr ehdr;
    size_t n = buf_read(prog, &ehdr, sizeof(ehdr), 0);
    if (n != sizeof(ehdr))
        return NULL;

    if (ehdr.e_phnum > PHNUM_MAX)
        return NULL;
    if (ehdr.e_phoff >= prog_size)
        return NULL;

    Elf64_Phdr phdr[ehdr.e_phnum];
    n = buf_read(prog, phdr, sizeof(Elf64_Phdr) * ehdr.e_phnum, ehdr.e_phoff);
    if (n != sizeof(Elf64_Phdr) * ehdr.e_phnum)
        return NULL;

    for (int x = 0; x < ehdr.e_phnum; x++) {
        if (phdr[x].p_type == PT_INTERP) {
            if (phdr[x].p_filesz >= INTERP_MAX)
                return NULL;
            char* interp = malloc(phdr[x].p_filesz);
            if (!interp)
                return NULL;
            size_t n = buf_read(prog, interp, phdr[x].p_filesz, phdr[x].p_offset);
            if (n != phdr[x].p_filesz) {
                free(interp);
                return NULL;
            }
            return interp;
        }
    }
    return NULL;
}
