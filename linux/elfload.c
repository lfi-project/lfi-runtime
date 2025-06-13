#include "elfload.h"

#include "align.h"
#include "buf.h"
#include "log.h"

#include <elf.h>

// We do not load ELF files that have more than 64 program headers.
#define PHNUM_MAX 64
// We do not allow interpreter paths longer than 1024 bytes.
#define INTERP_MAX 1024
// We do not allow executable segments above 1GiB.
#define CODE_MAX (1UL * 1024 * 1024 * 1024)

static lfiptr
p2l(struct LFIBox *box, uintptr_t p)
{
    return lfi_box_p2l(box, p);
}

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

    // Look for the PT_INTERP section, which will hold the name of the dynamic
    // interpreter that this ELF binary should be loaded with.
    for (int x = 0; x < ehdr.e_phnum; x++) {
        if (phdr[x].p_type == PT_INTERP) {
            if (phdr[x].p_filesz >= INTERP_MAX)
                return NULL;
            char *interp = malloc(phdr[x].p_filesz);
            if (!interp)
                return NULL;
            size_t n = buf_read(prog, interp, phdr[x].p_filesz,
                phdr[x].p_offset);
            if (n != phdr[x].p_filesz) {
                free(interp);
                return NULL;
            }
            return interp;
        }
    }
    return NULL;
}

// Convert ELF protection flags to LFI mmap protection flags.
static int
pflags(int prot)
{
    return ((prot & PF_R) ? LFI_PROT_READ : 0) |
        ((prot & PF_W) ? LFI_PROT_WRITE : 0) |
        ((prot & PF_X) ? LFI_PROT_EXEC : 0);
}

// Sanity-check the ELF header.
static bool
elf_check(Elf64_Ehdr *ehdr)
{
    return memcmp(ehdr->e_ident, ELFMAG, SELFMAG) == 0 &&
        ehdr->e_ident[EI_CLASS] == ELFCLASS64 && ehdr->e_version == EV_CURRENT;
}

// If a region is to be marked as executable, fill it with an instruction that
// is always safe to execute, assuming that the buffer is currently filled with
// zeroes. For Arm64 we don't have to do anything, because the 0 instruction
// always causes a fault. On x86-64, the 0 instruction does a store (!), so
// we have to fill the buffer with int3 instructions.
static void
sanitize(void *p, size_t sz, int prot)
{
    if ((prot & LFI_PROT_EXEC) == 0)
        return;
#if (defined(__x86_64__) || defined(_M_X64))
    const uint8_t SAFE_BYTE = 0xcc;
    memset(p, SAFE_BYTE, sz);
#endif
}

static bool
buf_read_elfseg(struct LFILinuxProc *proc, uintptr_t start, uintptr_t offset,
    uintptr_t end, size_t p_offset, size_t filesz, int prot, struct Buf buf,
    size_t pagesize, bool perform_map)
{
    struct LFIBox *box = proc->box;
    lfiptr p;
    if (perform_map)
        p = lfi_box_mapat(box, p2l(box, start), p2l(box, end - start),
            LFI_PROT_READ | LFI_PROT_WRITE, LFI_MAP_PRIVATE | LFI_MAP_ANONYMOUS,
            -1, 0);
    else
        p = lfi_box_mapat_register(box, p2l(box, start), p2l(box, end - start),
            prot, LFI_MAP_PRIVATE | LFI_MAP_ANONYMOUS, -1, 0);
    if (p == (lfiptr) -1) {
        return false;
    }

    if (!perform_map) {
        // We are just registering the mapping because it has already been
        // mapped by someone else.
        return true;
    }

    // If we have any subsequent errors, it is expected that the caller will
    // unmap all mapped regions.
    sanitize((void *) p, pagesize, prot);
    sanitize((void *) (end - pagesize), pagesize, prot);
    // Converting start to actual pointer requires no TUX_EXTERNAL_ADDR_SPACE.
    ssize_t n = buf_read(buf, (void *) (start + offset), filesz, p_offset);
    if (n != (ssize_t) filesz) {
        return false;
    }
    if (lfi_box_mprotect(box, p2l(box, start), p2l(box, end - start), prot) <
        0) {
        return false;
    }
    return true;
}

