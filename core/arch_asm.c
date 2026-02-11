#include "arch_asm.h"

#include "lfi_arch.h"
#include "lfi_core.h"
#include "core.h"

#include <stddef.h>

_Static_assert(sizeof(void *) == sizeof(void (*)(void)),
    "data pointer and function pointer differ in size");

_Static_assert(offsetof(struct LFIRegs, host_sp) == REGS_HOST_SP,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, host_tp) == REGS_HOST_TP,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, _tp) == REGS_TP,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, retaddr) == REGS_RETADDR,
    "incorrect REGS offset");

#if defined(LFI_ARCH_ARM64)

_Static_assert(offsetof(struct LFIRegs, x0) == REGS_X0,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, x1) == REGS_X(1),
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, sp) == REGS_SP,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, REG_BASE) == REGS_BASE,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, REG_ADDR) == REGS_ADDR,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, vector) == REGS_V0,
    "incorrect REGS offset");

_Static_assert(offsetof(struct LFIRegs, vector) % 16 == 0,
    "incorrect vector alignment");

_Static_assert(offsetof(struct LFIContext, abort_callback) == CTX_ABORT_CALLBACK,
    "incorrect abort_callback offset");
_Static_assert(offsetof(struct LFIContext, abort_status) == CTX_ABORT_STATUS,
    "incorrect abort_status offset");

#elif defined(LFI_ARCH_X64)

_Static_assert(offsetof(struct LFIRegs, pkey) == REGS_PKEY,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, rsp) == REGS_RSP,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, rax) == REGS_RAX,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, rcx) == REGS_RCX,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, rdx) == REGS_RDX,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, rbx) == REGS_RBX,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, rbp) == REGS_RBP,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, rsi) == REGS_RSI,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, rdi) == REGS_RDI,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, r8) == REGS_R8,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, r9) == REGS_R9,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, r10) == REGS_R10,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, r11) == REGS_R11,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, r12) == REGS_R12,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, r13) == REGS_R13,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, r14) == REGS_R14,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, r15) == REGS_R15,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, fcw) == REGS_FCW,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, mxcsr) == REGS_MXCSR,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, xmm) == REGS_XMM0,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, REG_BASE) == REGS_BASE,
    "incorrect REGS offset");

_Static_assert(offsetof(struct LFIRegs, xmm) % 16 == 0,
    "incorrect xmm alignment");

_Static_assert(offsetof(struct LFIContext, abort_callback) == CTX_ABORT_CALLBACK,
    "incorrect abort_callback offset");
_Static_assert(offsetof(struct LFIContext, abort_status) == CTX_ABORT_STATUS,
    "incorrect abort_callback offset");

#ifdef CTXREG
_Static_assert(offsetof(struct LFIContext, scs_limit) == CTX_SCS_LIMIT,
    "incorrect CTX_SCS_LIMIT offset");
_Static_assert(offsetof(struct LFIContext, scs_save_stack) == CTX_SCS_SAVE_STACK,
    "incorrect CTX_SCS_SAVE_STACK offset");
_Static_assert(offsetof(struct LFIContext, scs_save_sp) == CTX_SCS_SAVE_SP,
    "incorrect CTX_SCS_SAVE_SP offset");
#endif

#elif defined(LFI_ARCH_RISCV64)

_Static_assert(offsetof(struct LFIRegs, zero) == REGS_ZERO,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, ra) == REGS_RA,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, sp) == REGS_SP,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, gp) == REGS_GP,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, rtp) == REGS_RTP,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, t0) == REGS_T0,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, t1) == REGS_T1,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, t2) == REGS_T2,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, s0) == REGS_S0,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, s1) == REGS_S1,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, a0) == REGS_A0,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, a1) == REGS_A1,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, a2) == REGS_A2,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, a3) == REGS_A3,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, a4) == REGS_A4,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, a5) == REGS_A5,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, a6) == REGS_A6,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, a7) == REGS_A7,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, s2) == REGS_S2,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, s3) == REGS_S3,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, s4) == REGS_S4,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, s5) == REGS_S5,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, s6) == REGS_S6,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, s7) == REGS_S7,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, s8) == REGS_S8,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, s9) == REGS_S9,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, s10) == REGS_S10,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, s11) == REGS_S11,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, t3) == REGS_T3,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, t4) == REGS_T4,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, t5) == REGS_T5,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, t6) == REGS_T6,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, f) == REGS_F, "incorrect REGS offset");

_Static_assert(offsetof(struct LFIRegs, REG_BASE) == REGS_BASE,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, REG_ADDR) == REGS_ADDR,
    "incorrect REGS offset");

#endif

_Static_assert(offsetof(struct LFIInvokeInfo, ctx) == INVOKE_CTX,
    "incorrect INVOKE offset");
_Static_assert(offsetof(struct LFIInvokeInfo, targetfn) == INVOKE_TARGETFN,
    "incorrect INVOKE offset");
_Static_assert(offsetof(struct LFIInvokeInfo, box) == INVOKE_BOX,
    "incorrect INVOKE offset");
