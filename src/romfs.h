/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Versioned, memory-mapped, read-only filesystem for GBA cartridge ROM.
 */
#ifndef TYRIAN_GBA_ROMFS_H
#define TYRIAN_GBA_ROMFS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    OT_ROMFS_FORMAT_VERSION = 1,
    OT_ROMFS_HEADER_BYTES = 64,
    OT_ROMFS_ENTRY_BYTES = 32,
    OT_ROMFS_FEATURE_ASCII_CASEFOLD = 1,
    OT_ROMFS_MAX_PATH = 128,
    OT_ROMFS_SEEK_SET = 0,
    OT_ROMFS_SEEK_CUR = 1,
    OT_ROMFS_SEEK_END = 2,
};

typedef enum {
    OT_ROMFS_OK = 0,
    OT_ROMFS_BAD_ARGUMENT,
    OT_ROMFS_BAD_IMAGE,
    OT_ROMFS_UNSUPPORTED,
    OT_ROMFS_INVALID_PATH,
    OT_ROMFS_NOT_FOUND,
    OT_ROMFS_READ_ONLY,
    OT_ROMFS_TOO_MANY_OPEN_FILES,
    OT_ROMFS_IO_ERROR,
} OtRomFsResult;

typedef struct {
    const uint8_t *image;
    uint32_t image_size;
    uint32_t entry_count;
    uint32_t index_offset;
    uint32_t strings_offset;
    uint32_t data_offset;
    uint32_t payload_bytes;
    uint32_t path_bytes;
    uint32_t manifest_crc32;
    uint32_t metadata_crc32;
    uint32_t payload_crc32;
    bool mounted;
} OtRomFs;

typedef struct {
    const uint8_t *data;
    const char *path;
    uint32_t size;
    uint32_t crc32;
    uint16_t flags;
} OtRomFsStat;

typedef struct {
    const uint8_t *data;
    uint32_t size;
    uint32_t position;
    uint32_t crc32;
    uint16_t flags;
    bool open;
    bool eof;
    bool error;
} OtRomFsFile;

OtRomFsResult ot_romfs_mount(
    OtRomFs *fs,
    const void *image,
    uint32_t available_bytes
);
OtRomFsResult ot_romfs_mount_embedded(OtRomFs *fs);
OtRomFsResult ot_romfs_stat(
    const OtRomFs *fs,
    const char *path,
    OtRomFsStat *stat
);
OtRomFsResult ot_romfs_open(
    const OtRomFs *fs,
    OtRomFsFile *file,
    const char *path
);
size_t ot_romfs_read(
    void *destination,
    size_t element_size,
    size_t element_count,
    OtRomFsFile *file
);
int ot_romfs_seek(OtRomFsFile *file, int32_t offset, int origin);
int32_t ot_romfs_tell(OtRomFsFile *file);
int ot_romfs_close(OtRomFsFile *file);
int ot_romfs_getc(OtRomFsFile *file);
bool ot_romfs_eof(const OtRomFsFile *file);
bool ot_romfs_error(const OtRomFsFile *file);
const uint8_t *ot_romfs_direct_data(
    const OtRomFsFile *file,
    uint32_t *remaining_bytes
);
uint32_t ot_romfs_crc32(const void *data, uint32_t size);
bool ot_romfs_verify_file(const OtRomFsFile *file);
const char *ot_romfs_result_string(OtRomFsResult result);

#endif
