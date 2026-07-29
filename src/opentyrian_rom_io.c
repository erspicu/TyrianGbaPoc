/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "opentyrian_rom_io.h"

#include <string.h>

#include "res/tyrian_romfs_meta.h"

typedef struct {
    const char *path;
    uint32_t size;
    uint32_t crc32;
    uint32_t head_u32;
    uint32_t tail_u32;
} OtRomFsProbe;

static OtRomFs filesystem;
static OtFile handles[OT_ROM_IO_MAX_OPEN_FILES];
static bool initialized;

#define OT_ROMFS_PROBE_ENTRY(path_, size_, crc_, head_, tail_) \
    {(path_), (size_), (crc_), (head_), (tail_)},
static const OtRomFsProbe probes[] = {
    TYRIAN_ROMFS_PROBE_LIST(OT_ROMFS_PROBE_ENTRY)
};
#undef OT_ROMFS_PROBE_ENTRY

_Static_assert(
    sizeof(probes) / sizeof(probes[0]) == TYRIAN_ROMFS_PROBE_COUNT,
    "generated ROMFS probe count changed"
);
_Static_assert(TYRIAN_ROMFS_PROBE_COUNT > 0, "ROMFS needs a probe file");

static bool read_mode_is_supported(const char *mode)
{
    return mode != 0 &&
           (
               strcmp(mode, "r") == 0 ||
               strcmp(mode, "rb") == 0 ||
               strcmp(mode, "rt") == 0
           );
}

bool ot_rom_io_init(void)
{
    uint8_t index;

    for (index = 0; index < OT_ROM_IO_MAX_OPEN_FILES; index++) {
        handles[index] = (OtFile){0};
    }
    initialized =
        ot_romfs_mount_embedded(&filesystem) == OT_ROMFS_OK;
    return initialized;
}

const OtRomFs *ot_rom_io_filesystem(void)
{
    return initialized ? &filesystem : 0;
}

const char *ot_data_dir(void)
{
    return "data";
}

OtFile *ot_fopen(const char *path, const char *mode)
{
    uint8_t index;

    if (!initialized && !ot_rom_io_init()) return 0;
    if (!read_mode_is_supported(mode)) return 0;
    for (index = 0; index < OT_ROM_IO_MAX_OPEN_FILES; index++) {
        if (!handles[index].open) {
            if (
                ot_romfs_open(&filesystem, &handles[index], path) ==
                OT_ROMFS_OK
            ) {
                return &handles[index];
            }
            return 0;
        }
    }
    return 0;
}

OtFile *ot_dir_fopen(
    const char *directory,
    const char *file,
    const char *mode
)
{
    char path[OT_ROMFS_MAX_PATH];
    size_t directory_length;
    size_t file_length;

    if (directory == 0 || file == 0) return 0;
    directory_length = strlen(directory);
    file_length = strlen(file);
    if (
        directory_length + (directory_length != 0 ? 1u : 0u) +
        file_length + 1 > sizeof(path)
    ) {
        return 0;
    }

    memcpy(path, directory, directory_length);
    if (directory_length != 0) path[directory_length++] = '/';
    memcpy(path + directory_length, file, file_length + 1);
    return ot_fopen(path, mode);
}

size_t ot_fread(
    void *destination,
    size_t element_size,
    size_t element_count,
    OtFile *file
)
{
    return ot_romfs_read(
        destination,
        element_size,
        element_count,
        file
    );
}

bool ot_fread_exact(
    void *destination,
    size_t element_size,
    size_t element_count,
    OtFile *file
)
{
    return ot_fread(
        destination,
        element_size,
        element_count,
        file
    ) == element_count;
}

bool ot_fread_u8_exact(uint8_t *destination, size_t count, OtFile *file)
{
    return ot_fread_exact(destination, 1, count, file);
}

bool ot_fread_s8_exact(int8_t *destination, size_t count, OtFile *file)
{
    return ot_fread_exact(destination, 1, count, file);
}

