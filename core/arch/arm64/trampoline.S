#include "arch_asm.h"

.macro get_ctx reg
    mrs \reg, tpidr_el0
.endm

.macro get_tls_vars
    mov x11, x0
    mov x12, x30
    mrs x13, tpidr_el0

    // lfi_ctx in x8
    adrp x0, :tlsdesc:lfi_ctx
    ldr x8, [x0, :tlsdesc_lo12:lfi_ctx]
    add x0, x0, :tlsdesc_lo12:lfi_ctx
    .tlsdesccall lfi_ctx
    blr x8
    ldr x8, [x13, x0]

    // lfi_retfn in x9
    adrp x0, :tlsdesc:lfi_retfn
    ldr x9, [x0, :tlsdesc_lo12:lfi_retfn]
    add x0, x0, :tlsdesc_lo12:lfi_retfn
    .tlsdesccall lfi_retfn
    blr x9
    ldr x9, [x13, x0]

    // lfi_targetfn in x10
    adrp x0, :tlsdesc:lfi_targetfn
    ldr x10, [x0, :tlsdesc_lo12:lfi_targetfn]
    add x0, x0, :tlsdesc_lo12:lfi_targetfn
    .tlsdesccall lfi_targetfn
    blr x10
    ldr x10, [x13, x0]

    mov x30, x12
    mov x0, x11
.endm

.global lfi_trampoline
.p2align 4
lfi_trampoline:
    stp d8, d9,   [sp, #-16]!
    stp d10, d11, [sp, #-16]!
    stp d12, d13, [sp, #-16]!
    stp d14, d15, [sp, #-16]!
    stp x19, x20, [sp, #-16]!
    stp x21, x22, [sp, #-16]!
    stp x23, x24, [sp, #-16]!
    stp x25, x26, [sp, #-16]!
    stp x27, x28, [sp, #-16]!
    stp x29, x30, [sp, #-16]!
    
    // TODO: if LFI-full, zero the non-argument registers

    get_tls_vars

    // set the sandbox thread control block by saving the old value of
    // tpidr_el0 into the context and setting it to point to the context
    // itself.
    mrs x12, tpidr_el0
    str x12, [x8, REGS_HOST_TP]
    msr tpidr_el0, x8

    // save user sp to stack
    ldr x12, [x8, REGS_SP]
    stp x12, xzr, [sp, #-16]!

    // save host stack
    mov x11, sp
    str x11, [x8, REGS_HOST_SP]

    // set guest stack
    mov sp, x12
    // set return address to lfi_retfn
    mov x30, x9
    // set base pointer
    ldr REG_BASE, [x8, REGS_BASE]
    // set address pointer to a valid value
    mov REG_ADDR, REG_BASE
    // clear x8
    mov x8, #0
    // enter sandbox
    add x10, REG_BASE, w10, uxtw
    br x10
    // the previous call should return through lfi_ret
    udf #0

.global lfi_ret
.p2align 4
lfi_ret:
    get_ctx REG_BASE
    // restore host thread control block
    ldr x19, [REG_BASE, REGS_HOST_TP]
    msr tpidr_el0, x19
    // restore host stack
    ldr x19, [REG_BASE, REGS_HOST_SP]
    mov sp, x19
    // restore the user sp
    ldp x19, xzr, [sp], 16
    str x19, [REG_BASE, REGS_SP]
    // restore callee-saved registers
    ldp x29, x30, [sp], 16
    ldp x27, x28, [sp], 16
    ldp x25, x26, [sp], 16
    ldp x23, x24, [sp], 16
    ldp x21, x22, [sp], 16
    ldp x19, x20, [sp], 16
    ldp d14, d15, [sp], 16
    ldp d12, d13, [sp], 16
    ldp d10, d11, [sp], 16
    ldp d8, d9,   [sp], 16
    ret

#ifndef __APPLE__
.section .note.GNU-stack,"",@progbits
#endif
