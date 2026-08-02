.SUFFIXES:

DETAIL_LEVEL ?= config
GAME_SPEED ?= normal
ROUTE_EPISODE ?= 1
ROUTE_SECTION ?= 5
ROUTE_FRONT_WEAPON_POWER ?= 11
CAMPAIGN_EPISODE ?= 1
CAMPAIGN_SECTION ?= 1
CAMPAIGN_LEVELS ?= 4
STRESS_DIAGNOSTIC ?= active_mask
CAPTURE_STATE ?= 7
CAPTURE_EPISODE ?= 1
CAPTURE_SELECTION ?=
CAPTURE_SECTION ?=
AUTOTEST_DIAGNOSTIC_FLAGS ?=

DETAIL_LEVEL_CPPFLAG :=
ifeq ($(DETAIL_LEVEL),config)
else ifeq ($(DETAIL_LEVEL),low)
DETAIL_LEVEL_VALUE := 0
DETAIL_LEVEL_CPPFLAG := -DTYRIAN_GBA_DETAIL_LEVEL=$(DETAIL_LEVEL_VALUE)
else ifeq ($(DETAIL_LEVEL),normal)
DETAIL_LEVEL_VALUE := 1
DETAIL_LEVEL_CPPFLAG := -DTYRIAN_GBA_DETAIL_LEVEL=$(DETAIL_LEVEL_VALUE)
else ifeq ($(DETAIL_LEVEL),high)
DETAIL_LEVEL_VALUE := 2
DETAIL_LEVEL_CPPFLAG := -DTYRIAN_GBA_DETAIL_LEVEL=$(DETAIL_LEVEL_VALUE)
else ifeq ($(DETAIL_LEVEL),pentium)
DETAIL_LEVEL_VALUE := 3
DETAIL_LEVEL_CPPFLAG := -DTYRIAN_GBA_DETAIL_LEVEL=$(DETAIL_LEVEL_VALUE)
else
$(error DETAIL_LEVEL must be config, low, normal, high or pentium)
endif

ifeq ($(GAME_SPEED),low)
GAME_SPEED_VALUE := 0
else ifeq ($(GAME_SPEED),normal)
GAME_SPEED_VALUE := 1
else
$(error GAME_SPEED must be low or normal)
endif

ifeq ($(STRESS_DIAGNOSTIC),baseline)
STRESS_DIAGNOSTIC_FLAGS := \
	-DTYRIAN_GBA_PROJECTILE_PRECACHE_CULL=0 \
	-DTYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK=0 \
	-DTYRIAN_GBA_STRESS_SKIP_PLAYER_COLLISION=0 \
	-DTYRIAN_GBA_STRESS_SKIP_PLAYER_PROJECTILE_RENDER=0
else ifeq ($(STRESS_DIAGNOSTIC),no_collision)
STRESS_DIAGNOSTIC_FLAGS := \
	-DTYRIAN_GBA_PROJECTILE_PRECACHE_CULL=0 \
	-DTYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK=0 \
	-DTYRIAN_GBA_STRESS_SKIP_PLAYER_COLLISION=1 \
	-DTYRIAN_GBA_STRESS_SKIP_PLAYER_PROJECTILE_RENDER=0
else ifeq ($(STRESS_DIAGNOSTIC),no_render)
STRESS_DIAGNOSTIC_FLAGS := \
	-DTYRIAN_GBA_PROJECTILE_PRECACHE_CULL=0 \
	-DTYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK=0 \
	-DTYRIAN_GBA_STRESS_SKIP_PLAYER_COLLISION=0 \
	-DTYRIAN_GBA_STRESS_SKIP_PLAYER_PROJECTILE_RENDER=1
else ifeq ($(STRESS_DIAGNOSTIC),precache_cull)
STRESS_DIAGNOSTIC_FLAGS := \
	-DTYRIAN_GBA_PROJECTILE_PRECACHE_CULL=1 \
	-DTYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK=0 \
	-DTYRIAN_GBA_STRESS_SKIP_PLAYER_COLLISION=0 \
	-DTYRIAN_GBA_STRESS_SKIP_PLAYER_PROJECTILE_RENDER=0
else ifeq ($(STRESS_DIAGNOSTIC),active_mask)
STRESS_DIAGNOSTIC_FLAGS := \
	-DTYRIAN_GBA_PROJECTILE_PRECACHE_CULL=1 \
	-DTYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK=1 \
	-DTYRIAN_GBA_COLLISION_UNSIGNED_RANGE=0 \
	-DTYRIAN_GBA_COLLISION_MASK_FAST_PATH=0 \
	-DTYRIAN_GBA_STRESS_SKIP_PLAYER_COLLISION=0 \
	-DTYRIAN_GBA_STRESS_SKIP_PLAYER_PROJECTILE_RENDER=0
else ifeq ($(STRESS_DIAGNOSTIC),active_mask_range)
STRESS_DIAGNOSTIC_FLAGS := \
	-DTYRIAN_GBA_PROJECTILE_PRECACHE_CULL=1 \
	-DTYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK=1 \
	-DTYRIAN_GBA_COLLISION_UNSIGNED_RANGE=1 \
	-DTYRIAN_GBA_COLLISION_MASK_FAST_PATH=0 \
	-DTYRIAN_GBA_STRESS_SKIP_PLAYER_COLLISION=0 \
	-DTYRIAN_GBA_STRESS_SKIP_PLAYER_PROJECTILE_RENDER=0
else ifeq ($(STRESS_DIAGNOSTIC),active_mask_fast)
STRESS_DIAGNOSTIC_FLAGS := \
	-DTYRIAN_GBA_PROJECTILE_PRECACHE_CULL=1 \
	-DTYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK=1 \
	-DTYRIAN_GBA_COLLISION_UNSIGNED_RANGE=0 \
	-DTYRIAN_GBA_COLLISION_MASK_FAST_PATH=1 \
	-DTYRIAN_GBA_COLLISION_LAZY_RESULT=0 \
	-DTYRIAN_GBA_STRESS_SKIP_PLAYER_COLLISION=0 \
	-DTYRIAN_GBA_STRESS_SKIP_PLAYER_PROJECTILE_RENDER=0
else ifeq ($(STRESS_DIAGNOSTIC),active_mask_fast_lazy)
STRESS_DIAGNOSTIC_FLAGS := \
	-DTYRIAN_GBA_PROJECTILE_PRECACHE_CULL=1 \
	-DTYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK=1 \
	-DTYRIAN_GBA_COLLISION_UNSIGNED_RANGE=0 \
	-DTYRIAN_GBA_COLLISION_MASK_FAST_PATH=1 \
	-DTYRIAN_GBA_COLLISION_LAZY_RESULT=1 \
	-DTYRIAN_GBA_STRESS_SKIP_PLAYER_COLLISION=0 \
	-DTYRIAN_GBA_STRESS_SKIP_PLAYER_PROJECTILE_RENDER=0
else ifeq ($(STRESS_DIAGNOSTIC),active_mask_fast_lazy_packed)
STRESS_DIAGNOSTIC_FLAGS := \
	-DTYRIAN_GBA_PROJECTILE_PRECACHE_CULL=1 \
	-DTYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK=1 \
	-DTYRIAN_GBA_COLLISION_UNSIGNED_RANGE=0 \
	-DTYRIAN_GBA_COLLISION_MASK_FAST_PATH=1 \
	-DTYRIAN_GBA_COLLISION_LAZY_RESULT=1 \
	-DTYRIAN_GBA_COLLISION_PACKED_CALL=1 \
	-DTYRIAN_GBA_STRESS_SKIP_PLAYER_COLLISION=0 \
	-DTYRIAN_GBA_STRESS_SKIP_PLAYER_PROJECTILE_RENDER=0
else ifeq ($(STRESS_DIAGNOSTIC),active_mask_fast_defer)
STRESS_DIAGNOSTIC_FLAGS := \
	-DTYRIAN_GBA_PROJECTILE_PRECACHE_CULL=1 \
	-DTYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK=1 \
	-DTYRIAN_GBA_COLLISION_UNSIGNED_RANGE=0 \
	-DTYRIAN_GBA_COLLISION_MASK_FAST_PATH=1 \
	-DTYRIAN_GBA_DYNAMIC_FRAME_DROP=1 \
	-DTYRIAN_GBA_WALL_CLOCK_LOGIC=0 \
	-DTYRIAN_GBA_STRESS_SKIP_PLAYER_COLLISION=0 \
	-DTYRIAN_GBA_STRESS_SKIP_PLAYER_PROJECTILE_RENDER=0
else ifeq ($(STRESS_DIAGNOSTIC),active_mask_fast_wall)
STRESS_DIAGNOSTIC_FLAGS := \
	-DTYRIAN_GBA_PROJECTILE_PRECACHE_CULL=1 \
	-DTYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK=1 \
	-DTYRIAN_GBA_COLLISION_UNSIGNED_RANGE=0 \
	-DTYRIAN_GBA_COLLISION_MASK_FAST_PATH=1 \
	-DTYRIAN_GBA_DYNAMIC_FRAME_DROP=1 \
	-DTYRIAN_GBA_WALL_CLOCK_LOGIC=1 \
	-DTYRIAN_GBA_STRESS_SKIP_PLAYER_COLLISION=0 \
	-DTYRIAN_GBA_STRESS_SKIP_PLAYER_PROJECTILE_RENDER=0