bool ot_fread_u16le_exact(
    uint16_t *destination,
    size_t count,
    OtFile *file
)
{
    size_t index;

    for (index = 0; index < count; index++) {
        uint8_t bytes[2];
        if (!ot_fread_exact(bytes, 1, 2, file)) return false;
        destination[index] =
            (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
    }
    return true;
}

bool ot_fread_s16le_exact(
    int16_t *destination,
    size_t count,
    OtFile *file
)
{
    size_t index;

    for (index = 0; index < count; index++) {
        uint16_t value;
        if (!ot_fread_u16le_exact(&value, 1, file)) return false;
        destination[index] = (int16_t)value;
    }
    return true;
}

bool ot_fread_u32le_exact(
    uint32_t *destination,
    size_t count,
    OtFile *file
)
{
    size_t index;

    for (index = 0; index < count; index++) {
        uint8_t bytes[4];
        if (!ot_fread_exact(bytes, 1, 4, file)) return false;
        destination[index] =
            (uint32_t)bytes[0] |
            ((uint32_t)bytes[1] << 8) |
            ((uint32_t)bytes[2] << 16) |
            ((uint32_t)bytes[3] << 24);
    }
    return true;
}

bool ot_fread_s32le_exact(
    int32_t *destination,
    size_t count,
    OtFile *file
)
{
    size_t index;

    for (index = 0; index < count; index++) {
        uint32_t value;
        if (!ot_fread_u32le_exact(&value, 1, file)) return false;
        destination[index] = (int32_t)value;
    }
    return true;
}

int ot_fseek(OtFile *file, int32_t offset, int origin)
{
    return ot_romfs_seek(file, offset, origin);
}

int32_t ot_ftell(OtFile *file)
{
    return ot_romfs_tell(file);
}

int32_t ot_ftell_eof(OtFile *file)
{
    int32_t original = ot_ftell(file);
    int32_t size;

    if (original < 0) return -1;
    if (ot_fseek(file, 0, OT_ROMFS_SEEK_END) != 0) return -1;
    size = ot_ftell(file);
    if (ot_fseek(file, original, OT_ROMFS_SEEK_SET) != 0) return -1;
    return size;
}

int ot_fclose(OtFile *file)
{
    return ot_romfs_close(file);
}

int ot_fgetc(OtFile *file)
{
    return ot_romfs_getc(file);
}

bool ot_feof(const OtFile *file)
{
    return ot_romfs_eof(file);
}

bool ot_ferror(const OtFile *file)
{
    return ot_romfs_error(file);
}

bool ot_file_exists(const char *directory, const char *file)
{
    OtFile *handle = ot_dir_fopen(directory, file, "rb");

    if (handle == 0) return false;
    ot_fclose(handle);
    return true;
}

static uint32_t little_u32(const uint8_t bytes[4])
{
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

static bool make_case_slash_variant(
    const char *path,
    char variant[OT_ROMFS_MAX_PATH]
)
{
    size_t index;
    size_t length = strlen(path);

    if (length + 3 > OT_ROMFS_MAX_PATH) return false;
    variant[0] = '.';
    variant[1] = '\\';
    for (index = 0; index < length; index++) {
        char character = path[index];

        if (character == '/') {
            character = '\\';
        } else if (character >= 'a' && character <= 'z') {
            character = (char)(character - ('a' - 'A'));
        }
        variant[index + 2] = character;
    }
    variant[length + 2] = 0;
    return true;
}

static void record_check(
    bool condition,
    uint32_t *passed,
    uint32_t *failed
)
{
    if (condition) {
        (*passed)++;
    } else {
        (*failed)++;
    }
}

bool ot_rom_io_self_test(uint32_t *passed_checks, uint32_t *failed_checks)
{
    uint32_t passed = 0;
    uint32_t failed = 0;
    uint32_t probe_index;
    uint8_t handle_index;
    OtRomFsStat stat;
    OtFile *handle;
    OtFile *open_handles[OT_ROM_IO_MAX_OPEN_FILES] = {0};
    char alternate_path[OT_ROMFS_MAX_PATH] = {0};

    record_check(ot_rom_io_init(), &passed, &failed);
    record_check(
        filesystem.entry_count == TYRIAN_ROMFS_ENTRY_COUNT,
        &passed,
        &failed
    );
    record_check(
        filesystem.image_size == TYRIAN_ROMFS_IMAGE_BYTES,
        &passed,
        &failed
    );
    record_check(
        filesystem.payload_bytes == TYRIAN_ROMFS_PAYLOAD_BYTES,
        &passed,
        &failed
    );
    record_check(
        filesystem.manifest_crc32 == TYRIAN_ROMFS_MANIFEST_CRC32,
        &passed,
        &failed
    );

    for (probe_index = 0; probe_index < TYRIAN_ROMFS_PROBE_COUNT; probe_index++) {
        const OtRomFsProbe *probe = &probes[probe_index];
        uint8_t bytes[4] = {0};

        record_check(
            ot_romfs_stat(&filesystem, probe->path, &stat) == OT_ROMFS_OK,
            &passed,
            &failed
        );
        record_check(stat.size == probe->size, &passed, &failed);
        record_check(stat.crc32 == probe->crc32, &passed, &failed);

        handle = ot_fopen(probe->path, "rb");
        record_check(handle != 0, &passed, &failed);
        if (handle == 0) continue;
        record_check(
            ot_ftell_eof(handle) == (int32_t)probe->size,
            &passed,
            &failed
        );
        record_check(
            ot_fread(bytes, 1, sizeof(bytes), handle) == sizeof(bytes),
            &passed,
            &failed
        );
        record_check(
            little_u32(bytes) == probe->head_u32,
            &passed,
            &failed
        );
        record_check(
            ot_fseek(handle, -4, OT_ROMFS_SEEK_END) == 0,
            &passed,
            &failed
        );
        record_check(
            ot_fread(bytes, 1, sizeof(bytes), handle) == sizeof(bytes),
            &passed,
            &failed
        );
        record_check(
            little_u32(bytes) == probe->tail_u32,
            &passed,
            &failed
        );
        record_check(ot_fclose(handle) == 0, &passed, &failed);
    }

    make_case_slash_variant(probes[0].path, alternate_path);
    handle = ot_fopen(alternate_path, "RB");
    /*
     * Uppercase mode is intentionally unsupported; path normalization is
     * tested separately with the normal binary-read mode.
     */
    record_check(handle == 0, &passed, &failed);
    handle = ot_fopen(alternate_path, "rb");
    record_check(handle != 0, &passed, &failed);
    if (handle != 0) {
        uint16_t head;
        uint8_t tail[3];

        record_check(
            ot_fread_u16le_exact(&head, 1, handle) &&
            head == (uint16_t)probes[0].head_u32,
            &passed,
            &failed
        );
        record_check(
            ot_fseek(handle, -3, OT_ROMFS_SEEK_END) == 0,
            &passed,
            &failed
        );
        record_check(
            ot_fread(tail, 2, 2, handle) == 1,
            &passed,
            &failed
        );
        record_check(ot_feof(handle), &passed, &failed);
        record_check(
            ot_ftell(handle) == (int32_t)probes[0].size,
            &passed,
            &failed
        );
        record_check(
            ot_fseek(handle, 0, OT_ROMFS_SEEK_SET) == 0 &&
            !ot_feof(handle),
            &passed,
            &failed
        );
        record_check(ot_fclose(handle) == 0, &passed, &failed);
    }

    handle = ot_dir_fopen("DATA", "TYRIAN.HDT", "rb");
    record_check(handle != 0, &passed, &failed);
    if (handle != 0) record_check(ot_fclose(handle) == 0, &passed, &failed);
    record_check(
        ot_file_exists(ot_data_dir(), "music.mus"),
        &passed,
        &failed
    );
    record_check(
        !ot_file_exists(ot_data_dir(), "missing.file"),
        &passed,
        &failed
    );
    record_check(
        ot_fopen(probes[0].path, "wb") == 0,
        &passed,
        &failed
    );

    for (
        handle_index = 0;
        handle_index < OT_ROM_IO_MAX_OPEN_FILES;
        handle_index++
    ) {
        open_handles[handle_index] =
            ot_fopen(probes[0].path, "rb");
        record_check(
            open_handles[handle_index] != 0,
            &passed,
            &failed
        );
    }
    record_check(
        ot_fopen(probes[0].path, "rb") == 0,
        &passed,
        &failed
    );
    for (
        handle_index = 0;
        handle_index < OT_ROM_IO_MAX_OPEN_FILES;
        handle_index++
    ) {
        record_check(
            open_handles[handle_index] != 0 &&
            ot_fclose(open_handles[handle_index]) == 0,
            &passed,
            &failed
        );
    }
    handle = ot_fopen(probes[0].path, "rb");
    record_check(handle != 0, &passed, &failed);
    if (handle != 0) {
        record_check(ot_fclose(handle) == 0, &passed, &failed);
    }

    if (passed_checks != 0) *passed_checks = passed;
    if (failed_checks != 0) *failed_checks = failed;
    return failed == 0;
}
