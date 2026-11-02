// Tests that host/sandbox transitions on arm64 keep floating-point argument
// registers and the FP control register (FPCR) intact. The sandbox code is
// verified, so this also checks that verified code can write FPCR.

#include "lfi_arch.h"
#include "lfi_core.h"
#include "test.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// FPCR set by the sandbox programs (round towards plus infinity).
#define GUEST_FPCR 0x400000
// A non-default host FPCR (flush-to-zero), different from both the sandbox's
// state and the context's default.
#define HOST_FPCR 0x1000000

// Each program gets its own 32-byte slot.
#define SLOT 8

static const uint32_t progs[][SLOT] = {
    // 0: return (d0 passes through as the return value)
    { 0xd65f03c0 }, // ret
    // 1: return d7
    {
        0x1e6040e0, // fmov d0, d7
        0xd65f03c0, // ret
    },
    // 2: set FPCR, then return
    {
        0xd2a00809, // mov x9, #0x400000
        0xd51b4409, // msr fpcr, x9
        0xd65f03c0, // ret
    },
    // 3: set FPCR, then tail-call the callback in x0
    {
        0xd2a00809, // mov x9, #0x400000
        0xd51b4409, // msr fpcr, x9
        0xaa0003e2, // mov x2, x0
        0x8b22437c, // add x28, x27, w2, uxtw
        0xd61f0380, // br x28
    },
    // 4: return FPCR
    {
        0xd53b4400, // mrs x0, fpcr
        0xd65f03c0, // ret
    },
    // 5: entry for lfi_ctx_run: set FPCR, make syscall 1, then make syscall 2
    // with the FPCR in x8
    {
        0xd2a00809, // mov x9, #0x400000
        0xd51b4409, // msr fpcr, x9
        0xd2800028, // mov x8, #1
        0xf85f837e, // ldur x30, [x27, #-8]
        0xd63f03c0, // blr x30
        0xd53b4408, // mrs x8, fpcr
        0xf85f837e, // ldur x30, [x27, #-8]
        0xd63f03c0, // blr x30
    },
};

enum {
    P_RET_D0,
    P_RET_D7,
    P_SET_FPCR,
    P_SET_FPCR_CB,
    P_GET_FPCR,
    P_RUN,
};

static uint64_t
get_fpcr(void)
{
    uint64_t v;
    __asm__ volatile("mrs %0, fpcr" : "=r"(v));
    return v;
}

static void
set_fpcr(uint64_t v)
{
    __asm__ volatile("msr fpcr, %0" : : "r"(v));
}

static uint64_t cb_fpcr;

static void
callback(void)
{
    cb_fpcr = get_fpcr();
}

// Set by main for nested_callback.
static struct LFIBox *nest_box;
static struct LFIContext **nest_ctxp;
static lfiptr nest_prog;
static uint64_t nest_fpcr;

// A callback that temporarily changes the host's FPCR and invokes the sandbox
// on the same context while it is changed, then restores it.
static void
nested_callback(void)
{
    uint64_t saved = get_fpcr();
    set_fpcr(0);
    nest_fpcr = LFI_INVOKE(nest_box, nest_ctxp, nest_prog, uint64_t, (void));
    assert(get_fpcr() == 0);
    set_fpcr(saved);
}

static int run_calls;
static uint64_t run_guest_fpcr;

// Syscall handler for P_RUN: the host's FPCR is loaded while it runs.
static void
run_handler(struct LFIContext *ctx)
{
    assert(get_fpcr() == HOST_FPCR);
    if (run_calls++ == 0) {
        assert(lfi_ctx_regs(ctx)->fpcr == GUEST_FPCR);
        return;
    }
    run_guest_fpcr = lfi_ctx_regs(ctx)->x8;
    lfi_ctx_exit(ctx, 1);
}

static struct LFIContext *
clone_cb(struct LFIBox *box)
{
    size_t pagesize = getpagesize();
    struct LFIContext *ctx = lfi_ctx_new(box, NULL);
    assert(ctx);
    lfiptr stack = lfi_box_mapany(box, pagesize, LFI_PROT_READ | LFI_PROT_WRITE,
        LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE, -1, 0);
    assert(stack != (lfiptr) -1);
    lfi_ctx_regs(ctx)->sp = stack + pagesize;
    return ctx;
}

