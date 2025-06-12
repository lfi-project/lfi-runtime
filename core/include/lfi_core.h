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
    // System page size.
    size_t pagesize;

    // Sandbox size.
    size_t boxsize;

    // Display verbose debugging information using the system logger.
    bool verbose;

    // Enable stores-only mode (for verification).
    bool stores_only;

    // Disable verification (unsafe).
    bool no_verify;

    // Allow WX mappings (unsafe).
    //
    // If allow_wx is enabled you must also enable no_verify.
    bool allow_wx;
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

#define LFI_PROT_NONE  0
#define LFI_PROT_READ  1
#define LFI_PROT_WRITE 2
#define LFI_PROT_EXEC  4

// CAUTION: the runtime must be careful allowing mappings with MAP_SHARED,
// since such a page could be mapped twice: once as W and once as X. Thus, the
// user of liblfi should not allow the sandbox to aliased WX mappings via
// MAP_SHARED, and liblfi does not internally enforce this restriction.
#define LFI_MAP_SHARED    1
#define LFI_MAP_PRIVATE   2
#define LFI_MAP_FIXED     16
#define LFI_MAP_ANONYMOUS 32

// Creates a new LFI engine and reserve a certain amount of virtual address
// space for it. For future lfi_box_new calls to succeed, the reservation must
// be large enough to allocate at least one sandbox.
struct LFIEngine *
lfi_new(struct LFIOptions opts, size_t reserve);

// Frees the LFIEngine.
void
lfi_free(struct LFIEngine *lfi);

// Sets the system call handler for the given engine.
void
lfi_sys_handler(struct LFIEngine *engine,
    void (*sys_handler)(struct LFIContext *ctx));

// Returns the options for this LFIEngine.
struct LFIOptions
lfi_opts(struct LFIEngine *lfi);

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

// Registers an existing memory mapping in the sandbox at 'addr'. This is used
// in cases where the system loader has already mmapped a region in the sandbox
// before liblfi has started. Returns -1 on failure. May run the verifier for
// executable pages.
lfiptr
lfi_box_mapat_register(struct LFIBox *box, lfiptr addr, size_t size, int prot,
    int flags, int fd, off_t off);

// Creates a new memory mapping in the sandbox at an arbitrary location,
// similar to mapat.
lfiptr
lfi_box_mapany(struct LFIBox *box, size_t size, int prot, int flags, int fd,
    off_t off);

// Creates a new memory mapping in the sandbox at an arbitrary location,
// similar to mapat, but does not perform verification (USE WITH CAUTION).
lfiptr
lfi_box_mapany_noverify(struct LFIBox *box, size_t size, int prot, int flags, int fd,
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
lfi_box_copyto(struct LFIBox *box, lfiptr dst, const void *src, size_t size);

// Use of the following two functions assumes that the sandbox and host share
// an address space.
//
// Converts a sandbox pointer to a host pointer. Assumes l is valid.
uintptr_t
lfi_box_l2p(struct LFIBox *box, lfiptr l);

// Converts a host pointer to a sandbox pointer.
uintptr_t
lfi_box_p2l(struct LFIBox *box, uintptr_t p);

// Frees all resources associated with box and deallocates its reservation in
// the LFIEngine in which it was allocated.
void
lfi_box_free(struct LFIBox *box);

// Creates a new LFIContext for a given sandbox, along with a user-provided
// data pointer.
struct LFIContext *
lfi_ctx_new(struct LFIBox *box, void *ctxp);

// Returns the ctxp user data pointer for ctx.
void *
lfi_ctx_data(struct LFIContext *ctx);

// Begins executing the sandbox context.
int
lfi_ctx_run(struct LFIContext *ctx, uintptr_t entry);

// Frees the sandbox context.
void
lfi_ctx_free(struct LFIContext *ctx);

// Returns a pointer to the sandbox context's registers, which can be read and
// written.
struct LFIRegs *
lfi_ctx_regs(struct LFIContext *ctx);

// Initializes registers to values that maintain sandbox invariants
// (essentially, this sets all sandbox-reserved registers to the sandbox base
// address).
void
lfi_ctx_regs_init(struct LFIContext *ctx);

// Causes the sandbox context to exit with a given exit code.
void
lfi_ctx_exit(struct LFIContext *ctx, int code);

// Returns the sandbox associated with the given context.
struct LFIBox *
lfi_ctx_box(struct LFIContext *ctx);

// Set the context's ret function to ret, which should be a function inside the
// sandbox that calls the lfi_ret runtime call. Initializing this is necessary
// if you use LFI_INVOKE, but otherwise is not necessary.
void
lfi_ctx_init_ret(struct LFIContext *ctx, lfiptr ret);

// Return the context's ret function.
lfiptr
lfi_ctx_ret(struct LFIContext *ctx);

// Returns the error code for the current error.
int
lfi_errno(void);

// Returns a string description of the current error.
const char *
lfi_errmsg(void);

// Possible values returned by lfi_error().
enum {
    // No error.
    LFI_ERR_NONE = 0,
    // Allocation error (malloc failed).
    LFI_ERR_ALLOC = 1,
    // Error inside libboxmap (sandbox allocator).
    LFI_ERR_BOXMAP = 2,
    // Error reserving memory for a sandbox.
    LFI_ERR_RESERVE = 3,
    // Error inside libmmap (sandbox memory mapper).
    LFI_ERR_MMAP = 4,
};

struct LFIInvokeInfo {
    struct LFIContext *ctx;
    lfiptr targetfn;
    lfiptr retfn;
};

extern _Thread_local struct LFIInvokeInfo lfi_invoke_info;

extern const void *lfi_trampoline_addr;

#define LFI_INVOKE(ctx, fn, ret_type, args, ...)                              \
    ({                                                                        \
        lfi_invoke_info = (struct LFIInvokeInfo) {                            \
            .ctx = ctx,                                                       \
            .targetfn = fn,                                                   \
            .retfn = lfi_ctx_ret(ctx),                                        \
        };                                                                    \
        ret_type(*_trampoline) args = (ret_type(*) args) lfi_trampoline_addr; \
        _trampoline(__VA_ARGS__);                                             \
    })

#ifdef __cplusplus
}
#endif
