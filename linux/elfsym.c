#include "elfsym.h"

#include "buf.h"
#include "elfdefs.h"
#include "proc.h"

#include <stdalign.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static bool
region_ok(uint64_t off, uint64_t len, uint64_t size)
{
    return off <= size && len <= size - off;
}

static bool
str_in_bounds(const char *strtab, uint64_t strsize, uint64_t off)
{
    if (off >= strsize)
        return false;
    return memchr(strtab + off, '\0', strsize - off) != NULL;
}

EXPORT uint64_t
lfi_proc_sym(struct LFILinuxProc *proc, const char *symname)
{
    Elf64_Sym *symbols = (Elf64_Sym *) (proc->dynsym.data);
    const char *strtab = (const char *) (proc->dynstr.data);
    size_t strsize = proc->dynstr.size;
    size_t count = proc->dynsym.size / sizeof(Elf64_Sym);

    for (size_t i = 0; i < count; i++) {
        uint32_t off = symbols[i].st_name;
        if (!str_in_bounds(strtab, strsize, off))
            continue;
        if (strcmp(strtab + off, symname) == 0) {
            return proc->elfinfo.elfbase + symbols[i].st_value;
        }
    }

    return 0;
}

static bool
loadsym(struct LFILinuxProc *proc, lfiptr *p, const char *sym)
{
    lfiptr bp = lfi_proc_sym(proc, sym);
    if (!lfi_box_ptrvalid(proc->box, bp)) {
        return false;
    }
    if (bp % 4 != 0) {
        LOG(proc->engine, "%s: invalid alignment", sym);
        return false;
    }
    *p = bp;
    return true;
}

static bool
load_libsyms(struct LFILinuxProc *proc)
{
#ifndef SYS_MINIMAL
    if (!loadsym(proc, &proc->libsyms.thread_create, "_lfi_thread_create"))
        LOG(proc->engine, "warning: _lfi_thread_create not found");
    if (!loadsym(proc, &proc->libsyms.thread_destroy, "_lfi_thread_destroy"))
        LOG(proc->engine, "warning: _lfi_thread_destroy not found");
#endif
    if (!loadsym(proc, &proc->libsyms.malloc, "_lfi_malloc"))
        LOG(proc->engine, "warning: _lfi_malloc not found");
    if (!loadsym(proc, &proc->libsyms.realloc, "_lfi_realloc"))
        LOG(proc->engine, "warning: _lfi_realloc not found");
    if (!loadsym(proc, &proc->libsyms.calloc, "_lfi_calloc"))
        LOG(proc->engine, "warning: _lfi_calloc not found");
    if (!loadsym(proc, &proc->libsyms.free, "_lfi_free"))
        LOG(proc->engine, "warning: _lfi_free not found");
    if (!loadsym(proc, &proc->libsyms.setjmp, "_lfi_setjmp"))
        LOG(proc->engine, "warning: _lfi_setjmp not found");
    return true;
}

static bool
read_shdr(struct Buf buf, const Elf64_Ehdr *ehdr, uint64_t idx,
    Elf64_Shdr *out)
{
    uint64_t off = ehdr->e_shoff + idx * sizeof(Elf64_Shdr);
    return buf_read(buf, out, sizeof(*out), off) == sizeof(*out);
}

