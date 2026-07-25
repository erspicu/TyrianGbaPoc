/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "romfs.h"

#include <limits.h>
#include <string.h>

extern const uint8_t tyrian_romfs[];
extern const uint8_t tyrian_romfs_end[];

enum {
    HEADER_VERSION = 8,
    HEADER_HEADER_SIZE = 10,
    HEADER_ENTRY_SIZE = 12,
    HEADER_FEATURES = 14,
    HEADER_ENTRY_COUNT = 16,
    HEADER_INDEX_OFFSET = 20,
    HEADER_STRINGS_OFFSET = 24,
    HEADER_DATA_OFFSET = 28,
    HEADER_IMAGE_SIZE = 32,
    HEADER_PAYLOAD_BYTES = 36,
    HEADER_PATH_BYTES = 40,
    HEADER_MANIFEST_CRC32 = 44,
    HEADER_METADATA_CRC32 = 48,
    HEADER_PAYLOAD_CRC32 = 52,
    HEADER_HEADER_CRC32 = 56,

    ENTRY_PATH_HASH = 0,
    ENTRY_PATH_OFFSET = 4,
    ENTRY_DATA_OFFSET = 8,
    ENTRY_STORED_SIZE = 12,
    ENTRY_LOGICAL_SIZE = 16,
    ENTRY_CRC32 = 20,
    ENTRY_FLAGS = 24,
    ENTRY_ALIGNMENT = 26,
};

static const uint8_t expected_magic[8] = {
    'T', 'Y', 'R', 'V', 'F', 'S', '1', 0
};

static uint16_t read_u16(const uint8_t *source)
{
    return (uint16_t)source[0] | ((uint16_t)source[1] << 8);
}

static uint32_t read_u32(const uint8_t *source)
{
    return (uint32_t)source[0] |
           ((uint32_t)source[1] << 8) |
           ((uint32_t)source[2] << 16) |
           ((uint32_t)source[3] << 24);
}

static uint32_t crc32_byte(uint32_t crc, uint8_t byte)
{
    uint8_t bit;

    crc ^= byte;
    for (bit = 0; bit < 8; bit++) {
        uint32_t mask = 0u - (crc & 1u);
        crc = (crc >> 1) ^ (0xedb88320u & mask);
    }
    return crc;
}

uint32_t ot_romfs_crc32(const void *data, uint32_t size)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t crc = 0xffffffffu;
    uint32_t index;

    if (data == 0 && size != 0) return 0;
    for (index = 0; index < size; index++) {
        crc = crc32_byte(crc, bytes[index]);
    }
    return ~crc;
}

static uint32_t header_crc32(const uint8_t *header)
{
    uint32_t crc = 0xffffffffu;
    uint8_t index;

    for (index = 0; index < OT_ROMFS_HEADER_BYTES; index++) {
        uint8_t byte = header[index];
        if (
            index >= HEADER_HEADER_CRC32 &&
            index < HEADER_HEADER_CRC32 + 4
        ) {
            byte = 0;
        }
        crc = crc32_byte(crc, byte);
    }
    return ~crc;
}

static bool power_of_two(uint16_t value)
{
    return value != 0 && (value & (value - 1)) == 0;
}

