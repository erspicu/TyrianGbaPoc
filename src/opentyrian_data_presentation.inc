/*
 * ROMFS PCX and ANM presentation decoders.
 *
 * Included by opentyrian_data.c to share its validated ROMFS view helpers.
 */
bool ot_data_pcx_decode(
    const char *filename,
    uint8_t *destination,
    uint32_t destination_bytes,
    OtDataView *palette
)
{
    OtRomFsStat file;
    uint32_t source_offset = 128;
    uint32_t encoded_end;
    uint16_t x_min;
    uint16_t y_min;
    uint16_t width;
    uint16_t height;
    uint16_t bytes_per_line;
    uint16_t y;

    if (
        filename == 0 ||
        destination == 0 ||
        palette == 0 ||
        !stat_data_file(filename, &file) ||
        file.size < 128u + 769u
    ) {
        return false;
    }
    encoded_end = file.size - 769u;
    if (
        file.data[0] != 10 ||
        file.data[2] != 1 ||
        file.data[3] != 8 ||
        file.data[65] != 1 ||
        file.data[encoded_end] != 0x0c
    ) {
        return false;
    }
    x_min = read_u16(file.data + 4);
    y_min = read_u16(file.data + 6);
    width = (uint16_t)(read_u16(file.data + 8) - x_min + 1u);
    height = (uint16_t)(read_u16(file.data + 10) - y_min + 1u);
    bytes_per_line = read_u16(file.data + 66);
    if (
        width != OT_PIC_DECODED_WIDTH ||
        height != OT_PIC_DECODED_HEIGHT ||
        bytes_per_line < width ||
        destination_bytes < (uint32_t)width * height
    ) {
        return false;
    }
    for (y = 0; y < height; y++) {
        uint16_t decoded_x = 0;

        while (decoded_x < bytes_per_line) {
            uint8_t code;
            uint8_t count = 1;
            uint8_t value;
            uint8_t repeat;

            if (source_offset >= encoded_end) return false;
            code = file.data[source_offset++];
            if ((code & 0xc0u) == 0xc0u) {
                count = (uint8_t)(code & 0x3fu);
                if (count == 0 || source_offset >= encoded_end) {
                    return false;
                }
                value = file.data[source_offset++];
            } else {
                value = code;
            }
            if ((uint32_t)decoded_x + count > bytes_per_line) {
                return false;
            }
            for (repeat = 0; repeat < count; repeat++) {
                if (decoded_x < width) {
                    destination[(uint32_t)y * width + decoded_x] =
                        value;
                }
                decoded_x++;
            }
        }
    }
    if (source_offset != encoded_end) return false;
    palette->data = file.data + encoded_end + 1u;
    palette->size = OT_PALETTE_BYTES;
    return true;
}

typedef struct {
    uint8_t *destination;
    uint32_t source_offset;
    uint16_t source_x;
    uint16_t source_y;
    int16_t destination_row;
} OtAnmOutputCursor;

static void anm_output_cursor_seek(
    OtAnmOutputCursor *cursor,
    uint32_t source_offset
)
{
    cursor->source_offset = source_offset;
    cursor->source_y = (uint16_t)(source_offset / 320u);
    cursor->source_x = (uint16_t)(
        source_offset - (uint32_t)cursor->source_y * 320u
    );
    cursor->destination_row =
        cursor->source_y < 200u && cursor->source_y % 5u != 4u ?
            (int16_t)(
                (cursor->source_y - cursor->source_y / 5u) * 240u
            ) :
            -1;
}

static bool anm_output_cursor_skip(
    OtAnmOutputCursor *cursor,
    uint32_t count
)
{
    if (
        cursor->source_offset > OT_PIC_DECODED_BYTES ||
        count > OT_PIC_DECODED_BYTES - cursor->source_offset
    ) {
        return false;
    }
    anm_output_cursor_seek(cursor, cursor->source_offset + count);
    return true;
}

static bool anm_output_cursor_write(
    OtAnmOutputCursor *cursor,
    uint8_t value
)
{
    if (cursor->source_offset >= OT_PIC_DECODED_BYTES) return false;
    if (
        cursor->destination_row >= 0 &&
        (cursor->source_x & 3u) != 3u
    ) {
        uint16_t destination_x = (uint16_t)(
            cursor->source_x - (cursor->source_x >> 2)
        );

        cursor->destination[
            (uint32_t)cursor->destination_row + destination_x
        ] = value;
    }
    cursor->source_offset++;
    cursor->source_x++;
    if (cursor->source_x == 320u) {
        cursor->source_x = 0;
        cursor->source_y++;
        cursor->destination_row =
            cursor->source_y < 200u &&
                cursor->source_y % 5u != 4u ?
                (int16_t)(
                    (cursor->source_y - cursor->source_y / 5u) * 240u
                ) :
                -1;
    }
    return true;
}

