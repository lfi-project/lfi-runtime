#include "align.h"
#include "elfdefs.h"
#include "host.h"
#include "lfi_arch.h"
#include "lfi_core.h"
#include "linux.h"
#include "proc.h"

#include <assert.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#ifdef HAVE_GETAUXVAL
#include <sys/auxv.h>
static unsigned long
host_getauxval(unsigned long type)
{
    switch (type) {
    case LINUX_AT_HWCAP:
        return getauxval(AT_HWCAP);
    case LINUX_AT_HWCAP2:
        return getauxval(AT_HWCAP2);
    }
    return 0;
}
#else
static unsigned long
host_getauxval(unsigned long type)
{
    // TODO: non-Linux support
    return 0;
}
#endif

// First TID, to avoid using low TID numbers.
#define BASE_TID 10000
// Cap on the host-allocated SafeStack safe stack size. Holds return
// addresses + provably-safe locals — small per frame, so 1MB is plenty.
// The unsafe stack still gets sized by LFILinuxOptions.stacksize.
#define SAFE_STACK_SIZE (1024 * 1024)
// Maximum number of argv arguments.
#define ARGV_MAX 1024
// Maxmimum number of envp arguments.
#define ENVP_MAX 1024
// Maximum length of an argv string.
#define ARGV_MAXLEN 1024
// Maxmimum length of an envp string.
#define ENVP_MAXLEN 1024

static int
next_tid(struct LFILinuxProc *p)
{
    return BASE_TID +
        atomic_fetch_add_explicit(&p->total_thread_count, 1,
            memory_order_relaxed);
}

struct Auxv {
    uint64_t a_type;
    uint64_t a_val;
};

struct AuxvList {
    struct Auxv at_secure;
    struct Auxv at_base;
    struct Auxv at_phdr;
    struct Auxv at_phnum;
    struct Auxv at_phent;
    struct Auxv at_entry;
    struct Auxv at_execfn;
    struct Auxv at_pagesz;
    struct Auxv at_hwcap;
    struct Auxv at_hwcap2;
    struct Auxv at_random;
    struct Auxv at_flags;
    struct Auxv at_uid;
    struct Auxv at_euid;
    struct Auxv at_gid;
    struct Auxv at_egid;
    struct Auxv at_sysinfo;
    struct Auxv at_sysinfo_ehdr;
    struct Auxv at_null;
};

