#pragma once

// Offsets for the LFIRegs struct. These are statically asserted to be correct
// in arch_asm.c and used in hand-written assembly for accessing offsets of the
// LFIRegs struct.
#define REGS_HOST_SP 0
#define REGS_HOST_TP 8
#define REGS_TP      16

#if defined(__aarch64__) || defined(_M_ARM64)

#define REGS_X0     32
#define REGS_ADDR   (REGS_X0 + 18 * 8) // x18
#define REGS_BASE   (REGS_X0 + 21 * 8) // x21
#define REGS_SP     280
#define REGS_VECTOR 288

#define REG_BASE    x21
#define REG_ADDR    x18

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

#endif
