struct MacroInst {
    int size;
    int ninstr;
};

static bool assert_reg(FdInstr *instr, uint8_t op_index, FdReg reg, uint8_t op_size) {
    return (FD_OP_TYPE(instr, op_index) == FD_OT_REG &&
        FD_OP_REG(instr, op_index) == reg &&
        FD_OP_SIZE(instr, op_index) == op_size);
}

static struct MacroInst macroinst_stos(struct Verifier *v, uint8_t *buf, size_t size) {
    // stos becomes:
    //
    // movl %edi, %edi
    // leaq (%r14, %rdi), %rdi
    // rep stosq
    // movl %edi, %edi

    FdInstr i_mov, i_lea, i_stos, i_mov2;
    size_t offset = 0;

    if (fd_decode(&buf[offset], size - offset, 64, 0, &i_mov) < 0) {
        return (struct MacroInst){-1, 0};
    }
    offset += i_mov.size;
    if (FD_TYPE(&i_mov) != FDI_MOV ||
        !assert_reg(&i_mov, 0, FD_REG_DI, 4) ||
        !assert_reg(&i_mov, 1, FD_REG_DI, 4)) {
        return (struct MacroInst){-1, 0};
    }

    if (fd_decode(&buf[offset], size - offset, 64, 0, &i_lea) < 0) {
        return (struct MacroInst){-1, 0};
    }
    offset += i_lea.size;
    if (FD_TYPE(&i_lea) != FDI_LEA ||
        !assert_reg(&i_lea, 0, FD_REG_DI, 8) ||
        FD_OP_TYPE(&i_lea, 1) != FD_OT_MEM ||
        FD_OP_BASE(&i_lea, 1) != FD_REG_R14 ||
        FD_OP_INDEX(&i_lea, 1) != FD_REG_DI ||
        FD_OP_DISP(&i_lea, 1) != 0 ||
        FD_OP_SCALE(&i_lea, 1) != 0){
        return (struct MacroInst){-1, 0};
    }

    if (fd_decode(&buf[offset], size - offset, 64, 0, &i_stos) < 0) {
        return (struct MacroInst){-1, 0};
    }
    offset += i_stos.size;
    if (FD_TYPE(&i_stos) != FDI_STOS) {
        return (struct MacroInst){-1, 0};
    }

    if (fd_decode(&buf[offset], size - offset, 64, 0, &i_mov2) < 0) {
        return (struct MacroInst){-1, 0};
    }
    offset += i_mov2.size;
    if (FD_TYPE(&i_mov2) != FDI_MOV ||
        !assert_reg(&i_mov2, 0, FD_REG_DI, 4) ||
        !assert_reg(&i_mov2, 1, FD_REG_DI, 4)) {
        return (struct MacroInst){-1, 0};
    }

    return (struct MacroInst){offset, 4};
}

static struct MacroInst macroinst_movs(struct Verifier *v, uint8_t *buf, size_t size) {
    // movs becomes:
    //
    // movl %edi, %edi
    // leaq (%r14, %rdi), %rdi
    // movl %esi, %esi
    // leaq (%r14, %rsi), %rsi
    // movs
    // movl %edi, %edi
    // movl %esi, %esi

    FdInstr i_mov, i_lea, i_mov2, i_lea2, i_movs, i_movpost1, i_movpost2;
    size_t offset = 0;

    if (fd_decode(&buf[offset], size - offset, 64, 0, &i_mov) < 0) {
        return (struct MacroInst){-1, 0};
    }
    offset += i_mov.size;
    if (FD_TYPE(&i_mov) != FDI_MOV ||
        !assert_reg(&i_mov, 0, FD_REG_DI, 4) ||
        !assert_reg(&i_mov, 1, FD_REG_DI, 4)) {
        return (struct MacroInst){-1, 0};
    }

    if (fd_decode(&buf[offset], size - offset, 64, 0, &i_lea) < 0) {
        return (struct MacroInst){-1, 0};
    }
    offset += i_lea.size;
    if (FD_TYPE(&i_lea) != FDI_LEA ||
        !assert_reg(&i_lea, 0, FD_REG_DI, 8) ||
        FD_OP_TYPE(&i_lea, 1) != FD_OT_MEM ||
        FD_OP_BASE(&i_lea, 1) != FD_REG_R14 ||
        FD_OP_INDEX(&i_lea, 1) != FD_REG_DI ||
        FD_OP_DISP(&i_lea, 1) != 0 ||
        FD_OP_SCALE(&i_lea, 1) != 0){
        return (struct MacroInst){-1, 0};
    }

