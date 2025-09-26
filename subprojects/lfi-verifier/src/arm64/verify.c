#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <stdlib.h>

#include "lfiv.h"
#include "disarm64.h"

struct Verifier {
    bool failed;
    bool abort;
    uintptr_t addr;
    struct LFIVOptions *opts;
};

enum {
    ERRMAX = 128, // maximum size for error
};

static void verrmin(struct Verifier *v, const char* fmt, ...) {
    v->failed = true;

    if (!v->opts->err)
        return;

    va_list ap;

    char errbuf[ERRMAX];

    va_start(ap, fmt);
    vsnprintf(errbuf, ERRMAX, fmt, ap);
    va_end(ap);

    v->opts->err(errbuf, strlen(errbuf));
}

static void verr(struct Verifier *v, struct Da64Inst *inst, const char* msg) {
    char fmtbuf[128];
    da64_format(inst, fmtbuf);
    verrmin(v, "%lx: %s: %s", v->addr, fmtbuf, msg);
}

enum {
    INSN_SIZE = 4,
};

#define INSN_NOP       0xd503201f
#define INSN_AUTIA1716 0xd503219f
#define INSN_AUTIB1716 0xd50321df
#define INSN_AUTIASP   0xd50323bf
#define INSN_PACM      0xd50324ff
#define INSN_PACIASP   0xd503233f
#define INSN_BTIJC     0xd50324df
#define INSN_BTIJ      0xd503249f
#define INSN_BTIC      0xd503245f
#define INSN_BTI       0xd503241f

enum {
    REG_ADDR    = 28,
    REG_BASE    = 27,
    REG_RET     = 30,
};

enum {
    SYS_tpidr_el0        = 0xde82,
    SYS_fpsr             = 0xda21,
    SYS_fpcr             = 0xda20,
    SYS_id_aa64pfr0_el1  = 0xc020,
    SYS_id_aa64pfr1_el1  = 0xc021,
    SYS_id_aa64zfr0_el1  = 0xc024, // requires SVE
    SYS_id_aa64isar0_el1 = 0xc030,
    SYS_id_aa64isar1_el1 = 0xc031,
};

static bool cfreg(struct Verifier *v, uint8_t reg) {
    return reg == REG_ADDR || reg == REG_RET;
}

static bool rtsysreg(struct Verifier *v, uint8_t reg) {
    return reg == REG_BASE;
}

static bool basereg(uint8_t reg) {
    return reg == REG_BASE;
}

static bool retreg(uint8_t reg) {
    return reg == REG_RET;
}

static bool fixedreg(struct Verifier *v, uint8_t reg) {
    if (reg == REG_BASE)
        return true;
    return false;
}

static bool ldstreg(struct Verifier *v, uint8_t reg, bool sp) {
    if (sp && reg == 31)
        return true;
    if (reg == REG_ADDR)
        return true;
    return false;
}

static bool addrreg(struct Verifier *v, uint8_t reg, bool sp) {
    if (sp && reg == 31)
        return true;
    if (cfreg(v, reg))
        return true;
    if (reg == REG_ADDR || reg == REG_RET)
        return true;
    return false;
}

static bool sysreg(uint16_t sysreg) {
    return sysreg == SYS_fpsr ||
        sysreg == SYS_fpcr ||
        sysreg == SYS_id_aa64pfr0_el1 ||
        sysreg == SYS_id_aa64pfr1_el1 ||
        sysreg == SYS_id_aa64zfr0_el1 ||
        sysreg == SYS_id_aa64isar0_el1 ||
        sysreg == SYS_id_aa64isar1_el1;
}

