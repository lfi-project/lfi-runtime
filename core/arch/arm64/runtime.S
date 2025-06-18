#include "arch_asm.h"

.text

// lfi_ctx_entry(struct LFIContext *ctx, uintptr_t *host_sp_ptr, uintptr_t entry)
.global lfi_ctx_entry
.p2align 4
lfi_ctx_entry:
    // save callee-saved registers to stack
    stp d8, d9,   [sp, #-16]!
    stp d10, d11, [sp, #-16]!
    stp d12, d13, [sp, #-16]!
    stp d14, d15, [sp, #-16]!
    // x18 is not callee-saved but we save/restore x18 anyway
    stp xzr, x18, [sp, #-16]!
    stp x19, x20, [sp, #-16]!
    stp x21, x22, [sp, #-16]!
    stp x23, x24, [sp, #-16]!
    stp x25, x26, [sp, #-16]!
    stp x27, x28, [sp, #-16]!
    stp x29, x30, [sp, #-16]!
    // save stack to host_sp_pointer
    mov x8, sp
    str x8, [x1]
    // put entrypoint in x30
    mov x30, x2

    // set the sandbox thread control block by saving the old value of
    // [tpidr_el0, TLS_SLOT_LFI] into the context and setting it to point to
    // the context itself.
    get_ctx x8
    str x8, [x0, REGS_HOST_TP]
    write_ctx x0, x8

    // load all registers
    ldp x2, x3,   [x0, REGS_X0+16*1]
    ldp x4, x5,   [x0, REGS_X0+16*2]
    ldp x6, x7,   [x0, REGS_X0+16*3]
    ldp x8, x9,   [x0, REGS_X0+16*4]
    ldp x10, x11, [x0, REGS_X0+16*5]
    ldp x12, x13, [x0, REGS_X0+16*6]
    ldp x14, x15, [x0, REGS_X0+16*7]
    ldp x16, x17, [x0, REGS_X0+16*8]
    ldp x18, x19, [x0, REGS_X0+16*9]
    ldp x20, x21, [x0, REGS_X0+16*10]
    ldp x22, x23, [x0, REGS_X0+16*11]
    ldp x24, x25, [x0, REGS_X0+16*12]
    ldp x26, x27, [x0, REGS_X0+16*13]
    ldp x28, x29, [x0, REGS_X0+16*14]
    ldp xzr, x1,  [x0, REGS_X0+16*15]
    mov sp, x1
    ldp x0, x1, [x0, REGS_X0+16*0]
    // zero vector registers
    movi v0.2D, #0
    movi v1.2D, #0
    movi v2.2D, #0
    movi v3.2D, #0
    movi v4.2D, #0
    movi v5.2D, #0
    movi v6.2D, #0
    movi v7.2D, #0
    movi v8.2D, #0
    movi v9.2D, #0
    movi v10.2D, #0
    movi v11.2D, #0
    movi v12.2D, #0
    movi v13.2D, #0
    movi v14.2D, #0
    movi v15.2D, #0
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
    // NOTE: x30 must contain a valid sandbox address
    add x30, REG_BASE, w30, uxtw
    ret

// lfi_ctx_end(struct LFIContext *ctx, int val)
.global lfi_ctx_end
.p2align 4
lfi_ctx_end:
    // restore host_sp
    ldr x19, [x0, REGS_HOST_SP]
    mov sp, x19
    // restore host_tp
    ldr x19, [x0, REGS_HOST_TP]
    write_ctx x19, x20
    // put val into return register
    mov x0, x1
    ldp x29, x30, [sp], 16
    ldp x27, x28, [sp], 16
    ldp x25, x26, [sp], 16
    ldp x23, x24, [sp], 16
    ldp x21, x22, [sp], 16
    ldp x19, x20, [sp], 16
    ldp xzr, x18, [sp], 16
    ldp d14, d15, [sp], 16
    ldp d12, d13, [sp], 16
    ldp d10, d11, [sp], 16
    ldp d8, d9,   [sp], 16
    ret

.global lfi_syscall_entry
.p2align 4
lfi_syscall_entry:
    get_ctx REG_BASE
    // save registers
    stp x0, x1,   [REG_BASE, REGS_X0+16*0]
    stp x2, x3,   [REG_BASE, REGS_X0+16*1]
    stp x4, x5,   [REG_BASE, REGS_X0+16*2]
    stp x6, x7,   [REG_BASE, REGS_X0+16*3]
    stp x8, x9,   [REG_BASE, REGS_X0+16*4]
    stp x10, x11, [REG_BASE, REGS_X0+16*5]
    stp x12, x13, [REG_BASE, REGS_X0+16*6]
    stp x14, x15, [REG_BASE, REGS_X0+16*7]
    stp x16, x17, [REG_BASE, REGS_X0+16*8]
    str REG_ADDR, [REG_BASE, REGS_ADDR]
    mov x1, sp
    stp x30, x1,  [REG_BASE, REGS_X0+16*15]
    stp q0,  q1,  [REG_BASE, REGS_VECTOR+32*0]
    stp q2,  q3,  [REG_BASE, REGS_VECTOR+32*1]
    stp q4,  q5,  [REG_BASE, REGS_VECTOR+32*2]
    stp q6,  q7,  [REG_BASE, REGS_VECTOR+32*3]
    stp q8,  q9,  [REG_BASE, REGS_VECTOR+32*4]
    stp q10, q11, [REG_BASE, REGS_VECTOR+32*5]
    stp q12, q13, [REG_BASE, REGS_VECTOR+32*6]
    stp q14, q15, [REG_BASE, REGS_VECTOR+32*7]
    stp q16, q17, [REG_BASE, REGS_VECTOR+32*8]
    stp q18, q19, [REG_BASE, REGS_VECTOR+32*9]
    stp q20, q21, [REG_BASE, REGS_VECTOR+32*10]
    stp q22, q23, [REG_BASE, REGS_VECTOR+32*11]
    stp q24, q25, [REG_BASE, REGS_VECTOR+32*12]
    stp q26, q27, [REG_BASE, REGS_VECTOR+32*13]
    stp q28, q29, [REG_BASE, REGS_VECTOR+32*14]
    stp q30, q31, [REG_BASE, REGS_VECTOR+32*15]

    // swap the thread control block back to the host one
    ldr x0, [REG_BASE, REGS_HOST_TP]
    write_ctx x0, x1

    // restore host stack and call handler
    ldr x0, [REG_BASE, REGS_HOST_SP]
    mov sp, x0
    str REG_BASE, [sp, #-16]! // push
    mov x0, REG_BASE
    bl lfi_syscall_handler
    ldr x0, [sp], 16 // pop

    // swap thread control block back to context pointer
    write_ctx x0, x1

    // restore registers
    ldp x2, x3,   [x0, REGS_X0+16*1]
    ldp x4, x5,   [x0, REGS_X0+16*2]
    ldp x6, x7,   [x0, REGS_X0+16*3]
    ldp x8, x9,   [x0, REGS_X0+16*4]
    ldp x10, x11, [x0, REGS_X0+16*5]
    ldp x12, x13, [x0, REGS_X0+16*6]
    ldp x14, x15, [x0, REGS_X0+16*7]
    ldp x16, x17, [x0, REGS_X0+16*8]
    ldr REG_ADDR, [x0, REGS_ADDR]
    ldr REG_BASE, [x0, REGS_BASE]
    ldp x30, x1,  [x0, REGS_X0+16*15]
    ldp q0,  q1,  [x0, REGS_VECTOR+32*0]
    ldp q2,  q3,  [x0, REGS_VECTOR+32*1]
    ldp q4,  q5,  [x0, REGS_VECTOR+32*2]
    ldp q6,  q7,  [x0, REGS_VECTOR+32*3]
    ldp q8,  q9,  [x0, REGS_VECTOR+32*4]
    ldp q10, q11, [x0, REGS_VECTOR+32*5]
    ldp q12, q13, [x0, REGS_VECTOR+32*6]
    ldp q14, q15, [x0, REGS_VECTOR+32*7]
    ldp q16, q17, [x0, REGS_VECTOR+32*8]
    ldp q18, q19, [x0, REGS_VECTOR+32*9]
    ldp q20, q21, [x0, REGS_VECTOR+32*10]
    ldp q22, q23, [x0, REGS_VECTOR+32*11]
    ldp q24, q25, [x0, REGS_VECTOR+32*12]
    ldp q26, q27, [x0, REGS_VECTOR+32*13]
    ldp q28, q29, [x0, REGS_VECTOR+32*14]
    ldp q30, q31, [x0, REGS_VECTOR+32*15]
    mov sp, x1
    ldp x0, x1,   [x0, REGS_X0+16*0]
    add x30, REG_BASE, w30, uxtw
    ret

.global lfi_get_tp
.p2align 4
lfi_get_tp:
    get_ctx x0
    ldr x0, [x0, REGS_TP]
    add x30, REG_BASE, w30, uxtw
    ret

.global lfi_set_tp
.p2align 4
lfi_set_tp:
    get_ctx REG_BASE
    str x0, [REG_BASE, REGS_TP]
    // reload REG_BASE
    ldr REG_BASE, [REG_BASE, REGS_BASE]
    add x30, REG_BASE, w30, uxtw
    ret

#ifndef __APPLE__
.section .note.GNU-stack,"",@progbits
#endif
