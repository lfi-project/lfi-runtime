// Tests that host/sandbox transitions on x86-64 keep floating-point argument
// registers and FP control state (x87 control word and MXCSR) intact.

#include "lfi_arch.h"
#include "lfi_core.h"
#include "test.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// clang-format off
// Return to the address on the stack.
#define RETSEQ                                                   \
    0x41, 0x5b,             /* pop %r11 */                       \
    0x41, 0x83, 0xe3, 0xe0, /* and $0xffffffe0, %r11d */         \
    0x4d, 0x09, 0xf3,       /* or %r14, %r11 */                  \
    0x41, 0xff, 0xe3        /* jmp *%r11 */

// Set MXCSR and the x87 control word to round up.
#define SET_FP                                                   \
    0xc7, 0x44, 0x24, 0xf8, 0x80, 0x5f, 0x00, 0x00, /* movl $0x5f80, -8(%rsp) */ \
    0x0f, 0xae, 0x54, 0x24, 0xf8,             /* ldmxcsr -8(%rsp) */             \
    0x66, 0xc7, 0x44, 0x24, 0xf8, 0x7f, 0x0b, /* movw $0xb7f, -8(%rsp) */        \
    0xd9, 0x6c, 0x24, 0xf8                    /* fldcw -8(%rsp) */
// clang-format on

// MXCSR and x87 control word set by SET_FP (round up).
#define GUEST_MXCSR 0x5f80
#define GUEST_FCW   0x0b7f

// Each program gets its own 64-byte slot.
#define SLOT 64

static const uint8_t progs[][SLOT] = {
    // 0: return (xmm0 passes through as the return value)
    { RETSEQ },
    // 1: return xmm7
    { 0x0f, 0x28, 0xc7, /* movaps %xmm7, %xmm0 */ RETSEQ },
    // 2: set MXCSR and FCW, then return
    { SET_FP, RETSEQ },
    // 3: set MXCSR and FCW, then tail-call the callback in %rdi
    { SET_FP,
        0x48, 0x89, 0xf8, // mov %rdi, %rax
        0x83, 0xe0, 0xe0, // and $0xffffffe0, %eax
        0x4c, 0x09, 0xf0, // or %r14, %rax
        0xff, 0xe0 },     // jmp *%rax
    // 4: return MXCSR
    { 0x0f, 0xae, 0x5c, 0x24, 0xf8, // stmxcsr -8(%rsp)
        0x8b, 0x44, 0x24, 0xf8,     // mov -8(%rsp), %eax
        RETSEQ },
    // 5: return FCW
    { 0xd9, 0x7c, 0x24, 0xf8,       // fnstcw -8(%rsp)
        0x0f, 0xb7, 0x44, 0x24, 0xf8, // movzwl -8(%rsp), %eax
        RETSEQ },
    // 6: entry for lfi_ctx_run: set FCW, make syscall 1, then make syscall 2
    // with the FCW in %rax
    { 0x66, 0xc7, 0x44, 0x24, 0xf8, 0x7f, 0x0b,   // movw $0xb7f, -8(%rsp)
        0xd9, 0x6c, 0x24, 0xf8,                   // fldcw -8(%rsp)
        0xb8, 0x01, 0x00, 0x00, 0x00,             // mov $1, %eax
        0x4c, 0x8d, 0x1d, 0x04, 0x00, 0x00, 0x00, // lea 4(%rip), %r11
        0x41, 0xff, 0x66, 0xf8,                   // jmp *-8(%r14)
        0xd9, 0x7c, 0x24, 0xf8,                   // fnstcw -8(%rsp)
        0x0f, 0xb7, 0x44, 0x24, 0xf8,             // movzwl -8(%rsp), %eax
        0x4c, 0x8d, 0x1d, 0x04, 0x00, 0x00, 0x00, // lea 4(%rip), %r11
        0x41, 0xff, 0x66, 0xf8 },                 // jmp *-8(%r14)
};

enum {
    P_RET_XMM0,
    P_RET_XMM7,
    P_SET_FP,
    P_SET_FP_CB,
    P_GET_MXCSR,
    P_GET_FCW,
    P_RUN,
};

static uint32_t
get_mxcsr(void)
{
    uint32_t m;
    __asm__ volatile("stmxcsr %0" : "=m"(m));
    return m;
}

