#include "log.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifndef __linux__

bool
perf_output_jit_interface_file(const uint8_t *elf_data, size_t size,
    uintptr_t offset)
{
    (void) elf_data, (void) size, (void) offset;
    ERROR("perf is not supported on non-linux platforms");
    return false;
}

#else

#include <dirent.h>
#include <elf.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

static bool
direxists(const char *dirname)
{
    DIR *dir = opendir(dirname);
    if (dir) {
        closedir(dir);
        return true;
    }
    return false;
}

static bool
region_ok(uint64_t off, uint64_t len, uint64_t size)
{
    return off <= size && len <= size - off;
}

static bool
elf_copy(const uint8_t *data, size_t size, uint64_t off, void *dst,
    size_t len)
{
    if (!region_ok(off, len, size))
        return false;
    memcpy(dst, data + off, len);
    return true;
}

static bool
str_in_bounds(const char *strtab, uint64_t strsize, uint64_t off)
{
    if (off >= strsize)
        return false;
    return memchr(strtab + off, '\0', strsize - off) != NULL;
}

bool
perf_output_jit_interface_file(const uint8_t *elf_data, size_t size,
    uintptr_t offset)
{
    char *tmpdir = "/data/local/tmp";
    if (!direxists(tmpdir))
        tmpdir = "/tmp";
    if (!direxists(tmpdir))
        tmpdir = ".";

    char output_file[256];
    snprintf(output_file, sizeof(output_file), "%s/perf-%d.map", tmpdir,
        getpid());
    FILE *out = fopen(output_file, "w");
    if (!out) {
        ERROR("failed to create output file %s: %s", output_file,
            strerror(errno));
        goto err;
    }

    // Ensure the buffer is large enough for an ELF header
    if (size < sizeof(Elf64_Ehdr)) {
        ERROR("Buffer too small for ELF header.\n");
        goto err;
    }

    // Read the ELF header into an aligned local.
    Elf64_Ehdr ehdr;
    if (!elf_copy(elf_data, size, 0, &ehdr, sizeof(ehdr))) {
        ERROR("Buffer too small for ELF header.\n");
        goto err;
    }

    // Verify ELF magic number
    if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0) {
        ERROR("Not an ELF file.\n");
        goto err;
    }

    if (ehdr.e_shentsize != sizeof(Elf64_Shdr)) {
        ERROR("Invalid ELF section header size.\n");
        goto err;
    }

    // Ensure the section header table fits within the buffer.
    uint64_t shtab_bytes = (uint64_t) ehdr.e_shnum * sizeof(Elf64_Shdr);
    if (!region_ok(ehdr.e_shoff, shtab_bytes, size)) {
        ERROR("Invalid ELF section headers.\n");
        goto err;
    }

    // Find symbol table and its linked string table.
    Elf64_Shdr symtab, strtab;
    bool found = false;
    for (uint64_t i = 0; i < ehdr.e_shnum; i++) {
        Elf64_Shdr sh;
        if (!elf_copy(elf_data, size,
                ehdr.e_shoff + i * sizeof(Elf64_Shdr), &sh, sizeof(sh)))
            goto err;
        if (sh.sh_type != SHT_SYMTAB)
            continue;
        if (sh.sh_link >= ehdr.e_shnum) {
            ERROR("Invalid symbol table link.\n");
            goto err;
        }
        if (!elf_copy(elf_data, size,
                ehdr.e_shoff + (uint64_t) sh.sh_link * sizeof(Elf64_Shdr),
                &strtab, sizeof(strtab)))
            goto err;
        symtab = sh;
        found = true;
        break;
    }

    if (!found) {
        ERROR("No symbol table found.\n");
        goto err;
    }

    // Ensure symbol and string tables fit within the buffer.
    if (!region_ok(symtab.sh_offset, symtab.sh_size, size) ||
        !region_ok(strtab.sh_offset, strtab.sh_size, size)) {
        ERROR("Invalid symbol or string table offsets.\n");
        goto err;
    }

    const char *strtab_data = (const char *) (elf_data + strtab.sh_offset);
    uint64_t strtab_size = strtab.sh_size;

    // Calculate number of symbols.
    uint64_t num_symbols = symtab.sh_size / sizeof(Elf64_Sym);

    // Print symbols.
    for (uint64_t i = 0; i < num_symbols; i++) {
        // Copy each symbol into an aligned local.
        Elf64_Sym sym;
        if (!elf_copy(elf_data, size,
                symtab.sh_offset + i * sizeof(Elf64_Sym), &sym, sizeof(sym)))
            goto err;
        if (sym.st_size == 0)
            continue;
        if (sym.st_name != 0 &&
            str_in_bounds(strtab_data, strtab_size, sym.st_name)) {
            fprintf(out, "0x%016lx 0x%08lx %s\n",
                (unsigned long) sym.st_value + offset,
                (unsigned long) sym.st_size,
                &strtab_data[sym.st_name]);
        }
    }

    fclose(out);
    LOG_("Perf map written to: %s\n", output_file);
    return true;
err:
    if (out)
        fclose(out);
    return false;
}

#endif