else ifeq ($(STRESS_DIAGNOSTIC),active_mask_fast_wall_lazy)
STRESS_DIAGNOSTIC_FLAGS := \
	-DTYRIAN_GBA_PROJECTILE_PRECACHE_CULL=1 \
	-DTYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK=1 \
	-DTYRIAN_GBA_COLLISION_UNSIGNED_RANGE=0 \
	-DTYRIAN_GBA_COLLISION_MASK_FAST_PATH=1 \
	-DTYRIAN_GBA_COLLISION_LAZY_RESULT=1 \
	-DTYRIAN_GBA_DYNAMIC_FRAME_DROP=1 \
	-DTYRIAN_GBA_WALL_CLOCK_LOGIC=1 \
	-DTYRIAN_GBA_STRESS_SKIP_PLAYER_COLLISION=0 \
	-DTYRIAN_GBA_STRESS_SKIP_PLAYER_PROJECTILE_RENDER=0
else ifeq ($(STRESS_DIAGNOSTIC),active_mask_fast_wall_lazy_no_recovery)
STRESS_DIAGNOSTIC_FLAGS := \
	-DTYRIAN_GBA_PROJECTILE_PRECACHE_CULL=1 \
	-DTYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK=1 \
	-DTYRIAN_GBA_COLLISION_UNSIGNED_RANGE=0 \
	-DTYRIAN_GBA_COLLISION_MASK_FAST_PATH=1 \
	-DTYRIAN_GBA_COLLISION_LAZY_RESULT=1 \
	-DTYRIAN_GBA_DYNAMIC_FRAME_DROP=1 \
	-DTYRIAN_GBA_WALL_CLOCK_LOGIC=1 \
	-DTYRIAN_GBA_RECOVER_MISSED_VBLANK=0 \
	-DTYRIAN_GBA_STRESS_SKIP_PLAYER_COLLISION=0 \
	-DTYRIAN_GBA_STRESS_SKIP_PLAYER_PROJECTILE_RENDER=0
else ifeq ($(STRESS_DIAGNOSTIC),active_mask_fast_wall_lazy_packed)
STRESS_DIAGNOSTIC_FLAGS := \
	-DTYRIAN_GBA_PROJECTILE_PRECACHE_CULL=1 \
	-DTYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK=1 \
	-DTYRIAN_GBA_COLLISION_UNSIGNED_RANGE=0 \
	-DTYRIAN_GBA_COLLISION_MASK_FAST_PATH=1 \
	-DTYRIAN_GBA_COLLISION_LAZY_RESULT=1 \
	-DTYRIAN_GBA_COLLISION_PACKED_CALL=1 \
	-DTYRIAN_GBA_DYNAMIC_FRAME_DROP=1 \
	-DTYRIAN_GBA_WALL_CLOCK_LOGIC=1 \
	-DTYRIAN_GBA_STRESS_SKIP_PLAYER_COLLISION=0 \
	-DTYRIAN_GBA_STRESS_SKIP_PLAYER_PROJECTILE_RENDER=0
else ifeq ($(STRESS_DIAGNOSTIC),active_mask_fast_wall_full)
STRESS_DIAGNOSTIC_FLAGS := \
	-DTYRIAN_GBA_PROJECTILE_PRECACHE_CULL=1 \
	-DTYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK=1 \
	-DTYRIAN_GBA_COLLISION_UNSIGNED_RANGE=0 \
	-DTYRIAN_GBA_COLLISION_MASK_FAST_PATH=1 \
	-DTYRIAN_GBA_COLLISION_LAZY_RESULT=1 \
	-DTYRIAN_GBA_DYNAMIC_FRAME_DROP=1 \
	-DTYRIAN_GBA_WALL_CLOCK_LOGIC=1 \
	-DTYRIAN_GBA_PRESENTATION_DEFER=0 \
	-DTYRIAN_GBA_STRESS_SKIP_PLAYER_COLLISION=0 \
	-DTYRIAN_GBA_STRESS_SKIP_PLAYER_PROJECTILE_RENDER=0
else ifeq ($(STRESS_DIAGNOSTIC),active_mask_fast_wall_bg_live)
STRESS_DIAGNOSTIC_FLAGS := \
	-DTYRIAN_GBA_PROJECTILE_PRECACHE_CULL=1 \
	-DTYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK=1 \
	-DTYRIAN_GBA_COLLISION_UNSIGNED_RANGE=0 \
	-DTYRIAN_GBA_COLLISION_MASK_FAST_PATH=1 \
	-DTYRIAN_GBA_DYNAMIC_FRAME_DROP=1 \
	-DTYRIAN_GBA_WALL_CLOCK_LOGIC=1 \
	-DTYRIAN_GBA_PRESENTATION_DEFER=1 \
	-DTYRIAN_GBA_FREEZE_BACKGROUND_ON_DEFER=0 \
	-DTYRIAN_GBA_STRESS_SKIP_PLAYER_COLLISION=0 \
	-DTYRIAN_GBA_STRESS_SKIP_PLAYER_PROJECTILE_RENDER=0
else ifeq ($(STRESS_DIAGNOSTIC),active_mask_range_fast)
STRESS_DIAGNOSTIC_FLAGS := \
	-DTYRIAN_GBA_PROJECTILE_PRECACHE_CULL=1 \
	-DTYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK=1 \
	-DTYRIAN_GBA_COLLISION_UNSIGNED_RANGE=1 \
	-DTYRIAN_GBA_COLLISION_MASK_FAST_PATH=1 \
	-DTYRIAN_GBA_STRESS_SKIP_PLAYER_COLLISION=0 \
	-DTYRIAN_GBA_STRESS_SKIP_PLAYER_PROJECTILE_RENDER=0
else
$(error STRESS_DIAGNOSTIC must be baseline, no_collision, no_render, precache_cull, active_mask, active_mask_range, active_mask_fast, active_mask_fast_lazy, active_mask_fast_lazy_no_recovery, active_mask_fast_lazy_packed, active_mask_fast_defer, active_mask_fast_wall, active_mask_fast_wall_lazy, active_mask_fast_wall_lazy_packed, active_mask_fast_wall_full, active_mask_fast_wall_bg_live or active_mask_range_fast)
endif

# Release-positive defaults must not silently rewrite the controlled v35
# diagnostic matrix.  Only explicitly named packed/scheduler variants inherit
# those mechanisms; every other stress target keeps its historical baseline.
ifeq ($(findstring packed,$(STRESS_DIAGNOSTIC)),)
STRESS_DIAGNOSTIC_FLAGS += -DTYRIAN_GBA_COLLISION_PACKED_CALL=0
endif
STRESS_SCHEDULER_DIAGNOSTICS := \
	active_mask_fast_defer \
	active_mask_fast_wall \
	active_mask_fast_wall_lazy \
	active_mask_fast_wall_lazy_no_recovery \
	active_mask_fast_wall_lazy_packed \
	active_mask_fast_wall_full \
	active_mask_fast_wall_bg_live
ifeq ($(filter $(STRESS_DIAGNOSTIC),$(STRESS_SCHEDULER_DIAGNOSTICS)),)
STRESS_DIAGNOSTIC_FLAGS += \
	-DTYRIAN_GBA_DYNAMIC_FRAME_DROP=0 \
	-DTYRIAN_GBA_WALL_CLOCK_LOGIC=0
endif

CONFIG_SUFFIX := detail_$(DETAIL_LEVEL)_speed_$(GAME_SPEED)
TARGET := tyrian_gba_level1_pc_flow_mode4_romfs_v40_$(CONFIG_SUFFIX)
TEST_TARGET := tyrian_gba_level1_pc_flow_mode4_autotest_romfs_v40_$(CONFIG_SUFFIX)
DEATH_TEST_TARGET := tyrian_gba_level1_pc_flow_mode4_death_autotest_romfs_v40_$(CONFIG_SUFFIX)
JUKEBOX_TEST_TARGET := tyrian_gba_jukebox_autotest_romfs_v40_$(CONFIG_SUFFIX)
DEMO_TEST_TARGET := tyrian_gba_demo_autotest_romfs_v40_$(CONFIG_SUFFIX)
SAVE_TEST_TARGET := tyrian_gba_save_autotest_v61_$(CONFIG_SUFFIX)
ROMFS_MATRIX_TEST_TARGET := tyrian_gba_romfs_all_levels_matrix_v40_$(CONFIG_SUFFIX)
ROUTE_TEST_TARGET := tyrian_gba_route_smoke_ep$(ROUTE_EPISODE)_section$(ROUTE_SECTION)_v40_$(CONFIG_SUFFIX)
ARCADE_ROUTE_TEST_TARGET := tyrian_gba_arcade_route_smoke_ep1_section1_v40_$(CONFIG_SUFFIX)
SCRIPTED_SURVIVAL_TEST_TARGET := tyrian_gba_time_war_exit_autotest_v64_$(CONFIG_SUFFIX)
EPISODE_WRAP_TEST_TARGET := tyrian_gba_episode4_skip_it_autotest_v65_$(CONFIG_SUFFIX)
CAMPAIGN_TEST_TARGET := tyrian_gba_campaign_smoke_ep$(CAMPAIGN_EPISODE)_section$(CAMPAIGN_SECTION)_levels$(CAMPAIGN_LEVELS)_v40_$(CONFIG_SUFFIX)
STRESS_TARGET := tyrian_gba_full_loadout_sprite_stress_ep2_v36_$(STRESS_DIAGNOSTIC)_$(CONFIG_SUFFIX)
PLAYABLE_STRESS_TARGET := tyrian_gba_full_loadout_playable_v36_$(CONFIG_SUFFIX)
ifneq ($(strip $(CAPTURE_SELECTION)),)
FRONTEND_CAPTURE_SELECTION_TAG := _sel$(CAPTURE_SELECTION)
FRONTEND_CAPTURE_SELECTION_FLAG := \
	-DAUTOTEST_FRONTEND_CAPTURE_SELECTION=$(CAPTURE_SELECTION)
