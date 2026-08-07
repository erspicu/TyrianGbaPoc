	.syntax unified
	.cpu arm7tdmi
	.arm
	.align 2

/* Per-render WIN0H table builder; isolated so non-spotlight profiles GC it. */
	.section .iwram, "ax", %progbits
	.align 2
	.global source_detail_spotlight_prepare_table_asm
	.type source_detail_spotlight_prepare_table_asm, %function
/* void ...(u16 *destination, s16 center_x, s16 apex_y) */
source_detail_spotlight_prepare_table_asm:
	stmfd   sp!, {r4-r6}
	mov     r1, r1, lsl #16
	mov     r1, r1, asr #16
	mov     r2, r2, lsl #16
	mov     r2, r2, asr #16
	mov     r3, #0
.Lspotlight_line:
	subs    r4, r2, r3
	movle   r6, #0
	ble     .Lspotlight_store
	sub     r5, r1, r4
	add     r6, r1, r4
	add     r6, r6, #1
	cmp     r5, #0
	movlt   r5, #0
	cmp     r6, #240
	movgt   r6, #240
	cmp     r5, r6
	movge   r6, #0
	orrlt   r6, r6, r5, lsl #8
.Lspotlight_store:
	strh    r6, [r0], #2
	add     r3, r3, #1
	cmp     r3, #160
	bne     .Lspotlight_line
	mov     r6, #0
	strh    r6, [r0]
	ldmfd   sp!, {r4-r6}
	bx      lr
	.size source_detail_spotlight_prepare_table_asm, \
		.-source_detail_spotlight_prepare_table_asm
