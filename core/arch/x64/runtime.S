#include "arch_asm.h"

.text

// lfi_ctx_entry(struct LFIContext *ctx, uintptr_t *host_sp_ptr, uintptr_t entry)
.global lfi_ctx_entry
.p2align 4
lfi_ctx_entry:
    // save callee-saved registers to stack
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %rbx
    pushq %rbp
    // save stack to host_sp_ptr
    movq %rsp, (%rsi)
    // put entrypoint in %r11
    movq %rdx, %r11

    // set the sandbox thread control block by saving the old value of
    // %fs:TLS_SLOT_LFI into the context and setting it to point to the context
    // itself.
    get_ctx %rax
    movq %rax, REGS_HOST_TP(%rdi)
    write_ctx %rdi

    // load all registers
    movq REGS_RSP(%rdi), %rsp
    movq REGS_RAX(%rdi), %rax
    movq REGS_RCX(%rdi), %rcx
    movq REGS_RDX(%rdi), %rdx
    movq REGS_RBX(%rdi), %rbx
    movq REGS_RBP(%rdi), %rbp
    movq REGS_RSI(%rdi), %rsi
    movq REGS_R8(%rdi), %r8
    movq REGS_R9(%rdi), %r9
    movq REGS_R10(%rdi), %r10
    // %r11 skipped because it holds the entrypoint
    movq REGS_R12(%rdi), %r12
    movq REGS_R13(%rdi), %r13
    movq REGS_R14(%rdi), %r14
    movq REGS_R15(%rdi), %r15

#ifdef HAVE_PKU
    // save registers that we clobber with wrpkru into the scratchpad
    movq %rax, SCRATCHPAD+0(%REG_BASE)
    movq %rdx, SCRATCHPAD+8(%REG_BASE)
    movq %rcx, SCRATCHPAD+16(%REG_BASE)
    movq REGS_RDI(%rdi), %rdx
    movq %rdx, SCRATCHPAD+24(%REG_BASE)
    pku_box_access REGS_PKEY(%rdi)
    // restore registers from the scratchpad
    movq SCRATCHPAD+0(%REG_BASE), %rax
    movq SCRATCHPAD+8(%REG_BASE), %rdx
    movq SCRATCHPAD+16(%REG_BASE), %rcx
    movq SCRATCHPAD+24(%REG_BASE), %rdi
#else
    movq REGS_RDI(%rdi), %rdi
#endif

#ifdef ENABLE_SEGUE
    wrgsbase %REG_BASE
#endif
    // zero vector registers
    xorps %xmm0, %xmm0
    xorps %xmm1, %xmm1
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
    jmpq *%r11

// lfi_ctx_end(struct LFIContext *ctx, int val)
.global lfi_ctx_end
.p2align 4
lfi_ctx_end:
    // restore host_sp
    movq REGS_HOST_SP(%rdi), %rsp
    // restore host_tp
    movq REGS_HOST_TP(%rdi), %rbp
    write_ctx %rbp
    // put val in the return register
    movq %rsi, %rax
    // restore callee-saved registers saved in lfi_ctx_entry
    popq %rbp
    popq %rbx
    popq %r12
    popq %r13
    popq %r14
    popq %r15
    ret

.global lfi_syscall_entry
.p2align 4
lfi_syscall_entry:
#ifdef HAVE_PKU
    movq %rax, SCRATCHPAD+0(%REG_BASE)
    movq %rdx, SCRATCHPAD+8(%REG_BASE)
    pku_all_access
    movq SCRATCHPAD+0(%REG_BASE), %rax
    movq SCRATCHPAD+8(%REG_BASE), %rdx
#endif
    get_ctx %rcx
    // save registers
    movq %rsp, REGS_RSP(%rcx)
    movq %rax, REGS_RAX(%rcx)
    // rcx clobbered
    movq %rdx, REGS_RDX(%rcx)
    movq %rbx, REGS_RBX(%rcx)
    movq %rbp, REGS_RBP(%rcx)
    movq %rsi, REGS_RSI(%rcx)
    movq %rdi, REGS_RDI(%rcx)
    movq %r8,  REGS_R8(%rcx)
    movq %r9,  REGS_R9(%rcx)
    movq %r10, REGS_R10(%rcx)
    movq %r11, REGS_R11(%rcx)
    movq %xmm0, REGS_XMM+0*16(%rcx)
    movq %xmm1, REGS_XMM+1*16(%rcx)
    movq %xmm2, REGS_XMM+2*16(%rcx)
    movq %xmm3, REGS_XMM+3*16(%rcx)
    movq %xmm4, REGS_XMM+4*16(%rcx)
    movq %xmm5, REGS_XMM+5*16(%rcx)
    movq %xmm6, REGS_XMM+6*16(%rcx)
    movq %xmm7, REGS_XMM+7*16(%rcx)
    movq %xmm8, REGS_XMM+8*16(%rcx)
    movq %xmm9, REGS_XMM+9*16(%rcx)
    movq %xmm10, REGS_XMM+10*16(%rcx)
    movq %xmm11, REGS_XMM+11*16(%rcx)
    movq %xmm12, REGS_XMM+12*16(%rcx)
    movq %xmm13, REGS_XMM+13*16(%rcx)
    movq %xmm14, REGS_XMM+14*16(%rcx)
    movq %xmm15, REGS_XMM+15*16(%rcx)

    // swap the thread control block back to the host one
    movq REGS_HOST_TP(%rcx), %rdi
    write_ctx %rdi

    // restore host stack and call handler
    movq REGS_HOST_SP(%rcx), %rsp
    pushq %rcx
    movq %rcx, %rdi
    callq lfi_syscall_handler
    popq %rcx

    // swap thread control block back to context pointer
    write_ctx %rcx

    // NOTE: consider removing this entirely if we can assume that syscall
    // execution never modifies %gs (i.e., never calls into another sandbox).
