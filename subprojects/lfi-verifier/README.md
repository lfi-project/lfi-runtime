# LFI Verifier

## Installation

```
meson setup build
cd build
ninja
```

This produces the `liblfiv.a` library, along with a `lfi-verify` tool that can
be used on ELF binaries.

## Usage

For actual usage of the verifier, you should link with the `liblfiv.a` library,
which provides the following API:

```c
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
```
