#pragma once

#include "boxmap.h"
#include "lfi_arch.h"
#include "lfi_core.h"
#include "lfiv.h"
#include "log.h"
#include "mmap.h"

#define EXPORT __attribute__((visibility("default")))

struct LFIEngine {
    struct BoxMap *bm;
    struct LFIOptions opts;
    struct LFIVerifier verifier;

    size_t guardsize;
};

struct LFIBox {
    // Address space information.
    uintptr_t base;
    size_t size;
    lfiptr min;
    lfiptr max;

    // Memory mapper object.
    MMAddrSpace mm;

    struct LFIEngine *engine;
};

struct Sys {
    uintptr_t rtcalls[256];
};

struct LFIContext {
    // Registers, including saved host stack and thread pointers.
    struct LFIRegs regs;

    // User-provided data pointer.
    void *ctxp;

    // Pointer to the page at the start of the sandbox holding runtime call
    // entrypoints.
    struct Sys *sys;

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
