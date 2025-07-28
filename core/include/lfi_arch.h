#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__aarch64__) || defined(_M_ARM64)

struct LFIRegs {
    // The host sp and tp slots are used for saving/restoring host information
    // when transitioning in and out of the sandbox and should not typically be
    // directly modified.
    uint64_t host_sp;
    uint64_t host_tp;
    // Sandbox thread pointer.
    uint64_t tp;
    // Return address for lfi_trampoline invocations (generally this should be
    // a pointer to a function in the sandbox that calls lfi_ret).
    uint64_t retaddr;

    uint64_t x0;
    uint64_t x1;
    uint64_t x2;
    uint64_t x3;
    uint64_t x4;
    uint64_t x5;
    uint64_t x6;
    uint64_t x7;
    uint64_t x8;
    uint64_t x9;
    uint64_t x10;
    uint64_t x11;
    uint64_t x12;
    uint64_t x13;
    uint64_t x14;
    uint64_t x15;
    uint64_t x16;
    uint64_t x17;
    uint64_t x18;
    uint64_t x19;
    uint64_t x20;
    uint64_t x21;
    uint64_t x22;
    uint64_t x23;
    uint64_t x24;
    uint64_t x25;
    uint64_t x26;
    uint64_t x27;
    uint64_t x28;
    uint64_t x29;
    uint64_t x30;
    uint64_t sp;
    uint64_t vector[64];
};

#define LFI_ARCH_ARM64

#elif defined(__x86_64__) || defined(_M_X64)

struct LFIRegs {
    uint64_t host_sp;
    // The host tp that is saved/restored is actually the value pointed to by
    // the thread pointer, not the thread pointer itself. This is because
    // directly modifying %fs (where the thread pointer is stored on x86-64) is
    // a slow operation: wrfsbase can take up to 40 cycles.
    uint64_t host_tp;
    uint64_t tp;
    uint64_t retaddr;
    uint64_t pkey;

    uint64_t rsp;
    uint64_t rax;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rbx;
    uint64_t rbp;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint16_t fcw;
    uint32_t mxcsr;
    uint64_t _pad[2];
    uint64_t xmm[32];
};

#define LFI_ARCH_X64

#elif defined(__riscv) && (__riscv_xlen == 64)

struct LFIRegs {
    // Host context saving
    uint64_t host_sp;
    uint64_t host_tp;
    // Sandbox thread pointer
    uint64_t tp;
    // Return address for lfi_trampoline invocations
    uint64_t retaddr;

    // RISC-V general purpose registers
    uint64_t zero; // zero (hardwired zero)
    uint64_t ra;   // ra (return address)
    uint64_t sp;   // sp (stack pointer)
    uint64_t gp;   // gp (global pointer)
    uint64_t rtp;  // tp (thread pointer)
    uint64_t t0;   // t0 (temporary)
    uint64_t t1;   // t1 (temporary)
    uint64_t t2;   // t2 (temporary)
    uint64_t s0;   // s0/fp (saved/frame pointer)
    uint64_t s1;   // s1 (saved)
    uint64_t a0;   // a0 (argument/return value)
    uint64_t a1;   // a1 (argument/return value)
    uint64_t a2;   // a2 (argument)
    uint64_t a3;   // a3 (argument)
    uint64_t a4;   // a4 (argument)
    uint64_t a5;   // a5 (argument)
    uint64_t a6;   // a6 (argument)
    uint64_t a7;   // a7 (argument)
    uint64_t s2;   // s2 (saved)
    uint64_t s3;   // s3 (saved)
    uint64_t s4;   // s4 (saved)
    uint64_t s5;   // s5 (saved)
    uint64_t s6;   // s6 (saved)
    uint64_t s7;   // s7 (saved)
    uint64_t s8;   // s8 (saved)
    uint64_t s9;   // s9 (saved)
    uint64_t s10;  // s10 (saved)
    uint64_t s11;  // s11 (saved)
    uint64_t t3;   // t3 (temporary)
    uint64_t t4;   // t4 (temporary)
    uint64_t t5;   // t5 (temporary)
    uint64_t t6;   // t6 (temporary)

    // Floating point registers
    uint64_t f[32]; // f0-f31
};

#define LFI_ARCH_RISCV64

#endif

#ifdef __cplusplus
}
#endif
