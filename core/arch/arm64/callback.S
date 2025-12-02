#include "arch_asm.h"

.text

.macro zero_caller_regs
    // skip x0 and x1 because return value could be there.
    mov x2, #0
    mov x3, #0
    mov x4, #0
    mov x5, #0
    mov x6, #0
    mov x7, #0
    // skip x8 due to indirect result
    mov x9, #0
    // skip x10 (target)
    mov x11, #0
    mov x12, #0
    mov x13, #0
    mov x14, #0
    mov x15, #0
    mov x16, #0
    mov x17, #0
    // skip x18 because it is platform-reserved
    // skip v0 and v1 because return value could be there.
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
    // zero integer and fp flags
    ands xzr, xzr, xzr
    msr fpsr, xzr
.endm

.macro lfi_callback_gen with_struct
    get_ctx x9

    // Save and replace tpidr_el0 with host value.
    ldr x12, [x9, REGS_HOST_TP]
    write_ctx x12, x11

.if \with_struct
    str x0, [x9, #REGS_X(0)]
    str x1, [x9, #REGS_X(1)]
    str x2, [x9, #REGS_X(2)]
    str x3, [x9, #REGS_X(3)]
    str x4, [x9, #REGS_X(4)]
    str x5, [x9, #REGS_X(5)]
    str x6, [x9, #REGS_X(6)]
    str x7, [x9, #REGS_X(7)]
    str x8, [x9, #REGS_X(8)]
    str d0, [x9, #REGS_V(0)]
    str d1, [x9, #REGS_V(1)]
    str d2, [x9, #REGS_V(2)]
    str d3, [x9, #REGS_V(3)]
    str d4, [x9, #REGS_V(4)]
    str d5, [x9, #REGS_V(5)]
    str d6, [x9, #REGS_V(6)]
    str d7, [x9, #REGS_V(7)]
.endif

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

    // Check if abort_callback is set.
    ldr x10, [x9, CTX_ABORT_CALLBACK]
    cmp x10, #1
    bne 1f

    // Clear abort_callback and set abort_status.
    str xzr, [x9, CTX_ABORT_CALLBACK]
    mov x10, #1
    str x10, [x9, CTX_ABORT_STATUS]
    // Call lfi_ret_end with ctx as argument.
    mov x0, x9
    bl lfi_ret_end
1:

.if \with_struct
    ldr x0, [x9, #REGS_X(0)]
    ldr x1, [x9, #REGS_X(1)]
    ldr x8, [x9, #REGS_X(8)]
    ldr d0, [x9, #REGS_V(0)]
    ldr d1, [x9, #REGS_V(1)]
.endif

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

#ifndef STORES_ONLY
    zero_caller_regs
#endif

    // Restore valid REG_ADDR
    mov REG_ADDR, REG_BASE

    add x30, REG_BASE, w30, uxtw
    ret
.endm

.p2align 4
.global lfi_callback
lfi_callback:
    lfi_callback_gen 0

.p2align 4
.global lfi_callback_struct
lfi_callback_struct:
    lfi_callback_gen 1

#ifndef __APPLE__
.section .note.GNU-stack,"",@progbits
#endif
