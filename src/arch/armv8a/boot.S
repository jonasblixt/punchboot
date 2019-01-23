.section .text

.global arch_jump_linux_dt

arch_jump_linux_dt:
    b .

.global arch_jump_atf
arch_jump_atf:

    mov x4, x0 /* Jump address */
    mov x5, x1 /* Device tree address */

    mov x0, x5
    ldr x1, =0
    ldr x2, =0
    ldr x3, =0

    br x4
    b .
