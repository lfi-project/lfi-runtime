#define _GNU_SOURCE
#include "sys.h"

#include "arch_sys.h"
#include "lfi_arch.h"
#include "lfi_core.h"
#include "linux.h"
#include "proc.h"

#include <assert.h>
#include <sys/ucontext.h>

#define ROUNDDOWN(X, K) ((X) & -(K))

static const int kRedzoneSize = 128;

static void
put32(uint8_t *p, uint32_t v)
{
    __builtin_memcpy(p, &v, 4);
}

static void
put64(uint8_t *p, uint64_t v)
{
    __builtin_memcpy(p, &v, 8);
}

static uint64_t
read64(uint8_t *p)
{
    uint64_t v;
    __builtin_memcpy(&v, p, 8);
    return v;
}

void
arch_syshandle(struct LFIContext *ctx)
{
    struct LFILinuxThread *t = lfi_ctx_data(ctx);
    assert(t);
    struct LFIRegs *regs = lfi_ctx_regs(ctx);

    uint64_t orig_rax = regs->rax;

    regs->rax = syshandle(t, orig_rax, regs->rdi, regs->rsi, regs->rdx,
        regs->r10, regs->r8, regs->r9);
}

// Deliver a signal to the currently-running sandbox context on this thread.
// Returns true if the signal was successfully delivered to a sandbox handler
// and returned, false if it fails to handle the forwarded signal.
bool
arch_forward_signal(struct LFIContext *ctx, int sig, siginfo_t *si,
    void *ucontext)
{
    if (!ctx)
        return false;
    struct LFILinuxThread *t = lfi_ctx_data(ctx);
    if (!t || !t->proc)
        return false;
    if (sig < 1 || sig >= LINUX_NSIG)
        return false;
    if (!t->proc->signals[sig].valid)
        return false;

    // NOTE: before adding support for other signals, their host sighandler must be
    // registered.
    assert(sig == LINUX_SIGSEGV || sig == LINUX_SIGILL);

    struct SigAction sighand = t->proc->signals[sig].entry;

    struct SigFrame sf = {0};

    ucontext_t *uctx = (ucontext_t *)ucontext;
    greg_t *host_regs = uctx->uc_mcontext.gregs;

    // Fill SigInfo.
    put32(sf.si.signo, sig);
    put32(sf.si.code, si->si_code);
    if (sig == LINUX_SIGILL || sig == LINUX_SIGFPE || sig == LINUX_SIGSEGV ||
        sig == LINUX_SIGBUS || sig == LINUX_SIGTRAP) {
        put64(sf.si.addr, (uintptr_t)si->si_addr);
    }

    // Fill UContext from interrupted sandbox registers.
    // TODO: sigmask, fpstate
    put64(sf.uc.r8,     host_regs[REG_R8]);
    put64(sf.uc.r9,     host_regs[REG_R9]);
    put64(sf.uc.r10,    host_regs[REG_R10]);
    put64(sf.uc.r11,    host_regs[REG_R11]);
    put64(sf.uc.r12,    host_regs[REG_R12]);
    put64(sf.uc.r13,    host_regs[REG_R13]);
    put64(sf.uc.r14,    host_regs[REG_R14]);
    put64(sf.uc.r15,    host_regs[REG_R15]);
    put64(sf.uc.rdi,    host_regs[REG_RDI]);
    put64(sf.uc.rsi,    host_regs[REG_RSI]);
    put64(sf.uc.rbp,    host_regs[REG_RBP]);
    put64(sf.uc.rbx,    host_regs[REG_RBX]);
    put64(sf.uc.rdx,    host_regs[REG_RDX]);
    put64(sf.uc.rax,    host_regs[REG_RAX]);
    put64(sf.uc.rcx,    host_regs[REG_RCX]);
    put64(sf.uc.rsp,    host_regs[REG_RSP]);
    put64(sf.uc.rip,    host_regs[REG_RIP]);
    put64(sf.uc.eflags, host_regs[REG_EFL]);

    // Compute signal frame location: below redzone, 16-byte aligned, with the
    // return address slot making rsp 8-mod-16 (x86-64 call convention).
    uintptr_t sp = (uintptr_t)host_regs[REG_RSP];
    sp -= kRedzoneSize; // skip redzone
    sp = ROUNDDOWN(sp, 16);
    sp -= sizeof(sf);
    assert((sp & 15) == 8);

    put64(sf.ret, sighand.restorer);
    put64(sf.uc.fpstate, sp + offsetof(struct SigFrame, fp));

    lfi_box_copyto(t->proc->box, sp, &sf, sizeof(sf));

    // Set up sandbox registers for handler invocation.
    struct LFIRegs *regs = lfi_ctx_regs(ctx);
    regs->rsp = sp;
    regs->rdi = sig;
    regs->rsi = sp + offsetof(struct SigFrame, si);
    regs->rdx = sp + offsetof(struct SigFrame, uc);

    uintptr_t saved_host_sp = regs->host_sp;

    // Run the sandbox signal handler. Returns when the handler calls
    // rt_sigreturn, which invokes lfi_ctx_exit.
    lfi_ctx_run(ctx, sighand.handler);

    regs->host_sp = saved_host_sp;

    // before signal frame re-read
    uintptr_t old_requested_rip = read64(sf.uc.rip);

    // After the handler's ret, rsp was incremented by 8 (ret popped the
    // restorer address). Read back the (potentially modified) signal frame.
    uintptr_t frame_sp = lfi_ctx_regs(ctx)->rsp - 8;
    lfi_box_copyfm(t->proc->box, &sf, frame_sp, sizeof(sf));

    // Validate RIP the handler wants the sandbox context to resume at.
    uintptr_t requested_rip = read64(sf.uc.rip);
    if (!lfi_box_ptrvalid(t->proc->box, requested_rip)) {
        ERROR("%s: requested RIP is invalid!\n", __PRETTY_FUNCTION__);
        return false;
    }
    if (old_requested_rip == requested_rip) {
        // resume at the faulting instruction, possibly unaligned
        LOG(t->proc->engine, "%s: returning to the faulting RIP!\n", __PRETTY_FUNCTION__);
    } else if ((requested_rip & 0x1f) != 0) {
        // RIP has changed by the sandbox, must aligned
        ERROR("%s: requested RIP is not bundle aligned!\n", __PRETTY_FUNCTION__);
        return false;
    }

    // Tell the kernel to resume sandbox execution at the validated RIP.
    uctx->uc_mcontext.gregs[REG_RIP] = requested_rip;

    // Restore sandbox context registers to pre-signal state for consistency.
    regs->r8  = host_regs[REG_R8];
    regs->r9  = host_regs[REG_R9];
    regs->r10 = host_regs[REG_R10];
    regs->r11 = host_regs[REG_R11];
    regs->r12 = host_regs[REG_R12];
    regs->r13 = host_regs[REG_R13];
    regs->r14 = host_regs[REG_R14];
    regs->r15 = host_regs[REG_R15];
    regs->rdi = host_regs[REG_RDI];
    regs->rsi = host_regs[REG_RSI];
    regs->rbp = host_regs[REG_RBP];
    regs->rbx = host_regs[REG_RBX];
    regs->rdx = host_regs[REG_RDX];
    regs->rax = host_regs[REG_RAX];
    regs->rcx = host_regs[REG_RCX];
    regs->rsp = host_regs[REG_RSP];

    return true;
}