    if (fd_decode(&buf[offset], size - offset, 64, 0, &i_mov2) < 0) {
        return (struct MacroInst){-1, 0};
    }
    offset += i_mov2.size;
    if (FD_TYPE(&i_mov2) != FDI_MOV ||
        !assert_reg(&i_mov2, 0, FD_REG_SI, 4) ||
        !assert_reg(&i_mov2, 1, FD_REG_SI, 4)) {
        return (struct MacroInst){-1, 0};
    }

    if (fd_decode(&buf[offset], size - offset, 64, 0, &i_lea2) < 0) {
        return (struct MacroInst){-1, 0};
    }
    offset += i_lea2.size;
    if (FD_TYPE(&i_lea2) != FDI_LEA ||
        !assert_reg(&i_lea2, 0, FD_REG_SI, 8) ||
        FD_OP_TYPE(&i_lea2, 1) != FD_OT_MEM ||
        FD_OP_BASE(&i_lea2, 1) != FD_REG_R14 ||
        FD_OP_INDEX(&i_lea2, 1) != FD_REG_SI ||
        FD_OP_DISP(&i_lea2, 1) != 0 ||
        FD_OP_SCALE(&i_lea2, 1) != 0){
        return (struct MacroInst){-1, 0};
    }

    if (fd_decode(&buf[offset], size - offset, 64, 0, &i_movs) < 0) {
        return (struct MacroInst){-1, 0};
    }
    offset += i_movs.size;
    if (FD_TYPE(&i_movs) != FDI_MOVS) {
        return (struct MacroInst){-1, 0};
    }

    if (fd_decode(&buf[offset], size - offset, 64, 0, &i_movpost1) < 0) {
        return (struct MacroInst){-1, 0};
    }
    offset += i_movpost1.size;
    if (FD_TYPE(&i_movpost1) != FDI_MOV ||
        !assert_reg(&i_movpost1, 0, FD_REG_DI, 4) ||
        !assert_reg(&i_movpost1, 1, FD_REG_DI, 4)) {
        return (struct MacroInst){-1, 0};
    }

    if (fd_decode(&buf[offset], size - offset, 64, 0, &i_movpost2) < 0) {
        return (struct MacroInst){-1, 0};
    }
    offset += i_movpost2.size;
    if (FD_TYPE(&i_movpost2) != FDI_MOV ||
        !assert_reg(&i_movpost2, 0, FD_REG_SI, 4) ||
        !assert_reg(&i_movpost2, 1, FD_REG_SI, 4)) {
        return (struct MacroInst){-1, 0};
    }

    return (struct MacroInst){offset, 7};
}

static struct MacroInst macroinst_jmp(struct Verifier *v, uint8_t *buf, size_t size) {
    // andl $0xffffffe0, %eX
    // addq %r14, %rX
    // jmpq *%rX

    FdInstr i_and, i_add, i_jmp;
    if (fd_decode(&buf[0], size, 64, 0, &i_and) < 0)
        return (struct MacroInst){-1, 0};
    if (fd_decode(&buf[i_and.size], size - i_and.size, 64, 0, &i_add) < 0)
        return (struct MacroInst){-1, 0};
    if (fd_decode(&buf[i_and.size + i_add.size], size - i_and.size - i_add.size, 64, 0, &i_jmp) < 0)
        return (struct MacroInst){-1, 0};

    if (FD_TYPE(&i_and) != FDI_AND ||
            FD_OP_TYPE(&i_and, 0) != FD_OT_REG ||
            reserved(&i_and, 0) ||
            FD_OP_SIZE(&i_and, 0) != 4 ||
            FD_OP_TYPE(&i_and, 1) != FD_OT_IMM ||
            FD_OP_IMM(&i_and, 1) != 0xffffffffffffffe0)
        return (struct MacroInst){-1, 0};

    if (FD_TYPE(&i_add) != FDI_ADD ||
            FD_OP_TYPE(&i_add, 0) != FD_OT_REG ||
            FD_OP_TYPE(&i_add, 1) != FD_OT_REG ||
            FD_OP_SIZE(&i_add, 0) != 8 ||
            FD_OP_SIZE(&i_add, 1) != 8 ||
            FD_OP_REG(&i_add, 1) != FD_REG_R14 ||
            FD_OP_REG(&i_add, 0) != FD_OP_REG(&i_and, 0))
        return (struct MacroInst){-1, 0};

    if (FD_TYPE(&i_jmp) != FDI_JMP ||
            FD_OP_TYPE(&i_jmp, 0) != FD_OT_REG ||
            FD_OP_REG(&i_jmp, 0) != FD_OP_REG(&i_and, 0))
        return (struct MacroInst){-1, 0};

    return (struct MacroInst){i_and.size + i_add.size + i_jmp.size, 3};
}

static bool okdisp(int64_t disp) {
    return (disp % 8 == 0) && (disp < 256);
}

