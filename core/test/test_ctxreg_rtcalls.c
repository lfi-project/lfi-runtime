// Tests runtime calls through the context register: they work in the default
// configuration, and with LFIOptions.ctxreg_rtcalls they are the only runtime
// calls allowed and sandboxes have no sys page below their base.

#include "lfi_arch.h"
#include "lfi_core.h"
#include "test.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "test_progs.h"

// Programs for lfi_ctx_run that make a syscall with 42 as its number.
#if defined(LFI_ARCH_ARM64)

static uint8_t sys_ctxreg[] = {
    0x48, 0x05, 0x80, 0xd2, // mov x8, #42
    0x3e, 0x83, 0x5f, 0xf8, // ldur x30, [x25, #-8]
    0xc0, 0x03, 0x3f, 0xd6, // blr x30
};

static uint8_t sys_base[] = {
    0x48, 0x05, 0x80, 0xd2, // mov x8, #42
    0x7e, 0x83, 0x5f, 0xf8, // ldur x30, [x27, #-8]
    0xc0, 0x03, 0x3f, 0xd6, // blr x30
};

#elif defined(LFI_ARCH_X64)

static uint8_t sys_ctxreg[] = {
    0x48, 0xc7, 0xc0, 0x2a, 0x00, 0x00, 0x00, // mov $0x2a, %rax
    0x4c, 0x8d, 0x1d, 0x04, 0x00, 0x00, 0x00, // lea 0x4(%rip), %r11
    0x41, 0xff, 0x67, 0xf8,                   // jmp *-8(%r15)
};

static uint8_t sys_base[] = {
    0x48, 0xc7, 0xc0, 0x2a, 0x00, 0x00, 0x00, // mov $0x2a, %rax
    0x4c, 0x8d, 0x1d, 0x04, 0x00, 0x00, 0x00, // lea 0x4(%rip), %r11
    0x41, 0xff, 0x66, 0xf8,                   // jmp *-8(%r14)
};

#else
#error "architecture not supported"
#endif

static void
handler(struct LFIContext *ctx)
{
#if defined(LFI_ARCH_ARM64)
    int arg = (int) lfi_ctx_regs(ctx)->x8;
#elif defined(LFI_ARCH_X64)
    int arg = (int) lfi_ctx_regs(ctx)->rax;
#endif
    assert(arg == 42);
    lfi_ctx_exit(ctx, 1);
}

static void
set_sp(struct LFIContext *ctx, lfiptr sp)
{
#if defined(LFI_ARCH_X64)
    lfi_ctx_regs(ctx)->rsp = sp;
#elif defined(LFI_ARCH_ARM64)
    lfi_ctx_regs(ctx)->sp = sp;
#endif
}

static struct LFIContext *
new_ctx(struct LFIBox *box)
{
    size_t pagesize = getpagesize();
    struct LFIContext *ctx = lfi_ctx_new(box, NULL);
    assert(ctx);
    lfiptr stack = lfi_box_mapany(box, pagesize, LFI_PROT_READ | LFI_PROT_WRITE,
        LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE, -1, 0);
    assert(stack != (lfiptr) -1);
    set_sp(ctx, stack + pagesize);
    return ctx;
}

// Maps code into box and makes it executable, which verifies it. Returns its
// address, or -1 if verification rejected it.
static lfiptr
load(struct LFIBox *box, const uint8_t *code, size_t size)
{
    size_t pagesize = getpagesize();
    lfiptr p = lfi_box_mapany(box, pagesize, LFI_PROT_READ | LFI_PROT_WRITE,
        LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE, -1, 0);
    assert(p != (lfiptr) -1);
#if defined(LFI_ARCH_X64)
    // Set all bytes to trap instructions, since 0 does not pass verification.
    memset((void *) lfi_box_l2p(box, p), 0xcc, pagesize);
#endif
    lfi_box_copyto(box, p, code, size);
    if (lfi_box_mprotect(box, p, pagesize, LFI_PROT_READ | LFI_PROT_EXEC) != 0)
        return (lfiptr) -1;
    return p;
}

// Runs a syscall program in a fresh context.
static void
run_sys(struct LFIBox *box, lfiptr p)
{
    struct LFIContext *ctx = new_ctx(box);
    assert(lfi_ctx_run(ctx, p) == 1);
    lfi_ctx_free(ctx);
}

