#include "arch_asm.h"

.text

// lfi_ctx_entry(struct LFIContext *ctx, uintptr_t *host_sp_ptr, uintptr_t entry)
.global lfi_ctx_entry
.p2align 4
lfi_ctx_entry:
    // save callee-saved registers to stack
    addi sp, sp, -272

    sd ra, 0(sp)
    sd s0, 8(sp)
    sd s1, 16(sp)
    sd s2, 24(sp)
    sd s3, 32(sp)
    sd s4, 40(sp)
    sd s5, 48(sp)
    sd s6, 56(sp)
    sd s7, 64(sp)
    sd s8, 72(sp)
    sd s9, 80(sp)
    sd s10, 88(sp)
    sd s11, 96(sp)

    fsd fs0, 104(sp)
    fsd fs1, 112(sp)
    fsd fs2, 120(sp)
    fsd fs3, 128(sp)
    fsd fs4, 136(sp)
    fsd fs5, 144(sp)
    fsd fs6, 152(sp)
    fsd fs7, 160(sp)
    fsd fs8, 168(sp)
    fsd fs9, 176(sp)
    fsd fs10, 184(sp)
    fsd fs11, 192(sp)

    // save stack to host_sp_pointer
    sd sp, 0(a1)

    // put entrypoint in ra
    mv ra, a2

    // set the sandbox thread control block
    get_ctx t0
    sd t0, REGS_HOST_TP(a0)
    write_ctx a0

    // skip ra
    ld sp, REGS_SP(a0)
    ld gp, REGS_GP(a0)
    // skip tp
    ld t0, REGS_T0(a0)
    ld t1, REGS_T1(a0)
    ld t2, REGS_T2(a0)
    ld s0, REGS_S0(a0)
    ld s1, REGS_S1(a0)
    // skip a0
    ld a1, REGS_A1(a0)
    ld a2, REGS_A2(a0)
    ld a3, REGS_A3(a0)
    ld a4, REGS_A4(a0)
    ld a5, REGS_A5(a0)
    ld a6, REGS_A6(a0)
    ld a7, REGS_A7(a0)
    ld s2, REGS_S2(a0)
    ld s3, REGS_S3(a0)
    ld s4, REGS_S4(a0)
    ld s5, REGS_S5(a0)
    ld s6, REGS_S6(a0)
    ld s7, REGS_S7(a0)
    ld s8, REGS_S8(a0)
    ld s9, REGS_S9(a0)
    ld s10, REGS_S10(a0)
    ld s11, REGS_S11(a0)
    ld t3, REGS_T3(a0)
    ld t4, REGS_T4(a0)
    ld t5, REGS_T5(a0)
    ld t6, REGS_T6(a0)
    ld a0, REGS_A0(a0)

    fcvt.d.w f0, zero
    fcvt.d.w f1, zero
    fcvt.d.w f2, zero
    fcvt.d.w f3, zero
    fcvt.d.w f4, zero
    fcvt.d.w f5, zero
    fcvt.d.w f6, zero
    fcvt.d.w f7, zero
    fcvt.d.w f8, zero
    fcvt.d.w f9, zero
    fcvt.d.w f10, zero
    fcvt.d.w f11, zero
    fcvt.d.w f12, zero
    fcvt.d.w f13, zero
    fcvt.d.w f14, zero
    fcvt.d.w f15, zero
    fcvt.d.w f16, zero
    fcvt.d.w f17, zero
    fcvt.d.w f18, zero
    fcvt.d.w f19, zero
    fcvt.d.w f20, zero
    fcvt.d.w f21, zero
    fcvt.d.w f22, zero
    fcvt.d.w f23, zero
    fcvt.d.w f24, zero
    fcvt.d.w f25, zero
    fcvt.d.w f26, zero
    fcvt.d.w f27, zero
    fcvt.d.w f28, zero
    fcvt.d.w f29, zero
    fcvt.d.w f30, zero
    fcvt.d.w f31, zero

    add.uw ra, ra, REG_BASE
    ret

