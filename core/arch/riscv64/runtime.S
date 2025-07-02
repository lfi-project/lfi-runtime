#include "arch_asm.h"

.text

// lfi_ctx_entry(struct LFIContext *ctx, uintptr_t *host_sp_ptr, uintptr_t entry)
.global lfi_ctx_entry
.p2align 4

lfi_ctx_entry:
    #TODO: Implement RISC-V context entry
    ebreak

// lfi_ctx_end(struct LFIContext *ctx, int val)
.global lfi_ctx_end
.p2align 4
lfi_ctx_end:
    #TODO: Implement RISC-V context end
    ebreak

.global lfi_syscall_entry
.p2align 4
lfi_syscall_entry:
    #TODO: Implement RISC-V syscall entry
    ebreak

.global lfi_get_tp
.p2align 4
lfi_get_tp:
    #TODO: Implement RISC-V get thread pointer
    ebreak

.global lfi_set_tp
.p2align 4
lfi_set_tp:
    #TODO: Implement RISC-V set thread pointer
    ebreak

#ifndef __APPLE__
.section .note.GNU-stack,"",@progbits
#endif