else
FRONTEND_CAPTURE_SELECTION_TAG :=
FRONTEND_CAPTURE_SELECTION_FLAG :=
endif
ifneq ($(strip $(CAPTURE_SECTION)),)
FRONTEND_CAPTURE_SECTION_TAG := _ep$(CAPTURE_EPISODE)_section$(CAPTURE_SECTION)
FRONTEND_CAPTURE_SECTION_FLAG := \
	-DAUTOTEST_FRONTEND_CAPTURE_SECTION=$(CAPTURE_SECTION) \
	-DAUTOTEST_FRONTEND_CAPTURE_EPISODE=$(CAPTURE_EPISODE)
else
FRONTEND_CAPTURE_SECTION_TAG :=
FRONTEND_CAPTURE_SECTION_FLAG :=
endif
FRONTEND_CAPTURE_VARIANT := $(FRONTEND_CAPTURE_SELECTION_TAG)$(FRONTEND_CAPTURE_SECTION_TAG)
FRONTEND_CAPTURE_TARGET := tyrian_gba_frontend_capture_state$(CAPTURE_STATE)$(FRONTEND_CAPTURE_VARIANT)_$(CONFIG_SUFFIX)
FRONTEND_MENU_STRESS_TARGET := tyrian_gba_frontend_menu_stress_v42_$(CONFIG_SUFFIX)
FRONTEND_NAV_STRESS_TARGET := tyrian_gba_frontend_nav_obj_stress_v43_$(CONFIG_SUFFIX)
FRONTEND_NAV_CAMERA_STRESS_TARGET := tyrian_gba_frontend_nav_camera_stress_v43_$(CONFIG_SUFFIX)
FRONTEND_TRANSITION_STRESS_TARGET := tyrian_gba_frontend_transition_stress_v48_$(CONFIG_SUFFIX)
BUILD := build
RES := res

PROJECT_ROOT := $(CURDIR)
VENDOR_ROOT := $(PROJECT_ROOT)/vendor
SDK_ROOT ?= $(VENDOR_ROOT)/gba-sdk
LIBGBA := $(SDK_ROOT)/libgba
MAXMOD := $(SDK_ROOT)/maxmod
TOOLS := $(SDK_ROOT)/tools/bin
GBA_CRT := $(SDK_ROOT)/devkitARM/arm-none-eabi/lib

CC := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
SIZE := arm-none-eabi-size
PYTHON ?= python

ARCH := -mcpu=arm7tdmi -mtune=arm7tdmi -mthumb -mthumb-interwork
CFLAGS := $(ARCH) -std=gnu17 -O3 -g -Wall -Wextra \
	-ffunction-sections -fdata-sections \
	-I. -Isrc -I$(LIBGBA)/include -I$(MAXMOD)/include
CFLAGS += $(EXTRA_CFLAGS)
CFLAGS += \
	$(DETAIL_LEVEL_CPPFLAG) \
	-DTYRIAN_GBA_GAME_SPEED=$(GAME_SPEED_VALUE)
ASFLAGS := $(ARCH) -x assembler-with-cpp
LINKFLAGS := $(ARCH) -B$(GBA_CRT)/ -specs=$(GBA_CRT)/gba.specs \
	-Wl,--gc-sections \
	-L$(GBA_CRT) -L$(LIBGBA)/lib -L$(MAXMOD)/lib

ASSET_STAMP := $(RES)/assets.stamp
BUILD_VERSION_HEADER := $(RES)/build_version.h
VFS_SOURCE_ROOT := vendor/tyrian/data
VFS_MANIFEST := vfs/manifest.json
VFS_IMAGE := $(RES)/tyrian_romfs.bin
VFS_META := $(RES)/tyrian_romfs_meta.h
VFS_AUDIT := $(RES)/tyrian_romfs_audit.json
VFS_OUTPUTS := $(VFS_IMAGE) $(VFS_META) $(VFS_AUDIT)
VFS_INPUTS := \
	tools/build_romfs.py \
	$(VFS_MANIFEST) \
	$(wildcard $(VFS_SOURCE_ROOT)/*)

ASSET_INPUTS := \
	Configure.h \
	tools/audit_project_independence.py \
	tools/build_assets.py \
	tools/gba_asset_support.py \
	tools/gba_music_builder.py \
	tools/templates/gba_maxmod_base.it \
	tools/background_palette_training.py \
	tools/music_maxmod_calibration.py \
	tools/frontend_native_font.txt \
	tools/frontend_pregame_font.txt \
	vendor/opentyrian/REVISION \
	vendor/tyrian/data/tyrian.hdt \
	vendor/tyrian/data/tyrian.pic \
	vendor/tyrian/data/tyrian.shp \
	$(wildcard vendor/tyrian/data/newsh*.shp) \
	vendor/tyrian/data/palette.dat \
	$(wildcard vendor/tyrian/data/shapes*.dat) \
	$(wildcard vendor/tyrian/data/tyrian*.lvl) \
	vendor/tyrian/data/tyrian.snd \
	vendor/tyrian/data/voices.snd \
	$(wildcard vendor/tyrian/image/pics/*.png) \
	$(wildcard vendor/tyrian/image/sprites/00_font/*.png) \
	$(wildcard vendor/tyrian/image/sprites/01_smallfont/*.png) \
	$(wildcard vendor/tyrian/image/sprites/02_tinyfont/*.png) \
	$(wildcard vendor/tyrian/image/sprites/03_planet/*.png) \
	$(wildcard vendor/tyrian/image/sheets/08_player_shots/*.png) \
	$(wildcard vendor/tyrian/image/sheets/09_player_ships/*.png) \
	$(wildcard vendor/tyrian/image/sheets/10_powerups/*.png) \
	$(wildcard vendor/tyrian/image/sheets/11_coins_cubes/*.png) \
	$(wildcard vendor/tyrian/image/sheets_newsh/newsh_2/*.png) \
	$(wildcard vendor/tyrian/image/sheets_newsh/newsh_4/*.png) \
	$(wildcard vendor/tyrian/image/sheets_newsh/newsh_6/*.png) \
	$(wildcard vendor/tyrian/image/sheets_newsh/newsh_e/*.png) \
	vendor/opentyrian/src/tyrian2.c \
	vendor/opentyrian/src/varz.h \
	vendor/opentyrian/src/episodes.h \
	vendor/opentyrian/src/jukebox.c \
	vendor/opentyrian/src/starlib.c \
	vendor/opentyrian/src/musmast.c \
	vendor/audio/Music/gba-opl-reference.json \
	$(wildcard vendor/audio/Music/*.tym)

ASSET_BINARIES := \
	$(RES)/obj_tiles.bin \
	$(RES)/obj_palette.bin \
	$(RES)/secret_level_palettes.bin \
	$(RES)/insert_coin_palette.bin \
	$(RES)/background_gba_palette.bin \
	$(RES)/background_palette_nearest.bin \
	$(RES)/background_palette_mask_bank.bin \
	$(RES)/frontend_frames.bin \
	$(RES)/frontend_palettes.bin \
	$(RES)/frontend_glyphs.bin \
	$(RES)/frontend_stats_tiles.bin \
	$(RES)/frontend_stats_widths.bin \
	$(RES)/frontend_native_font.bin \
	$(RES)/frontend_pregame_font.bin \
	$(RES)/frontend_static_menu_panels.bin \
	$(RES)/frontend_static_pre_game_frames.bin \
	$(RES)/frontend_static_quit_overlay.bin \
	$(RES)/frontend_static_quit_choices.bin \
	$(RES)/frontend_static_quit_shade.bin \
	$(RES)/frontend_static_help_strips.bin \
	$(RES)/frontend_nav_obj_tiles.bin \
	$(RES)/frontend_nav_obj_meta.bin \
	$(RES)/frontend_nav_obj_palette.bin \
	$(RES)/frontend_nav_bitmap_blocks.bin \
	$(RES)/frontend_nav_bitmap_indices.bin \
	$(RES)/frontend_source_stamp_offsets.bin \
	$(RES)/frontend_source_stamp_data.bin \
	$(RES)/jukebox_font_tiles.bin \
	$(RES)/jukebox_backdrop_tiles.bin \
	$(RES)/jukebox_backdrop_map.bin \
	$(RES)/jukebox_bg_palette.bin \
	$(RES)/jukebox_obj_tiles.bin \
	$(RES)/jukebox_obj_palette.bin \
	$(RES)/jukebox_titles.bin \
	$(RES)/jukebox_reciprocal.bin \
	$(RES)/jukebox_sine.bin \
	$(RES)/sprite2_raw_components.bin

TYRIAN_MUSIC_TRACKS := \
	00 01 02 03 04 05 06 07 08 09 \
	10 11 12 13 14 15 16 17 18 19 \
	20 21 22 23 24 25 26 27 28 29 \
	30 31 32 33 34 35 36 37 38 39 40
TYRIAN_MUSIC_INPUTS := $(foreach track,$(TYRIAN_MUSIC_TRACKS),\
	$(RES)/tyrian_music_$(track).it)
TYRIAN_MUSIC_ONCE_INPUTS := \
	$(RES)/tyrian_music_09_once.it \
	$(RES)/tyrian_music_10_once.it \
	$(RES)/tyrian_music_30_once.it

TYRIAN_SOUND_IDS := \
	01 02 03 04 05 06 07 08 09 10 \
	11 12 13 14 15 16 17 18 19 20 \
	21 22 23 24 25 26 27 28 29 30 \
	31 32 33 34 35 36 37 38
TYRIAN_SOUND_INPUTS := $(foreach sound,$(TYRIAN_SOUND_IDS),\
	$(RES)/source_sound_$(sound).wav)

AUDIO_INPUTS := \
	$(TYRIAN_MUSIC_INPUTS) \
	$(TYRIAN_MUSIC_ONCE_INPUTS) \
	$(TYRIAN_SOUND_INPUTS)

COMMON_OBJECTS := \
	$(BUILD)/assets.o \
	$(BUILD)/gba_heap.o \
	$(BUILD)/opentyrian_data.o \
	$(BUILD)/opentyrian_sprite2.o \
	$(BUILD)/opentyrian_level_port.o \
	$(BUILD)/romfs.o \
	$(BUILD)/opentyrian_rom_io.o

STRESS_LEVEL_OBJECT := \
	$(BUILD)/opentyrian_level_port_stress_$(STRESS_DIAGNOSTIC)_$(CONFIG_SUFFIX).o
STRESS_COMMON_OBJECTS := \
	$(filter-out $(BUILD)/opentyrian_level_port.o,$(COMMON_OBJECTS)) \
	$(STRESS_LEVEL_OBJECT)

MAIN_INCLUDES := \
	Configure.h \
	$(BUILD_VERSION_HEADER) \
	$(wildcard src/*.inc src/*/*.inc)

