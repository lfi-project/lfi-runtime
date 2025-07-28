#include "arch_asm.h"

.macro zero_regs
    fldcw REGS_FCW(%r13)
    ldmxcsr REGS_MXCSR(%r13)
    xorq %rax, %rax
    xorq %rbx, %rbx
    xorq %rbp, %rbp
    xorq %r10, %r10
    // skip %r11 (target)
    xorq %r12, %r12
    xorq %r13, %r13
    // skip %r14 (base)
    xorq %r15, %r15
    xorps %xmm8, %xmm8
    xorps %xmm9, %xmm9
    xorps %xmm10, %xmm10
    xorps %xmm11, %xmm11
    xorps %xmm12, %xmm12
    xorps %xmm13, %xmm13
    xorps %xmm14, %xmm14
    xorps %xmm15, %xmm15
.endm

// We need to access the invoke info through a TLS variable. The most efficient
// TLS access works when the TLS model is initial-exec, but this is only usable
// when this code is in a library that is loaded at execution-time, making it
// unsuitable for libraries that are opened with dlopen.
//
// The most efficient sequence that works with dlopen uses x86-64 support for
// TLS descriptors, which can be enabled by compiling with -mtls-dialect=gnu2.
// In this model, the TLS lookup involves a function call that has a special
// calling convention, allowing us to avoid saving/restoring registers other
// than %rax. However, TLS descriptors may not always be available (e.g., when
// Bionic is the libc).
//
// The fallback is the standard x86-64 TLS global-dynamic access, which
// involves a normal function call to __tls_get_addr, requiring a full
// save/restore of all argument registers being used by the invocation.
.macro get_invoke_info reg
#if defined(ENABLE_TLS_EXEC)
    movq %fs:0x0, \reg
    addq lfi_invoke_info@gottpoff(%rip), \reg
#elif defined(ENABLE_TLS_DESC)
    pushq %rax
    leaq lfi_invoke_info@TLSDESC(%rip), %rax
    callq *lfi_invoke_info@TLSCALL(%rax)
    addq %fs:0x0, %rax
    movq %rax, \reg
    popq %rax
#else
    // Save/restore registers used for arguments.
    pushq %rdi
    pushq %rsi
    pushq %rdx
    pushq %rcx
    pushq %r8
    pushq %r9
    subq $136, %rsp
    movdqu %xmm0, 0(%rsp)
    movdqu %xmm1, 16(%rsp)
    movdqu %xmm2, 32(%rsp)
    movdqu %xmm3, 48(%rsp)
    movdqu %xmm4, 64(%rsp)
    movdqu %xmm5, 80(%rsp)
    movdqu %xmm6, 96(%rsp)
    movdqu %xmm7, 112(%rsp)

    .byte 0x66
    leaq lfi_invoke_info@TLSGD(%rip), %rdi
    .byte 0x66, 0x66
    rex64 callq __tls_get_addr@PLT
    movq %rax, \reg

    movdqu 0(%rsp), %xmm0
    movdqu 16(%rsp), %xmm1
    movdqu 32(%rsp), %xmm2
    movdqu 48(%rsp), %xmm3
    movdqu 64(%rsp), %xmm4
    movdqu 80(%rsp), %xmm5
    movdqu 96(%rsp), %xmm6
    movdqu 112(%rsp), %xmm7
    addq $136, %rsp
    popq %r9
    popq %r8
    popq %rcx
    popq %rdx
    popq %rsi
    popq %rdi
#endif
.endm

.macro lazy_ctx_clone
    // Check if the context pointer is zero.
    cmpq $0, (%r11)
    jne 1f
    // Initialize the context pointer with the clone callback.
    call .trampoline_clone
1:
    // Load the context pointer from struct LFIContext **ctx.
    movq (%r11), %r11
.endm

.p2align 4
.global lfi_trampoline_struct
lfi_trampoline_struct:
    endbr64
    // save callee-saved registers
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %rbx
    pushq %rbp

    get_invoke_info %r13
    movq INVOKE_TARGETFN(%r13), %r12
    // Loads a pointer to the context pointer.
    movq INVOKE_BOX(%r13), %r14
    movq INVOKE_CTX(%r13), %r13
    movq %r13, %r11
    movq $0, %r15

    lazy_ctx_clone

    movq REGS_RDI(%r11), %rdi
    movq REGS_RSI(%r11), %rsi
    movq REGS_RDX(%r11), %rdx
    movq REGS_RCX(%r11), %rcx
    movq REGS_R8(%r11), %r8
    movq REGS_R9(%r11), %r9
    movdqu REGS_XMM(0)(%r11), %xmm0
    movdqu REGS_XMM(1)(%r11), %xmm1
    movdqu REGS_XMM(2)(%r11), %xmm2
    movdqu REGS_XMM(3)(%r11), %xmm3
    movdqu REGS_XMM(4)(%r11), %xmm4
    movdqu REGS_XMM(5)(%r11), %xmm5
    movdqu REGS_XMM(6)(%r11), %xmm6
    movdqu REGS_XMM(7)(%r11), %xmm7

    jmp .lfi_trampoline_common

