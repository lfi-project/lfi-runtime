#include "arch_asm.h"

.macro get_ctx reg
    movq %fs:0x0, \reg
.endm

.text

.global lfi_callback
lfi_callback:
    // Entrypoint for callbacks, with the target in %r10.
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
    movq %r12, %fs:0x0

    // Save %r11 across the call (and stack alignment).
    pushq %r11
    callq *%r10
    popq %r11

    // TODO: for full sandboxing, zero all non-return registers.

    // Set the sandbox thread control block to the context pointer.
    movq %r11, %fs:0x0
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

    // Return to sandbox

    // NOTE: This pop instruction could fault.
.global lfi_callback_userpop
lfi_callback_userpop:
    popq %r11
    andl $0xffffffe0, %r11d
    addq %REG_BASE, %r11
    jmpq *%r11

#ifndef __APPLE__
.section .note.GNU-stack,"",@progbits
#endif
