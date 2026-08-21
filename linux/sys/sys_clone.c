#include "sys/sys.h"
#include "trampoline.h"

#include <stdatomic.h>

static bool
isfork(uint64_t flags)
{
    uint64_t allowed = LINUX_CLONE_CHILD_SETTID | LINUX_CLONE_CHILD_CLEARTID;
    return (flags & ~allowed) == LINUX_SIGCHLD ||
        (flags & ~allowed) ==
        (LINUX_CLONE_VM | LINUX_CLONE_VFORK | LINUX_SIGCHLD);
}

static void
threadspawn_fake(struct LFILinuxThread *t)
{
    struct LFIRegs *regs = lfi_ctx_regs(t->ctx);
    uintptr_t entry;
#if defined(LFI_ARCH_X64)
    entry = regs->r11;
#elif defined(LFI_ARCH_ARM64)
    entry = regs->x30;
#elif defined(LFI_ARCH_RISCV64)
    entry = regs->ra;
#else
#error "invalid arch"
#endif

    int code = lfi_ctx_run(t->ctx, entry);
    assert(code == 0);
}

static void *
threadspawn(void *arg)
{
    struct LFILinuxThread *t = (struct LFILinuxThread *) arg;
    struct LFIRegs *regs = lfi_ctx_regs(t->ctx);
    uintptr_t entry;
#if defined(LFI_ARCH_X64)
    entry = regs->r11;
#elif defined(LFI_ARCH_ARM64)
    entry = regs->x30;
#elif defined(LFI_ARCH_RISCV64)
    entry = regs->ra;
#else
#error "invalid arch"
#endif

    lfi_ctx_run(t->ctx, entry);

    struct LFILinuxProc *proc = t->proc;
    int tid = t->tid;
    lfi_thread_free(t);
    lock(&proc->lk_threads);
    proc->active_threads--;
    pthread_cond_signal(&proc->cond_threads);
    LOG(proc->engine, "thread %d exited", tid);
    unlock(&proc->lk_threads);
    return NULL;
}

static int
spawn(struct LFILinuxThread *p, uint64_t flags, uint64_t stack, uint64_t ptidp,
    uint64_t ctidp, uint64_t tls, uint64_t func)
{
    if ((flags & 0xff) != 0 && (flags & 0xff) != LINUX_SIGCHLD) {
        LOG(p->proc->engine, "unsupported clone signal: %x",
            (unsigned) flags & 0xff);
        return -LINUX_EINVAL;
    }
    flags &= ~0xff;
    unsigned allowed = LINUX_CLONE_THREAD | LINUX_CLONE_VM | LINUX_CLONE_FS |
        LINUX_CLONE_FILES | LINUX_CLONE_SIGHAND | LINUX_CLONE_SETTLS |
        LINUX_CLONE_PARENT_SETTID | LINUX_CLONE_CHILD_CLEARTID |
        LINUX_CLONE_CHILD_SETTID | LINUX_CLONE_SYSVSEM;
    unsigned required = LINUX_CLONE_THREAD | LINUX_CLONE_VM | LINUX_CLONE_FS |
        LINUX_CLONE_FILES | LINUX_CLONE_SIGHAND;
    unsigned ignored = LINUX_CLONE_DETACHED | LINUX_CLONE_IO;
    flags &= ~ignored;

    if (flags & ~allowed) {
        LOG(p->proc->engine, "disallowed clone flags: %lx",
            (unsigned long) (flags & ~allowed));
        return -LINUX_EINVAL;
    }
    if ((flags & required) != required) {
        LOG(p->proc->engine, "missing required clone flags: %lx",
            (unsigned long) required);
        return -LINUX_EINVAL;
    }

    if (!ptrcheck(p, stack))
        return -LINUX_EFAULT;
    if (!ptrcheck(p, ctidp))
        return -LINUX_EFAULT;
    if (!ptrcheck(p, ptidp))
        return -LINUX_EFAULT;

    _Atomic(int) *ctid = (_Atomic(int) *) ptrhost(p, ctidp);
    _Atomic(int) *ptid = (_Atomic(int) *) ptrhost(p, ptidp);

    // A clone issued on the proc's clone_ctx is not a real thread: it is the
    // lazy-attachment path, which hands the new context back to
    // lfi_linux_clone_cb so a host thread can run on it. Everything else gets
    // a pthread the runtime owns.
    bool attach = p->ctx == p->proc->clone_ctx;

    // Both paths start out running under lfi_ctx_run below.
    struct LFILinuxThread *p2 = thread_clone(p, LFI_THREAD_RUNTIME);
    if (!p2) {
        return -LINUX_EAGAIN;
    }
    int tid = p2->tid;

    struct LFIRegs *regs = lfi_ctx_regs(p2->ctx);
    if (flags & LINUX_CLONE_SETTLS) {
        lfi_ctx_set_tp(p2->ctx, tls);
    }
    if (flags & LINUX_CLONE_CHILD_CLEARTID) {
        p2->ctidp = ctidp;
    }
    if (flags & LINUX_CLONE_CHILD_SETTID) {
        atomic_store_explicit(ctid, p2->tid, memory_order_release);
    }

    LOG(p->proc->engine, "sys_clone(%lx, %lx, %lx, %lx, %lx) = %d",
        (long) flags, (long) stack, (long) ptidp, (long) ctidp, (long) tls,
        (int) p2->tid);

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
        threadspawn_fake(p2);
        lfi_invoke_info.ctx = old.ctx;
        lfi_invoke_info.targetfn = old.targetfn;
        lfi_invoke_info.box = old.box;

        // The lfi_ctx_run frame is gone and from here the context is only ever
        // entered through the trampoline, by whichever host thread the clone
        // callback hands it to.
        p2->owner = LFI_THREAD_HOST;
        new_ctx = p2->ctx;
    } else {
        pthread_t *thread = malloc(sizeof(pthread_t));
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        p2->pthread = thread;
        LOG(p->proc->engine, "creating new thread: %d", tid);

        lock(&p->proc->lk_threads);
        list_make_first(&p->proc->threads, &p2->threads_elem);
        p->proc->active_threads++;
        unlock(&p->proc->lk_threads);

        int err = pthread_create(thread, &attr, threadspawn, p2);
        pthread_attr_destroy(&attr);
        if (err) {
            lock(&p->proc->lk_threads);
            list_remove(&p->proc->threads, &p2->threads_elem);
            p->proc->active_threads--;
            pthread_cond_signal(&p->proc->cond_threads);
            unlock(&p->proc->lk_threads);

            lfi_thread_free(p2);
            return -LINUX_EAGAIN;
        }
    }

    if (flags & LINUX_CLONE_PARENT_SETTID) {
        atomic_store_explicit(ptid, tid, memory_order_release);
    }
    return tid;
}

int
sys_clone(struct LFILinuxThread *t, uint64_t flags, uint64_t stack,
    uint64_t ptid, uint64_t ctid, uint64_t tls, uint64_t func)
{
    if (isfork(flags)) {
        LOG(t->proc->engine, "received fork/vfork request: not supported");
        return -LINUX_ENOSYS;
    }
    return spawn(t, flags, stack, ptid, ctid, tls, func);
}