.global lfi_trampoline
.p2align 4
lfi_trampoline:
    endbr64
    // save callee-saved registers
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %rbx
    pushq %rbp

    get_invoke_info %r13
    movq INVOKE_TARGETFN(%r13), %r12
    // Loads a pointer to the context pointer.
    movq INVOKE_BOX(%r13), %r14
    movq INVOKE_CTX(%r13), %r13
    movq %r13, %r11
    movq $0, %r15

    lazy_ctx_clone

    jmp .lfi_trampoline_common

// Direct trampoline that doesn't involve a thread-local lookup. Instead the
// info usually passed via the lfi_invoke_info thread-local are passed via
// the stack:
// (%rsp+24): n_stack (unused)
// (%rsp+16): lfiptr target
// (%rsp+8): struct LFIContext *box
// (%rsp): struct LFIContext **ctxp
.global lfi_trampoline_direct
.p2align 4
lfi_trampoline_direct:
    // ctxp
    popq %r11
    // box
    popq %r10

    // save callee-saved registers
    xchgq %r15, (%rsp)
    // xchgq %r14, -8(%rsp) not included yet
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %rbx
    pushq %rbp

    // Put arguments where we expect them.
    movq %r15, %r12 // target
    movq %r11, %r13 // ctxp
    movq %r14, %r15 // n_stack
    movq %r10, %r14 // box

    lazy_ctx_clone

.lfi_trampoline_common:
    // dummy push to keep the stack aligned
    pushq %rbp
    // push sandbox rsp
    pushq REGS_RSP(%r11)

    // save current host stack
    movq %rsp, REGS_HOST_SP(%r11)
    // load sandbox rsp
    movq REGS_RSP(%r11), %rsp
    // load sandbox base
    movq REGS_BASE(%r11), %REG_BASE
    // also write base to %gs
#ifdef ENABLE_SEGUE
# ifndef SEGUE_SINGLE_SANDBOX
    wrgsbase %REG_BASE
# endif
#endif

    // load address of the lfi_retfn function that will make the return rtcall
    movq REGS_RETADDR(%r11), %r11
    // align stack
    andq $0xfffffffffffffff0, %rsp
    // push this as the return address onto the sandbox stack
    pushq %r11

    // load address of the target function
    movq %r12, %r11
    // apply mask just to be safe
    andl $0xffffffe0, %r11d
    addq %REG_BASE, %r11

    // set the sandbox thread control block by saving the old value of
    // %fs:TLS_SLOT_LFI into the context and setting it to point to the context
    // itself
    get_ctx %r12
    movq (%r13), %r13
    movq %r12, REGS_HOST_TP(%r13)
    write_ctx %r13

#ifdef HAVE_PKU
    movq %rcx, %rbx
    movq %rdx, %rbp
    pku_box_access REGS_PKEY(%r13)
    movq %rbp, %rdx
    movq %rbx, %rcx
#endif

#ifndef STORES_ONLY
    // Zero non-argument registers for full LFI.
    zero_regs
#endif

    // this function should return via a runtime call
    jmpq *%r11
    int3

// Accelerated return for library sandboxes.
.p2align 4
.global lfi_ret
lfi_ret:
#ifdef HAVE_PKU
    movq %rax, %rdi
    pku_all_access
    movq %rdi, %rax
#endif
    get_ctx %rdi
.global lfi_ret_end
lfi_ret_end:
    // restore host sp
    movq REGS_HOST_SP(%rdi), %rsp
    // restore host tp
    movq REGS_HOST_TP(%rdi), %rbp
    write_ctx %rbp
    // save return registers
    movq %rax, REGS_RAX(%rdi)
    movq %rdx, REGS_RDX(%rdi)
    movdqu %xmm0, REGS_XMM(0)(%rdi)
    movdqu %xmm1, REGS_XMM(1)(%rdi)
    // restore sandbox sp to its initial value
    popq %rbp
    movq %rbp, REGS_RSP(%rdi)
    popq %rbp // pop dummy value
    // return value should already be in %rax
    popq %rbp
    popq %rbx
    popq %r12
    popq %r13
    popq %r14
    popq %r15
    ret

// TODO: we can make this more efficient with one rsp subtraction.
.macro pushx xmm
    subq $16, %rsp
    movdqu \xmm, (%rsp)
.endm

.macro popx xmm
    movdqu (%rsp), \xmm
    addq $16, %rsp
.endm

// Slowpath taken when the sandbox context needs to be initialized.
.p2align 4
.trampoline_clone:
    pushq %rdi
    pushq %rsi
    pushq %rdx
    pushq %rcx
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushx %xmm0
    pushx %xmm1
    pushx %xmm2
    pushx %xmm3
    pushx %xmm4
    pushx %xmm5
    pushx %xmm6
    pushx %xmm7
    movq %r14, %rdi
    movq %r11, %rsi
    callq lfi_clone
    popx %xmm0
    popx %xmm1
    popx %xmm2
    popx %xmm3
    popx %xmm4
    popx %xmm5
    popx %xmm6
    popx %xmm7
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rcx
    popq %rdx
    popq %rsi
    popq %rdi
    ret

#ifndef __APPLE__
.section .note.GNU-stack,"",@progbits
#endif
