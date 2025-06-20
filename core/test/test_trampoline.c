#include "lfi_arch.h"
#include "lfi_core.h"
#include "test.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// Use -b to run the benchmark: ./core/test/test_trampoline.c.elf -b

#include "test_progs.h"

static inline long long unsigned
time_ns()
{
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts)) {
        exit(1);
    }
    return ((long long unsigned) ts.tv_sec) * 1000000000LLU +
        (long long unsigned) ts.tv_nsec;
}

static int
callback(int a)
{
    printf("callback got %d\n", a);
    return a;
}

static int
callback_bench(int a)
{
    return a;
}

int
main(int argc, char **argv)
{
    bool bench = false;
    if (argc >= 2 && strcmp(argv[1], "-b") == 0)
        bench = true;
    size_t pagesize = getpagesize();
    // Initialize LFI.
    struct LFIEngine *engine = lfi_new(
        (struct LFIOptions) {
            .boxsize = gb(4),
            .pagesize = pagesize,
            .verbose = true,
            .no_verify = false,
        },
        1);
    assert(engine);

    // Create a new sandbox.
    struct LFIBox *box = lfi_box_new(engine);
    assert(box);

    bool ok = lfi_box_cbinit(box);
    assert(ok);

    lfiptr p = lfi_box_mapany(box, pagesize, LFI_PROT_READ | LFI_PROT_WRITE,
        LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE, -1, 0);
    assert(p != (lfiptr) -1);
    assert(lfi_box_ptrvalid(box, p));

#if defined(LFI_ARCH_X64)
    // Set all bytes to trap instructions, since 0 does not pass verification.
    memset((void *) lfi_box_l2p(box, p), 0xcc, pagesize);
#endif

    lfiptr p_prog = lfi_box_copyto(box, p, prog, sizeof(prog));
    lfiptr p_prog_cb = lfi_box_copyto(box, p + sizeof(prog), prog_cb,
        sizeof(prog_cb));
    lfiptr p_ret = lfi_box_copyto(box, p + sizeof(prog) + sizeof(prog_cb), ret,
        sizeof(ret));

    lfi_box_init_ret(box, p_ret);

    int r = lfi_box_mprotect(box, p, pagesize, LFI_PROT_READ | LFI_PROT_EXEC);
    assert(r == 0);

    lfiptr stack = lfi_box_mapany(box, pagesize, LFI_PROT_READ | LFI_PROT_WRITE,
        LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE, -1, 0);

    struct LFIContext *ctx = lfi_ctx_new(box, NULL);
    assert(ctx);

#if defined(LFI_ARCH_X64)
    lfi_ctx_regs(ctx)->rsp = stack + pagesize;
#elif defined(LFI_ARCH_ARM64)
    lfi_ctx_regs(ctx)->sp = stack + pagesize;
#endif

    int x = LFI_INVOKE(box, &ctx, p_prog, int, (int, int), 10, 32);
    assert(x == 42);
    printf("add(%d, %d) = %d\n", 10, 32, x);

    void *box_callback = lfi_box_register_cb(box, callback);
    void *box_callback_bench = lfi_box_register_cb(box, callback_bench);

    x = LFI_INVOKE(box, &ctx, p_prog_cb, int, (int (*)(int), int), box_callback, 42);
    assert(x == 42);

    if (bench) {
        size_t iters = 100000000;
        long long unsigned start = time_ns();
        for (size_t i = 0; i < iters; i++) {
            LFI_INVOKE(box, &ctx, p_prog, int, (int, int), 10, 32);
        }
        long long unsigned elapsed = time_ns() - start;
        printf("time per invocation: %.1f ns\n",
            (float) elapsed / (float) iters);

        start = time_ns();
        for (size_t i = 0; i < iters; i++) {
            LFI_INVOKE(box, &ctx, p_prog_cb, int, (int (*)(int), int),
                box_callback_bench, 42);
        }
        elapsed = time_ns() - start;
        printf("time per invocation with callback: %.1f ns\n",
            (float) elapsed / (float) iters);
    }

    lfi_ctx_free(ctx);

    lfi_box_free(box);

    lfi_free(engine);

    return 0;
}