.PHONY: all autotest death-autotest jukebox-autotest demo-autotest \
	save-autotest \
	romfs-matrix-autotest route-smoke-autotest arcade-route-smoke-autotest \
	scripted-survival-autotest episode-wrap-autotest \
	campaign-smoke-autotest \
	full-loadout-stress full-loadout-playable frontend-capture \
	frontend-menu-stress frontend-nav-stress frontend-nav-camera-stress \
	frontend-transition-stress \
	assets clean distclean FORCE

all: $(BUILD)/$(TARGET).gba

autotest: $(BUILD)/$(TEST_TARGET).gba

death-autotest: $(BUILD)/$(DEATH_TEST_TARGET).gba

jukebox-autotest: $(BUILD)/$(JUKEBOX_TEST_TARGET).gba

demo-autotest: $(BUILD)/$(DEMO_TEST_TARGET).gba

save-autotest: $(BUILD)/$(SAVE_TEST_TARGET).gba

romfs-matrix-autotest: $(BUILD)/$(ROMFS_MATRIX_TEST_TARGET).gba

route-smoke-autotest: $(BUILD)/$(ROUTE_TEST_TARGET).gba

arcade-route-smoke-autotest: $(BUILD)/$(ARCADE_ROUTE_TEST_TARGET).gba

scripted-survival-autotest: $(BUILD)/$(SCRIPTED_SURVIVAL_TEST_TARGET).gba

episode-wrap-autotest: $(BUILD)/$(EPISODE_WRAP_TEST_TARGET).gba

campaign-smoke-autotest: $(BUILD)/$(CAMPAIGN_TEST_TARGET).gba

full-loadout-stress: $(BUILD)/$(STRESS_TARGET).gba

full-loadout-playable: $(BUILD)/$(PLAYABLE_STRESS_TARGET).gba

frontend-capture: $(BUILD)/$(FRONTEND_CAPTURE_TARGET).gba

frontend-menu-stress: $(BUILD)/$(FRONTEND_MENU_STRESS_TARGET).gba

frontend-nav-stress: $(BUILD)/$(FRONTEND_NAV_STRESS_TARGET).gba

frontend-nav-camera-stress: $(BUILD)/$(FRONTEND_NAV_CAMERA_STRESS_TARGET).gba

frontend-transition-stress: $(BUILD)/$(FRONTEND_TRANSITION_STRESS_TARGET).gba

assets: $(RES)/soundbank.bin $(RES)/soundbank.h $(VFS_OUTPUTS)

$(BUILD) $(BUILD)/preview $(RES):
	mkdir -p $@

FORCE:

$(BUILD_VERSION_HEADER): FORCE tools/write_build_version.py | $(RES)
	$(PYTHON) tools/write_build_version.py \
		--project-root "$(PROJECT_ROOT)" \
		--output "$(CURDIR)/$(BUILD_VERSION_HEADER)"

$(ASSET_STAMP): $(ASSET_INPUTS) | $(BUILD)/preview
	$(PYTHON) tools/audit_project_independence.py \
		--project-root "$(PROJECT_ROOT)" \
		--output "$(CURDIR)/$(RES)/project_independence_audit.json"
	$(PYTHON) tools/build_assets.py \
		--project-root "$(PROJECT_ROOT)" \
		--output "$(CURDIR)/$(RES)" \
		--preview-dir "$(CURDIR)/$(BUILD)/preview"

$(ASSET_BINARIES) $(AUDIO_INPUTS) $(RES)/asset_meta.h \
		$(RES)/sprite2_raw_meta.h: $(ASSET_STAMP)

$(RES)/soundbank.bin: $(ASSET_STAMP) $(AUDIO_INPUTS) | $(BUILD)
	$(TOOLS)/mmutil $(AUDIO_INPUTS) \
		-o$(RES)/soundbank.bin -h$(RES)/soundbank.h

$(RES)/soundbank.h: $(RES)/soundbank.bin

$(VFS_OUTPUTS) &: $(VFS_INPUTS) | $(RES)
	$(PYTHON) tools/build_romfs.py \
		--manifest "$(VFS_MANIFEST)" \
		--source-root "$(VFS_SOURCE_ROOT)" \
		--output "$(VFS_IMAGE)" \
		--meta-header "$(VFS_META)" \
		--audit "$(VFS_AUDIT)"

$(BUILD)/main_release_$(CONFIG_SUFFIX).o: main.c $(MAIN_INCLUDES) \
		src/opentyrian_data.h src/opentyrian_level_port.h \
		src/opentyrian_rom_io.h src/opentyrian_sprite2.h src/port_config.h \
		$(RES)/asset_meta.h $(RES)/sprite2_raw_meta.h \
		$(RES)/soundbank.h $(VFS_META) $(BUILD_VERSION_HEADER) | $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/main_test_$(CONFIG_SUFFIX).o: main.c $(MAIN_INCLUDES) \
		src/opentyrian_data.h src/opentyrian_level_port.h \
		src/opentyrian_rom_io.h src/opentyrian_sprite2.h src/port_config.h \
		$(RES)/asset_meta.h $(RES)/sprite2_raw_meta.h \
		$(RES)/soundbank.h $(VFS_META) $(BUILD_VERSION_HEADER) | $(BUILD)
	$(CC) $(CFLAGS) -DAUTOTEST \
		-DAUTOTEST_STACK_CANARY \
		-DTYRIAN_GBA_AUTOTEST_FRONT_WEAPON_POWER=11 \
		$(AUTOTEST_DIAGNOSTIC_FLAGS) \
		-MMD -MP -c $< -o $@

$(BUILD)/main_death_test_$(CONFIG_SUFFIX).o: main.c $(MAIN_INCLUDES) \
		src/opentyrian_data.h src/opentyrian_level_port.h \
		src/opentyrian_rom_io.h src/opentyrian_sprite2.h src/port_config.h \
		$(RES)/asset_meta.h $(RES)/sprite2_raw_meta.h \
		$(RES)/soundbank.h $(VFS_META) $(BUILD_VERSION_HEADER) | $(BUILD)
	$(CC) $(CFLAGS) -DAUTOTEST -DAUTOTEST_FORCE_PLAYER_DEATH \
		-DAUTOTEST_DEATH_FLOW \
		-DTYRIAN_GBA_DEV_PLAYER_INVINCIBLE=0 \
		-MMD -MP -c $< -o $@

$(BUILD)/main_jukebox_test_$(CONFIG_SUFFIX).o: main.c $(MAIN_INCLUDES) \
		src/opentyrian_data.h src/opentyrian_level_port.h \
		src/opentyrian_rom_io.h src/opentyrian_sprite2.h src/port_config.h \
		$(RES)/asset_meta.h $(RES)/sprite2_raw_meta.h \
		$(RES)/soundbank.h $(VFS_META) $(BUILD_VERSION_HEADER) | $(BUILD)
	$(CC) $(CFLAGS) -DAUTOTEST -DAUTOTEST_JUKEBOX_FLOW \
		-MMD -MP -c $< -o $@

$(BUILD)/main_demo_test_$(CONFIG_SUFFIX).o: main.c $(MAIN_INCLUDES) \
		src/opentyrian_data.h src/opentyrian_level_port.h \
		src/opentyrian_rom_io.h src/opentyrian_sprite2.h src/port_config.h \
		$(RES)/asset_meta.h $(RES)/sprite2_raw_meta.h \
		$(RES)/soundbank.h $(VFS_META) $(BUILD_VERSION_HEADER) | $(BUILD)
	$(CC) $(CFLAGS) -DAUTOTEST -DAUTOTEST_DEMO_FLOW \
		-MMD -MP -c $< -o $@

$(BUILD)/main_save_test_$(CONFIG_SUFFIX).o: main.c $(MAIN_INCLUDES) \
		src/opentyrian_data.h src/opentyrian_level_port.h \
		src/opentyrian_rom_io.h src/opentyrian_sprite2.h src/port_config.h \
		$(RES)/asset_meta.h $(RES)/sprite2_raw_meta.h \
		$(RES)/soundbank.h $(VFS_META) | $(BUILD)
	$(CC) $(CFLAGS) -DAUTOTEST -DAUTOTEST_SAVE_FLOW \
		-MMD -MP -c $< -o $@

