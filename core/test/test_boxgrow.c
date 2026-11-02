// Tests creating more sandboxes than the engine pre-reserved space for: the
// extra sandboxes reserve address space on demand and give it back when they
// are freed, while the pre-reserved space stays.

#include "lfi_arch.h"
#include "lfi_core.h"
#include "test.h"

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "test_progs.h"

#define PRERESERVED 2
#define NBOXES      5

// Virtual address space of this process in bytes, or 0 if unknown.
static size_t
vmsize(void)
{
    size_t kb = 0;
#ifdef __linux__
    FILE *f = fopen("/proc/self/status", "r");
    assert(f);
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "VmSize:", 7) == 0) {
            kb = strtoul(line + 7, NULL, 10);
            break;
        }
    }
    fclose(f);
#endif
    return kb * 1024;
}

// Changes in VmSize smaller than this are not reservations: every reservation
// is at least a sandbox (gigabytes), while malloc, including glibc's per-thread
// arenas, and thread stacks account for far less.
#define NOISE gb(1)

static double
gib(size_t x)
{
    return (double) x / gb(1);
}

static size_t
absdiff(size_t a, size_t b)
{
    return a > b ? a - b : b - a;
}

struct Prog {
    struct LFIBox *box;
    struct LFIContext *ctx;
    lfiptr p;
};

static void
call(struct Prog *prog)
{
    int x = LFI_INVOKE(prog->box, &prog->ctx, prog->p, int, (int, int), 10, 32);
    assert(x == 42);
}

// Loads the test program into box and runs it.
static struct Prog
run_prog(struct LFIBox *box)
{
    size_t pagesize = getpagesize();
    lfiptr p = lfi_box_mapany(box, pagesize, LFI_PROT_READ | LFI_PROT_WRITE,
        LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE, -1, 0);
    assert(p != (lfiptr) -1);
#if defined(LFI_ARCH_X64)
    memset((void *) lfi_box_l2p(box, p), 0xcc, pagesize);
#endif
    lfi_box_copyto(box, p, prog, sizeof(prog));
    bool ok = lfi_box_init_ret(box);
    assert(ok);
    int r = lfi_box_mprotect(box, p, pagesize, LFI_PROT_READ | LFI_PROT_EXEC);
    assert(r == 0);

    struct LFIContext *ctx = lfi_ctx_new(box, NULL);
    assert(ctx);
    lfiptr stack = lfi_box_mapany(box, pagesize, LFI_PROT_READ | LFI_PROT_WRITE,
        LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE, -1, 0);
    assert(stack != (lfiptr) -1);
#if defined(LFI_ARCH_X64)
    lfi_ctx_regs(ctx)->rsp = stack + pagesize;
#elif defined(LFI_ARCH_ARM64)
    lfi_ctx_regs(ctx)->sp = stack + pagesize;
#endif
    struct Prog prog = { .box = box, .ctx = ctx, .p = p };
    call(&prog);
    return prog;
}

static struct LFIEngine *engine;

static void *
churn(void *arg)
{
    (void) arg;
    for (int i = 0; i < 20; i++) {
        struct LFIBox *box = lfi_box_new(engine);
        assert(box);
        lfiptr p = lfi_box_mapany(box, getpagesize(),
            LFI_PROT_READ | LFI_PROT_WRITE,
            LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE, -1, 0);
        assert(p != (lfiptr) -1);
        *(volatile int *) lfi_box_l2p(box, p) = i;
        lfi_box_free(box);
    }
    return NULL;
}

int
main(void)
{
    (void) prog_cb;
    size_t boxsize = gb(4);
    size_t v0 = vmsize();
    engine = lfi_new(
        (struct LFIOptions) {
            .boxsize = boxsize,
            .pagesize = getpagesize(),
        },
        PRERESERVED);
    assert(engine);
    size_t v1 = vmsize();
    printf("pre-reserved %d sandboxes: %.2f GiB\n", PRERESERVED, gib(v1 - v0));

    struct LFIBox *boxes[NBOXES];
    struct Prog progs[NBOXES];
    size_t after[NBOXES];
    for (int i = 0; i < NBOXES; i++) {
        boxes[i] = lfi_box_new(engine);
        assert(boxes[i]);
        after[i] = vmsize();
        progs[i] = run_prog(boxes[i]);
    }

    // The sandboxes do not overlap.
    for (int i = 0; i < NBOXES; i++) {
        struct LFIBoxInfo a = lfi_box_info(boxes[i]);
        for (int j = i + 1; j < NBOXES; j++) {
            struct LFIBoxInfo b = lfi_box_info(boxes[j]);
            assert(a.base + a.size <= b.base || b.base + b.size <= a.base);
        }
    }

    size_t ondemand = after[PRERESERVED] - after[PRERESERVED - 1];
    printf("on-demand sandbox: %.2f GiB\n", gib(ondemand));
    if (v0 != 0) {
        // The pre-reserved sandboxes use the existing reservation.
        assert(absdiff(after[PRERESERVED - 1], v1) < NOISE);
        // Each further sandbox reserves the same amount, at least its size.
        assert(ondemand >= boxsize);
        for (int i = PRERESERVED + 1; i < NBOXES; i++)
            assert(absdiff(after[i] - after[i - 1], ondemand) < NOISE);
    }

    // Freeing an on-demand sandbox gives its space back; freeing a
    // pre-reserved one does not.
    size_t before_free = vmsize();
    lfi_ctx_free(progs[NBOXES - 1].ctx);
    lfi_box_free(boxes[NBOXES - 1]);
    size_t freed = before_free - vmsize();
    lfi_ctx_free(progs[0].ctx);
    lfi_box_free(boxes[0]);
    size_t freed_pre = before_free - freed - vmsize();
    printf("freed: on-demand %.2f GiB, pre-reserved %.2f GiB\n", gib(freed),
        gib(freed_pre));
    if (v0 != 0) {
        assert(absdiff(freed, ondemand) < NOISE);
        assert(freed_pre < NOISE);
    }

    // Freeing a sandbox leaves the others working, including the one placed
    // directly above it, whose sys page lies at the freed sandbox's top.
    for (int i = 1; i < NBOXES - 1; i++)
        call(&progs[i]);

    // The freed slots can be used again, without disturbing the others.
    boxes[0] = lfi_box_new(engine);
    boxes[NBOXES - 1] = lfi_box_new(engine);
    assert(boxes[0] && boxes[NBOXES - 1]);
    progs[0] = run_prog(boxes[0]);
    progs[NBOXES - 1] = run_prog(boxes[NBOXES - 1]);
    for (int i = 0; i < NBOXES; i++)
        call(&progs[i]);
    for (int i = 0; i < NBOXES; i++) {
        lfi_ctx_free(progs[i].ctx);
        lfi_box_free(boxes[i]);
    }
    if (v0 != 0)
        assert(absdiff(vmsize(), v1) < NOISE);

    // Concurrent creation and freeing, beyond the pre-reserved space.
    pthread_t threads[6];
    for (int i = 0; i < 6; i++)
        pthread_create(&threads[i], NULL, churn, NULL);
    for (int i = 0; i < 6; i++)
        pthread_join(threads[i], NULL);
    if (v0 != 0)
        assert(absdiff(vmsize(), v1) < NOISE);

    lfi_free(engine);
    if (v0 != 0)
        assert(absdiff(vmsize(), v0) < NOISE);

    printf("OK\n");
    return 0;
}
