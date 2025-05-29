.text

.global lfi_syscall_entry
lfi_syscall_entry:
    int3

.global lfi_get_tp
lfi_get_tp:
    int3

.global lfi_set_tp
lfi_set_tp:
    int3

.global lfi_ret
lfi_ret:
    int3
