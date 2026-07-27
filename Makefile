.SUFFIXES:

DETAIL_LEVEL ?= low
GAME_SPEED ?= normal
ROUTE_EPISODE ?= 1
ROUTE_SECTION ?= 5
CAMPAIGN_EPISODE ?= 1
CAMPAIGN_SECTION ?= 1
CAMPAIGN_LEVELS ?= 4

ifeq ($(DETAIL_LEVEL),low)
DETAIL_LEVEL_VALUE := 0
else ifeq ($(DETAIL_LEVEL),normal)
DETAIL_LEVEL_VALUE := 1
else
$(error DETAIL_LEVEL must be low or normal)
endif

ifeq ($(GAME_SPEED),low)
GAME_SPEED_VALUE := 0
else ifeq ($(GAME_SPEED),normal)
GAME_SPEED_VALUE := 1
else
$(error GAME_SPEED must be low or normal)
endif

CONFIG_SUFFIX := detail_$(DETAIL_LEVEL)_speed_$(GAME_SPEED)
TARGET := tyrian_gba_level1_pc_flow_mode4_romfs_v31_$(CONFIG_SUFFIX)
TEST_TARGET := tyrian_gba_level1_pc_flow_mode4_autotest_romfs_v31_$(CONFIG_SUFFIX)
DEATH_TEST_TARGET := tyrian_gba_level1_pc_flow_mode4_death_autotest_romfs_v31_$(CONFIG_SUFFIX)
JUKEBOX_TEST_TARGET := tyrian_gba_jukebox_autotest_romfs_v31_$(CONFIG_SUFFIX)
ROMFS_MATRIX_TEST_TARGET := tyrian_gba_romfs_all_levels_matrix_v31_$(CONFIG_SUFFIX)
ROUTE_TEST_TARGET := tyrian_gba_route_smoke_ep$(ROUTE_EPISODE)_section$(ROUTE_SECTION)_v31_$(CONFIG_SUFFIX)
CAMPAIGN_TEST_TARGET := tyrian_gba_campaign_smoke_ep$(CAMPAIGN_EPISODE)_section$(CAMPAIGN_SECTION)_levels$(CAMPAIGN_LEVELS)_v31_$(CONFIG_SUFFIX)
BUILD := build
RES := res

WORKSPACE ?= /c/ai_project/AprTyrianNes
SDK_ROOT ?= $(WORKSPACE)/tools/gba-sdk
LIBGBA := $(SDK_ROOT)/libgba
MAXMOD := $(SDK_ROOT)/maxmod
TOOLS := $(SDK_ROOT)/tools/bin

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
	-DTYRIAN_GBA_DETAIL_LEVEL=$(DETAIL_LEVEL_VALUE) \
	-DTYRIAN_GBA_GAME_SPEED=$(GAME_SPEED_VALUE)
ASFLAGS := $(ARCH) -x assembler-with-cpp
LINKFLAGS := $(ARCH) -specs=gba.specs -Wl,--gc-sections \
	-L$(LIBGBA)/lib -L$(MAXMOD)/lib