static OtRomFsResult normalize_path(
    const char *path,
    char normalized[OT_ROMFS_MAX_PATH]
)
{
    const char *cursor;
    uint16_t output_length = 0;

    if (path == 0 || path[0] == 0 || path[0] == '/' || path[0] == '\\') {
        return OT_ROMFS_INVALID_PATH;
    }

    cursor = path;
    while (*cursor != 0) {
        const char *segment;
        uint16_t segment_length = 0;
        uint16_t index;

        while (*cursor == '/' || *cursor == '\\') cursor++;
        if (*cursor == 0) break;
        segment = cursor;
        while (
            cursor[segment_length] != 0 &&
            cursor[segment_length] != '/' &&
            cursor[segment_length] != '\\'
        ) {
            segment_length++;
        }
        cursor += segment_length;

        if (segment_length == 1 && segment[0] == '.') continue;
        if (
            segment_length == 2 &&
            segment[0] == '.' &&
            segment[1] == '.'
        ) {
            return OT_ROMFS_INVALID_PATH;
        }
        if (
            output_length != 0 &&
            output_length + 1 >= OT_ROMFS_MAX_PATH
        ) {
            return OT_ROMFS_INVALID_PATH;
        }
        if (output_length != 0) normalized[output_length++] = '/';

        for (index = 0; index < segment_length; index++) {
            uint8_t character = (uint8_t)segment[index];

            if (
                character < 0x20 ||
                character > 0x7e ||
                character == ':'
            ) {
                return OT_ROMFS_INVALID_PATH;
            }
            if (character >= 'A' && character <= 'Z') {
                character = (uint8_t)(character + ('a' - 'A'));
            }
            if (output_length + 1 >= OT_ROMFS_MAX_PATH) {
                return OT_ROMFS_INVALID_PATH;
            }
            normalized[output_length++] = (char)character;
        }
    }

    if (output_length == 0) return OT_ROMFS_INVALID_PATH;
    normalized[output_length] = 0;
    return OT_ROMFS_OK;
}

static uint32_t path_hash(const char *path)
{
    uint32_t hash = 0x811c9dc5u;

    while (*path != 0) {
        hash ^= (uint8_t)*path++;
        hash *= 0x01000193u;
    }
    return hash;
}

static const uint8_t *entry_at(const OtRomFs *fs, uint32_t index)
{
    return fs->image + fs->index_offset + index * OT_ROMFS_ENTRY_BYTES;
}

static const char *entry_path(
    const OtRomFs *fs,
    const uint8_t *entry
)
{
    return (const char *)(
        fs->image + fs->strings_offset + read_u32(entry + ENTRY_PATH_OFFSET)
    );
}

static bool validate_stored_path(
    const OtRomFs *fs,
    const uint8_t *entry,
    const char **path_out
)
{
    uint32_t offset = read_u32(entry + ENTRY_PATH_OFFSET);
    uint32_t remaining;
    uint32_t length = 0;
    const char *path;
    char normalized[OT_ROMFS_MAX_PATH];

    if (offset >= fs->path_bytes) return false;
    remaining = fs->path_bytes - offset;
    path = (const char *)(fs->image + fs->strings_offset + offset);
    while (length < remaining && path[length] != 0) length++;
    if (length == remaining || length >= OT_ROMFS_MAX_PATH) return false;
    if (normalize_path(path, normalized) != OT_ROMFS_OK) return false;
    if (strcmp(path, normalized) != 0) return false;
    if (path_hash(path) != read_u32(entry + ENTRY_PATH_HASH)) return false;
    *path_out = path;
    return true;
}

