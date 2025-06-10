#include "align.h"
#include "lfi_arch.h"
#include "lfi_core.h"
#include "linux.h"
#include "proc.h"

#include <elf.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>

// First TID, to avoid using low TID numbers.
#define BASE_TID 10000
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
        atomic_fetch_add_explicit(&p->threads, 1, memory_order_relaxed);
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

// Initializes the thread's stack with argv, envp, and auxv.
//
// The layout will look something like this:
// | argc | argv[0] .. 0 | envp[0] .. 0 | auxv | rand | argv/envp strings |
// ^ sp
static lfiptr
stack_init(struct LFILinuxThread *t, int argc, char **argv, char **envp)
{
    argc = MIN(argc, ARGV_MAX);

    // Calculate how many bytes we will need to copy from argv and envp.
    size_t strs_len = 0;
    size_t nargv = 0;
    for (int i = 0; i < argc; i++) {
        if (!argv[i])
            break;
        strs_len += strnlen(argv[i], ARGV_MAXLEN);
        nargv++;
    }
    size_t nenvp = 0;
    for (int i = 0; i < ENVP_MAX; i++) {
        if (!envp[i])
            break;
        strs_len += strnlen(envp[i], ENVP_MAXLEN);
        nenvp++;
    }

    // Store the sandbox pointers to each argv/envp string.
    lfiptr box_argv[nargv];
    lfiptr box_envp[nenvp];

    // We make this 16-byte aligned so that we can use this as a base offset
    // for also storing aligned values.
    lfiptr strs_start = truncp(t->stack + t->stack_size - strs_len, 16);
    size_t count = 0;
    struct LFIBox *box = t->proc->box;
    // Copy the argv and envp strings into the sandbox.
    for (int i = 0; i < nargv; i++) {
        size_t len = strnlen(argv[i], ARGV_MAXLEN);
        box_argv[i] = lfi_box_copyto(box, strs_start + count, argv[i], len);
        count += len;
    }
    for (int i = 0; i < nenvp; i++) {
        size_t len = strnlen(envp[i], ARGV_MAXLEN);
        box_envp[i] = lfi_box_copyto(box, strs_start + count, envp[i], len);
        count += len;
    }

    // Create 16 random bytes for AT_RANDOM.
    char random[16];
    ssize_t r = getrandom(&random[0], sizeof(random), 0);
    if (r != sizeof(random)) {
        LOG(t->proc->engine,
            "warning: getrandom for AT_RANDOM returned %ld (less than 16)", r);
    }
    lfiptr rand_start = strs_start - sizeof(random);
    // Copy into the sandbox.
    lfi_box_copyto(box, rand_start, &random[0], sizeof(random));

    // Create the auxiliary vector.
    struct AuxvList auxv = {
        (struct Auxv) { AT_SECURE, 0 },
        (struct Auxv) { AT_BASE, t->proc->elfinfo.ldbase },
        (struct Auxv) { AT_PHDR,
            t->proc->elfinfo.elfbase + t->proc->elfinfo.elfphoff },
        (struct Auxv) { AT_PHNUM, t->proc->elfinfo.elfphnum },
        (struct Auxv) { AT_PHENT, t->proc->elfinfo.elfphentsize },
        (struct Auxv) { AT_ENTRY, t->proc->elfinfo.elfentry },
        (struct Auxv) { AT_EXECFN, box_argv[0] },
        (struct Auxv) { AT_PAGESZ, lfi_opts(t->proc->engine->engine).pagesize },
        (struct Auxv) { AT_HWCAP, 0 },
        (struct Auxv) { AT_HWCAP2, 0 },
        (struct Auxv) { AT_RANDOM, rand_start },
        (struct Auxv) { AT_FLAGS, 0 },
        (struct Auxv) { AT_UID, 1000 },
        (struct Auxv) { AT_EUID, 1000 },
        (struct Auxv) { AT_GID, 1000 },
        (struct Auxv) { AT_EGID, 1000 },
        (struct Auxv) { AT_SYSINFO, 0 },
        (struct Auxv) { AT_SYSINFO_EHDR, 0 },
        (struct Auxv) { AT_NULL, 0 },
    };

    uint64_t box_argc = argc;
    // Calculate where the sp should go (enough space from rand_start to store
    // argc/argv/envp/auxv).
    lfiptr stack_start = rand_start - sizeof(box_argc) - sizeof(box_argv) -
        sizeof(box_envp) - sizeof(auxv);
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

EXPORT struct LFILinuxThread *
lfi_thread_new(struct LFILinuxProc *proc, int argc, char **argv, char **envp)
{
    struct LFILinuxThread *t = calloc(sizeof(struct LFILinuxThread), 1);
    if (!t)
        goto err1;
    t->proc = proc;
    t->ctx = lfi_ctx_new(proc->box, t);
    if (!t->ctx)
        goto err2;
    t->tid = next_tid(proc);

    size_t stacksize = proc->engine->opts.stacksize;
    t->stack = lfi_box_mapat(proc->box, proc->box_info.max - stacksize,
        stacksize, LFI_PROT_READ | LFI_PROT_WRITE,
        LFI_MAP_PRIVATE | LFI_MAP_ANONYMOUS, -1, 0);
    if (t->stack == (lfiptr) -1) {
        LOG(proc->engine, "failed to map stack");
        goto err3;
    }
    t->stack_size = stacksize;

    if (!stack_init(t, argc, argv, envp))
        goto err4;

    return t;
err4:
    lfi_box_munmap(proc->box, t->stack, stacksize);
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
    if (new_t->tid == -1)
        goto err2;
    // Copy all registers.
    *lfi_ctx_regs(new_t->ctx) = *lfi_ctx_regs(t->ctx);
    lfi_ctx_regs(new_t->ctx)->tp = 0;

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
    // Unmap the stack.
    size_t stacksize = t->proc->engine->opts.stacksize;
    lfi_box_munmap(t->proc->box, t->stack, stacksize);
    // Free the context.
    lfi_ctx_free(t->ctx);
    // Free the thread.
    free(t);
}
