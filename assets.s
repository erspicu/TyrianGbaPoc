	.section .rodata
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
