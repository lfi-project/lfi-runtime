#ifdef HAVE_PKU
#define _GNU_SOURCE
#endif

#include "arch_asm.h"
#include "core.h"
#include "lfi_core.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#if defined(HAVE_GETRANDOM)
#include <sys/random.h>
#endif

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
lfi_syscall_entry(void) __asm__("lfi_syscall_entry");
#ifndef SYS_MINIMAL
extern void
lfi_get_tp(void) __asm__("lfi_get_tp");
extern void
lfi_set_tp(void) __asm__("lfi_set_tp");
#endif
extern void
lfi_ret(void) __asm__("lfi_ret");
extern void
lfi_rtcall_bad(void) __asm__("lfi_rtcall_bad");

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
// runtime call entrypoints. Returns false if the syspage could not be mapped.
static bool
syssetup(struct LFIBox *box)
{
    // The sys page exists at the page before the start of the sandbox. Only
    // the last 32 slots in the page are valid pointers to runtime call
    // entrypoints. We start by mapping it read/write, insert the runtime calls
    // we support, and then mark it read-only. If sandboxes are densely packed,
    // this page also exists inside the guard region of the adjacent sandbox.

    // Map read/write.
    size_t pagesize = box->engine->opts.pagesize;
    box->sys_page = mmap((void *) (box->base - pagesize), pagesize,
        PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (box->sys_page != (void *) (box->base - pagesize)) {
        if (box->sys_page != MAP_FAILED)
            munmap(box->sys_page, pagesize);
        box->sys_page = NULL;
        return false;
    }

    box->sys = (struct Sys *) ((char *) box->sys_page + pagesize -
        sizeof(struct Sys));

    size_t n = sizeof(box->sys->rtcalls) / sizeof(box->sys->rtcalls[0]);
    for (size_t i = 0; i < n; i++)
        box->sys->rtcalls[i] = (uintptr_t) &lfi_rtcall_bad;
    box->sys->rtcalls[n - 1] = (uintptr_t) &lfi_syscall_entry;
#ifndef SYS_MINIMAL
    box->sys->rtcalls[n - 2] = (uintptr_t) &lfi_get_tp;
    box->sys->rtcalls[n - 3] = (uintptr_t) &lfi_set_tp;
#endif
    box->sys->rtcalls[n - 4] = (uintptr_t) &lfi_ret;

    // Map read-only.
    int r = protectmem(box->sys_page, box->engine->opts.pagesize, PROT_READ,
        box->pkey);
    if (r != 0) {
        munmap(box->sys_page, pagesize);
        box->sys_page = NULL;
        return false;
    }
    return true;
}

// Generate a random seed for address space layout randomization.
static uint64_t
aslr_seed(void)
{
    uint64_t seed;
#if defined(HAVE_SYS_GETRANDOM)
    if (syscall(SYS_getrandom, &seed, sizeof(seed), 0) == (long) sizeof(seed))
        return seed;
#elif defined(HAVE_GETRANDOM)
    if (getrandom(&seed, sizeof(seed), 0) == sizeof(seed))
        return seed;
#elif defined(HAVE_ARC4RANDOM_BUF)
    arc4random_buf(&seed, sizeof(seed));
    return seed;
#elif defined(HAVE_GETENTROPY)
    if (getentropy(&seed, sizeof(seed)) == 0)
        return seed;
#endif
    int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (fd >= 0) {
        ssize_t n = read(fd, &seed, sizeof(seed));
        close(fd);
        if (n == (ssize_t) sizeof(seed))
            return seed;
    }
    // Weak fallback: not unpredictable, but better than nothing.
    struct timespec ts = { 0 };
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((uint64_t) ts.tv_sec << 32) ^ (uint64_t) ts.tv_nsec ^
        (uint64_t) (uintptr_t) &seed;
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
    uintptr_t base = boxmap_addspace(engine->bm, box_footprint(size));
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
        goto err2;
    }
    assert(pkey != 0);
    int r = pkey_mprotect((void *) base, size, PROT_NONE, pkey);
    assert(r == 0);
#endif

    size_t min_off = BOX_INTERNAL_GUARD > engine->opts.pagesize
        ? BOX_INTERNAL_GUARD
        : engine->opts.pagesize;
    *box = (struct LFIBox) {
        .pkey = pkey,
        .base = base,
        .size = size,
        .engine = engine,
        .min = base + min_off,
        .max = base + size - BOX_INTERNAL_GUARD,
        .max_exec = base + size - BOX_INTERNAL_GUARD,
    };
    if (!syssetup(box)) {
        lfi_error = LFI_ERR_MMAP;
#ifdef HAVE_PKU
        pkey_free(pkey);
#endif
        goto err2;
    }

    if (pthread_mutex_init(&box->lk, NULL) != 0) {
        lfi_error = LFI_ERR_MMAP;
#ifdef HAVE_PKU
        pkey_free(pkey);
#endif
        goto err2;
    }

    box->mm = mmap_create(box->min, box->max - box->min,
        engine->opts.pagesize);
    if (!box->mm) {
        lfi_error = LFI_ERR_MMAP;
#ifdef HAVE_PKU
        pkey_free(pkey);
#endif
        goto err3;
    }
    if (!engine->opts.no_aslr)
        mmap_enable_aslr(box->mm, aslr_seed());

    return box;

err3:
    pthread_mutex_destroy(&box->lk);
err2:
    boxmap_rmspace(engine->bm, base, box_footprint(size));
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
        .max_exec = box->max_exec,
    };
}

