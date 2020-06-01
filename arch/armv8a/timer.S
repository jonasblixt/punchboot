
.section .text

.global read_cntps_tval_el1
read_cntps_tval_el1:
	mrs	x0, cntps_tval_el1
	ret

.global read_cntp_tval_el0
read_cntp_tval_el0:
	mrs	x0, cntp_tval_el0
	ret

.global read_cntfrq_el0
read_cntfrq_el0:
	mrs	x0, cntfrq_el0
	ret