static struct MacroInst macroinst_rtcall(struct Verifier *v, uint8_t *buf, size_t size) {
    // leaq 1f(%rip), %r11
    // jmpq *N(%r14)
    // 1:
    FdInstr i_lea, i_jmp;
    if (fd_decode(&buf[0], size, 64, 0, &i_lea) < 0)
        return (struct MacroInst){-1, 0};
    if (fd_decode(&buf[i_lea.size], size - i_lea.size, 64, 0, &i_jmp) < 0)
        return (struct MacroInst){-1, 0};

    if (FD_TYPE(&i_lea) != FDI_LEA ||
            FD_OP_TYPE(&i_lea, 0) != FD_OT_REG ||
            FD_OP_REG(&i_lea, 0) != FD_REG_R11 ||
            FD_OP_TYPE(&i_lea, 1) != FD_OT_MEM ||
            FD_OP_BASE(&i_lea, 1) != FD_REG_IP)
        return (struct MacroInst){-1, 0};

    if (FD_TYPE(&i_jmp) != FDI_JMP ||
            FD_OP_TYPE(&i_jmp, 0) != FD_OT_MEM ||
            FD_OP_BASE(&i_jmp, 0) != FD_REG_R14 ||
            FD_OP_INDEX(&i_jmp, 0) != FD_REG_NONE ||
            FD_OP_SCALE(&i_jmp, 0) != 0 ||
            !okdisp(FD_OP_DISP(&i_jmp, 0)))
        return (struct MacroInst){-1, 0};

    // Return target can either be the next instruction or can be some
    // bundle-aligned location.
    uintptr_t ret = v->addr + i_lea.size + FD_OP_DISP(&i_lea, 1);
    bool ok = FD_OP_DISP(&i_lea, 1) == i_jmp.size || ret % v->bundlesize == 0;
    if (!ok)
        return (struct MacroInst){-1, 0};

    return (struct MacroInst){i_lea.size + i_jmp.size, 2};
}

static struct MacroInst macroinst_call(struct Verifier *v, uint8_t *buf, size_t size) {
    size_t bundlesize = 32;

    // andl $0xffffffe0, %eX
    // addq %r14, %rX
    // nop*
    // callq *%rX

    FdInstr i_and, i_add, i_jmp;
    if (fd_decode(&buf[0], size, 64, 0, &i_and) < 0)
        return (struct MacroInst){-1, 0};
    if (FD_TYPE(&i_and) != FDI_AND)
        return (struct MacroInst){-1, 0};

    if (fd_decode(&buf[i_and.size], size - i_and.size, 64, 0, &i_add) < 0)
        return (struct MacroInst){-1, 0};
    size_t count = i_and.size + i_add.size;
    size_t icount = 2;
    while (count < bundlesize) {
        if (fd_decode(&buf[count], size - count, 64, 0, &i_jmp) < 0)
            return (struct MacroInst){-1, 0};
        icount++;
        count += i_jmp.size;
        if (FD_TYPE(&i_jmp) != FDI_NOP)
            break;
    }

    if ((v->addr + count) % bundlesize != 0)
        return (struct MacroInst){-1, 0};

    if (FD_TYPE(&i_and) != FDI_AND ||
            FD_OP_TYPE(&i_and, 0) != FD_OT_REG ||
            reserved(&i_and, 0) ||
            FD_OP_SIZE(&i_and, 0) != 4 ||
            FD_OP_TYPE(&i_and, 1) != FD_OT_IMM ||
            FD_OP_IMM(&i_and, 1) != 0xffffffffffffffe0)
        return (struct MacroInst){-1, 0};

    if (FD_TYPE(&i_add) != FDI_ADD ||
            FD_OP_TYPE(&i_add, 0) != FD_OT_REG ||
            FD_OP_TYPE(&i_add, 1) != FD_OT_REG ||
            FD_OP_SIZE(&i_add, 0) != 8 ||
            FD_OP_SIZE(&i_add, 1) != 8 ||
            FD_OP_REG(&i_add, 1) != FD_REG_R14 ||
            FD_OP_REG(&i_add, 0) != FD_OP_REG(&i_and, 0))
        return (struct MacroInst){-1, 0};

    if (FD_TYPE(&i_jmp) != FDI_CALL ||
            FD_OP_TYPE(&i_jmp, 0) != FD_OT_REG ||
            FD_OP_REG(&i_jmp, 0) != FD_OP_REG(&i_and, 0))
        return (struct MacroInst){-1, 0};

    return (struct MacroInst){count, icount};
}

static struct MacroInst macroinst_load(struct Verifier *v, uint8_t *buf, size_t size) {
    // movl %eX, %eX
    // movq (%r14, %rX), %rX

