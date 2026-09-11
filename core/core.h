#pragma once

#include "arch_callback.h"
#include "boxmap.h"
#include "lfi_arch.h"
#include "lfi_core.h"
#include "lfiv.h"
#include "log.h"
#include "mmap/mmap_c.h"

#include <assert.h>
#include <pthread.h>

#ifndef thread_local
#define thread_local _Thread_local
#endif

#define EXPORT __attribute__((visibility("default")))

struct LFIEngine {
    struct BoxMap *bm;
    struct LFIOptions opts;
    struct LFIVerifier verifier;

    void (*sys_handler)(struct LFIContext *ctx);
    struct LFIContext *(*clone_cb)(struct LFIBox *box);
};

struct LFIBox {
    // Non-zero protection key if pku is enabled (zero if disabled).
    int pkey;

    // Address space information.
    uintptr_t base;
    size_t size;
    lfiptr min;      // Smallest valid address (up to guard region)
    lfiptr max;      // Largest valid address (up to guard region)
    lfiptr max_exec; // Largest valid executable address

    // Memory mapper object from libmmap.
    struct MMapAddrSpace *mm;

    // Pointer to the page at the start of the sandbox holding runtime call
    // entrypoints.
    void *sys_page;
    struct Sys *sys;

    // Address of return function in this sandbox.
    lfiptr retaddr;

    struct CallbackInfo cbinfo;

    void *callbacks[MAXCALLBACKS];

    struct LFIEngine *engine;

    void *userdata;

    // Guards every mutation of box->mm and of the callback table.
    pthread_mutex_t lk;
};

static inline struct LFIBox *
box_lock(struct LFIBox *box)
{
    int r = pthread_mutex_lock(&box->lk);
    assert(r == 0);
    (void) r;
    return box;
}

static inline void
box_unlock(struct LFIBox *box)
{
    int r = pthread_mutex_unlock(&box->lk);
    assert(r == 0);
    (void) r;
}

static inline void
box_unlock_deferred(struct LFIBox **box)
{
    box_unlock(*box);
}

// Acquire box->lk for the rest of the enclosing scope.
#define BOX_LOCK(b, name)                                                \
    struct LFIBox *name __attribute__((cleanup(box_unlock_deferred))) =  \
        box_lock(b);                                                     \
    (void) name;

lfiptr
box_mapany_locked(struct LFIBox *box, size_t size, int prot, int flags, int fd,
    off_t off, bool no_verify);

lfiptr
box_mapat_locked(struct LFIBox *box, lfiptr addr, size_t size, int prot,
    int flags, int fd, off_t off);

int
box_mprotect_locked(struct LFIBox *box, lfiptr addr, size_t size, int prot,
    bool no_verify);

int
box_munmap_locked(struct LFIBox *box, lfiptr addr, size_t size);

struct Sys {
    struct LFIContext *ctx;
    uintptr_t rtcalls[32];
};

void
sys_init(struct Sys *sys, struct LFIContext *ctx);

bool
sys_page_init(void *page, size_t pagesize, int pkey, struct LFIContext *ctx);

#define CTXREG_SLOTS 8

#define CTX_BLOCK_CTXREG_OFF(pagesize) (pagesize)
#define CTX_BLOCK_SIZE(pagesize)       (2 * (pagesize))

bool
ctx_block_init(void *block, size_t pagesize, int pkey, struct LFIContext *ctx);

struct LFIContext {
    // Registers of sandbox and associated host thread (stack, thread pointer)
    // are stored here.
    struct LFIRegs regs;

    // Set to 1 if a lfi_ret_end should be automatically called when returning
    // from a callback to abort further sandbox execution of the callback.
    uint64_t abort_callback;
    // Set to 1 if the most recent callback was aborted.
    uint64_t abort_status;

#ifdef SEGUE_CACHE_GS
    uintptr_t *gs_cache;
#endif

    // Context register storage, available for thread-local data. The array is
    // page-aligned and lives in the context's block, directly after the
    // read-only runtime call page whose struct Sys points back at this
    // LFIContext.
    uint64_t *ctxreg;

    // Start of the two-page block that holds the runtime call page and the
    // ctxreg array.
    void *block;

    // User-provided data pointer -- tracks per-sandbox context for Linux
    // runtime.
    void *userdata;

    // Sandbox that this context is associated with.
    struct LFIBox *box;

    // True while regs.host_sp refers to a frame pushed by lfi_ctx_entry rather
    // than one pushed by lfi_trampoline.
    bool in_ctx_run;
};

// Gives the calling host thread access to every protection key.
static inline void
pku_host_access(void)
{
#if defined(HAVE_PKU) && defined(__x86_64__)
    __asm__ volatile("wrpkru" : : "a"(0), "c"(0), "d"(0) : "memory");
#endif
}

extern thread_local int lfi_error;
extern thread_local char *lfi_error_desc;

void lfi_box_cb_free(struct LFIBox *box);

void lfi_init_sigaltstack(struct LFIEngine *engine);

static inline size_t
kb(size_t x)
{
    return x * 1024;
}

static inline size_t
mb(size_t x)
{
    return x * 1024 * 1024;
}

static inline size_t
gb(size_t x)
{
    return x * 1024 * 1024 * 1024;
}

// Guard sizes for sandbox isolation. These are configurable per-architecture.
//
// BOX_INTERNAL_GUARD: space at the end of each sandbox (within the box's
// allocated size) that is not usable by sandbox code. This reduces the
// sandbox's usable address range.
//
// BOX_EXTERNAL_GUARD: additional space allocated beyond each sandbox's size,
// increasing the per-sandbox footprint. This provides isolation between
// adjacent sandboxes when hardware masking alone is insufficient.
//
// REGION_GUARD: space reserved on each side of the entire sandbox pool region,
// protecting against accesses escaping the pool.

#if defined(LFI_ARCH_ARM64)

#define BOX_INTERNAL_GUARD kb(192)
#define BOX_EXTERNAL_GUARD 0
#define REGION_GUARD       mb(512)

#elif defined(LFI_ARCH_X64)

#define BOX_INTERNAL_GUARD 0
#ifdef HAVE_PKU
#define BOX_EXTERNAL_GUARD 0
#define REGION_GUARD       0
#else
// The verifier enforces a scale of no more than 1, so a guard region of 4GiB
// is possible in both Segue and non-Segue configurations. Segue further allows
// arbitrary scales with a 4GiB guard region, but the verifier doesn't
// currently allow it.
#define BOX_EXTERNAL_GUARD gb(4)
#define REGION_GUARD       gb(4)
#endif

#elif defined(LFI_ARCH_RISCV64)

#define BOX_INTERNAL_GUARD 0
#define BOX_EXTERNAL_GUARD 0
#define REGION_GUARD       0

#else
#error "invalid architecture"
#endif

// Return the total amount of virtual address space needed for a sandbox of a
// certain size. This is the sandbox size plus any external guard.
static inline size_t
box_footprint(size_t boxsize)
{
    return boxsize + BOX_EXTERNAL_GUARD;
}
