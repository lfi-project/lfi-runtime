#include "arch_asm.h"

#include "lfi_arch.h"
#include "lfi_core.h"

#include <stddef.h>

_Static_assert(offsetof(struct LFIRegs, host_sp) == REGS_HOST_SP,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, host_tp) == REGS_HOST_TP,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, tp) == REGS_TP,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, retaddr) == REGS_RETADDR,
    "incorrect REGS offset");

#if defined(LFI_ARCH_ARM64)

_Static_assert(offsetof(struct LFIRegs, x0) == REGS_X0,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, sp) == REGS_SP,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, REG_BASE) == REGS_BASE,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, REG_ADDR) == REGS_ADDR,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, vector) == REGS_VECTOR,
    "incorrect REGS offset");

_Static_assert(offsetof(struct LFIRegs, vector) % 16 == 0,
    "incorrect vector alignment");

#elif defined(LFI_ARCH_X64)

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
_Static_assert(offsetof(struct LFIRegs, xmm) == REGS_XMM,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, REG_BASE) == REGS_BASE,
    "incorrect REGS offset");

_Static_assert(offsetof(struct LFIRegs, xmm) % 16 == 0,
    "incorrect xmm alignment");

#elif defined(LFI_ARCH_RISCV64)

_Static_assert(offsetof(struct LFIRegs, x0) == REGS_X0,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, x2) == REGS_SP,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, x18) == REGS_BASE,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, x19) == REGS_ADDR,
    "incorrect REGS offset");
_Static_assert(offsetof(struct LFIRegs, f) == REGS_F, "incorrect REGS offset");

#endif

_Static_assert(offsetof(struct LFIInvokeInfo, ctx) == INVOKE_CTX,
    "incorrect INVOKE offset");
_Static_assert(offsetof(struct LFIInvokeInfo, targetfn) == INVOKE_TARGETFN,
    "incorrect INVOKE offset");
_Static_assert(offsetof(struct LFIInvokeInfo, box) == INVOKE_BOX,
    "incorrect INVOKE offset");
