#include "lfi_arch.h"
#include "lfi_core.h"
#include "test.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define BENCHMARK 0

#if defined(LFI_ARCH_ARM64)

static uint8_t prog[] = {
    0x00, 0x00, 0x01, 0x8b, // add x0, x0, x1
    0xc0, 0x03, 0x5f, 0xd6, // ret
};

static uint8_t prog_cb[] = {
    0xe2, 0x03, 0x00, 0xaa, // mov x2, x0
    0xe0, 0x03, 0x01, 0x2a, // mov w0, w1
    0xb2, 0x42, 0x22, 0x8b, // add x18, x21, w2, uxtw
    0x40, 0x02, 0x1f, 0xd6, // br x18
};

static uint8_t ret[] = {
    0xf6, 0x03, 0x1e, 0xaa, // mov x22, x30
    0xbe, 0x0e, 0x40, 0xf9, // ldr x30, [x21, #24]
    0xc0, 0x03, 0x3f, 0xd6, // blr x30
    0xbe, 0x42, 0x36, 0x8b, // add x30, x21, w22, uxtw
};

#elif defined(LFI_ARCH_X64)

static uint8_t prog[] = {
    0x48, 0x01, 0xfe,       // add %rdi, %rsi
    0x48, 0x89, 0xf0,       // mov %rsi, %rax
    0x41, 0x5b,             // pop %r11
    0x41, 0x83, 0xe3, 0xe0, // and $0xffffffe0, %r11d
    0x4d, 0x09, 0xf3,       // or %r14, %r11
    0x41, 0xff, 0xe3,       // jmp *%r11
    0x66, 0x2e, 0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00, // nop
    0x0f, 0x1f, 0x40, 0x00,                                     // nop
};

_Static_assert(sizeof(prog) % 32 == 0, "end of prog is not bundle-aligned");

static uint8_t prog_cb[] = {
    // clang-format off
    0x48, 0x89, 0xf8, // mov %rdi, %rax
    0x89, 0xf7,       // mov %esi, %edi
    0x83, 0xe0, 0xe0, // and $0xffffffe0, %eax
    0x4c, 0x09, 0xf0, // or %r14, %rax
    0xff, 0xe0,       // jmp *%rax
    0x66, 0x2e, 0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00, // nop
    0x0f, 0x1f, 0x40, 0x00, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,       // nop/int3
    // clang-format on
};

_Static_assert(sizeof(prog_cb) % 32 == 0,
    "end of prog_cb is not bundle-aligned");

static uint8_t ret[] = {
    0x4c, 0x8d, 0x1d, 0x04, 0x00, 0x00, 0x00, // lea 0x4(%rip), %r11
    0x41, 0xff, 0x66, 0x18,                   // jmp *0x18(%r14)
};

#else

#error "architecture not supported"

#endif

#if BENCHMARK
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
#endif

static int
callback(int a)
{
#if !BENCHMARK
    printf("callback got %d\n", a);
#endif
    return a;
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
            .no_verify = true,
        },
        gb(256));
    assert(engine);

    // Create a new sandbox.
    struct LFIBox *box = lfi_box_new(engine);
    assert(box);

    bool ok = lfi_box_cbinit(box);
    assert(ok);

    struct LFIContext *ctx = lfi_ctx_new(box, NULL);
    assert(ctx);

    lfiptr p = lfi_box_mapany(box, pagesize, LFI_PROT_READ | LFI_PROT_WRITE,
        LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE, -1, 0);
    assert(p != (lfiptr) -1);
    assert(lfi_box_ptrvalid(box, p));

    lfiptr p_prog = lfi_box_copyto(box, p, prog, sizeof(prog));
    lfiptr p_prog_cb = lfi_box_copyto(box, p + sizeof(prog), prog_cb,
        sizeof(prog_cb));
    lfiptr p_ret = lfi_box_copyto(box, p + sizeof(prog) + sizeof(prog_cb), ret,
        sizeof(ret));

    lfi_ctx_init_ret(ctx, p_ret);

    int r = lfi_box_mprotect(box, p, pagesize, LFI_PROT_READ | LFI_PROT_EXEC);
    assert(r == 0);

    lfiptr stack = lfi_box_mapany(box, pagesize, LFI_PROT_READ | LFI_PROT_WRITE,
        LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE, -1, 0);

#if defined(LFI_ARCH_X64)
    lfi_ctx_regs(ctx)->rsp = stack + pagesize;
#elif defined(LFI_ARCH_ARM64)
    lfi_ctx_regs(ctx)->sp = stack + pagesize;
#endif

    int x = LFI_INVOKE(ctx, p_prog, int, (int, int), 10, 32);
    assert(x == 42);
    printf("add(%d, %d) = %d\n", 10, 32, x);

    void *box_callback = lfi_box_register_cb(box, callback);

    x = LFI_INVOKE(ctx, p_prog_cb, int, (int (*)(int), int), box_callback, 42);
    assert(x == 42);

#if BENCHMARK
    size_t iters = 100000000;
    long long unsigned start = time_ns();
    for (size_t i = 0; i < iters; i++) {
        LFI_INVOKE(ctx, p_prog, int, (int, int), 10, 32);
    }
    long long unsigned elapsed = time_ns() - start;
    printf("time per invocation: %.1f ns\n", (float) elapsed / (float) iters);

    start = time_ns();
    for (size_t i = 0; i < iters; i++) {
        LFI_INVOKE(ctx, p_prog_cb, int, (int (*)(int), int), box_callback, 42);
    }
    elapsed = time_ns() - start;
    printf("time per invocation with callback: %.1f ns\n",
        (float) elapsed / (float) iters);
#endif

    lfi_ctx_free(ctx);

    lfi_box_free(box);

    lfi_free(engine);

    return 0;
}