$(BUILD)/main_romfs_matrix_test_$(CONFIG_SUFFIX).o: main.c $(MAIN_INCLUDES) \
		src/opentyrian_data.h src/opentyrian_level_port.h \
		src/opentyrian_rom_io.h src/opentyrian_sprite2.h src/port_config.h \
		$(RES)/asset_meta.h $(RES)/sprite2_raw_meta.h \
		$(RES)/soundbank.h $(VFS_META) | $(BUILD)
	$(CC) $(CFLAGS) -DAUTOTEST -DAUTOTEST_ROMFS_LEVEL_MATRIX \
		-MMD -MP -c $< -o $@

$(BUILD)/main_route_test_ep$(ROUTE_EPISODE)_section$(ROUTE_SECTION)_$(CONFIG_SUFFIX).o: \
		main.c $(MAIN_INCLUDES) \
		src/opentyrian_data.h src/opentyrian_level_port.h \
		src/opentyrian_rom_io.h src/opentyrian_sprite2.h src/port_config.h \
		$(RES)/asset_meta.h $(RES)/sprite2_raw_meta.h \
		$(RES)/soundbank.h $(VFS_META) | $(BUILD)
	$(CC) $(CFLAGS) -DAUTOTEST \
		-DAUTOTEST_FRONTEND_ROUTE_EPISODE=$(ROUTE_EPISODE) \
		-DAUTOTEST_FRONTEND_ROUTE_SECTION=$(ROUTE_SECTION) \
		-DTYRIAN_GBA_AUTOTEST_FRONT_WEAPON_POWER=$(ROUTE_FRONT_WEAPON_POWER) \
		-MMD -MP -c $< -o $@

$(BUILD)/main_arcade_route_test_ep1_section1_$(CONFIG_SUFFIX).o: \
		main.c $(MAIN_INCLUDES) \
		src/opentyrian_data.h src/opentyrian_level_port.h \
		src/opentyrian_rom_io.h src/opentyrian_sprite2.h src/port_config.h \
		$(RES)/asset_meta.h $(RES)/sprite2_raw_meta.h \
		$(RES)/soundbank.h $(VFS_META) | $(BUILD)
	$(CC) $(CFLAGS) -DAUTOTEST \
		-DAUTOTEST_FRONTEND_ROUTE_EPISODE=1 \
		-DAUTOTEST_FRONTEND_ROUTE_SECTION=1 \
		-DAUTOTEST_FRONTEND_ROUTE_ARCADE \
		-DTYRIAN_GBA_AUTOTEST_FRONT_WEAPON_POWER=11 \
		-MMD -MP -c $< -o $@

$(BUILD)/main_scripted_survival_test_$(CONFIG_SUFFIX).o: \
		main.c $(MAIN_INCLUDES) \
		src/opentyrian_data.h src/opentyrian_level_port.h \
		src/opentyrian_rom_io.h src/opentyrian_sprite2.h src/port_config.h \
		$(RES)/asset_meta.h $(RES)/sprite2_raw_meta.h \
		$(RES)/soundbank.h $(VFS_META) $(BUILD_VERSION_HEADER) | $(BUILD)
	$(CC) $(CFLAGS) -DAUTOTEST -DAUTOTEST_FORCE_PLAYER_DEATH \
		-DAUTOTEST_SCRIPTED_SURVIVAL_FLOW \
		-DAUTOTEST_FRONTEND_ROUTE_EPISODE=4 \
		-DAUTOTEST_FRONTEND_ROUTE_SECTION=37 \
		-DTYRIAN_GBA_DEV_PLAYER_INVINCIBLE=0 \
		-MMD -MP -c $< -o $@

$(BUILD)/main_episode_wrap_test_$(CONFIG_SUFFIX).o: \
		main.c $(MAIN_INCLUDES) \
		src/opentyrian_data.h src/opentyrian_level_port.h \
		src/opentyrian_rom_io.h src/opentyrian_sprite2.h src/port_config.h \
		$(RES)/asset_meta.h $(RES)/sprite2_raw_meta.h \
		$(RES)/soundbank.h $(VFS_META) $(BUILD_VERSION_HEADER) | $(BUILD)
	$(CC) $(CFLAGS) -DAUTOTEST \
		-DAUTOTEST_EPISODE_WRAP_FLOW \
		-DAUTOTEST_FRONTEND_ROUTE_EPISODE=4 \
		-DAUTOTEST_FRONTEND_ROUTE_SECTION=44 \
		-DTYRIAN_GBA_DEV_PLAYER_INVINCIBLE=1 \
		-MMD -MP -c $< -o $@

$(BUILD)/main_campaign_test_ep$(CAMPAIGN_EPISODE)_section$(CAMPAIGN_SECTION)_levels$(CAMPAIGN_LEVELS)_$(CONFIG_SUFFIX).o: \
		main.c $(MAIN_INCLUDES) \
		src/opentyrian_data.h src/opentyrian_level_port.h \
		src/opentyrian_rom_io.h src/opentyrian_sprite2.h src/port_config.h \
		$(RES)/asset_meta.h $(RES)/sprite2_raw_meta.h \
		$(RES)/soundbank.h $(VFS_META) | $(BUILD)
	$(CC) $(CFLAGS) -DAUTOTEST \
		-DAUTOTEST_FRONTEND_ROUTE_EPISODE=$(CAMPAIGN_EPISODE) \
		-DAUTOTEST_FRONTEND_ROUTE_SECTION=$(CAMPAIGN_SECTION) \
		-DAUTOTEST_CAMPAIGN_LEVEL_COUNT=$(CAMPAIGN_LEVELS) \
		-DTYRIAN_GBA_AUTOTEST_FRONT_WEAPON_POWER=11 \
		-MMD -MP -c $< -o $@

$(BUILD)/main_full_loadout_stress_$(STRESS_DIAGNOSTIC)_$(CONFIG_SUFFIX).o: \
		main.c $(MAIN_INCLUDES) \
		src/opentyrian_data.h src/opentyrian_level_port.h \
		src/opentyrian_rom_io.h src/opentyrian_sprite2.h src/port_config.h \
		$(RES)/asset_meta.h $(RES)/sprite2_raw_meta.h \
		$(RES)/soundbank.h $(VFS_META) | $(BUILD)
	$(CC) $(CFLAGS) -DAUTOTEST \
		-DAUTOTEST_FULL_LOADOUT_STRESS \
		-DAUTOTEST_FRONTEND_ROUTE_EPISODE=2 \
		-DAUTOTEST_FRONTEND_ROUTE_SECTION=1 \
		-DTYRIAN_GBA_STRESS_LOADOUT=1 \
		$(STRESS_DIAGNOSTIC_FLAGS) \
		-MMD -MP -c $< -o $@

$(BUILD)/main_full_loadout_playable_$(CONFIG_SUFFIX).o: \
		main.c $(MAIN_INCLUDES) \
		src/opentyrian_data.h src/opentyrian_level_port.h \
		src/opentyrian_rom_io.h src/opentyrian_sprite2.h src/port_config.h \
		$(RES)/asset_meta.h $(RES)/sprite2_raw_meta.h \
		$(RES)/soundbank.h $(VFS_META) | $(BUILD)
	$(CC) $(CFLAGS) \
		-DTYRIAN_GBA_STRESS_LOADOUT=1 \
		-DTYRIAN_GBA_DYNAMIC_FRAME_DROP=1 \
		-DTYRIAN_GBA_WALL_CLOCK_LOGIC=1 \
		-MMD -MP -c $< -o $@

$(BUILD)/main_frontend_capture_state$(CAPTURE_STATE)$(FRONTEND_CAPTURE_VARIANT)_$(CONFIG_SUFFIX).o: \
		main.c $(MAIN_INCLUDES) \
		src/opentyrian_data.h src/opentyrian_level_port.h \
		src/opentyrian_rom_io.h src/opentyrian_sprite2.h src/port_config.h \
		$(RES)/asset_meta.h $(RES)/sprite2_raw_meta.h \
		$(RES)/soundbank.h $(VFS_META) | $(BUILD)
	$(CC) $(CFLAGS) -DAUTOTEST \
		-DAUTOTEST_FRONTEND_CAPTURE_STATE=$(CAPTURE_STATE) \
		$(FRONTEND_CAPTURE_SELECTION_FLAG) \
		$(FRONTEND_CAPTURE_SECTION_FLAG) \
		-DTYRIAN_GBA_AUTOTEST_FRONT_WEAPON_POWER=11 \
		-MMD -MP -c $< -o $@

$(BUILD)/main_frontend_menu_stress_$(CONFIG_SUFFIX).o: \
		main.c $(MAIN_INCLUDES) \
		src/opentyrian_data.h src/opentyrian_level_port.h \
		src/opentyrian_rom_io.h src/opentyrian_sprite2.h src/port_config.h \
		$(RES)/asset_meta.h $(RES)/sprite2_raw_meta.h \
		$(RES)/soundbank.h $(VFS_META) | $(BUILD)
	$(CC) $(CFLAGS) -DAUTOTEST -DAUTOTEST_FRONTEND_STRESS \
		-DTYRIAN_GBA_AUTOTEST_FRONT_WEAPON_POWER=11 \
		-MMD -MP -c $< -o $@

$(BUILD)/main_frontend_nav_stress_$(CONFIG_SUFFIX).o: \
		main.c $(MAIN_INCLUDES) \
		src/opentyrian_data.h src/opentyrian_level_port.h \
		src/opentyrian_rom_io.h src/opentyrian_sprite2.h src/port_config.h \
		$(RES)/asset_meta.h $(RES)/sprite2_raw_meta.h \
		$(RES)/soundbank.h $(VFS_META) | $(BUILD)
	$(CC) $(CFLAGS) -DAUTOTEST -DAUTOTEST_FRONTEND_NAV_STRESS \
		-DTYRIAN_GBA_AUTOTEST_FRONT_WEAPON_POWER=11 \
		-MMD -MP -c $< -o $@