ASSET_STAMP := $(RES)/assets.stamp
VFS_SOURCE_ROOT := ../../org/AprCSTyrian/Build/data
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
	tools/build_assets.py \
	../../org/TyrianSnesPoc/tools/build_assets.py \
	../../org/TyrianNesPoc/tools/build_assets.py \
	../../org/AprCSTyrian/Build/data/tyrian.hdt \
	../../org/AprCSTyrian/Build/data/tyrian.pic \
	../../org/AprCSTyrian/Build/data/tyrian.shp \
	$(wildcard ../../org/AprCSTyrian/Build/data/newsh*.shp) \
	../../org/AprCSTyrian/Build/data/palette.dat \
	../../org/AprCSTyrian/Build/data/tyrian.snd \
	../../org/AprCSTyrian/Build/data/voices.snd \
	$(wildcard ../../org/AprCSTyrian/image/sheets/10_powerups/*.png) \
	$(wildcard ../../org/AprCSTyrian/image/sheets/11_coins_cubes/*.png) \
	$(wildcard ../../org/AprCSTyrian/image/sheets_newsh/newsh_*/*.png) \
	../../org/opentyrian/src/tyrian2.c \
	../../org/opentyrian/src/varz.h \
	../../org/opentyrian/src/episodes.h \
	../../org/opentyrian/src/jukebox.c \
	../../org/opentyrian/src/starlib.c \
	../../org/opentyrian/src/musmast.c \
	../../org/TyrianAudioLab/Music/channel-calibration.json \
	$(wildcard ../../org/TyrianAudioLab/Music/*.tym)

ASSET_BINARIES := \
	$(RES)/obj_tiles.bin \
	$(RES)/obj_palette.bin \
	$(RES)/frontend_frames.bin \
	$(RES)/frontend_palettes.bin \
	$(RES)/frontend_glyphs.bin \
	$(RES)/frontend_cube.bin \
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

AUDIO_INPUTS := \
	$(TYRIAN_MUSIC_INPUTS) \
	$(RES)/weapon_1.wav \
	$(RES)/enemy_hit.wav \
	$(RES)/explosion_9.wav \
	$(RES)/explosion_11.wav \
	$(RES)/explosion_22.wav \
	$(RES)/item.wav \
	$(RES)/enemy_shot_4.wav \
	$(RES)/enemy_shot_6.wav \
	$(RES)/enemy_shot_13.wav \
	$(RES)/level_complete.wav

COMMON_OBJECTS := \
	$(BUILD)/assets.o \
	$(BUILD)/gba_heap.o \
	$(BUILD)/opentyrian_data.o \
	$(BUILD)/opentyrian_sprite2.o \
	$(BUILD)/opentyrian_level_port.o \
	$(BUILD)/romfs.o \
	$(BUILD)/opentyrian_rom_io.o

MAIN_INCLUDES := $(wildcard src/*.inc)

.PHONY: all autotest death-autotest jukebox-autotest \
	romfs-matrix-autotest route-smoke-autotest campaign-smoke-autotest \
	assets clean distclean

all: $(BUILD)/$(TARGET).gba

autotest: $(BUILD)/$(TEST_TARGET).gba

death-autotest: $(BUILD)/$(DEATH_TEST_TARGET).gba

jukebox-autotest: $(BUILD)/$(JUKEBOX_TEST_TARGET).gba

romfs-matrix-autotest: $(BUILD)/$(ROMFS_MATRIX_TEST_TARGET).gba

route-smoke-autotest: $(BUILD)/$(ROUTE_TEST_TARGET).gba

campaign-smoke-autotest: $(BUILD)/$(CAMPAIGN_TEST_TARGET).gba

assets: $(RES)/soundbank.bin $(RES)/soundbank.h $(VFS_OUTPUTS)

$(BUILD) $(BUILD)/preview $(RES):
	mkdir -p $@

$(ASSET_STAMP): $(ASSET_INPUTS) | $(BUILD)/preview
	$(PYTHON) tools/build_assets.py \
		--workspace "$(WORKSPACE)" \
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
		$(RES)/soundbank.h $(VFS_META) | $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/main_test_$(CONFIG_SUFFIX).o: main.c $(MAIN_INCLUDES) \
		src/opentyrian_data.h src/opentyrian_level_port.h \
		src/opentyrian_rom_io.h src/opentyrian_sprite2.h src/port_config.h \
		$(RES)/asset_meta.h $(RES)/sprite2_raw_meta.h \
		$(RES)/soundbank.h $(VFS_META) | $(BUILD)
	$(CC) $(CFLAGS) -DAUTOTEST -MMD -MP -c $< -o $@

$(BUILD)/main_death_test_$(CONFIG_SUFFIX).o: main.c $(MAIN_INCLUDES) \
		src/opentyrian_data.h src/opentyrian_level_port.h \
		src/opentyrian_rom_io.h src/opentyrian_sprite2.h src/port_config.h \
		$(RES)/asset_meta.h $(RES)/sprite2_raw_meta.h \
		$(RES)/soundbank.h $(VFS_META) | $(BUILD)
	$(CC) $(CFLAGS) -DAUTOTEST -DAUTOTEST_FORCE_PLAYER_DEATH \
		-DAUTOTEST_DEATH_FLOW \
		-DTYRIAN_GBA_DEV_PLAYER_INVINCIBLE=0 \
		-MMD -MP -c $< -o $@

$(BUILD)/main_jukebox_test_$(CONFIG_SUFFIX).o: main.c $(MAIN_INCLUDES) \
		src/opentyrian_data.h src/opentyrian_level_port.h \
		src/opentyrian_rom_io.h src/opentyrian_sprite2.h src/port_config.h \
		$(RES)/asset_meta.h $(RES)/sprite2_raw_meta.h \
		$(RES)/soundbank.h $(VFS_META) | $(BUILD)
	$(CC) $(CFLAGS) -DAUTOTEST -DAUTOTEST_JUKEBOX_FLOW \
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
		-MMD -MP -c $< -o $@

$(BUILD)/gba_heap.o: gba_heap.c | $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/opentyrian_data.o: src/opentyrian_data.c \
		src/opentyrian_data.h src/opentyrian_rom_io.h src/romfs.h | $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/opentyrian_level_port.o: src/opentyrian_level_port.c \
		src/opentyrian_level_port.h src/opentyrian_data.h | $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

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

$(BUILD)/$(CAMPAIGN_TEST_TARGET).elf: \
		$(BUILD)/main_campaign_test_ep$(CAMPAIGN_EPISODE)_section$(CAMPAIGN_SECTION)_levels$(CAMPAIGN_LEVELS)_$(CONFIG_SUFFIX).o \
		$(COMMON_OBJECTS)
	$(CC) $(LINKFLAGS) -Wl,-Map,$(BUILD)/$(CAMPAIGN_TEST_TARGET).map $^ \
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

$(BUILD)/$(ROMFS_MATRIX_TEST_TARGET).gba: \
		$(BUILD)/$(ROMFS_MATRIX_TEST_TARGET).elf
	$(OBJCOPY) -O binary $< $@
	$(TOOLS)/gbafix $@ "-tTYRIAN MATRIX" -cTYGM -m00

$(BUILD)/$(ROUTE_TEST_TARGET).gba: $(BUILD)/$(ROUTE_TEST_TARGET).elf
	$(OBJCOPY) -O binary $< $@
	$(TOOLS)/gbafix $@ "-tTYRIAN ROUTE" -cTYGR -m00

$(BUILD)/$(CAMPAIGN_TEST_TARGET).gba: $(BUILD)/$(CAMPAIGN_TEST_TARGET).elf
	$(OBJCOPY) -O binary $< $@
	$(TOOLS)/gbafix $@ "-tTYRIAN CAMP" -cTYGC -m00

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
		$(BUILD)/main_romfs_matrix_test_$(CONFIG_SUFFIX).o \
		$(BUILD)/main_romfs_matrix_test_$(CONFIG_SUFFIX).d \
		$(BUILD)/main_route_test_ep$(ROUTE_EPISODE)_section$(ROUTE_SECTION)_$(CONFIG_SUFFIX).o \
		$(BUILD)/main_route_test_ep$(ROUTE_EPISODE)_section$(ROUTE_SECTION)_$(CONFIG_SUFFIX).d \
		$(BUILD)/main_campaign_test_ep$(CAMPAIGN_EPISODE)_section$(CAMPAIGN_SECTION)_levels$(CAMPAIGN_LEVELS)_$(CONFIG_SUFFIX).o \
		$(BUILD)/main_campaign_test_ep$(CAMPAIGN_EPISODE)_section$(CAMPAIGN_SECTION)_levels$(CAMPAIGN_LEVELS)_$(CONFIG_SUFFIX).d \
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
		$(BUILD)/$(ROMFS_MATRIX_TEST_TARGET).elf \
		$(BUILD)/$(ROMFS_MATRIX_TEST_TARGET).gba \
		$(BUILD)/$(ROMFS_MATRIX_TEST_TARGET).map \
		$(BUILD)/$(ROUTE_TEST_TARGET).elf \
		$(BUILD)/$(ROUTE_TEST_TARGET).gba \
		$(BUILD)/$(ROUTE_TEST_TARGET).map \
		$(BUILD)/$(CAMPAIGN_TEST_TARGET).elf \
		$(BUILD)/$(CAMPAIGN_TEST_TARGET).gba \
		$(BUILD)/$(CAMPAIGN_TEST_TARGET).map

distclean: clean
	rm -f \
		$(ASSET_STAMP) $(RES)/soundbank.bin $(RES)/soundbank.h \
		$(VFS_OUTPUTS)

-include $(BUILD)/main_release_$(CONFIG_SUFFIX).d
-include $(BUILD)/main_test_$(CONFIG_SUFFIX).d
-include $(BUILD)/main_death_test_$(CONFIG_SUFFIX).d
-include $(BUILD)/main_jukebox_test_$(CONFIG_SUFFIX).d
-include $(BUILD)/main_romfs_matrix_test_$(CONFIG_SUFFIX).d
-include $(BUILD)/main_route_test_ep$(ROUTE_EPISODE)_section$(ROUTE_SECTION)_$(CONFIG_SUFFIX).d
-include $(BUILD)/main_campaign_test_ep$(CAMPAIGN_EPISODE)_section$(CAMPAIGN_SECTION)_levels$(CAMPAIGN_LEVELS)_$(CONFIG_SUFFIX).d
-include $(BUILD)/gba_heap.d
-include $(BUILD)/opentyrian_data.d
-include $(BUILD)/opentyrian_sprite2.d
-include $(BUILD)/opentyrian_level_port.d
-include $(BUILD)/romfs.d
-include $(BUILD)/opentyrian_rom_io.d
