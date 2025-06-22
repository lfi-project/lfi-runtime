#include "arch_asm.h"

.text

.macro zero_caller_regs
    // Skip x0 and x1 because return value could be there.
    mov x2, #0
    mov x3, #0
    mov x4, #0
    mov x5, #0
    mov x6, #0
    mov x7, #0
    // TODO: can an indirect result be placed in x8?
    mov x8, #0
    mov x9, #0
    // skip x10 (target)
    mov x11, #0
    mov x12, #0
    mov x13, #0
    mov x14, #0
    mov x15, #0
    mov x16, #0
    mov x17, #0
    // TODO: consider skipping x18 because it is platform-reserved.
    mov x18, #0
    // Skip v0 and v1 because return value could be there.
    movi v2.2D, #0
    movi v3.2D, #0
    movi v4.2D, #0
    movi v5.2D, #0
    movi v6.2D, #0
    movi v7.2D, #0
    mov v8.d[1], xzr
    mov v9.d[1], xzr
    mov v10.d[1], xzr
    mov v11.d[1], xzr
    mov v12.d[1], xzr
    mov v13.d[1], xzr
    mov v14.d[1], xzr
    mov v15.d[1], xzr
    movi v16.2D, #0
    movi v17.2D, #0
    movi v18.2D, #0
    movi v19.2D, #0
    movi v20.2D, #0
    movi v21.2D, #0
    movi v22.2D, #0
    movi v23.2D, #0
    movi v24.2D, #0
    movi v25.2D, #0
    movi v26.2D, #0
    movi v27.2D, #0
    movi v28.2D, #0
    movi v29.2D, #0
    movi v30.2D, #0
    movi v31.2D, #0
.endm

.global lfi_callback
.p2align 4
lfi_callback:
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
    // Reload host x18
    ldr x18, [x9, REGS_X(18)]
    mov x23, x18
    blr x16
    // Restore x18 and x9.
    mov x18, x23
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

#ifndef STORES_ONLY_TRAMPOLINE
    zero_caller_regs
#endif

    // Restore valid REG_ADDR
    mov REG_ADDR, REG_BASE

    add x30, REG_BASE, w30, uxtw
    ret

#ifndef __APPLE__
.section .note.GNU-stack,"",@progbits
#endif
