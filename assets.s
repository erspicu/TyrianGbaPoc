	.section .rodata
	.balign 4

	.global bg1_tiles
bg1_tiles:
	.incbin "res/bg1_tiles.bin"

	.balign 4
	.global bg2_tiles
bg2_tiles:
	.incbin "res/bg2_tiles.bin"

	.balign 4
	.global bg3_tiles
bg3_tiles:
	.incbin "res/bg3_tiles.bin"

	.balign 4
	.global bg_palette
bg_palette:
	.incbin "res/bg_palette.bin"

	.balign 4
	.global bg1_map
bg1_map:
	.incbin "res/bg1_map.bin"

	.balign 4
	.global bg2_map
bg2_map:
	.incbin "res/bg2_map.bin"

	.balign 4
	.global bg3_map
bg3_map:
	.incbin "res/bg3_map.bin"

	.balign 4
	.global obj_tiles
obj_tiles:
	.incbin "res/obj_tiles.bin"

	.balign 4
	.global obj_palette
obj_palette:
	.incbin "res/obj_palette.bin"

	.balign 4
	.global soundbank
soundbank:
	.incbin "res/soundbank.bin"

	.balign 4
	.global tyrian_romfs
tyrian_romfs:
	.incbin "res/tyrian_romfs.bin"
	.global tyrian_romfs_end
tyrian_romfs_end:
