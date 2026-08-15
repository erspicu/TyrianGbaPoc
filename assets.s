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
	.global player_nort_tiles
player_nort_tiles:
	.incbin "res/player_nort_tiles.bin"

	.balign 4
	.global player_nort_palette
player_nort_palette:
	.incbin "res/player_nort_palette.bin"

	.balign 4
	.global secret_level_palettes
secret_level_palettes:
	.incbin "res/secret_level_palettes.bin"

	.balign 4
	.global insert_coin_palette
insert_coin_palette:
	.incbin "res/insert_coin_palette.bin"

	.balign 4
	.global background_gba_palette
background_gba_palette:
	.incbin "res/background_gba_palette.bin"

	.balign 4
	.global background_palette_nearest_asset
background_palette_nearest_asset:
	.incbin "res/background_palette_nearest.bin"

	.balign 4
	.global background_palette_mask_bank
background_palette_mask_bank:
	.incbin "res/background_palette_mask_bank.bin"

	.balign 4
	.global frontend_frames
frontend_frames:
	.incbin "res/frontend_frames.bin"

	.balign 4
	.global frontend_palettes
frontend_palettes:
	.incbin "res/frontend_palettes.bin"

	.balign 4
	.global frontend_stats_tiles
frontend_stats_tiles:
	.incbin "res/frontend_stats_tiles.bin"

	.balign 4
	.global frontend_stats_widths
frontend_stats_widths:
	.incbin "res/frontend_stats_widths.bin"

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
	.global frontend_static_save_name_overlay
frontend_static_save_name_overlay:
	.incbin "res/frontend_static_save_name_overlay.bin"

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
	.global frontend_static_help_strips
frontend_static_help_strips:
	.incbin "res/frontend_static_help_strips.bin"

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
	.global frontend_nav_bitmap_blocks
frontend_nav_bitmap_blocks:
	.incbin "res/frontend_nav_bitmap_blocks.bin"

	.balign 4
	.global frontend_nav_bitmap_indices
frontend_nav_bitmap_indices:
	.incbin "res/frontend_nav_bitmap_indices.bin"

	.balign 4
	.global frontend_source_stamp_offsets
frontend_source_stamp_offsets:
	.incbin "res/frontend_source_stamp_offsets.bin"

	.balign 4
	.global frontend_source_stamp_data
frontend_source_stamp_data:
	.incbin "res/frontend_source_stamp_data.bin"

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
	.global tyrend_gba_frames
tyrend_gba_frames:
	.incbin "res/tyrend_gba_frames.bin"
	.global tyrend_gba_frames_end
tyrend_gba_frames_end:

	.balign 4
	.global tyrend_gba_palette
tyrend_gba_palette:
	.incbin "res/tyrend_gba_palette.bin"
	.global tyrend_gba_palette_end
tyrend_gba_palette_end:

	.balign 4
	.global sprite2_raw_components
sprite2_raw_components:
	.incbin "res/sprite2_raw_components.bin"
	.global sprite2_raw_components_end
sprite2_raw_components_end:

	.balign 4
	.global sprite2_xmas_raw_components
sprite2_xmas_raw_components:
	.incbin "res/sprite2_xmas_raw_components.bin"
	.global sprite2_xmas_raw_components_end
sprite2_xmas_raw_components_end:

	.balign 4
	.global textres_scene
textres_scene:
	.incbin "res/textres_scene.bin"
	.global textres_scene_end
textres_scene_end:

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