int
main(void)
{
    size_t pagesize = getpagesize();
    struct LFIEngine *engine = lfi_new(
        (struct LFIOptions) {
            .boxsize = gb(4),
            .pagesize = pagesize,
        },
        1);
    assert(engine);
    lfi_set_clone_cb(engine, clone_cb);

    struct LFIBox *box = lfi_box_new(engine);
    assert(box);

    lfiptr p = lfi_box_mapany(box, pagesize, LFI_PROT_READ | LFI_PROT_WRITE,
        LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE, -1, 0);
    assert(p != (lfiptr) -1);
    lfi_box_copyto(box, p, progs, sizeof(progs));
    bool ret_ok = lfi_box_init_ret(box);
    assert(ret_ok);
    int r = lfi_box_mprotect(box, p, pagesize, LFI_PROT_READ | LFI_PROT_EXEC);
    assert(r == 0);

#define PROG(i) (p + (i) * SLOT * sizeof(uint32_t))

    // The first invocation on a NULL context clones one lazily; the float
    // argument registers must arrive in order.
    struct LFIContext *ctx = NULL;
    double d0 = LFI_INVOKE(box, &ctx, PROG(P_RET_D0), double,
        (double, double, double, double, double, double, double, double), 1.0,
        2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0);
    assert(ctx);
    printf("lazy clone: d0 = %f\n", d0);
    assert(d0 == 1.0);
    lfi_ctx_free(ctx);
    ctx = NULL;
    double d7 = LFI_INVOKE(box, &ctx, PROG(P_RET_D7), double,
        (double, double, double, double, double, double, double, double), 1.0,
        2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0);
    printf("lazy clone: d7 = %f\n", d7);
    assert(d7 == 8.0);

#if !defined(STORES_ONLY) && !defined(ASSUME_FIXED_FPCTRL)
    uint64_t orig_fpcr = get_fpcr();
    set_fpcr(HOST_FPCR);
    assert(get_fpcr() == HOST_FPCR);

    // The sandbox starts with the default FPCR, not the host's.
    uint64_t v = LFI_INVOKE(box, &ctx, PROG(P_GET_FPCR), uint64_t, (void));
    printf("sandbox initial fpcr = %#lx\n", (unsigned long) v);
    assert(v == 0);
    assert(get_fpcr() == HOST_FPCR);

    // Changes the sandbox makes to FPCR do not leak into the host, and persist
    // across invocations.
    LFI_INVOKE(box, &ctx, PROG(P_SET_FPCR), void, (void));
    assert(get_fpcr() == HOST_FPCR);
    v = LFI_INVOKE(box, &ctx, PROG(P_GET_FPCR), uint64_t, (void));
    printf("sandbox fpcr = %#lx\n", (unsigned long) v);
    assert(v == GUEST_FPCR);

    // Callbacks run with the host's FPCR, and the sandbox's FPCR survives the
    // callback.
    lfi_ctx_regs(ctx)->fpcr = 0;
    void *box_cb = lfi_box_register_cb(box, (void *) callback);
    assert(box_cb);
    LFI_INVOKE(box, &ctx, PROG(P_SET_FPCR_CB), void, (void (*)(void)), box_cb);
    printf("callback fpcr = %#lx\n", (unsigned long) cb_fpcr);
    assert(cb_fpcr == HOST_FPCR);
    assert(get_fpcr() == HOST_FPCR);
    v = LFI_INVOKE(box, &ctx, PROG(P_GET_FPCR), uint64_t, (void));
    assert(v == GUEST_FPCR);

    // A callback that changes the host's FPCR, invokes the same context, and
    // restores its FPCR before returning leaves the outer invocation's host
    // FPCR intact.
    nest_box = box;
    nest_ctxp = &ctx;
    nest_prog = PROG(P_GET_FPCR);
    void *box_nested_cb = lfi_box_register_cb(box, (void *) nested_callback);
    assert(box_nested_cb);
    LFI_INVOKE(box, &ctx, PROG(P_SET_FPCR_CB), void, (void (*)(void)),
        box_nested_cb);
    printf("nested: sandbox fpcr = %#lx, host fpcr = %#lx\n",
        (unsigned long) nest_fpcr, (unsigned long) get_fpcr());
    assert(nest_fpcr == GUEST_FPCR);
    assert(get_fpcr() == HOST_FPCR);

    // lfi_ctx_run: syscalls run with the host's FPCR, the sandbox's FPCR
    // survives them, and the host's FPCR is restored when the sandbox exits.
    struct LFIContext *run_ctx = clone_cb(box);
    lfi_sys_handler(engine, run_handler);
    int code = lfi_ctx_run(run_ctx, PROG(P_RUN));
    printf("ctx_run: sandbox fpcr = %#lx, host fpcr = %#lx\n",
        (unsigned long) run_guest_fpcr, (unsigned long) get_fpcr());
    assert(code == 1);
    assert(run_calls == 2);
    assert(run_guest_fpcr == GUEST_FPCR);
    assert(get_fpcr() == HOST_FPCR);
    lfi_ctx_free(run_ctx);

    set_fpcr(orig_fpcr);
#endif

    lfi_ctx_free(ctx);
    lfi_box_free(box);
    lfi_free(engine);

    printf("OK\n");
    return 0;
}
