#!/usr/bin/env python3
"""Export and compile editable inter-level text overrides.

The original levelsN.dat stream remains authoritative for pictures, music,
jumps and all route decisions.  TextRes replaces only a yielded text block,
using the block's stable byte offset in the stock script as its key.
"""

from __future__ import annotations

import argparse
import re
import struct
from dataclasses import dataclass
from pathlib import Path


CRYPT_KEY = bytes((204, 129, 63, 255, 71, 19, 25, 62, 1, 99))
MAGIC = b"ATXT"
VERSION = 1
MAX_LINES = 10
MAX_LINE_BYTES = 60
OFFSET_PATTERN = re.compile(r"offset_([0-9a-fA-F]{8})\.txt$")
EPISODE_PATTERN = re.compile(r"Episode([1-4])$")


@dataclass(frozen=True)
class Record:
    offset: int
    end: int
    text: str


@dataclass(frozen=True)
class TextEntry:
    episode: int
    offset: int
    lines: tuple[bytes, ...]
    path: Path


def read_records(path: Path) -> list[Record]:
    source = path.read_bytes()
    cursor = 0
    result: list[Record] = []
    while cursor < len(source):
        offset = cursor
        length = source[cursor]
        cursor += 1
        if cursor + length > len(source):
            raise ValueError(f"{path}: truncated Pascal record at {offset}")
        encrypted = source[cursor : cursor + length]
        cursor += length
        decoded = bytearray(encrypted)
        for index in range(length - 1, -1, -1):
            decoded[index] ^= CRYPT_KEY[index % len(CRYPT_KEY)]
            if index:
                decoded[index] ^= encrypted[index - 1]
        result.append(
            Record(offset, cursor, decoded.decode("cp437", errors="strict"))
        )
    return result


def write_text_file(path: Path, lines: list[str], force: bool) -> None:
    content = "\n".join(lines) + "\n"
    if path.exists() and not force:
        if path.read_text(encoding="utf-8") != content:
            raise FileExistsError(
                f"refusing to overwrite edited file {path}; use --force"
            )
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8", newline="\n")


def export_episode(
    episode: int,
    script_path: Path,
    output_root: Path,
    force: bool,
) -> int:
    records = read_records(script_path)
    section = 0
    section_text_index: dict[int, int] = {}
    exported = 0
    index = 0
    episode_root = output_root / f"Episode{episode}"

    while index < len(records):
        record = records[index]
        text = record.text
        if text.startswith("*"):
            section += 1
            index += 1
            continue
        if text.startswith("]W"):
            block_start = index + 1
            cursor = block_start
            lines: list[str] = []
            while cursor < len(records) and not records[cursor].text.startswith("#"):
                lines.append(records[cursor].text)
                cursor += 1
            if cursor >= len(records) or block_start >= len(records):
                raise ValueError(f"{script_path}: unterminated ]W at {record.offset}")
            ordinal = section_text_index.get(section, 0)
            section_text_index[section] = ordinal + 1
            source_offset = records[block_start].offset
            name = (
                f"section_{section:03d}_text_{ordinal:02d}_"
                f"offset_{source_offset:08X}.txt"
            )
            write_text_file(episode_root / name, lines, force)
            exported += 1
            index = cursor + 1
            continue
        if text.startswith("]Q"):
            cursor = index + 1
            for hint in range(1, 10):
                if cursor >= len(records):
                    raise ValueError(f"{script_path}: missing ]Q hint {hint}")
                source_offset = records[cursor].offset
                lines = []
                while (
                    cursor < len(records)
                    and not records[cursor].text.startswith("#")
                ):
                    lines.append(records[cursor].text)
                    cursor += 1
                if cursor >= len(records):
                    raise ValueError(f"{script_path}: unterminated ]Q hint {hint}")
                name = (
                    f"section_{section:03d}_end_hint_{hint:02d}_"
                    f"offset_{source_offset:08X}.txt"
                )
                write_text_file(episode_root / name, lines, force)
                exported += 1
                cursor += 1
            index = cursor
            continue
        index += 1
    return exported


def load_entries(input_root: Path) -> list[TextEntry]:
    entries: list[TextEntry] = []
    keys: set[tuple[int, int]] = set()
    for path in sorted(input_root.glob("Episode*/*.txt")):
        episode_match = EPISODE_PATTERN.fullmatch(path.parent.name)
        offset_match = OFFSET_PATTERN.search(path.name)
        if episode_match is None or offset_match is None:
            raise ValueError(f"unexpected TextRes filename: {path}")
        episode = int(episode_match.group(1))
        offset = int(offset_match.group(1), 16)
        key = (episode, offset)
        if key in keys:
            raise ValueError(f"duplicate TextRes key episode={episode}, offset={offset:#x}")
        keys.add(key)

        text = path.read_text(encoding="utf-8-sig")
        lines_text = text.splitlines()
        if not lines_text:
            raise ValueError(f"{path}: a text block must contain at least one line")
        if len(lines_text) > MAX_LINES:
            raise ValueError(f"{path}: {len(lines_text)} lines exceeds {MAX_LINES}")
        encoded: list[bytes] = []
        for line_number, line in enumerate(lines_text, start=1):
            try:
                data = line.encode("cp437", errors="strict")
            except UnicodeEncodeError as error:
                raise ValueError(
                    f"{path}:{line_number}: character is not supported by the stock font"
                ) from error
            if len(data) > MAX_LINE_BYTES:
                raise ValueError(
                    f"{path}:{line_number}: {len(data)} bytes exceeds {MAX_LINE_BYTES}; "
                    "insert a manual line break"
                )
            encoded.append(data)
        entries.append(TextEntry(episode, offset, tuple(encoded), path))
    if not entries:
        raise ValueError(f"no TextRes files found under {input_root}")
    return sorted(entries, key=lambda item: (item.episode, item.offset))


def build_pack(input_root: Path, output: Path) -> None:
    entries = load_entries(input_root)
    header_size = 16
    entry_size = 12
    data_offset = header_size + len(entries) * entry_size
    index = bytearray()
    payload = bytearray()
    for entry in entries:
        entry_payload_offset = data_offset + len(payload)
        index.extend(
            struct.pack(
                "<BBHII",
                entry.episode,
                len(entry.lines),
                0,
                entry.offset,
                entry_payload_offset,
            )
        )
        for line in entry.lines:
            payload.append(len(line))
            payload.extend(line)
    header = struct.pack(
        "<4sHHII",
        MAGIC,
        VERSION,
        len(entries),
        header_size,
        data_offset,
    )
    image = header + index + payload
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(image)
    print(
        f"TextRes: {len(entries)} blocks, {len(payload)} text bytes, "
        f"{len(image)} bytes -> {output}"
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    export_parser = subparsers.add_parser(
        "export", help="export stock levelsN.dat prose to editable UTF-8 files"
    )
    export_parser.add_argument("--source-root", type=Path, required=True)
    export_parser.add_argument("--output", type=Path, required=True)
    export_parser.add_argument("--force", action="store_true")

    build_parser = subparsers.add_parser(
        "build", help="compile editable files to a ROM lookup pack"
    )
    build_parser.add_argument("--input", type=Path, required=True)
    build_parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.command == "export":
        total = 0
        for episode in range(1, 5):
            total += export_episode(
                episode,
                args.source_root / f"levels{episode}.dat",
                args.output,
                args.force,
            )
        print(f"TextRes: exported {total} editable blocks to {args.output}")
        return 0
    build_pack(args.input, args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
