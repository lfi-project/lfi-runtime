#include "arch_asm.h"

.text

// lfi_ctx_entry(struct LFIContext *ctx, uintptr_t *host_sp_ptr, uintptr_t entry)
.global lfi_ctx_entry
.global _lfi_ctx_entry
.p2align 4
_lfi_ctx_entry:
lfi_ctx_entry:
    bti jc
    paciasp
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

#ifndef CTXREG
    // set the sandbox thread control block by saving the old value of
    // [tpidr_el0, TLS_SLOT_LFI] into the context and setting it to point to
    // the context itself.
    get_ctx x8
    str x8, [x0, REGS_HOST_TP]
    write_ctx x0, x8
#endif

    // load all registers
    ldp x2, x3,   [x0, REGS_X(2)]
    ldp x4, x5,   [x0, REGS_X(4)]
    ldp x6, x7,   [x0, REGS_X(6)]
    ldp x8, x9,   [x0, REGS_X(8)]
    ldp x10, x11, [x0, REGS_X(10)]
    ldp x12, x13, [x0, REGS_X(12)]
    ldp x14, x15, [x0, REGS_X(14)]
    ldp x16, x17, [x0, REGS_X(16)]
    ldp x18, x19, [x0, REGS_X(18)]
    ldp x20, x21, [x0, REGS_X(20)]
    ldp x22, x23, [x0, REGS_X(22)]
    ldp x24, x25, [x0, REGS_X(24)]
    ldp x26, x27, [x0, REGS_X(26)]
    ldp x28, x29, [x0, REGS_X(28)]
    ldp xzr, x1,  [x0, REGS_X(30)]
    mov sp, x1
    ldp x0, x1, [x0, REGS_X(0)]
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
    // zero integer and fp flags
    ands xzr, xzr, xzr
    msr fpsr, xzr
    // NOTE: x30 must contain a valid sandbox address
    add x30, REG_BASE, w30, uxtw
    ret

// lfi_ctx_end(struct LFIContext *ctx, int val)
.global lfi_ctx_end
.p2align 4
lfi_ctx_end:
    bti jc
    paciasp
    // restore host_sp
    ldr x19, [x0, REGS_HOST_SP]
    mov sp, x19
#ifndef CTXREG
    // restore host_tp
    ldr x19, [x0, REGS_HOST_TP]
    write_ctx x19, x20
#endif
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
    autiasp
    ret

.global lfi_syscall_entry
.p2align 4
lfi_syscall_entry:
    bti jc
    // reset REG_ADDR
    mov REG_ADDR, REG_BASE
    get_ctx REG_BASE
    // save registers
    stp x0, x1,   [REG_BASE, REGS_X(0)]
    stp x2, x3,   [REG_BASE, REGS_X(2)]
    stp x4, x5,   [REG_BASE, REGS_X(4)]
    stp x6, x7,   [REG_BASE, REGS_X(6)]
    stp x8, x9,   [REG_BASE, REGS_X(8)]
    stp x10, x11, [REG_BASE, REGS_X(10)]
    stp x12, x13, [REG_BASE, REGS_X(12)]
    stp x14, x15, [REG_BASE, REGS_X(14)]
    stp x16, x17, [REG_BASE, REGS_X(16)]
    str REG_ADDR, [REG_BASE, REGS_ADDR]
    mov x1, sp
    stp x30, x1,  [REG_BASE, REGS_X(30)]
    stp q0,  q1,  [REG_BASE, REGS_V(0)]
    stp q2,  q3,  [REG_BASE, REGS_V(2)]
    stp q4,  q5,  [REG_BASE, REGS_V(4)]
    stp q6,  q7,  [REG_BASE, REGS_V(6)]
    stp q8,  q9,  [REG_BASE, REGS_V(8)]
    stp q10, q11, [REG_BASE, REGS_V(10)]
    stp q12, q13, [REG_BASE, REGS_V(12)]
    stp q14, q15, [REG_BASE, REGS_V(14)]
    stp q16, q17, [REG_BASE, REGS_V(16)]
    stp q18, q19, [REG_BASE, REGS_V(18)]
    stp q20, q21, [REG_BASE, REGS_V(20)]
    stp q22, q23, [REG_BASE, REGS_V(22)]
    stp q24, q25, [REG_BASE, REGS_V(24)]
    stp q26, q27, [REG_BASE, REGS_V(26)]
    stp q28, q29, [REG_BASE, REGS_V(28)]
    stp q30, q31, [REG_BASE, REGS_V(30)]