    FdInstr i_mov, i_load;
    if (fd_decode(&buf[0], size, 64, 0, &i_mov) < 0)
        return (struct MacroInst){-1, 0};
    if (fd_decode(&buf[i_mov.size], size - i_mov.size, 64, 0, &i_load) < 0)
        return (struct MacroInst){-1, 0};

    if (FD_TYPE(&i_mov) != FDI_MOV ||
            FD_OP_TYPE(&i_mov, 0) != FD_OT_REG ||
            FD_OP_TYPE(&i_mov, 1) != FD_OT_REG ||
            FD_OP_SIZE(&i_mov, 0) != 4 ||
            FD_OP_SIZE(&i_mov, 1) != 4 ||
            reserved(&i_mov, 0) ||
            FD_OP_REG(&i_mov, 0) != FD_OP_REG(&i_mov, 1))
        return (struct MacroInst){-1, 0};

    if (FD_TYPE(&i_load) != FDI_MOV ||
            FD_OP_TYPE(&i_load, 0) != FD_OT_REG ||
            reserved(&i_load, 0) ||
            FD_OP_TYPE(&i_load, 1) != FD_OT_MEM ||
            FD_OP_BASE(&i_load, 1) != FD_REG_R14 ||
            FD_OP_INDEX(&i_load, 1) != FD_OP_REG(&i_mov, 0) ||
            FD_OP_SCALE(&i_load, 1) != 0)
        return (struct MacroInst){-1, 0};

    return (struct MacroInst){i_mov.size + i_load.size, 2};
}

static struct MacroInst macroinst_modsp(struct Verifier *v, uint8_t *buf, size_t size) {
    // movl/addl/subl/andl/leal ..., %esp
    // addq %r14, %rsp

    FdInstr i_mov, i_add;
    if (fd_decode(&buf[0], size, 64, 0, &i_mov) < 0)
        return (struct MacroInst){-1, 0};
    if (fd_decode(&buf[i_mov.size], size - i_mov.size, 64, 0, &i_add) < 0)
        return (struct MacroInst){-1, 0};

    // allow addl, subl, lea, add movl
    if (FD_TYPE(&i_mov) != FDI_MOV &&
            FD_TYPE(&i_mov) != FDI_ADD &&
            FD_TYPE(&i_mov) != FDI_AND &&
            FD_TYPE(&i_mov) != FDI_SUB &&
            FD_TYPE(&i_mov) != FDI_LEA)
        return (struct MacroInst){-1, 0};

    if (FD_OP_TYPE(&i_mov, 0) != FD_OT_REG ||
            FD_OP_SIZE(&i_mov, 0) != 4 ||
            FD_OP_REG(&i_mov, 0) != FD_REG_SP)
        return (struct MacroInst){-1, 0};

    // Allow 'addq %r14, %rsp' add 'lea (%rsp, %r14, 1), %rsp'
    bool fail_add = (FD_TYPE(&i_add) != FDI_ADD ||
            FD_OP_TYPE(&i_add, 0) != FD_OT_REG ||
            FD_OP_TYPE(&i_add, 1) != FD_OT_REG ||
            FD_OP_SIZE(&i_add, 0) != 8 ||
            FD_OP_SIZE(&i_add, 1) != 8 ||
            FD_OP_REG(&i_add, 0) != FD_REG_SP ||
            FD_OP_REG(&i_add, 1) != FD_REG_R14);
    bool fail_lea = (FD_TYPE(&i_add) != FDI_LEA ||
            FD_OP_TYPE(&i_add, 1) != FD_OT_MEM ||
            FD_OP_BASE(&i_add, 1) != FD_REG_SP ||
            FD_OP_INDEX(&i_add, 1) != FD_REG_R14 ||
            FD_OP_DISP(&i_add, 1) != 0 ||
            FD_OP_SCALE(&i_add, 1) != 0 ||
            FD_OP_TYPE(&i_add, 0) != FD_OT_REG ||
            FD_OP_SIZE(&i_add, 0) != 8 ||
            FD_OP_REG(&i_add, 0) != FD_REG_SP);
    if (fail_lea && fail_add)
        return (struct MacroInst){-1, 0};

    return (struct MacroInst){i_mov.size + i_add.size, 2};
}


typedef struct MacroInst (*MacroFn)(struct Verifier *, uint8_t*, size_t);

static MacroFn mfns[] = {
    macroinst_jmp,
    macroinst_call,
    macroinst_rtcall,
    macroinst_modsp,
    macroinst_stos,
    macroinst_movs,
    macroinst_load,
};

static struct MacroInst macroinst(struct Verifier *v, uint8_t *buf, size_t size) {
    for (size_t i = 0; i < sizeof(mfns) / sizeof(mfns[0]); i++) {
        struct MacroInst mi = mfns[i](v, buf, size);
        if (mi.size > 0)
            return mi;
    }
    return (struct MacroInst){-1, 0};
}
