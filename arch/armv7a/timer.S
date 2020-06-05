

.section .text

.global read_cntp_tval
read_cntp_tval:
    /* MRC{cond} P15, <Opcode_1>, <Rd>, <CRn>, <CRm>, <Opcode_2>*/
    mrc p15, 0, r0, c14, c2, 0 
	bx lr

