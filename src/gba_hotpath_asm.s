	#include "gba_hotpath_layout.inc"

	.syntax unified
	.cpu arm7tdmi
	.arm
	.align 2

	.section .text.gba_hotpath_colour, "ax", %progbits
	.align 2
	.global gameplay_overlay_colour_distance
	.global gameplay_overlay_colour_distance_asm
	.type gameplay_overlay_colour_distance, %function
	.type gameplay_overlay_colour_distance_asm, %function
/* u32 gameplay_overlay_colour_distance(u16 left, u16 right) */
gameplay_overlay_colour_distance:
gameplay_overlay_colour_distance_asm:
	and     r2, r1, #31
	and     r3, r0, #31
	sub     r3, r3, r2
	mov     r2, r1, lsr #5
	mov     r1, r1, lsr #10
	rsb     r1, r1, r0, lsr #10
	mul     r12, r1, r1
	mov     r1, r12
	mov     r12, r0, lsl #22
	and     r0, r2, #31
	mla     r2, r3, r3, r1
	mov     r3, r2
	rsb     r0, r0, r12, lsr #27
	mla     r2, r0, r0, r3
	mov     r0, r2
	bx      lr
	.size gameplay_overlay_colour_distance, \
		.-gameplay_overlay_colour_distance

	.align 2
	.global source_detail_palette_distance
	.global source_detail_palette_distance_asm
	.type source_detail_palette_distance, %function
	.type source_detail_palette_distance_asm, %function
