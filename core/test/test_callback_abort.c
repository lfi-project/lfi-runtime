// Tests aborting a callback with lfi_ctx_abort_cb, including after the
// callback invoked the same context again.

#include "lfi_arch.h"
#include "lfi_core.h"
#include "test.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "test_progs.h"

static struct LFIBox *box;
static struct LFIContext *ctx;
static lfiptr p_prog;
static lfiptr p_prog_cb;

// Aborts: the invocation returns this callback's return value.
static int
abort_cb(int a)
{
    lfi_ctx_abort_cb(ctx);
    return a + 1;
}

// Invokes the same context again, then aborts.
static int
nested_abort_cb(int a)
{
    int x = LFI_INVOKE(box, &ctx, p_prog, int, (int, int), a, 1);
    assert(x == a + 1);
    lfi_ctx_abort_cb(ctx);
    return x + 1;
}

static void *cb_abort;

// Invokes the same context again and returns normally.
static int
nested_cb(int a)
{
    int x = LFI_INVOKE(box, &ctx, p_prog, int, (int, int), a, 2);
    assert(x == a + 2);
    return x;
}

// Makes a nested invocation whose callback aborts, then returns normally.
static int
nested_inner_abort_cb(int a)
{
    int x = LFI_INVOKE(box, &ctx, p_prog_cb, int, (int (*)(int), int),
        cb_abort, a);
    assert(x == a + 1);
    assert(lfi_ctx_abort_cb_status(ctx));
    return x + 1;
}

// Struct callbacks take their argument from and leave their return value in
// the context's registers.
static uint64_t
struct_arg(void)
{
#if defined(LFI_ARCH_X64)
    return lfi_ctx_regs(ctx)->rdi;
#elif defined(LFI_ARCH_ARM64)
    return lfi_ctx_regs(ctx)->x0;
#endif
}

static void
struct_ret(uint64_t v)
{
#if defined(LFI_ARCH_X64)
    lfi_ctx_regs(ctx)->rax = v;
#elif defined(LFI_ARCH_ARM64)
    lfi_ctx_regs(ctx)->x0 = v;
#endif
}

static void
struct_cb(void)
{
    struct_ret(struct_arg() + 100);
}

static void
struct_abort_cb(void)
{
    struct_ret(struct_arg() + 200);
    lfi_ctx_abort_cb(ctx);
}

static uint64_t
sandbox_sp(void)
{
#if defined(LFI_ARCH_X64)
    return lfi_ctx_regs(ctx)->rsp;
#elif defined(LFI_ARCH_ARM64)
    return lfi_ctx_regs(ctx)->sp;
#endif
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

    box = lfi_box_new(engine);
    assert(box);

    lfiptr p = lfi_box_mapany(box, pagesize, LFI_PROT_READ | LFI_PROT_WRITE,
        LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE, -1, 0);
    assert(p != (lfiptr) -1);
#if defined(LFI_ARCH_X64)
    // Set all bytes to trap instructions, since 0 does not pass verification.
    memset((void *) lfi_box_l2p(box, p), 0xcc, pagesize);
#endif
    p_prog = lfi_box_copyto(box, p, prog, sizeof(prog));
    p_prog_cb = lfi_box_copyto(box, p + sizeof(prog), prog_cb,
        sizeof(prog_cb));
    bool ret_ok = lfi_box_init_ret(box);
    assert(ret_ok);
    int r = lfi_box_mprotect(box, p, pagesize, LFI_PROT_READ | LFI_PROT_EXEC);
    assert(r == 0);

    ctx = lfi_ctx_new(box, NULL);
    assert(ctx);
    lfiptr stack = lfi_box_mapany(box, pagesize, LFI_PROT_READ | LFI_PROT_WRITE,
        LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE, -1, 0);
    assert(stack != (lfiptr) -1);
#if defined(LFI_ARCH_X64)
    lfi_ctx_regs(ctx)->rsp = stack + pagesize;
#elif defined(LFI_ARCH_ARM64)
    lfi_ctx_regs(ctx)->sp = stack + pagesize;
#endif

    cb_abort = lfi_box_register_cb(box, (void *) abort_cb);
    void *cb_nested_inner_abort = lfi_box_register_cb(box,
        (void *) nested_inner_abort_cb);
    void *cb_nested_abort = lfi_box_register_cb(box, (void *) nested_abort_cb);
    void *cb_nested = lfi_box_register_cb(box, (void *) nested_cb);
    void *cb_struct = lfi_box_register_cb_struct(box, (void *) struct_cb);
    void *cb_struct_abort = lfi_box_register_cb_struct(box,
        (void *) struct_abort_cb);
    assert(cb_abort && cb_nested_abort && cb_nested);
    assert(cb_struct && cb_struct_abort && cb_nested_inner_abort);

    uint64_t sp = sandbox_sp();

    // Repeat so that a host or sandbox stack imbalance would show up.
    for (int i = 0; i < 1000; i++) {
        int x = LFI_INVOKE(box, &ctx, p_prog_cb, int, (int (*)(int), int),
            cb_abort, i);
        assert(x == i + 1);
        assert(lfi_ctx_abort_cb_status(ctx));

        x = LFI_INVOKE(box, &ctx, p_prog_cb, int, (int (*)(int), int),
            cb_nested_abort, i);
        assert(x == i + 2);
        assert(lfi_ctx_abort_cb_status(ctx));

        x = LFI_INVOKE(box, &ctx, p_prog_cb, int, (int (*)(int), int),
            cb_nested, i);
        assert(x == i + 2);
        assert(!lfi_ctx_abort_cb_status(ctx));

        // Only the nested invocation is aborted.
        x = LFI_INVOKE(box, &ctx, p_prog_cb, int, (int (*)(int), int),
            cb_nested_inner_abort, i);
        assert(x == i + 2);
        assert(!lfi_ctx_abort_cb_status(ctx));

        x = LFI_INVOKE(box, &ctx, p_prog_cb, int, (void (*)(void), int),
            cb_struct, i);
        assert(x == i + 100);
        assert(!lfi_ctx_abort_cb_status(ctx));

        x = LFI_INVOKE(box, &ctx, p_prog_cb, int, (void (*)(void), int),
            cb_struct_abort, i);
        assert(x == i + 200);
        assert(lfi_ctx_abort_cb_status(ctx));

        x = LFI_INVOKE(box, &ctx, p_prog, int, (int, int), 10, 32);
        assert(x == 42);
        assert(!lfi_ctx_abort_cb_status(ctx));

        assert(sandbox_sp() == sp);
    }

    lfi_ctx_free(ctx);
    lfi_box_free(box);
    lfi_free(engine);

    printf("OK\n");
    return 0;
}
