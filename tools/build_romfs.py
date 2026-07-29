#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Build a deterministic, memory-mapped read-only filesystem for GBA ROM."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import zlib
from dataclasses import dataclass
from pathlib import Path


MAGIC = b"TYRVFS1\0"
FORMAT_VERSION = 1
HEADER_SIZE = 64
ENTRY_SIZE = 32
FEATURE_ASCII_CASEFOLD = 1
MAX_GBA_ROM_BYTES = 32 * 1024 * 1024
MAX_VFS_PATH_CHARS = 127


@dataclass
class FileRecord:
    source_name: str
    path: str
    data: bytes
    path_hash: int
    crc32: int
    sha256: str
    path_offset: int = 0
    data_offset: int = 0


def align_up(value: int, alignment: int) -> int:
    return (value + alignment - 1) & ~(alignment - 1)


def normalize_path(path: str) -> str:
    """Match the ASCII, slash-insensitive path rules used by the C reader."""
    if not path or path[0] in "/\\" or "\0" in path:
        raise ValueError(f"invalid relative ROMFS path: {path!r}")

    normalized_segments: list[str] = []
    for segment in path.replace("\\", "/").split("/"):
        if segment in ("", "."):
            continue
        if segment == "..":
            raise ValueError(f"parent traversal is not allowed: {path!r}")
        if any(ord(char) < 0x20 or ord(char) > 0x7E for char in segment):
            raise ValueError(f"ROMFS paths must be printable ASCII: {path!r}")
        if ":" in segment:
            raise ValueError(f"drive/device syntax is not allowed: {path!r}")
        normalized_segments.append(segment.lower())

    if not normalized_segments:
        raise ValueError(f"ROMFS path normalizes to empty: {path!r}")
    return "/".join(normalized_segments)


def fnv1a32(data: bytes) -> int:
    value = 0x811C9DC5
    for byte in data:
        value ^= byte
        value = (value * 0x01000193) & 0xFFFFFFFF
    return value