static uint16_t
get_fcw(void)
{
    uint16_t f;
    __asm__ volatile("fnstcw %0" : "=m"(f));
    return f;
}

static uint32_t cb_mxcsr;
static uint16_t cb_fcw;

static void
callback(void)
{
    cb_mxcsr = get_mxcsr();
    cb_fcw = get_fcw();
}

// Set by main for nested_callback.
static struct LFIBox *nest_box;
static struct LFIContext **nest_ctxp;
static lfiptr nest_prog;
static uint16_t nest_fcw;

// A callback that temporarily changes the host's FP control state and invokes
// the sandbox on the same context while it is changed, then restores it.
static void
nested_callback(void)
{
    uint32_t saved_mxcsr = get_mxcsr(), up_mxcsr = 0x5f80;
    uint16_t saved_fcw = get_fcw(), up_fcw = 0x0b7f;
    __asm__ volatile("ldmxcsr %0" : : "m"(up_mxcsr));
    __asm__ volatile("fldcw %0" : : "m"(up_fcw));
    nest_fcw = LFI_INVOKE(nest_box, nest_ctxp, nest_prog, uint16_t, (void));
    assert(get_mxcsr() == up_mxcsr);
    assert(get_fcw() == up_fcw);
    __asm__ volatile("ldmxcsr %0" : : "m"(saved_mxcsr));
    __asm__ volatile("fldcw %0" : : "m"(saved_fcw));
}

// Host state expected by run_handler.
static uint32_t run_host_mxcsr;
static uint16_t run_host_fcw;
static int run_calls;
static uint16_t run_guest_fcw;

// Syscall handler for P_RUN: the host's state is loaded while it runs.
static void
run_handler(struct LFIContext *ctx)
{
    assert(get_mxcsr() == run_host_mxcsr);
    assert(get_fcw() == run_host_fcw);
    if (run_calls++ == 0) {
        assert(lfi_ctx_regs(ctx)->fcw == GUEST_FCW);
        lfi_ctx_regs(ctx)->rax = 0;
        return;
    }
    run_guest_fcw = (uint16_t) lfi_ctx_regs(ctx)->rax;
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
    lfi_ctx_regs(ctx)->rsp = stack + pagesize;
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
            .no_verify = true,
        },
        1);
    assert(engine);
    lfi_set_clone_cb(engine, clone_cb);

    struct LFIBox *box = lfi_box_new(engine);
    assert(box);

    lfiptr p = lfi_box_mapany(box, pagesize, LFI_PROT_READ | LFI_PROT_WRITE,
        LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE, -1, 0);
    assert(p != (lfiptr) -1);
    memset((void *) lfi_box_l2p(box, p), 0xcc, pagesize);
    lfi_box_copyto(box, p, progs, sizeof(progs));
    bool ret_ok = lfi_box_init_ret(box);
    assert(ret_ok);
    int r = lfi_box_mprotect(box, p, pagesize, LFI_PROT_READ | LFI_PROT_EXEC);
    assert(r == 0);

#define PROG(i) (p + (i) * SLOT)

    // The first invocation on a NULL context clones one lazily; the float
    // argument registers must arrive in order.
    struct LFIContext *ctx = NULL;
    double x0 = LFI_INVOKE(box, &ctx, PROG(P_RET_XMM0), double,
        (double, double, double, double, double, double, double, double), 1.0,
        2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0);
    assert(ctx);
    printf("lazy clone: xmm0 = %f\n", x0);
    assert(x0 == 1.0);
    lfi_ctx_free(ctx);
    ctx = NULL;
    double x7 = LFI_INVOKE(box, &ctx, PROG(P_RET_XMM7), double,
        (double, double, double, double, double, double, double, double), 1.0,
        2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0);
    printf("lazy clone: xmm7 = %f\n", x7);
    assert(x7 == 8.0);

