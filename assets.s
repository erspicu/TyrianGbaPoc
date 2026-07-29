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
	.global secret_level_palettes
secret_level_palettes:
	.incbin "res/secret_level_palettes.bin"

	.balign 4
	.global insert_coin_palette
insert_coin_palette:
	.incbin "res/insert_coin_palette.bin"

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
	.global frontend_native_font
frontend_native_font:
	.incbin "res/frontend_native_font.bin"

	.balign 4
	.global frontend_pregame_font
frontend_pregame_font:
	.incbin "res/frontend_pregame_font.bin"

	.balign 4
	.global frontend_static_menu_panels
frontend_static_menu_panels:
	.incbin "res/frontend_static_menu_panels.bin"

	.balign 4
	.global frontend_static_pre_game_frames
frontend_static_pre_game_frames:
	.incbin "res/frontend_static_pre_game_frames.bin"

	.balign 4
	.global frontend_static_quit_overlay
frontend_static_quit_overlay:
	.incbin "res/frontend_static_quit_overlay.bin"

	.balign 4
	.global frontend_static_quit_choices
frontend_static_quit_choices:
	.incbin "res/frontend_static_quit_choices.bin"

	.balign 4
	.global frontend_static_quit_shade
frontend_static_quit_shade:
	.incbin "res/frontend_static_quit_shade.bin"

	.balign 4
	.global frontend_nav_obj_tiles
frontend_nav_obj_tiles:
	.incbin "res/frontend_nav_obj_tiles.bin"

	.balign 4
	.global frontend_nav_obj_meta
frontend_nav_obj_meta:
	.incbin "res/frontend_nav_obj_meta.bin"

	.balign 4
	.global frontend_nav_obj_palette
frontend_nav_obj_palette:
	.incbin "res/frontend_nav_obj_palette.bin"

	.balign 4
	.global frontend_nav_bitmap_pages
frontend_nav_bitmap_pages:
	.incbin "res/frontend_nav_bitmap_pages.bin"

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
	.global sprite2_raw_components
sprite2_raw_components:
	.incbin "res/sprite2_raw_components.bin"
	.global sprite2_raw_components_end
sprite2_raw_components_end:

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