bool ot_data_anm_begin(OtAnmReader *reader, OtDataView *palette)
{
    OtRomFsStat file;
    uint16_t page_count;
    uint32_t record_count;

    if (
        reader == 0 ||
        palette == 0 ||
        !stat_data_file("tyrend.anm", &file) ||
        file.size < 0x0b00u
    ) {
        return false;
    }
    page_count = read_u16(file.data + 6);
    record_count = read_u32(file.data + 8);
    if (
        page_count == 0 ||
        page_count > 256 ||
        record_count < 2 ||
        record_count > UINT16_MAX
    ) {
        return false;
    }
    *reader = (OtAnmReader){0};
    reader->file.data = file.data;
    reader->file.size = file.size;
    reader->record_count = record_count;
    reader->page_count = page_count;
    reader->valid = true;
    /* ANM stores B,G,R,unused records rather than RGB triplets. */
    palette->data = file.data + 256u;
    palette->size = 4u * OT_PALETTE_COLOUR_COUNT;
    return true;
}

int8_t ot_data_anm_decode_next(
    OtAnmReader *reader,
    uint8_t *destination,
    uint32_t destination_bytes,
    uint16_t destination_width,
    uint16_t destination_height
)
{
    const uint8_t *data;
    uint32_t size;
    uint16_t page;
    uint16_t record;
    uint16_t first_record = 0;
    uint16_t page_record_count = 0;
    uint32_t page_offset = 0;
    uint16_t relative_record;
    uint32_t record_offset;
    uint16_t record_size;
    uint32_t record_end;
    OtAnmOutputCursor output;

    if (
        reader == 0 ||
        destination == 0 ||
        !reader->valid ||
        destination_width != 240 ||
        destination_height != 160 ||
        destination_bytes < (uint32_t)destination_width * destination_height
    ) {
        return -1;
    }
    if (reader->next_record >= reader->record_count - 1u) return 0;
    data = reader->file.data;
    size = reader->file.size;
    record = reader->next_record;
    for (page = 0; page < reader->page_count; page++) {
        uint32_t descriptor = 1280u + (uint32_t)page * 6u;
        uint16_t candidate_first;
        uint16_t candidate_count;

        if (!span_is_valid(size, descriptor, 6)) goto fail;
        candidate_first = read_u16(data + descriptor);
        candidate_count = read_u16(data + descriptor + 2u);
        if (
            record >= candidate_first &&
            (uint32_t)record <
                (uint32_t)candidate_first + candidate_count
        ) {
            first_record = candidate_first;
            page_record_count = candidate_count;
            page_offset = 0x0b00u + ((uint32_t)page << 16);
            break;
        }
    }
    if (page == reader->page_count || page_record_count == 0) goto fail;
    if (
        !span_is_valid(
            size,
            page_offset,
            8u + (uint32_t)page_record_count * 2u
        )
    ) {
        goto fail;
    }
    relative_record = (uint16_t)(record - first_record);
    record_offset =
        page_offset + 8u + (uint32_t)page_record_count * 2u;
    for (page = 0; page < relative_record; page++) {
        record_offset += read_u16(
            data + page_offset + 8u + (uint32_t)page * 2u
        );
    }
    record_size = read_u16(
        data + page_offset + 8u + (uint32_t)relative_record * 2u
    );
    if (
        record_size < 4 ||
        !span_is_valid(size, record_offset, record_size)
    ) {
        goto fail;
    }
    record_end = record_offset + record_size;
    record_offset += 4u;
    output.destination = destination;
    anm_output_cursor_seek(&output, 0);
    while (record_offset < record_end) {
        uint8_t opcode = data[record_offset++];

        if (opcode == 0) {
            uint8_t count;
            uint8_t value;

            if (!span_is_valid(record_end, record_offset, 2)) goto fail;
            count = data[record_offset++];
            value = data[record_offset++];
            while (count-- != 0) {
                if (!anm_output_cursor_write(&output, value)) goto fail;
            }
        } else if (opcode > 0x80u) {
            if (!anm_output_cursor_skip(&output, opcode - 0x80u)) {
                goto fail;
            }
        } else if (opcode < 0x80u) {
            uint8_t count = opcode;

            if (!span_is_valid(record_end, record_offset, count)) {
                goto fail;
            }
            while (count-- != 0) {
                if (!anm_output_cursor_write(
                        &output,
                        data[record_offset++]
                    )) {
                    goto fail;
                }
            }
        } else {
            uint16_t long_opcode;

            if (!span_is_valid(record_end, record_offset, 2)) goto fail;
            long_opcode = read_u16(data + record_offset);
            record_offset += 2u;
            if (long_opcode == 0) break;
            if (long_opcode >= 0xc000u) {
                uint16_t count = (uint16_t)(long_opcode - 0xc000u);
                uint8_t value;

                if (!span_is_valid(record_end, record_offset, 1)) {
                    goto fail;
                }
                value = data[record_offset++];
                while (count-- != 0) {
                    if (!anm_output_cursor_write(&output, value)) {
                        goto fail;
                    }
                }
            } else if (long_opcode < 0x8000u) {
                if (!anm_output_cursor_skip(&output, long_opcode)) {
                    goto fail;
                }
            } else {
                uint16_t count = (uint16_t)(long_opcode - 0x8000u);

                if (!span_is_valid(record_end, record_offset, count)) {
                    goto fail;
                }
                while (count-- != 0) {
                    if (!anm_output_cursor_write(
                            &output,
                            data[record_offset++]
                        )) {
                        goto fail;
                    }
                }
            }
        }
    }
    reader->next_record++;
    return 1;

fail:
    reader->valid = false;
    return -1;
}
