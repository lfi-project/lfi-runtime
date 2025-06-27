#pragma once

#include "arch_callback.h"
#include "boxmap.h"
#include "lfi_arch.h"
#include "lfi_core.h"
#include "lfiv.h"
#include "log.h"
#include "mmap.h"

#include <signal.h>

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
    // Address space information.
    uintptr_t base;
    size_t size;
    lfiptr min;
    lfiptr max;

    // Memory mapper object.
    MMAddrSpace mm;

    // Pointer to the page at the start of the sandbox holding runtime call
    // entrypoints.
    struct Sys *sys;

    // Address of return function in this sandbox.
    lfiptr retaddr;

    struct CallbackInfo cbinfo;

    void *callbacks[MAXCALLBACKS];

    struct LFIEngine *engine;

    void *userdata;
};

struct Sys {
    uintptr_t rtcalls[256];
};

struct LFIContext {
    // Registers, including saved host stack and thread pointers.
    struct LFIRegs regs;

    // User-provided data pointer.
    void *userdata;

    // Sandbox that this context is associated with.
    struct LFIBox *box;
};

extern _Thread_local int lfi_error;
extern _Thread_local char *lfi_error_desc;

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
box_footprint(size_t boxsize)
{
    if (boxsize != gb(4)) {
        LOG_(
            "warning: box_footprint does not properly support non-4GiB sandboxes");
        return boxsize;
    }
#if defined(LFI_ARCH_ARM64)
    return gb(4);
#elif defined(LFI_ARCH_X64)
    return gb(44);
#else
#error "invalid architecture"
#endif
}
