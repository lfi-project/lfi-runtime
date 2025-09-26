ldr x0, [x0]
---
br x0
---
mov sp, x0
---
mov x27, x0
---
mov x28, x0
---
mov x30, x0
---
ret x0
---
str x0, [x1]
---
svc #0
---
svc #12
---
.long 0b01001010000000001000000000000000 // undefined eor encoding
---
ldr x30, [sp]
---
add x30, x27, x30
---
ldr x0, [x27], #16
---
// ldr x0, [x0], #16
.long 0xf8410400
---
wfi
---
ldr x0, #0
---
dc cvau, x0
---
adr x28, foo
foo:
---
msr	s3_5_c12_c9_4, x28
---
mrs	x28, tpidr_el0
---
.long 0x09fa09f2
---
ld4 { v29.2s, v30.2s, v31.2s, v0.2s }, [x0]
---
.long 0xc4ff8005
---
swplb w26, w27, [x28]
---
swpal x20, x27, [x28]
---
ldadd x20, x27, [x28]
---
caspl x26, x27, x0, x1, [x27]
---
add x27, sp, #0xc20
---
ldr x0, [x27, 16]!
---
mrs x0, tpidr_el0
---
msr tpidr_el0, x0
---
ldr x30, [x27, 4]
---
ldr x30, [x27, 264]
---
// ldp x0, x0, [x28]
.long 0xa9400240
---
ldp x0, x28, [x28]
---
add w28, w27, w7, uxtw #0
---
// ldp w11, w28, [x28], #-0x28
.long 0x28fd738b
---
// stp w11, w28, [x28], #-0x28
.long 0x28bb738b
---
.inst   0x48207e43
---
// flags: --sandbox=stores
ldr x0, [x27], #16
---
// flags: --sandbox=stores
str x0, [x1]
