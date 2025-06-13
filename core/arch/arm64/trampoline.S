#include "arch_asm.h"

.macro get_ctx reg
    mrs \reg, tpidr_el0
.endm

// Returns a pointer to lfi_invoke_info in reg.
.macro get_invoke_info reg
#if defined(ENABLE_TLS_EXEC)
    adrp x11, :gottprel:lfi_invoke_info
    ldr x11, [x11, :gottprel_lo12:lfi_invoke_info]
    mrs x12, tpidr_el0
    add \reg, x12, x11
#else
    mov x11, x0
    mov x12, x30

    adrp x0, :tlsdesc:lfi_invoke_info
    ldr x8, [x0, :tlsdesc_lo12:lfi_invoke_info]
    add x0, x0, :tlsdesc_lo12:lfi_invoke_info
    .tlsdesccall lfi_invoke_info
    blr x8
    mrs x8, tpidr_el0
    add x8, x8, x0

    mov x30, x12
    mov x0, x11
#endif
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

    get_invoke_info x8
    ldr x9, [x8, #INVOKE_RETFN]
    ldr x10, [x8, #INVOKE_TARGETFN]
    ldr x8, [x8, #INVOKE_CTX]

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
    // clear x8, x9
    mov x8, #0
    mov x9, #0
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