def read_manifest(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as stream:
        manifest = json.load(stream)
    if manifest.get("format_version") != FORMAT_VERSION:
        raise ValueError(
            f"manifest format_version must be {FORMAT_VERSION}"
        )
    alignment = manifest.get("alignment", 4)
    if alignment <= 0 or alignment > 256 or alignment & (alignment - 1):
        raise ValueError("alignment must be a power of two from 1 through 256")
    manifest["alignment"] = alignment
    if not manifest.get("include"):
        raise ValueError("manifest include list is empty")
    if not manifest.get("probes"):
        raise ValueError("manifest probes list is empty")
    return manifest


def collect_files(manifest: dict, source_root: Path) -> list[FileRecord]:
    mount = normalize_path(manifest["mount"])
    included: dict[str, Path] = {}

    for pattern in manifest["include"]:
        matches = sorted(
            (path for path in source_root.glob(pattern) if path.is_file()),
            key=lambda path: path.name.lower(),
        )
        if not matches:
            raise FileNotFoundError(
                f"manifest pattern matched no files: {pattern!r}"
            )
        for source in matches:
            relative = source.relative_to(source_root).as_posix()
            key = relative.lower()
            previous = included.get(key)
            if previous is not None and previous != source:
                raise ValueError(
                    "source names collide after ASCII case folding: "
                    f"{previous} and {source}"
                )
            included[key] = source

    for pattern in manifest.get("exclude", []):
        for source in source_root.glob(pattern):
            if source.is_file():
                relative = source.relative_to(source_root).as_posix()
                included.pop(relative.lower(), None)

    records: list[FileRecord] = []
    seen_paths: set[str] = set()
    for source in sorted(included.values(), key=lambda path: path.name.lower()):
        source_name = source.relative_to(source_root).as_posix()
        virtual_path = normalize_path(f"{mount}/{source_name}")
        if len(virtual_path) > MAX_VFS_PATH_CHARS:
            raise ValueError(
                f"ROMFS path exceeds {MAX_VFS_PATH_CHARS} characters: "
                f"{virtual_path}"
            )
        if virtual_path in seen_paths:
            raise ValueError(f"duplicate normalized path: {virtual_path}")
        seen_paths.add(virtual_path)

        data = source.read_bytes()
        encoded_path = virtual_path.encode("ascii")
        records.append(
            FileRecord(
                source_name=source_name,
                path=virtual_path,
                data=data,
                path_hash=fnv1a32(encoded_path),
                crc32=zlib.crc32(data) & 0xFFFFFFFF,
                sha256=hashlib.sha256(data).hexdigest(),
            )
        )

    probe_paths = [
        normalize_path(f"{mount}/{name}") for name in manifest["probes"]
    ]
    probe_names = set(probe_paths)
    if len(probe_names) != len(probe_paths):
        raise ValueError("manifest probes contain duplicate normalized paths")
    missing_probes = probe_names.difference(seen_paths)
    if missing_probes:
        raise FileNotFoundError(
            "probe files are not in the image: " +
            ", ".join(sorted(missing_probes))
        )
    records_by_path = {record.path: record for record in records}
    for probe_name in probe_names:
        if len(records_by_path[probe_name].data) < 4:
            raise ValueError(
                f"probe must contain at least four bytes: {probe_name}"
            )
    return records


def collect_omitted_duplicates(
    manifest: dict,
    source_root: Path,
    active_records: list[FileRecord],
) -> list[dict]:
    """Audit source payloads replaced by an active generated runtime asset.

    Unique stock data stays in ROMFS even before its feature is connected.
    Only a source whose complete runtime role is already served by another
    embedded asset belongs here; the source itself remains in vendor/.
    """
    active_names = {
        record.source_name.lower()
        for record in active_records
    }
    omitted: dict[str, dict] = {}

    for entry in manifest.get("omitted_duplicates", []):
        if not isinstance(entry, dict):
            raise ValueError(
                "manifest omitted_duplicates entries must be objects"
            )
        pattern = entry.get("pattern")
        replacement = entry.get("replacement")
        if not isinstance(pattern, str) or not pattern:
            raise ValueError("omitted duplicate pattern is missing")
        if not isinstance(replacement, str) or not replacement.strip():
            raise ValueError(
                f"omitted duplicate replacement is missing: {pattern!r}"
            )
        matches = sorted(
            (
                path
                for path in source_root.glob(pattern)
                if path.is_file()
            ),
            key=lambda path: path.name.lower(),
        )
        if not matches:
            raise FileNotFoundError(
                "omitted duplicate pattern matched no files: "
                f"{pattern!r}"
            )
        for source in matches:
            source_name = source.relative_to(source_root).as_posix()
            key = source_name.lower()
            if key in active_names:
                raise ValueError(
                    "ROMFS source cannot be active and an omitted duplicate: "
                    f"{source_name}"
                )
            if key in omitted:
                raise ValueError(
                    f"duplicate omitted ROMFS source: {source_name}"
                )
            data = source.read_bytes()
            omitted[key] = {
                "path": normalize_path(
                    f"{manifest['mount']}/{source_name}"
                ),
                "bytes": len(data),
                "sha256": hashlib.sha256(data).hexdigest(),
                "replacement": replacement.strip(),
            }

    return [
        omitted[key]
        for key in sorted(omitted)
    ]


def build_image(manifest: dict, records: list[FileRecord]) -> tuple[bytes, dict]:
    alignment = manifest["alignment"]
    mount = normalize_path(manifest["mount"])
    path_blob = bytearray()
    by_path = sorted(records, key=lambda record: record.path)
    probe_paths = [
        normalize_path(f"{mount}/{name}") for name in manifest["probes"]
    ]

    for record in by_path:
        record.path_offset = len(path_blob)
        path_blob.extend(record.path.encode("ascii"))
        path_blob.append(0)

    index_offset = HEADER_SIZE
    strings_offset = index_offset + len(records) * ENTRY_SIZE
    data_offset = align_up(strings_offset + len(path_blob), alignment)

    image = bytearray(data_offset)
    image[strings_offset:strings_offset + len(path_blob)] = path_blob
    cursor = data_offset
    for record in by_path:
        cursor = align_up(cursor, alignment)
        if len(image) < cursor:
            image.extend(b"\0" * (cursor - len(image)))
        record.data_offset = cursor
        image.extend(record.data)
        cursor += len(record.data)
    final_size = align_up(len(image), alignment)
    image.extend(b"\0" * (final_size - len(image)))

    index_blob = bytearray()
    for record in sorted(
        records,
        key=lambda item: (item.path_hash, item.path),
    ):
        index_blob.extend(
            struct.pack(
                "<IIIIIIHHI",
                record.path_hash,
                record.path_offset,
                record.data_offset,
                len(record.data),
                len(record.data),
                record.crc32,
                0,  # flags: stored/uncompressed
                alignment,
                0,
            )
        )
    image[index_offset:index_offset + len(index_blob)] = index_blob

    identity = {
        "format_version": FORMAT_VERSION,
        "mount": mount,
        "alignment": alignment,
        "files": [
            {
                "path": record.path,
                "bytes": len(record.data),
                "crc32": f"{record.crc32:08x}",
                "sha256": record.sha256,
            }
            for record in by_path
        ],
    }
    identity_bytes = json.dumps(
        identity,
        sort_keys=True,
        separators=(",", ":"),
    ).encode("utf-8")
    manifest_crc32 = zlib.crc32(identity_bytes) & 0xFFFFFFFF
    metadata_crc32 = zlib.crc32(
        image[index_offset:data_offset]
    ) & 0xFFFFFFFF
    payload_crc32 = zlib.crc32(image[data_offset:]) & 0xFFFFFFFF
    payload_bytes = sum(len(record.data) for record in records)

    def make_header(header_crc32: int) -> bytes:
        return struct.pack(
            "<8sHHHHIIIIIIIIIIII",
            MAGIC,
            FORMAT_VERSION,
            HEADER_SIZE,
            ENTRY_SIZE,
            FEATURE_ASCII_CASEFOLD,
            len(records),
            index_offset,
            strings_offset,
            data_offset,
            len(image),
            payload_bytes,
            len(path_blob),
            manifest_crc32,
            metadata_crc32,
            payload_crc32,
            header_crc32,
            0,
        )

    zero_crc_header = make_header(0)
    header_crc32 = zlib.crc32(zero_crc_header) & 0xFFFFFFFF
    header = make_header(header_crc32)
    if len(header) != HEADER_SIZE:
        raise AssertionError(f"header is {len(header)} bytes, expected 64")
    image[:HEADER_SIZE] = header

    audit = {
        **identity,
        "magic": MAGIC.rstrip(b"\0").decode("ascii"),
        "header_bytes": HEADER_SIZE,
        "entry_bytes": ENTRY_SIZE,
        "entry_count": len(records),
        "probe_count": len(probe_paths),
        "probes": probe_paths,
        "index_offset": index_offset,
        "strings_offset": strings_offset,
        "data_offset": data_offset,
        "path_bytes": len(path_blob),
        "payload_bytes": payload_bytes,
        "image_bytes": len(image),
        "overhead_bytes": len(image) - payload_bytes,
        "manifest_crc32": f"{manifest_crc32:08x}",
        "metadata_crc32": f"{metadata_crc32:08x}",
        "payload_crc32": f"{payload_crc32:08x}",
        "header_crc32": f"{header_crc32:08x}",
        "image_sha256": hashlib.sha256(image).hexdigest(),
    }
    return bytes(image), audit


def u32_at(data: bytes, offset: int) -> int:
    return int.from_bytes(data[offset:offset + 4], "little")


def write_meta_header(
    path: Path,
    manifest: dict,
    records: list[FileRecord],
    audit: dict,
) -> None:
    by_path = {record.path: record for record in records}
    mount = normalize_path(manifest["mount"])
    probes = [
        by_path[normalize_path(f"{mount}/{name}")]
        for name in manifest["probes"]
    ]
    lines = [
        "/* Generated by tools/build_romfs.py; do not edit. */",
        "#ifndef TYRIAN_GBA_ROMFS_META_H",
        "#define TYRIAN_GBA_ROMFS_META_H",
        "",
        f"#define TYRIAN_ROMFS_FORMAT_VERSION {FORMAT_VERSION}u",
        f"#define TYRIAN_ROMFS_ENTRY_COUNT {audit['entry_count']}u",
        f"#define TYRIAN_ROMFS_IMAGE_BYTES {audit['image_bytes']}u",
        f"#define TYRIAN_ROMFS_PAYLOAD_BYTES {audit['payload_bytes']}u",
        (
            "#define TYRIAN_ROMFS_MANIFEST_CRC32 "
            f"0x{audit['manifest_crc32']}u"
        ),
        (
            "#define TYRIAN_ROMFS_METADATA_CRC32 "
            f"0x{audit['metadata_crc32']}u"
        ),
        (
            "#define TYRIAN_ROMFS_PAYLOAD_CRC32 "
            f"0x{audit['payload_crc32']}u"
        ),
        f"#define TYRIAN_ROMFS_PROBE_COUNT {len(probes)}u",
        "",
        "#define TYRIAN_ROMFS_PROBE_LIST(X) \\",
    ]
    for index, record in enumerate(probes):
        continuation = " \\" if index + 1 < len(probes) else ""
        lines.extend(
            [
                (
                    "    X("
                    f"{json.dumps(record.path)}, "
                    f"{len(record.data)}u, "
                    f"0x{record.crc32:08x}u, "
                    f"0x{u32_at(record.data, 0):08x}u, "
                    f"0x{u32_at(record.data, len(record.data) - 4):08x}u"
                    f"){continuation}"
                ),
            ]
        )
    lines.append("")
    lines.extend(["#endif", ""])
    path.write_text("\n".join(lines), encoding="ascii", newline="\n")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--meta-header", type=Path, required=True)
    parser.add_argument("--audit", type=Path, required=True)
    args = parser.parse_args()

    manifest = read_manifest(args.manifest)
    source_root = args.source_root.resolve()
    if not source_root.is_dir():
        raise FileNotFoundError(f"source root is missing: {source_root}")

    records = collect_files(manifest, source_root)
    omitted_duplicates = collect_omitted_duplicates(
        manifest,
        source_root,
        records,
    )
    image, audit = build_image(manifest, records)
    audit["omitted_duplicate_files"] = omitted_duplicates
    audit["omitted_duplicate_count"] = len(omitted_duplicates)
    audit["omitted_duplicate_bytes"] = sum(
        record["bytes"]
        for record in omitted_duplicates
    )
    if len(image) >= MAX_GBA_ROM_BYTES:
        raise ValueError(
            f"ROMFS image is {len(image)} bytes; it alone exceeds GBA ROM"
        )

    for output in (args.output, args.meta_header, args.audit):
        output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(image)
    write_meta_header(args.meta_header, manifest, records, audit)
    args.audit.write_text(
        json.dumps(audit, indent=2, ensure_ascii=True) + "\n",
        encoding="ascii",
        newline="\n",
    )

    print(
        "ROMFS "
        f"files={audit['entry_count']} "
        f"payload={audit['payload_bytes']} "
        f"deduplicated={audit['omitted_duplicate_count']}/"
        f"{audit['omitted_duplicate_bytes']} "
        f"image={audit['image_bytes']} "
        f"sha256={audit['image_sha256']}"
    )


if __name__ == "__main__":
    main()
