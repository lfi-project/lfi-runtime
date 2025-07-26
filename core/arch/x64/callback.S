#include "arch_asm.h"

.text

.macro zero_caller_regs
    // Skip %rax and %rdx (return registers).
    xorq %rdi, %rdi
    xorq %rcx, %rcx
    xorq %rsi, %rsi
    xorq %r8, %r8
    xorq %r9, %r9
    // Skip %xmm0 and %xmm1 (return registers).
    xorps %xmm2, %xmm2
    xorps %xmm3, %xmm3
    xorps %xmm4, %xmm4
    xorps %xmm5, %xmm5
    xorps %xmm6, %xmm6
    xorps %xmm7, %xmm7
    xorps %xmm8, %xmm8
    xorps %xmm9, %xmm9
    xorps %xmm10, %xmm10
    xorps %xmm11, %xmm11
    xorps %xmm12, %xmm12
    xorps %xmm13, %xmm13
    xorps %xmm14, %xmm14
    xorps %xmm15, %xmm15
.endm

.macro lfi_callback_gen with_struct
    // Entrypoint for callbacks, with the target in %r10.
#ifdef HAVE_PKU
    pushq %rcx
    movq %rdx, %r11
    pku_all_access
    movq %r11, %rdx
    popq %rcx
#endif

    get_ctx %r11
    // Swap the sandbox/host stacks. The sandbox stack won't remain in the
    // REGS_HOST_SP spot for long because we'll restore the saved host stack on
    // the way out.
    xchg REGS_HOST_SP(%r11), %rsp
    // Save %r12 to use as temp.
    pushq %r12
    // Move sandbox stack pointer (xchged earlier) to %r12.
    movq REGS_HOST_SP(%r11), %r12
    // Push old sandbox stack pointer to stack.
    pushq REGS_RSP(%r11)
    // Save current sandbox stack pointer.
    movq %r12, REGS_RSP(%r11)

    // Restore host tp.
    movq REGS_HOST_TP(%r11), %r12
    write_ctx %r12

.if \with_struct
    movq %rdi, REGS_RDI(%r11)
    movq %rsi, REGS_RSI(%r11)
    movq %rdx, REGS_RDX(%r11)
    movq %rcx, REGS_RCX(%r11)
    movq %r8, REGS_R8(%r11)
    movq %r9, REGS_R9(%r11)
    movdqu %xmm0, REGS_XMM+0*16(%r11)
    movdqu %xmm1, REGS_XMM+1*16(%r11)
    movdqu %xmm2, REGS_XMM+2*16(%r11)
    movdqu %xmm3, REGS_XMM+3*16(%r11)
    movdqu %xmm4, REGS_XMM+4*16(%r11)
    movdqu %xmm5, REGS_XMM+5*16(%r11)
    movdqu %xmm6, REGS_XMM+6*16(%r11)
    movdqu %xmm7, REGS_XMM+7*16(%r11)
.endif

    // Save %r11 across the call (and stack alignment).
    pushq %r11
    callq *%r10
    popq %r11

#ifdef ENABLE_SEGUE
# ifndef SEGUE_SINGLE_SANDBOX
    wrgsbase %REG_BASE
# endif
#endif

    // Set the sandbox thread control block to the context pointer.
    write_ctx %r11
    // Move current sandbox stack pointer to %r10
    movq REGS_RSP(%r11), %r10
    // Restore old sandbox stack pointer in the Regs struct.
    popq REGS_RSP(%r11)
    // Restore %r12.
    popq %r12
    // Restore the saved host stack pointer.
    movq %rsp, REGS_HOST_SP(%r11)
    // Restore sandbox stack pointer.
    movq %r10, %rsp

.if \with_struct
    movq REGS_RAX(%r11), %rax
    movq REGS_RDX(%r11), %rdx
    movdqu REGS_XMM+0*16(%r11), %xmm0
    movdqu REGS_XMM+1*16(%r11), %xmm1
.endif

#ifdef HAVE_PKU
    movq REGS_PKEY(%r11), %rcx
    movq %rax, %r11
    pku_box_access %rcx
    movq %r11, %rax
#endif

#ifndef STORES_ONLY
    zero_caller_regs
#endif

    // Return to sandbox

    // NOTE: This pop instruction could fault.
    popq %r11
    andl $0xffffffe0, %r11d
    addq %REG_BASE, %r11
    jmpq *%r11
.endm

.p2align 4
.global lfi_callback
lfi_callback:
    lfi_callback_gen 0

.p2align 4
.global lfi_callback_struct
lfi_callback_struct:
    lfi_callback_gen 1

#ifndef __APPLE__
.section .note.GNU-stack,"",@progbits
#endif
