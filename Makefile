.SUFFIXES:

TARGET := tyrian_gba_level1_pc_flow_mode4_romfs_v23
TEST_TARGET := tyrian_gba_level1_pc_flow_mode4_autotest_romfs_v23
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
	../../org/AprCSTyrian/Build/data/tyrian1.lvl \
	../../org/AprCSTyrian/Build/data/tyrian.hdt \
	../../org/AprCSTyrian/Build/data/tyrian.pic \
	../../org/AprCSTyrian/Build/data/tyrian.shp \
	../../org/AprCSTyrian/Build/data/palette.dat \
	../../org/AprCSTyrian/Build/data/tyrian.snd \
	$(wildcard ../../org/AprCSTyrian/image/sheets/10_powerups/*.png) \
	$(wildcard ../../org/AprCSTyrian/image/sheets/11_coins_cubes/*.png) \
	$(wildcard ../../org/AprCSTyrian/image/sheets_newsh/newsh_*/*.png) \
	../../org/opentyrian/src/tyrian2.c \
	../../org/opentyrian/src/varz.h \
	../../org/opentyrian/src/episodes.h \
	../../org/TyrianAudioLab/Music/30_tyrian_the_song.tym \
	../../org/TyrianAudioLab/Music/18_tyrian_the_level.tym

ASSET_BINARIES := \
	$(RES)/bg1_tiles.bin \
	$(RES)/bg2_tiles.bin \
	$(RES)/bg3_tiles.bin \
	$(RES)/bg_palette.bin \
	$(RES)/bg1_map.bin \
	$(RES)/bg2_map.bin \
	$(RES)/bg3_map.bin \
	$(RES)/obj_tiles.bin \
	$(RES)/obj_palette.bin \
	$(RES)/frontend_frames.bin \
	$(RES)/frontend_palettes.bin \
	$(RES)/frontend_glyphs.bin

AUDIO_INPUTS := \
	$(RES)/tyrian_title_full.it \
	$(RES)/tyrian_level_full.it \
	$(RES)/weapon_1.wav \
	$(RES)/enemy_hit.wav \
	$(RES)/explosion_9.wav \
	$(RES)/item.wav \
	$(RES)/enemy_shot_4.wav \
	$(RES)/enemy_shot_6.wav \
	$(RES)/enemy_shot_13.wav

COMMON_OBJECTS := \
	$(BUILD)/assets.o \
	$(BUILD)/gba_heap.o \
	$(BUILD)/opentyrian_data.o \
	$(BUILD)/opentyrian_sprite2.o \
	$(BUILD)/opentyrian_level_port.o \
	$(BUILD)/romfs.o \
	$(BUILD)/opentyrian_rom_io.o

MAIN_INCLUDES := $(wildcard src/*.inc)

.PHONY: all autotest assets clean distclean

all: $(BUILD)/$(TARGET).gba

autotest: $(BUILD)/$(TEST_TARGET).gba

assets: $(RES)/soundbank.bin $(RES)/soundbank.h $(VFS_OUTPUTS)

$(BUILD) $(BUILD)/preview $(RES):
	mkdir -p $@

$(ASSET_STAMP): $(ASSET_INPUTS) | $(BUILD)/preview
	$(PYTHON) tools/build_assets.py \
		--workspace "$(WORKSPACE)" \
		--output "$(CURDIR)/$(RES)" \
		--preview-dir "$(CURDIR)/$(BUILD)/preview"

$(ASSET_BINARIES) $(AUDIO_INPUTS) $(RES)/asset_meta.h: $(ASSET_STAMP)

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

$(BUILD)/main_release.o: main.c $(MAIN_INCLUDES) \
		src/opentyrian_data.h src/opentyrian_level_port.h \
		src/opentyrian_rom_io.h src/opentyrian_sprite2.h \
		$(RES)/asset_meta.h $(RES)/soundbank.h $(VFS_META) | $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/main_test.o: main.c $(MAIN_INCLUDES) \
		src/opentyrian_data.h src/opentyrian_level_port.h \
		src/opentyrian_rom_io.h src/opentyrian_sprite2.h \
		$(RES)/asset_meta.h $(RES)/soundbank.h $(VFS_META) | $(BUILD)
	$(CC) $(CFLAGS) -DAUTOTEST -MMD -MP -c $< -o $@

$(BUILD)/gba_heap.o: gba_heap.c | $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/opentyrian_data.o: src/opentyrian_data.c \
		src/opentyrian_data.h src/opentyrian_rom_io.h src/romfs.h | $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/opentyrian_level_port.o: src/opentyrian_level_port.c \
		src/opentyrian_level_port.h src/opentyrian_data.h | $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/opentyrian_sprite2.o: src/opentyrian_sprite2.c \
		src/opentyrian_sprite2.h src/opentyrian_data.h | $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/romfs.o: src/romfs.c src/romfs.h | $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/opentyrian_rom_io.o: src/opentyrian_rom_io.c \
		src/opentyrian_rom_io.h src/romfs.h $(VFS_META) | $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/assets.o: assets.s $(ASSET_BINARIES) \
		$(RES)/soundbank.bin $(VFS_IMAGE) | $(BUILD)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD)/$(TARGET).elf: $(BUILD)/main_release.o $(COMMON_OBJECTS)
	$(CC) $(LINKFLAGS) -Wl,-Map,$(BUILD)/$(TARGET).map $^ \
		-lmm -lgba -o $@
	$(SIZE) $@

$(BUILD)/$(TEST_TARGET).elf: $(BUILD)/main_test.o $(COMMON_OBJECTS)
	$(CC) $(LINKFLAGS) -Wl,-Map,$(BUILD)/$(TEST_TARGET).map $^ \
		-lmm -lgba -o $@
	$(SIZE) $@

$(BUILD)/$(TARGET).gba: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@
	$(TOOLS)/gbafix $@ "-tTYRIAN GBA" -cTYGA -m00

$(BUILD)/$(TEST_TARGET).gba: $(BUILD)/$(TEST_TARGET).elf
	$(OBJCOPY) -O binary $< $@
	$(TOOLS)/gbafix $@ "-tTYRIAN TEST" -cTYGT -m00

clean:
	rm -f \
		$(BUILD)/main_release.o $(BUILD)/main_release.d \
		$(BUILD)/main_test.o $(BUILD)/main_test.d \
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
		$(BUILD)/$(TEST_TARGET).map

distclean: clean
	rm -f \
		$(ASSET_STAMP) $(RES)/soundbank.bin $(RES)/soundbank.h \
		$(VFS_OUTPUTS)

-include $(BUILD)/main_release.d
-include $(BUILD)/main_test.d
-include $(BUILD)/gba_heap.d
-include $(BUILD)/opentyrian_data.d
-include $(BUILD)/opentyrian_sprite2.d
-include $(BUILD)/opentyrian_level_port.d
-include $(BUILD)/romfs.d
-include $(BUILD)/opentyrian_rom_io.d
