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
    // restore host_sp
    movq REGS_HOST_SP(%rdi), %rsp
#ifndef CTXREG
    // restore host_tp
    movq REGS_HOST_TP(%rdi), %rbp
    write_ctx %rbp
#endif
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
#ifdef HAVE_PKU
    pushq %rax
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

#ifdef CTXREG

// Shadow call stack runtime calls.
//
// The SCS pointer lives at 16(%r15) (ctxreg[2]). These runtime calls manage
// an internal save stack in the LFIContext for setjmp/longjmp and C++ exception
// unwinding support. All registers except %rax (output for save) are preserved.
//
// We switch to the host stack (via ctx->regs.host_sp) so we can freely
// push/pop to save registers.

// SCS save: push current SCS pointer onto save stack, return index in %rax.
.global lfi_scs_save
.p2align 4
lfi_scs_save:
    // Switch to host stack.
    movq %rsp, 24(%r15)                        // save sandbox rsp to temp slot
    movq (%r15), %rsp                           // rsp = ctx (ctxreg[0])
    movq REGS_HOST_SP(%rsp), %rsp               // rsp = host stack

    pushq %rcx
    pushq %rdi

    movq (%r15), %rdi                           // rdi = ctx
    movq CTX_SCS_SAVE_SP(%rdi), %rcx            // rcx = save_sp

    // Bounds check: save_sp < &scs_save_stack[32]
    leaq (CTX_SCS_SAVE_STACK + 32*8)(%rdi), %rax
    cmpq %rax, %rcx
    jae .Lscs_save_full

    // *save_sp = current SCS pointer
    movq 16(%r15), %rax
    movq %rax, (%rcx)

    // Advance save_sp
    addq $8, %rcx
    movq %rcx, CTX_SCS_SAVE_SP(%rdi)

    // Compute index = (save_sp_old - &scs_save_stack[0]) / 8
    subq $8, %rcx                               // rcx = save_sp before advance
    leaq CTX_SCS_SAVE_STACK(%rdi), %rax
    subq %rax, %rcx                             // byte offset
    shrq $3, %rcx                               // index
    movq %rcx, %rax                             // return value

    popq %rdi
    popq %rcx
    movq 24(%r15), %rsp
    jmpq *%r11

.Lscs_save_full:
    movq $-1, %rax
    popq %rdi
    popq %rcx
    movq 24(%r15), %rsp
    jmpq *%r11

// SCS restore: restore SCS pointer from save stack at index %edi, pop above.
.global lfi_scs_restore
.p2align 4
lfi_scs_restore:
    movq %rsp, 24(%r15)
    movq (%r15), %rsp
    movq REGS_HOST_SP(%rsp), %rsp

    pushq %rax
    pushq %rcx

    movq (%r15), %rax                           // rax = ctx

    // Bounds check: index < 32 (unsigned)
    cmpl $32, %edi
    jae .Lscs_restore_oob

    // Compute &save_stack[index]
    movl %edi, %ecx                             // zero-extend index
    leaq CTX_SCS_SAVE_STACK(%rax,%rcx,8), %rcx

    // Set save_sp = &save_stack[index] (invalidate index and above)
    movq %rcx, CTX_SCS_SAVE_SP(%rax)

    // Restore SCS pointer from save_stack[index]
    movq (%rcx), %rcx
    movq %rcx, 16(%r15)

.Lscs_restore_oob:
    popq %rcx
    popq %rax
    movq 24(%r15), %rsp
    jmpq *%r11

// SCS unwind: pop n entries from SCS (add n*8 to SCS pointer), clamped to limit.
.global lfi_scs_unwind
.p2align 4
lfi_scs_unwind:
    movq %rsp, 24(%r15)
    movq (%r15), %rsp
    movq REGS_HOST_SP(%rsp), %rsp

    pushq %rax
    pushq %rcx

    // Compute new SCS pointer
    movl %edi, %eax                             // zero-extend count
    shlq $3, %rax                               // n * 8
    movq 16(%r15), %rcx
    addq %rax, %rcx                             // new SCS pointer

    // Bounds check: clamp to scs_limit
    movq (%r15), %rax                           // ctx
    cmpq CTX_SCS_LIMIT(%rax), %rcx
    jbe .Lscs_unwind_ok
    movq CTX_SCS_LIMIT(%rax), %rcx              // clamp
.Lscs_unwind_ok:
    movq %rcx, 16(%r15)

    popq %rcx
    popq %rax
    movq 24(%r15), %rsp
    jmpq *%r11

#endif

#ifndef __APPLE__
.section .note.GNU-stack,"",@progbits
#endif