/* u32 source_detail_palette_distance(u16 colour, const u8 *rgb) */
source_detail_palette_distance:
source_detail_palette_distance_asm:
	and     r2, r0, #31
	ldrb    r3, [r1, #0]
	sub     r2, r2, r3, lsr #1
	mul     r12, r2, r2

	mov     r2, r0, lsr #5
	and     r2, r2, #31
	ldrb    r3, [r1, #1]
	sub     r2, r2, r3, lsr #1
	mla     r12, r2, r2, r12

	mov     r2, r0, lsr #10
	and     r2, r2, #31
	ldrb    r3, [r1, #2]
	sub     r2, r2, r3, lsr #1
	mla     r0, r2, r2, r12
	bx      lr
	.size source_detail_palette_distance, \
		.-source_detail_palette_distance

	.section .iwram, "ax", %progbits
	.align 2

	.global frontend_upgrade_recolour_words_asm
	.type frontend_upgrade_recolour_words_asm, %function
/*
 * void frontend_upgrade_recolour_words_asm(
 *     u32 *pixels,
 *     u32 word_count,
 *     u32 selected
 * )
 *
 * The upgrade list uses two two-colour ramps.  A cursor move changes only
 * FA/F8 -> FE/FC (selected) or the inverse (unselected).  Process four
 * pixels per aligned EWRAM access and use ARM conditional execution so the
 * hot loop has no per-pixel branches.
 */
frontend_upgrade_recolour_words_asm:
	cmp     r1, #0
	bxeq    lr
	cmp     r2, #0
	beq     .Lupgrade_recolour_unselected

.Lupgrade_recolour_selected:
	ldr     r3, [r0]
	and     r12, r3, #0xff
	cmp     r12, #0xfa
	cmpne   r12, #0xf8
	orreq   r3, r3, #0x00000004
	mov     r12, r3, lsr #8
	and     r12, r12, #0xff
	cmp     r12, #0xfa
	cmpne   r12, #0xf8
	orreq   r3, r3, #0x00000400
	mov     r12, r3, lsr #16
	and     r12, r12, #0xff
	cmp     r12, #0xfa
	cmpne   r12, #0xf8
	orreq   r3, r3, #0x00040000
	mov     r12, r3, lsr #24
	cmp     r12, #0xfa
	cmpne   r12, #0xf8
	orreq   r3, r3, #0x04000000
	str     r3, [r0], #4
	subs    r1, r1, #1
	bne     .Lupgrade_recolour_selected
	bx      lr

.Lupgrade_recolour_unselected:
	ldr     r3, [r0]
	and     r12, r3, #0xff
	cmp     r12, #0xfe
	cmpne   r12, #0xfc
	biceq   r3, r3, #0x00000004
	mov     r12, r3, lsr #8
	and     r12, r12, #0xff
	cmp     r12, #0xfe
	cmpne   r12, #0xfc
	biceq   r3, r3, #0x00000400
	mov     r12, r3, lsr #16
	and     r12, r12, #0xff
	cmp     r12, #0xfe
	cmpne   r12, #0xfc
	biceq   r3, r3, #0x00040000
	mov     r12, r3, lsr #24
	cmp     r12, #0xfe
	cmpne   r12, #0xfc
	biceq   r3, r3, #0x04000000
	str     r3, [r0], #4
	subs    r1, r1, #1
	bne     .Lupgrade_recolour_unselected
	bx      lr
	.size frontend_upgrade_recolour_words_asm, \
		.-frontend_upgrade_recolour_words_asm

	.align 2

	.global source_pool_lowest_set_bit_asm
	.type source_pool_lowest_set_bit_asm, %function
/* u32 source_pool_lowest_set_bit_asm(u32 bits), zero -> 32. */
source_pool_lowest_set_bit_asm:
	cmp     r0, #0
	moveq   r0, #32
	bxeq    lr
	rsb     r1, r0, #0
	and     r0, r0, r1
	ldr     r1, =0x077cb531
	mul     r2, r1, r0
	adr     r1, .Lpool_debruijn_index
	ldrb    r0, [r1, r2, lsr #27]
	bx      lr
	.size source_pool_lowest_set_bit_asm, \
		.-source_pool_lowest_set_bit_asm

	.align 2
	.global source_pool_highest_set_bit_asm
	.type source_pool_highest_set_bit_asm, %function
/* u32 source_pool_highest_set_bit_asm(u32 bits), zero -> 32. */
source_pool_highest_set_bit_asm:
	cmp     r0, #0
	moveq   r0, #32
	bxeq    lr
	mov     r1, #0
	mov     r2, #1
	mov     r2, r2, lsl #16
	cmp     r0, r2
	movhs   r0, r0, lsr #16
	addhs   r1, r1, #16
	cmp     r0, #0x100
	movhs   r0, r0, lsr #8
	addhs   r1, r1, #8
	cmp     r0, #0x10
	movhs   r0, r0, lsr #4
	addhs   r1, r1, #4
	cmp     r0, #0x4
	movhs   r0, r0, lsr #2
	addhs   r1, r1, #2
	cmp     r0, #0x2
	addhs   r1, r1, #1
	mov     r0, r1
	bx      lr
	.size source_pool_highest_set_bit_asm, \
		.-source_pool_highest_set_bit_asm

	.align 2
.Lpool_debruijn_index:
	.byte   0, 1, 28, 2, 29, 14, 24, 3
	.byte   30, 22, 20, 15, 25, 17, 4, 8
	.byte   31, 27, 13, 23, 21, 19, 16, 7
	.byte   26, 12, 18, 6, 11, 5, 10, 9
	.ltorg

	.align 2

	.global source_enemy_cache_find_exact_asm
	.type source_enemy_cache_find_exact_asm, %function
/*
 * const SourceEnemyCacheSlot *source_enemy_cache_find_exact_asm(
 *     const SourceEnemyCacheSlot *cache,
 *     u32 slot_count,
 *     u32 packed_key,
 *     u8 filter
 * )
 *
 * packed_key = graphic | shape_table<<16 | size<<24.  The fields at offsets
 * 8..11 are deliberately laid out as that little-endian word, so one aligned
 * load replaces three field comparisons.  The function is read-only,
 * zero-stack and uses only AAPCS caller-saved registers.  A miss returns NULL
 * and the C manager performs its full free/eviction/pending scan.
 */
source_enemy_cache_find_exact_asm:
	cmp     r1, #0
	moveq   r0, #0
	bxeq    lr
	mov     r12, r0
1:
	ldr     r0, [r12, #SOURCE_ENEMY_ASM_GRAPHIC_OFFSET]
	cmp     r0, r2
	bne     2f
	ldrb    r0, [r12, #SOURCE_ENEMY_ASM_FILTER_OFFSET]
	cmp     r0, r3
	bne     2f
	ldrb    r0, [r12, #SOURCE_ENEMY_ASM_VALID_OFFSET]
	cmp     r0, #0
	movne   r0, r12
	bxne    lr
2:
	add     r12, r12, #SOURCE_ENEMY_ASM_CACHE_SLOT_SIZE
	subs    r1, r1, #1
	bne     1b
	mov     r0, #0
	bx      lr
	.size source_enemy_cache_find_exact_asm, \
		.-source_enemy_cache_find_exact_asm

	.align 2
	.global source_projectile_cache_find_hint_asm
	.type source_projectile_cache_find_hint_asm, %function
/*
 * const SourceProjectileCacheSlot *source_projectile_cache_find_hint_asm(
 *     const u8 *hint, const SourceProjectileCacheSlot *cache,
 *     u32 packed_key, u32 slot_count
 * )
 *
 * The directory is advisory.  This leaf validates the encoded index, valid
 * flag and complete shape/graphic key before returning a slot; any collision
 * or stale value returns NULL and lets C perform its authoritative scan.
 */
source_projectile_cache_find_hint_asm:
	eor     r12, r2, r2, lsr #6
	eor     r12, r12, r2, lsr #12
	and     r12, r12, #63
	ldrb    r0, [r0, r12]
	subs    r0, r0, #1
	movmi   r0, #0
	bxmi    lr
	cmp     r0, r3
	movhs   r0, #0
	bxhs    lr
	add     r0, r1, r0, lsl #4
	ldrb    r1, [r0, #SOURCE_PROJECTILE_ASM_VALID_OFFSET]
	cmp     r1, #0
	moveq   r0, #0
	bxeq    lr
	ldrh    r1, [r0, #SOURCE_PROJECTILE_ASM_GRAPHIC_OFFSET]
	mov     r12, r2, lsl #16
	mov     r12, r12, lsr #16
	cmp     r1, r12
	movne   r0, #0
	bxne    lr
	ldrb    r1, [r0, #SOURCE_PROJECTILE_ASM_SHAPE_TABLE_OFFSET]
	cmp     r1, r2, lsr #16
	movne   r0, #0
	bx      lr
	.size source_projectile_cache_find_hint_asm, \
		.-source_projectile_cache_find_hint_asm

	.align 2
	.global gameplay_overlay_divmod320_asm
	.type gameplay_overlay_divmod320_asm, %function
/*
 * u32 gameplay_overlay_divmod320_asm(u16 position)
 *
 * Return x in bits 0..15 and y in bits 16..31.  For a 16-bit input,
 * floor((position >> 6) * 205 / 1024) is exactly position / 320 for the
 * complete domain.  The remainder is then position - y * 320.
 */
gameplay_overlay_divmod320_asm:
	mov     r0, r0, lsl #16
	mov     r0, r0, lsr #16
	mov     r1, r0, lsr #6
	mov     r2, #205
	mul     r3, r2, r1
	mov     r1, r3, lsr #10
	add     r2, r1, r1, lsl #2
	sub     r0, r0, r2, lsl #6
	orr     r0, r0, r1, lsl #16
	bx      lr
	.size gameplay_overlay_divmod320_asm, \
		.-gameplay_overlay_divmod320_asm

	.align 2
	.global gameplay_overlay_plot_star_tile_asm
	.type gameplay_overlay_plot_star_tile_asm, %function
/*
 * void gameplay_overlay_plot_star_tile_asm(
 *     u8 *tile, u8 local_x, u8 local_y, u32 packed_nibbles
 * )
 *
 * packed_nibbles is centre | dim<<8; dim zero suppresses the bright-star
 * cross.  Every optional arm remains inside the same 8x8 4bpp tile, matching
 * the old per-pixel clipping rule while avoiding four repeated tile lookups.
 */
gameplay_overlay_plot_star_tile_asm:
	stmfd   sp!, {r4, r5}
	and     r4, r3, #0xff
	mov     r5, r3, lsr #8
	and     r5, r5, #0xff

	/* Centre. */
	add     r12, r0, r2, lsl #2
	add     r12, r12, r1, lsr #1
	ldrb    r3, [r12]
	tst     r1, #1
	bicne   r3, r3, #0xf0
	orrne   r3, r3, r4, lsl #4
	biceq   r3, r3, #0x0f
	orreq   r3, r3, r4
	strb    r3, [r12]
	cmp     r5, #0
	beq     .Lstar_plot_return

	/* Left. */
	cmp     r1, #0
	beq     .Lstar_plot_right
	sub     r4, r1, #1
	add     r12, r0, r2, lsl #2
	add     r12, r12, r4, lsr #1
	ldrb    r3, [r12]
	tst     r4, #1
	bicne   r3, r3, #0xf0
	orrne   r3, r3, r5, lsl #4
	biceq   r3, r3, #0x0f
	orreq   r3, r3, r5
	strb    r3, [r12]

.Lstar_plot_right:
	cmp     r1, #7
	beq     .Lstar_plot_up
	add     r4, r1, #1
	add     r12, r0, r2, lsl #2
	add     r12, r12, r4, lsr #1
	ldrb    r3, [r12]
	tst     r4, #1
	bicne   r3, r3, #0xf0
	orrne   r3, r3, r5, lsl #4
	biceq   r3, r3, #0x0f
	orreq   r3, r3, r5
	strb    r3, [r12]

.Lstar_plot_up:
	cmp     r2, #0
	beq     .Lstar_plot_down
	sub     r4, r2, #1
	add     r12, r0, r4, lsl #2
	add     r12, r12, r1, lsr #1
	ldrb    r3, [r12]
	tst     r1, #1
	bicne   r3, r3, #0xf0
	orrne   r3, r3, r5, lsl #4
	biceq   r3, r3, #0x0f
	orreq   r3, r3, r5
	strb    r3, [r12]

.Lstar_plot_down:
	cmp     r2, #7
	beq     .Lstar_plot_return
	add     r4, r2, #1
	add     r12, r0, r4, lsl #2
	add     r12, r12, r1, lsr #1
	ldrb    r3, [r12]
	tst     r1, #1
	bicne   r3, r3, #0xf0
	orrne   r3, r3, r5, lsl #4
	biceq   r3, r3, #0x0f
	orreq   r3, r3, r5
	strb    r3, [r12]

.Lstar_plot_return:
	ldmfd   sp!, {r4, r5}
	bx      lr
	.size gameplay_overlay_plot_star_tile_asm, \
		.-gameplay_overlay_plot_star_tile_asm

	.global ot_mt_rand_core_asm
	.type ot_mt_rand_core_asm, %function
/*
 * uint32_t ot_mt_rand_core_asm(OtMt19937 *rng)
 *
 * OtMt19937 stores values[624] followed by three uint16_t cursors.  Keep the
 * stateful C wrapper responsible for OtLevelPortState::rng_call_count; this
 * leaf uses only AAPCS caller-saved registers and never touches the stack.
 */
ot_mt_rand_core_asm:
	add     r0, r0, #0x9c0
	ldrh    r1, [r0, #0]
	ldrh    r2, [r0, #2]
	ldrh    r3, [r0, #4]

	strh    r2, [r0, #0]
	add     r12, r2, #1
	cmp     r12, #624
	moveq   r12, #0
	strh    r12, [r0, #2]
	add     r12, r3, #1
	cmp     r12, #624
	moveq   r12, #0
	strh    r12, [r0, #4]

	sub     r0, r0, #0x9c0
	ldr     r12, [r0, r2, lsl #2]
	ldr     r2, [r0, r1, lsl #2]
	and     r2, r2, #0x80000000
	bic     r12, r12, #0x80000000
	orr     r2, r2, r12

	movs    r2, r2, lsr #1
	ldr     r12, [r0, r3, lsl #2]
	eor     r2, r12, r2
	ldrcs   r12, =0x9908b0df
	eorcs   r2, r2, r12
	str     r2, [r0, r1, lsl #2]

	eor     r2, r2, r2, lsr #11
	ldr     r3, =0x9d2c5680
	and     r3, r3, r2, lsl #7
	eor     r2, r2, r3
	ldr     r3, =0xefc60000
	and     r3, r3, r2, lsl #15
	eor     r2, r2, r3
	eor     r0, r2, r2, lsr #18
	bx      lr
	.ltorg
	.size ot_mt_rand_core_asm, .-ot_mt_rand_core_asm

	.align 2
	.global frontend_scale_swar_row_asm
	.type frontend_scale_swar_row_asm, %function
/* void frontend_scale_swar_row_asm(u32 *dst, const u32 *src, u32 blocks) */
frontend_scale_swar_row_asm:
	cmp     r2, #0
	bxeq    lr
	stmfd   sp!, {r4-r9}
.Lscale_loop:
	ldmia   r1!, {r3-r6}

	bic     r7, r3, #0xff000000
	orr     r7, r7, r4, lsl #24

	mov     r8, r4, lsr #8
	bic     r8, r8, #0x00ff0000
	orr     r8, r8, r5, lsl #16

	mov     r9, r5, lsr #16
	and     r9, r9, #0xff
	orr     r9, r9, r6, lsl #8

	stmia   r0!, {r7-r9}
	subs    r2, r2, #1
	bne     .Lscale_loop
	ldmfd   sp!, {r4-r9}
	bx      lr
	.size frontend_scale_swar_row_asm, .-frontend_scale_swar_row_asm

	.align 2
	.global source_sprite2_l2_pack_raw_word_asm
	.type source_sprite2_l2_pack_raw_word_asm, %function
/*
 * u32 source_sprite2_l2_pack_raw_word_asm(
 *     const u8 *raw, u8 filter, const u8 *palette_map
 * )
 *
 * Save the possibly unaligned source in r12 and process the four lanes in
 * order.  Unlike the consultation draft this version needs no r4/r5 save,
 * so the cache-miss inner leaf is also zero-stack.
 */
source_sprite2_l2_pack_raw_word_asm:
	mov     r12, r0
	cmp     r1, #0
	bne     .Lpack_filter

	ldrb    r0, [r12, #0]
	cmp     r0, #0
	ldrbne  r0, [r2, r0]
	ldrb    r3, [r12, #1]
	cmp     r3, #0
	ldrbne  r3, [r2, r3]
	orr     r0, r0, r3, lsl #8
	ldrb    r3, [r12, #2]
	cmp     r3, #0
	ldrbne  r3, [r2, r3]
	orr     r0, r0, r3, lsl #16
	ldrb    r3, [r12, #3]
	cmp     r3, #0
	ldrbne  r3, [r2, r3]
	orr     r0, r0, r3, lsl #24
	bx      lr

.Lpack_filter:
	ldrb    r0, [r12, #0]
	cmp     r0, #0
	andne   r0, r0, #0x0f
	orrne   r0, r0, r1
	ldrbne  r0, [r2, r0]
	ldrb    r3, [r12, #1]
	cmp     r3, #0
	andne   r3, r3, #0x0f
	orrne   r3, r3, r1
	ldrbne  r3, [r2, r3]
	orr     r0, r0, r3, lsl #8
	ldrb    r3, [r12, #2]
	cmp     r3, #0
	andne   r3, r3, #0x0f
	orrne   r3, r3, r1
	ldrbne  r3, [r2, r3]
	orr     r0, r0, r3, lsl #16
	ldrb    r3, [r12, #3]
	cmp     r3, #0
	andne   r3, r3, #0x0f
	orrne   r3, r3, r1
	ldrbne  r3, [r2, r3]
	orr     r0, r0, r3, lsl #24
	bx      lr
	.size source_sprite2_l2_pack_raw_word_asm, \
		.-source_sprite2_l2_pack_raw_word_asm

	.align 2
	.global ot_player_shot_axis_overlaps
	.global ot_player_shot_axis_overlaps_asm
	.type ot_player_shot_axis_overlaps, %function
	.type ot_player_shot_axis_overlaps_asm, %function
/* bool ot_player_shot_axis_overlaps(int16_t delta, uint16_t radius) */
ot_player_shot_axis_overlaps:
ot_player_shot_axis_overlaps_asm:
	cmp     r0, #0
	rsb     r3, r0, #0
	mov     r3, r3, lsl #16
	movge   r3, r0
	mov     r0, r1, lsl #16
	mov     r0, r0, asr #16
	movlt   r3, r3, asr #16
	cmp     r0, r3
	movle   r0, #0
	movgt   r0, #1
	bx      lr
	.size ot_player_shot_axis_overlaps, \
		.-ot_player_shot_axis_overlaps

	.align 2
	.global ot_player_shot_axis_overlaps_unsigned_asm
	.type ot_player_shot_axis_overlaps_unsigned_asm, %function
/* Optional measured one-range experiment; source parity uses the function above. */
ot_player_shot_axis_overlaps_unsigned_asm:
	add     r0, r0, r1
	sub     r0, r0, #1
	add     r1, r1, r1
	sub     r1, r1, #1
	cmp     r0, r1
	movlo   r0, #1
	movhs   r0, #0
	bx      lr
	.size ot_player_shot_axis_overlaps_unsigned_asm, \
		.-ot_player_shot_axis_overlaps_unsigned_asm

	/* Keep fast and generic kernels independently garbage-collectable. */
	.section .iwram, "ax", %progbits, unique, 1
	.align 2
	.global ot_level_port_collide_player_shot_packed_asm
	.global ot_level_port_collide_player_shot_packed_instrumented_asm
	.type ot_level_port_collide_player_shot_packed_asm, %function
	.type ot_level_port_collide_player_shot_packed_instrumented_asm, %function
/*
 * Source-parity packed collision kernel for the shipping configuration:
 * active mask + mask-fast-path + lazy result + signed strict axis test.
 *
 * The two entry points share the exact gameplay kernel.  The instrumented
 * form additionally records candidate visits for the stress harness.  A high
 * sentinel in r5 selects the release return without putting a configuration
 * test in the per-candidate loop.
 */
ot_level_port_collide_player_shot_packed_asm:
	cmp     r3, #0
	bxeq    lr
	stmfd   sp!, {r4-r11, lr}
	mov     r5, #0x80000000
	b       .Lpacked_fast_setup

ot_level_port_collide_player_shot_packed_instrumented_asm:
	cmp     r3, #0
	bxeq    lr
	stmfd   sp!, {r4-r11, lr}
	mov     r5, #0

.Lpacked_fast_setup:
	/* 12 local bytes restore 8-byte AAPCS alignment after the 36-byte save. */
	sub     sp, sp, #12
	str     r3, [sp, #0]
	mov     r4, r0
	mov     r0, #0
	strb    r0, [r3, #OT_ASM_RESULT_COLLIDED_OFFSET]
	cmp     r4, #0
	beq     .Lpacked_fast_return

	ldr     r12, [sp, #48]
	str     r12, [sp, #4]
	mov     r6, r12, lsr #8
	and     r6, r6, #0xff
	mov     r7, r12, lsr #16
	and     r7, r7, #0xff
	mov     r8, r2, lsl #16
	mov     r8, r8, asr #16
	mov     r10, r1, lsl #16
	mov     r10, r10, asr #16
	/* Reused non-zero-cycle Y base: -6 - shot_y - radius_h. */
	mvn     r3, #5
	sub     r3, r3, r8
	sub     r3, r3, r7
	str     r3, [sp, #8]
	mov     r11, #0

.Lpacked_fast_mask:
	mov     r3, r11, lsr #5
	/* state + 13652 + word_index * 4; split for ARM immediates. */
	add     r2, r3, #3408
	add     r2, r2, #4
	add     r2, r4, r2, lsl #2
	ldr     r9, [r2, #4]
	and     r1, r11, #31
	mvn     r0, #0
	ands    r9, r9, r0, lsl r1
	beq     .Lpacked_fast_next_word

	/* A miss cannot mutate the mask, so retain this word in r9. */
.Lpacked_fast_extract_candidate:
	/* De Bruijn ctz.  Rs is an isolated power of two: one-cycle early MUL. */
	rsb     r1, r9, #0
	and     r2, r9, r1
	bic     r9, r9, r2
	ldr     r0, =0x077cb531
	mul     r1, r0, r2
	adr     r0, .Lpacked_fast_debruijn_index
	bic     r12, r11, #31
	ldrb    r11, [r0, r1, lsr #27]
	add     r11, r11, r12
	and     r11, r11, #0xff
	cmp     r11, #99
	bhi     .Lpacked_fast_done

	/* enemy = state + offsetof(enemy) + index * 134. */
	add     r2, r11, r11, lsl #5
	add     r2, r11, r2, lsl #1
	add     r2, r4, r2, lsl #1
	add     r3, r2, #(OT_ASM_STATE_ENEMY_OFFSET + \
		OT_ASM_ENEMY_MAPOFFSET_OFFSET)
	ldrh    r3, [r3, #0]
	ldrh    r0, [r2, #(OT_ASM_STATE_ENEMY_OFFSET + \
		OT_ASM_ENEMY_EX_OFFSET)]
	add     r0, r0, r3
	add     r3, r6, r10
	ldrb    r1, [r2, #(OT_ASM_STATE_ENEMY_OFFSET + \
		OT_ASM_ENEMY_CYCLE_OFFSET)]
	cmp     r1, #0
	mov     r0, r0, lsl #16
	mov     r3, r3, lsl #16
	mov     r0, r0, lsr #16
	mov     r3, r3, lsr #16
	sub     r1, r0, r3
	mov     r1, r1, lsl #16
	add     r5, r5, #1
	mov     r1, r1, asr #16
	bne     .Lpacked_fast_cycle_nonzero

	/* enemycycle == 0: X radius 25+rw, Y offset/radius 12/29. */
	sub     r3, r3, r0
	mov     r3, r3, lsl #16
	cmp     r1, #0
	movlt   r1, r3, asr #16
	add     r3, r6, #25
	cmp     r3, r1
	ble     .Lpacked_fast_miss
	mvn     r1, #11
	ldrh    r3, [r2, #(OT_ASM_STATE_ENEMY_OFFSET + \
		OT_ASM_ENEMY_EY_OFFSET)]
	sub     r1, r1, r8
	add     r2, r8, #12
	add     r2, r7, r2
	sub     r1, r1, r7
	sub     r2, r2, r3
	add     r3, r3, r1
	mov     r3, r3, lsl #16
	mov     r3, r3, asr #16
	cmp     r3, #0
	mov     r2, r2, lsl #16
	add     r0, r7, #29
	movlt   r3, r2, asr #16
	cmp     r0, r3
	ble     .Lpacked_fast_miss
	b       .Lpacked_fast_hit

.Lpacked_fast_cycle_nonzero:
	sub     r3, r3, r0
	mov     r3, r3, lsl #16
	cmp     r1, #0
	movlt   r1, r3, asr #16
	add     r3, r6, #13
	cmp     r3, r1
	ble     .Lpacked_fast_miss
	ldrh    r3, [r2, #(OT_ASM_STATE_ENEMY_OFFSET + \
		OT_ASM_ENEMY_EY_OFFSET)]
	ldr     r1, [sp, #8]
	add     r2, r8, #6
	add     r2, r7, r2
	sub     r2, r2, r3
	add     r3, r3, r1
	mov     r3, r3, lsl #16
	mov     r3, r3, asr #16
	mov     r2, r2, lsl #16
	cmp     r3, #0
	movlt   r3, r2, asr #16
	add     r2, r7, #15
	cmp     r2, r3
	ble     .Lpacked_fast_miss

.Lpacked_fast_hit:
	mov     r0, r4
	mov     r1, r11
	mov     r2, r10
	mov     r3, r8
	bl      ot_player_shot_collision_apply_hit_c
	cmp     r0, #0
	bne     .Lpacked_fast_done
	/* A true hit may release/spawn linked slots: discard the cached word. */
	add     r11, r11, #1
	and     r11, r11, #0xff
	cmp     r11, #100
	blo     .Lpacked_fast_mask
	b       .Lpacked_fast_done

.Lpacked_fast_miss:
	cmp     r9, #0
	bne     .Lpacked_fast_extract_candidate
	bic     r11, r11, #31
	add     r11, r11, #32
	cmp     r11, #99
	bls     .Lpacked_fast_mask
	b       .Lpacked_fast_done

.Lpacked_fast_next_word:
	add     r3, r3, #1
	mov     r3, r3, lsl #5
	and     r11, r3, #0xff
	cmp     r11, #99
	bls     .Lpacked_fast_mask

.Lpacked_fast_done:
	tst     r5, #0x80000000
	bne     .Lpacked_fast_return
	add     r0, r4, #0x4000
	ldr     r3, [r0, #(OT_ASM_STATE_CANDIDATE_VISITS_OFFSET - 0x4000)]
	add     r3, r3, r5
	str     r3, [r0, #(OT_ASM_STATE_CANDIDATE_VISITS_OFFSET - 0x4000)]

.Lpacked_fast_return:
	add     sp, sp, #12
	ldmfd   sp!, {r4-r11, lr}
	bx      lr

	.size ot_level_port_collide_player_shot_packed_asm, \
		.-ot_level_port_collide_player_shot_packed_asm
	.size ot_level_port_collide_player_shot_packed_instrumented_asm, \
		.-ot_level_port_collide_player_shot_packed_instrumented_asm

	.align 2
.Lpacked_fast_debruijn_index:
	.byte   0, 1, 28, 2, 29, 14, 24, 3
	.byte   30, 22, 20, 15, 25, 17, 4, 8
	.byte   31, 27, 13, 23, 21, 19, 16, 7
	.byte   26, 12, 18, 6, 11, 5, 10, 9
	.ltorg

	/* The snapshot kernel is independently garbage-collectable from v35. */
	.section .iwram, "ax", %progbits, unique, 3
	.align 2
	.global ot_level_port_collide_player_shot_packed_snapshot_asm
	.global ot_level_port_collide_player_shot_packed_snapshot_instrumented_asm
	.type ot_level_port_collide_player_shot_packed_snapshot_asm, %function
	.type ot_level_port_collide_player_shot_packed_snapshot_instrumented_asm, \
		%function
/*
 * Source-exact packed collision over an 8-byte EWRAM snapshot per enemy.
 * The phase builder has folded ex+mapoffset and enemycycle's Y/radius choice
 * into two adjacent words.  Misses therefore avoid four sparse loads from
 * the 134-byte OtEnemy array; every true hit still enters the reviewed C
 * mutation path and then re-reads the live mask in source slot order.
 */
ot_level_port_collide_player_shot_packed_snapshot_asm:
	cmp     r3, #0
	bxeq    lr
	stmfd   sp!, {r4-r11, lr}
	mov     r5, #0x80000000
	b       .Lpacked_snapshot_setup

ot_level_port_collide_player_shot_packed_snapshot_instrumented_asm:
	cmp     r3, #0
	bxeq    lr
	stmfd   sp!, {r4-r11, lr}
	mov     r5, #0

.Lpacked_snapshot_setup:
	/* 12 local bytes restore 8-byte AAPCS alignment after the 36-byte save. */
	sub     sp, sp, #12
	str     r3, [sp, #0]
	mov     r4, r0
	mov     r0, #0
	strb    r0, [r3, #OT_ASM_RESULT_COLLIDED_OFFSET]
	cmp     r4, #0
	beq     .Lpacked_snapshot_return

	ldr     r12, [sp, #48]
	str     r12, [sp, #4]
	mov     r6, r12, lsr #8
	and     r6, r6, #0xff
	mov     r7, r12, lsr #16
	and     r7, r7, #0xff
	mov     r8, r2, lsl #16
	mov     r8, r8, asr #16
	mov     r10, r1, lsl #16
	mov     r10, r10, asr #16
	ldr     r3, =OT_ASM_STATE_COLLISION_SNAPSHOT_OFFSET
	add     r3, r4, r3
	str     r3, [sp, #8]
	mov     r11, #0

.Lpacked_snapshot_mask:
	mov     r3, r11, lsr #5
	/* state + 13652 + word_index * 4; split for ARM immediates. */
	add     r2, r3, #3408
	add     r2, r2, #4
	add     r2, r4, r2, lsl #2
	ldr     r9, [r2, #4]
	and     r1, r11, #31
	mvn     r0, #0
	ands    r9, r9, r0, lsl r1
	beq     .Lpacked_snapshot_next_word

.Lpacked_snapshot_extract_candidate:
	/* Ascending ctz; clear the candidate locally until a hit mutates mask. */
	rsb     r1, r9, #0
	and     r2, r9, r1
	bic     r9, r9, r2
	ldr     r0, =0x077cb531
	mul     r1, r0, r2
	adr     r0, .Lpacked_snapshot_debruijn_index
	bic     r12, r11, #31
	ldrb    r11, [r0, r1, lsr #27]
	add     r11, r11, r12
	and     r11, r11, #0xff
	cmp     r11, #99
	bhi     .Lpacked_snapshot_done
	add     r5, r5, #1

	/* Two sequential EWRAM words: {s16 x,s16 y,u16 rx,u16 ry}. */
	ldr     r2, [sp, #8]
	add     r2, r2, r11, lsl #3
	ldmia   r2, {r0, r1}

	/* Strict source X test: abs((s16)(x-shot_x-rw)) < base_rx+rw. */
	mov     r2, r0, lsl #16
	mov     r2, r2, asr #16
	sub     r2, r2, r10
	sub     r2, r2, r6
	mov     r2, r2, lsl #16
	mov     r2, r2, asr #16
	cmp     r2, #0
	rsblt   r2, r2, #0
	movlt   r2, r2, lsl #16
	movlt   r2, r2, asr #16
	mov     r3, r1, lsl #16
	mov     r3, r3, lsr #16
	add     r3, r3, r6
	cmp     r3, r2
	ble     .Lpacked_snapshot_miss

	/* Strict source Y test; snapshot y already includes -12 or -6. */
	mov     r2, r0, asr #16
	sub     r2, r2, r8
	sub     r2, r2, r7
	mov     r2, r2, lsl #16
	mov     r2, r2, asr #16
	cmp     r2, #0
	rsblt   r2, r2, #0
	movlt   r2, r2, lsl #16
	movlt   r2, r2, asr #16
	mov     r3, r1, lsr #16
	add     r3, r3, r7
	cmp     r3, r2
	ble     .Lpacked_snapshot_miss

.Lpacked_snapshot_hit:
	mov     r0, r4
	mov     r1, r11
	mov     r2, r10
	mov     r3, r8
	bl      ot_player_shot_collision_apply_hit_c
	cmp     r0, #0
	bne     .Lpacked_snapshot_done
	/* A hit can release/spawn/change linked candidates; re-read live state. */
	add     r11, r11, #1
	and     r11, r11, #0xff
	cmp     r11, #100
	blo     .Lpacked_snapshot_mask
	b       .Lpacked_snapshot_done

.Lpacked_snapshot_miss:
	cmp     r9, #0
	bne     .Lpacked_snapshot_extract_candidate
	bic     r11, r11, #31
	add     r11, r11, #32
	cmp     r11, #99
	bls     .Lpacked_snapshot_mask
	b       .Lpacked_snapshot_done

.Lpacked_snapshot_next_word:
	add     r3, r3, #1
	mov     r3, r3, lsl #5
	and     r11, r3, #0xff
	cmp     r11, #99
	bls     .Lpacked_snapshot_mask

.Lpacked_snapshot_done:
	tst     r5, #0x80000000
	bne     .Lpacked_snapshot_return
	add     r0, r4, #0x4000
	ldr     r3, [r0, #(OT_ASM_STATE_CANDIDATE_VISITS_OFFSET - 0x4000)]
	add     r3, r3, r5
	str     r3, [r0, #(OT_ASM_STATE_CANDIDATE_VISITS_OFFSET - 0x4000)]

.Lpacked_snapshot_return:
	add     sp, sp, #12
	ldmfd   sp!, {r4-r11, lr}
	bx      lr

	.size ot_level_port_collide_player_shot_packed_snapshot_asm, \
		.-ot_level_port_collide_player_shot_packed_snapshot_asm
	.size ot_level_port_collide_player_shot_packed_snapshot_instrumented_asm, \
		.-ot_level_port_collide_player_shot_packed_snapshot_instrumented_asm

	.align 2
.Lpacked_snapshot_debruijn_index:
	.byte   0, 1, 28, 2, 29, 14, 24, 3
	.byte   30, 22, 20, 15, 25, 17, 4, 8
	.byte   31, 27, 13, 23, 21, 19, 16, 7
	.byte   26, 12, 18, 6, 11, 5, 10, 9
	.ltorg

	.section .iwram, "ax", %progbits, unique, 2
	.align 2
	.global ot_level_port_collide_player_shot_packed_generic_asm
	.type ot_level_port_collide_player_shot_packed_generic_asm, %function
/*
 * void ot_level_port_collide_player_shot_packed(
 *     OtLevelPortState *state, s16 shot_x, s16 shot_y,
 *     OtShotCollisionResult *result, u32 damage_and_radii
 * )
 *
 * The dominant live-mask scan, enemy address calculation and both strict
 * AABB tests are entirely ARM.  A true overlap calls the reviewed C helper
 * for the large, rare source-parity mutation path (links, death spawns,
 * rewards, damaged graphics and event jumps), then re-reads the live mask.
 */
ot_level_port_collide_player_shot_packed_generic_asm:
	cmp     r3, #0
	bxeq    lr
	stmfd   sp!, {r4-r11, r12, lr}
	mov     r4, r0
	mov     r5, r1, lsl #16
	mov     r5, r5, asr #16
	mov     r6, r2, lsl #16
	mov     r6, r6, asr #16
	mov     r7, r3
	ldr     r8, [sp, #40]
	sub     sp, sp, #8
	mov     r9, #0
	mov     r10, #0
	ldr     r11, =ot_player_shot_collision_asm_config
	ldr     r11, [r11]

	mov     r0, #0
	strb    r0, [r7, #OT_ASM_RESULT_COLLIDED_OFFSET]
	tst     r11, #OT_ASM_CONFIG_LAZY_RESULT
	bne     .Lpacked_state_check
	strb    r0, [r7, #OT_ASM_RESULT_CONSUMED_OFFSET]
	and     r1, r8, #0xff
	strb    r1, [r7, #OT_ASM_RESULT_REMAINING_OFFSET]
	strb    r0, [r7, #OT_ASM_RESULT_HIT_COUNT_OFFSET]
	strb    r0, [r7, #OT_ASM_RESULT_KILL_COUNT_OFFSET]
	strb    r0, [r7, #OT_ASM_RESULT_EFFECT_COUNT_OFFSET]
	strb    r0, [r7, #OT_ASM_RESULT_SUPERPIXEL_COUNT_OFFSET]
	strb    r0, [r7, #OT_ASM_RESULT_CUBES_OFFSET]
	str     r0, [r7, #OT_ASM_RESULT_CASH_OFFSET]

.Lpacked_state_check:
	cmp     r4, #0
	beq     .Lpacked_return

.Lpacked_candidate:
	cmp     r9, #100
	bhs     .Lpacked_done
	tst     r11, #OT_ASM_CONFIG_ACTIVE_MASK
	beq     .Lpacked_linear_no_visit
	tst     r11, #OT_ASM_CONFIG_MASK_FAST_PATH
	bne     .Lpacked_mask
	ldr     r0, =OT_ASM_STATE_COLLISION_MASK_ACTIVE_OFFSET
	ldrb    r0, [r4, r0]
	cmp     r0, #0
	bne     .Lpacked_mask

.Lpacked_linear_visit:
	add     r10, r10, #1
.Lpacked_linear_no_visit:
	mov     r12, r9
	add     r9, r9, #1
	str     r12, [sp, #4]
	ldr     r0, =OT_ASM_STATE_ENEMY_AVAIL_OFFSET
	add     r0, r4, r0
	ldrb    r0, [r0, r12]
	cmp     r0, #0
	bne     .Lpacked_candidate
	b       .Lpacked_candidate_ready

.Lpacked_mask:
	mov     r12, r9, lsr #5
	ldr     r0, =OT_ASM_STATE_COLLISION_MASK_OFFSET
	add     r0, r4, r0
	ldr     r1, [r0, r12, lsl #2]
	and     r2, r9, #31
	mvn     r3, #0
	mov     r3, r3, lsl r2
	and     r1, r1, r3
	cmp     r1, #0
	beq     .Lpacked_next_mask_word
	rsb     r2, r1, #0
	and     r2, r1, r2
	ldr     r3, =0x077cb531
	mul     r2, r3, r2
	mov     r2, r2, lsr #27
	adr     r3, .Lpacked_debruijn_index
	ldrb    r2, [r3, r2]
	bic     r12, r9, #31
	add     r12, r12, r2
	cmp     r12, #100
	bhs     .Lpacked_done
	add     r9, r12, #1
	add     r10, r10, #1
	str     r12, [sp, #4]
	tst     r11, #OT_ASM_CONFIG_MASK_FAST_PATH
	bne     .Lpacked_candidate_ready
	ldr     r0, =OT_ASM_STATE_ENEMY_AVAIL_OFFSET
	add     r0, r4, r0
	ldrb    r0, [r0, r12]
	cmp     r0, #0
	bne     .Lpacked_candidate
	b       .Lpacked_candidate_ready

.Lpacked_next_mask_word:
	add     r9, r9, #32
	bic     r9, r9, #31
	b       .Lpacked_candidate

.Lpacked_candidate_ready:
	/* enemy = state->enemy + candidate * 134 */
	ldr     r12, [sp, #4]
	add     r0, r4, #128
	add     r0, r0, #22
	add     r0, r0, r12, lsl #7
	add     r0, r0, r12, lsl #2
	add     r0, r0, r12, lsl #1
	mov     r1, r8, lsr #8
	and     r1, r1, #0xff
	mov     r2, r8, lsr #16
	and     r2, r2, #0xff

	ldrsh   r3, [r0, #OT_ASM_ENEMY_EX_OFFSET]
	ldrh    r12, [r0, #OT_ASM_ENEMY_MAPOFFSET_OFFSET]
	add     r3, r3, r12
	sub     r3, r3, r5
	sub     r3, r3, r1
	mov     r3, r3, lsl #16
	mov     r3, r3, asr #16
	ldrb    r12, [r0, #OT_ASM_ENEMY_CYCLE_OFFSET]
	cmp     r12, #0
	addeq   r12, r1, #25
	addne   r12, r1, #13
	tst     r11, #OT_ASM_CONFIG_UNSIGNED_RANGE
	bne     .Lpacked_x_unsigned
	cmp     r3, #0
	rsblt   r3, r3, #0
	mov     r3, r3, lsl #16
	mov     r3, r3, asr #16
	mov     r1, r12, lsl #16
	mov     r1, r1, asr #16
	cmp     r3, r1
	bge     .Lpacked_candidate
	b       .Lpacked_y

.Lpacked_x_unsigned:
	add     r3, r3, r12
	sub     r3, r3, #1
	add     r1, r12, r12
	sub     r1, r1, #1
	cmp     r3, r1
	bhs     .Lpacked_candidate

.Lpacked_y:
	ldrsh   r3, [r0, #OT_ASM_ENEMY_EY_OFFSET]
	sub     r3, r3, r6
	ldrb    r12, [r0, #OT_ASM_ENEMY_CYCLE_OFFSET]
	cmp     r12, #0
	subeq   r3, r3, #12
	subne   r3, r3, #6
	sub     r3, r3, r2
	mov     r3, r3, lsl #16
	mov     r3, r3, asr #16
	addeq   r12, r2, #29
	addne   r12, r2, #15
	tst     r11, #OT_ASM_CONFIG_UNSIGNED_RANGE
	bne     .Lpacked_y_unsigned
	cmp     r3, #0
	rsblt   r3, r3, #0
	mov     r3, r3, lsl #16
	mov     r3, r3, asr #16
	mov     r1, r12, lsl #16
	mov     r1, r1, asr #16
	cmp     r3, r1
	bge     .Lpacked_candidate
	b       .Lpacked_hit

.Lpacked_y_unsigned:
	add     r3, r3, r12
	sub     r3, r3, #1
	add     r1, r12, r12
	sub     r1, r1, #1
	cmp     r3, r1
	bhs     .Lpacked_candidate

.Lpacked_hit:
	mov     r0, r4
	ldr     r1, [sp, #4]
	mov     r2, r5
	mov     r3, r6
	sub     sp, sp, #8
	str     r7, [sp, #0]
	str     r8, [sp, #4]
	bl      ot_player_shot_collision_apply_hit_c
	add     sp, sp, #8
	cmp     r0, #0
	beq     .Lpacked_candidate

.Lpacked_done:
	and     r0, r11, #OT_ASM_CONFIG_VISIT_MODE
	cmp     r0, #OT_ASM_CONFIG_VISIT_MODE
	bne     .Lpacked_return
	ldr     r0, =OT_ASM_STATE_CANDIDATE_VISITS_OFFSET
	add     r0, r4, r0
	ldr     r1, [r0]
	add     r1, r1, r10
	str     r1, [r0]

.Lpacked_return:
	add     sp, sp, #8
	ldmfd   sp!, {r4-r11, r12, lr}
	bx      lr
	.size ot_level_port_collide_player_shot_packed_generic_asm, \
		.-ot_level_port_collide_player_shot_packed_generic_asm

	.align 2
.Lpacked_debruijn_index:
	.byte   0, 1, 28, 2, 29, 14, 24, 3
	.byte   30, 22, 20, 15, 25, 17, 4, 8
	.byte   31, 27, 13, 23, 21, 19, 16, 7
	.byte   26, 12, 18, 6, 11, 5, 10, 9
	.ltorg

	.section .note.GNU-stack, "", %progbits
