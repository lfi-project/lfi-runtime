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
// sandbox's registers, thread pointer, stack, and a reference to the
// corresponding LFIBox. There may be multiple LFIContexts for one LFIBox,
// corresponding to multiple sandbox threads.
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

    // Do not initialize a sigaltstack automatically. For multithreaded
    // sandboxes, if a signal arrives during sandbox execution, the signal
    // handler must execute on a different stack from the sandbox stack,
    // because otherwise other sandbox threads could manipulate the stack that
    // the host signal handler executes on. The user is responsible for making
    // sure that signal handlers use SA_ONSTACK. If this option is enabled,
    // the user is also responsible for creating an alternate signal stack.
    bool no_init_sigaltstack;
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

    // One past the maximum address that may be marked as executable.
    lfiptr max_exec;
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

// We use MAP_EXECUTABLE to indicate that the memory mapping should be
// allocated from the region of the sandbox where code pages may be allocated.
// For certain architectures/configurations of LFI pages may only be marked as
// executable if they are in a certain region of the sandbox (e.g., not in the
// top 128MiB of the sandbox).
#define LFI_MAP_EXECUTABLE 0x1000

// Creates a new LFI engine and reserve enough virtual address space for 'n'
// sandboxes.
struct LFIEngine *
lfi_new(struct LFIOptions opts, size_t nsandboxes);

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

// Return the engine that this box belongs to.
struct LFIEngine *
lfi_box_engine(struct LFIBox *box);

// Set this box's associated user data.
void
lfi_box_setdata(struct LFIBox *box, void *userdata);

// Return this box's associated user data.
void *
lfi_box_data(struct LFIBox *box);

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
// similar to mapat, but does not perform verification (use with caution).
lfiptr
lfi_box_mapany_noverify(struct LFIBox *box, size_t size, int prot, int flags,
    int fd, off_t off);

// Unmaps a memory mapping in the sandbox. Returns -1 on failure.
int
lfi_box_munmap(struct LFIBox *box, lfiptr addr, size_t size);

// Changes the protection of a memory mapping inside the sandbox. May run the
// verifier for executable pages.
int
lfi_box_mprotect(struct LFIBox *box, lfiptr addr, size_t size, int prot);

// Same as lfi_box_mprotect but does not perform verification (use with
// caution).
int
lfi_box_mprotect_noverify(struct LFIBox *box, lfiptr addr, size_t size,
    int prot);

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

// Deprecated.
bool
lfi_box_cbinit(struct LFIBox *box);

// Register fn as a callback. Returns the function pointer that should be
// passed to the sandbox code in order to call 'fn'. Returns NULL if there are
// no more callback slots available or if callback initialization failed.
void *
lfi_box_register_cb(struct LFIBox *box, void *fn);

// Similar to lfi_box_register_cb, but expects the arguments and return values
// to be accessed through the struct LFIContext associated with the callback
// invoker.
void *
lfi_box_register_cb_struct(struct LFIBox *box, void *fn);

// Unregister fn as a callback.
void
lfi_box_unregister_cb(struct LFIBox *box, void *fn);

// Frees all resources associated with box and deallocates its reservation in
// the LFIEngine in which it was allocated.
void
lfi_box_free(struct LFIBox *box);

// Creates a new LFIContext for a given sandbox, along with a user-provided
// data pointer.
struct LFIContext *
lfi_ctx_new(struct LFIBox *box, void *userdata);

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

// Initialize the sandbox's return function. This will create a function inside
// the sandbox that calls lfi_ret, and will be used as the return address for
// LFI_INVOKE calls into the sandbox. The new memory allocated in the sandbox
// will be marked as PROT_EXEC.
void
lfi_box_init_ret(struct LFIBox *box);

// Register the sandbox's return function as retaddr. This is similar to
// lfi_box_init_ret, but rather than dynamically creating the lfi_ret function
// inside the sandbox, this assumes that it already exists and simply registers
// it. This is useful if the platform has PROT_EXEC memory restrictions.
void
lfi_box_register_ret(struct LFIBox *box, lfiptr retaddr);

// Register a clone callback. This callback will be called when transferring
// control to a NULL LFI context through the LFI trampoline (e.g., via
// LFI_INVOKE). The callback should return a newly initialized LFI context that
// will be used for the invocation.
void
lfi_set_clone_cb(struct LFIEngine *engine,
    struct LFIContext *(*clone_cb)(struct LFIBox *) );

// Return the currently active sandbox context.
struct LFIContext *
lfi_cur_ctx(void);

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
    // Error creating sigaltstack.
    LFI_ERR_SIGALTSTACK = 5,
    // Error with protection keys.
    LFI_ERR_PKU = 6,
};

struct LFIInvokeInfo {
    struct LFIContext **ctx;
    lfiptr targetfn;
    struct LFIBox *box;
};

#ifdef __cplusplus
#define lfi_thread_local thread_local
#else
#define lfi_thread_local _Thread_local
#endif

extern lfi_thread_local struct LFIInvokeInfo lfi_invoke_info __asm__(
    "lfi_invoke_info");

// Direct trampoline that loads arguments/return values from the struct
// LFIContext that is used in the invocation.
void
lfi_trampoline_struct(void) __asm__("lfi_trampoline_struct");

#define LFI_X(x, y)  x##y
#define LFI_XX(x, y) LFI_X(x, y)

#define LFI_INVOKE_NAMED(name, box_, ctxp, fn, ret_type, args, ...) \
    __extension__({                                                 \
        ret_type LFI_XX(__lfi_trampoline, name)                     \
            args __asm__("lfi_trampoline");                         \
        lfi_invoke_info = (struct LFIInvokeInfo) {                  \
            .ctx = ctxp,                                            \
            .targetfn = fn,                                         \
            .box = box_,                                            \
        };                                                          \
        LFI_XX(__lfi_trampoline, name)(__VA_ARGS__);                \
    })

// LFI_INVOKE(struct LFIContext **ctx, lfiptr fn, return type, arg types, ...)
#define LFI_INVOKE(box_, ctxp, fn, ret_type, args, ...) \
    LFI_INVOKE_NAMED(__COUNTER__, box_, ctxp, fn, ret_type, args, __VA_ARGS__)

#ifdef __cplusplus
}
#endif