#if !defined(STORES_ONLY) && !defined(ASSUME_FIXED_FPCTRL)
    // Use a non-default host state (flush-to-zero, double precision) so that
    // it differs from both the sandbox's state and the context's defaults.
    uint32_t orig_mxcsr = get_mxcsr();
    uint16_t orig_fcw = get_fcw();
    uint32_t host_mxcsr = 0x9f80;
    uint16_t host_fcw = 0x27f;
    __asm__ volatile("ldmxcsr %0" : : "m"(host_mxcsr));
    __asm__ volatile("fldcw %0" : : "m"(host_fcw));

    // Changes the sandbox makes to its FP control state do not leak into the
    // host. Changes to the x87 control word persist across invocations; MXCSR
    // changes do not (verified code cannot make them), so the sandbox sees the
    // MXCSR in its context.
    LFI_INVOKE(box, &ctx, PROG(P_SET_FP), void, (void));
    assert(get_mxcsr() == host_mxcsr);
    assert(get_fcw() == host_fcw);
    uint32_t m = LFI_INVOKE(box, &ctx, PROG(P_GET_MXCSR), uint32_t, (void));
    uint16_t f = LFI_INVOKE(box, &ctx, PROG(P_GET_FCW), uint16_t, (void));
    printf("sandbox mxcsr = %#x, fcw = %#x\n", m, f);
    assert(m == lfi_ctx_regs(ctx)->mxcsr);
    assert(f == GUEST_FCW);

    // The host's state is restored even when the sandbox's context holds a
    // different one.
    lfi_ctx_regs(ctx)->mxcsr = GUEST_MXCSR;
    m = LFI_INVOKE(box, &ctx, PROG(P_GET_MXCSR), uint32_t, (void));
    assert(m == GUEST_MXCSR);
    assert(get_mxcsr() == host_mxcsr);
    lfi_ctx_regs(ctx)->mxcsr = 0x1f80;

    // Callbacks run with the host's FP control state, and the sandbox's state
    // survives the callback.
    lfi_ctx_regs(ctx)->fcw = 0x37f;
    void *box_cb = lfi_box_register_cb(box, (void *) callback);
    assert(box_cb);
    LFI_INVOKE(box, &ctx, PROG(P_SET_FP_CB), void, (void (*)(void)), box_cb);
    printf("callback mxcsr = %#x, fcw = %#x\n", cb_mxcsr, cb_fcw);
    assert(cb_mxcsr == host_mxcsr);
    assert(cb_fcw == host_fcw);
    assert(get_mxcsr() == host_mxcsr);
    assert(get_fcw() == host_fcw);
    f = LFI_INVOKE(box, &ctx, PROG(P_GET_FCW), uint16_t, (void));
    assert(f == GUEST_FCW);

    // A callback that changes the host's state, invokes the same context, and
    // restores its state before returning leaves the outer invocation's host
    // state intact.
    nest_box = box;
    nest_ctxp = &ctx;
    nest_prog = PROG(P_GET_FCW);
    void *box_nested_cb = lfi_box_register_cb(box, (void *) nested_callback);
    assert(box_nested_cb);
    LFI_INVOKE(box, &ctx, PROG(P_SET_FP_CB), void, (void (*)(void)),
        box_nested_cb);
    printf("nested: sandbox fcw = %#x, host mxcsr = %#x, fcw = %#x\n",
        nest_fcw, get_mxcsr(), get_fcw());
    assert(nest_fcw == GUEST_FCW);
    assert(get_mxcsr() == host_mxcsr);
    assert(get_fcw() == host_fcw);

    // lfi_ctx_run: syscalls run with the host's state, the sandbox's state
    // survives them, and the host's state is restored when the sandbox exits.
    struct LFIContext *run_ctx = clone_cb(box);
    run_host_mxcsr = host_mxcsr;
    run_host_fcw = host_fcw;
    lfi_sys_handler(engine, run_handler);
    int code = lfi_ctx_run(run_ctx, PROG(P_RUN));
    printf("ctx_run: sandbox fcw = %#x, host mxcsr = %#x, fcw = %#x\n",
        run_guest_fcw, get_mxcsr(), get_fcw());
    assert(code == 1);
    assert(run_calls == 2);
    assert(run_guest_fcw == GUEST_FCW);
    assert(get_mxcsr() == host_mxcsr);
    assert(get_fcw() == host_fcw);
    lfi_ctx_free(run_ctx);

    __asm__ volatile("ldmxcsr %0" : : "m"(orig_mxcsr));
    __asm__ volatile("fldcw %0" : : "m"(orig_fcw));
#endif

    lfi_ctx_free(ctx);
    lfi_box_free(box);
    lfi_free(engine);

    printf("OK\n");
    return 0;
}