// lfi_ctx_end(struct LFIContext *ctx, int val)
.global lfi_ctx_end
.p2align 4
lfi_ctx_end:
    // restore host sp and tp
    ld sp, REGS_HOST_SP(a0)
    ld t0, REGS_HOST_TP(a0)
    write_ctx t0

    // put val in return register
    mv a0, a1

    // restore callee-saved registers
    ld ra, 0(sp)
    ld s0, 8(sp)
    ld s1, 16(sp)
    ld s2, 24(sp)
    ld s3, 32(sp)
    ld s4, 40(sp)
    ld s5, 48(sp)
    ld s6, 56(sp)
    ld s7, 64(sp)
    ld s8, 72(sp)
    ld s9, 80(sp)
    ld s10, 88(sp)
    ld s11, 96(sp)

    fld fs0, 104(sp)
    fld fs1, 112(sp)
    fld fs2, 120(sp)
    fld fs3, 128(sp)
    fld fs4, 136(sp)
    fld fs5, 144(sp)
    fld fs6, 152(sp)
    fld fs7, 160(sp)
    fld fs8, 168(sp)
    fld fs9, 176(sp)
    fld fs10, 184(sp)
    fld fs11, 192(sp)

    addi sp, sp, 272

    ret

.global lfi_syscall_entry
.p2align 4
lfi_syscall_entry:
    get_ctx REG_BASE

    // save registers
    sd ra, REGS_RA(REG_BASE)
    sd sp, REGS_SP(REG_BASE)
    sd gp, REGS_GP(REG_BASE)
    
    sd t0, REGS_T0(REG_BASE)
    sd t1, REGS_T1(REG_BASE)
    sd t2, REGS_T2(REG_BASE)
    sd s0, REGS_S0(REG_BASE)
    sd s1, REGS_S1(REG_BASE)
    sd a0, REGS_A0(REG_BASE)
    sd a1, REGS_A1(REG_BASE)
    sd a2, REGS_A2(REG_BASE)
    sd a3, REGS_A3(REG_BASE)
    sd a4, REGS_A4(REG_BASE)
    sd a5, REGS_A5(REG_BASE)
    sd a6, REGS_A6(REG_BASE)
    sd a7, REGS_A7(REG_BASE)
    sd s2, REGS_S2(REG_BASE)
    sd s3, REGS_S3(REG_BASE)
    sd s4, REGS_S4(REG_BASE)
    sd s5, REGS_S5(REG_BASE)
    sd s6, REGS_S6(REG_BASE)
    sd s7, REGS_S7(REG_BASE)
    sd s8, REGS_S8(REG_BASE)
    sd s9, REGS_S9(REG_BASE)
    sd s10, REGS_S10(REG_BASE)
    sd t3, REGS_T3(REG_BASE)
    sd t4, REGS_T4(REG_BASE)
    sd t5, REGS_T5(REG_BASE)
    sd t6, REGS_T6(REG_BASE)

    fsd f0, REGS_F+8*0(REG_BASE)
    fsd f1, REGS_F+8*1(REG_BASE)
    fsd f2, REGS_F+8*2(REG_BASE)
    fsd f3, REGS_F+8*3(REG_BASE)
    fsd f4, REGS_F+8*4(REG_BASE)
    fsd f5, REGS_F+8*5(REG_BASE)
    fsd f6, REGS_F+8*6(REG_BASE)
    fsd f7, REGS_F+8*7(REG_BASE)
    fsd f8, REGS_F+8*8(REG_BASE)
    fsd f9, REGS_F+8*9(REG_BASE)
    fsd f10, REGS_F+8*10(REG_BASE)
    fsd f11, REGS_F+8*11(REG_BASE)
    fsd f12, REGS_F+8*12(REG_BASE)
    fsd f13, REGS_F+8*13(REG_BASE)
    fsd f14, REGS_F+8*14(REG_BASE)
    fsd f15, REGS_F+8*15(REG_BASE)
    fsd f16, REGS_F+8*16(REG_BASE)
    fsd f17, REGS_F+8*17(REG_BASE)
    fsd f18, REGS_F+8*18(REG_BASE)
    fsd f19, REGS_F+8*19(REG_BASE)
    fsd f20, REGS_F+8*20(REG_BASE)
    fsd f21, REGS_F+8*21(REG_BASE)
    fsd f22, REGS_F+8*22(REG_BASE)
    fsd f23, REGS_F+8*23(REG_BASE)
    fsd f24, REGS_F+8*24(REG_BASE)
    fsd f25, REGS_F+8*25(REG_BASE)
    fsd f26, REGS_F+8*26(REG_BASE)
    fsd f27, REGS_F+8*27(REG_BASE)
    fsd f28, REGS_F+8*28(REG_BASE)
    fsd f29, REGS_F+8*29(REG_BASE)
    fsd f30, REGS_F+8*30(REG_BASE)
    fsd f31, REGS_F+8*31(REG_BASE)

    ld t0, REGS_HOST_TP(REG_BASE)
    write_ctx t0

    ld sp, REGS_HOST_SP(REG_BASE)

    addi sp, sp, -8
    sd REG_BASE, 0(sp)
    mv a0, REG_BASE
    call lfi_syscall_handler
    ld a0, 0(sp)
    addi sp, sp, 8

    write_ctx a0

    ld ra, REGS_RA(a0)
    ld sp, REGS_SP(a0)
    ld gp, REGS_GP(a0)
    // skip tp
    ld t0, REGS_T0(a0)
    ld t1, REGS_T1(a0)
    ld t2, REGS_T2(a0)
    ld s0, REGS_S0(a0)
    ld s1, REGS_S1(a0)
    // skip a0
    ld a1, REGS_A1(a0)
    ld a2, REGS_A2(a0)
    ld a3, REGS_A3(a0)
    ld a4, REGS_A4(a0)
    ld a5, REGS_A5(a0)
    ld a6, REGS_A6(a0)
    ld a7, REGS_A7(a0)
    ld s2, REGS_S2(a0)
    ld s3, REGS_S3(a0)
    ld s4, REGS_S4(a0)
    ld s5, REGS_S5(a0)
    ld s6, REGS_S6(a0)
    ld s7, REGS_S7(a0)
    ld s8, REGS_S8(a0)
    ld s9, REGS_S9(a0)
    ld s10, REGS_S10(a0)
    ld s11, REGS_S11(a0)
    ld t3, REGS_T3(a0)
    ld t4, REGS_T4(a0)
    ld t5, REGS_T5(a0)
    ld t6, REGS_T6(a0)

    fld f0, REGS_F+8*0(a0)
    fld f1, REGS_F+8*1(a0)
    fld f2, REGS_F+8*2(a0)
    fld f3, REGS_F+8*3(a0)
    fld f4, REGS_F+8*4(a0)
    fld f5, REGS_F+8*5(a0)
    fld f6, REGS_F+8*6(a0)
    fld f7, REGS_F+8*7(a0)
    fld f8, REGS_F+8*8(a0)
    fld f9, REGS_F+8*9(a0)
    fld f10, REGS_F+8*10(a0)
    fld f11, REGS_F+8*11(a0)
    fld f12, REGS_F+8*12(a0)
    fld f13, REGS_F+8*13(a0)
    fld f14, REGS_F+8*14(a0)
    fld f15, REGS_F+8*15(a0)
    fld f16, REGS_F+8*16(a0)
    fld f17, REGS_F+8*17(a0)
    fld f18, REGS_F+8*18(a0)
    fld f19, REGS_F+8*19(a0)
    fld f20, REGS_F+8*20(a0)
    fld f21, REGS_F+8*21(a0)
    fld f22, REGS_F+8*22(a0)
    fld f23, REGS_F+8*23(a0)
    fld f24, REGS_F+8*24(a0)
    fld f25, REGS_F+8*25(a0)
    fld f26, REGS_F+8*26(a0)
    fld f27, REGS_F+8*27(a0)
    fld f28, REGS_F+8*28(a0)
    fld f29, REGS_F+8*29(a0)
    fld f30, REGS_F+8*30(a0)
    fld f31, REGS_F+8*31(a0)
    ld a0, REGS_A0(a0)

    add.uw ra, ra, REG_BASE
    ret

.global lfi_get_tp
.p2align 4
lfi_get_tp:
    get_ctx t0
    ld a0, REGS_TP(t0)
    add.uw ra, ra, REG_BASE
    ret

.global lfi_set_tp
.p2align 4
lfi_set_tp:
    get_ctx REG_BASE
    sd a0, REGS_TP(REG_BASE)
    ld REG_BASE, REGS_BASE(REG_BASE)
    add.uw ra, ra, REG_BASE
    ret

#ifndef __APPLE__
.section .note.GNU-stack,"",@progbits
#endif