// Returns the permissions of the mapping containing addr ("---p" etc.), or
// NULL if addr is not mapped.
static const char *
perms(uintptr_t addr)
{
    static char buf[8];
    FILE *f = fopen("/proc/self/maps", "r");
    if (!f)
        return "?";
    char line[512];
    const char *ret = NULL;
    while (fgets(line, sizeof(line), f)) {
        unsigned long lo, hi;
        if (sscanf(line, "%lx-%lx %7s", &lo, &hi, buf) == 3 && addr >= lo &&
            addr < hi) {
            ret = buf;
            break;
        }
    }
    fclose(f);
    return ret;
}

struct Box {
    struct LFIBox *box;
    struct LFIContext *ctx;
    lfiptr add;
};

// Sets up box for calling the add program through LFI_INVOKE, which returns
// through lfi_box_init_ret's return stub.
static struct Box
setup_add(struct LFIBox *box)
{
    bool ok = lfi_box_init_ret(box);
    assert(ok);
    lfiptr add = load(box, prog, sizeof(prog));
    assert(add != (lfiptr) -1);
    return (struct Box) { .box = box, .ctx = new_ctx(box), .add = add };
}

static void
call_add(struct Box *b)
{
    int x = LFI_INVOKE(b->box, &b->ctx, b->add, int, (int, int), 10, 32);
    assert(x == 42);
}

int
main(void)
{
    (void) prog_cb;
    size_t pagesize = getpagesize();

    // Default configuration: both kinds of runtime calls are allowed.
    {
        struct LFIEngine *engine = lfi_new(
            (struct LFIOptions) { .boxsize = gb(4), .pagesize = pagesize }, 1);
        assert(engine);
        lfi_sys_handler(engine, handler);
        struct LFIBox *box = lfi_box_new(engine);
        assert(box);
        lfiptr p = load(box, sys_ctxreg, sizeof(sys_ctxreg));
        assert(p != (lfiptr) -1);
        run_sys(box, p);
        p = load(box, sys_base, sizeof(sys_base));
        assert(p != (lfiptr) -1);
        run_sys(box, p);
        struct LFIBoxInfo info = lfi_box_info(box);
        assert(perms(info.base - pagesize) &&
            strcmp(perms(info.base - pagesize), "r--p") == 0);
        lfi_box_free(box);
        lfi_free(engine);
        printf("default: both runtime call kinds work\n");
    }

    // ctxreg_rtcalls: no sys page, and only runtime calls through the context
    // register.
    struct LFIEngine *engine = lfi_new(
        (struct LFIOptions) {
            .boxsize = gb(4),
            .pagesize = pagesize,
            .ctxreg_rtcalls = true,
        },
        2);
    assert(engine);
    lfi_sys_handler(engine, handler);

    struct LFIBox *a = lfi_box_new(engine);
    struct LFIBox *b = lfi_box_new(engine);
    assert(a && b);
    struct LFIBoxInfo ia = lfi_box_info(a), ib = lfi_box_info(b);

    // The page below the higher of the two pre-reserved sandboxes is inside
    // the reservation (the top of the lower sandbox, or its external guard),
    // and holds no sys page. (Below the lowest sandbox there may be no guard
    // at all, with PKU.)
    {
        uintptr_t upper = ia.base > ib.base ? ia.base : ib.base;
        const char *below = perms(upper - pagesize);
        printf("ctxreg_rtcalls: page below the upper sandbox: %s\n",
            below ? below : "unmapped");
        assert(below && strcmp(below, "---p") == 0);
    }

    lfiptr p = load(a, sys_base, sizeof(sys_base));
    assert(p == (lfiptr) -1);
    printf("ctxreg_rtcalls: base-relative runtime call rejected\n");
    p = load(a, sys_ctxreg, sizeof(sys_ctxreg));
    assert(p != (lfiptr) -1);
    run_sys(a, p);

    struct Box ba = setup_add(a), bb = setup_add(b);
    call_add(&ba);
    call_add(&bb);

    // Freeing a sandbox leaves the one next to it working.
    struct Box *lower = ia.base < ib.base ? &ba : &bb;
    struct Box *upper = lower == &ba ? &bb : &ba;
    lfi_ctx_free(lower->ctx);
    lfi_box_free(lower->box);
    call_add(upper);
    lfi_ctx_free(upper->ctx);
    lfi_box_free(upper->box);

    // A sandbox beyond the pre-reserved ones (in a region of its own).
    struct LFIBox *boxes[3];
    for (int i = 0; i < 3; i++) {
        boxes[i] = lfi_box_new(engine);
        assert(boxes[i]);
    }
    struct Box bc = setup_add(boxes[2]);
    call_add(&bc);
    lfi_ctx_free(bc.ctx);
    for (int i = 0; i < 3; i++)
        lfi_box_free(boxes[i]);

    lfi_free(engine);
    printf("OK\n");
    return 0;
}
