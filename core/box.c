#include "core.h"
#include "lfi_core.h"

#include <assert.h>
#include <stdlib.h>
#include <sys/mman.h>

// Use of the following two functions assumes that the sandbox and host share
// an address space.
//
// Convert sandbox pointer to host pointer.
static inline uintptr_t
l2p(struct LFIBox *box, lfiptr l)
{
    return (uintptr_t) l;
}

// Convert host pointer to sandbox pointer.
static inline lfiptr
p2l(struct LFIBox *box, uintptr_t p)
{
    return (lfiptr) p;
}

// Runtime call entrypoints. These are defined in runtime.S.
extern void
lfi_syscall_entry(void) asm ("lfi_syscall_entry");
extern void
lfi_get_tp(void) asm ("lfi_get_tp");
extern void
lfi_set_tp(void) asm ("lfi_set_tp");
extern void
lfi_ret(void) asm ("lfi_ret");

// Initialize the sys page (at the beginning of the sandbox) to contain the
// runtime call entrypoints.
static void
syssetup(struct LFIBox *box)
{
    // Map read/write.
    box->sys = mmap((void *) box->base, box->engine->opts.pagesize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    assert(box->sys == (void *) box->base);

    box->sys->rtcalls[0] = (uintptr_t) &lfi_syscall_entry;
    box->sys->rtcalls[1] = (uintptr_t) &lfi_get_tp;
    box->sys->rtcalls[2] = (uintptr_t) &lfi_set_tp;
    box->sys->rtcalls[3] = (uintptr_t) &lfi_ret;

    // Map read-only.
    int r = mprotect((void *) box->base, box->engine->opts.pagesize, PROT_READ);
    assert(r == 0);
}

EXPORT struct LFIBox *
lfi_box_new(struct LFIEngine *engine)
{
    struct LFIBox *box = malloc(sizeof(struct LFIBox));
    if (!box) {
        lfi_error = LFI_ERR_ALLOC;
        return NULL;
    }

    size_t size = engine->opts.boxsize;
    uintptr_t base = boxmap_addspace(engine->bm, size);
    if (base == 0) {
        lfi_error = LFI_ERR_BOXMAP;
        goto err1;
    }

    *box = (struct LFIBox) {
        .base = base,
        .size = size,
        .engine = engine,
        .min = base + engine->guardsize + engine->opts.pagesize, // for sys page
        .max = base + size - engine->guardsize,
    };
    syssetup(box);

    bool ok = mm_init(&box->mm, box->min, box->max - box->min,
        engine->opts.pagesize);
    if (!ok) {
        lfi_error = LFI_ERR_MMAP;
        goto err2;
    }

    return box;

err2:
    boxmap_rmspace(engine->bm, base, size);
err1:
    free(box);
    return NULL;
}

EXPORT struct LFIBoxInfo
lfi_box_info(struct LFIBox *box)
{
    return (struct LFIBoxInfo) {
        .base = box->base,
        .size = box->size,
        .min = box->min,
        .max = box->max,
    };
}

// These functions convert from the LFI mapping flags to the underlying host's
// mmap flags. This allows liblfi to export a more platform-independent API.
static int
host_prot(int prot)
{
    return ((prot & LFI_PROT_READ) ? PROT_READ : 0) |
        ((prot & LFI_PROT_WRITE) ? PROT_WRITE : 0) |
        ((prot & LFI_PROT_EXEC) ? PROT_EXEC : 0);
}

static int
host_flags(int flags)
{
    return ((flags & LFI_MAP_PRIVATE) ? MAP_PRIVATE : 0) |
        ((flags & LFI_MAP_ANONYMOUS) ? MAP_ANONYMOUS : 0) |
        ((flags & LFI_MAP_FIXED) ? MAP_FIXED : 0);
}

// Create a fixed memory mapping with the LFI_MAP/LFI_PROT flags.
static int
mapmem(uintptr_t start, size_t size, int prot, int flags, int fd, off_t off)
{
    void *mem = mmap((void *) start, size, host_prot(prot),
        host_flags(flags) | MAP_FIXED, fd, off);
    if (mem == (void *) -1)
        return -1;
    return 0;
}

// Set the protection for a memory mapping, and verify if necessary.
static int
protectverify(struct LFIBox *box, uintptr_t base, size_t size, int prot)
{
    if ((prot & LFI_PROT_EXEC) == 0) {
        return mprotect((void *) base, size, host_prot(prot));
    } else if ((prot & LFI_PROT_EXEC) && (prot & LFI_PROT_WRITE)) {
        LOG(box->engine, "error: attempted to set memory as WX");
        return -1;
    }

    assert((prot & LFI_PROT_EXEC));

    // Mark the memory as read-only so we can verify it without someone else
    // writing to it at the same time.
    mprotect((void *) base, size, PROT_READ);

    // Verify.
    if (!lfiv_verify(&box->engine->verifier, (char *) base, size, base))
        return -1;
    // Mark the memory as requested.
    return mprotect((void *) base, size, host_prot(prot));
}

// Create a new memory mapping, and verify if necessary.
static int
mapverify(struct LFIBox *box, uintptr_t start, size_t size, int prot, int flags,
    int fd, off_t off)
{
    if ((prot & LFI_PROT_EXEC) == 0) {
        return mapmem(start, size, prot, flags, fd, off);
    } else if ((prot & LFI_PROT_EXEC) && (prot & LFI_PROT_WRITE)) {
        LOG(box->engine, "error: attempted to map WX memory");
        return -1;
    }

    assert((prot & LFI_PROT_EXEC));

    // Map memory as readable so that we can verify it.
    int r;
    if ((r = mapmem(start, size, LFI_PROT_READ, flags, fd, off)) < 0)
        return r;
    // Verify.
    if (!lfiv_verify(&box->engine->verifier, (char *) start, size, start))
        return -1;
    // Mark the memory as requested.
    return mprotect((void *) start, size, host_prot(prot));
}

EXPORT lfiptr
lfi_box_mapany(struct LFIBox *box, size_t size, int prot, int flags, int fd,
    off_t off)
{
    uintptr_t addr = mm_mapany(&box->mm, size, prot, flags, fd, off);
    if (addr == (uintptr_t) -1)
        return (lfiptr) -1;
    int r = mapverify(box, addr, size, prot, flags, fd, off);
    if (r < 0) {
        mm_unmap(&box->mm, addr, size);
        return (lfiptr) -1;
    }
    return p2l(box, addr);
}

static void
cbunmap(uint64_t start, size_t len, MMInfo info, void *udata)
{
    (void) udata, (void) info;
    void *p = mmap((void *) start, len, PROT_NONE,
        MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED, -1, 0);
    assert(p == (void *) start);
}

EXPORT lfiptr
lfi_box_mapat(struct LFIBox *box, lfiptr addr, size_t size, int prot, int flags,
    int fd, off_t off)
{
    assert(l2p(box, addr) >= box->min && l2p(box, addr) + size <= box->max);

    uintptr_t m_addr = mm_mapat_cb(&box->mm, l2p(box, addr), size, prot, flags,
        fd, off, cbunmap, NULL);
    if (m_addr == (uintptr_t) -1)
        return (lfiptr) -1;
    int r = mapverify(box, m_addr, size, prot, flags, fd, off);
    if (r < 0) {
        mm_unmap(&box->mm, m_addr, size);
        return (lfiptr) -1;
    }
    return p2l(box, m_addr);
}

EXPORT int
lfi_box_mprotect(struct LFIBox *box, lfiptr addr, size_t size, int prot)
{
    assert(l2p(box, addr) >= box->min && l2p(box, addr) + size <= box->max);

    // TODO: we are not registering this change of protection with libmmap,
    // meaning we could get incorrect results if we try to use mm_query.
    // Currently not an issue.
    return protectverify(box, l2p(box, addr), size, prot);
}

EXPORT int
lfi_box_munmap(struct LFIBox *box, lfiptr addr, size_t size)
{
    if (l2p(box, addr) >= box->min && l2p(box, addr) + size < box->max)
        return mm_unmap_cb(&box->mm, l2p(box, addr), size, cbunmap, NULL);
    return -1;
}

EXPORT void
lfi_box_free(struct LFIBox *box)
{
    void *p = mmap((void *) box->base, box->size, PROT_NONE,
        MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED, -1, 0);
    assert(p == (void *) box->base);
    boxmap_rmspace(box->engine->bm, box->base, box->size);
    free(box);
}
