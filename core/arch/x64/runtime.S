#include "arch_asm.h"

lfi_cet_note

.text

// lfi_ctx_entry(struct LFIContext *ctx, uintptr_t *host_sp_ptr, uintptr_t entry)
.global lfi_ctx_entry
.p2align 4
lfi_ctx_entry:
    endbr64
    // save callee-saved registers to stack
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %rbx
    pushq %rbp
#ifdef ENABLE_CET
    // Save the shadow stack position for the eventual non-local return in
    // lfi_ctx_end. The extra padding slot preserves the host stack alignment
    // expected by lfi_syscall_entry when it calls back into C.
    xorl %eax, %eax
    rdsspq %rax
    pushq %rax
    pushq $0
#endif
    // save stack to host_sp_ptr
    movq %rsp, (%rsi)
    // put entrypoint in %r11
    movq %rdx, %r11

#ifndef CTXREG
    // set the sandbox thread control block by saving the old value of
    // %fs:TLS_SLOT_LFI into the context and setting it to point to the context
    // itself.
    get_ctx %rax
    movq %rax, REGS_HOST_TP(%rdi)
    write_ctx %rdi
#endif

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
    fldcw REGS_FCW(%rdi)
    ldmxcsr REGS_MXCSR(%rdi)

#ifdef HAVE_PKU
    // enabling PKU clobbers %rcx, %rdx, %rax
    movq REGS_PKEY(%rdi), %rcx
    movq REGS_RDI(%rdi), %rdi
    pku_box_access %rcx
    xorl %eax, %eax
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
    endbr64
    // restore host_sp
    movq REGS_HOST_SP(%rdi), %rsp
#ifndef CTXREG
    // restore host_tp
    movq REGS_HOST_TP(%rdi), %rbp
    write_ctx %rbp
#endif
    // put val in the return register
    movq %rsi, %rax
#ifdef ENABLE_CET
    // lfi_ctx_end abandons the host call chain between the syscall handler and
    // this function. Move SSP back to the entry saved by lfi_ctx_entry so the
    // final ret compares against the same return address as the normal stack.
    movq 8(%rsp), %r10
    addq $16, %rsp
    xorl %r11d, %r11d
    rdsspq %r11
    cmpq %r11, %r10
    je .Lctx_end_shstk_done
    jb .Lctx_end_shstk_bad
    subq %r11, %r10
    testq $7, %r10
    jne .Lctx_end_shstk_bad
    shrq $3, %r10
.Lctx_end_shstk_loop:
    cmpq $255, %r10
    jbe .Lctx_end_shstk_tail
    movl $255, %ecx
    incsspq %rcx
    subq $255, %r10
    jmp .Lctx_end_shstk_loop
.Lctx_end_shstk_tail:
    testq %r10, %r10
    je .Lctx_end_shstk_done
    movq %r10, %rcx
    incsspq %rcx
    jmp .Lctx_end_shstk_done
.Lctx_end_shstk_bad:
    ud2
.Lctx_end_shstk_done:
#endif
    cld
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
    endbr64
#ifdef HAVE_PKU
    pushq %rax
    pushq %rdx
    pku_all_access
    popq %rdx
    popq %rax
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
    fnstcw REGS_FCW(%rcx)
    movq %xmm0, REGS_XMM(0)(%rcx)
    movq %xmm1, REGS_XMM(1)(%rcx)
    movq %xmm2, REGS_XMM(2)(%rcx)
    movq %xmm3, REGS_XMM(3)(%rcx)
    movq %xmm4, REGS_XMM(4)(%rcx)
    movq %xmm5, REGS_XMM(5)(%rcx)
    movq %xmm6, REGS_XMM(6)(%rcx)
    movq %xmm7, REGS_XMM(7)(%rcx)
    movq %xmm8, REGS_XMM(8)(%rcx)
    movq %xmm9, REGS_XMM(9)(%rcx)
    movq %xmm10, REGS_XMM(10)(%rcx)
    movq %xmm11, REGS_XMM(11)(%rcx)
    movq %xmm12, REGS_XMM(12)(%rcx)
    movq %xmm13, REGS_XMM(13)(%rcx)
    movq %xmm14, REGS_XMM(14)(%rcx)
    movq %xmm15, REGS_XMM(15)(%rcx)

#ifndef CTXREG
    // swap the thread control block back to the host one
    movq REGS_HOST_TP(%rcx), %rdi
    write_ctx %rdi
