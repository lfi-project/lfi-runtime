#ifdef __APPLE__
.const
#else
.section .rodata
#endif

.align 4
.global lfi_bdd_data
lfi_bdd_data:
    .incbin "src/x64/x86-all.bdd.bin"
.global lfi_bdd_data_end
lfi_bdd_data_end:

#ifndef __APPLE__
.section .note.GNU-stack,"",@progbits
#endif