OtRomFsResult ot_romfs_mount(
    OtRomFs *fs,
    const void *image_pointer,
    uint32_t available_bytes
)
{
    const uint8_t *image = (const uint8_t *)image_pointer;
    uint32_t image_size;
    uint32_t index_end;
    uint32_t entry_index;
    uint32_t previous_hash = 0;
    const char *previous_path = 0;

    if (fs == 0 || image == 0) return OT_ROMFS_BAD_ARGUMENT;
    *fs = (OtRomFs){0};
    if (available_bytes < OT_ROMFS_HEADER_BYTES) return OT_ROMFS_BAD_IMAGE;
    if (memcmp(image, expected_magic, sizeof(expected_magic)) != 0) {
        return OT_ROMFS_BAD_IMAGE;
    }
    if (
        read_u16(image + HEADER_VERSION) != OT_ROMFS_FORMAT_VERSION ||
        read_u16(image + HEADER_HEADER_SIZE) != OT_ROMFS_HEADER_BYTES ||
        read_u16(image + HEADER_ENTRY_SIZE) != OT_ROMFS_ENTRY_BYTES
    ) {
        return OT_ROMFS_UNSUPPORTED;
    }
    if (
        read_u16(image + HEADER_FEATURES) !=
        OT_ROMFS_FEATURE_ASCII_CASEFOLD
    ) {
        return OT_ROMFS_UNSUPPORTED;
    }
    if (
        header_crc32(image) !=
        read_u32(image + HEADER_HEADER_CRC32)
    ) {
        return OT_ROMFS_BAD_IMAGE;
    }

    image_size = read_u32(image + HEADER_IMAGE_SIZE);
    if (
        image_size < OT_ROMFS_HEADER_BYTES ||
        image_size > available_bytes
    ) {
        return OT_ROMFS_BAD_IMAGE;
    }

    fs->image = image;
    fs->image_size = image_size;
    fs->entry_count = read_u32(image + HEADER_ENTRY_COUNT);
    fs->index_offset = read_u32(image + HEADER_INDEX_OFFSET);
    fs->strings_offset = read_u32(image + HEADER_STRINGS_OFFSET);
    fs->data_offset = read_u32(image + HEADER_DATA_OFFSET);
    fs->payload_bytes = read_u32(image + HEADER_PAYLOAD_BYTES);
    fs->path_bytes = read_u32(image + HEADER_PATH_BYTES);
    fs->manifest_crc32 = read_u32(image + HEADER_MANIFEST_CRC32);
    fs->metadata_crc32 = read_u32(image + HEADER_METADATA_CRC32);
    fs->payload_crc32 = read_u32(image + HEADER_PAYLOAD_CRC32);

    if (fs->index_offset != OT_ROMFS_HEADER_BYTES) {
        return OT_ROMFS_BAD_IMAGE;
    }
    if (
        fs->entry_count >
        (UINT32_MAX - fs->index_offset) / OT_ROMFS_ENTRY_BYTES
    ) {
        return OT_ROMFS_BAD_IMAGE;
    }
    index_end =
        fs->index_offset + fs->entry_count * OT_ROMFS_ENTRY_BYTES;
    if (
        index_end > fs->strings_offset ||
        fs->strings_offset > fs->data_offset ||
        fs->data_offset > fs->image_size ||
        fs->path_bytes > fs->data_offset - fs->strings_offset ||
        fs->payload_bytes > fs->image_size - fs->data_offset
    ) {
        return OT_ROMFS_BAD_IMAGE;
    }
    if (
        ot_romfs_crc32(
            fs->image + fs->index_offset,
            fs->data_offset - fs->index_offset
        ) != fs->metadata_crc32
    ) {
        return OT_ROMFS_BAD_IMAGE;
    }

    for (entry_index = 0; entry_index < fs->entry_count; entry_index++) {
        const uint8_t *entry = entry_at(fs, entry_index);
        uint32_t hash = read_u32(entry + ENTRY_PATH_HASH);
        uint32_t data_offset = read_u32(entry + ENTRY_DATA_OFFSET);
        uint32_t stored_size = read_u32(entry + ENTRY_STORED_SIZE);
        uint32_t logical_size = read_u32(entry + ENTRY_LOGICAL_SIZE);
        uint16_t flags = read_u16(entry + ENTRY_FLAGS);
        uint16_t alignment = read_u16(entry + ENTRY_ALIGNMENT);
        const char *path;

        if (
            flags != 0 ||
            stored_size != logical_size ||
            !power_of_two(alignment) ||
            alignment > 256 ||
            (data_offset & (alignment - 1)) != 0 ||
            data_offset < fs->data_offset ||
            data_offset > fs->image_size ||
            stored_size > fs->image_size - data_offset ||
            !validate_stored_path(fs, entry, &path)
        ) {
            return OT_ROMFS_BAD_IMAGE;
        }
        if (
            entry_index != 0 &&
            (
                hash < previous_hash ||
                (
                    hash == previous_hash &&
                    strcmp(path, previous_path) <= 0
                )
            )
        ) {
            return OT_ROMFS_BAD_IMAGE;
        }
        previous_hash = hash;
        previous_path = path;
    }

    fs->mounted = true;
    return OT_ROMFS_OK;
}

