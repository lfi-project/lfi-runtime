#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

enum LFIBoxType {
    LFI_BOX_FULL,
    LFI_BOX_STORES,
};

struct LFIVOptions {
    // Sandbox type (full, stores-only).
    enum LFIBoxType box;

    // Callback to print a null-terminated error message if verification fails.
    void (*err)(char *msg, size_t size);
};

struct LFIVerifier {
    // Verifier options.
    struct LFIVOptions opts;

    // Verify the given code buffer, assuming a start address of vaddr.
    bool (*verify)(char *code, size_t size, uintptr_t vaddr, struct LFIVOptions *opts);
};

// Run the arm64 verifier.
bool
lfiv_verify_arm64(char *code, size_t size, uintptr_t addr, struct LFIVOptions *opts);

// Run the x64 verifier.
bool
lfiv_verify_x64(char *code, size_t size, uintptr_t addr, struct LFIVOptions *opts);

// Run the riscv64 verifier.
bool
lfiv_verify_riscv64(char *code, size_t size, uintptr_t addr, struct LFIVOptions *opts);

static inline bool
lfiv_verify(struct LFIVerifier *v, char *code, size_t size, uintptr_t addr)
{
    if (!v->verify)
        return false;
    return v->verify(code, size, addr, &v->opts);
}
