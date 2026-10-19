#include "full/sys/sys.h"
#include "lock.h"

#include <pthread.h>
#include <stdalign.h>

static bool
isfork(uint64_t flags)
{
    uint64_t allowed = LINUX_CLONE_CHILD_SETTID | LINUX_CLONE_CHILD_CLEARTID;
    return (flags & ~allowed) == LINUX_SIGCHLD ||
        (flags & ~allowed) ==
        (LINUX_CLONE_VM | LINUX_CLONE_VFORK | LINUX_SIGCHLD);
}

// Returns the address a new thread starts at: the instruction after the clone
// syscall, which the syscall saved as its return address.
static uintptr_t
thread_entry(struct LFILinuxThread *t)
{
    struct LFIRegs *regs = lfi_ctx_regs(t->ctx);
#if defined(LFI_ARCH_X64)
    return regs->r11;
#elif defined(LFI_ARCH_ARM64)
    return regs->x30;
#elif defined(LFI_ARCH_RISCV64)
    return regs->ra;
#else
#error "invalid arch"
#endif
}

// Runs a thread cloned on the proc's clone_ctx until it hands its context back
// with the LFI pause syscall. Returns false if it left the sandbox some other
// way.
static bool
threadspawn_fake(struct LFILinuxThread *t)
{
    lfi_ctx_run(t->ctx, thread_entry(t));
    return t->paused;
}

static void *
threadspawn(void *arg)
{
    struct LFILinuxThread *t = (struct LFILinuxThread *) arg;

    lfi_ctx_run(t->ctx, thread_entry(t));

    struct LFILinuxProc *proc = t->proc;
    struct LFIFullProc *full = proc->full;
    int tid = t->full->tid;
    lock(&full->lk_threads);
    lfi_thread_free(t);
    full->active_threads--;
    pthread_cond_signal(&full->cond_threads);
    LOG(proc->engine, "thread %d exited", tid);
    unlock(&full->lk_threads);
    return NULL;
}

// Writes a TID to an address the sandbox passed to clone. Like Linux, the
// write is skipped if the address is no longer mapped writable.
static void
settid(struct LFILinuxThread *t, lfiptr p, int tid)
{
    lfi_box_copyto_checked(t->proc->box, p, &tid, sizeof(tid));
}

