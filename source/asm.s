.section ".text"
.arm
.align 4

.global flush_dcache
.type flush_dcache, %function
flush_dcache:
	mov r0, #0
	mcr p15, 0, r0, c7, c10, 0
	bx lr

.global invalidate_icache
.type invalidate_icache, %function
invalidate_icache:
	mov r0, #0
	mcr p15, 0, r0, c7, c5, 0
	bx lr

.global svcMiniBackdoor
.type svcMiniBackdoor, %function
svcMiniBackdoor:
	svc 0x30
	bx lr