#endif

    // restore host stack and call handler
    movq REGS_HOST_SP(%rcx), %rsp
    cld
    pushq %rcx
    movq %rcx, %rdi
    callq lfi_syscall_handler
    popq %rcx

#ifndef CTXREG
    // swap thread control block back to context pointer
    write_ctx %rcx
#endif

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
    fldcw REGS_FCW(%rcx)
    movq REGS_XMM(0)(%rcx), %xmm0
    movq REGS_XMM(1)(%rcx), %xmm1
    movq REGS_XMM(2)(%rcx), %xmm2
    movq REGS_XMM(3)(%rcx), %xmm3
    movq REGS_XMM(4)(%rcx), %xmm4
    movq REGS_XMM(5)(%rcx), %xmm5
    movq REGS_XMM(6)(%rcx), %xmm6
    movq REGS_XMM(7)(%rcx), %xmm7
    movq REGS_XMM(8)(%rcx), %xmm8
    movq REGS_XMM(9)(%rcx), %xmm9
    movq REGS_XMM(10)(%rcx), %xmm10
    movq REGS_XMM(11)(%rcx), %xmm11
    movq REGS_XMM(12)(%rcx), %xmm12
    movq REGS_XMM(13)(%rcx), %xmm13
    movq REGS_XMM(14)(%rcx), %xmm14
    movq REGS_XMM(15)(%rcx), %xmm15
    fldcw REGS_FCW(%rcx)
    ldmxcsr REGS_MXCSR(%rcx)
#ifdef HAVE_PKU
    pushq %rax
    pushq %rdx
    pku_box_access REGS_PKEY(%rcx)
    popq %rdx
    popq %rax
#endif
    movq $0, %rcx // clobbered
    jmpq *%r11

.global lfi_get_tp
.p2align 4
lfi_get_tp:
    endbr64
#if defined(HAVE_PKU) && !defined(STORES_ONLY)
    pushq %rdx
    pushq %rcx
    pku_all_access

    get_ctx %rax
    pushq REGS_TP(%rax)

    movq REGS_PKEY(%rax), %rcx
    pku_box_access %rcx
    popq %rax
    popq %rcx
    popq %rdx
#else
    get_ctx %rax
    movq REGS_TP(%rax), %rax
#endif
    jmpq *%r11

.global lfi_set_tp
.p2align 4
lfi_set_tp:
    endbr64
#ifdef HAVE_PKU
    pushq %rdx
    pushq %rcx
    pushq %rax
    pku_all_access
    popq %rax

    get_ctx %REG_BASE
    movq %rax, REGS_TP(%REG_BASE)
    movq REGS_PKEY(%REG_BASE), %rcx
    movq REGS_BASE(%REG_BASE), %REG_BASE
    pku_box_access %rcx

    popq %rcx
    popq %rdx
#else
    get_ctx %REG_BASE
    movq %rax, REGS_TP(%REG_BASE)
    movq REGS_BASE(%REG_BASE), %REG_BASE
#endif
    jmpq *%r11

#ifdef ENABLE_SW_SHSTK
// Software shadow call stack unwind. Counterpart to incsspq: pop N entries off
// the SCS by adding N*8 to the SCS pointer at CTXREG_SCS_OFFSET(%r15).
//
// Inputs (LFI runtime call ABI):
//   CTXREG_RT_SLOT_OFFSET(%r15) — entry count N (low 32 bits used)
//   %r11                        — return address back into sandbox
//
// Bounds-checked against CTXREG_SCS_BOUND_OFFSET(%r15) so a sandbox-supplied
// count cannot move the SCS pointer past the SCS region; an out-of-range
// count traps via ud2.
.global lfi_scs_unwind
.p2align 4
lfi_scs_unwind:
    endbr64
    movq CTXREG_RT_SLOT_OFFSET(%r15), %rax
    shlq $3, %rax
    addq CTXREG_SCS_OFFSET(%r15), %rax
    cmpq %rax, CTXREG_SCS_BOUND_OFFSET(%r15)
    jb .Lscs_unwind_bad
    movq %rax, CTXREG_SCS_OFFSET(%r15)
    jmpq *%r11
.Lscs_unwind_bad:
    ud2
#endif

#ifndef __APPLE__
.section .note.GNU-stack,"",@progbits
#endif