// Load a single in-memory ELF image into the address space.
static bool
elf_load_one(struct LFILinuxProc *proc, struct Buf elf, lfiptr base,
    size_t pagesize, bool perform_map, uintptr_t *p_first, uintptr_t *p_last,
    uintptr_t *p_entry, Elf64_Ehdr *ehdr)
{
    size_t n = buf_read(elf, ehdr, sizeof(*ehdr), 0);
    if (n != sizeof(*ehdr)) {
        LOG(proc->engine, "elf_load error: buf_read ehdr failed");
        return false;
    }

    if (!elf_check(ehdr)) {
        LOG(proc->engine, "elf_load error: elf_check failed");
        return false;
    }

    // In theory we could support fully static executables if not using
    // stores_only, since the LFI instrumentation is like a poor man's
    // position-independence. However, there is basically no reason to do this
    // instead of static-pie and supporting it adds some complexity. As a
    // result, we require binaries to be position-independent (ET_DYN).
    if (ehdr->e_type != ET_DYN) {
        LOG(proc->engine, "elf_load error: elf binary is not PIE");
        return false;
    }

    if (ehdr->e_phnum > PHNUM_MAX) {
        LOG(proc->engine, "elf_load error: e_phnum (%d) is too large",
            ehdr->e_phnum);
        return false;
    }

    Elf64_Phdr *phdrs = malloc(sizeof(Elf64_Phdr) * ehdr->e_phnum);
    if (!phdrs) {
        LOG(proc->engine, "elf_load error: out of memory");
        return false;
    }

    if (ehdr->e_phoff >= elf.size) {
        LOG(proc->engine, "elf_load error: e_phoff (%ld) is too large",
            ehdr->e_phoff);
        goto err1;
    }

    n = buf_read(elf, phdrs, sizeof(*phdrs) * ehdr->e_phnum, ehdr->e_phoff);
    if (n != sizeof(*phdrs) * ehdr->e_phnum) {
        LOG(proc->engine, "elf_load error: buf_read phdrs failed");
        goto err1;
    }

    if (ehdr->e_entry >= CODE_MAX) {
        LOG(proc->engine,
            "elf_load error: e_entry (0x%lx) is larger than CODE_MAX (0x%lx)",
            ehdr->e_entry, CODE_MAX);
        goto err1;
    }

    uintptr_t first = 0;
    uintptr_t last = 0;
    uintptr_t elfbase = base;
    uintptr_t laststart = -1;

    for (int i = 0; i < ehdr->e_phnum; i++) {
        Elf64_Phdr *p = &phdrs[i];
        if (p->p_type != PT_LOAD)
            continue;
        if (p->p_memsz == 0)
            continue;

        if (p->p_align % pagesize != 0) {
            LOG(proc->engine,
                "elf_load error: invalid p_align (%ld) not a multiple of pagesize (%ld)",
                p->p_align, pagesize);
            goto err1;
        }

        uintptr_t start = truncp(p->p_vaddr, p->p_align);
        uintptr_t end = ceilp(p->p_vaddr + p->p_memsz, p->p_align);
        uintptr_t offset = p->p_vaddr - start;

        if (p->p_memsz < p->p_filesz) {
            LOG(proc->engine,
                "elf_load error: p_memsz (0x%lx) < p_filesz (0x%lx)",
                p->p_memsz, p->p_filesz);
            goto err1;
        }
        if (end <= start || start >= CODE_MAX || end >= CODE_MAX) {
            LOG(proc->engine,
                "elf_load error: segment start (0x%lx) or end (0x%lx) is invalid",
                start, end);
            goto err1;
        }

        if (first == 0 || elfbase + start < first)
            first = elfbase + start;
        if (start == laststart)
            LOG(proc->engine,
                "warning: current segment will overwrite previous segment");

        laststart = start;

        LOG(proc->engine, "elf_load [0x%lx, 0x%lx] (P: %d)", base + start,
            base + end, pflags(p->p_flags));

        if (!buf_read_elfseg(proc, base + start, offset, base + end,
                p->p_offset, p->p_filesz, pflags(p->p_flags), elf, pagesize,
                perform_map)) {
            LOG(proc->engine, "elf_load error: reading elf segment failed");
            goto err1;
        }

        if (base == 0)
            base = base + start;
        if (base + end > last)
            last = base + end;
    }

    *p_last = last;
    *p_entry = base + ehdr->e_entry;
    *p_first = first;

    free(phdrs);

    return true;

err1:
    free(phdrs);
    return false;
}

bool
elf_load(struct LFILinuxProc *proc, uint8_t *prog_data, size_t prog_size,
    uint8_t *interp_data, size_t interp_size, bool perform_map,
    struct ELFLoadInfo *info)
{
    struct Buf prog = (struct Buf) {
        .data = prog_data,
        .size = prog_size,
    };

    struct Buf interp = (struct Buf) {
        .data = interp_data,
        .size = interp_size,
    };

    uintptr_t base = proc->box_info.min;
    uintptr_t p_first, p_last, p_entry, i_first, i_last, i_entry;
    bool has_interp = interp.data != NULL;
    size_t pagesize = lfi_opts(proc->engine->engine).pagesize;
    Elf64_Ehdr p_ehdr, i_ehdr;

    if (!elf_load_one(proc, prog, base, pagesize, perform_map, &p_first,
            &p_last, &p_entry, &p_ehdr))
        goto err;
    if (has_interp) {
        if (!elf_load_one(proc, interp, p_last, pagesize, perform_map, &i_first,
                &i_last, &i_entry, &i_ehdr))
            goto err;
    }

    if (proc->engine->opts.perf)
        if (!perf_output_jit_interface_file(prog_data, prog_size, p_first))
            LOG(proc->engine, "failed to generated perf map");

    struct LFIBox *box = proc->box;
    *info = (struct ELFLoadInfo) {
        .lastva = has_interp ? p2l(box, i_last) : p2l(box, p_last),
        .elfentry = p2l(box, p_entry),
        .ldentry = has_interp ? p2l(box, i_entry) : 0,
        .elfbase = p2l(box, p_first),
        .ldbase = has_interp ? p2l(box, i_first) : p2l(box, p_first),
        .elfphoff = p_ehdr.e_phoff,
        .elfphnum = p_ehdr.e_phnum,
        .elfphentsize = p_ehdr.e_phentsize,
    };

    return true;

err:
    // ELF loading failed: address space may be in a partially-loaded state.
    return false;
}
