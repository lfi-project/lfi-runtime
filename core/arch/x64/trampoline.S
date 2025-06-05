#include "arch_asm.h"

.macro get_ctx reg
    movq %fs:0x0, \reg
.endm

// TODO: TLS accesses in this file assume an initial-exec TLS model. This will
// not work if liblfi is included in a dlopened library.

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

    // load lfi_myctx thread-local
    movq lfi_ctx@gottpoff(%rip), %r11
    movq %fs:(%r11), %r11

    // dummy push to keep the stack aligned
    pushq %rbp
    // push user rsp
    pushq REGS_RSP(%r11)

    // save current host stack
    movq %rsp, REGS_HOST_SP(%r11)
    // load user rsp
    movq REGS_RSP(%r11), %rsp
    // load sandbox base
    movq REGS_BASE(%r11), %REG_BASE
    // also write base to %gs
    wrgsbase %REG_BASE

    // load address of the lfi_retfn function that will make the return rtcall
    movq lfi_retfn@gottpoff(%rip), %r11
    movq %fs:(%r11), %r11
    // align stack
    andq $0xfffffffffffffff0, %rsp
    // push this as the return address onto the user stack
    pushq %r11

    // load address of the target function
    movq lfi_targetfn@gottpoff(%rip), %r11
    movq %fs:(%r11), %r11
    // apply mask just to be safe
    andl $0xffffffe0, %r11d
    addq %REG_BASE, %r11

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
    // restore user sp to its initial value
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