// returns a bitmask of modified operands
static uint8_t nmod(struct Da64Inst *dinst) {
    switch (DA64_GROUP(dinst->mnem)) {
    case DA64G_BRANCH:
    case DA64G_BCOND:
    case DA64G_BRANCHREG:
        return 0b0;
    case DA64G_CASP:
    case DA64G_CAS:
        return 0b11;
    case DA64G_LDATOMIC:
    case DA64G_SWP:
        return 0b10;
    }

    switch (dinst->mnem) {
    // load pair instructions
    case DA64I_LDPW_POST:
    case DA64I_LDPW:
    case DA64I_LDPW_PRE:
    case DA64I_LDPSW_POST:
    case DA64I_LDPSW:
    case DA64I_LDPSW_PRE:
    case DA64I_LDPX_POST:
    case DA64I_LDPX:
    case DA64I_LDPX_PRE:
    case DA64I_LDNPW:
    case DA64I_LDNPX:
    case DA64I_LDXPW:
    case DA64I_LDXPX:
    case DA64I_LDAXPW:
    case DA64I_LDAXPX:
    case DA64I_LDP_FP_POST:
    case DA64I_LDP_FP:
    case DA64I_LDP_FP_PRE:
        return 0b11;
    // stores
    case DA64I_STR_IMM:
    case DA64I_STR_REG:
    case DA64I_STRX_PRE:
    case DA64I_STRX_POST:
    case DA64I_STRB_IMM:
    case DA64I_STRB_REG:
    case DA64I_STRB_PRE:
    case DA64I_STRB_POST:
    case DA64I_STRH_IMM:
    case DA64I_STRH_REG:
    case DA64I_STRH_PRE:
    case DA64I_STRH_POST:
    case DA64I_STRW_IMM:
    case DA64I_STRW_REG:
    case DA64I_STRW_PRE:
    case DA64I_STRW_POST:
    case DA64I_STPX_POST:
    case DA64I_STPX:
    case DA64I_STPX_PRE:
        return 0b0;
    }

    return 0b1;
}

static void chkbranch(struct Verifier *v, struct Da64Inst *dinst) {
    switch (dinst->mnem) {
    case DA64I_BLR:
        if (rtsysreg(v, dinst->ops[0].reg))
            return;
        // fallthrough
    case DA64I_BR:
    case DA64I_RET:
        assert(dinst->ops[0].type == DA_OP_REGGP);
        if (!cfreg(v, dinst->ops[0].reg)) {
            verr(v, dinst, "indirect branch using illegal register");
        }
        break;
    case DA64I_RETAA:
    case DA64I_RETAB:
        break;
    default:
        assert(DA64_GROUP(dinst->mnem) != DA64G_BRANCHREG);
        break;
    }
}

static void chksys(struct Verifier *v, struct Da64Inst *dinst) {
    switch (dinst->mnem) {
    case DA64I_MSR:
        assert(dinst->ops[0].type == DA_OP_SYSREG);
        if (!sysreg(dinst->ops[0].sysreg))
            verr(v, dinst, "write to illegal sysreg");
        break;
    case DA64I_MRS:
        assert(dinst->ops[1].type == DA_OP_SYSREG);
        if (!sysreg(dinst->ops[1].sysreg))
            verr(v, dinst, "read from illegal sysreg");
        break;
    }
}

static bool isload(struct Verifier *v, struct Da64Inst *dinst) {
    switch (dinst->mnem) {
#include "loads.instrs"
    }
    return false;
}

static bool okmnem(struct Verifier *v, struct Da64Inst *dinst) {
    switch (dinst->mnem) {
#include "base.instrs"
    }

    return false;
}

static bool okrtcallimm(int16_t simm16) {
    return (simm16 % 8 == 0) && simm16 < 256;
}

static bool okmemop(struct Verifier *v, struct Da64Op *op, bool load) {
    bool storesonly = v->opts->box == LFI_BOX_STORES;
    switch (op->type) {
    case DA_OP_MEMUOFF:
    case DA_OP_MEMSOFF:
        if (load && storesonly)
            return true;
        // runtime call
        if (rtsysreg(v, op->reg) && okrtcallimm(op->simm16))
            return true;
        return ldstreg(v, op->reg, true);
    case DA_OP_MEMSOFFPRE:
    case DA_OP_MEMSOFFPOST:
        if (load && storesonly)
            return !fixedreg(v, op->reg);
        return ldstreg(v, op->reg, true);
    case DA_OP_MEMREG:
        if (load && storesonly)
            return true;
        return basereg(op->reg) && op->memreg.ext == DA_EXT_UXTW && op->memreg.sc == 0;
    case DA_OP_MEMREGPOST:
        if (load && storesonly)
            return !fixedreg(v, op->reg);
        return false;
    case DA_OP_MEMINC:
        if (load && storesonly)
            return !fixedreg(v, op->reg);
        return false;
    default:
        return true;
    }
}

static void chkmemops(struct Verifier *v, struct Da64Inst *dinst) {
    bool load = isload(v, dinst);
    for (size_t i = 0; i < sizeof(dinst->ops) / sizeof(struct Da64Op); i++) {
        if (!okmemop(v, &dinst->ops[i], load))
            verr(v, dinst, "illegal memory operand");
    }
}

