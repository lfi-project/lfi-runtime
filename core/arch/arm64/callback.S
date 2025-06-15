#include "arch_asm.h"

.text

.global lfi_callback
lfi_callback:
    // REG_ADDR was used to branch here, reset it to a valid value.
    mov REG_ADDR, REG_BASE

    get_ctx x9

    // Save and replace tpidr_el0 with host value.
    ldr x12, [x9, REGS_HOST_TP]
    write_ctx x12, x11

    // Get current sandbox sp.
    mov x12, sp
    // Enable host sp.
    ldr x11, [x9, REGS_HOST_SP]
    mov sp, x11
    // Save previous sandbox sp and return address to stack.
    ldr x11, [x9, REGS_SP]
    stp x11, x30, [sp, #-16]!
    // Save current sandbox sp to context.
    str x12, [x9, REGS_SP]
    // Save context pointer in callee-saved register.

    mov x22, x9
    blr x16
    mov x9, x22

    // Read previous sandbox sp and return address from stack.
    ldp x11, x30, [sp], 16
    // Read current sandbox sp from context.
    ldr x12, [x9, REGS_SP]
    // Restore previous sandbox sp in context.
    str x11, [x9, REGS_SP]
    // Restore the saved host sp.
    mov x11, sp
    str x11, [x9, REGS_HOST_SP]
    // Restore sp to current sandbox sp.
    mov sp, x12

    // Restore sandbox thread control block to point to context.
    write_ctx x9, x10

    // TODO: zero caller-saved registers that may have secret values in LFI
    // full mode.

    add x30, x21, w30, uxtw
    ret

#ifndef __APPLE__
.section .note.GNU-stack,"",@progbits
#endif
