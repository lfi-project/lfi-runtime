#include "sys.h"

#include "arch_sys.h"
#include "lfi_arch.h"
#include "lfi_core.h"
#include "linux.h"
#include "proc.h"
#include "lock.h"

#include <assert.h>
#include <sys/ucontext.h>

#define ROUNDDOWN(X, K) ((X) & -(K))

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

    regs->x0 = syshandle(t, regs->x8, regs->x0, regs->x1, regs->x2, regs->x3,
        regs->x4, regs->x5);
}

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

    {
        LOCK_WITH_DEFER(&t->proc->lk_signals, lk_signals);
        if (!t->proc->signals[sig].valid)
            return false;
    }

    // NOTE: before adding support for other signals, their host sighandler must be
    // registered.
    assert(sig == LINUX_SIGSEGV || sig == LINUX_SIGILL || sig == LINUX_SIGBUS);

    struct SigAction sighand = t->proc->signals[sig].entry;

    struct SigFrame sf = {0};

    ucontext_t *uctx = (ucontext_t *)ucontext;

    // Fill SigInfo.
    put32(sf.si.signo, sig);
    put32(sf.si.code, si->si_code);
    if (sig == LINUX_SIGILL || sig == LINUX_SIGFPE || sig == LINUX_SIGSEGV ||
        sig == LINUX_SIGBUS || sig == LINUX_SIGTRAP) {
        put64(sf.si.addr, (uintptr_t)si->si_addr);
    }

    put64(sf.uc.fault_address, uctx->uc_mcontext.fault_address);
    __builtin_memcpy(sf.uc.regs, uctx->uc_mcontext.regs, sizeof(sf.uc.regs));
    put64(sf.uc.sp, uctx->uc_mcontext.sp);
    put64(sf.uc.pc, uctx->uc_mcontext.pc);
    put64(sf.uc.pstate, uctx->uc_mcontext.pstate);
    // TODO: __reserved for fpsimd, esr, sve

    uintptr_t sp = ROUNDDOWN((uintptr_t)uctx->uc_mcontext.sp, 16);
    sp -= sizeof(sf);
    if (!lfi_box_ptrvalid(t->proc->box, sp)) {
        ERROR("%s: signal handler may access memory outside sandbox region.!\n", __PRETTY_FUNCTION__);
        return false;
    }

    lfi_box_copyto(t->proc->box, sp, &sf, sizeof(sf));

    // Set up sandbox registers for handler invocation.
    struct LFIRegs *regs = lfi_ctx_regs(ctx);
    regs->sp = sp;
    regs->x0 = sig;
    regs->x1 = sp + offsetof(struct SigFrame, si);
    regs->x2 = sp + offsetof(struct SigFrame, uc);
    regs->x30 = sighand.restorer; // SAFETY: lfiptr_t restorer

    uintptr_t saved_host_sp = regs->host_sp;

    // Run the sandbox signal handler. Returns when the handler calls
    // rt_sigreturn, which invokes lfi_ctx_exit. siglongjmp is not supported.
    lfi_ctx_run(ctx, sighand.handler);

    regs->host_sp = saved_host_sp;

    // before signal frame re-read
    uintptr_t old_requested_pc = read64(sf.uc.pc);

    uintptr_t frame_sp = lfi_ctx_regs(ctx)->sp;
    lfi_box_copyfm(t->proc->box, &sf, frame_sp, sizeof(sf));

    // Validate RIP the handler wants the sandbox context to resume at.
    uintptr_t requested_pc = read64(sf.uc.pc);
    if (!lfi_box_ptrvalid(t->proc->box, requested_pc)) {
        ERROR("%s: requested pc is invalid!\n", __PRETTY_FUNCTION__);
        return false;
    }
    if (old_requested_pc == requested_pc) {
        // resume at the faulting instruction, possibly unaligned
        LOG(t->proc->engine, "%s: returning to the faulting pc!\n", __PRETTY_FUNCTION__);
    }

    // Tell the kernel to resume sandbox execution at the validated RIP.
    uctx->uc_mcontext.pc = requested_pc;

    // SAFETY: X30 must be validated.
    uintptr_t x30 = uctx->uc_mcontext.regs[30];
    if (!lfi_box_ptrvalid(t->proc->box, x30)) {
        ERROR("%s: x30 points to host memory!\n", __PRETTY_FUNCTION__);
        return false;
    }
    regs->x30 = x30;

    regs->x0 = uctx->uc_mcontext.regs[0];
    regs->x1 = uctx->uc_mcontext.regs[1];
    regs->x2 = uctx->uc_mcontext.regs[2];
    regs->x3 = uctx->uc_mcontext.regs[3];
    regs->x4 = uctx->uc_mcontext.regs[4];
    regs->x5 = uctx->uc_mcontext.regs[5];
    regs->x6 = uctx->uc_mcontext.regs[6];
    regs->x7 = uctx->uc_mcontext.regs[7];
    regs->x8 = uctx->uc_mcontext.regs[8];
    regs->x9 = uctx->uc_mcontext.regs[9];
    regs->x10 = uctx->uc_mcontext.regs[10];
    regs->x11 = uctx->uc_mcontext.regs[11];
    regs->x12 = uctx->uc_mcontext.regs[12];
    regs->x13 = uctx->uc_mcontext.regs[13];
    regs->x14 = uctx->uc_mcontext.regs[14];
    regs->x15 = uctx->uc_mcontext.regs[15];
    regs->x16 = uctx->uc_mcontext.regs[16];
    regs->x17 = uctx->uc_mcontext.regs[17];
    regs->x18 = uctx->uc_mcontext.regs[18];
    regs->x19 = uctx->uc_mcontext.regs[19];
    regs->x20 = uctx->uc_mcontext.regs[20];
    regs->x21 = uctx->uc_mcontext.regs[21];
    regs->x22 = uctx->uc_mcontext.regs[22];
    regs->x23 = uctx->uc_mcontext.regs[23];
    regs->x24 = uctx->uc_mcontext.regs[24];
    regs->x25 = uctx->uc_mcontext.regs[25];
    regs->x26 = uctx->uc_mcontext.regs[26];
    regs->x27 = uctx->uc_mcontext.regs[27];
    regs->x28 = uctx->uc_mcontext.regs[28];
    regs->x29 = uctx->uc_mcontext.regs[29];

    return true;
}