static uintptr_t
spawn(struct LFILinuxThread *p, uint64_t flags, lfiptr stack, lfiptr ptidp,
    lfiptr ctidp, uint64_t tls)
{
    struct LFILinuxProc *proc = p->proc;
    struct LFIFullProc *full = proc->full;

    if ((flags & 0xff) != 0 && (flags & 0xff) != LINUX_SIGCHLD) {
        LOG(proc->engine, "unsupported clone signal: %x",
            (unsigned) flags & 0xff);
        return -LINUX_EINVAL;
    }
    flags &= ~0xff;
    uint64_t allowed = LINUX_CLONE_THREAD | LINUX_CLONE_VM | LINUX_CLONE_FS |
        LINUX_CLONE_FILES | LINUX_CLONE_SIGHAND | LINUX_CLONE_SETTLS |
        LINUX_CLONE_PARENT_SETTID | LINUX_CLONE_CHILD_CLEARTID |
        LINUX_CLONE_CHILD_SETTID | LINUX_CLONE_SYSVSEM;
    uint64_t required = LINUX_CLONE_THREAD | LINUX_CLONE_VM | LINUX_CLONE_FS |
        LINUX_CLONE_FILES | LINUX_CLONE_SIGHAND;
    uint64_t ignored = LINUX_CLONE_DETACHED | LINUX_CLONE_IO;
    flags &= ~ignored;

    if (flags & ~allowed) {
        LOG(proc->engine, "disallowed clone flags: %lx",
            (unsigned long) (flags & ~allowed));
        return -LINUX_EINVAL;
    }
    if ((flags & required) != required) {
        LOG(proc->engine, "missing required clone flags: %lx",
            (unsigned long) required);
        return -LINUX_EINVAL;
    }

    // The TID pointers are only checked when a flag says to use them, since
    // Linux ignores them otherwise.
    if (!ptrcheck(p, stack))
        return -LINUX_EFAULT;
    if ((flags & (LINUX_CLONE_CHILD_SETTID | LINUX_CLONE_CHILD_CLEARTID)) &&
        !bufcheck(p, ctidp, sizeof(int), alignof(int)))
        return -LINUX_EFAULT;
    if ((flags & LINUX_CLONE_PARENT_SETTID) &&
        !bufcheck(p, ptidp, sizeof(int), alignof(int)))
        return -LINUX_EFAULT;

    // A clone issued on the proc's clone_ctx is not a real thread: it is the
    // lazy-attachment path, which hands the new context back to
    // full_clone_thread so a host thread can run on it. Everything else gets
    // a pthread the runtime owns.
    bool attach = p->ctx == full->clone_ctx;

    // Both paths start out running under lfi_ctx_run below.
    struct LFILinuxThread *p2 = thread_clone(p, LFI_THREAD_RUNTIME);
    if (!p2)
        return -LINUX_EAGAIN;
    int tid = p2->full->tid;

    struct LFIRegs *regs = lfi_ctx_regs(p2->ctx);
    if (flags & LINUX_CLONE_SETTLS)
        lfi_ctx_set_tp(p2->ctx, tls);
    if (flags & LINUX_CLONE_CHILD_CLEARTID)
        p2->full->ctidp = ctidp;
    // Like Linux, both TIDs are in place before the new thread runs.
    if (flags & LINUX_CLONE_CHILD_SETTID)
        settid(p, ctidp, tid);
    if (flags & LINUX_CLONE_PARENT_SETTID)
        settid(p, ptidp, tid);

    LOG(proc->engine, "sys_clone(%lx, %lx, %lx, %lx, %lx) = %d", (long) flags,
        (long) stack, (long) ptidp, (long) ctidp, (long) tls, tid);

#if defined(LFI_ARCH_X64)
    regs->rax = 0;
    regs->rsp = stack;
#elif defined(LFI_ARCH_ARM64)
    regs->x0 = 0;
    regs->sp = stack;
#elif defined(LFI_ARCH_RISCV64)
    regs->a0 = 0;
    regs->sp = stack;
#else
#error "invalid arch"
#endif

    if (attach) {
        // TODO: rethink whether this save/restore is necessary and how it
        // interacts with signals?
        struct LFIInvokeInfo old = lfi_invoke_info;
        bool paused = threadspawn_fake(p2);
        lfi_invoke_info.ctx = old.ctx;
        lfi_invoke_info.targetfn = old.targetfn;
        lfi_invoke_info.box = old.box;

        if (!paused) {
            LOG(proc->engine, "cloned thread %d exited before pausing", tid);
            lfi_thread_free(p2);
            return -LINUX_EAGAIN;
        }

        // The lfi_ctx_run frame is gone and from here the context is only ever
        // entered through the trampoline, by whichever host thread the clone
        // callback hands it to.
        p2->owner = LFI_THREAD_HOST;
        full_new_ctx = p2->ctx;
    } else {
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        LOG(proc->engine, "creating new thread: %d", tid);

        lock(&full->lk_threads);
        full->active_threads++;
        unlock(&full->lk_threads);

        pthread_t thread;
        int err = pthread_create(&thread, &attr, threadspawn, p2);
        pthread_attr_destroy(&attr);
        if (err) {
            lock(&full->lk_threads);
            full->active_threads--;
            pthread_cond_signal(&full->cond_threads);
            unlock(&full->lk_threads);

            lfi_thread_free(p2);
            return -LINUX_EAGAIN;
        }
    }

    return tid;
}

uintptr_t
full_sys_clone(struct LFILinuxThread *t, uint64_t flags, lfiptr stack,
    lfiptr ptidp, lfiptr ctidp, uint64_t tls)
{
    if (isfork(flags)) {
        LOG(t->proc->engine, "received fork/vfork request: not supported");
        return -LINUX_ENOSYS;
    }
    return spawn(t, flags, stack, ptidp, ctidp, tls);
}