// Lays out argv, envp, and auxv at the top of the thread's in-sandbox stack
// (which under SafeStack is the unsafe stack).
//
// The layout will look something like this:
// | argc | argv[0] .. 0 | envp[0] .. 0 | auxv | rand | argv/envp strings |
// ^ returned lfiptr
//
// Returns the sandbox-relative pointer to argc; that is the value musl's
// _start expects in %rdi (it walks argv/envp/auxv from there).
static lfiptr
stack_init(struct LFILinuxThread *t, int argc, const char **argv,
    const char **envp)
{
    argc = MIN(argc, ARGV_MAX);

    // Calculate how many bytes we will need to copy from argv and envp.
    size_t strs_len = 0;
    size_t nargv = 0;
    for (int i = 0; i < argc; i++) {
        if (!argv[i])
            break;
        size_t len = strnlen(argv[i], ARGV_MAXLEN);
        assert(argv[i][len] == '\0');
        strs_len += len + 1;
        nargv++;
    }
    size_t nenvp = 0;
    for (int i = 0; i < ENVP_MAX; i++) {
        if (!envp[i])
            break;
        size_t len = strnlen(envp[i], ENVP_MAXLEN);
        assert(envp[i][len] == '\0');
        strs_len += len + 1;
        nenvp++;
    }

    // Store the sandbox pointers to each argv/envp string.
    lfiptr box_argv[nargv + 1];
    lfiptr box_envp[nenvp + 1];
    box_argv[nargv] = 0;
    box_envp[nenvp] = 0;

    // We make this 16-byte aligned so that we can use this as a base offset
    // for also storing aligned values.
    lfiptr strs_start = truncp(t->stack + t->stack_size - strs_len, 16);
    size_t count = 0;
    struct LFIBox *box = t->proc->box;
    // Copy the argv and envp strings into the sandbox.
    for (size_t i = 0; i < nargv; i++) {
        size_t len = strnlen(argv[i], ARGV_MAXLEN) + 1;
        box_argv[i] = lfi_box_copyto(box, strs_start + count, argv[i], len);
        count += len;
    }
    for (size_t i = 0; i < nenvp; i++) {
        size_t len = strnlen(envp[i], ARGV_MAXLEN) + 1;
        box_envp[i] = lfi_box_copyto(box, strs_start + count, envp[i], len);
        count += len;
    }

    // Create 16 random bytes for AT_RANDOM.
    char random[16];
    ssize_t r = host_getrandom(&random[0], sizeof(random), 0);
    if (r != sizeof(random)) {
        LOG(t->proc->engine,
            "warning: getrandom for AT_RANDOM returned %ld (less than 16)", r);
    }
    lfiptr rand_start = strs_start - sizeof(random);
    // Copy into the sandbox.
    lfi_box_copyto(box, rand_start, &random[0], sizeof(random));

    // Create the auxiliary vector.
    struct AuxvList auxv = {
        (struct Auxv) { LINUX_AT_SECURE, 0 },
        (struct Auxv) { LINUX_AT_BASE, t->proc->elfinfo.ldbase },
        (struct Auxv) { LINUX_AT_PHDR,
            t->proc->elfinfo.elfbase + t->proc->elfinfo.elfphoff },
        (struct Auxv) { LINUX_AT_PHNUM, t->proc->elfinfo.elfphnum },
        (struct Auxv) { LINUX_AT_PHENT, t->proc->elfinfo.elfphentsize },
        (struct Auxv) { LINUX_AT_ENTRY, t->proc->elfinfo.elfentry },
        (struct Auxv) { LINUX_AT_EXECFN, box_argv[0] },
        (struct Auxv) { LINUX_AT_PAGESZ,
            lfi_opts(t->proc->engine->engine).pagesize },
        (struct Auxv) { LINUX_AT_HWCAP, host_getauxval(LINUX_AT_HWCAP) },
        (struct Auxv) { LINUX_AT_HWCAP2, host_getauxval(LINUX_AT_HWCAP2) },
        (struct Auxv) { LINUX_AT_RANDOM, rand_start },
        (struct Auxv) { LINUX_AT_FLAGS, 0 },
        (struct Auxv) { LINUX_AT_UID, 1000 },
        (struct Auxv) { LINUX_AT_EUID, 1000 },
        (struct Auxv) { LINUX_AT_GID, 1000 },
        (struct Auxv) { LINUX_AT_EGID, 1000 },
        (struct Auxv) { LINUX_AT_SYSINFO, 0 },
        (struct Auxv) { LINUX_AT_SYSINFO_EHDR, 0 },
        (struct Auxv) { LINUX_AT_NULL, 0 },
    };

    uint64_t box_argc = argc;
    // Calculate where the sp should go (enough space from rand_start to store
    // argc/argv/envp/auxv). Align down to 16 bytes: the SysV ABI requires the
    // stack pointer to be 16-byte aligned at process entry, and (more
    // critically here) the SafeStack pass computes per-frame unsafe SPs as
    // base - FrameSize, where FrameSize is rounded up to the frame's max
    // alignment — so a misaligned base propagates into every alloca and breaks
    // any movaps-style 16-byte access. Up to 8 bytes between auxv and the
    // random area become unused padding.
    lfiptr stack_start = (rand_start - sizeof(box_argc) - sizeof(box_argv) -
        sizeof(box_envp) - sizeof(auxv)) & ~(lfiptr) 15;
    // Copy each item onto the stack.
    lfiptr next = lfi_box_copyto(box, stack_start, &box_argc,
                      sizeof(box_argc)) +
        sizeof(box_argc);
    next = lfi_box_copyto(box, next, box_argv, sizeof(box_argv)) +
        sizeof(box_argv);
    next = lfi_box_copyto(box, next, box_envp, sizeof(box_envp)) +
        sizeof(box_envp);
    lfi_box_copyto(box, next, &auxv, sizeof(auxv));

    return stack_start;
}

static void
sp_init(struct LFILinuxThread *t, lfiptr sp)
{
#if defined(LFI_ARCH_ARM64)
    lfi_ctx_regs(t->ctx)->sp = sp;
#elif defined(LFI_ARCH_X64)
    lfi_ctx_regs(t->ctx)->rsp = sp;
#elif defined(LFI_ARCH_RISCV64)
    lfi_ctx_regs(t->ctx)->sp = sp;
#else
#error "invalid arch"
#endif
}

EXPORT void
lfi_thread_reload(struct LFILinuxThread *t, int argc, const char **argv,
    const char **envp)
{
    memset((void *) lfi_box_l2p(t->proc->box, t->stack), 0, t->stack_size);
    if (t->safe_stack)
        memset(t->safe_stack, 0, t->safe_stack_size);

    lfiptr argv_area = stack_init(t, argc, argv, envp);
    *lfi_ctx_regs(t->ctx) = (struct LFIRegs) { 0 };
    lfi_ctx_regs_init(t->ctx);

#ifdef CTXREG
    if (t->safe_stack) {
        // Place the argv-area pointer at the top of the safe stack; musl's
        // _start loads it via `mov (%rsp), %rdi` under __LFI__.
        uint64_t *safe_top = (uint64_t *) ((uintptr_t) t->safe_stack +
            t->safe_stack_size - sizeof(uint64_t));
        *safe_top = (uint64_t) argv_area;
        lfi_ctx_set_ustack(t->ctx, (uint64_t) argv_area);
        sp_init(t, (lfiptr) (uintptr_t) safe_top);
    } else {
        sp_init(t, argv_area);
    }
#else
    sp_init(t, argv_area);
#endif
}

