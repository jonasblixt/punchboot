

.section .text

.global read_cntp_tval
read_cntp_tval:
    mrc p15, 0, r0, c14, c2, 0 
	bx lr

