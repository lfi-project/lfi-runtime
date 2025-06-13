#include "arch_asm.h"

.macro get_ctx reg
    movq %fs:0x0, \reg
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

.global lfi_trampoline
.p2align 4
lfi_trampoline:
    // save callee-saved registers
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %rbx
    pushq %rbp

    get_invoke_info %r13
    movq INVOKE_CTX(%r13), %r11

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
    wrgsbase %REG_BASE

    // load address of the lfi_retfn function that will make the return rtcall
    movq %fs:0x0, %r11
    movq INVOKE_RETFN(%r13), %r11
    // align stack
    andq $0xfffffffffffffff0, %rsp
    // push this as the return address onto the sandbox stack
    pushq %r11

    // load address of the target function
    movq INVOKE_TARGETFN(%r13), %r11
    // apply mask just to be safe
    andl $0xffffffe0, %r11d
    addq %REG_BASE, %r11

    // set the sandbox thread control block by saving the old value of %fs:0x0
    // into the context and setting it to point to the context itself
    movq %fs:0x0, %r12
    movq INVOKE_CTX(%r13), %r13
    movq %r12, REGS_HOST_TP(%r13)
    movq %r13, %fs:0x0

    // this function should return via a runtime call
    jmpq *%r11
    int3

// Accelerated return for library sandboxes.
.p2align 4
.global lfi_ret
lfi_ret:
    get_ctx %r11
    // restore host sp
    movq REGS_HOST_SP(%r11), %rsp
    // restore host tp
    movq REGS_HOST_TP(%r11), %rbp
    movq %rbp, %fs:0x0
    // restore sandbox sp to its initial value
    popq %rbp
    movq %rbp, REGS_RSP(%r11)
    popq %rbp // pop dummy value
    // return value should already be in %rax
    popq %rbp
    popq %rbx
    popq %r12
    popq %r13
    popq %r14
    popq %r15
    ret

#ifndef __APPLE__
.section .note.GNU-stack,"",@progbits
#endif
