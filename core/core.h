#pragma once

#include "arch_callback.h"
#include "boxmap.h"
#include "lfi_arch.h"
#include "lfi_core.h"
#include "lfiv.h"
#include "log.h"
#include "mmap.h"

#include <signal.h>
#include <threads.h>

#define EXPORT __attribute__((visibility("default")))

struct LFIEngine {
    struct BoxMap *bm;
    struct LFIOptions opts;
    struct LFIVerifier verifier;

    void (*sys_handler)(struct LFIContext *ctx);
    struct LFIContext *(*clone_cb)(struct LFIBox *box);

    size_t guardsize;
    stack_t altstack;
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
    struct MMAddrSpace mm;

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
};

struct Sys {
    uintptr_t rtcalls[32];
};

struct LFIContext {
    // Registers of sandbox and associated host thread (stack, thread pointer)
    // are stored here.
    struct LFIRegs regs;

    // Set to 1 if a lfi_ret_end should be automatically called when returning
    // from a callback to abort further sandbox execution of the callback.
    uint64_t abort_callback;
    // Set to 1 if the most recent callback was aborted.
    uint64_t abort_status;

#ifdef CTXREG
    // Context register storage. The first slot holds a pointer to this
    // LFIContext, and remaining slots are available for thread-local data
    // (e.g., thread pointer).
    uint64_t ctxreg[64];

    // Base pointer for the shadow call stack mapping (includes guard pages).
    void *scs_base;
    // Upper bound for the SCS pointer.
    void *scs_limit;
    // Total size of the SCS mapping (guard + stack + guard).
    size_t scs_total;

    // SCS save stack for setjmp/longjmp and exception unwinding support.
    // scs_save_sp points to the next free slot; entries below it are valid.
    uint64_t scs_save_stack[32];
    uint64_t *scs_save_sp;
#endif

    // User-provided data pointer -- tracks per-sandbox context for Linux
    // runtime.
    void *userdata;

    // Sandbox that this context is associated with.
    struct LFIBox *box;
};

extern thread_local int lfi_error;
extern thread_local char *lfi_error_desc;

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

// Return the total amount of virtual address space needed for a sandbox of a
// certain size. This depends on the architecture because it is effectively
// reserving a guard region beyond the sandbox.
static inline size_t
box_footprint(size_t boxsize, struct LFIOptions opts)
{
    if (boxsize != gb(4)) {
        LOG_(
            "warning: box_footprint does not properly support non-4GiB sandboxes");
        return boxsize;
    }
#if defined(LFI_ARCH_ARM64)
    return gb(8);
#elif defined(LFI_ARCH_X64)

#ifdef HAVE_PKU
    return gb(4);
#else
    // TODO: verify that we can safely reduce this to gb(4) if Segue is enabled.
    return gb(44);
#endif

#elif defined(LFI_ARCH_RISCV64)
    return gb(4);
#else
#error "invalid architecture"
#endif
}
