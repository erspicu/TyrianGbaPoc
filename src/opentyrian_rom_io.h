/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * OpenTyrian-facing stdio-style adapter over the cartridge ROMFS.
 */
#ifndef TYRIAN_GBA_OPENTYRIAN_ROM_IO_H
#define TYRIAN_GBA_OPENTYRIAN_ROM_IO_H

#include "romfs.h"

enum {
    OT_ROM_IO_MAX_OPEN_FILES = 8,
    OT_ROM_IO_SELF_TEST_FIXED_CHECKS = 38,
};

typedef OtRomFsFile OtFile;

bool ot_rom_io_init(void);
bool ot_rom_io_self_test(uint32_t *passed_checks, uint32_t *failed_checks);
const OtRomFs *ot_rom_io_filesystem(void);
const char *ot_data_dir(void);

OtFile *ot_fopen(const char *path, const char *mode);
OtFile *ot_dir_fopen(const char *directory, const char *file, const char *mode);
size_t ot_fread(
    void *destination,
    size_t element_size,
    size_t element_count,
    OtFile *file
);
bool ot_fread_exact(
    void *destination,
    size_t element_size,
    size_t element_count,
    OtFile *file
);
bool ot_fread_u8_exact(uint8_t *destination, size_t count, OtFile *file);
bool ot_fread_s8_exact(int8_t *destination, size_t count, OtFile *file);
bool ot_fread_u16le_exact(uint16_t *destination, size_t count, OtFile *file);
bool ot_fread_s16le_exact(int16_t *destination, size_t count, OtFile *file);
bool ot_fread_u32le_exact(uint32_t *destination, size_t count, OtFile *file);
bool ot_fread_s32le_exact(int32_t *destination, size_t count, OtFile *file);
int ot_fseek(OtFile *file, int32_t offset, int origin);
int32_t ot_ftell(OtFile *file);
int32_t ot_ftell_eof(OtFile *file);
int ot_fclose(OtFile *file);
int ot_fgetc(OtFile *file);
bool ot_feof(const OtFile *file);
bool ot_ferror(const OtFile *file);
bool ot_file_exists(const char *directory, const char *file);

#endif