static bool
load_dynshs(const uint8_t *elfdat, size_t elfsize, Elf64_Shdr *o_dynsym,
    Elf64_Shdr *o_dynstr)
{
    struct Buf buf = (struct Buf) {
        .fd = -1,
        .data = elfdat,
        .size = elfsize,
    };

    // Copy the ELF header into an aligned local.
    Elf64_Ehdr ehdr;
    if (buf_read(buf, &ehdr, sizeof(ehdr), 0) != sizeof(ehdr))
        return false;
    if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0)
        return false;
    if (ehdr.e_shoff == 0 || ehdr.e_shentsize != sizeof(Elf64_Shdr))
        return false;
    if (ehdr.e_shnum == 0)
        return false;

    uint64_t shtab_bytes = (uint64_t) ehdr.e_shnum * sizeof(Elf64_Shdr);
    if (!region_ok(ehdr.e_shoff, shtab_bytes, elfsize))
        return false;
    if (ehdr.e_shstrndx >= ehdr.e_shnum)
        return false;

    // Section header string table.
    Elf64_Shdr shstr;
    if (!read_shdr(buf, &ehdr, ehdr.e_shstrndx, &shstr))
        return false;
    if (!region_ok(shstr.sh_offset, shstr.sh_size, elfsize))
        return false;
    const char *shstrtab = (const char *) &elfdat[shstr.sh_offset];
    uint64_t shstrsize = shstr.sh_size;

    // Locate .dynsym and its associated .dynstr.
    for (uint64_t i = 0; i < ehdr.e_shnum; i++) {
        Elf64_Shdr sh;
        if (!read_shdr(buf, &ehdr, i, &sh))
            return false;
        if (!str_in_bounds(shstrtab, shstrsize, sh.sh_name))
            continue;
        if (strcmp(shstrtab + sh.sh_name, ".dynsym") != 0)
            continue;

        if (sh.sh_link >= ehdr.e_shnum)
            return false;
        Elf64_Shdr link;
        if (!read_shdr(buf, &ehdr, sh.sh_link, &link))
            return false;
        *o_dynsym = sh;
        *o_dynstr = link;
        return true;
    }

    return false;
}

bool
elf_dynamic(const uint8_t *elfdat, size_t elfsize, uintptr_t *o_dynamic)
{
    struct Buf prog = (struct Buf) {
        .fd = -1,
        .data = elfdat,
        .size = elfsize,
    };

    Elf64_Ehdr ehdr;
    size_t n = buf_read(prog, &ehdr, sizeof(ehdr), 0);
    if (n != sizeof(ehdr))
        return false;

    for (uint64_t x = 0; x < ehdr.e_phnum; x++) {
        Elf64_Phdr phdr;
        n = buf_read(prog, &phdr, sizeof(phdr),
            ehdr.e_phoff + x * sizeof(Elf64_Phdr));
        if (n != sizeof(phdr))
            return false;
        if (phdr.p_type == PT_DYNAMIC) {
            *o_dynamic = phdr.p_vaddr;
            return true;
        }
    }
    return false;
}

bool
elf_loadsyms(struct LFILinuxProc *proc, const uint8_t *elfdat, size_t elfsize)
{
    Elf64_Shdr dynsym_sh;
    Elf64_Shdr dynstr_sh;

    bool ok = load_dynshs(elfdat, elfsize, &dynsym_sh, &dynstr_sh);
    if (!ok)
        return false;

    if (!region_ok(dynsym_sh.sh_offset, dynsym_sh.sh_size, elfsize) ||
        !region_ok(dynstr_sh.sh_offset, dynstr_sh.sh_size, elfsize))
        return false;

    proc->dynsym.size = dynsym_sh.sh_size;
    proc->dynsym.data = malloc(proc->dynsym.size ? proc->dynsym.size : 1);
    if (!proc->dynsym.data)
        return false;
    memcpy(proc->dynsym.data, &elfdat[dynsym_sh.sh_offset], proc->dynsym.size);

    proc->dynstr.size = dynstr_sh.sh_size;
    proc->dynstr.data = malloc(proc->dynstr.size ? proc->dynstr.size : 1);
    if (!proc->dynstr.data)
        goto err1;
    memcpy(proc->dynstr.data, &elfdat[dynstr_sh.sh_offset], proc->dynstr.size);

    if (!load_libsyms(proc))
        goto err2;

    return true;

err2:
    free(proc->dynstr.data);
    proc->dynstr = (struct ElfSection) { 0 };
err1:
    free(proc->dynsym.data);
    proc->dynsym = (struct ElfSection) { 0 };
    return false;
}
