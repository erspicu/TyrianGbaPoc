	.syntax unified
	.cpu arm7tdmi
	.arm
	.align 2

/*
 * Palette construction runs only when the authored filter state changes.
 * Keep these ARM kernels in Game Pak ROM: they still avoid Thumb's register
 * pressure and byte-at-a-time output without consuming scarce IWRAM.
 */
	.section .text.gba_detail_palette, "ax", %progbits
	.align 2
	.global source_detail_build_colour_lut_asm
	.type source_detail_build_colour_lut_asm, %function
/*
 * void source_detail_build_colour_lut_asm(
 *     u16 *destination,
 *     const u8 *source_rgb,
 *     u32 effect_kind,
 *     u32 hue_brightness
 * )
 *
 * hue_brightness bits 8..11 hold the replacement hue; its signed low byte
 * holds brightness.  The three loops deliberately specialize NONE, GLOBAL
 * and WATER hue selection once outside the 256-entry body.
 */
source_detail_build_colour_lut_asm:
	stmfd   sp!, {r4-r11}
	mov     r12, r3, lsl #24
	mov     r12, r12, asr #24
	mov     r7, r3, lsr #4
	and     r7, r7, #0xf0
	mov     r4, #0
	cmp     r2, #1
	beq     .Lpalette_global_loop
	cmp     r2, #2
	beq     .Lpalette_water_loop

.Lpalette_none_loop:
	and     r5, r4, #0x0f
	add     r5, r5, r12
	cmp     r5, #0
	movlt   r5, #0
	cmp     r5, #15
	movgt   r5, #15
	and     r6, r4, #0xf0
	orr     r6, r6, r5
	add     r8, r6, r6, lsl #1
	add     r8, r1, r8
	ldrb    r9, [r8, #0]
	ldrb    r10, [r8, #1]
	ldrb    r11, [r8, #2]
	mov     r9, r9, lsr #1
	mov     r10, r10, lsr #1
	orr     r9, r9, r10, lsl #5
	mov     r11, r11, lsr #1
	orr     r9, r9, r11, lsl #10
	strh    r9, [r0], #2
	add     r4, r4, #1
	cmp     r4, #256
	bne     .Lpalette_none_loop
	b       .Lpalette_done

.Lpalette_global_loop:
	and     r5, r4, #0x0f
	add     r5, r5, r12
	cmp     r5, #0
	movlt   r5, #0
	cmp     r5, #15
	movgt   r5, #15
	orr     r6, r7, r5
	add     r8, r6, r6, lsl #1
	add     r8, r1, r8
	ldrb    r9, [r8, #0]
	ldrb    r10, [r8, #1]
	ldrb    r11, [r8, #2]
	mov     r9, r9, lsr #1
	mov     r10, r10, lsr #1
	orr     r9, r9, r10, lsl #5
	mov     r11, r11, lsr #1
	orr     r9, r9, r11, lsl #10
	strh    r9, [r0], #2
	add     r4, r4, #1
	cmp     r4, #256
	bne     .Lpalette_global_loop
	b       .Lpalette_done

.Lpalette_water_loop:
	and     r5, r4, #0x0f
	add     r5, r5, r12
	cmp     r5, #0
	movlt   r5, #0
	cmp     r5, #15
	movgt   r5, #15
	and     r6, r4, #0xf0
	tst     r4, #0x30
	movne   r6, r7
	orr     r6, r6, r5
	add     r8, r6, r6, lsl #1
	add     r8, r1, r8
	ldrb    r9, [r8, #0]
	ldrb    r10, [r8, #1]
	ldrb    r11, [r8, #2]
	mov     r9, r9, lsr #1
	mov     r10, r10, lsr #1
	orr     r9, r9, r10, lsl #5
	mov     r11, r11, lsr #1
	orr     r9, r9, r11, lsl #10
	strh    r9, [r0], #2
	add     r4, r4, #1
	cmp     r4, #256
	bne     .Lpalette_water_loop

.Lpalette_done:
	ldmfd   sp!, {r4-r11}
	bx      lr
	.size source_detail_build_colour_lut_asm, \
		.-source_detail_build_colour_lut_asm

	.align 2
	.global source_detail_apply_colour_lut_asm
	.type source_detail_apply_colour_lut_asm, %function
/*
 * void source_detail_apply_colour_lut_asm(
 *     u16 *destination, const u8 *indices, const u16 *lut, u32 count
 * )
 * Count may be arbitrary; production ranges are multiples of four.
 */
source_detail_apply_colour_lut_asm:
	cmp     r3, #0
	bxeq    lr
	stmfd   sp!, {r4-r7}
	cmp     r3, #4
	blo     .Lpalette_map_tail
.Lpalette_map_four:
	ldrb    r4, [r1], #1
	ldrb    r5, [r1], #1
	ldrb    r6, [r1], #1
	ldrb    r7, [r1], #1
	add     r4, r2, r4, lsl #1
	add     r5, r2, r5, lsl #1
	add     r6, r2, r6, lsl #1
	add     r7, r2, r7, lsl #1
	ldrh    r4, [r4]
	ldrh    r5, [r5]
	ldrh    r6, [r6]
	ldrh    r7, [r7]
	orr     r4, r4, r5, lsl #16
	orr     r6, r6, r7, lsl #16
	stmia   r0!, {r4, r6}
	sub     r3, r3, #4
	cmp     r3, #4
	bhs     .Lpalette_map_four
.Lpalette_map_tail:
	cmp     r3, #0
	beq     .Lpalette_map_done
.Lpalette_map_one:
	ldrb    r4, [r1], #1
	add     r4, r2, r4, lsl #1
	ldrh    r4, [r4]
	strh    r4, [r0], #2
	subs    r3, r3, #1
	bne     .Lpalette_map_one
.Lpalette_map_done:
	ldmfd   sp!, {r4-r7}
	bx      lr
	.size source_detail_apply_colour_lut_asm, \
		.-source_detail_apply_colour_lut_asm
