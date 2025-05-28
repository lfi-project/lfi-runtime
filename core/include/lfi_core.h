#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// A pointer inside an LFI sandbox. By default, an lfiptr is also a valid host
// pointer, but in some future configurations this may not necessarily hold.
typedef uintptr_t lfiptr;

// The LFIEngine tracks a large pool of virtual memory and manages the
// allocation of sandboxes from this pool.
struct LFIEngine;

// An LFIBox represents a region of the virtual address space reserved for a
// sandbox. There is one LFIBox object per sandbox.
struct LFIBox;

// An LFIContext represents a sandbox execution context, tracking the
// registers, thread pointer, stack, and a reference to the corresponding
// LFIBox. There may be multiple LFIContexts for one LFIBox, corresponding to
// multiple sandbox threads.
struct LFIContext;

struct LFIOptions {
    // Disable verification.
    bool noverify;

    // System page size
    size_t pagesize;

    // Display verbose debugging information using the system logger.
    bool verbose;

    // Enable stores-only mode (for verification).
    bool stores_only;
};

struct LFIBoxInfo {
    // Base address of the sandbox in the host address space.
    uintptr_t base;

    // Total size of the sandbox.
    size_t size;

    // Minimum accessible address.
    lfiptr min;

    // One past the maximum accessible address.
    lfiptr max;
};

// Creates a new LFI engine and reserve a certain amount of virtual address
// space for it. For future lfi_box_new calls to succeed, the reservation must
// be large enough to allocate at least one sandbox.
struct LFIEngine *
lfi_new(struct LFIOptions opts, size_t reserve);

// Frees the LFIEngine.
void
lfi_free(struct LFIEngine *lfi);

// Creates a new LFIBox by allocating a portion of the virtual address space.
struct LFIBox *
lfi_box_new(struct LFIEngine *lfi);

// Returns information about the sandbox (base and size).
struct LFIBoxInfo
lfi_box_info(struct LFIBox *box);

// Creates a new memory mapping in the sandbox at 'addr'. Returns -1 on
// failure. May run the verifier for executable pages. Disallows shared
// mappings to prevent double mapping pages as W and X.
lfiptr
lfi_box_mapat(struct LFIBox *box, lfiptr addr, size_t size, int prot, int flags,
    int fd, off_t off);

// Creates a new memory mapping in the sandbox at an arbitrary location,
// similar to mapat.
lfiptr
lfi_box_mapany(struct LFIBox *box, size_t size, int prot, int flags, int fd,
    off_t off);

// Unmaps a memory mapping in the sandbox. Returns -1 on failure.
int
lfi_box_munmap(struct LFIBox *box, lfiptr addr, size_t size);

// Changes the protection of a memory mapping inside the sandbox. May run the
// verifier for executable pages.
int
lfi_box_mprotect(struct LFIBox *box, lfiptr addr, size_t size, int prot);

// Returns whether a pointer is valid within the given sandbox.
bool
lfi_box_ptrvalid(struct LFIBox *box, lfiptr addr);

// Returns whether a buffer is valid within the given sandbox.
bool
lfi_box_bufvalid(struct LFIBox *box, lfiptr addr, size_t size);

// Copies data out from the given sandbox location.
void *
lfi_box_copyfm(struct LFIBox *box, void *dst, lfiptr src, size_t size);

// Copies data in to the given sandbox location.
lfiptr
lfi_box_copyto(struct LFIBox *box, lfiptr dst, void *src, size_t size);

// Frees all resources associated with box and deallocates its reservation in
// the LFIEngine.
void *
lfi_box_free(struct LFIEngine *lfi, struct LFIBox *box);

// Creates a new LFIContext for a given sandbox, along with a user-provided
// data pointer.
struct LFIContext *
lfi_ctx_new(struct LFIBox *box, void *ctxp);

// Begins executing the sandbox context.
int
lfi_ctx_run(struct LFIContext *ctx);

// Frees the sandbox context.
void *
lfi_ctx_free(struct LFIContext *ctx);

// Returns a pointer to the sandbox context's registers, which can be read and
// written.
struct LFIRegs *
lfi_ctx_regs(struct LFIContext *ctx);

// Causes the sandbox context to exit with a given exit code.
void
lfi_ctx_exit(struct LFIContext *ctx, int code);

// Returns the sandbox associated with the given context.
struct LFIBox *
lfi_ctx_box(struct LFIContext *ctx);

// Returns the error code for the current error.
int
lfi_errno(void);

// Returns a string description of the current error.
const char *
lfi_errmsg(void);

enum {
    LFI_ERR_NONE = 0,
    LFI_ERR_ALLOC = 1,
    LFI_ERR_BOXMAP = 2,
    LFI_ERR_RESERVE = 3,
};

#ifdef __cplusplus
}
#endif
