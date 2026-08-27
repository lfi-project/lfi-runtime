#pragma once

#include "ctxreg.h"

// Offsets for the LFIRegs struct. These are statically asserted to be correct
// in arch_asm.c and used in hand-written assembly for accessing offsets of the
// LFIRegs struct.
#define REGS_HOST_SP 0
#define REGS_RETADDR 8

#if defined(__aarch64__) || defined(_M_ARM64)

#define REGS_X0   16
#define REGS_X(n) (REGS_X0 + 8 * n)
#define REGS_ADDR REGS_X(28) // x28
#define REGS_BASE REGS_X(27) // x27
#define REGS_SP   264
#define REGS_V0   272
#define REGS_V(n) (REGS_V0 + 16 * n)

#define CTX_ABORT_CALLBACK 784
#define CTX_ABORT_STATUS   792
#define CTX_CTXREG         800

#define REG_BASE  x27
#define REG_ADDR  x28

// clang-format off
#ifdef __ASSEMBLER__
// x25 points to the ctxreg array, which holds the context pointer at
// CTXREG_CTX_OFFSET.
.macro get_ctx reg
    ldr \reg, [x25, CTXREG_CTX_OFFSET]
.endm
#endif
// clang-format on

#elif defined(__x86_64__) || defined(_M_X64)

#define REGS_PKEY   16

#define REGS_RSP    24
#define REGS_RAX    32
#define REGS_RCX    40
#define REGS_RDX    48
#define REGS_RBX    56
#define REGS_RBP    64
#define REGS_RSI    72
#define REGS_RDI    80
#define REGS_R8     88
#define REGS_R9     96
#define REGS_R10    104
#define REGS_R11    112
#define REGS_R12    120
#define REGS_R13    128
#define REGS_R14    136
#define REGS_R15    144
#define REGS_FCW    152
#define REGS_MXCSR  156
#define REGS_XMM0   176
#define REGS_XMM(n) (REGS_XMM0 + 16 * n)

#define REGS_BASE   REGS_R14

#define REG_BASE    r14

#define CTX_ABORT_CALLBACK 432
#define CTX_ABORT_STATUS   440
// Offset of the gs_cache pointer (only present when SEGUE_CACHE_GS is enabled).
#define CTX_GS_CACHE       448

// clang-format off
#ifdef __ASSEMBLER__
// r15 points to the ctxreg array, which holds the context pointer at
// CTXREG_CTX_OFFSET.
.macro get_ctx reg
    movq CTXREG_CTX_OFFSET(%r15), \reg
.endm

// Load the sandbox base (%REG_BASE) into the %gs base register for Segue.
//
// Without SEGUE_CACHE_GS this unconditionally executes wrgsbase on every
// transition into a sandbox. The wrgsbase instruction is slow, so with
// SEGUE_CACHE_GS we instead keep a per-thread cache of the base currently held
// in %gs and only execute wrgsbase when entering a sandbox with a different
// base than the one already loaded. This makes repeated transitions into the
// same sandbox cheap while still supporting multiple sandboxes. The cache is
// correct as long as %gs is only ever modified through these LFI transitions
// (i.e. the host itself never clobbers the %gs base).
//
// The cache value lives in a per-thread thread_local slot
// (lfi_invoke_info.gs_base), but to avoid any TLS access from the transition
// fast paths each context stores a pointer to its running thread's slot in
// ctx->gs_cache. The trampoline (which already resolves lfi_invoke_info) and
// lfi_ctx_run set up that pointer. Here we simply load it and compare. ctx is
// a register holding the LFIContext pointer and scratch is a register free to
// clobber at the call site. Both are unused when SEGUE_CACHE_GS is disabled.
.macro seg_gs_set ctx scratch
#ifdef SEGUE_CACHE_GS
    movq CTX_GS_CACHE(\ctx), \scratch
    cmpq %REG_BASE, (\scratch)
    je .Lseg_gs_skip\@
    wrgsbase %REG_BASE
    movq %REG_BASE, (\scratch)
.Lseg_gs_skip\@:
#else
    wrgsbase %REG_BASE
#endif
.endm

// Allow access to all PKU regions.
.macro pku_all_access
    xorl %eax, %eax
    xorl %ecx, %ecx
    xorl %edx, %edx
    wrpkru
.endm

// Only allow access to the PKU region specified by 'key'.
.macro pku_box_access key
    movq \key, %rcx
    leal (%ecx, %ecx), %ecx
    movl $0b11, %eax
    shll %cl, %eax
    notl %eax
#ifdef STORES_ONLY
    andl $0xfffffffe, %eax
#endif
    xorl %ecx, %ecx
    xorl %edx, %edx
    wrpkru
.endm

#endif
// clang-format on

#endif

#if defined(__riscv) && (__riscv_xlen == 64)

#define REGS_ZERO 16
#define REGS_RA   24
#define REGS_SP   32
#define REGS_GP   40
#define REGS_RTP  48
#define REGS_T0   56
#define REGS_T1   64
#define REGS_T2   72
#define REGS_S0   80
#define REGS_S1   88
#define REGS_A0   96
#define REGS_A1   104
#define REGS_A2   112
#define REGS_A3   120
#define REGS_A4   128
#define REGS_A5   136
#define REGS_A6   144
#define REGS_A7   152
#define REGS_S2   160
#define REGS_S3   168
#define REGS_S4   176
#define REGS_S5   184
#define REGS_S6   192
#define REGS_S7   200
#define REGS_S8   208
#define REGS_S9   216
#define REGS_S10  224
#define REGS_S11  232
#define REGS_T3   240
#define REGS_T4   248
#define REGS_T5   256
#define REGS_T6   264
#define REGS_F    272

#define REGS_BASE REGS_S11
#define REGS_ADDR REGS_S1
#define REGS_CTX  REGS_S10
#define REG_BASE  s11
#define REG_ADDR  s1
#define REG_CTX   s10
// clang-format off
#ifdef __ASSEMBLER__
// s10 points to the ctxreg array, which holds the context pointer at
// CTXREG_CTX_OFFSET.
.macro get_ctx reg
    ld \reg, CTXREG_CTX_OFFSET(REG_CTX)
.endm
#endif
// clang-format on

#endif

// Offsets for the LFIInvokeInfo struct

#define INVOKE_CTX      0
#define INVOKE_TARGETFN 8
#define INVOKE_BOX      16
#define INVOKE_GS_BASE  24