#ifdef ENABLE_SEGUE
# ifndef SEGUE_SINGLE_SANDBOX
    wrgsbase %REG_BASE
# endif
#endif

    // restore registers
    movq REGS_RSP(%rcx), %rsp
    movq REGS_RAX(%rcx), %rax
    movq REGS_RDI(%rcx), %rdi
    movq REGS_RDX(%rcx), %rdx
    movq REGS_RBX(%rcx), %rbx
    movq REGS_RBP(%rcx), %rbp
    movq REGS_RSI(%rcx), %rsi
    movq REGS_R8(%rcx), %r8
    movq REGS_R9(%rcx), %r9
    movq REGS_R10(%rcx), %r10
    movq REGS_R11(%rcx), %r11
    movq REGS_XMM+0*16(%rcx), %xmm0
    movq REGS_XMM+1*16(%rcx), %xmm1
    movq REGS_XMM+2*16(%rcx), %xmm2
    movq REGS_XMM+3*16(%rcx), %xmm3
    movq REGS_XMM+4*16(%rcx), %xmm4
    movq REGS_XMM+5*16(%rcx), %xmm5
    movq REGS_XMM+6*16(%rcx), %xmm6
    movq REGS_XMM+7*16(%rcx), %xmm7
    movq REGS_XMM+8*16(%rcx), %xmm8
    movq REGS_XMM+9*16(%rcx), %xmm9
    movq REGS_XMM+10*16(%rcx), %xmm10
    movq REGS_XMM+11*16(%rcx), %xmm11
    movq REGS_XMM+12*16(%rcx), %xmm12
    movq REGS_XMM+13*16(%rcx), %xmm13
    movq REGS_XMM+14*16(%rcx), %xmm14
    movq REGS_XMM+15*16(%rcx), %xmm15
#ifdef HAVE_PKU
    movq %rax, SCRATCHPAD+0(%REG_BASE)
    movq %rdx, SCRATCHPAD+8(%REG_BASE)
    pku_box_access REGS_PKEY(%rcx)
    movq SCRATCHPAD+0(%REG_BASE), %rax
    movq SCRATCHPAD+8(%REG_BASE), %rdx
#endif
    movq $0, %rcx // clobbered
    jmpq *%r11

.global lfi_get_tp
.p2align 4
lfi_get_tp:
#if defined(HAVE_PKU) && !defined(STORES_ONLY)
    movq %rdx, SCRATCHPAD+0(%REG_BASE)
    movq %rcx, SCRATCHPAD+8(%REG_BASE)
    pku_all_access

    get_ctx %rax
    movq REGS_TP(%rax), %rdx
    movq %rdx, SCRATCHPAD+16(%REG_BASE)

    movq REGS_PKEY(%rax), %rcx
    pku_box_access %rcx
    movq SCRATCHPAD+0(%REG_BASE), %rdx
    movq SCRATCHPAD+8(%REG_BASE), %rcx
    movq SCRATCHPAD+16(%REG_BASE), %rax
#else
    get_ctx %rax
    movq REGS_TP(%rax), %rax
#endif
    jmpq *%r11

.global lfi_set_tp
.p2align 4
lfi_set_tp:
#ifdef HAVE_PKU
    movq %rdx, SCRATCHPAD+0(%REG_BASE)
    movq %rcx, SCRATCHPAD+8(%REG_BASE)
    movq %rax, SCRATCHPAD+16(%REG_BASE)
    pku_all_access
    movq SCRATCHPAD+16(%REG_BASE), %rax

    get_ctx %REG_BASE
    movq %rax, REGS_TP(%REG_BASE)
    movq REGS_PKEY(%REG_BASE), %rcx
    movq REGS_BASE(%REG_BASE), %REG_BASE
    pku_box_access %rcx

    movq SCRATCHPAD+0(%REG_BASE), %rdx
    movq SCRATCHPAD+8(%REG_BASE), %rcx
#else
    get_ctx %REG_BASE
    movq %rax, REGS_TP(%REG_BASE)
    movq REGS_BASE(%REG_BASE), %REG_BASE
#endif
    jmpq *%r11

#ifndef __APPLE__
.section .note.GNU-stack,"",@progbits
#endif
