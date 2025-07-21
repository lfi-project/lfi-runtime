#ifdef HAVE_PKU
#define _GNU_SOURCE
#endif

#include "arch_asm.h"
#include "core.h"
#include "lfi_core.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
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
lfi_syscall_entry(void) asm("lfi_syscall_entry");
extern void
lfi_get_tp(void) asm("lfi_get_tp");
extern void
lfi_set_tp(void) asm("lfi_set_tp");
extern void
lfi_ret(void) asm("lfi_ret");

static int
protectmem(void *start, size_t size, int prot, int pkey)
{
#ifdef HAVE_PKU
    return pkey_mprotect(start, size, prot, pkey);
#else
    return mprotect(start, size, prot);
#endif
}

// Initialize the sys page (at the beginning of the sandbox) to contain the
// runtime call entrypoints.
static void
syssetup(struct LFIBox *box)
{
    // Map read/write.
    box->sys = mmap((void *) box->base, box->engine->opts.pagesize,
        PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    assert(box->sys == (void *) box->base);

    box->sys->rtcalls[0] = (uintptr_t) &lfi_syscall_entry;
    box->sys->rtcalls[1] = (uintptr_t) &lfi_get_tp;
    box->sys->rtcalls[2] = (uintptr_t) &lfi_set_tp;
    box->sys->rtcalls[3] = (uintptr_t) &lfi_ret;

    // Map read-only.
    int r = protectmem((void *) box->base, box->engine->opts.pagesize,
        PROT_READ, box->pkey);
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
    uintptr_t base = boxmap_addspace(engine->bm,
        box_footprint(size, engine->opts));
    if (base == 0) {
        lfi_error = LFI_ERR_BOXMAP;
        goto err1;
    }

    int pkey = 0;
#ifdef HAVE_PKU
    pkey = pkey_alloc(0, 0);
    if (pkey == -1) {
        if (errno == ENOSPC)
            LOG(engine, "could not allocate pkey: no more keys available");
        else
            LOG(engine, "could not allocate pkey: invalid argument");
        lfi_error = LFI_ERR_PKU;
        return NULL;
    }
    assert(pkey != 0);
    int r = pkey_mprotect((void *) base, size, PROT_NONE, pkey);
    assert(r == 0);
#endif

    *box = (struct LFIBox) {
        .pkey = pkey,
        .base = base,
        .size = size,
        .engine = engine,
        .min = base + engine->guardsize + engine->opts.pagesize,
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
    boxmap_rmspace(engine->bm, base, box_footprint(size, engine->opts));
err1:
    free(box);
    return NULL;
}

EXPORT struct LFIEngine *
lfi_box_engine(struct LFIBox *box)
{
    return box->engine;
}

EXPORT void
lfi_box_setdata(struct LFIBox *box, void *userdata)
{
    box->userdata = userdata;
}

EXPORT void *
lfi_box_data(struct LFIBox *box)
{
    return box->userdata;
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
        ((flags & LFI_MAP_FIXED) ? MAP_FIXED : 0) |
        ((flags & LFI_MAP_SHARED) ? MAP_SHARED : 0);
}

// Create a fixed memory mapping with the LFI_MAP/LFI_PROT flags.
static int
mapmem(uintptr_t start, size_t size, int prot, int flags, int fd, off_t off,
    int pkey)
{
    void *mem = mmap((void *) start, size, host_prot(prot),
        host_flags(flags) | MAP_FIXED, fd, off);
    if (mem == (void *) -1)
        return -1;

#ifdef HAVE_PKU
    if (pkey_mprotect((void *) start, size, host_prot(prot), pkey) == -1) {
        munmap((void *) start, size);
        return -1;
    }
#endif
    return 0;
}

// Verify a region given that it is marked with certain protections.
static bool
verify(struct LFIBox *box, uintptr_t base, size_t size, int prot)
{
    bool no_verify = box->engine->opts.no_verify;
    bool allow_wx = box->engine->opts.allow_wx && no_verify;
    bool w = (prot & LFI_PROT_WRITE) != 0;
    bool x = (prot & LFI_PROT_EXEC) != 0;

    // Allow mprotect if mapping is not executable, or verification is disabled
    // and it's not WX, or WX is allowed (and verification is disabled).
    if (!x || (no_verify && !(w && x)) || allow_wx) {
        return true;
    } else if (w && x) {
        LOG(box->engine, "error: region is WX");
        return false;
    }

    assert(x && !w);

    // Verify.
    if (!lfiv_verify(&box->engine->verifier, (char *) base, size, base)) {
        LOG(box->engine, "verification failed");
        return false;
    }

    return true;
}

// Set the protection for a memory mapping, and verify if necessary.
static int
protectverify(struct LFIBox *box, uintptr_t base, size_t size, int prot,
    bool no_verify)
{
    no_verify = no_verify || box->engine->opts.no_verify;
    bool allow_wx = box->engine->opts.allow_wx && no_verify;
    bool w = (prot & LFI_PROT_WRITE) != 0;
    bool x = (prot & LFI_PROT_EXEC) != 0;

    // Allow mprotect if mapping is not executable, or verification is disabled
    // and it's not WX, or WX is allowed (and verification is disabled).
    if (!x || (no_verify && !(w && x)) || allow_wx) {
        return protectmem((void *) base, size, host_prot(prot), box->pkey);
    } else if (w && x) {
        LOG(box->engine, "error: attempted to set memory as WX");
        return -1;
    }

    assert(x);

    // Mark the memory as read-only so we can verify it without someone else
    // writing to it at the same time.
    protectmem((void *) base, size, PROT_READ, box->pkey);

    // Verify.
    if (!lfiv_verify(&box->engine->verifier, (char *) base, size, base)) {
        LOG(box->engine, "verification failed");
        return -1;
    }
    // Mark the memory as requested.
    return protectmem((void *) base, size, host_prot(prot), box->pkey);
}

// Create a new memory mapping, and verify if necessary.
static int
mapverify(struct LFIBox *box, uintptr_t start, size_t size, int prot, int flags,
    int fd, off_t off, bool no_verify)
{
    no_verify = no_verify || box->engine->opts.no_verify;
    bool allow_wx = box->engine->opts.allow_wx && no_verify;
    bool w = (prot & LFI_PROT_WRITE) != 0;
    bool x = (prot & LFI_PROT_EXEC) != 0;

    // Allow mprotect if mapping is not executable, or verification is disabled
    // and it's not WX, or WX is allowed (and verification is disabled).
    if (!x || (no_verify && !(w && x)) || allow_wx) {
        return mapmem(start, size, prot, flags, fd, off, box->pkey);
    } else if (w && x) {
        LOG(box->engine, "error: attempted to map WX memory");
        return -1;
    }

    assert(x);

    // Map memory as readable so that we can verify it.
    int r;
    if ((r = mapmem(start, size, LFI_PROT_READ, flags, fd, off, box->pkey)) < 0)
        return r;
    // Verify.
    if (!lfiv_verify(&box->engine->verifier, (char *) start, size, start))
        return -1;
    // Mark the memory as requested.
    return protectmem((void *) start, size, host_prot(prot), box->pkey);
}

static lfiptr
box_mapany(struct LFIBox *box, size_t size, int prot, int flags, int fd,
    off_t off, bool no_verify)
{
    uintptr_t addr = mm_mapany(&box->mm, size, prot, flags, fd, off);
    if (addr == (uintptr_t) -1)
        return (lfiptr) -1;
    int r = mapverify(box, addr, size, prot, flags, fd, off, no_verify);
    if (r < 0) {
        mm_unmap(&box->mm, addr, size);
        return (lfiptr) -1;
    }
    return p2l(box, addr);
}

EXPORT lfiptr
lfi_box_mapany_noverify(struct LFIBox *box, size_t size, int prot, int flags,
    int fd, off_t off)
{
    return box_mapany(box, size, prot, flags, fd, off, true);
}

EXPORT lfiptr
lfi_box_mapany(struct LFIBox *box, size_t size, int prot, int flags, int fd,
    off_t off)
{
    return box_mapany(box, size, prot, flags, fd, off, false);
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
    int r = mapverify(box, m_addr, size, prot, flags, fd, off, false);
    if (r < 0) {
        mm_unmap(&box->mm, m_addr, size);
        return (lfiptr) -1;
    }
    return p2l(box, m_addr);
}

EXPORT lfiptr
lfi_box_mapat_register(struct LFIBox *box, lfiptr addr, size_t size, int prot,
    int flags, int fd, off_t off)
{
    assert(l2p(box, addr) >= box->min && l2p(box, addr) + size <= box->max);

    uintptr_t m_addr = mm_mapat_cb(&box->mm, l2p(box, addr), size, prot, flags,
        fd, off, cbunmap, NULL);
    if (m_addr == (uintptr_t) -1)
        return (lfiptr) -1;
    int r = verify(box, m_addr, size, prot);
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
    return protectverify(box, l2p(box, addr), size, prot, false);
}

EXPORT int
lfi_box_mprotect_noverify(struct LFIBox *box, lfiptr addr, size_t size,
    int prot)
{
    // Same todo as above.
    return protectverify(box, l2p(box, addr), size, prot, true);
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
    boxmap_rmspace(box->engine->bm, box->base,
        box_footprint(box->size, box->engine->opts));
#ifdef HAVE_PKU
    if (box->pkey != 0)
        pkey_free(box->pkey);
#endif
    free(box);
}

EXPORT bool
lfi_box_ptrvalid(struct LFIBox *box, lfiptr addr)
{
    lfiptr lp = l2p(box, addr);
    return lp >= box->min && lp < box->max;
}

EXPORT bool
lfi_box_bufvalid(struct LFIBox *box, lfiptr addr, size_t size)
{
    lfiptr lp = l2p(box, addr);
    return lp >= box->min && lp + size <= box->max;
}

EXPORT void *
lfi_box_copyfm(struct LFIBox *box, void *dst, lfiptr src, size_t size)
{
    assert(lfi_box_bufvalid(box, src, size));
    memcpy(dst, (void *) l2p(box, src), size);
    return dst;
}

EXPORT lfiptr
lfi_box_copyto(struct LFIBox *box, lfiptr dst, const void *src, size_t size)
{
    assert(lfi_box_bufvalid(box, dst, size));
    memcpy((void *) l2p(box, dst), src, size);
    return dst;
}

EXPORT uintptr_t
lfi_box_l2p(struct LFIBox *box, lfiptr l)
{
    return l2p(box, l);
}

EXPORT uintptr_t
lfi_box_p2l(struct LFIBox *box, uintptr_t p)
{
    return p2l(box, p);
}

#if defined(LFI_ARCH_ARM64)

static uint8_t ret[] = {
    0x7e, 0x0f, 0x40, 0xf9, // ldr x30, [x27, #24]
    0xc0, 0x03, 0x3f, 0xd6, // blr x30
};

#elif defined(LFI_ARCH_X64)

static uint8_t ret[] = {
    0x4c, 0x8d, 0x1d, 0x04, 0x00, 0x00, 0x00, // lea 0x4(%rip), %r11
    0x41, 0xff, 0x66, 0x18,                   // jmp *0x18(%r14)
};

#elif defined(LFI_ARCH_RISCV64)

static uint8_t ret[] = {
    0x83, 0xb0, 0x8a, 0x01, // la ra, 24(x21)
    0xe7, 0x80, 0x00, 0x00, // jalr ra
};

#else

#error "architecture not supported"

#endif

EXPORT void
lfi_box_init_ret(struct LFIBox *box)
{
    size_t pagesize = box->engine->opts.pagesize;
    lfiptr p = lfi_box_mapany(box, pagesize, LFI_PROT_READ | LFI_PROT_WRITE,
        LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE, -1, 0);
    assert(p != (lfiptr) -1);
    assert(lfi_box_ptrvalid(box, p));

#if defined(LFI_ARCH_X64)
    // Set all bytes to trap instructions, since 0 does not pass verification.
    memset((void *) lfi_box_l2p(box, p), 0xcc, pagesize);
#endif

    lfiptr p_ret = lfi_box_copyto(box, p, ret, sizeof(ret));

    int r = lfi_box_mprotect(box, p, pagesize, LFI_PROT_READ | LFI_PROT_EXEC);
    assert(r == 0);

    box->retaddr = p_ret;
}
