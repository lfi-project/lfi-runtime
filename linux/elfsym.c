#include "elfsym.h"

#include "buf.h"
#include "elfdefs.h"
#include "proc.h"

#include <stdlib.h>
#include <string.h>

EXPORT uint64_t
lfi_proc_sym(struct LFILinuxProc *proc, const char *symname)
{
    Elf64_Sym *symbols = (Elf64_Sym *) (proc->dynsym.data);
    const char *strtab = (const char *) (proc->dynstr.data);
    size_t count = proc->dynsym.size / sizeof(Elf64_Sym);

    for (size_t i = 0; i < count; i++) {
        const char *name = strtab + symbols[i].st_name;
        if (strcmp(name, symname) == 0) {
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
    if (!loadsym(proc, &proc->libsyms.thread_create, "_lfi_thread_create"))
        return false;
    if (!loadsym(proc, &proc->libsyms.thread_destroy, "_lfi_thread_destroy"))
        return false;
    if (!loadsym(proc, &proc->libsyms.malloc, "_lfi_malloc"))
        return false;
    if (!loadsym(proc, &proc->libsyms.realloc, "_lfi_realloc"))
        return false;
    if (!loadsym(proc, &proc->libsyms.calloc, "_lfi_calloc"))
        return false;
    if (!loadsym(proc, &proc->libsyms.free, "_lfi_free"))
        return false;
    return true;
}

static bool
load_dynshs(uint8_t *elfdat, size_t elfsize, Elf64_Shdr **o_dynsym_sh, Elf64_Shdr **o_dynstr_sh)
{
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *) elfdat;

    if (elfsize < sizeof(Elf64_Ehdr) ||
        memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
        return false;
    }

    if (ehdr->e_shoff == 0 || ehdr->e_shentsize != sizeof(Elf64_Shdr))
        return false;
    if ((ehdr->e_shoff + (ehdr->e_shnum * sizeof(Elf64_Shdr))) > elfsize)
        return false;

    Elf64_Shdr *sh_table = (Elf64_Shdr *) (elfdat + ehdr->e_shoff);

    // Get section header string table.
    if (ehdr->e_shstrndx >= ehdr->e_shnum)
        return false;
    Elf64_Shdr *shstr_sh = &sh_table[ehdr->e_shstrndx];
    if (shstr_sh->sh_offset + shstr_sh->sh_size > elfsize)
        return false;
    const char *shstrtab = (const char *) (elfdat + shstr_sh->sh_offset);

    // Locate .dynsym and its associated .dynstr.
    Elf64_Shdr *dynsym_sh = NULL;
    Elf64_Shdr *dynstr_sh = NULL;

    for (int i = 0; i < ehdr->e_shnum; ++i) {
        const char *name = shstrtab + sh_table[i].sh_name;
        if (strcmp(name, ".dynsym") == 0) {
            dynsym_sh = &sh_table[i];
            if (dynsym_sh->sh_link < ehdr->e_shnum) {
                dynstr_sh = &sh_table[dynsym_sh->sh_link];
            }
            break;
        }
    }

    *o_dynsym_sh = dynsym_sh;
    *o_dynstr_sh = dynstr_sh;

    return true;
}

bool
elf_dynamic(uint8_t *elfdat, size_t elfsize, uintptr_t *o_dynamic)
{
    struct Buf prog = (struct Buf) {
        .data = elfdat,
        .size = elfsize,
    };

    Elf64_Ehdr ehdr;
    size_t n = buf_read(prog, &ehdr, sizeof(ehdr), 0);
    if (n != sizeof(ehdr))
        return NULL;

    Elf64_Phdr phdr[ehdr.e_phnum];
    n = buf_read(prog, phdr, sizeof(Elf64_Phdr) * ehdr.e_phnum, ehdr.e_phoff);
    if (n != sizeof(Elf64_Phdr) * ehdr.e_phnum)
        return NULL;

    for (int x = 0; x < ehdr.e_phnum; x++) {
        if (phdr[x].p_type == PT_DYNAMIC) {
            *o_dynamic = phdr[x].p_vaddr;
            return true;
        }
    }
    return false;
}

bool
elf_loadsyms(struct LFILinuxProc *proc, uint8_t *elfdat, size_t elfsize)
{
    Elf64_Shdr *dynsym_sh;
    Elf64_Shdr *dynstr_sh;

    bool ok = load_dynshs(elfdat, elfsize, &dynsym_sh, &dynstr_sh);
    if (!ok)
        return false;

    if (!dynsym_sh || !dynstr_sh)
        return false;
    if (dynsym_sh->sh_offset + dynsym_sh->sh_size > elfsize ||
        dynstr_sh->sh_offset + dynstr_sh->sh_size > elfsize)
        return false;

    proc->dynsym.size = dynsym_sh->sh_size;
    proc->dynsym.data = malloc(proc->dynsym.size);
    if (!proc->dynsym.data)
        return false;
    memcpy(proc->dynsym.data, &elfdat[dynsym_sh->sh_offset], proc->dynsym.size);

    proc->dynstr.size = dynstr_sh->sh_size;
    proc->dynstr.data = malloc(proc->dynstr.size);
    if (!proc->dynstr.data)
        goto err1;
    memcpy(proc->dynstr.data, &elfdat[dynstr_sh->sh_offset], proc->dynstr.size);

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
