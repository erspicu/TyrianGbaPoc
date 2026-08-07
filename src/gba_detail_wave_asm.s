	.syntax unified
	.cpu arm7tdmi
	.arm
	.align 2

/* Per-render lava/water table builder; isolated so other profiles GC it. */
	.section .iwram, "ax", %progbits
	.align 2
	.global source_detail_wave_prepare_table_asm
	.type source_detail_wave_prepare_table_asm, %function
/*
 * void source_detail_wave_prepare_table_asm(
 *     u16 *destination, const s8 *profile, u32 strength_q8,
 *     u32 bg0_hofs_vofs, u32 bg1_hofs_vofs
 * )
 */
source_detail_wave_prepare_table_asm:
	stmfd   sp!, {r4-r11}
	mov     r4, r3, lsl #16
	mov     r4, r4, lsr #16
	mov     r5, r3, lsr #16
	mov     r5, r5, lsl #16
	ldr     r8, [sp, #32]
	mov     r6, r8, lsl #16
	mov     r6, r6, lsr #16
	mov     r7, r8, lsr #16
	mov     r7, r7, lsl #16
	mov     r12, #161
	cmp     r2, #256
	beq     .Lwave_full
	cmp     r2, #0
	beq     .Lwave_zero

.Lwave_scaled:
	ldrsb   r8, [r1], #1
	mul     r9, r8, r2
	cmp     r9, #0
	rsblt   r9, r9, #0
	add     r9, r9, #128
	mov     r9, r9, asr #8
	rsblt   r8, r9, #0
	movge   r8, r9
	add     r10, r4, r8
	mov     r10, r10, lsl #16
	mov     r10, r10, lsr #16
	orr     r10, r10, r5
	add     r11, r6, r8
	mov     r11, r11, lsl #16
	mov     r11, r11, lsr #16
	orr     r11, r11, r7
	stmia   r0!, {r10, r11}
	subs    r12, r12, #1
	bne     .Lwave_scaled
	b       .Lwave_done

.Lwave_full:
	ldrsb   r8, [r1], #1
	add     r10, r4, r8
	mov     r10, r10, lsl #16
	mov     r10, r10, lsr #16
	orr     r10, r10, r5
	add     r11, r6, r8
	mov     r11, r11, lsl #16
	mov     r11, r11, lsr #16
	orr     r11, r11, r7
	stmia   r0!, {r10, r11}
	subs    r12, r12, #1
	bne     .Lwave_full
	b       .Lwave_done

.Lwave_zero:
	orr     r10, r4, r5
	orr     r11, r6, r7
.Lwave_zero_loop:
	stmia   r0!, {r10, r11}
	subs    r12, r12, #1
	bne     .Lwave_zero_loop

.Lwave_done:
	ldmfd   sp!, {r4-r11}
	bx      lr
	.size source_detail_wave_prepare_table_asm, \
		.-source_detail_wave_prepare_table_asm
