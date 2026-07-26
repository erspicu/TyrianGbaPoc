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
	.global frontend_frames
frontend_frames:
	.incbin "res/frontend_frames.bin"

	.balign 4
	.global frontend_palettes
frontend_palettes:
	.incbin "res/frontend_palettes.bin"

	.balign 4
	.global frontend_glyphs
frontend_glyphs:
	.incbin "res/frontend_glyphs.bin"

	.balign 4
	.global frontend_cube
frontend_cube:
	.incbin "res/frontend_cube.bin"

	.balign 4
	.global jukebox_font_tiles
jukebox_font_tiles:
	.incbin "res/jukebox_font_tiles.bin"

	.balign 4
	.global jukebox_backdrop_tiles
jukebox_backdrop_tiles:
	.incbin "res/jukebox_backdrop_tiles.bin"

	.balign 4
	.global jukebox_backdrop_map
jukebox_backdrop_map:
	.incbin "res/jukebox_backdrop_map.bin"

	.balign 4
	.global jukebox_bg_palette
jukebox_bg_palette:
	.incbin "res/jukebox_bg_palette.bin"

	.balign 4
	.global jukebox_obj_tiles
jukebox_obj_tiles:
	.incbin "res/jukebox_obj_tiles.bin"

	.balign 4
	.global jukebox_obj_palette
jukebox_obj_palette:
	.incbin "res/jukebox_obj_palette.bin"

	.balign 4
	.global jukebox_titles
jukebox_titles:
	.incbin "res/jukebox_titles.bin"

	.balign 4
	.global jukebox_reciprocal
jukebox_reciprocal:
	.incbin "res/jukebox_reciprocal.bin"

	.balign 4
	.global jukebox_sine
jukebox_sine:
	.incbin "res/jukebox_sine.bin"

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
