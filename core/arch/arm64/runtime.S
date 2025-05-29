.text

.global lfi_syscall_entry
lfi_syscall_entry:
    brk #0

.global lfi_get_tp
lfi_get_tp:
    brk #0

.global lfi_set_tp
lfi_set_tp:
    brk #0

.global lfi_ret
lfi_ret:
    brk #0