OtRomFsResult ot_romfs_mount_embedded(OtRomFs *fs)
{
    uintptr_t start = (uintptr_t)tyrian_romfs;
    uintptr_t end = (uintptr_t)tyrian_romfs_end;

    if (end < start || end - start > UINT32_MAX) {
        return OT_ROMFS_BAD_IMAGE;
    }
    return ot_romfs_mount(fs, tyrian_romfs, (uint32_t)(end - start));
}

OtRomFsResult ot_romfs_stat(
    const OtRomFs *fs,
    const char *path,
    OtRomFsStat *stat
)
{
    char normalized[OT_ROMFS_MAX_PATH];
    uint32_t target_hash;
    uint32_t low;
    uint32_t high;
    OtRomFsResult result;

    if (fs == 0 || stat == 0) return OT_ROMFS_BAD_ARGUMENT;
    *stat = (OtRomFsStat){0};
    if (!fs->mounted) return OT_ROMFS_BAD_IMAGE;

    result = normalize_path(path, normalized);
    if (result != OT_ROMFS_OK) return result;
    target_hash = path_hash(normalized);

    low = 0;
    high = fs->entry_count;
    while (low < high) {
        uint32_t middle = low + (high - low) / 2;
        uint32_t hash = read_u32(entry_at(fs, middle) + ENTRY_PATH_HASH);

        if (hash < target_hash) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }

    while (low < fs->entry_count) {
        const uint8_t *entry = entry_at(fs, low);
        uint32_t hash = read_u32(entry + ENTRY_PATH_HASH);
        const char *stored_path;

        if (hash != target_hash) break;
        stored_path = entry_path(fs, entry);
        if (strcmp(stored_path, normalized) == 0) {
            uint32_t offset = read_u32(entry + ENTRY_DATA_OFFSET);

            stat->data = fs->image + offset;
            stat->path = stored_path;
            stat->size = read_u32(entry + ENTRY_LOGICAL_SIZE);
            stat->crc32 = read_u32(entry + ENTRY_CRC32);
            stat->flags = read_u16(entry + ENTRY_FLAGS);
            return OT_ROMFS_OK;
        }
        low++;
    }
    return OT_ROMFS_NOT_FOUND;
}

OtRomFsResult ot_romfs_open(
    const OtRomFs *fs,
    OtRomFsFile *file,
    const char *path
)
{
    OtRomFsStat stat;
    OtRomFsResult result;

    if (file == 0) return OT_ROMFS_BAD_ARGUMENT;
    *file = (OtRomFsFile){0};
    result = ot_romfs_stat(fs, path, &stat);
    if (result != OT_ROMFS_OK) return result;

    file->data = stat.data;
    file->size = stat.size;
    file->crc32 = stat.crc32;
    file->flags = stat.flags;
    file->open = true;
    return OT_ROMFS_OK;
}

size_t ot_romfs_read(
    void *destination,
    size_t element_size,
    size_t element_count,
    OtRomFsFile *file
)
{
    uint32_t requested_bytes;
    uint32_t available_bytes;
    uint32_t copied_bytes;

    if (
        file == 0 ||
        !file->open ||
        file->position > file->size ||
        (
            destination == 0 &&
            element_size != 0 &&
            element_count != 0
        )
    ) {
        if (file != 0) file->error = true;
        return 0;
    }
    if (element_size == 0 || element_count == 0) return 0;
    if (
        element_size > UINT32_MAX ||
        element_count > UINT32_MAX ||
        element_count > UINT32_MAX / element_size
    ) {
        file->error = true;
        return 0;
    }

    requested_bytes = (uint32_t)(element_size * element_count);
    available_bytes = file->size - file->position;
    copied_bytes =
        requested_bytes < available_bytes ? requested_bytes : available_bytes;
    memcpy(destination, file->data + file->position, copied_bytes);
    file->position += copied_bytes;
    if (copied_bytes < requested_bytes) file->eof = true;
    return copied_bytes / element_size;
}

