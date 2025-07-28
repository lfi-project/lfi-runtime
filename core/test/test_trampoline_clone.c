#include "lfi_arch.h"
#include "lfi_core.h"
#include "test.h"

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <time.h>
#include <unistd.h>

// Use -b to run the benchmark: ./core/test/test_trampoline.c.elf -b

#include "test_progs.h"

static int
callback(int a)
{
    printf("callback got %d\n", a);
    return a;
}

static thread_local struct LFIContext *lib_ctx;

struct ThreadArgs {
    struct LFIBox *box;
    void *box_callback;
    lfiptr prog;
    lfiptr prog_cb;
};

static void *
thread_function(void *arg)
{
    struct ThreadArgs *args = (struct ThreadArgs *) arg;
    printf("new thread\n");
    // The first invocation on a separate thread will automatically initialize
    // lib_ctx using clone_cb.
    int x = LFI_INVOKE(args->box, &lib_ctx, args->prog, int, (int, int), 10,
        32);
    assert(x == 42);

    x = LFI_INVOKE(args->box, &lib_ctx, args->prog_cb, int, (int (*)(int), int),
        args->box_callback, 42);
    assert(x == 42);
    printf("OK\n");
    return NULL;
}

static void
run_on_new_thread(struct ThreadArgs *args)
{
    pthread_t thread;
    int r = pthread_create(&thread, NULL, thread_function, (void *) args);
    assert(r == 0);
    r = pthread_join(thread, NULL);
    assert(r == 0);
}

static pthread_key_t thread_key;

static void
thread_destructor(void *p)
{
    struct LFIContext *ctx = p;
    lfi_ctx_free(ctx);
}

static struct LFIContext *
clone_cb(struct LFIBox *box)
{
    size_t pagesize = getpagesize();

    struct LFIContext *ctx = lfi_ctx_new(box, NULL);
    assert(ctx);

    lfiptr stack = lfi_box_mapany(box, pagesize, LFI_PROT_READ | LFI_PROT_WRITE,
        LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE, -1, 0);

#if defined(LFI_ARCH_X64)
    lfi_ctx_regs(ctx)->rsp = stack + pagesize;
#elif defined(LFI_ARCH_ARM64)
    lfi_ctx_regs(ctx)->sp = stack + pagesize;
#endif
    printf("clone_cb initialized ctx: %p, stack: %lx\n", (void *) ctx, stack);

    pthread_setspecific(thread_key, ctx);

    return ctx;
}

int
main(void)
{
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

    lfi_set_clone_cb(engine, clone_cb);

    pthread_key_create(&thread_key, thread_destructor);

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

    lfi_box_init_ret(box);

    int r = lfi_box_mprotect(box, p, pagesize, LFI_PROT_READ | LFI_PROT_EXEC);
    assert(r == 0);

    lib_ctx = lfi_ctx_new(box, NULL);
    assert(lib_ctx);

    lfiptr stack = lfi_box_mapany(box, pagesize, LFI_PROT_READ | LFI_PROT_WRITE,
        LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE, -1, 0);

#if defined(LFI_ARCH_X64)
    lfi_ctx_regs(lib_ctx)->rsp = stack + pagesize;
#elif defined(LFI_ARCH_ARM64)
    lfi_ctx_regs(lib_ctx)->sp = stack + pagesize;
#endif

    void *box_callback = lfi_box_register_cb(box, (void *) callback);

    struct ThreadArgs args = (struct ThreadArgs) {
        .box = box,
        .box_callback = box_callback,
        .prog = p_prog,
        .prog_cb = p_prog_cb,
    };

    run_on_new_thread(&args);

    printf("main thread\n");

    int x = LFI_INVOKE(box, &lib_ctx, p_prog, int, (int, int), 10, 32);
    assert(x == 42);
    printf("add(%d, %d) = %d\n", 10, 32, x);

    x = LFI_INVOKE(box, &lib_ctx, p_prog_cb, int, (int (*)(int), int),
        box_callback, 42);
    assert(x == 42);

    lfi_ctx_free(lib_ctx);

    lfi_box_free(box);

    lfi_free(engine);

    return 0;
}
