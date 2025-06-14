#include "sys/sys.h"

#include <stdatomic.h>

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
    lfi_ctx_run(t->ctx, entry);
    LOG(t->proc->engine, "thread %d exited", t->tid);
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
    int err = pthread_create(thread, &attr, threadspawn, p2);
    pthread_attr_destroy(&attr);
    if (err) {
        lfi_thread_free(p2);
        return -LINUX_EAGAIN;
    }

    if (flags & LINUX_CLONE_PARENT_SETTID) {
        atomic_store_explicit(ptid, p2->tid, memory_order_release);
    }
    return p2->tid;
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