$(BUILD)/main_frontend_nav_camera_stress_$(CONFIG_SUFFIX).o: \
		main.c $(MAIN_INCLUDES) \
		src/opentyrian_data.h src/opentyrian_level_port.h \
		src/opentyrian_rom_io.h src/opentyrian_sprite2.h src/port_config.h \
		$(RES)/asset_meta.h $(RES)/sprite2_raw_meta.h \
		$(RES)/soundbank.h $(VFS_META) | $(BUILD)
	$(CC) $(CFLAGS) -DAUTOTEST -DAUTOTEST_FRONTEND_NAV_CAMERA_STRESS \
		-DTYRIAN_GBA_AUTOTEST_FRONT_WEAPON_POWER=11 \
		-MMD -MP -c $< -o $@

$(BUILD)/main_frontend_transition_stress_$(CONFIG_SUFFIX).o: \
		main.c $(MAIN_INCLUDES) \
		src/opentyrian_data.h src/opentyrian_level_port.h \
		src/opentyrian_rom_io.h src/opentyrian_sprite2.h src/port_config.h \
		$(RES)/asset_meta.h $(RES)/sprite2_raw_meta.h \
		$(RES)/soundbank.h $(VFS_META) Makefile | $(BUILD)
	$(CC) $(CFLAGS) -DAUTOTEST -DAUTOTEST_FRONTEND_TRANSITION_STRESS \
		-DAUTOTEST_STACK_CANARY \
		-DTYRIAN_GBA_DYNAMIC_FRAME_DROP=0 \
		-DTYRIAN_GBA_WALL_CLOCK_LOGIC=0 \
		-DTYRIAN_GBA_AUTOTEST_FRONT_WEAPON_POWER=11 \
		-MMD -MP -c $< -o $@

$(BUILD)/gba_heap.o: gba_heap.c | $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/opentyrian_data.o: src/opentyrian_data.c \
		src/opentyrian_data_episode_scene.inc \
		src/opentyrian_data_presentation.inc \
		src/opentyrian_data.h src/opentyrian_rom_io.h src/romfs.h | $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/opentyrian_level_port.o: src/opentyrian_level_port.c \
		src/opentyrian_level_port.h src/opentyrian_data.h | $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(STRESS_LEVEL_OBJECT): src/opentyrian_level_port.c \
		src/opentyrian_level_port.h src/opentyrian_data.h | $(BUILD)
	$(CC) $(CFLAGS) -DAUTOTEST_FULL_LOADOUT_STRESS \
		$(STRESS_DIAGNOSTIC_FLAGS) \
		-MMD -MP -c $< -o $@

$(BUILD)/opentyrian_sprite2.o: src/opentyrian_sprite2.c \
		src/opentyrian_sprite2.h src/opentyrian_data.h \
		$(RES)/sprite2_raw_meta.h | $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/romfs.o: src/romfs.c src/romfs.h | $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/opentyrian_rom_io.o: src/opentyrian_rom_io.c \
		src/opentyrian_rom_io.h src/romfs.h $(VFS_META) | $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/assets.o: assets.s $(ASSET_BINARIES) \
		$(RES)/soundbank.bin $(VFS_IMAGE) \
		$(RES)/sprite2_raw_meta.h | $(BUILD)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD)/$(TARGET).elf: \
		$(BUILD)/main_release_$(CONFIG_SUFFIX).o $(COMMON_OBJECTS)
	$(CC) $(LINKFLAGS) -Wl,-Map,$(BUILD)/$(TARGET).map $^ \
		-lmm -lgba -o $@
	$(SIZE) $@

$(BUILD)/$(TEST_TARGET).elf: \
		$(BUILD)/main_test_$(CONFIG_SUFFIX).o $(COMMON_OBJECTS)
	$(CC) $(LINKFLAGS) -Wl,-Map,$(BUILD)/$(TEST_TARGET).map $^ \
		-lmm -lgba -o $@
	$(SIZE) $@

$(BUILD)/$(DEATH_TEST_TARGET).elf: \
		$(BUILD)/main_death_test_$(CONFIG_SUFFIX).o $(COMMON_OBJECTS)
	$(CC) $(LINKFLAGS) -Wl,-Map,$(BUILD)/$(DEATH_TEST_TARGET).map $^ \
		-lmm -lgba -o $@
	$(SIZE) $@

$(BUILD)/$(JUKEBOX_TEST_TARGET).elf: \
		$(BUILD)/main_jukebox_test_$(CONFIG_SUFFIX).o $(COMMON_OBJECTS)
	$(CC) $(LINKFLAGS) -Wl,-Map,$(BUILD)/$(JUKEBOX_TEST_TARGET).map $^ \
		-lmm -lgba -o $@
	$(SIZE) $@

$(BUILD)/$(DEMO_TEST_TARGET).elf: \
		$(BUILD)/main_demo_test_$(CONFIG_SUFFIX).o $(COMMON_OBJECTS)
	$(CC) $(LINKFLAGS) -Wl,-Map,$(BUILD)/$(DEMO_TEST_TARGET).map $^ \
		-lmm -lgba -o $@
	$(SIZE) $@

$(BUILD)/$(SAVE_TEST_TARGET).elf: \
		$(BUILD)/main_save_test_$(CONFIG_SUFFIX).o $(COMMON_OBJECTS)
	$(CC) $(LINKFLAGS) -Wl,-Map,$(BUILD)/$(SAVE_TEST_TARGET).map $^ \
		-lmm -lgba -o $@
	$(SIZE) $@

$(BUILD)/$(ROMFS_MATRIX_TEST_TARGET).elf: \
		$(BUILD)/main_romfs_matrix_test_$(CONFIG_SUFFIX).o $(COMMON_OBJECTS)
	$(CC) $(LINKFLAGS) -Wl,-Map,$(BUILD)/$(ROMFS_MATRIX_TEST_TARGET).map $^ \
		-lmm -lgba -o $@
	$(SIZE) $@

$(BUILD)/$(ROUTE_TEST_TARGET).elf: \
		$(BUILD)/main_route_test_ep$(ROUTE_EPISODE)_section$(ROUTE_SECTION)_$(CONFIG_SUFFIX).o \
		$(COMMON_OBJECTS)
	$(CC) $(LINKFLAGS) -Wl,-Map,$(BUILD)/$(ROUTE_TEST_TARGET).map $^ \
	-lmm -lgba -o $@
	$(SIZE) $@

$(BUILD)/$(ARCADE_ROUTE_TEST_TARGET).elf: \
		$(BUILD)/main_arcade_route_test_ep1_section1_$(CONFIG_SUFFIX).o \
		$(COMMON_OBJECTS)
	$(CC) $(LINKFLAGS) -Wl,-Map,$(BUILD)/$(ARCADE_ROUTE_TEST_TARGET).map $^ \
		-lmm -lgba -o $@
	$(SIZE) $@

$(BUILD)/$(SCRIPTED_SURVIVAL_TEST_TARGET).elf: \
		$(BUILD)/main_scripted_survival_test_$(CONFIG_SUFFIX).o \
		$(COMMON_OBJECTS)
	$(CC) $(LINKFLAGS) -Wl,-Map,$(BUILD)/$(SCRIPTED_SURVIVAL_TEST_TARGET).map $^ \
		-lmm -lgba -o $@
	$(SIZE) $@

$(BUILD)/$(EPISODE_WRAP_TEST_TARGET).elf: \
		$(BUILD)/main_episode_wrap_test_$(CONFIG_SUFFIX).o \
		$(COMMON_OBJECTS)
	$(CC) $(LINKFLAGS) -Wl,-Map,$(BUILD)/$(EPISODE_WRAP_TEST_TARGET).map $^ \
		-lmm -lgba -o $@
	$(SIZE) $@

$(BUILD)/$(CAMPAIGN_TEST_TARGET).elf: \
		$(BUILD)/main_campaign_test_ep$(CAMPAIGN_EPISODE)_section$(CAMPAIGN_SECTION)_levels$(CAMPAIGN_LEVELS)_$(CONFIG_SUFFIX).o \
		$(COMMON_OBJECTS)
	$(CC) $(LINKFLAGS) -Wl,-Map,$(BUILD)/$(CAMPAIGN_TEST_TARGET).map $^ \
	-lmm -lgba -o $@
	$(SIZE) $@

$(BUILD)/$(STRESS_TARGET).elf: \
		$(BUILD)/main_full_loadout_stress_$(STRESS_DIAGNOSTIC)_$(CONFIG_SUFFIX).o \
		$(STRESS_COMMON_OBJECTS)
	$(CC) $(LINKFLAGS) -Wl,-Map,$(BUILD)/$(STRESS_TARGET).map $^ \
		-lmm -lgba -o $@
	$(SIZE) $@

$(BUILD)/$(PLAYABLE_STRESS_TARGET).elf: \
		$(BUILD)/main_full_loadout_playable_$(CONFIG_SUFFIX).o \
		$(COMMON_OBJECTS)
	$(CC) $(LINKFLAGS) -Wl,-Map,$(BUILD)/$(PLAYABLE_STRESS_TARGET).map $^ \
	-lmm -lgba -o $@
	$(SIZE) $@

$(BUILD)/$(FRONTEND_CAPTURE_TARGET).elf: \
		$(BUILD)/main_frontend_capture_state$(CAPTURE_STATE)$(FRONTEND_CAPTURE_VARIANT)_$(CONFIG_SUFFIX).o \
		$(COMMON_OBJECTS)
	$(CC) $(LINKFLAGS) -Wl,-Map,$(BUILD)/$(FRONTEND_CAPTURE_TARGET).map $^ \
		-lmm -lgba -o $@
	$(SIZE) $@

