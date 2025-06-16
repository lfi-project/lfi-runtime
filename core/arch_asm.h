#pragma once

// Offsets for the LFIRegs struct. These are statically asserted to be correct
// in arch_asm.c and used in hand-written assembly for accessing offsets of the
// LFIRegs struct.
#define REGS_HOST_SP 0
#define REGS_HOST_TP 8
#define REGS_TP      16

// When inside the sandbox, we need a fast way to look up the current LFI
// context in thread-local storage without using the stack (because we can't
// execute host code on the sandbox stack). We can put a pointer to the LFI
// context in the stack protector TLS slot when entering the sandbox, and
// restore the stack protector when exiting so that the host application isn't
// affected. The only case in which the host would notice is if a signal
// arrives during sandbox execution, then the stack protector during the signal
// handler will be a value that points to the current LFI context (not hugely
// problematic since this is still a random value due to ASLR). The stack
// protector is only stored in this TLS slot to begin with on x86-64 and
// Android-Arm64. On standard Arm64 the slot is unused because the stack
// protector is global.
#define TLS_SLOT_LFI 5

#if defined(__aarch64__) || defined(_M_ARM64)

#define REGS_X0     32
#define REGS_X(n)   (REGS_X0 + 8 * n)
#define REGS_ADDR   REGS_X(18) // x18
#define REGS_BASE   REGS_X(21) // x21
#define REGS_SP     280
#define REGS_VECTOR 288

#define REG_BASE    x21
#define REG_ADDR    x18

#ifdef __ASSEMBLER__
.macro get_ctx reg
    mrs \reg, tpidr_el0
    ldr \reg, [\reg, 8*TLS_SLOT_LFI]
.endm

.macro write_ctx reg tmp
    mrs \tmp, tpidr_el0
    str \reg, [\tmp, 8*TLS_SLOT_LFI]
.endm
#endif

#elif defined(__x86_64__) || defined(_M_X64)

#define REGS_RSP  32
#define REGS_RAX  40
#define REGS_RCX  48
#define REGS_RDX  56
#define REGS_RBX  64
#define REGS_RBP  72
#define REGS_RSI  80
#define REGS_RDI  88
#define REGS_R8   96
#define REGS_R9   104
#define REGS_R10  112
#define REGS_R11  120
#define REGS_R12  128
#define REGS_R13  136
#define REGS_R14  144
#define REGS_R15  152
#define REGS_XMM  192

#define REGS_BASE REGS_R14

#define REG_BASE  r14

#ifdef __ASSEMBLER__
.macro get_ctx reg
    movq %fs:(8*TLS_SLOT_LFI), \reg
.endm

.macro write_ctx reg
    movq \reg, %fs:(8*TLS_SLOT_LFI)
.endm
#endif

#endif

// Offsets for the LFIInvokeInfo struct

#define INVOKE_CTX      0
#define INVOKE_TARGETFN 8
#define INVOKE_RETFN    16