EXPORT struct LFILinuxThread *
lfi_thread_new(struct LFILinuxProc *proc, int argc, const char **argv,
    const char **envp)
{
    struct LFILinuxThread *t = calloc(sizeof(struct LFILinuxThread), 1);
    if (!t)
        goto err1;
    t->proc = proc;
    t->ctx = lfi_ctx_new(proc->box, t);
    if (!t->ctx)
        goto err2;
    t->tid = next_tid(proc);
    pthread_mutex_init(&t->lk_ready, NULL);
    pthread_cond_init(&t->cond_ready, NULL);
    list_init(&t->threads_elem);

    size_t stacksize = proc->engine->opts.stacksize;
    // In-sandbox stack. Holds argc/argv/envp/auxv at the top and (under
    // SafeStack) is the unsafe stack the SafeStack pass allocates from.
    t->stack = lfi_box_mapat(proc->box, proc->box_info.max - stacksize,
        stacksize, LFI_PROT_READ | LFI_PROT_WRITE,
        LFI_MAP_PRIVATE | LFI_MAP_ANONYMOUS, -1, 0);
    if (t->stack == (lfiptr) -1) {
        LOG(proc->engine, "failed to map stack");
        goto err3;
    }
    t->stack_size = stacksize;

    lfiptr argv_area = stack_init(t, argc, argv, envp);

#ifdef CTXREG
    // Host-allocated safe stack outside the sandbox, used by SafeStack as the
    // backward-edge CFI stack. Sandbox code cannot reach this region through
    // any sandbox-bounded memory access; it is only addressable via %rsp.
    //
    // The safe stack only holds return addresses and locals the SafeStack
    // pass proves are address-safe — typically a few dozen bytes per frame.
    // Cap it independently of `stacksize` (which sizes the unsafe stack and
    // therefore needs to match what an OS would give a normal process).
    size_t safe_stacksize = stacksize < SAFE_STACK_SIZE
                                ? stacksize
                                : SAFE_STACK_SIZE;
    void *ss = mmap(NULL, safe_stacksize, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ss == MAP_FAILED) {
        LOG(proc->engine, "failed to map safe stack");
        goto err4;
    }
    t->safe_stack = ss;
    t->safe_stack_size = safe_stacksize;

    // The top of the safe stack holds a single 8-byte slot: the sandbox
    // pointer to argc. musl's _start reads it with `mov (%rsp), %rdi` (under
    // __LFI__), then walks argv/envp/auxv normally.
    uint64_t *safe_top = (uint64_t *) ((uintptr_t) ss + safe_stacksize -
        sizeof(uint64_t));
    *safe_top = (uint64_t) argv_area;

    // Publish the unsafe stack pointer so SafeStack-instrumented code can
    // load/store it via 24(%r15). Initial value is the start of the argv
    // area; SafeStack-allocated frames grow downward from there.
    lfi_ctx_set_ustack(t->ctx, (uint64_t) argv_area);

    sp_init(t, (lfiptr) (uintptr_t) safe_top);
#else
    sp_init(t, argv_area);
#endif

    lfi_box_mark_original(t->proc->box);

    return t;
#ifdef CTXREG
err4:
    lfi_box_munmap(t->proc->box, t->stack, stacksize);
#endif
err3:
    lfi_ctx_free(t->ctx);
err2:
    free(t);
err1:
    return NULL;
}

// Create a new thread cloned from the given thread.
struct LFILinuxThread *
thread_clone(struct LFILinuxThread *t)
{
    struct LFILinuxThread *new_t = calloc(sizeof(struct LFILinuxThread), 1);
    if (!new_t)
        goto err1;
    struct LFIContext *ctx = lfi_ctx_new(t->proc->box, new_t);
    if (!ctx)
        goto err2;
    new_t->ctx = ctx;
    new_t->proc = t->proc;
    new_t->tid = next_tid(t->proc);
    pthread_mutex_init(&new_t->lk_ready, NULL);
    pthread_cond_init(&new_t->cond_ready, NULL);
    list_init(&new_t->threads_elem);
    if (new_t->tid == -1)
        goto err2;
    // Copy all registers.
    *lfi_ctx_regs(new_t->ctx) = *lfi_ctx_regs(t->ctx);

    return new_t;

err2:
    free(new_t);
err1:
    return NULL;
}

EXPORT int
lfi_thread_run(struct LFILinuxThread *p)
{
    return lfi_ctx_run(p->ctx, p->proc->entry);
}

EXPORT void
lfi_thread_free(struct LFILinuxThread *t)
{
    // Unmap the stack (if created). If this thread was spawned by the sandbox
    // calling clone (fake or real), the stack will have been created by the
    // sandbox and not by us, so we don't have to free it.
    if (t->stack) {
        size_t stacksize = t->proc->engine->opts.stacksize;
        lfi_box_munmap(t->proc->box, t->stack, stacksize);
    }
    // Unmap the host-allocated safe stack (only present on the main thread
    // when SafeStack support is enabled).
    if (t->safe_stack)
        munmap(t->safe_stack, t->safe_stack_size);
    // Free pthread object (if created).
    if (t->pthread)
        free(t->pthread);
    // Free the context.
    lfi_ctx_free(t->ctx);
    // Free the thread.
    free(t);
}

EXPORT struct LFIContext **
lfi_thread_ctxp(struct LFILinuxThread *t)
{
    return &t->ctx;
}
