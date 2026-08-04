#!/usr/bin/env python3
"""Build a directly playable GBA frame stream from Tyrian's ANM file.

The stock animation is a sequence of delta-compressed 320x200 indexed
frames.  Replaying those deltas and projecting every write on ARM7TDMI is
far too expensive for a front-end transition.  This module performs only
the immutable work at build time and retains the exact palette indices and
OpenTyrian frame sequence.  Runtime therefore needs one linear ROM copy per
authored frame.
"""

from __future__ import annotations

import struct
import zlib
from dataclasses import dataclass
from pathlib import Path


SOURCE_WIDTH = 320
SOURCE_HEIGHT = 200
SOURCE_FRAME_BYTES = SOURCE_WIDTH * SOURCE_HEIGHT
TARGET_WIDTH = 240
TARGET_HEIGHT = 160
TARGET_FRAME_BYTES = TARGET_WIDTH * TARGET_HEIGHT
ANM_PALETTE_OFFSET = 0x100
ANM_PALETTE_BYTES = 4 * 256
ANM_DESCRIPTOR_OFFSET = 0x500
ANM_DESCRIPTOR_BYTES = 6
ANM_PAGE_OFFSET = 0xB00
ANM_PAGE_STRIDE = 1 << 16


@dataclass(frozen=True)
class PageDescriptor:
    first_record: int
    record_count: int
    records_size: int


def _require_span(data: bytes, offset: int, size: int, label: str) -> None:
    if offset < 0 or size < 0 or offset + size > len(data):
        raise ValueError(
            f"ANM {label} is out of bounds: offset={offset}, size={size}, "
            f"file={len(data)}"
        )


def _u16(data: bytes, offset: int) -> int:
    _require_span(data, offset, 2, "u16")
    return struct.unpack_from("<H", data, offset)[0]


def _u32(data: bytes, offset: int) -> int:
    _require_span(data, offset, 4, "u32")
    return struct.unpack_from("<I", data, offset)[0]


def _page_descriptors(data: bytes, page_count: int) -> list[PageDescriptor]:
    descriptors: list[PageDescriptor] = []
    _require_span(
        data,
        ANM_DESCRIPTOR_OFFSET,
        page_count * ANM_DESCRIPTOR_BYTES,
        "page descriptors",
    )
    for page in range(page_count):
        offset = ANM_DESCRIPTOR_OFFSET + page * ANM_DESCRIPTOR_BYTES
        descriptors.append(
            PageDescriptor(
                first_record=_u16(data, offset),
                record_count=_u16(data, offset + 2),
                records_size=_u16(data, offset + 4),
            )
        )
    return descriptors


def _record_view(
    data: bytes,
    descriptors: list[PageDescriptor],
    record_number: int,
) -> memoryview:
    for page_number, descriptor in enumerate(descriptors):
        relative = record_number - descriptor.first_record
        if relative < 0 or relative >= descriptor.record_count:
            continue
        if descriptor.record_count == 0:
            break

        page_offset = ANM_PAGE_OFFSET + page_number * ANM_PAGE_STRIDE
        sizes_offset = page_offset + 8
        records_offset = sizes_offset + descriptor.record_count * 2
        page_size = 8 + descriptor.record_count * 2 + descriptor.records_size
        _require_span(data, page_offset, page_size, f"page {page_number}")

        record_offset = records_offset
        for index in range(relative):
            record_offset += _u16(data, sizes_offset + index * 2)
        record_size = _u16(data, sizes_offset + relative * 2)
        if record_size < 4:
            raise ValueError(
                f"ANM record {record_number} is shorter than its header"
            )
        _require_span(data, record_offset, record_size, f"record {record_number}")
        if record_offset + record_size > page_offset + page_size:
            raise ValueError(f"ANM record {record_number} escapes page {page_number}")
        return memoryview(data)[record_offset : record_offset + record_size]

    raise ValueError(f"ANM record {record_number} has no page descriptor")


def _sample_offset(source_offset: int) -> int | None:
    source_y, source_x = divmod(source_offset, SOURCE_WIDTH)
    if source_y % 5 == 4 or source_x % 4 == 3:
        return None
    target_y = source_y - source_y // 5
    target_x = source_x - source_x // 4
    return target_y * TARGET_WIDTH + target_x


def _decode_record(
    record: memoryview,
    source_frame: bytearray,
    sampled_frame: bytearray,
) -> None:
    """Apply one RunSkipDump delta to full and runtime-sampled frames."""
    if len(record) < 4:
        raise ValueError("ANM record header is truncated")
    cursor = 4
    output_offset = 0
    stopped = False

    def skip(count: int) -> None:
        nonlocal output_offset
        if count < 0 or output_offset + count > SOURCE_FRAME_BYTES:
            raise ValueError("ANM skip escapes the 320x200 image")
        output_offset += count

    def write(values: bytes | memoryview) -> None:
        nonlocal output_offset
        count = len(values)
        if output_offset + count > SOURCE_FRAME_BYTES:
            raise ValueError("ANM write escapes the 320x200 image")
        for value in values:
            source_frame[output_offset] = value
            sampled_offset = _sample_offset(output_offset)
            if sampled_offset is not None:
                sampled_frame[sampled_offset] = value
            output_offset += 1

    while cursor < len(record):
        opcode = record[cursor]
        cursor += 1
        if opcode == 0:
            if cursor + 2 > len(record):
                raise ValueError("ANM short run is truncated")
            count = record[cursor]
            value = record[cursor + 1]
            cursor += 2
            write(bytes((value,)) * count)
        elif opcode > 0x80:
            skip(opcode - 0x80)
        elif opcode < 0x80:
            if cursor + opcode > len(record):
                raise ValueError("ANM short dump is truncated")
            write(record[cursor : cursor + opcode])
            cursor += opcode
        else:
            if cursor + 2 > len(record):
                raise ValueError("ANM long opcode is truncated")
            long_opcode = record[cursor] | (record[cursor + 1] << 8)
            cursor += 2
            if long_opcode == 0:
                stopped = True
                break
            if long_opcode >= 0xC000:
                count = long_opcode - 0xC000
                if cursor >= len(record):
                    raise ValueError("ANM long run is truncated")
                value = record[cursor]
                cursor += 1
                write(bytes((value,)) * count)
            elif long_opcode < 0x8000:
                skip(long_opcode)
            else:
                count = long_opcode - 0x8000
                if cursor + count > len(record):
                    raise ValueError("ANM long dump is truncated")
                write(record[cursor : cursor + count])
                cursor += count

    # OpenTyrian accepts a record that consumes its declared byte span
    # without an explicit stop, so mirror that behaviour.  Bytes following a
    # stop are padding and intentionally ignored.
    if cursor > len(record) or (not stopped and cursor != len(record)):
        raise ValueError("ANM decoder did not consume a valid record span")