int ot_romfs_seek(OtRomFsFile *file, int32_t offset, int origin)
{
    int32_t base;
    int32_t position;

    if (
        file == 0 ||
        !file->open ||
        file->position > file->size ||
        file->size > INT32_MAX
    ) {
        if (file != 0) file->error = true;
        return -1;
    }
    switch (origin) {
    case OT_ROMFS_SEEK_SET:
        base = 0;
        break;
    case OT_ROMFS_SEEK_CUR:
        base = (int32_t)file->position;
        break;
    case OT_ROMFS_SEEK_END:
        base = (int32_t)file->size;
        break;
    default:
        file->error = true;
        return -1;
    }

    if (
        (offset > 0 && base > INT32_MAX - offset) ||
        (offset < 0 && base < INT32_MIN - offset)
    ) {
        file->error = true;
        return -1;
    }
    position = base + offset;
    if (position < 0 || (uint32_t)position > file->size) {
        file->error = true;
        return -1;
    }
    file->position = (uint32_t)position;
    file->eof = false;
    return 0;
}

int32_t ot_romfs_tell(OtRomFsFile *file)
{
    if (
        file == 0 ||
        !file->open ||
        file->position > file->size ||
        file->position > INT32_MAX
    ) {
        if (file != 0) file->error = true;
        return -1;
    }
    return (int32_t)file->position;
}

int ot_romfs_close(OtRomFsFile *file)
{
    if (file == 0 || !file->open) return -1;
    *file = (OtRomFsFile){0};
    return 0;
}

int ot_romfs_getc(OtRomFsFile *file)
{
    uint8_t value;

    if (ot_romfs_read(&value, 1, 1, file) != 1) return -1;
    return value;
}

bool ot_romfs_eof(const OtRomFsFile *file)
{
    return file != 0 && file->eof;
}

bool ot_romfs_error(const OtRomFsFile *file)
{
    return file == 0 || file->error;
}

const uint8_t *ot_romfs_direct_data(
    const OtRomFsFile *file,
    uint32_t *remaining_bytes
)
{
    if (
        file == 0 ||
        !file->open ||
        file->position > file->size
    ) {
        if (remaining_bytes != 0) *remaining_bytes = 0;
        return 0;
    }
    if (remaining_bytes != 0) {
        *remaining_bytes = file->size - file->position;
    }
    return file->data + file->position;
}

bool ot_romfs_verify_file(const OtRomFsFile *file)
{
    return file != 0 &&
           file->open &&
           ot_romfs_crc32(file->data, file->size) == file->crc32;
}

const char *ot_romfs_result_string(OtRomFsResult result)
{
    switch (result) {
    case OT_ROMFS_OK:
        return "ok";
    case OT_ROMFS_BAD_ARGUMENT:
        return "bad argument";
    case OT_ROMFS_BAD_IMAGE:
        return "bad image";
    case OT_ROMFS_UNSUPPORTED:
        return "unsupported";
    case OT_ROMFS_INVALID_PATH:
        return "invalid path";
    case OT_ROMFS_NOT_FOUND:
        return "not found";
    case OT_ROMFS_READ_ONLY:
        return "read only";
    case OT_ROMFS_TOO_MANY_OPEN_FILES:
        return "too many open files";
    case OT_ROMFS_IO_ERROR:
        return "I/O error";
    default:
        return "unknown";
    }
}
