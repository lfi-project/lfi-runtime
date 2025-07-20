#pragma once

// Offsets for the LFIRegs struct. These are statically asserted to be correct
// in arch_asm.c and used in hand-written assembly for accessing offsets of the
// LFIRegs struct.
#define REGS_HOST_SP 0
#define REGS_HOST_TP 8
#define REGS_TP      16
#define REGS_RETADDR 24

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
#define REGS_ADDR   REGS_X(28) // x28
#define REGS_BASE   REGS_X(27) // x27
#define REGS_SP     280
#define REGS_VECTOR 288

#define REG_BASE    x27
#define REG_ADDR    x28

// clang-format off
#ifdef __ASSEMBLER__
# if defined(__APPLE__)
// We save and restore whatever tpidrro_el0 points to. This probably will
// completely break behavior inside signal handlers if we receive a signal
// during sandbox execution...
.macro get_ctx reg
    mrs \reg, tpidrro_el0
    ldr \reg, [\reg, 0]
.endm

.macro write_ctx reg tmp
    mrs \tmp, tpidrro_el0
    str \reg, [\tmp, 0]
.endm
# else
// On linux we save and restore the TLS slot offset from tpidr_el0.
.macro get_ctx reg
    mrs \reg, tpidr_el0
    ldr \reg, [\reg, 8*TLS_SLOT_LFI]
.endm

.macro write_ctx reg tmp
    mrs \tmp, tpidr_el0
    str \reg, [\tmp, 8*TLS_SLOT_LFI]
.endm
# endif
#endif
// clang-format on

#elif defined(__x86_64__) || defined(_M_X64)

#define REGS_PKEY 32

#define REGS_RSP  40
#define REGS_RAX  48
#define REGS_RCX  56
#define REGS_RDX  64
#define REGS_RBX  72
#define REGS_RBP  80
#define REGS_RSI  88
#define REGS_RDI  96
#define REGS_R8   104
#define REGS_R9   112
#define REGS_R10  120
#define REGS_R11  128
#define REGS_R12  136
#define REGS_R13  144
#define REGS_R14  152
#define REGS_R15  160
#define REGS_XMM  192

#define REGS_BASE REGS_R14

#define REG_BASE  r14

// clang-format off
#ifdef __ASSEMBLER__
.macro get_ctx reg
    movq %fs:(8*TLS_SLOT_LFI), \reg
.endm

.macro write_ctx reg
    movq \reg, %fs:(8*TLS_SLOT_LFI)
.endm

.macro write_pkru val
    movq \val, %rax
    xorl %ecx, %ecx
    xorl %edx, %edx
    wrpkru
.endm
#endif
// clang-format on

#endif

#if defined(__riscv) && (__riscv_xlen == 64)

#define REGS_ZERO 32
#define REGS_RA   40
#define REGS_SP   48
#define REGS_GP   56
#define REGS_RTP  64
#define REGS_T0   72
#define REGS_T1   80
#define REGS_T2   88
#define REGS_S0   96
#define REGS_S1   104
#define REGS_A0   112
#define REGS_A1   120
#define REGS_A2   128
#define REGS_A3   136
#define REGS_A4   144
#define REGS_A5   152
#define REGS_A6   160
#define REGS_A7   168
#define REGS_S2   176
#define REGS_S3   184
#define REGS_S4   192
#define REGS_S5   200
#define REGS_S6   208
#define REGS_S7   216
#define REGS_S8   224
#define REGS_S9   232
#define REGS_S10  240
#define REGS_S11  248
#define REGS_T3   256
#define REGS_T4   264
#define REGS_T5   272
#define REGS_T6   280
#define REGS_F    288

#define REGS_BASE REGS_S11
#define REGS_ADDR REGS_S10
#define REG_BASE  s11
#define REG_ADDR  s10
// clang-format off
#ifdef __ASSEMBLER__
.macro get_ctx reg
    ld \reg, (8*TLS_SLOT_LFI)(tp)
.endm

.macro write_ctx reg
    sd \reg, (8*TLS_SLOT_LFI)(tp)
.endm
#endif
// clang-format on

#endif

// Offsets for the LFIInvokeInfo struct

#define INVOKE_CTX      0
#define INVOKE_TARGETFN 8
#define INVOKE_BOX      16