#ifndef CTXREG
    // swap the thread control block back to the host one
    ldr x0, [REG_BASE, REGS_HOST_TP]
    write_ctx x0, x1
#endif

    // restore host stack and call handler
    ldr x0, [REG_BASE, REGS_HOST_SP]
    mov sp, x0
    str REG_BASE, [sp, #-16]! // push
    mov x0, REG_BASE
    bl lfi_syscall_handler
    ldr x0, [sp], 16 // pop

#ifndef CTXREG
    // swap thread control block back to context pointer
    write_ctx x0, x1
#endif

    // restore registers
    ldp x2, x3,   [x0, REGS_X(2)]
    ldp x4, x5,   [x0, REGS_X(4)]
    ldp x6, x7,   [x0, REGS_X(6)]
    ldp x8, x9,   [x0, REGS_X(8)]
    ldp x10, x11, [x0, REGS_X(10)]
    ldp x12, x13, [x0, REGS_X(12)]
    ldp x14, x15, [x0, REGS_X(14)]
    ldp x16, x17, [x0, REGS_X(16)]
    ldr REG_ADDR, [x0, REGS_ADDR]
    ldr REG_BASE, [x0, REGS_BASE]
    ldp x30, x1,  [x0, REGS_X(30)]
    ldp q0,  q1,  [x0, REGS_V(0)]
    ldp q2,  q3,  [x0, REGS_V(2)]
    ldp q4,  q5,  [x0, REGS_V(4)]
    ldp q6,  q7,  [x0, REGS_V(6)]
    ldp q8,  q9,  [x0, REGS_V(8)]
    ldp q10, q11, [x0, REGS_V(10)]
    ldp q12, q13, [x0, REGS_V(12)]
    ldp q14, q15, [x0, REGS_V(14)]
    ldp q16, q17, [x0, REGS_V(16)]
    ldp q18, q19, [x0, REGS_V(18)]
    ldp q20, q21, [x0, REGS_V(20)]
    ldp q22, q23, [x0, REGS_V(22)]
    ldp q24, q25, [x0, REGS_V(24)]
    ldp q26, q27, [x0, REGS_V(26)]
    ldp q28, q29, [x0, REGS_V(28)]
    ldp q30, q31, [x0, REGS_V(30)]
    mov sp, x1
    ldp x0, x1,   [x0, REGS_X(0)]
    // zero integer and fp flags
    ands xzr, xzr, xzr
    msr fpsr, xzr
    add x30, REG_BASE, w30, uxtw
    ret

.global lfi_get_tp
.p2align 4
lfi_get_tp:
#ifdef CTXREG
    brk #0
#else
    bti jc
    // reset REG_ADDR
    mov REG_ADDR, REG_BASE
    get_ctx x0
    ldr x0, [x0, REGS_TP]
    add x30, REG_BASE, w30, uxtw
    ret
#endif

.global lfi_set_tp
.p2align 4
lfi_set_tp:
#ifdef CTXREG
    brk #0
#else
    bti jc
    // reset REG_ADDR
    mov REG_ADDR, REG_BASE
    get_ctx REG_BASE
    str x0, [REG_BASE, REGS_TP]
    // reload REG_BASE
    ldr REG_BASE, [REG_BASE, REGS_BASE]
    add x30, REG_BASE, w30, uxtw
    ret
#endif

#ifndef __APPLE__
.section .note.GNU-stack,"",@progbits

// GNU property note for BTI and PAC support
.pushsection .note.gnu.property, "a"
.balign 8
.long 4           // namesz
.long 0x10        // descsz
.long 5           // type: NT_GNU_PROPERTY_TYPE_0
.asciz "GNU"      // name
.long 0xc0000000  // GNU_PROPERTY_AARCH64_FEATURE_1_AND
.long 4           // pr_datasz
.long 3           // GNU_PROPERTY_AARCH64_FEATURE_1_BTI | GNU_PROPERTY_AARCH64_FEATURE_1_PAC
.long 0           // padding
.popsection
#endif
