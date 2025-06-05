#include "lfi_core.h"
#include "lfi_arch.h"
#include "test.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

// clang-format: off

#if defined(LFI_ARCH_ARM64)

static uint8_t prog[] = {
    0x48, 0x05, 0x80, 0xd2, // mov x8, #42
    0xf6, 0x03, 0x1e, 0xaa, // mov x22, x30
    0xbe, 0x02, 0x40, 0xf9, // ldr x30, [x21]
    0xc0, 0x03, 0x3f, 0xd6, // blr x30
    0xbe, 0x42, 0x36, 0x8b, // add x30, x21, w22, uxtw
};

#elif defined(LFI_ARCH_X64)

static uint8_t prog[] = {
    0x48, 0xc7, 0xc0, 0x2a, 0x00, 0x00, 0x00, // mov $0x2a, %rax
    0x4c, 0x8d, 0x1d, 0x03, 0x00, 0x00, 0x00, // lea 0x3(%rip), %r11
    0x41, 0xff, 0x26,                         // jmp *(%r14)
};

#else

#error "architecture not supported"

#endif

// clang-format: on

int
main(void)
{
    size_t pagesize = getpagesize();
    // Initialize LFI.
    struct LFIEngine *engine = lfi_new(
        (struct LFIOptions) {
            .boxsize = gb(4),
            .pagesize = pagesize,
            .noverify = true,
            .verbose = true,
        },
        gb(256));

    if (!engine) {
        fprintf(stderr, "failed to create LFI engine: %s\n", lfi_errmsg());
        exit(1);
    }

    // Create a new sandbox.
    struct LFIBox *box = lfi_box_new(engine);
    if (!box) {
        fprintf(stderr, "failed to create LFI sandbox: %s\n", lfi_errmsg());
        exit(1);
    }

    struct LFIContext *ctx = lfi_ctx_new(box, NULL);
    if (!ctx) {
        fprintf(stderr, "failed to create LFI context: %s\n", lfi_errmsg());
        exit(1);
    }

    lfiptr p = lfi_box_mapany(box, pagesize, LFI_PROT_READ | LFI_PROT_WRITE, LFI_MAP_ANONYMOUS | LFI_MAP_PRIVATE, -1, 0);
    assert(p != (lfiptr) -1);
    assert(lfi_box_ptrvalid(box, p));

    lfi_box_copyto(box, p, prog, sizeof(prog));

    int r = lfi_box_mprotect(box, p, pagesize, LFI_PROT_READ | LFI_PROT_EXEC);
    assert(r == 0);

    lfi_ctx_run(ctx, p);

    lfi_ctx_free(ctx);

    lfi_box_free(box);

    lfi_free(engine);

    return 0;
}
