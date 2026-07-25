.SUFFIXES:

TARGET := tyrian_gba_level1_tech_demo_v9
TEST_TARGET := tyrian_gba_level1_autotest_v9
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
	-I$(LIBGBA)/include -I$(MAXMOD)/include
ASFLAGS := $(ARCH) -x assembler-with-cpp
LINKFLAGS := $(ARCH) -specs=gba.specs -Wl,--gc-sections \
	-L$(LIBGBA)/lib -L$(MAXMOD)/lib

ASSET_STAMP := $(RES)/assets.stamp
ASSET_INPUTS := \
	tools/build_assets.py \
	../../org/TyrianSnesPoc/tools/build_assets.py \
	../../org/TyrianNesPoc/tools/build_assets.py \
	../../org/AprCSTyrian/Build/data/tyrian1.lvl \
	../../org/AprCSTyrian/Build/data/tyrian.hdt \
	../../org/AprCSTyrian/Build/data/tyrian.snd \
	../../org/TyrianAudioLab/Music/30_tyrian_the_song.tym \
	../../org/TyrianAudioLab/Music/18_tyrian_the_level.tym

ASSET_BINARIES := \
	$(RES)/title_bitmap.bin \
	$(RES)/bg1_tiles.bin \
	$(RES)/bg2_tiles.bin \
	$(RES)/bg3_tiles.bin \
	$(RES)/bg_palette.bin \
	$(RES)/bg1_map.bin \
	$(RES)/bg2_map.bin \
	$(RES)/bg3_map.bin \
	$(RES)/obj_tiles.bin \
	$(RES)/obj_palette.bin \
	$(RES)/level_events.bin

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
	$(BUILD)/gba_heap.o

.PHONY: all autotest assets clean distclean

all: $(BUILD)/$(TARGET).gba

autotest: $(BUILD)/$(TEST_TARGET).gba

assets: $(RES)/soundbank.bin $(RES)/soundbank.h

$(BUILD) $(BUILD)/preview:
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

$(BUILD)/main_release.o: main.c $(RES)/asset_meta.h $(RES)/soundbank.h | $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/main_test.o: main.c $(RES)/asset_meta.h $(RES)/soundbank.h | $(BUILD)
	$(CC) $(CFLAGS) -DAUTOTEST -MMD -MP -c $< -o $@

$(BUILD)/gba_heap.o: gba_heap.c | $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/assets.o: assets.s $(ASSET_BINARIES) $(RES)/soundbank.bin | $(BUILD)
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
		$(BUILD)/assets.o \
		$(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).gba \
		$(BUILD)/$(TARGET).map \
		$(BUILD)/$(TEST_TARGET).elf $(BUILD)/$(TEST_TARGET).gba \
		$(BUILD)/$(TEST_TARGET).map

distclean: clean
	rm -f $(ASSET_STAMP) $(RES)/soundbank.bin $(RES)/soundbank.h

-include $(BUILD)/main_release.d
-include $(BUILD)/main_test.d
-include $(BUILD)/gba_heap.d
