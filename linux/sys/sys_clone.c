#include "sys/sys.h"

#include <stdatomic.h>
#include <signal.h>

// Receiver for SIGLFI when the main thread wants to kill child threads.
static void
thread_signal(int sig)
{
    assert(sig == SIGLFI);

    struct LFIContext *ctx = lfi_cur_ctx();
    assert(ctx);
    struct LFILinuxThread *t = lfi_ctx_data(ctx);
    assert(t);
    sys_exit(t, 0);
}

static bool
isfork(uint64_t flags)
{
    uint64_t allowed = LINUX_CLONE_CHILD_SETTID | LINUX_CLONE_CHILD_CLEARTID;
    return (flags & ~allowed) == LINUX_SIGCHLD ||
        (flags & ~allowed) ==
        (LINUX_CLONE_VM | LINUX_CLONE_VFORK | LINUX_SIGCHLD);
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
#else
#error "invalid arch"
#endif

    stack_t ss;
    ss.ss_sp = malloc(SIGSTKSZ);
    if (!ss.ss_sp) {
        LOG(t->proc->engine, "failed to allocate signal stack");
        goto end;
    }
    ss.ss_size = SIGSTKSZ;
    ss.ss_flags = 0;

    // Register alternate stack.
    if (sigaltstack(&ss, NULL) == -1) {
        LOG(t->proc->engine, "failed to register signal stack");
        goto end;
    }

    struct sigaction sa = {0};
    sa.sa_handler = &thread_signal;
    sa.sa_flags = SA_ONSTACK;
    if (sigaction(SIGLFI, &sa, NULL) == -1) {
        LOG(t->proc->engine, "failed to register SIGLFI handler");
        goto end;
    }

    // Signal to the parent that the thread is now ready.
    pthread_mutex_lock(&t->lk_ready);
    t->ready = true;
    pthread_cond_signal(&t->cond_ready);
    unlock(&t->lk_ready);

    lfi_ctx_run(t->ctx, entry);

end:
    lock(&t->lk_ready);
    lock(&t->proc->lk_threads);
    t->proc->active_threads--;
    pthread_cond_signal(&t->proc->cond_threads);
    LOG(t->proc->engine, "thread %d exited", t->tid);
    unlock(&t->proc->lk_threads);
    unlock(&t->lk_ready);
    lfi_thread_free(t);
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

    struct LFILinuxThread *p2 = thread_clone(p);
    if (!p2) {
        return -LINUX_EAGAIN;
    }
    int tid = p2->tid;

    struct LFIRegs *regs = lfi_ctx_regs(p2->ctx);
    if (flags & LINUX_CLONE_SETTLS) {
        regs->tp = tls;
    }
    if (flags & LINUX_CLONE_CHILD_CLEARTID) {
        p2->ctidp = ctidp;
    }
    if (flags & LINUX_CLONE_CHILD_SETTID) {
        atomic_store_explicit(ctid, p2->tid, memory_order_release);
    }

    LOG(p->proc->engine, "sys_clone(%lx, %lx, %lx, %lx, %lx) = %d", flags,
        stack, ptidp, ctidp, tls, p2->tid);

#if defined(LFI_ARCH_X64)
    regs->rax = 0;
    regs->rsp = stack;
#elif defined(LFI_ARCH_ARM64)
    regs->x0 = 0;
    regs->sp = stack;
#else
#error "invalid arch"
#endif

    pthread_t *thread = malloc(sizeof(pthread_t));
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    p2->pthread = thread;
    LOG(p->proc->engine, "creating new thread: %d", tid);
    int err = pthread_create(thread, &attr, threadspawn, p2);
    pthread_attr_destroy(&attr);
    if (err) {
        lfi_thread_free(p2);
        return -LINUX_EAGAIN;
    }

    // Wait until thread is ready.
    lock(&p2->lk_ready);
    while (!p2->ready) {
        pthread_cond_wait(&p2->cond_ready, &p2->lk_ready);
    }
    lock(&p->proc->lk_threads);
    list_make_first(&p->proc->threads, &p2->threads_elem);
    p->proc->active_threads++;
    unlock(&p->proc->lk_threads);
    unlock(&p2->lk_ready);

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
