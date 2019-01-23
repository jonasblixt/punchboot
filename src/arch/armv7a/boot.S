.section .text

.globl arch_jump_linux_dt
arch_jump_linux_dt:

    mov r4, r0 /* Jump address */
    mov r5, r1 /* Device tree address */

    ldr r0, =0
    ldr r1, =0xFFFFFFFF
    mov r2, r5
    ldr r3, =0

    mov pc, r4
    b .


.globl arch_jump_atf
arch_jump_atf:

    mov r4, r0 /* Jump address */
    mov r5, r1 /* BL31 image parameters struct */

    mov r0, r1
    ldr r1, =0
    ldr r2, =0
    ldr r3, =0

    mov pc, r4
    b .