def _project_reference(source_frame: bytearray) -> bytes:
    """Straight reference projection matching the current GBA runtime."""
    projected = bytearray()
    for source_y in range(SOURCE_HEIGHT):
        if source_y % 5 == 4:
            continue
        row = source_y * SOURCE_WIDTH
        projected.extend(
            source_frame[row + source_x]
            for source_x in range(SOURCE_WIDTH)
            if source_x % 4 != 3
        )
    if len(projected) != TARGET_FRAME_BYTES:
        raise AssertionError("ANM reference projection has the wrong size")
    return bytes(projected)


def _build_palette(data: bytes) -> bytes:
    _require_span(data, ANM_PALETTE_OFFSET, ANM_PALETTE_BYTES, "palette")
    words = bytearray()
    for index in range(256):
        offset = ANM_PALETTE_OFFSET + index * 4
        blue = data[offset]
        green = data[offset + 1]
        red = data[offset + 2]
        colour = 0 if index == 0 else (
            (red >> 3) | ((green >> 3) << 5) | ((blue >> 3) << 10)
        )
        words.extend(struct.pack("<H", colour))
    return bytes(words)


def build_gba_anm_assets(
    source_path: Path,
) -> tuple[bytes, bytes, dict[str, int], list[str]]:
    """Return raw GBA frames, palette, C metadata and an audit report."""
    data = source_path.read_bytes()
    if len(data) < ANM_PAGE_OFFSET:
        raise ValueError("ANM file is shorter than its fixed header")
    page_count = _u16(data, 6)
    record_count = _u32(data, 8)
    if page_count < 1 or page_count > 256:
        raise ValueError(f"ANM page count is invalid: {page_count}")
    if record_count < 2 or record_count > 0xFFFF:
        raise ValueError(f"ANM record count is invalid: {record_count}")

    descriptors = _page_descriptors(data, page_count)
    source_frame = bytearray(SOURCE_FRAME_BYTES)
    sampled_frame = bytearray(TARGET_FRAME_BYTES)
    frames = bytearray()
    for record_number in range(record_count - 1):
        record = _record_view(data, descriptors, record_number)
        _decode_record(record, source_frame, sampled_frame)
        reference = _project_reference(source_frame)
        if reference != sampled_frame:
            raise ValueError(
                "ANM GBA projection differs from the runtime write/skip "
                f"semantics at record {record_number}"
            )
        frames.extend(sampled_frame)

    palette = _build_palette(data)
    frame_count = record_count - 1
    if len(frames) != frame_count * TARGET_FRAME_BYTES:
        raise AssertionError("ANM frame stream has the wrong final size")
    metadata = {
        "TYREND_GBA_FRAME_WIDTH": TARGET_WIDTH,
        "TYREND_GBA_FRAME_HEIGHT": TARGET_HEIGHT,
        "TYREND_GBA_FRAME_BYTES": TARGET_FRAME_BYTES,
        "TYREND_GBA_FRAME_COUNT": frame_count,
        "TYREND_GBA_DATA_BYTES": len(frames),
        "TYREND_GBA_PALETTE_BYTES": len(palette),
        "TYREND_GBA_SOURCE_BYTES": len(data),
        "TYREND_GBA_SOURCE_CRC32": zlib.crc32(data),
        "TYREND_GBA_DATA_CRC32": zlib.crc32(frames),
        "TYREND_GBA_PALETTE_CRC32": zlib.crc32(palette),
    }
    report = [
        "tyrend_runtime_strategy=build-time lossless delta decode + exact "
        "240x160 indexed projection",
        f"tyrend_source_bytes={len(data)}",
        f"tyrend_source_crc32={zlib.crc32(data):08x}",
        f"tyrend_source_pages={page_count}",
        f"tyrend_source_records={record_count}",
        f"tyrend_gba_frames={frame_count}",
        f"tyrend_gba_frame_bytes={TARGET_FRAME_BYTES}",
        f"tyrend_gba_data_bytes={len(frames)}",
        f"tyrend_gba_data_crc32={zlib.crc32(frames):08x}",
        f"tyrend_gba_palette_crc32={zlib.crc32(palette):08x}",
        "tyrend_build_verification=full-frame reference projection equals "
        "runtime-sampled delta projection for every frame",
        "tyrend_romfs_source_retained=0",
    ]
    return bytes(frames), palette, metadata, report
