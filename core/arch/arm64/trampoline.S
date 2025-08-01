#include "arch_asm.h"

.macro zero_regs
    mov x9, #0
    // skip x10 (target)
    mov x11, #0
    mov x12, #0
    mov x13, #0
    mov x14, #0
    mov x15, #0
    mov x16, #0
    mov x17, #0
    // skip x18 (platform-reserved)
    mov x19, #0
    mov x20, #0
    mov x21, #0
    mov x22, #0
    mov x23, #0
    mov x24, #0
    mov x25, #0
    mov x26, #0
    // skip x27 (base)
    // skip x28 (addr)
    mov x29, #0
    // skip x30 (ret)
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
.endm

// Returns a pointer to lfi_invoke_info in reg.
.macro get_invoke_info reg
#if defined(__linux__)
# if defined(ENABLE_TLS_EXEC)
    adrp x11, :gottprel:lfi_invoke_info
    ldr x11, [x11, :gottprel_lo12:lfi_invoke_info]
    mrs x12, tpidr_el0
    add \reg, x12, x11
# else
    mov x11, x0
    mov x12, x30

    adrp x0, :tlsdesc:lfi_invoke_info
    ldr \reg, [x0, :tlsdesc_lo12:lfi_invoke_info]
    add x0, x0, :tlsdesc_lo12:lfi_invoke_info
    .tlsdesccall lfi_invoke_info
    blr \reg
    mrs \reg, tpidr_el0
    add \reg, \reg, x0

    mov x30, x12
    mov x0, x11
# endif
#elif defined(__APPLE__)
    mov x11, x0
    mov x12, x30
Lloh0:
    adrp x0, lfi_invoke_info@TLVPPAGE
Lloh1:
    ldr x0, [x0, lfi_invoke_info@TLVPPAGEOFF]
    ldr \reg, [x0]
    blr \reg
    mov \reg, x0
    mov x30, x12
    mov x0, x11
.loh AdrpLdr Lloh0, Lloh1
#else
#error "unsupported OS"
#endif
.endm

.macro lazy_ctx_clone
    // Load the context pointer from the LFIContext **.
    ldr x13, [x20]
    // If NULL, initialize the context pointer.
    cbnz x13, 1f
    bl .trampoline_clone
    ldr x13, [x20]
1:
.endm

.global lfi_trampoline_struct
.p2align 4
lfi_trampoline_struct:
    bti jc
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

    get_invoke_info x19
    ldr x20, [x19, #INVOKE_CTX]
    ldr x21, [x19, #INVOKE_BOX]
    ldr x22, [x19, #INVOKE_TARGETFN]

    lazy_ctx_clone

    ldr x0, [x13, #REGS_X(0)]
    ldr x1, [x13, #REGS_X(1)]
    ldr x2, [x13, #REGS_X(2)]
    ldr x3, [x13, #REGS_X(3)]
    ldr x4, [x13, #REGS_X(4)]
    ldr x5, [x13, #REGS_X(5)]
    ldr x6, [x13, #REGS_X(6)]
    ldr x7, [x13, #REGS_X(7)]
    ldr x8, [x13, #REGS_X(8)]
    ldr d0, [x13, #REGS_V(0)]
    ldr d1, [x13, #REGS_V(1)]
    ldr d2, [x13, #REGS_V(2)]
    ldr d3, [x13, #REGS_V(3)]
    ldr d4, [x13, #REGS_V(4)]
    ldr d5, [x13, #REGS_V(5)]
    ldr d6, [x13, #REGS_V(6)]
    ldr d7, [x13, #REGS_V(7)]

    b .lfi_trampoline_common

.global lfi_trampoline
.p2align 4
lfi_trampoline:
    bti jc
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

    get_invoke_info x19
    ldr x20, [x19, #INVOKE_CTX]
    ldr x21, [x19, #INVOKE_BOX]
    ldr x22, [x19, #INVOKE_TARGETFN]

    lazy_ctx_clone

    b .lfi_trampoline_common

// Alternate trampoline with stack-based calling convention:
// sp+24: size_t n_stack (unused)
// sp+16: lfiptr target
// sp+8: struct LFIBox *box
// sp: struct LFIContext **ctxp
.global lfi_trampoline_direct
.p2align 4
lfi_trampoline_direct:
    ldp x10, x13, [sp], #16
    ldp x11, x12, [sp], #16

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

    mov x22, x10
    mov x20, x11
    mov x21, x12

    lazy_ctx_clone

.lfi_trampoline_common:
    ldr x9, [x13, #REGS_RETADDR]
    // load target into x10
    mov x10, x22

    // set the sandbox thread control block by saving the old value of
    // [tpidr_el0, TLS_SLOT_LFI] into the context and setting it to point to
    // the context itself.
    get_ctx x12
    str x12, [x13, REGS_HOST_TP]
    write_ctx x13, x12

    // save user sp to stack
    ldr x12, [x13, REGS_SP]
    stp x12, xzr, [sp, #-16]!

    // save host stack
    mov x11, sp
    str x11, [x13, REGS_HOST_SP]

    // set guest stack
    mov sp, x12
    // set return address to lfi_retfn
    mov x30, x9
    // set base pointer
    ldr REG_BASE, [x13, REGS_BASE]
    // set address pointer to a valid value
    mov REG_ADDR, REG_BASE

#ifndef STORES_ONLY
    // Zero non-argument registers for full LFI.
    // TODO: calling convention or LFI_INVOKE should enforce that all unused
    // argument registers are zeroed.
    zero_regs
#endif

    // enter sandbox
    add x10, REG_BASE, w10, uxtw
    br x10
    // the previous call should return through lfi_ret
    udf #0

.global lfi_ret_end
.p2align 4
lfi_ret_end:
    mov REG_BASE, x0
    b .lfi_ret_internal

.global lfi_ret
.p2align 4
lfi_ret:
    get_ctx REG_BASE
    // restore host thread control block
    ldr x19, [REG_BASE, REGS_HOST_TP]
    write_ctx x19, x20
.lfi_ret_internal:
    // restore host stack
    ldr x19, [REG_BASE, REGS_HOST_SP]
    mov sp, x19
    // save return registers
    str x0, [REG_BASE, REGS_X(0)]
    str x1, [REG_BASE, REGS_X(1)]
    str x8, [REG_BASE, REGS_X(8)]
    str d0, [REG_BASE, REGS_V(0)]
    str d1, [REG_BASE, REGS_V(1)]
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

// Slowpath taken when the sandbox context needs to be initialized.
.p2align 4
.trampoline_clone:
    stp x29, x30, [sp, #-16]!
    stp x0, x1, [sp, #-16]!
    stp x2, x3, [sp, #-16]!
    stp x4, x5, [sp, #-16]!
    stp x6, x7, [sp, #-16]!
    stp q0, q1, [sp, #-32]!
    stp q2, q3, [sp, #-32]!
    stp q4, q5, [sp, #-32]!
    stp q6, q7, [sp, #-32]!
    // load LFIBox argument
    mov x0, x21
    mov x1, x20
    bl lfi_clone
    ldp q6, q7, [sp], 32
    ldp q4, q5, [sp], 32
    ldp q2, q3, [sp], 32
    ldp q0, q1, [sp], 32
    ldp x6, x7, [sp], 16
    ldp x4, x5, [sp], 16
    ldp x2, x3, [sp], 16
    ldp x0, x1, [sp], 16
    ldp x29, x30, [sp], 16
    ret

#ifndef __APPLE__
.section .note.GNU-stack,"",@progbits
#endif