static bool okmod(struct Verifier *v, struct Da64Inst *dinst, struct Da64Op *op) {
    if (op->type != DA_OP_REGGP &&
        op->type != DA_OP_REGGPINC &&
        op->type != DA_OP_REGGPEXT &&
        op->type != DA_OP_REGSP)
        return true;

    if (fixedreg(v, op->reg))
        return false;
    if (!addrreg(v, op->reg, op->type == DA_OP_REGSP))
        return true;

    if (dinst->mnem == DA64I_ADD_EXT) {
        // 'add addrreg, base, lo, uxtw' is allowed.
        if (addrreg(v, dinst->ops[0].reg, true) && dinst->ops[0].reggp.sf == 1 && basereg(dinst->ops[1].reg) &&
                dinst->ops[2].reggpext.ext == DA_EXT_UXTW &&
                dinst->ops[2].reggpext.sf == 0 && dinst->ops[2].reggpext.shift == 0)
            return true;
    }

    if (retreg(op->reg) && dinst->mnem == DA64I_LDR_IMM) {
        // 'ldr x30, [rtsysreg, #n]' is allowed.
        if (dinst->ops[1].type == DA_OP_MEMUOFF &&
                rtsysreg(v, dinst->ops[1].reg))
            return true;
    }

    return false;
}

static void chkwriteback(struct Verifier *v, struct Da64Inst *dinst) {
    uint8_t memreg;
    bool prepost = false;
    for (size_t i = 0; i < sizeof(dinst->ops) / sizeof(struct Da64Op); i++) {
        struct Da64Op *op = &dinst->ops[i];
        switch (op->type) {
        case DA_OP_MEMSOFFPRE:
        case DA_OP_MEMSOFFPOST:
        case DA_OP_MEMREGPOST:
            memreg = op->reg;
            prepost = true;
            break;
        }
    }
    if (!prepost)
        return;
    for (size_t i = 0; i < sizeof(dinst->ops) / sizeof(struct Da64Op); i++) {
        struct Da64Op *op = &dinst->ops[i];
        if (op->type == DA_OP_REGGP && op->reg == memreg && op->reg != 31) {
            verr(v, dinst, "unpredictable writeback to register");
            break;
        }
    }
}

static void vchk(struct Verifier *v, uint32_t insn) {
    struct Da64Inst dinst;
    da64_decode(insn, &dinst);

    if (dinst.mnem == DA64I_UNKNOWN) {
        verrmin(v, "%lx: unknown instruction: %x", v->addr, insn);
        return;
    }

    switch (insn) {
    case INSN_NOP:
    case INSN_AUTIA1716:
    case INSN_AUTIB1716:
    case INSN_AUTIASP:
    case INSN_PACM:
    case INSN_PACIASP:
    case INSN_BTIJC:
    case INSN_BTIJ:
    case INSN_BTIC:
    case INSN_BTI:
        return;
    }

    if (!okmnem(v, &dinst)) {
        verr(v, &dinst, "illegal instruction");
        return;
    }

    chkbranch(v, &dinst);
    chksys(v, &dinst);
    chkmemops(v, &dinst);

    uint8_t mask = nmod(&dinst);
    for (int i = 0; i < sizeof(dinst.ops) / sizeof(struct Da64Op); i++) {
        if (((mask >> i) & 1) == 1)
            if (!okmod(v, &dinst, &dinst.ops[i]))
                verr(v, &dinst, "illegal modification of reserved register");
    }

    assert(mask <= 0b11);
    if (mask == 0b11 && dinst.ops[0].reg == dinst.ops[1].reg)
        verr(v, &dinst, "simultaneous modification of the same register is unpredictable");

    chkwriteback(v, &dinst);
}

bool lfiv_verify_arm64(char *code, size_t size, uintptr_t addr, struct LFIVOptions *opts) {
    if (size % INSN_SIZE != 0)
        return false;

    uint32_t* insns = (uint32_t*) code;

    struct Verifier v = {
        .addr = addr,
        .opts = opts,
    };

    for (size_t i = 0; i < size / INSN_SIZE; i++) {
        vchk(&v, insns[i]);
        v.addr += INSN_SIZE;
        // Exit early if there is no error reporter.
        if (v.failed && v.opts->err == NULL)
            return false;
    }

    return !v.failed;
}
