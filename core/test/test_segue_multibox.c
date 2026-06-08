#include "lfi_arch.h"
#include "lfi_core.h"
#include "test.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// This test exercises running multiple sandboxes on a single thread, which is
// the case that the segue_cache_gs option must handle correctly (the old
// segue_single_sandbox option assumed a single sandbox). It is meaningful only
// with Segue (x86-64), where the sandbox base lives in %gs.
//
// Each sandbox runs a tiny program that loads a value through the Segue base
// (mov %gs:(%edi), %rax). The host stores a distinct value in each box, so if
// %gs ever holds the wrong base (e.g. a wrgsbase was incorrectly skipped) one
// box reads the other's memory and the assertions fail. A register-only program
// (like the add() in test_trampoline.c) would NOT catch such a bug, since it
// never touches %gs.

#if defined(LFI_ARCH_X64)

// mov %gs:(%edi), %rax ; pop %r11 ; and $~0x1f,%r11d ; or %r14,%r11 ; jmp *%r11
// padded to a 32-byte bundle with nops.
static uint8_t gsload[] = {
    0x65, 0x67, 0x48, 0x8b, 0x07, // mov %gs:(%edi), %rax
    0x41, 0x5b,                   // pop %r11
    0x41, 0x83, 0xe3, 0xe0,       // and $0xffffffe0, %r11d
    0x4d, 0x09, 0xf3,             // or %r14, %r11
    0x41, 0xff, 0xe3,             // jmp *%r11
    0x66, 0x66, 0x2e, 0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00, // nop
    0x0f, 0x1f, 0x40, 0x00,       // nop
};
_Static_assert(sizeof(gsload) % 32 == 0, "gsload not bundle-aligned");

struct boxsetup {
    struct LFIBox *box;
    struct LFIContext *ctx;
    lfiptr code;
    uint32_t off; // offset of the stored value within the box
};

static struct boxsetup
setup(struct LFIEngine *engine, uint64_t value)
{
    size_t pagesize = getpagesize();
    struct LFIBox *box = lfi_box_new(engine);
    assert(box);

    // Data page: store the box-specific value at its start.
    lfiptr data = lfi_box_mapany(box, pagesize, LFI_PROT_READ | LFI_PROT_WRITE,
        LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE, -1, 0);
    assert(data != (lfiptr) -1);
    *(uint64_t *) lfi_box_l2p(box, data) = value;

    // Code page: the gsload program.
    lfiptr code = lfi_box_mapany(box, pagesize, LFI_PROT_READ | LFI_PROT_WRITE,
        LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE, -1, 0);
    assert(code != (lfiptr) -1);
    memset((void *) lfi_box_l2p(box, code), 0xcc, pagesize);
    lfi_box_copyto(box, code, gsload, sizeof(gsload));
    int r = lfi_box_mprotect(box, code, pagesize, LFI_PROT_READ | LFI_PROT_EXEC);
    assert(r == 0);

    lfi_box_init_ret(box);

    lfiptr stack = lfi_box_mapany(box, pagesize, LFI_PROT_READ | LFI_PROT_WRITE,
        LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE, -1, 0);
    assert(stack != (lfiptr) -1);

    struct LFIContext *ctx = lfi_ctx_new(box, NULL);
    assert(ctx);
    lfi_ctx_regs(ctx)->rsp = stack + pagesize;

    struct LFIBoxInfo info = lfi_box_info(box);
    return (struct boxsetup) {
        .box = box,
        .ctx = ctx,
        .code = code,
        .off = (uint32_t) (data - info.base),
    };
}

int
main(void)
{
    struct LFIEngine *engine = lfi_new(
        (struct LFIOptions) {
            .boxsize = gb(4),
            .pagesize = getpagesize(),
            .verbose = true,
            .no_verify = false,
        },
        2);
    assert(engine);

    uint64_t valA = 0xAAAAAAAAAAAAAAAAULL;
    uint64_t valB = 0xBBBBBBBBBBBBBBBBULL;

    struct boxsetup a = setup(engine, valA);
    struct boxsetup b = setup(engine, valB);

    uint64_t ra = LFI_INVOKE(a.box, &a.ctx, a.code, uint64_t, (uint32_t), a.off);
    uint64_t rb = LFI_INVOKE(b.box, &b.ctx, b.code, uint64_t, (uint32_t), b.off);
    printf("A=%016llx B=%016llx\n", (unsigned long long) ra,
        (unsigned long long) rb);
    assert(ra == valA);
    assert(rb == valB);

    // Alternate between the two boxes on the same thread. With segue_cache_gs
    // this forces a %gs base switch on every call; a missed wrgsbase would make
    // one box observe the other's value.
    for (int i = 0; i < 100000; i++) {
        uint64_t xa = LFI_INVOKE(a.box, &a.ctx, a.code, uint64_t, (uint32_t),
            a.off);
        uint64_t xb = LFI_INVOKE(b.box, &b.ctx, b.code, uint64_t, (uint32_t),
            b.off);
        assert(xa == valA);
        assert(xb == valB);
    }

    // Repeated same-box calls (cache hits) interleaved with switches.
    for (int i = 0; i < 10000; i++) {
        assert(LFI_INVOKE(a.box, &a.ctx, a.code, uint64_t, (uint32_t), a.off) ==
            valA);
        assert(LFI_INVOKE(a.box, &a.ctx, a.code, uint64_t, (uint32_t), a.off) ==
            valA);
        assert(LFI_INVOKE(b.box, &b.ctx, b.code, uint64_t, (uint32_t), b.off) ==
            valB);
    }

    printf("segue multibox: OK\n");

    lfi_ctx_free(a.ctx);
    lfi_ctx_free(b.ctx);
    lfi_box_free(a.box);
    lfi_box_free(b.box);
    lfi_free(engine);
    return 0;
}

#else

int
main(void)
{
    // Segue is x86-64 only; nothing to test elsewhere.
    return 0;
}

#endif