// These functions convert from the LFI mapping flags to the underlying host's
// mmap flags. This allows liblfi to export a more platform-independent API.
static int
host_prot(int prot)
{
    int p = ((prot & LFI_PROT_READ) ? PROT_READ : 0) |
        ((prot & LFI_PROT_WRITE) ? PROT_WRITE : 0) |
        ((prot & LFI_PROT_EXEC) ? PROT_EXEC : 0);
#if defined(LFI_ARCH_ARM64) && defined(__linux__)
    if (prot & LFI_PROT_BTI)
        p |= PROT_BTI;
#endif
    return p;
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

static void
cbunmap(uintptr_t start, size_t len, struct MMapInfo info, void *udata)
{
    (void) udata, (void) info;
    void *p = mmap((void *) start, len, PROT_NONE,
        MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED, -1, 0);
    if (p == (void *) start)
        return;
    if (mprotect((void *) start, len, PROT_NONE) != 0)
        ERROR("failed to clear sandbox mapping %lx-%lx: %s",
            (unsigned long) start, (unsigned long) (start + len),
            strerror(errno));
}

lfiptr
box_mapany_locked(struct LFIBox *box, size_t size, int prot, int flags, int fd,
    off_t off, bool no_verify)
{
    uintptr_t addr = mmap_map_any(box->mm, 0, size, prot, flags, fd, off);
    if (addr == (uintptr_t) -1)
        return (lfiptr) -1;
    int r = mapverify(box, addr, size, prot, flags, fd, off, no_verify);
    if (r < 0) {
        mmap_unmap(box->mm, addr, size, cbunmap, NULL);
        return (lfiptr) -1;
    }
    return p2l(box, addr);
}

EXPORT lfiptr
lfi_box_mapany_noverify(struct LFIBox *box, size_t size, int prot, int flags,
    int fd, off_t off)
{
    BOX_LOCK(box, lk);
    return box_mapany_locked(box, size, prot, flags, fd, off, true);
}

EXPORT lfiptr
lfi_box_mapany(struct LFIBox *box, size_t size, int prot, int flags, int fd,
    off_t off)
{
    BOX_LOCK(box, lk);
    return box_mapany_locked(box, size, prot, flags, fd, off, false);
}

lfiptr
box_mapat_locked(struct LFIBox *box, lfiptr addr, size_t size, int prot,
    int flags, int fd, off_t off)
{
    if (!lfi_box_bufvalid(box, addr, size))
        return (lfiptr) -1;

    uintptr_t m_addr = mmap_map_at(box->mm, l2p(box, addr), size, prot, flags,
        fd, off, cbunmap, NULL);
    if (m_addr == (uintptr_t) -1)
        return (lfiptr) -1;
    int r = mapverify(box, m_addr, size, prot, flags, fd, off, false);
    if (r < 0) {
        mmap_unmap(box->mm, m_addr, size, cbunmap, NULL);
        return (lfiptr) -1;
    }
    return p2l(box, m_addr);
}

EXPORT lfiptr
lfi_box_mapat(struct LFIBox *box, lfiptr addr, size_t size, int prot, int flags,
    int fd, off_t off)
{
    BOX_LOCK(box, lk);
    return box_mapat_locked(box, addr, size, prot, flags, fd, off);
}

EXPORT lfiptr
lfi_box_mapat_register(struct LFIBox *box, lfiptr addr, size_t size, int prot,
    int flags, int fd, off_t off)
{
    BOX_LOCK(box, lk);

    if (!lfi_box_bufvalid(box, addr, size))
        return (lfiptr) -1;

    uintptr_t m_addr = mmap_map_at(box->mm, l2p(box, addr), size, prot, flags,
        fd, off, cbunmap, NULL);
    if (m_addr == (uintptr_t) -1)
        return (lfiptr) -1;
    bool r = verify(box, m_addr, size, prot);
    if (!r) {
        mmap_unmap(box->mm, m_addr, size, NULL, NULL);
        return (lfiptr) -1;
    }
    return p2l(box, m_addr);
}

int
box_mprotect_locked(struct LFIBox *box, lfiptr addr, size_t size, int prot,
    bool no_verify)
{
    size_t pagesize = box->engine->opts.pagesize;
    if (size == 0 || addr % pagesize != 0 || size % pagesize != 0)
        return -1;
    if (!lfi_box_bufvalid(box, addr, size))
        return -1;

    int r = protectverify(box, l2p(box, addr), size, prot, no_verify);
    if (r < 0)
        return r;
    return mmap_protect(box->mm, l2p(box, addr), size, prot, NULL, NULL);
}

EXPORT int
lfi_box_mprotect(struct LFIBox *box, lfiptr addr, size_t size, int prot)
{
    BOX_LOCK(box, lk);
    return box_mprotect_locked(box, addr, size, prot, false);
}

EXPORT int
lfi_box_mprotect_noverify(struct LFIBox *box, lfiptr addr, size_t size,
    int prot)
{
    BOX_LOCK(box, lk);
    return box_mprotect_locked(box, addr, size, prot, true);
}

EXPORT bool
lfi_box_mquery(struct LFIBox *box, lfiptr addr, struct LFIMapInfo *info)
{
    BOX_LOCK(box, lk);
    struct MMapInfo mminfo;
    if (!mmap_query_page(box->mm, l2p(box, addr), &mminfo))
        return false;
    *info = (struct LFIMapInfo) {
        .prot = mminfo.prot,
        .flags = mminfo.flags,
        .fd = mminfo.fd,
        .offset = mminfo.offset,
    };
    return true;
}

int
box_munmap_locked(struct LFIBox *box, lfiptr addr, size_t size)
{
    size_t pagesize = box->engine->opts.pagesize;
    if (!lfi_box_bufvalid(box, addr, size) || size == 0 ||
        addr % pagesize != 0)
        return -EINVAL;

    uintptr_t p = l2p(box, addr);
    size_t len = (size + pagesize - 1) & ~(pagesize - 1);
    void *m = mmap((void *) p, len, PROT_NONE,
        MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED, -1, 0);
    if (m != (void *) p)
        return -ENOMEM;

    if (mmap_unmap(box->mm, p, size, NULL, NULL) != MMAP_OK)
        return -EINVAL;
    return 0;
}

EXPORT int
lfi_box_munmap(struct LFIBox *box, lfiptr addr, size_t size)
{
    BOX_LOCK(box, lk);
    return box_munmap_locked(box, addr, size);
}

EXPORT void
lfi_box_free(struct LFIBox *box)
{
    size_t pagesize = box->engine->opts.pagesize;
    void *p = mmap((void *) (box->base - pagesize), box->size + pagesize,
        PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED, -1, 0);
    assert(p == (void *) (box->base - pagesize));
    lfi_box_cb_free(box);
    boxmap_rmspace(box->engine->bm, box->base, box_footprint(box->size));
#ifdef HAVE_PKU
    if (box->pkey != 0)
        pkey_free(box->pkey);
#endif
    mmap_destroy(box->mm);
    pthread_mutex_destroy(&box->lk);
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
    return lp >= box->min && lp <= box->max && size <= box->max - lp;
}

EXPORT size_t
lfi_box_strnlen(struct LFIBox *box, lfiptr addr, size_t max)
{
    if (!lfi_box_ptrvalid(box, addr))
        return SIZE_MAX;
    uintptr_t lp = l2p(box, addr);
    size_t room = box->max - lp;
    size_t limit = max < room ? max : room;
    size_t n = strnlen((const char *) lp, limit);
    if (n == limit)
        return SIZE_MAX;
    return n;
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
    0x7e, 0x03, 0x5e, 0xf8, // ldr x30, [x27, #-32]
    0xc0, 0x03, 0x3f, 0xd6, // blr x30
};

#elif defined(LFI_ARCH_X64)

static uint8_t ret[] = {
    0x4c, 0x8d, 0x1d, 0x04, 0x00, 0x00, 0x00, // lea 0x4(%rip), %r11
    0x41, 0xff, 0x66, 0xe0,                   // jmp *-0x20(%r14)
};

#elif defined(LFI_ARCH_RISCV64)

static uint8_t ret[] = {
    0x83, 0xb0, 0x0a, 0xfe, // la ra, -32(x21)
    0xe7, 0x80, 0x00, 0x00, // jalr ra
};

#else

#error "architecture not supported"

#endif

EXPORT bool
lfi_box_init_ret(struct LFIBox *box)
{
    BOX_LOCK(box, lk);

    size_t pagesize = box->engine->opts.pagesize;
    lfiptr p = box_mapany_locked(box, pagesize, LFI_PROT_READ | LFI_PROT_WRITE,
        LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE, -1, 0, false);
    if (p == (lfiptr) -1 || !lfi_box_ptrvalid(box, p))
        return false;

#if defined(LFI_ARCH_X64)
    // Set all bytes to trap instructions, since 0 does not pass verification.
    memset((void *) lfi_box_l2p(box, p), 0xcc, pagesize);
#endif

    lfiptr p_ret = lfi_box_copyto(box, p, ret, sizeof(ret));

    int r = box_mprotect_locked(box, p, pagesize,
        LFI_PROT_READ | LFI_PROT_EXEC, false);
    if (r != 0)
        return false;

    box->retaddr = p_ret;
    return true;
}

EXPORT void
lfi_box_register_ret(struct LFIBox *box, lfiptr retaddr)
{
    assert(lfi_box_ptrvalid(box, retaddr));
    box->retaddr = retaddr;
}

EXPORT bool
lfi_box_cbinit(struct LFIBox *box)
{
    LOG(box->engine,
        "warning: lfi_box_cbinit is deprecated and is a no-op, please remove");
    return true;
}