$(BUILD)/$(FRONTEND_MENU_STRESS_TARGET).elf: \
		$(BUILD)/main_frontend_menu_stress_$(CONFIG_SUFFIX).o \
		$(COMMON_OBJECTS)
	$(CC) $(LINKFLAGS) -Wl,-Map,$(BUILD)/$(FRONTEND_MENU_STRESS_TARGET).map $^ \
	-lmm -lgba -o $@
	$(SIZE) $@

$(BUILD)/$(FRONTEND_NAV_STRESS_TARGET).elf: \
		$(BUILD)/main_frontend_nav_stress_$(CONFIG_SUFFIX).o \
		$(COMMON_OBJECTS)
	$(CC) $(LINKFLAGS) -Wl,-Map,$(BUILD)/$(FRONTEND_NAV_STRESS_TARGET).map $^ \
		-lmm -lgba -o $@
	$(SIZE) $@

$(BUILD)/$(FRONTEND_NAV_CAMERA_STRESS_TARGET).elf: \
		$(BUILD)/main_frontend_nav_camera_stress_$(CONFIG_SUFFIX).o \
		$(COMMON_OBJECTS)
	$(CC) $(LINKFLAGS) -Wl,-Map,$(BUILD)/$(FRONTEND_NAV_CAMERA_STRESS_TARGET).map $^ \
		-lmm -lgba -o $@
	$(SIZE) $@

$(BUILD)/$(FRONTEND_TRANSITION_STRESS_TARGET).elf: \
		$(BUILD)/main_frontend_transition_stress_$(CONFIG_SUFFIX).o \
		$(COMMON_OBJECTS)
	$(CC) $(LINKFLAGS) -Wl,-Map,$(BUILD)/$(FRONTEND_TRANSITION_STRESS_TARGET).map $^ \
		-lmm -lgba -o $@
	$(SIZE) $@

$(BUILD)/$(TARGET).gba: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@
	$(TOOLS)/gbafix $@ "-tTYRIAN GBA" -cTYGA -m00

$(BUILD)/$(TEST_TARGET).gba: $(BUILD)/$(TEST_TARGET).elf
	$(OBJCOPY) -O binary $< $@
	$(TOOLS)/gbafix $@ "-tTYRIAN TEST" -cTYGT -m00

$(BUILD)/$(DEATH_TEST_TARGET).gba: $(BUILD)/$(DEATH_TEST_TARGET).elf
	$(OBJCOPY) -O binary $< $@
	$(TOOLS)/gbafix $@ "-tTYRIAN DEATH" -cTYGD -m00

$(BUILD)/$(JUKEBOX_TEST_TARGET).gba: $(BUILD)/$(JUKEBOX_TEST_TARGET).elf
	$(OBJCOPY) -O binary $< $@
	$(TOOLS)/gbafix $@ "-tTYRIAN JUKE" -cTYGJ -m00

$(BUILD)/$(DEMO_TEST_TARGET).gba: $(BUILD)/$(DEMO_TEST_TARGET).elf
	$(OBJCOPY) -O binary $< $@
	$(TOOLS)/gbafix $@ "-tTYRIAN DEMO" -cTYGX -m00

$(BUILD)/$(SAVE_TEST_TARGET).gba: $(BUILD)/$(SAVE_TEST_TARGET).elf
	$(OBJCOPY) -O binary $< $@
	$(TOOLS)/gbafix $@ "-tTYRIAN SAVE" -cTYGV -m00

$(BUILD)/$(ROMFS_MATRIX_TEST_TARGET).gba: \
		$(BUILD)/$(ROMFS_MATRIX_TEST_TARGET).elf
	$(OBJCOPY) -O binary $< $@
	$(TOOLS)/gbafix $@ "-tTYRIAN MATRIX" -cTYGM -m00

$(BUILD)/$(ROUTE_TEST_TARGET).gba: $(BUILD)/$(ROUTE_TEST_TARGET).elf
	$(OBJCOPY) -O binary $< $@
	$(TOOLS)/gbafix $@ "-tTYRIAN ROUTE" -cTYGR -m00

$(BUILD)/$(ARCADE_ROUTE_TEST_TARGET).gba: \
		$(BUILD)/$(ARCADE_ROUTE_TEST_TARGET).elf
	$(OBJCOPY) -O binary $< $@
	$(TOOLS)/gbafix $@ "-tTYRIAN ARCADE" -cTYGQ -m00

$(BUILD)/$(SCRIPTED_SURVIVAL_TEST_TARGET).gba: \
		$(BUILD)/$(SCRIPTED_SURVIVAL_TEST_TARGET).elf
	$(OBJCOPY) -O binary $< $@
	$(TOOLS)/gbafix $@ "-tTYR TIME WAR" -cTYGH -m00

$(BUILD)/$(EPISODE_WRAP_TEST_TARGET).gba: \
		$(BUILD)/$(EPISODE_WRAP_TEST_TARGET).elf
	$(OBJCOPY) -O binary $< $@
	$(TOOLS)/gbafix $@ "-tTYR SKIP IT" -cTYGI -m00

$(BUILD)/$(CAMPAIGN_TEST_TARGET).gba: $(BUILD)/$(CAMPAIGN_TEST_TARGET).elf
	$(OBJCOPY) -O binary $< $@
	$(TOOLS)/gbafix $@ "-tTYRIAN CAMP" -cTYGC -m00

$(BUILD)/$(STRESS_TARGET).gba: $(BUILD)/$(STRESS_TARGET).elf
	$(OBJCOPY) -O binary $< $@
	$(TOOLS)/gbafix $@ "-tTYR STRESS" -cTYGS -m00

$(BUILD)/$(PLAYABLE_STRESS_TARGET).gba: \
		$(BUILD)/$(PLAYABLE_STRESS_TARGET).elf
	$(OBJCOPY) -O binary $< $@
	$(TOOLS)/gbafix $@ "-tTYR FULL ARM" -cTYGP -m00

$(BUILD)/$(FRONTEND_CAPTURE_TARGET).gba: \
		$(BUILD)/$(FRONTEND_CAPTURE_TARGET).elf
	$(OBJCOPY) -O binary $< $@
	$(TOOLS)/gbafix $@ "-tTYR UI CAP" -cTYGU -m00

$(BUILD)/$(FRONTEND_MENU_STRESS_TARGET).gba: \
		$(BUILD)/$(FRONTEND_MENU_STRESS_TARGET).elf
	$(OBJCOPY) -O binary $< $@
	$(TOOLS)/gbafix $@ "-tTYR UI TEST" -cTYGF -m00

$(BUILD)/$(FRONTEND_NAV_STRESS_TARGET).gba: \
		$(BUILD)/$(FRONTEND_NAV_STRESS_TARGET).elf
	$(OBJCOPY) -O binary $< $@
	$(TOOLS)/gbafix $@ "-tTYR NAV TEST" -cTYGN -m00

$(BUILD)/$(FRONTEND_NAV_CAMERA_STRESS_TARGET).gba: \
		$(BUILD)/$(FRONTEND_NAV_CAMERA_STRESS_TARGET).elf
	$(OBJCOPY) -O binary $< $@
	$(TOOLS)/gbafix $@ "-tTYR NAV MOVE" -cTYGK -m00

$(BUILD)/$(FRONTEND_TRANSITION_STRESS_TARGET).gba: \
		$(BUILD)/$(FRONTEND_TRANSITION_STRESS_TARGET).elf
	$(OBJCOPY) -O binary $< $@
	$(TOOLS)/gbafix $@ "-tTYR UI MOVE" -cTYGW -m00

