// Tests that the floating-point argument registers arrive in order when an
// invocation lazily clones its context.
//
// This is separate from test_fpcr.c and test_fpstate.c because Clang gives
// every LFI_INVOKE in a translation unit the prototype of the first one, so a
// file cannot mix invocations that pass or return values in different
// registers.

#include "lfi_arch.h"
#include "lfi_core.h"
#include "test.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#if defined(LFI_ARCH_ARM64)

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
};

#define NO_VERIFY false

#elif defined(LFI_ARCH_X64)

// clang-format off
// Return to the address on the stack.
#define RETSEQ                                                   \
    0x41, 0x5b,             /* pop %r11 */                       \
    0x41, 0x83, 0xe3, 0xe0, /* and $0xffffffe0, %r11d */         \
    0x4d, 0x09, 0xf3,       /* or %r14, %r11 */                  \
    0x41, 0xff, 0xe3        /* jmp *%r11 */
// clang-format on

// Each program gets its own 64-byte slot.
#define SLOT      64

static const uint8_t progs[][SLOT] = {
    // 0: return (xmm0 passes through as the return value)
    { RETSEQ },
    // 1: return xmm7
    { 0x0f, 0x28, 0xc7, /* movaps %xmm7, %xmm0 */ RETSEQ },
};

#define NO_VERIFY true

#else
#error "unsupported architecture"
#endif

enum {
    P_RET_ARG0,
    P_RET_ARG7,
};

static struct LFIContext *
clone_cb(struct LFIBox *box)
{
    size_t pagesize = getpagesize();
    struct LFIContext *ctx = lfi_ctx_new(box, NULL);
    assert(ctx);
    lfiptr stack = lfi_box_mapany(box, pagesize, LFI_PROT_READ | LFI_PROT_WRITE,
        LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE, -1, 0);
    assert(stack != (lfiptr) -1);
#if defined(LFI_ARCH_ARM64)
    lfi_ctx_regs(ctx)->sp = stack + pagesize;
#else
    lfi_ctx_regs(ctx)->rsp = stack + pagesize;
#endif
    return ctx;
}

// Invokes prog with the float arguments 1.0 through 8.0. The only LFI_INVOKE
// in this file.
static double
invoke(struct LFIBox *box, struct LFIContext **ctxp, lfiptr prog)
{
    return LFI_INVOKE(box, ctxp, prog, double,
        (double, double, double, double, double, double, double, double), 1.0,
        2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0);
}

int
main(void)
{
    size_t pagesize = getpagesize();
    struct LFIEngine *engine = lfi_new(
        (struct LFIOptions) {
            .boxsize = gb(4),
            .pagesize = pagesize,
            .no_verify = NO_VERIFY,
        },
        1);
    assert(engine);
    lfi_set_clone_cb(engine, clone_cb);

    struct LFIBox *box = lfi_box_new(engine);
    assert(box);

    lfiptr p = lfi_box_mapany(box, pagesize, LFI_PROT_READ | LFI_PROT_WRITE,
        LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE, -1, 0);
    assert(p != (lfiptr) -1);
#if defined(LFI_ARCH_X64)
    memset((void *) lfi_box_l2p(box, p), 0xcc, pagesize);
#endif
    lfi_box_copyto(box, p, progs, sizeof(progs));
    bool ret_ok = lfi_box_init_ret(box);
    assert(ret_ok);
    int r = lfi_box_mprotect(box, p, pagesize, LFI_PROT_READ | LFI_PROT_EXEC);
    assert(r == 0);

#define PROG(i) (p + (i) * sizeof(progs[0]))

    // Each invocation starts from a NULL context, so it clones one lazily.
    struct LFIContext *ctx = NULL;
    double a0 = invoke(box, &ctx, PROG(P_RET_ARG0));
    assert(ctx);
    printf("lazy clone: arg 0 = %f\n", a0);
    assert(a0 == 1.0);
    lfi_ctx_free(ctx);

    ctx = NULL;
    double a7 = invoke(box, &ctx, PROG(P_RET_ARG7));
    assert(ctx);
    printf("lazy clone: arg 7 = %f\n", a7);
    assert(a7 == 8.0);
    lfi_ctx_free(ctx);

    lfi_box_free(box);
    lfi_free(engine);

    printf("OK\n");
    return 0;
}