clean:
	rm -f \
		$(BUILD)/main_release_$(CONFIG_SUFFIX).o \
		$(BUILD)/main_release_$(CONFIG_SUFFIX).d \
		$(BUILD)/main_test_$(CONFIG_SUFFIX).o \
		$(BUILD)/main_test_$(CONFIG_SUFFIX).d \
		$(BUILD)/main_death_test_$(CONFIG_SUFFIX).o \
		$(BUILD)/main_death_test_$(CONFIG_SUFFIX).d \
		$(BUILD)/main_jukebox_test_$(CONFIG_SUFFIX).o \
		$(BUILD)/main_jukebox_test_$(CONFIG_SUFFIX).d \
		$(BUILD)/main_demo_test_$(CONFIG_SUFFIX).o \
		$(BUILD)/main_demo_test_$(CONFIG_SUFFIX).d \
		$(BUILD)/main_save_test_$(CONFIG_SUFFIX).o \
		$(BUILD)/main_save_test_$(CONFIG_SUFFIX).d \
		$(BUILD)/main_romfs_matrix_test_$(CONFIG_SUFFIX).o \
		$(BUILD)/main_romfs_matrix_test_$(CONFIG_SUFFIX).d \
		$(BUILD)/main_route_test_ep$(ROUTE_EPISODE)_section$(ROUTE_SECTION)_$(CONFIG_SUFFIX).o \
		$(BUILD)/main_route_test_ep$(ROUTE_EPISODE)_section$(ROUTE_SECTION)_$(CONFIG_SUFFIX).d \
		$(BUILD)/main_arcade_route_test_ep1_section1_$(CONFIG_SUFFIX).o \
		$(BUILD)/main_arcade_route_test_ep1_section1_$(CONFIG_SUFFIX).d \
		$(BUILD)/main_scripted_survival_test_$(CONFIG_SUFFIX).o \
		$(BUILD)/main_scripted_survival_test_$(CONFIG_SUFFIX).d \
		$(BUILD)/main_episode_wrap_test_$(CONFIG_SUFFIX).o \
		$(BUILD)/main_episode_wrap_test_$(CONFIG_SUFFIX).d \
		$(BUILD)/main_campaign_test_ep$(CAMPAIGN_EPISODE)_section$(CAMPAIGN_SECTION)_levels$(CAMPAIGN_LEVELS)_$(CONFIG_SUFFIX).o \
		$(BUILD)/main_campaign_test_ep$(CAMPAIGN_EPISODE)_section$(CAMPAIGN_SECTION)_levels$(CAMPAIGN_LEVELS)_$(CONFIG_SUFFIX).d \
		$(BUILD)/main_full_loadout_stress_$(STRESS_DIAGNOSTIC)_$(CONFIG_SUFFIX).o \
		$(BUILD)/main_full_loadout_stress_$(STRESS_DIAGNOSTIC)_$(CONFIG_SUFFIX).d \
		$(STRESS_LEVEL_OBJECT) \
		$(STRESS_LEVEL_OBJECT:.o=.d) \
		$(BUILD)/main_full_loadout_playable_$(CONFIG_SUFFIX).o \
		$(BUILD)/main_full_loadout_playable_$(CONFIG_SUFFIX).d \
		$(BUILD)/main_frontend_capture_state$(CAPTURE_STATE)$(FRONTEND_CAPTURE_VARIANT)_$(CONFIG_SUFFIX).o \
		$(BUILD)/main_frontend_capture_state$(CAPTURE_STATE)$(FRONTEND_CAPTURE_VARIANT)_$(CONFIG_SUFFIX).d \
		$(BUILD)/main_frontend_menu_stress_$(CONFIG_SUFFIX).o \
		$(BUILD)/main_frontend_menu_stress_$(CONFIG_SUFFIX).d \
		$(BUILD)/main_frontend_nav_stress_$(CONFIG_SUFFIX).o \
		$(BUILD)/main_frontend_nav_stress_$(CONFIG_SUFFIX).d \
		$(BUILD)/main_frontend_nav_camera_stress_$(CONFIG_SUFFIX).o \
		$(BUILD)/main_frontend_nav_camera_stress_$(CONFIG_SUFFIX).d \
		$(BUILD)/main_frontend_transition_stress_$(CONFIG_SUFFIX).o \
		$(BUILD)/main_frontend_transition_stress_$(CONFIG_SUFFIX).d \
		$(BUILD)/gba_heap.o $(BUILD)/gba_heap.d \
		$(BUILD)/opentyrian_data.o \
		$(BUILD)/opentyrian_data.d \
		$(BUILD)/opentyrian_sprite2.o \
		$(BUILD)/opentyrian_sprite2.d \
		$(BUILD)/opentyrian_level_port.o \
		$(BUILD)/opentyrian_level_port.d \
		$(BUILD)/romfs.o $(BUILD)/romfs.d \
		$(BUILD)/opentyrian_rom_io.o \
		$(BUILD)/opentyrian_rom_io.d \
		$(BUILD)/assets.o \
		$(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).gba \
		$(BUILD)/$(TARGET).map \
		$(BUILD)/$(TEST_TARGET).elf $(BUILD)/$(TEST_TARGET).gba \
		$(BUILD)/$(TEST_TARGET).map \
		$(BUILD)/$(DEATH_TEST_TARGET).elf \
		$(BUILD)/$(DEATH_TEST_TARGET).gba \
		$(BUILD)/$(DEATH_TEST_TARGET).map \
		$(BUILD)/$(JUKEBOX_TEST_TARGET).elf \
		$(BUILD)/$(JUKEBOX_TEST_TARGET).gba \
		$(BUILD)/$(JUKEBOX_TEST_TARGET).map \
		$(BUILD)/$(DEMO_TEST_TARGET).elf \
		$(BUILD)/$(DEMO_TEST_TARGET).gba \
		$(BUILD)/$(DEMO_TEST_TARGET).map \
		$(BUILD)/$(ROMFS_MATRIX_TEST_TARGET).elf \
		$(BUILD)/$(ROMFS_MATRIX_TEST_TARGET).gba \
		$(BUILD)/$(ROMFS_MATRIX_TEST_TARGET).map \
		$(BUILD)/$(ROUTE_TEST_TARGET).elf \
		$(BUILD)/$(ROUTE_TEST_TARGET).gba \
		$(BUILD)/$(ROUTE_TEST_TARGET).map \
		$(BUILD)/$(ARCADE_ROUTE_TEST_TARGET).elf \
		$(BUILD)/$(ARCADE_ROUTE_TEST_TARGET).gba \
		$(BUILD)/$(ARCADE_ROUTE_TEST_TARGET).map \
		$(BUILD)/$(SCRIPTED_SURVIVAL_TEST_TARGET).elf \
		$(BUILD)/$(SCRIPTED_SURVIVAL_TEST_TARGET).gba \
		$(BUILD)/$(SCRIPTED_SURVIVAL_TEST_TARGET).map \
		$(BUILD)/$(EPISODE_WRAP_TEST_TARGET).elf \
		$(BUILD)/$(EPISODE_WRAP_TEST_TARGET).gba \
		$(BUILD)/$(EPISODE_WRAP_TEST_TARGET).map \
		$(BUILD)/$(CAMPAIGN_TEST_TARGET).elf \
		$(BUILD)/$(CAMPAIGN_TEST_TARGET).gba \
		$(BUILD)/$(CAMPAIGN_TEST_TARGET).map \
		$(BUILD)/$(STRESS_TARGET).elf \
		$(BUILD)/$(STRESS_TARGET).gba \
		$(BUILD)/$(STRESS_TARGET).map \
		$(BUILD)/$(PLAYABLE_STRESS_TARGET).elf \
		$(BUILD)/$(PLAYABLE_STRESS_TARGET).gba \
		$(BUILD)/$(PLAYABLE_STRESS_TARGET).map \
		$(BUILD)/$(FRONTEND_CAPTURE_TARGET).elf \
		$(BUILD)/$(FRONTEND_CAPTURE_TARGET).gba \
		$(BUILD)/$(FRONTEND_CAPTURE_TARGET).map \
		$(BUILD)/$(FRONTEND_MENU_STRESS_TARGET).elf \
		$(BUILD)/$(FRONTEND_MENU_STRESS_TARGET).gba \
		$(BUILD)/$(FRONTEND_MENU_STRESS_TARGET).map \
		$(BUILD)/$(FRONTEND_NAV_STRESS_TARGET).elf \
		$(BUILD)/$(FRONTEND_NAV_STRESS_TARGET).gba \
		$(BUILD)/$(FRONTEND_NAV_STRESS_TARGET).map \
		$(BUILD)/$(FRONTEND_NAV_CAMERA_STRESS_TARGET).elf \
		$(BUILD)/$(FRONTEND_NAV_CAMERA_STRESS_TARGET).gba \
		$(BUILD)/$(FRONTEND_NAV_CAMERA_STRESS_TARGET).map \
		$(BUILD)/$(FRONTEND_TRANSITION_STRESS_TARGET).elf \
		$(BUILD)/$(FRONTEND_TRANSITION_STRESS_TARGET).gba \
		$(BUILD)/$(FRONTEND_TRANSITION_STRESS_TARGET).map

distclean: clean
	rm -f \
		$(ASSET_STAMP) $(RES)/soundbank.bin $(RES)/soundbank.h \
		$(VFS_OUTPUTS)

-include $(BUILD)/main_release_$(CONFIG_SUFFIX).d
-include $(BUILD)/main_test_$(CONFIG_SUFFIX).d
-include $(BUILD)/main_death_test_$(CONFIG_SUFFIX).d
-include $(BUILD)/main_jukebox_test_$(CONFIG_SUFFIX).d
-include $(BUILD)/main_demo_test_$(CONFIG_SUFFIX).d
-include $(BUILD)/main_frontend_menu_stress_$(CONFIG_SUFFIX).d
-include $(BUILD)/main_frontend_nav_stress_$(CONFIG_SUFFIX).d
-include $(BUILD)/main_frontend_nav_camera_stress_$(CONFIG_SUFFIX).d
-include $(BUILD)/main_frontend_transition_stress_$(CONFIG_SUFFIX).d
-include $(BUILD)/main_romfs_matrix_test_$(CONFIG_SUFFIX).d
-include $(BUILD)/main_route_test_ep$(ROUTE_EPISODE)_section$(ROUTE_SECTION)_$(CONFIG_SUFFIX).d
-include $(BUILD)/main_arcade_route_test_ep1_section1_$(CONFIG_SUFFIX).d
-include $(BUILD)/main_scripted_survival_test_$(CONFIG_SUFFIX).d
-include $(BUILD)/main_episode_wrap_test_$(CONFIG_SUFFIX).d
-include $(BUILD)/main_campaign_test_ep$(CAMPAIGN_EPISODE)_section$(CAMPAIGN_SECTION)_levels$(CAMPAIGN_LEVELS)_$(CONFIG_SUFFIX).d
-include $(BUILD)/main_full_loadout_stress_$(STRESS_DIAGNOSTIC)_$(CONFIG_SUFFIX).d
-include $(STRESS_LEVEL_OBJECT:.o=.d)
-include $(BUILD)/main_full_loadout_playable_$(CONFIG_SUFFIX).d
-include $(BUILD)/main_frontend_capture_state$(CAPTURE_STATE)$(FRONTEND_CAPTURE_VARIANT)_$(CONFIG_SUFFIX).d
-include $(BUILD)/gba_heap.d
-include $(BUILD)/opentyrian_data.d
-include $(BUILD)/opentyrian_sprite2.d
-include $(BUILD)/opentyrian_level_port.d
-include $(BUILD)/romfs.d
-include $(BUILD)/opentyrian_rom_io.d
