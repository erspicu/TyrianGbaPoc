#!/usr/bin/env python3
"""Build GBA-native Tyrian title, Mode-0, OBJ, event, and audio assets."""

from __future__ import annotations

import argparse
import collections
import hashlib
import importlib.util
import json
import re
import struct
import wave
import zlib
from pathlib import Path
from types import ModuleType

import numpy as np
from PIL import Image, ImageDraw


SCREEN_WIDTH = 240
SCREEN_HEIGHT = 160
PC_GAME_VIEW_WIDTH = 264
PC_GAME_VIEW_HEIGHT = 184
PC_GAME_SCREEN_VISIBLE_X = 24
PC_MAP_CELL_WIDTH = 24
PC_MAP_CELL_HEIGHT = 28
PC_BG1_FIRST_ROW = 3
PC_BG1_LAST_ROW = 299
PC_BG1_INITIAL_ROW = 292
PC_BG23_FIRST_ROW = 14
PC_BG23_LAST_ROW = 599
PC_BG23_INITIAL_ROW = 592
GBA_VIEW_CROP_X = (PC_GAME_VIEW_WIDTH - SCREEN_WIDTH) // 2
GBA_VIEW_CROP_Y = (PC_GAME_VIEW_HEIGHT - SCREEN_HEIGHT) // 2
GBA_BG_MAP_WIDTH = 512
GBA_BG_MAP_COLUMNS = GBA_BG_MAP_WIDTH // 8
GBA_BG1_SOURCE_HEIGHT = (
    (PC_BG1_LAST_ROW - PC_BG1_FIRST_ROW + 1) * PC_MAP_CELL_HEIGHT
)
GBA_BG1_PACK_HEIGHT = (GBA_BG1_SOURCE_HEIGHT + 7) // 8 * 8
GBA_BG23_PACK_HEIGHT = (
    (PC_BG23_LAST_ROW - PC_BG23_FIRST_ROW + 1) * PC_MAP_CELL_HEIGHT
)
GBA_BG1_ROWS = GBA_BG1_PACK_HEIGHT // 8
GBA_BG23_ROWS = GBA_BG23_PACK_HEIGHT // 8
GBA_BG1_INITIAL_SCROLL = (
    (PC_BG1_INITIAL_ROW - PC_BG1_FIRST_ROW) * PC_MAP_CELL_HEIGHT
    + GBA_VIEW_CROP_Y
)
GBA_BG23_INITIAL_SCROLL = (
    (PC_BG23_INITIAL_ROW - PC_BG23_FIRST_ROW) * PC_MAP_CELL_HEIGHT
    + GBA_VIEW_CROP_Y
)
# Before the first JE_mainGamePlayerFunctions() call, OpenTyrian leaves all
# map-X state at zero.  The 14-column layers therefore begin at column 1 and
# the 15-column layer at column 2.  These are full-map source coordinates for
# the central GBA crop's left edge (game_screen x=36).
GBA_BG12_INITIAL_HOFS = (
    PC_MAP_CELL_WIDTH
    + PC_GAME_SCREEN_VISIBLE_X
    + GBA_VIEW_CROP_X
)
GBA_BG3_INITIAL_HOFS = (
    2 * PC_MAP_CELL_WIDTH
    + PC_GAME_SCREEN_VISIBLE_X
    + GBA_VIEW_CROP_X
)
ATLAS_STRIDE_TILES = 16
EXPLOSION_SOURCE_SEQUENCES = (
    tuple(range(122, 134)),  # ordinary small enemy: type 1
    tuple(range(3, 15)),     # air large: top-left, type 7
    tuple(range(41, 53)),    # air large: top-right, type 9
    tuple(range(22, 34)),    # air large: bottom-left, type 8
    tuple(range(60, 72)),    # air large: bottom-right, type 10
    tuple(range(192, 204)),  # ground large: top-left, type 2
    tuple(range(154, 166)),  # ground large: top-right, type 4
    tuple(range(211, 223)),  # ground large: bottom-left, type 3
    tuple(range(173, 185)),  # ground large: bottom-right, type 5
)
EXPLOSION_FRAMES_PER_SEQUENCE = 12
REWARD_SOURCE_SEQUENCES = (
    (7, 9, 11),    # HDT 391: 25-credit coin, PC six-frame cycle sampled 2:1
    (26, 28, 30),  # HDT 392: 50-credit coin, PC six-frame cycle sampled 2:1
    (20, 22, 24),  # HDT 393: 75-credit coin, PC six-frame cycle sampled 2:1
    (32, 34, 36),  # HDT 394: 100-credit gem, PC 16-frame ping-pong keyframes
    (14, 16, 18),  # HDT 395: 250-credit gem, PC 16-frame ping-pong keyframes
)
REWARD_VALUES = (25, 50, 75, 100, 250)
REWARD_FRAMES_PER_SEQUENCE = 3
CASH_DIGIT_SOURCE_IDS = (79, 70, 71, 72, 73, 74, 75, 76, 77, 78)
PAUSE_TEXT = "PAUSED"
PAUSE_TEXT_SOURCE_IDS = (15, 0, 20, 18, 4, 3)
GAME_OVER_TEXT = "GAMEOVER"
GAME_OVER_TEXT_SOURCE_IDS = (6, 0, 12, 4, 14, 21, 4, 17)
GAME_OVER_WORD_GAP = (6 * 3 + 2) // 4
GAME_OVER_SOURCE_TILE = 640
GAME_OVER_RUNTIME_TILE = 512
SECRET_LEVEL_TEXT = "SECRET LEVEL!"
SECRET_LEVEL_UNIQUE_TEXT = "SECRTLV!"
SECRET_LEVEL_SOURCE_IDS = (18, 4, 2, 17, 19, 11, 21, 26)
SECRET_LEVEL_WORD_GAP = (6 * 3 + 2) // 4
SECRET_LEVEL_SOURCE_TILE = 672
SECRET_LEVEL_RUNTIME_TILE = GAME_OVER_RUNTIME_TILE
INSERT_COIN_TEXT = "INSERT COIN"
INSERT_COIN_UNIQUE_TEXT = "INSERTCO"
INSERT_COIN_SOURCE_IDS = (8, 13, 18, 4, 17, 19, 2, 14)
INSERT_COIN_SOURCE_TILE = 704
INSERT_COIN_RUNTIME_TILE = GAME_OVER_RUNTIME_TILE
ENEMY_PROJECTILE_SOURCE_IDS = (58, 112, 113, 145, 146, 147, 201, 202)
ENEMY_PROJECTILE_WEAPON_IDS = (2, 3, 4, 59, 62, 78, 115, 116, 125, 126)
BOSS_PROJECTILE_WEAPON_IDS = (59, 127)
ENEMY_PROJECTILE_PALETTE_GROUPS = (
    (10, (112, 113)),       # animated red aimed/spread shot
    (11, (58, 201, 202)),   # orange dart and diagonal variants
    (12, (145, 146, 147)),  # purple left/down/right laser variants
)
OPENTYRIAN_SOURCE_COMMIT = "1c34d1bddac8c8f2de834229d04b5a729525c944"
FIRST_LEVEL_EVENT_LIMIT = 5400
ENEMY_FRAME_MAGIC = b"OTEF"
ENEMY_FRAME_VERSION = 1
ENEMY_FRAME_RECORD_BYTES = 8
ENEMY_FRAME_TILES = 16
ENEMY_FRAME_BYTES = ENEMY_FRAME_TILES * 32

FRONTEND_FRAME_WIDTH = 240
FRONTEND_FRAME_HEIGHT = 160
FRONTEND_FRAME_BYTES = FRONTEND_FRAME_WIDTH * FRONTEND_FRAME_HEIGHT
FRONTEND_GLYPH_WIDTH = 8
FRONTEND_GLYPH_HEIGHT = 8
FRONTEND_GLYPH_CHARACTERS = "0123456789%"
FRONTEND_PCX_PALETTES = (0, 7, 5, 8, 10, 5, 18, 19, 19, 20, 21, 22, 5)

assert GBA_VIEW_CROP_X == 12
assert GBA_VIEW_CROP_Y == 12
assert GBA_BG_MAP_COLUMNS == 64
assert GBA_BG1_SOURCE_HEIGHT == 8316
assert GBA_BG1_ROWS == 1040
assert GBA_BG23_ROWS == 2051
assert GBA_BG1_INITIAL_SCROLL == 8104
assert GBA_BG23_INITIAL_SCROLL == 16196
assert GBA_BG12_INITIAL_HOFS == 60
assert GBA_BG3_INITIAL_HOFS == 84

# OBJ palettes 0/7..14 remain assigned to the player, explosions, rewards,
# digits, projectiles, boss bar and PAUSED. During the level body, bank 5 is a
# dedicated palette for the recurring 2x2 destructible ground structures; at
# the position-5400 POC handoff, runtime restores the mutually exclusive
# simplified boss palette to that bank. Exact source frames otherwise use
# banks 1/2/3/4/6; bank 15 reproduces the source filter/ice flash.
ENEMY_FRAME_PALETTE_GROUPS = {
    1: 1,
    2: 2,
    9: 3,
    21: 4,
    10: 6,
    20: 6,
}
ENEMY_STRUCTURE_PALETTE_BANK = 5
ENEMY_STRUCTURE_FRAME_KEYS = frozenset(
    (1, graphic, 1)
    for graphic in (77, 79, 81, 83, 115, 117, 119, 121)
)
ENEMY_FILTER_PALETTE_BANK = 15
SHAPE_TABLE_CHARACTERS = "2478ABCDEFGHIJKLMNOPQRSTU5#V0@3^59"
SPRITE2_RAW_VERSION = 1
SPRITE2_RAW_TABLE_COUNT = 38
SPRITE2_RAW_COMPONENTS_PER_TABLE = 304
SPRITE2_RAW_COMPONENT_WIDTH = 12
SPRITE2_RAW_COMPONENT_HEIGHT = 14
SPRITE2_RAW_COMPONENT_BYTES = (
    SPRITE2_RAW_COMPONENT_WIDTH * SPRITE2_RAW_COMPONENT_HEIGHT
)
JUKEBOX_MUSIC_COUNT = 41
JUKEBOX_TITLE_BYTES = 48
JUKEBOX_FONT_CHARACTERS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,!?'/:-%"
JUKEBOX_BACKDROP_TILE_COUNT = 16
JUKEBOX_STAR_TILE_COUNT = 3
JUKEBOX_RECIPROCAL_MAX_Z = 500


def enemy_frame_palette_bank(key: tuple[int, int, int]) -> int:
    if key in ENEMY_STRUCTURE_FRAME_KEYS:
        return ENEMY_STRUCTURE_PALETTE_BANK
    return ENEMY_FRAME_PALETTE_GROUPS[key[0]]


def load_snes_builder(workspace: Path) -> ModuleType:
    path = workspace / "org" / "TyrianSnesPoc" / "tools" / "build_assets.py"
    spec = importlib.util.spec_from_file_location("tyrian_snes_assets_for_gba", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"could not load SNES asset builder: {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def read_git_head(repo: Path) -> str:
    """Read a local Git HEAD without depending on git.exe being on PATH."""
    git_dir = repo / ".git"
    if git_dir.is_file():
        marker = git_dir.read_text(encoding="utf-8").strip()
        if not marker.startswith("gitdir: "):
            raise ValueError(f"unexpected Git worktree marker: {git_dir}")
        git_dir = (repo / marker[8:]).resolve()

    head = (git_dir / "HEAD").read_text(encoding="ascii").strip()
    if head.startswith("ref: "):
        reference = head[5:]
        loose_ref = git_dir / reference
        if loose_ref.is_file():
            head = loose_ref.read_text(encoding="ascii").strip()
        else:
            packed = git_dir / "packed-refs"
            for line in packed.read_text(encoding="ascii").splitlines():
                if line and not line.startswith(("#", "^")):
                    commit, name = line.split(" ", 1)
                    if name == reference:
                        head = commit
                        break
            else:
                raise ValueError(f"Git reference is missing: {reference}")
    if len(head) != 40 or any(char not in "0123456789abcdef" for char in head):
        raise ValueError(f"unexpected Git HEAD value: {head}")
    return head


def sprite2_tyrian_shp_section(
    tyrian_shp: bytes,
    section: int,
) -> bytes:
    """Return a one-based tyrian.shp section exactly as the ROM reader does."""
    section_count = struct.unpack_from("<H", tyrian_shp, 0)[0]
    if not 1 <= section <= section_count:
        raise ValueError(f"tyrian.shp section outside table: {section}")
    offsets = struct.unpack_from(
        f"<{section_count}I",
        tyrian_shp,
        2,
    )
    start = offsets[section - 1]
    end = offsets[section] if section < section_count else len(tyrian_shp)
    if not 0 <= start < end <= len(tyrian_shp):
        raise ValueError(
            f"malformed tyrian.shp section {section}: {start}..{end}"
        )
    return tyrian_shp[start:end]


def sprite2_logical_bank(
    data_root: Path,
    tyrian_shp: bytes,
    shape_table: int,
) -> bytes:
    """Mirror ot_data_comp_shape_bank_view() for all logical Sprite2 banks."""
    if shape_table == 21:
        return sprite2_tyrian_shp_section(tyrian_shp, 11)
    if shape_table == 26:
        return sprite2_tyrian_shp_section(tyrian_shp, 10)
    if shape_table == 35:
        return (data_root / "newsh6.shp").read_bytes()
    if shape_table == 36:
        return sprite2_tyrian_shp_section(tyrian_shp, 8)
    if shape_table == 37:
        return sprite2_tyrian_shp_section(tyrian_shp, 12)
    if shape_table == 38:
        return sprite2_tyrian_shp_section(tyrian_shp, 9)
    if not 1 <= shape_table <= len(SHAPE_TABLE_CHARACTERS):
        raise ValueError(f"Sprite2 logical bank outside table: {shape_table}")
    character = SHAPE_TABLE_CHARACTERS[shape_table - 1].lower()
    if character == "@":
        character = "~"
    return (data_root / f"newsh{character}.shp").read_bytes()


def sprite2_component_stream(
    bank: bytes,
    sprite_number: int,
) -> bytes:
    """Apply Sprite2's first-offset/count and one-based offset semantics."""
    if len(bank) < 2:
        raise ValueError("Sprite2 bank is shorter than its first offset")
    first_offset = struct.unpack_from("<H", bank, 0)[0]
    if first_offset < 2 or first_offset & 1 or first_offset > len(bank):
        raise ValueError(f"malformed Sprite2 first offset: {first_offset}")
    sprite_count = first_offset // 2
    if not 1 <= sprite_number <= sprite_count:
        raise ValueError(
            f"Sprite2 number outside bank: {sprite_number}/{sprite_count}"
        )
    start = struct.unpack_from("<H", bank, (sprite_number - 1) * 2)[0]
    end = (
        struct.unpack_from("<H", bank, sprite_number * 2)[0]
        if sprite_number < sprite_count
        else len(bank)
    )
    if start < first_offset or end <= start or end > len(bank):
        raise ValueError(
            f"malformed Sprite2 stream {sprite_number}: {start}..{end}"
        )
    return bank[start:end]


def decode_sprite2_raw_component(encoded: bytes) -> bytes:
    """Losslessly decode one 12x14 Sprite2 stream to palette-index bytes."""
    output = bytearray(SPRITE2_RAW_COMPONENT_BYTES)
    source = 0
    x = 0
    y = 0
    terminated = False

    while source < len(encoded):
        code = encoded[source]
        source += 1
        if code == 0x0F:
            terminated = True
            break
        skip_count = code & 0x0F
        fill_count = code >> 4
        x += skip_count
        if fill_count == 0:
            if x != SPRITE2_RAW_COMPONENT_WIDTH or (
                y >= SPRITE2_RAW_COMPONENT_HEIGHT
            ):
                raise ValueError(
                    f"malformed Sprite2 row ending at ({x}, {y})"
                )
            x = 0
            y += 1
            continue
        if (
            y >= SPRITE2_RAW_COMPONENT_HEIGHT
            or x + fill_count > SPRITE2_RAW_COMPONENT_WIDTH
            or source + fill_count > len(encoded)
        ):
            raise ValueError(
                f"malformed Sprite2 fill at ({x}, {y}), count={fill_count}"
            )
        for pixel in encoded[source : source + fill_count]:
            # Stock Sprite2 does not use opaque palette index zero.  Keeping
            # zero as transparent therefore preserves every authored pixel
            # in one byte instead of requiring a separate alpha mask.
            if pixel == 0:
                raise ValueError(
                    "opaque Sprite2 palette index zero cannot be represented"
                )
            output[
                y * SPRITE2_RAW_COMPONENT_WIDTH + x
            ] = pixel
            x += 1
        source += fill_count
    if not terminated:
        raise ValueError("Sprite2 stream has no 0x0f terminator")

    # Independent replay: every encoded skip must address a transparent raw
    # byte and every fill must recover the exact original palette index.
    source = 0
    x = 0
    y = 0
    while source < len(encoded):
        code = encoded[source]
        source += 1
        if code == 0x0F:
            break
        skip_count = code & 0x0F
        fill_count = code >> 4
        for skipped_x in range(x, x + skip_count):
            if output[
                y * SPRITE2_RAW_COMPONENT_WIDTH + skipped_x
            ] != 0:
                raise ValueError("Sprite2 raw round-trip changed a skip")
        x += skip_count
        if fill_count == 0:
            x = 0
            y += 1
            continue
        for pixel in encoded[source : source + fill_count]:
            if output[
                y * SPRITE2_RAW_COMPONENT_WIDTH + x
            ] != pixel:
                raise ValueError("Sprite2 raw round-trip changed a fill")
            x += 1
        source += fill_count
    return bytes(output)


def build_sprite2_raw_components(
    data_root: Path,
) -> tuple[bytes, dict[str, int | str]]:
    """
    Decode every logical newsh/tyrian.shp component, never an event-limited
    subset.  Runtime still chooses shape_table/graphic from stock LVL/HDT.
    """
    tyrian_shp = (data_root / "tyrian.shp").read_bytes()
    output = bytearray()
    encoded_crc32 = 0
    encoded_bytes = 0
    component_count = 0

    for shape_table in range(1, SPRITE2_RAW_TABLE_COUNT + 1):
        bank = sprite2_logical_bank(data_root, tyrian_shp, shape_table)
        first_offset = struct.unpack_from("<H", bank, 0)[0]
        sprite_count = first_offset // 2
        if sprite_count != SPRITE2_RAW_COMPONENTS_PER_TABLE:
            raise ValueError(
                f"Sprite2 table {shape_table} count changed: "
                f"{sprite_count} != {SPRITE2_RAW_COMPONENTS_PER_TABLE}"
            )
        for sprite_number in range(1, sprite_count + 1):
            encoded = sprite2_component_stream(bank, sprite_number)
            raw = decode_sprite2_raw_component(encoded)
            if len(raw) != SPRITE2_RAW_COMPONENT_BYTES:
                raise AssertionError("Sprite2 raw component stride changed")
            output.extend(raw)
            encoded_crc32 = zlib.crc32(encoded, encoded_crc32)
            encoded_bytes += len(encoded)
            component_count += 1

    expected_components = (
        SPRITE2_RAW_TABLE_COUNT *
        SPRITE2_RAW_COMPONENTS_PER_TABLE
    )
    expected_bytes = expected_components * SPRITE2_RAW_COMPONENT_BYTES
    if component_count != expected_components or len(output) != expected_bytes:
        raise AssertionError(
            f"Sprite2 raw catalog changed: {component_count=}, "
            f"bytes={len(output)}, expected={expected_bytes}"
        )
    report: dict[str, int | str] = {
        "version": SPRITE2_RAW_VERSION,
        "table_count": SPRITE2_RAW_TABLE_COUNT,
        "components_per_table": SPRITE2_RAW_COMPONENTS_PER_TABLE,
        "component_count": component_count,
        "component_width": SPRITE2_RAW_COMPONENT_WIDTH,
        "component_height": SPRITE2_RAW_COMPONENT_HEIGHT,
        "component_bytes": SPRITE2_RAW_COMPONENT_BYTES,
        "raw_bytes": len(output),
        "raw_crc32": f"{zlib.crc32(output) & 0xffffffff:08x}",
        "raw_sha256": hashlib.sha256(output).hexdigest(),
        "source_stream_bytes": encoded_bytes,
        "source_stream_crc32": f"{encoded_crc32 & 0xffffffff:08x}",
        "roundtrip_components": component_count,
    }
    return bytes(output), report


def write_sprite2_raw_header(
    output: Path,
    report: dict[str, int | str],
) -> None:
    lines = [
        "#ifndef TYRIAN_GBA_SPRITE2_RAW_META_H",
        "#define TYRIAN_GBA_SPRITE2_RAW_META_H",
        "",
        f"#define SPRITE2_RAW_VERSION {report['version']}u",
        f"#define SPRITE2_RAW_TABLE_COUNT {report['table_count']}u",
        (
            "#define SPRITE2_RAW_COMPONENTS_PER_TABLE "
            f"{report['components_per_table']}u"
        ),
        f"#define SPRITE2_RAW_COMPONENT_COUNT {report['component_count']}u",
        f"#define SPRITE2_RAW_COMPONENT_WIDTH {report['component_width']}u",
        f"#define SPRITE2_RAW_COMPONENT_HEIGHT {report['component_height']}u",
        f"#define SPRITE2_RAW_COMPONENT_BYTES {report['component_bytes']}u",
        f"#define SPRITE2_RAW_DATA_BYTES {report['raw_bytes']}u",
        f"#define SPRITE2_RAW_DATA_CRC32 0x{report['raw_crc32']}u",
        (
            "#define SPRITE2_RAW_SOURCE_STREAM_CRC32 "
            f"0x{report['source_stream_crc32']}u"
        ),
        (
            "#define SPRITE2_RAW_ROUNDTRIP_COMPONENTS "
            f"{report['roundtrip_components']}u"
        ),
        "",
        "#endif",
        "",
    ]
    (output / "sprite2_raw_meta.h").write_text(
        "\n".join(lines),
        encoding="ascii",
    )


def encode_gba_4bpp(values: np.ndarray) -> bytes:
    values = values.reshape(8, 8)
    output = bytearray(32)
    cursor = 0
    for y in range(8):
        for x in range(0, 8, 2):
            output[cursor] = int(values[y, x]) | (int(values[y, x + 1]) << 4)
            cursor += 1
    return bytes(output)


def decode_snes_4bpp(tile: bytes) -> np.ndarray:
    if len(tile) != 32:
        raise ValueError("SNES 4bpp tile must be 32 bytes")
    values = np.zeros((8, 8), dtype=np.uint8)
    for y in range(8):
        p0, p1 = tile[y * 2 : y * 2 + 2]
        p2, p3 = tile[16 + y * 2 : 18 + y * 2]
        for x in range(8):
            bit = 7 - x
            values[y, x] = (
                ((p0 >> bit) & 1)
                | (((p1 >> bit) & 1) << 1)
                | (((p2 >> bit) & 1) << 2)
                | (((p3 >> bit) & 1) << 3)
            )
    return values


def convert_tile_bank(snes_tiles: bytes) -> bytes:
    if len(snes_tiles) % 32:
        raise ValueError("SNES tile bank is not tile aligned")
    output = bytearray()
    for offset in range(0, len(snes_tiles), 32):
        output.extend(encode_gba_4bpp(decode_snes_4bpp(snes_tiles[offset : offset + 32])))
    return bytes(output)


def convert_tilemap(snes_map: bytes, palette_base: int = 0) -> bytes:
    if len(snes_map) % 2:
        raise ValueError("SNES tilemap is not word aligned")
    words = np.frombuffer(snes_map, dtype="<u2")
    output = np.empty(len(words), dtype="<u2")
    for index, source_word in enumerate(words):
        word = int(source_word)
        tile = word & 0x03FF
        palette = palette_base + ((word >> 10) & 0x07)
        if palette > 15:
            raise ValueError(f"GBA palette index exceeds 15: {palette}")
        horizontal_flip = (word >> 14) & 1
        vertical_flip = (word >> 15) & 1
        output[index] = (
            tile
            | (horizontal_flip << 10)
            | (vertical_flip << 11)
            | (palette << 12)
        )
    return output.tobytes()


def build_title(nes: ModuleType, image_root: Path) -> Image.Image:
    source = nes.build_title(image_root).crop((0, 8, 256, 174)).convert("RGB")
    top = source.resize((SCREEN_WIDTH, 113), Image.Resampling.LANCZOS)
    output = Image.new("RGB", (SCREEN_WIDTH, SCREEN_HEIGHT), (0, 0, 0))
    output.paste(top, (0, 0))

    draw = ImageDraw.Draw(output)
    draw.rectangle((0, 108, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1), fill=(0, 0, 0))

    def centred(y: int, text: str, size: int, colour: tuple[int, int, int]) -> None:
        font = nes.find_font(size)
        box = draw.textbbox((0, 0), text, font=font)
        width = box[2] - box[0]
        draw.text(((SCREEN_WIDTH - width) // 2, y), text, font=font, fill=colour)

    centred(116, "PRESS START", 15, (255, 255, 255))
    centred(145, "GBA LEVEL 1 HARDWARE DEMO", 8, (104, 208, 255))
    return output


def frontend_glyph_id(character: str) -> int:
    if "A" <= character <= "Z":
        return ord(character) - ord("A")
    if "a" <= character <= "z":
        return 34 + ord(character) - ord("a")
    if "1" <= character <= "9":
        return 69 + ord(character) - ord("0")
    return {
        "0": 79,
        "!": 26,
        '"': 33,
        "#": 60,
        "$": 61,
        "%": 62,
        "'": 32,
        "(": 64,
        ")": 65,
        "*": 63,
        "+": 84,
        ",": 29,
        "-": 83,
        ".": 28,
        "/": 80,
        ":": 31,
        ";": 30,
        "=": 85,
        "?": 27,
        "[": 68,
        "\\": 82,
        "]": 69,
        "{": 66,
        "|": 81,
        "}": 67,
    }.get(character, -1)


def decode_frontend_text(hdt_path: Path) -> dict[str, list[str]]:
    data = hdt_path.read_bytes()
    crypt_key = (204, 129, 63, 255, 71, 19, 25, 62, 1, 99)
    position = 4

    def skip() -> None:
        nonlocal position
        if position >= len(data):
            raise ValueError("tyrian.hdt text table is truncated")
        length = data[position]
        position += 1
        if position + length > len(data):
            raise ValueError("tyrian.hdt Pascal string is truncated")
        position += length

    def read() -> str:
        nonlocal position
        if position >= len(data):
            raise ValueError("tyrian.hdt text table is truncated")
        length = data[position]
        position += 1
        encrypted = bytearray(data[position : position + length])
        if len(encrypted) != length:
            raise ValueError("tyrian.hdt Pascal string is truncated")
        position += length
        for index in range(length - 1, -1, -1):
            encrypted[index] ^= crypt_key[index % len(crypt_key)]
            if index:
                encrypted[index] ^= encrypted[index - 1]
        return encrypted.decode("latin1")

    def skip_group(entry_count: int) -> None:
        skip()
        for _ in range(entry_count):
            skip()
        skip()

    def read_group(entry_count: int) -> list[str]:
        skip()
        values = [read() for _ in range(entry_count)]
        skip()
        return values

    # OpenTyrian JE_loadHelpText(), in source/file order.
    skip_group(39)
    planet_name = read_group(21)
    misc_text = read_group(68)
    skip_group(5)
    skip_group(11)
    title_menu = read_group(7)
    skip_group(9)
    skip_group(6)
    skip_group(34)
    full_game_menu = read_group(7)
    skip_group(9)
    skip_group(8)
    skip_group(6)
    skip_group(6)
    skip_group(5)
    episode_name = read_group(6)
    difficulty_name = read_group(7)
    gameplay_name = read_group(5)
    return {
        "planet_name": planet_name,
        "misc_text": misc_text,
        "title_menu": title_menu,
        "full_game_menu": full_game_menu,
        "episode_name": episode_name,
        "difficulty_name": difficulty_name,
        "gameplay_name": gameplay_name,
    }


class FrontendSourceRenderer:
    """Build-time counterpart of the OpenTyrian PIC/SHP menu renderer."""

    def __init__(self, data_root: Path):
        self.pic_data = (data_root / "tyrian.pic").read_bytes()
        self.shp_data = (data_root / "tyrian.shp").read_bytes()
        self.palette_data = (data_root / "palette.dat").read_bytes()
        self.text = decode_frontend_text(data_root / "tyrian.hdt")
        self.pic_count = struct.unpack_from("<H", self.pic_data, 0)[0]
        self.shp_count = struct.unpack_from("<H", self.shp_data, 0)[0]
        if self.pic_count != 13 or self.shp_count != 12:
            raise ValueError(
                "unexpected Tyrian PIC/SHP table count: "
                f"{self.pic_count=}, {self.shp_count=}"
            )
        if len(self.palette_data) != 23 * 256 * 3:
            raise ValueError("unexpected palette.dat size")
        self._picture_cache: dict[int, np.ndarray] = {}
        self._sprite_cache: dict[tuple[int, int], np.ndarray | None] = {}

    @staticmethod
    def scale_x(source_x: int) -> int:
        return max(0, source_x * FRONTEND_FRAME_WIDTH // 320)

    @staticmethod
    def scale_y(source_y: int) -> int:
        return max(0, source_y * FRONTEND_FRAME_HEIGHT // 200)

    def palette_rgb(self, picture_number: int) -> np.ndarray:
        palette_number = FRONTEND_PCX_PALETTES[picture_number - 1]
        offset = palette_number * 256 * 3
        return np.frombuffer(
            self.palette_data[offset : offset + 256 * 3],
            dtype=np.uint8,
        ).reshape(256, 3)

    def palette_gba(self, picture_number: int) -> bytes:
        rgb = self.palette_rgb(picture_number).astype(np.uint16)
        words = (
            (rgb[:, 0] >> 1)
            | ((rgb[:, 1] >> 1) << 5)
            | ((rgb[:, 2] >> 1) << 10)
        ).astype("<u2")
        return words.tobytes()

    def decode_picture(self, picture_number: int) -> np.ndarray:
        cached = self._picture_cache.get(picture_number)
        if cached is not None:
            return cached.copy()
        index = picture_number - 1
        if not 0 <= index < self.pic_count:
            raise ValueError(f"PIC number outside source table: {picture_number}")
        start = struct.unpack_from("<I", self.pic_data, 2 + index * 4)[0]
        end = (
            struct.unpack_from("<I", self.pic_data, 2 + (index + 1) * 4)[0]
            if index + 1 < self.pic_count
            else len(self.pic_data)
        )
        stream = self.pic_data[start:end]
        output = bytearray()
        position = 0
        while len(output) < 320 * 200:
            if position >= len(stream):
                raise ValueError(f"PIC {picture_number} RLE stream is truncated")
            code = stream[position]
            position += 1
            if code & 0xC0 == 0xC0:
                count = code & 0x3F
                if count == 0 or position >= len(stream):
                    raise ValueError(f"PIC {picture_number} has invalid RLE")
                output.extend(bytes((stream[position],)) * count)
                position += 1
            else:
                output.append(code)
        if len(output) != 320 * 200:
            raise ValueError(f"PIC {picture_number} RLE overruns its canvas")
        picture = np.frombuffer(bytes(output), dtype=np.uint8).reshape(200, 320)
        self._picture_cache[picture_number] = picture
        return picture.copy()

    def picture_frame(self, picture_number: int) -> np.ndarray:
        picture = self.decode_picture(picture_number)
        source_x = np.arange(FRONTEND_FRAME_WIDTH) * 320 // FRONTEND_FRAME_WIDTH
        source_y = np.arange(FRONTEND_FRAME_HEIGHT) * 200 // FRONTEND_FRAME_HEIGHT
        return picture[np.ix_(source_y, source_x)].copy()

    def sprite(self, table: int, sprite_index: int) -> np.ndarray | None:
        key = (table, sprite_index)
        if key in self._sprite_cache:
            sprite = self._sprite_cache[key]
            return None if sprite is None else sprite.copy()
        if not 0 <= table < 7:
            raise ValueError(f"SHP table outside source file: {table}")
        start = struct.unpack_from("<I", self.shp_data, 2 + table * 4)[0]
        position = start
        count = struct.unpack_from("<H", self.shp_data, position)[0]
        position += 2
        if not 0 <= sprite_index < count:
            raise ValueError(
                f"SHP sprite outside table: {table=}, {sprite_index=}, {count=}"
            )
        for index in range(sprite_index + 1):
            populated = self.shp_data[position] != 0
            position += 1
            if not populated:
                if index == sprite_index:
                    self._sprite_cache[key] = None
                    return None
                continue
            width, height, encoded_bytes = struct.unpack_from(
                "<HHH", self.shp_data, position
            )
            position += 6
            encoded = self.shp_data[position : position + encoded_bytes]
            position += encoded_bytes
            if index != sprite_index:
                continue
            pixels = np.full((height, width), 0xFF, dtype=np.uint8)
            source = 0
            x = 0
            y = 0
            while source < len(encoded):
                code = encoded[source]
                source += 1
                if code == 255:
                    if source >= len(encoded):
                        raise ValueError("SHP skip opcode is truncated")
                    x += encoded[source]
                    source += 1
                elif code == 254:
                    x = width
                elif code == 253:
                    x += 1
                else:
                    if x < width and y < height:
                        pixels[y, x] = code
                    x += 1
                while x >= width:
                    x -= width
                    y += 1
            self._sprite_cache[key] = pixels
            return pixels.copy()
        raise AssertionError("SHP sprite iterator did not reach its target")

    def text_width(self, text: str, font: int) -> int:
        width = 0
        for character in text:
            glyph_id = frontend_glyph_id(character)
            if character == " ":
                width += 6
            elif character != "~" and glyph_id >= 0:
                glyph = self.sprite(font, glyph_id)
                if glyph is not None:
                    width += glyph.shape[1] + 1
        return width

    def draw_glyph(
        self,
        frame: np.ndarray,
        glyph: np.ndarray,
        source_x: int,
        source_y: int,
        hue: int,
        value: int,
        shadow: bool = False,
    ) -> None:
        glyph_height, glyph_width = glyph.shape
        output_x0 = self.scale_x(source_x)
        output_y0 = self.scale_y(source_y)
        output_x1 = self.scale_x(source_x + glyph_width)
        output_y1 = self.scale_y(source_y + glyph_height)
        output_width = max(1, output_x1 - output_x0)
        output_height = max(1, output_y1 - output_y0)
        for output_y in range(output_height):
            target_y = output_y0 + output_y
            if not 0 <= target_y < FRONTEND_FRAME_HEIGHT:
                continue
            source_y0 = output_y * glyph_height // output_height
            source_y1 = min(
                glyph_height,
                (
                    (output_y + 1) * glyph_height +
                    output_height - 1
                ) // output_height,
            )
            for output_x in range(output_width):
                target_x = output_x0 + output_x
                if not 0 <= target_x < FRONTEND_FRAME_WIDTH:
                    continue
                source_x0 = output_x * glyph_width // output_width
                source_x1 = min(
                    glyph_width,
                    (
                        (output_x + 1) * glyph_width +
                        output_width - 1
                    ) // output_width,
                )
                footprint = glyph[
                    source_y0:source_y1,
                    source_x0:source_x1,
                ]
                opaque = footprint[footprint != 0xFF]
                if not opaque.size:
                    continue
                pixel = int(opaque[np.argmax(opaque & 15)])
                if shadow:
                    frame[target_y, target_x] = 0
                else:
                    brightness = min(15, max(0, (pixel & 15) + value))
                    frame[target_y, target_x] = (hue << 4) | brightness

    def draw_text(
        self,
        frame: np.ndarray,
        text: str,
        source_x: int,
        source_y: int,
        font: int,
        align: str,
        hue: int,
        value: int,
        shadow_distance: int,
    ) -> None:
        x = source_x
        if align == "center":
            x -= self.text_width(text, font) // 2
        elif align == "right":
            x -= self.text_width(text, font)

        def pass_text(start_x: int, y: int, shadow: bool) -> None:
            cursor_x = start_x
            bright = 0
            for character in text:
                glyph_id = frontend_glyph_id(character)
                if character == " ":
                    cursor_x += 6
                elif character == "~":
                    bright = 4 if bright == 0 else 0
                elif glyph_id >= 0:
                    glyph = self.sprite(font, glyph_id)
                    if glyph is not None:
                        self.draw_glyph(
                            frame,
                            glyph,
                            cursor_x,
                            y,
                            hue,
                            value + bright,
                            shadow,
                        )
                        cursor_x += glyph.shape[1] + 1

        if shadow_distance:
            pass_text(
                x + shadow_distance,
                source_y + shadow_distance,
                True,
            )
        pass_text(x, source_y, False)

    def draw_logo(self, frame: np.ndarray) -> None:
        logo = self.sprite(3, 146)
        if logo is None or logo.shape != (121, 304):
            raise ValueError(
                "Tyrian title logo shape changed: "
                f"{None if logo is None else logo.shape}"
            )
        for y in range(91):
            source_y = y * logo.shape[0] // 91
            for x in range(228):
                pixel = int(logo[source_y, x * logo.shape[1] // 228])
                if pixel != 0xFF:
                    frame[6 + y, 6 + x] = pixel


def build_frontend_mode4_assets(
    data_root: Path,
    preview: Path,
) -> tuple[bytes, bytes, bytes, bytes, dict[str, int], list[str]]:
    source = FrontendSourceRenderer(data_root)
    frames: list[np.ndarray] = []
    palettes: list[bytes] = []
    names: list[str] = []
    metadata: dict[str, int] = {
        "FRONTEND_FRAME_BYTES": FRONTEND_FRAME_BYTES,
        "FRONTEND_GLYPH_BYTES":
            FRONTEND_GLYPH_WIDTH * FRONTEND_GLYPH_HEIGHT,
        "FRONTEND_GLYPH_COUNT": len(FRONTEND_GLYPH_CHARACTERS),
    }
    frontend_preview = preview / "frontend_mode4"
    frontend_preview.mkdir(parents=True, exist_ok=True)

    def add(name: str, frame: np.ndarray, picture_number: int) -> int:
        if frame.shape != (FRONTEND_FRAME_HEIGHT, FRONTEND_FRAME_WIDTH):
            raise ValueError(f"front-end frame has invalid shape: {frame.shape}")
        index = len(frames)
        frames.append(frame.copy())
        palettes.append(source.palette_gba(picture_number))
        names.append(name)
        metadata[f"FRONTEND_FRAME_{name.upper()}"] = index
        rgb = np.minimum(
            source.palette_rgb(picture_number).astype(np.uint16) * 4,
            255,
        ).astype(np.uint8)
        Image.fromarray(rgb[frame], "RGB").save(
            frontend_preview / f"{index:02d}_{name}.png"
        )
        return index

    def render_title(selection: int) -> np.ndarray:
        frame = source.picture_frame(4)
        source.draw_logo(frame)
        source.draw_text(
            frame, "Start New Game", 160, 108, 1, "center",
            15, -1 if selection == 0 else -4, 2,
        )
        source.draw_text(
            frame, "Demo", 160, 120, 1, "center",
            15, -2 if selection == 1 else -5, 2,
        )
        source.draw_text(
            frame, "JukeBox", 160, 132, 1, "center",
            15, -6 if selection == 2 else -8, 2,
        )
        return frame

    def render_select_menu(
        title: str,
        items: list[str],
        selection: int,
        source_y: int,
        source_dy: int,
        left_aligned: bool,
    ) -> np.ndarray:
        frame = source.picture_frame(2)
        source.draw_text(
            frame, title, 160, 20, 0, "center", 15, -3, 2
        )
        for index, item in enumerate(items):
            source.draw_text(
                frame,
                item,
                20 if left_aligned else 160,
                source_y + source_dy * index,
                1,
                "left" if left_aligned else "center",
                15,
                -2 if selection == index else -4,
                2,
            )
        return frame

    def render_game_menu(selection: int) -> np.ndarray:
        frame = source.picture_frame(1)
        items = source.text["full_game_menu"]
        source.draw_text(
            frame, items[0] or "Game Menu",
            234, 10, 0, "center", 15, -3, 2,
        )
        for index in range(6):
            source_y = 38 + index * 16 + (16 if index == 5 else 0)
            disabled = index < 4
            value = -8 if disabled else -3
            if selection == index:
                value += 2
            source.draw_text(
                frame,
                items[index + 1],
                166,
                source_y,
                2,
                "left",
                15,
                value,
                2,
            )
        return frame

    def render_next_level(selection: int) -> np.ndarray:
        frame = source.picture_frame(1)
        source.draw_text(
            frame, "Next Level", 234, 10, 0, "center", 15, -3, 2
        )
        source.draw_text(
            frame,
            source.text["planet_name"][0] or "Tyrian",
            166,
            38,
            2,
            "left",
            15,
            -1 if selection == 0 else -3,
            2,
        )
        source.draw_text(
            frame,
            "Exit to Game Menu",
            166,
            150,
            2,
            "left",
            15,
            -1 if selection == 1 else -3,
            2,
        )
        return frame

    def render_stats(
        stage: int,
        collected: bool,
        arcade: bool,
    ) -> np.ndarray:
        frame = np.zeros(
            (FRONTEND_FRAME_HEIGHT, FRONTEND_FRAME_WIDTH),
            dtype=np.uint8,
        )
        misc = source.text["misc_text"]
        source.draw_text(
            frame, (misc[26] or "Completed") + " Tyrian",
            20, 20, 2, "left", 15, 2, 2,
        )
        cash_label = misc[27] or "Cash"
        if stage >= 1:
            source.draw_text(
                frame, cash_label,
                30, 50, 2, "left", 15, 2, 2,
            )
        enemy_label = misc[62] or "Enemies Destroyed"
        if stage >= 2:
            source.draw_text(
                frame, enemy_label,
                40, 90, 2, "left", 15, 2, 2,
            )
        if not arcade and stage >= 3:
            source.draw_text(
                frame, misc[3] or "Cubes",
                30, 120, 2, "left", 15, 2, 2,
            )
            if not collected:
                source.draw_text(
                    frame,
                    misc[14] or "None",
                    50,
                    135,
                    2,
                    "left",
                    15,
                    2,
                    2,
                )
        if stage >= 4:
            source.draw_text(
                frame, misc[4] or "Press a key",
                90, 160, 2, "left", 15, 2, 2,
            )
        metadata["FRONTEND_STATS_CASH_X"] = source.scale_x(
            30 + source.text_width(cash_label, 2) + 6
        )
        metadata["FRONTEND_STATS_CASH_Y"] = source.scale_y(50)
        metadata["FRONTEND_STATS_KILLED_X"] = source.scale_x(
            40 + source.text_width(enemy_label, 2) + 6
        )
        metadata["FRONTEND_STATS_KILLED_Y"] = source.scale_y(90)
        return frame

    add("intro_logo_1", source.picture_frame(10), 10)
    add("intro_logo_2", source.picture_frame(12), 12)
    metadata["FRONTEND_FRAME_TITLE_BASE"] = len(frames)
    for selection in range(3):
        add(f"title_{selection}", render_title(selection), 4)

    metadata["FRONTEND_FRAME_PLAY_MODE_BASE"] = len(frames)
    for selection in range(2):
        add(
            f"play_mode_{selection}",
            render_select_menu(
                "Play Mode",
                ["Full Game", "Arcade"],
                selection,
                54,
                24,
                False,
            ),
            2,
        )

    episode_items = [
        source.text["episode_name"][index + 1]
        for index in range(4)
    ]
    metadata["FRONTEND_FRAME_EPISODE_BASE"] = len(frames)
    for selection in range(4):
        add(
            f"episode_{selection}",
            render_select_menu(
                source.text["episode_name"][0] or "Select an Episode",
                episode_items,
                selection,
                50,
                30,
                True,
            ),
            2,
        )

    metadata["FRONTEND_FRAME_DIFFICULTY_BASE"] = len(frames)
    for selection in range(3):
        add(
            f"difficulty_{selection}",
            render_select_menu(
                source.text["difficulty_name"][0] or "Difficulty Level",
                ["Easy", "Normal", "Hard"],
                selection,
                54,
                24,
                False,
            ),
            2,
        )

    metadata["FRONTEND_FRAME_GAME_MENU_BASE"] = len(frames)
    for selection in range(6):
        add(f"game_menu_{selection}", render_game_menu(selection), 1)

    metadata["FRONTEND_FRAME_NEXT_LEVEL_BASE"] = len(frames)
    for selection in range(2):
        add(f"next_level_{selection}", render_next_level(selection), 1)

    add("stats_completed", render_stats(0, False, False), 1)
    add("stats_cash", render_stats(1, False, False), 1)
    add("stats_enemies", render_stats(2, False, False), 1)
    add("stats_full_none", render_stats(3, False, False), 1)
    add("stats_full_cubes", render_stats(3, True, False), 1)
    add("stats_full_final_none", render_stats(4, False, False), 1)
    add("stats_full_final_cubes", render_stats(4, True, False), 1)
    add("stats_arcade_final", render_stats(4, False, True), 1)

    game_over = np.zeros(
        (FRONTEND_FRAME_HEIGHT, FRONTEND_FRAME_WIDTH),
        dtype=np.uint8,
    )
    source.draw_text(
        game_over, "GAME OVER", 160, 74, 0, "center", 15, 2, 2
    )
    source.draw_text(
        game_over,
        source.text["misc_text"][4] or "Press a key",
        160,
        120,
        2,
        "center",
        15,
        0,
        2,
    )
    add("game_over", game_over, 1)

    glyphs = bytearray()
    for character_index, character in enumerate(FRONTEND_GLYPH_CHARACTERS):
        glyph = source.sprite(2, frontend_glyph_id(character))
        if glyph is None:
            raise ValueError(f"front-end dynamic glyph is empty: {character!r}")
        canvas = np.full(
            (FRONTEND_GLYPH_HEIGHT, FRONTEND_GLYPH_WIDTH),
            0xFF,
            dtype=np.uint8,
        )
        source.draw_glyph(canvas, glyph, 0, 0, 15, 2)
        glyphs.extend(canvas.tobytes())
        metadata[f"FRONTEND_GLYPH_{character_index}_ADVANCE"] = max(
            1,
            source.scale_x(glyph.shape[1] + 1),
        )

    # JE_drawCube() draws OPTION_SHAPES sprite 25 twice as a dark offset
    # shadow, followed by its hue-9 foreground.  Store one transparent
    # Mode-4 stamp so runtime can reveal the PC cube sprites one by one.
    cube = source.sprite(5, 25)
    if cube is None:
        raise ValueError("OPTION_SHAPES data cube sprite 25 is empty")
    cube_canvas = np.full(
        (FRONTEND_FRAME_HEIGHT, FRONTEND_FRAME_WIDTH),
        0xFF,
        dtype=np.uint8,
    )
    source.draw_glyph(cube_canvas, cube, 4, 4, 9, 0, True)
    source.draw_glyph(cube_canvas, cube, 3, 3, 9, 0, True)
    source.draw_glyph(cube_canvas, cube, 0, 0, 9, 0)
    cube_width = source.scale_x(cube.shape[1] + 4)
    cube_height = source.scale_y(cube.shape[0] + 4)
    cube_stamp = cube_canvas[:cube_height, :cube_width].copy()
    metadata["FRONTEND_CUBE_WIDTH"] = cube_width
    metadata["FRONTEND_CUBE_HEIGHT"] = cube_height
    metadata["FRONTEND_CUBE_BYTES"] = cube_width * cube_height
    metadata["FRONTEND_STATS_CUBE_Y"] = source.scale_y(135)
    for cube_index in range(4):
        metadata[f"FRONTEND_STATS_CUBE_X_{cube_index}"] = source.scale_x(
            20 + 30 * (cube_index + 1)
        )
    cube_rgb = np.minimum(
        source.palette_rgb(1).astype(np.uint16) * 4,
        255,
    ).astype(np.uint8)
    cube_preview = np.zeros(
        (cube_height, cube_width, 4),
        dtype=np.uint8,
    )
    cube_opaque = cube_stamp != 0xFF
    cube_preview[cube_opaque, :3] = cube_rgb[cube_stamp[cube_opaque]]
    cube_preview[cube_opaque, 3] = 255
    Image.fromarray(cube_preview, "RGBA").resize(
        (cube_width * 8, cube_height * 8),
        Image.Resampling.NEAREST,
    ).save(frontend_preview / "stats_data_cube_option_shape_25.png")

    metadata["FRONTEND_FRAME_COUNT"] = len(frames)
    frame_bytes = b"".join(frame.tobytes() for frame in frames)
    palette_bytes = b"".join(palettes)
    if len(frame_bytes) != len(frames) * FRONTEND_FRAME_BYTES:
        raise AssertionError("Mode 4 front-end frame packing changed")
    if len(palette_bytes) != len(frames) * 512:
        raise AssertionError("Mode 4 front-end palette packing changed")

    unique_counts: list[int] = []
    tile_mode_bytes: list[int] = []
    global_tiles: set[bytes] = set()
    for frame in frames:
        tiles = {
            frame[y : y + 8, x : x + 8].tobytes()
            for y in range(0, FRONTEND_FRAME_HEIGHT, 8)
            for x in range(0, FRONTEND_FRAME_WIDTH, 8)
        }
        unique_counts.append(len(tiles))
        global_tiles.update(tiles)
        tile_mode_bytes.append(len(tiles) * 64 + 600 * 2)

    selection_groups = (
        (
            "title", metadata["FRONTEND_FRAME_TITLE_BASE"], 3,
            lambda selection: (
                0, (108 + selection * 12) * 160 // 200, 240, 12
            ),
        ),
        (
            "play_mode", metadata["FRONTEND_FRAME_PLAY_MODE_BASE"], 2,
            lambda selection: (
                0, (54 + selection * 24) * 160 // 200, 240, 12
            ),
        ),
        (
            "episode", metadata["FRONTEND_FRAME_EPISODE_BASE"], 4,
            lambda selection: (
                0, (50 + selection * 30) * 160 // 200, 240, 12
            ),
        ),
        (
            "difficulty", metadata["FRONTEND_FRAME_DIFFICULTY_BASE"], 3,
            lambda selection: (
                0, (54 + selection * 24) * 160 // 200, 240, 12
            ),
        ),
        (
            "game_menu", metadata["FRONTEND_FRAME_GAME_MENU_BASE"], 6,
            lambda selection: (
                120,
                (
                    38 + selection * 16 +
                    (16 if selection == 5 else 0)
                ) * 160 // 200,
                120,
                9,
            ),
        ),
        (
            "next_level", metadata["FRONTEND_FRAME_NEXT_LEVEL_BASE"], 2,
            lambda selection: (
                120,
                (38 if selection == 0 else 150) * 160 // 200,
                120,
                9,
            ),
        ),
    )
    patch_transitions = 0
    patch_max_bytes = 0
    for group_name, base, count, rectangle in selection_groups:
        for old_selection in range(count):
            for new_selection in range(count):
                if old_selection == new_selection:
                    continue
                coverage = np.zeros(
                    (FRONTEND_FRAME_HEIGHT, FRONTEND_FRAME_WIDTH),
                    dtype=bool,
                )
                transfer_bytes = 0
                for selection in (old_selection, new_selection):
                    x, y, width, height = rectangle(selection)
                    coverage[y : y + height, x : x + width] = True
                    transfer_bytes += width * height
                changed = (
                    frames[base + old_selection] !=
                    frames[base + new_selection]
                )
                uncovered = changed & ~coverage
                if uncovered.any():
                    y_values, x_values = np.where(uncovered)
                    raise ValueError(
                        "Mode 4 selection rectangle misses changed pixels: "
                        f"{group_name=}, {old_selection=}, "
                        f"{new_selection=}, pixels={uncovered.sum()}, "
                        f"bbox=({x_values.min()},{y_values.min()},"
                        f"{x_values.max() + 1},{y_values.max() + 1})"
                    )
                patch_transitions += 1
                patch_max_bytes = max(patch_max_bytes, transfer_bytes)
    report = [
        "frontend_renderer=build-time OpenTyrian PIC/SHP/HDT",
        (
            "frontend_runtime=GBA Mode 4 full-state DMA/page flip "
            "+ selection-row patches"
        ),
        f"frontend_frame_count={len(frames)}",
        f"frontend_frame_bytes={FRONTEND_FRAME_BYTES}",
        f"frontend_frames_raw_bytes={len(frame_bytes)}",
        f"frontend_palettes_bytes={len(palette_bytes)}",
        f"frontend_dynamic_glyph_bytes={len(glyphs)}",
        f"frontend_data_cube_stamp_bytes={cube_stamp.size}",
        f"frontend_full_state_transfer_bytes={FRONTEND_FRAME_BYTES + 512}",
        f"frontend_zlib_reference_bytes={len(zlib.compress(frame_bytes, 9))}",
        f"frontend_tile_unique_min={min(unique_counts)}",
        f"frontend_tile_unique_max={max(unique_counts)}",
        (
            "frontend_tile_unique_mean="
            f"{sum(unique_counts) / len(unique_counts):.2f}"
        ),
        f"frontend_tile_global_unique={len(global_tiles)}",
        f"frontend_tile_per_state_min_bytes={min(tile_mode_bytes)}",
        f"frontend_tile_per_state_max_bytes={max(tile_mode_bytes)}",
        f"frontend_selection_patch_transitions={patch_transitions}",
        "frontend_selection_patch_uncovered_pixels=0",
        f"frontend_selection_patch_max_bytes={patch_max_bytes}",
        "frontend_decision=Mode4 avoids runtime decode and 4bpp palette partitioning",
        "frontend_invalid_key_redraw=none",
        *(
            f"frame_{index:02d}={name},unique_tiles={unique_counts[index]},"
            f"crc32={zlib.crc32(frames[index].tobytes()):08x}"
            for index, name in enumerate(names)
        ),
    ]
    return (
        frame_bytes,
        palette_bytes,
        bytes(glyphs),
        cube_stamp.tobytes(),
        metadata,
        report,
    )


def pack_gba_palette(
    banks: list[list[tuple[int, int, int]]],
) -> bytes:
    output = bytearray()
    for bank in banks:
        if len(bank) > 16:
            raise ValueError("GBA palette bank exceeds 16 colours")
        for red, green, blue in bank:
            if not (
                0 <= red <= 31 and
                0 <= green <= 31 and
                0 <= blue <= 31
            ):
                raise ValueError("GBA palette component outside 5-bit range")
            output.extend(struct.pack(
                "<H",
                red | green << 5 | blue << 10,
            ))
        output.extend(b"\0" * ((16 - len(bank)) * 2))
    return bytes(output).ljust(512, b"\0")


def gradient_palette_bank(
    colour: tuple[int, int, int],
) -> list[tuple[int, int, int]]:
    return [
        (0, 0, 0),
        *[
            tuple(
                max(0, min(31, component * level // 15))
                for component in colour
            )
            for level in range(1, 16)
        ],
    ]


def parse_opentyrian_music_titles(path: Path) -> list[str]:
    text = path.read_text(encoding="utf-8")
    marker = "const char musicTitle"
    start = text.index(marker)
    start = text.index("{", start)
    end = text.index("};", start)
    titles = [
        bytes(value, "utf-8").decode("unicode_escape")
        for value in re.findall(r'"((?:\\.|[^"\\])*)"', text[start:end])
    ]
    if len(titles) != JUKEBOX_MUSIC_COUNT:
        raise ValueError(
            "OpenTyrian musicTitle count changed: "
            f"{len(titles)} != {JUKEBOX_MUSIC_COUNT}"
        )
    if any(len(title.encode("ascii")) >= JUKEBOX_TITLE_BYTES for title in titles):
        raise ValueError("OpenTyrian Jukebox title exceeds fixed ROM record")
    return titles


def build_jukebox_assets(
    data_root: Path,
    opentyrian_root: Path,
    preview: Path,
) -> tuple[dict[str, bytes], dict[str, int], list[str]]:
    """Build a tile/OAM adapter for OpenTyrian's Jukebox and starlib."""
    source = FrontendSourceRenderer(data_root)
    titles = parse_opentyrian_music_titles(
        opentyrian_root / "src" / "musmast.c"
    )

    # Tile zero is the blank glyph used to clear dynamic text map cells.
    font_tiles = bytearray(32)
    for character in JUKEBOX_FONT_CHARACTERS:
        glyph = source.sprite(1, frontend_glyph_id(character))
        if glyph is None:
            raise ValueError(f"Jukebox font glyph is empty: {character!r}")
        glyph_height, glyph_width = glyph.shape
        output_width = max(
            1,
            min(8, (glyph_width * 8 + glyph_height // 2) // glyph_height),
        )
        x_offset = (8 - output_width) // 2
        values = np.zeros((8, 8), dtype=np.uint8)
        for output_y in range(8):
            source_y = min(
                glyph_height - 1,
                output_y * glyph_height // 8,
            )
            for output_x in range(output_width):
                source_x = min(
                    glyph_width - 1,
                    output_x * glyph_width // output_width,
                )
                pixel = int(glyph[source_y, source_x])
                if pixel != 0xFF:
                    values[output_y, x_offset + output_x] = (
                        10 + min(5, pixel & 7)
                    )
        font_tiles.extend(encode_gba_4bpp(values))

    random = np.random.default_rng(0x4A554B45)
    backdrop_tiles = bytearray()
    for tile_index in range(JUKEBOX_BACKDROP_TILE_COUNT):
        values = np.zeros((8, 8), dtype=np.uint8)
        point_count = 1 + tile_index % 4
        for point in range(point_count):
            x = int(random.integers(0, 8))
            y = int(random.integers(0, 8))
            values[y, x] = 1 + (tile_index + point) % 3
        backdrop_tiles.extend(encode_gba_4bpp(values))

    backdrop_map = bytearray()
    for y in range(32):
        for x in range(32):
            value = x * 37 + y * 53 + (x ^ (y * 3)) * 11
            tile = value % JUKEBOX_BACKDROP_TILE_COUNT
            palette = (value >> 4) & 7
            backdrop_map.extend(struct.pack(
                "<H",
                tile | palette << 12,
            ))

    star_tiles: list[np.ndarray] = []
    small = np.zeros((8, 8), dtype=np.uint8)
    small[3, 3] = 12
    small[3, 2] = small[3, 4] = 4
    star_tiles.append(small)
    cross = np.zeros((8, 8), dtype=np.uint8)
    cross[3, 3] = 15
    cross[3, 2] = cross[3, 4] = 8
    cross[2, 3] = cross[4, 3] = 8
    star_tiles.append(cross)
    flare = np.zeros((8, 8), dtype=np.uint8)
    flare[3, 3] = 15
    flare[3, 2:5] = (7, 15, 7)
    flare[2:5, 3] = (7, 15, 7)
    flare[3, 1] = flare[3, 5] = 3
    flare[1, 3] = flare[5, 3] = 3
    star_tiles.append(flare)
    star_tile_data = b"".join(
        encode_gba_4bpp(tile)
        for tile in star_tiles
    )

    colour_wheel = [
        (8, 15, 31),
        (0, 27, 31),
        (13, 10, 31),
        (27, 8, 31),
        (31, 9, 20),
        (31, 22, 5),
        (8, 31, 15),
        (25, 31, 31),
    ]
    bg_banks = [
        gradient_palette_bank(colour)
        for colour in colour_wheel
    ]
    bg_banks.extend((
        gradient_palette_bank((8, 27, 31)),
        gradient_palette_bank((31, 31, 31)),
        gradient_palette_bank((31, 22, 5)),
        gradient_palette_bank((22, 10, 31)),
    ))
    obj_banks = [
        gradient_palette_bank(colour)
        for colour in colour_wheel
    ]

    title_data = bytearray()
    for title in titles:
        encoded = title.encode("ascii")
        title_data.extend(encoded.ljust(JUKEBOX_TITLE_BYTES, b"\0"))

    reciprocals = [
        0,
        *[
            (1 << 16) // z
            for z in range(1, JUKEBOX_RECIPROCAL_MAX_Z + 1)
        ],
    ]
    reciprocal_data = struct.pack(
        f"<{len(reciprocals)}I",
        *reciprocals,
    )
    sine = np.rint(
        np.sin(
            np.arange(256, dtype=np.float64) *
            (2.0 * np.pi / 256.0)
        ) *
        32767.0
    ).astype("<i2")

    font_preview = np.zeros(
        (8, (len(JUKEBOX_FONT_CHARACTERS) + 1) * 8),
        dtype=np.uint8,
    )
    for index in range(len(JUKEBOX_FONT_CHARACTERS) + 1):
        tile = font_tiles[index * 32 : (index + 1) * 32]
        for y in range(8):
            for pair in range(4):
                packed = tile[y * 4 + pair]
                font_preview[y, index * 8 + pair * 2] = packed & 15
                font_preview[y, index * 8 + pair * 2 + 1] = packed >> 4
    Image.fromarray(
        (font_preview * 17).astype(np.uint8),
        "L",
    ).resize(
        (font_preview.shape[1] * 2, 16),
        Image.Resampling.NEAREST,
    ).save(preview / "jukebox_pc_font_tiles.png")

    assets = {
        "jukebox_font_tiles.bin": bytes(font_tiles),
        "jukebox_backdrop_tiles.bin": bytes(backdrop_tiles),
        "jukebox_backdrop_map.bin": bytes(backdrop_map),
        "jukebox_bg_palette.bin": pack_gba_palette(bg_banks),
        "jukebox_obj_tiles.bin": star_tile_data,
        "jukebox_obj_palette.bin": pack_gba_palette(obj_banks),
        "jukebox_titles.bin": bytes(title_data),
        "jukebox_reciprocal.bin": reciprocal_data,
        "jukebox_sine.bin": sine.tobytes(),
    }
    metadata = {
        "JUKEBOX_MUSIC_COUNT": JUKEBOX_MUSIC_COUNT,
        "JUKEBOX_TITLE_BYTES": JUKEBOX_TITLE_BYTES,
        "JUKEBOX_FONT_TILE_COUNT":
            len(JUKEBOX_FONT_CHARACTERS) + 1,
        "JUKEBOX_BACKDROP_TILE_COUNT": JUKEBOX_BACKDROP_TILE_COUNT,
        "JUKEBOX_STAR_TILE_COUNT": JUKEBOX_STAR_TILE_COUNT,
        "JUKEBOX_RECIPROCAL_MAX_Z": JUKEBOX_RECIPROCAL_MAX_Z,
        "JUKEBOX_SINE_COUNT": len(sine),
    }
    report = [
        "jukebox_source=OpenTyrian jukebox.c/starlib.c/musmast.c",
        f"jukebox_music_titles={len(titles)}",
        (
            "jukebox_font_characters="
            f"{len(JUKEBOX_FONT_CHARACTERS)}"
        ),
        f"jukebox_backdrop_tiles={JUKEBOX_BACKDROP_TILE_COUNT}",
        f"jukebox_star_tiles={JUKEBOX_STAR_TILE_COUNT}",
        "jukebox_presentation=Mode0 BG tile text + parallax BG + OBJ stars",
        "jukebox_runtime_full_frame_dma=0",
    ]
    return assets, metadata, report


def bitmap_555(image: Image.Image) -> bytes:
    pixels = np.asarray(image.convert("RGB"), dtype=np.uint16)
    words = (
        (pixels[:, :, 0] >> 3)
        | ((pixels[:, :, 1] >> 3) << 5)
        | ((pixels[:, :, 2] >> 3) << 10)
    ).astype("<u2")
    return words.tobytes()


def gba_colour(rgb: tuple[int, int, int]) -> int:
    red, green, blue = rgb
    return (red >> 3) | ((green >> 3) << 5) | ((blue >> 3) << 10)


def preserve_sprite_canvas(
    snes: ModuleType,
    image: Image.Image,
    size: tuple[int, int],
    offset: tuple[int, int] = (0, 0),
) -> Image.Image:
    """Place a sprite without cropping away its source-space anchor."""
    source = snes.normalize_sprite(image)
    x, y = offset
    if (
        x < 0
        or y < 0
        or x + source.width > size[0]
        or y + source.height > size[1]
    ):
        raise ValueError(
            "native sprite canvas does not fit: "
            f"source={source.size}, target={size}, offset={offset}"
        )
    canvas = Image.new("RGBA", size, (0, 0, 0, 0))
    canvas.alpha_composite(source, offset)
    return canvas


def compose_sprite_2x2(directory: Path, start: int) -> Image.Image:
    canvas = Image.new("RGBA", (24, 28), (0, 0, 0, 0))
    for source_id, x, y in (
        (start, 0, 0),
        (start + 1, 12, 0),
        (start + 19, 0, 14),
        (start + 20, 12, 14),
    ):
        canvas.alpha_composite(
            Image.open(directory / f"{source_id:03d}.png").convert("RGBA"),
            (x, y),
        )
    return canvas


def quantize_sprite_frames(
    snes: ModuleType,
    frames: list[Image.Image],
) -> tuple[bytes, bytes]:
    """Quantize equally sized RGBA frames to one GBA 4bpp OBJ palette."""
    if not frames:
        raise ValueError("sprite frame bank is empty")
    size = frames[0].size
    if size[0] % 8 or size[1] % 8:
        raise ValueError(f"sprite frame size is not tile aligned: {size}")
    if any(frame.size != size for frame in frames):
        raise ValueError("sprite frame bank contains mixed dimensions")

    rgba = np.stack(
        [np.asarray(frame, dtype=np.uint8) for frame in frames],
        axis=0,
    )
    opaque = rgba[:, :, :, 3] >= 80
    colours = snes.adaptive_palette(rgba[:, :, :, :3][opaque])
    palette = [(0, 0, 0)] + colours
    palette_array = np.asarray(palette[1:], dtype=np.int32)

    tile_data = bytearray()
    for frame in frames:
        frame_rgba = np.asarray(frame, dtype=np.uint8)
        frame_mask = frame_rgba[:, :, 3] >= 80
        values = np.zeros((size[1], size[0]), dtype=np.uint8)
        if frame_mask.any():
            pixels = frame_rgba[frame_mask, :3].astype(np.int32)
            nearest = (
                ((pixels[:, None, :] - palette_array[None, :, :]) ** 2)
                .sum(axis=2)
                .argmin(axis=1)
                .astype(np.uint8)
                + 1
            )
            values[frame_mask] = nearest
        for tile_y in range(size[1] // 8):
            for tile_x in range(size[0] // 8):
                tile_data.extend(
                    encode_gba_4bpp(
                        values[
                            tile_y * 8 : tile_y * 8 + 8,
                            tile_x * 8 : tile_x * 8 + 8,
                        ]
                    )
                )
    return bytes(tile_data), snes.snes_palette_bytes([palette])


def build_explosion_animation(
    snes: ModuleType,
    image_root: Path,
) -> tuple[bytes, bytes, Image.Image, Image.Image]:
    """Build Tyrian's small and four-quadrant air/ground explosions."""
    source_dir = image_root / "sheets_newsh" / "newsh_6"
    source_ids = [
        source_id
        for sequence in EXPLOSION_SOURCE_SEQUENCES
        for source_id in sequence
    ]
    frames: list[Image.Image] = []
    for source_id in source_ids:
        source = Image.open(
            source_dir / f"{source_id:03d}.png"
        ).convert("RGBA")
        if source.width != 12 or source.height > 14:
            raise ValueError(
                "unexpected Tyrian explosion source canvas: "
                f"{source_id} is {source.size}, expected 12x<=14"
            )
        # OpenTyrian positions the four large-explosion parts at x +/- 6 and
        # y - 14 / y.  The native 12-pixel width and top-left anchor therefore
        # make the quadrants meet exactly.  Cropping each alpha bbox and
        # centring it in a 16x16 OBJ introduced the visible cross-shaped gap.
        frames.append(preserve_sprite_canvas(snes, source, (16, 16)))
    rgba = np.stack(
        [np.asarray(frame, dtype=np.uint8) for frame in frames],
        axis=0,
    )
    opaque = rgba[:, :, :, 3] >= 80
    colours = snes.adaptive_palette(rgba[:, :, :, :3][opaque])
    palette = [(0, 0, 0)] + colours
    palette_array = np.asarray(palette[1:], dtype=np.int32)

    tile_data = bytearray()
    preview = Image.new(
        "RGBA",
        (
            16 * EXPLOSION_FRAMES_PER_SEQUENCE,
            16 * len(EXPLOSION_SOURCE_SEQUENCES),
        ),
        (0, 0, 0, 0),
    )
    for frame_index, frame in enumerate(frames):
        frame_rgba = np.asarray(frame, dtype=np.uint8)
        frame_mask = frame_rgba[:, :, 3] >= 80
        values = np.zeros((16, 16), dtype=np.uint8)
        if frame_mask.any():
            pixels = frame_rgba[frame_mask, :3].astype(np.int32)
            nearest = (
                ((pixels[:, None, :] - palette_array[None, :, :]) ** 2)
                .sum(axis=2)
                .argmin(axis=1)
                .astype(np.uint8)
                + 1
            )
            values[frame_mask] = nearest
        for tile_y in range(2):
            for tile_x in range(2):
                tile_data.extend(
                    encode_gba_4bpp(
                        values[
                            tile_y * 8 : tile_y * 8 + 8,
                            tile_x * 8 : tile_x * 8 + 8,
                        ]
                    )
                )
        preview.alpha_composite(
            frame,
            (
                (frame_index % EXPLOSION_FRAMES_PER_SEQUENCE) * 16,
                (frame_index // EXPLOSION_FRAMES_PER_SEQUENCE) * 16,
            ),
        )

    composite = Image.new(
        "RGBA",
        (24 * EXPLOSION_FRAMES_PER_SEQUENCE, 28 * 2),
        (0, 0, 0, 0),
    )
    for frame in range(EXPLOSION_FRAMES_PER_SEQUENCE):
        x = frame * 24
        for row, first_sequence in ((0, 1), (28, 5)):
            composite.alpha_composite(
                frames[(first_sequence + 0) * EXPLOSION_FRAMES_PER_SEQUENCE + frame],
                (x, row),
            )
            composite.alpha_composite(
                frames[(first_sequence + 1) * EXPLOSION_FRAMES_PER_SEQUENCE + frame],
                (x + 12, row),
            )
            composite.alpha_composite(
                frames[(first_sequence + 2) * EXPLOSION_FRAMES_PER_SEQUENCE + frame],
                (x, row + 14),
            )
            composite.alpha_composite(
                frames[(first_sequence + 3) * EXPLOSION_FRAMES_PER_SEQUENCE + frame],
                (x + 12, row + 14),
            )

    return (
        bytes(tile_data),
        snes.snes_palette_bytes([palette]),
        preview,
        composite,
    )


def build_reward_animation(
    snes: ModuleType,
    image_root: Path,
) -> tuple[bytes, bytes, Image.Image]:
    """Build unmodified spriteSheet11 coin animations for rewards."""
    source_dir = image_root / "sheets" / "11_coins_cubes"
    frames: list[Image.Image] = []
    preview = Image.new(
        "RGBA",
        (
            16 * REWARD_FRAMES_PER_SEQUENCE,
            16 * len(REWARD_SOURCE_SEQUENCES),
        ),
        (0, 0, 0, 0),
    )
    for sequence_index, sequence in enumerate(REWARD_SOURCE_SEQUENCES):
        if len(sequence) != REWARD_FRAMES_PER_SEQUENCE:
            raise ValueError("reward animation sequence has an invalid length")
        for frame_index, source_id in enumerate(sequence):
            source = Image.open(
                source_dir / f"{source_id:03d}.png"
            ).convert("RGBA")
            if source.width != 12 or source.height > 14:
                raise ValueError(
                    "unexpected Tyrian reward source canvas: "
                    f"{source_id} is {source.size}, expected 12x<=14"
                )
            source = preserve_sprite_canvas(
                snes, source, (16, 16), (2, 1)
            )
            # Keep the PC sprite's alpha edge and colours intact.  The older
            # GBA conversion added a pale one-pixel readability outline here,
            # which produced a ring that does not exist in Tyrian.
            frame = source
            frames.append(frame)
            preview.alpha_composite(
                frame,
                (frame_index * 16, sequence_index * 16),
            )
    tile_data, palette = quantize_sprite_frames(snes, frames)
    return tile_data, palette, preview


def load_tyrian_palette(path: Path) -> list[tuple[int, int, int]]:
    """Load the first 256-colour Tyrian palette and expand VGA 6-bit RGB."""
    data = path.read_bytes()
    if len(data) < 256 * 3:
        raise ValueError(f"Tyrian palette is truncated: {path}")
    palette: list[tuple[int, int, int]] = []
    for index in range(256):
        components = data[index * 3 : index * 3 + 3]
        palette.append(tuple((value << 2) | (value >> 4) for value in components))
    return palette


def build_boss_bar_assets(
    snes: ModuleType,
    palette_file: Path,
) -> tuple[bytes, bytes, Image.Image, tuple[tuple[int, int, int], ...]]:
    """Build the PC 51x6 boss-bar shading as reusable 8x8 OBJ segments.

    OpenTyrian's JE_barX uses palette 115 for the fixed dark backing and
    palette 118..124 for the damage-flash fill.  Four tiles are enough for a
    full backing segment, a full fill segment, and the two four-pixel fill
    halves used to keep the scaled GBA bar centred.
    """
    tyrian_palette = load_tyrian_palette(palette_file)
    palette = [
        (0, 0, 0),
        tyrian_palette[114],
        tyrian_palette[115],
        tyrian_palette[116],
        tyrian_palette[117],
        tyrian_palette[118],
        tyrian_palette[119],
    ]
    palette.extend([(0, 0, 0)] * (16 - len(palette)))

    def segment(top: int, middle: int, bottom: int, x1: int, x2: int) -> bytes:
        values = np.zeros((8, 8), dtype=np.uint8)
        values[0, x1:x2] = top
        values[1:5, x1:x2] = middle
        values[5, x1:x2] = bottom
        return encode_gba_4bpp(values)

    tiles = b"".join((
        segment(3, 2, 1, 0, 8),
        segment(6, 5, 4, 0, 8),
        segment(6, 5, 4, 0, 4),
        segment(6, 5, 4, 4, 8),
    ))

    preview = Image.new("RGBA", (48, 8), (0, 0, 0, 0))
    pixels = preview.load()
    for x in range(40):
        for y, colour_index in enumerate((3, 2, 2, 2, 2, 1)):
            pixels[x + 4, y + 1] = (*palette[colour_index], 255)
    for x in range(38):
        for y, colour_index in enumerate((6, 5, 5, 5, 5, 4)):
            pixels[x + 5, y + 1] = (*palette[colour_index], 255)

    flash_colours = tuple(
        (
            gba_colour(tyrian_palette[117 + flash]),
            gba_colour(tyrian_palette[118 + flash]),
            gba_colour(tyrian_palette[119 + flash]),
        )
        for flash in range(7)
    )
    return tiles, snes.snes_palette_bytes([palette]), preview, flash_colours


def build_cash_digits(
    snes: ModuleType,
    image_root: Path,
    palette_file: Path,
) -> tuple[bytes, bytes, Image.Image, tuple[int, ...]]:
    """Recreate PC JE_textShade cash digits from TINY_FONT sprites 79/70-78."""
    source_dir = image_root / "sprites" / "02_tinyfont"
    tyrian_palette = load_tyrian_palette(palette_file)

    # JE_inGameDisplays uses hue 2, brightness 4 and FULL_SHADE.  The tiny
    # glyphs contain palette low nibbles 3 and 7, so the visible pixels become
    # PC palette entries 0x27 and 0x2B.  FULL_SHADE adds four black copies at
    # x +/- 1 and y +/- 1 before drawing the coloured glyph.
    source_slots = {
        tyrian_palette[3]: 2,
        tyrian_palette[7]: 3,
    }
    palette = [
        (0, 0, 0),       # OBJ colour 0: transparent
        (0, 0, 0),       # FULL_SHADE outline
        tyrian_palette[0x27],
        tyrian_palette[0x2B],
    ]
    tile_data = bytearray()
    preview = Image.new("RGBA", (80, 8), (0, 0, 0, 0))
    preview_pixels = preview.load()
    advances: list[int] = []

    for digit, source_id in enumerate(CASH_DIGIT_SOURCE_IDS):
        source = Image.open(source_dir / f"{source_id:03d}.png").convert("RGBA")
        if source.height != 6 or source.width + 2 > 8:
            raise ValueError(
                "unexpected Tyrian TINY_FONT digit canvas: "
                f"sprite {source_id} is {source.size}"
            )
        advances.append(source.width + 1)

        rgba = np.asarray(source, dtype=np.uint8)
        opaque_points = [
            (x + 1, y + 1)
            for y, x in np.argwhere(rgba[:, :, 3] >= 80)
        ]
        values = np.zeros((8, 8), dtype=np.uint8)
        for x, y in opaque_points:
            for offset_x, offset_y in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                values[y + offset_y, x + offset_x] = 1
        for x, y in opaque_points:
            colour = tuple(int(value) for value in rgba[y - 1, x - 1, :3])
            if colour not in source_slots:
                raise ValueError(
                    "unexpected Tyrian TINY_FONT digit colour: "
                    f"sprite {source_id} contains {colour}"
                )
            values[y, x] = source_slots[colour]

        tile_data.extend(encode_gba_4bpp(values))
        for y in range(8):
            for x in range(8):
                value = int(values[y, x])
                if value:
                    colour = palette[value]
                    preview_pixels[digit * 8 + x, y] = (*colour, 255)
    return (
        bytes(tile_data),
        snes.snes_palette_bytes([palette]),
        preview,
        tuple(advances),
    )


def build_gameplay_status_text(
    snes: ModuleType,
    image_root: Path,
    palette_file: Path,
    text: str,
    source_ids: tuple[int, ...],
    brightness: int = -3,
    source_sheet: str = "00_font",
    native_size: bool = False,
) -> tuple[bytes, bytes, Image.Image, tuple[int, ...]]:
    """Recreate a JE_dString label from one stock Tyrian font sheet."""
    source_dir = image_root / "sprites" / source_sheet
    tyrian_palette = load_tyrian_palette(palette_file)
    font_colour_indices = {
        tyrian_palette[index]: index
        for index in range(0x10, 0x20)
    }
    frames: list[Image.Image] = []
    advances: list[int] = []

    for character, source_id in zip(
        text,
        source_ids,
        strict=True,
    ):
        source = Image.open(source_dir / f"{source_id:03d}.png").convert("RGBA")
        if (
            source.width < 1 or
            source.width > 17 or
            source.height < 1 or
            source.height > 15
        ):
            raise ValueError(
                "unexpected Tyrian FONT_SHAPES status glyph canvas: "
                f"{character}/{source_id} is {source.size}"
            )
        advances.append(
            source.width + 1
            if native_size
            else ((source.width + 1) * 3 + 2) // 4
        )
        rgba = np.asarray(source, dtype=np.uint8)
        transformed = np.zeros_like(rgba)
        for y, x in np.argwhere(rgba[:, :, 3] >= 80):
            colour = tuple(int(component) for component in rgba[y, x, :3])
            if colour not in font_colour_indices:
                raise ValueError(
                    "unexpected Tyrian FONT_SHAPES status glyph colour: "
                    f"{character}/{source_id} contains {colour}"
                )
            source_index = font_colour_indices[colour]
            output_index = (
                0xF0 |
                (((source_index & 0x0F) + brightness) & 0x0F)
            )
            transformed[y, x, :3] = tyrian_palette[output_index]
            transformed[y, x, 3] = 255

        if native_size:
            if source.width > 8 or source.height > 13:
                raise ValueError(
                    "native gameplay status glyph exceeds 8x13 OBJ: "
                    f"{character}/{source_id} is {source.size}"
                )
            foreground = Image.fromarray(transformed, "RGBA")
        else:
            # PC 320x200 -> GBA 240x160.  The 11/12x15 FONT_SHAPES glyphs
            # therefore become 8x12 and fit one 8x16 tall OBJ each.
            foreground = Image.fromarray(transformed, "RGBA").resize(
                (
                    min(8, max(1, (source.width * 3 + 2) // 4)),
                    max(1, (source.height * 4 + 2) // 5),
                ),
                Image.Resampling.NEAREST,
            )
        shadow = Image.new("RGBA", (8, 16), (0, 0, 0, 0))
        shadow_mask = foreground.getchannel("A")
        shadow_shape = Image.new("RGBA", foreground.size, (8, 8, 8, 255))
        shadow_shape.putalpha(shadow_mask)
        shadow.alpha_composite(shadow_shape, (1, 2))
        shadow.alpha_composite(foreground, (0, 0))
        frames.append(shadow)

    tile_data, palette = quantize_sprite_frames(snes, frames)
    preview = Image.new(
        "RGBA",
        (sum(advances), 16),
        (0, 0, 0, 0),
    )
    preview_x = 0
    for frame, advance in zip(frames, advances, strict=True):
        preview.alpha_composite(frame, (preview_x, 0))
        preview_x += advance
    return (
        tile_data,
        palette,
        preview,
        tuple(advances),
    )


def build_secret_level_status(
    snes: ModuleType,
    image_root: Path,
    palette_file: Path,
) -> tuple[bytes, bytes, Image.Image, tuple[int, ...]]:
    """Build stable tiles plus the six source flash palettes (-8..-3)."""
    source_dir = image_root / "sprites" / "00_font"
    tyrian_palette = load_tyrian_palette(palette_file)
    font_colour_indices = {
        tyrian_palette[index]: index
        for index in range(0x10, 0x20)
    }
    glyph_sources: list[
        tuple[np.ndarray, np.ndarray, int, int]
    ] = []
    used_nibbles: set[int] = set()
    advances: list[int] = []

    for character, source_id in zip(
        SECRET_LEVEL_UNIQUE_TEXT,
        SECRET_LEVEL_SOURCE_IDS,
        strict=True,
    ):
        source = Image.open(source_dir / f"{source_id:03d}.png").convert(
            "RGBA"
        )
        if (
            source.width < 1 or
            source.width > 17 or
            source.height < 1 or
            source.height > 15
        ):
            raise ValueError(
                "unexpected Tyrian SECRET LEVEL glyph canvas: "
                f"{character}/{source_id} is {source.size}"
            )
        advances.append(((source.width + 1) * 3 + 2) // 4)
        rgba = np.asarray(source, dtype=np.uint8)
        opaque = rgba[:, :, 3] >= 80
        nibbles = np.zeros((source.height, source.width), dtype=np.uint8)
        for y, x in np.argwhere(opaque):
            colour = tuple(int(component) for component in rgba[y, x, :3])
            if colour not in font_colour_indices:
                raise ValueError(
                    "unexpected Tyrian SECRET LEVEL glyph colour: "
                    f"{character}/{source_id} contains {colour}"
                )
            nibble = font_colour_indices[colour] & 0x0F
            nibbles[y, x] = nibble
            used_nibbles.add(nibble)
        target_width = min(
            8,
            max(1, (source.width * 3 + 2) // 4),
        )
        target_height = max(1, (source.height * 4 + 2) // 5)
        scaled_nibbles = np.asarray(
            Image.fromarray(nibbles, "L").resize(
                (target_width, target_height),
                Image.Resampling.NEAREST,
            ),
            dtype=np.uint8,
        )
        scaled_opaque = np.asarray(
            Image.fromarray(
                opaque.astype(np.uint8) * 255,
                "L",
            ).resize(
                (target_width, target_height),
                Image.Resampling.NEAREST,
            ),
            dtype=np.uint8,
        ) >= 80
        glyph_sources.append(
            (
                scaled_nibbles,
                scaled_opaque,
                target_width,
                target_height,
            )
        )

    ordered_nibbles = sorted(used_nibbles)
    if len(ordered_nibbles) > 14:
        raise ValueError("SECRET LEVEL glyphs exceed one 4bpp palette")
    nibble_slots = {
        nibble: index + 2
        for index, nibble in enumerate(ordered_nibbles)
    }
    tile_data = bytearray()
    indexed_frames: list[np.ndarray] = []
    for nibbles, opaque, width, height in glyph_sources:
        values = np.zeros((16, 8), dtype=np.uint8)
        for y, x in np.argwhere(opaque):
            shadow_x = x + 1
            shadow_y = y + 2
            if shadow_x < 8 and shadow_y < 16:
                values[shadow_y, shadow_x] = 1
        for y, x in np.argwhere(opaque):
            values[y, x] = nibble_slots[int(nibbles[y, x])]
        tile_data.extend(encode_gba_4bpp(values[0:8, :]))
        tile_data.extend(encode_gba_4bpp(values[8:16, :]))
        indexed_frames.append(values)

    palette_data = bytearray()
    preview_palette: list[tuple[int, int, int]] = []
    for brightness in range(-8, -2):
        palette = [(0, 0, 0), (8, 8, 8)]
        palette.extend(
            tyrian_palette[
                0xF0 | ((nibble + brightness) & 0x0F)
            ]
            for nibble in ordered_nibbles
        )
        palette_data.extend(snes.snes_palette_bytes([palette]))
        if brightness == -3:
            preview_palette = palette

    preview = Image.new(
        "RGBA",
        (sum(advances), 16),
        (0, 0, 0, 0),
    )
    preview_x = 0
    for values, advance in zip(indexed_frames, advances, strict=True):
        rgba = np.zeros((16, 8, 4), dtype=np.uint8)
        for palette_index, colour in enumerate(preview_palette):
            if palette_index == 0:
                continue
            mask = values == palette_index
            rgba[mask, :3] = colour
            rgba[mask, 3] = 255
        preview.alpha_composite(
            Image.fromarray(rgba, "RGBA"),
            (preview_x, 0),
        )
        preview_x += advance
    return (
        bytes(tile_data),
        bytes(palette_data),
        preview,
        tuple(advances),
    )


def build_enemy_projectiles(
    snes: ModuleType,
    image_root: Path,
) -> tuple[bytes, bytes, Image.Image, tuple[dict[str, int], ...]]:
    """Pack the exact PC projectile graphics used by level 1 and its boss.

    The PC blitter anchors every shot at the top-left of its original
    12-by-up-to-14 canvas. Cropping transparent pixels lets the GBA use its
    8x8/8x16/16x16 OBJ shapes efficiently; generated offsets restore that
    original anchor at render time.
    """
    source_dir = image_root / "sheets" / "08_player_shots"
    sources: list[tuple[int, Image.Image, tuple[int, int, int, int]]] = []

    for source_id in ENEMY_PROJECTILE_SOURCE_IDS:
        source = Image.open(source_dir / f"{source_id:03d}.png").convert("RGBA")
        if source.width != 12 or source.height > 14:
            raise ValueError(
                "unexpected Tyrian projectile source canvas: "
                f"{source_id} is {source.size}, expected 12x<=14"
            )
        bbox = source.getchannel("A").getbbox()
        if bbox is None:
            raise ValueError(f"Tyrian projectile {source_id} is transparent")
        sources.append((source_id, source, bbox))

    source_lookup = {
        source_id: source
        for source_id, source, _ in sources
    }
    source_palette_banks: dict[int, int] = {}
    palette_arrays: dict[int, np.ndarray] = {}
    palette_data = bytearray()
    for expected_bank, (palette_bank, source_ids) in enumerate(
        ENEMY_PROJECTILE_PALETTE_GROUPS,
        start=10,
    ):
        if palette_bank != expected_bank:
            raise ValueError("enemy projectile palette banks must be contiguous")
        opaque_colours: list[np.ndarray] = []
        for source_id in source_ids:
            source = source_lookup[source_id]
            rgba = np.asarray(source, dtype=np.uint8)
            opaque = rgba[:, :, 3] >= 80
            opaque_colours.append(rgba[:, :, :3][opaque])
            source_palette_banks[source_id] = palette_bank
        unique_colours = np.unique(
            np.concatenate(opaque_colours, axis=0),
            axis=0,
        )
        if len(unique_colours) > 15:
            raise ValueError(
                f"projectile palette {palette_bank} needs "
                f"{len(unique_colours)} opaque colours"
            )
        colours = snes.adaptive_palette(np.concatenate(opaque_colours, axis=0))
        palette = [(0, 0, 0)] + colours
        palette_arrays[palette_bank] = np.asarray(
            palette[1:],
            dtype=np.int32,
        )
        palette_data.extend(snes.snes_palette_bytes([palette]))
    if set(source_palette_banks) != set(ENEMY_PROJECTILE_SOURCE_IDS):
        raise ValueError("enemy projectile palette groups are incomplete")

    tile_data = bytearray()
    layouts: list[dict[str, int]] = []
    preview = Image.new(
        "RGBA",
        (16 * len(ENEMY_PROJECTILE_SOURCE_IDS), 16),
        (0, 0, 0, 0),
    )

    for slot, (source_id, source, bbox) in enumerate(sources):
        left, top, right, bottom = bbox
        content_width = right - left
        content_height = bottom - top
        width = 8 if content_width <= 8 else 16
        height = 8 if content_height <= 8 else 16
        frame = Image.new("RGBA", (width, height), (0, 0, 0, 0))
        frame.alpha_composite(source.crop(bbox), (0, 0))
        rgba = np.asarray(frame, dtype=np.uint8)
        opaque = rgba[:, :, 3] >= 80
        values = np.zeros((height, width), dtype=np.uint8)
        pixels = rgba[opaque, :3].astype(np.int32)
        palette_bank = source_palette_banks[source_id]
        palette_array = palette_arrays[palette_bank]
        nearest = (
            ((pixels[:, None, :] - palette_array[None, :, :]) ** 2)
            .sum(axis=2)
            .argmin(axis=1)
            .astype(np.uint8)
            + 1
        )
        values[opaque] = nearest
        start_tile = len(tile_data) // 32
        for tile_y in range(height // 8):
            for tile_x in range(width // 8):
                tile_data.extend(
                    encode_gba_4bpp(
                        values[
                            tile_y * 8 : tile_y * 8 + 8,
                            tile_x * 8 : tile_x * 8 + 8,
                        ]
                    )
                )
        layouts.append({
            "source_id": source_id,
            "start_tile": start_tile,
            "tile_count": (width // 8) * (height // 8),
            "offset_x": left,
            "offset_y": top,
            "width": width,
            "height": height,
            "palette_bank": palette_bank,
        })
        preview.alpha_composite(source, (slot * 16 + 2, 1))

    if len(tile_data) // 32 != 18:
        raise ValueError(
            "level-1 projectile packing changed unexpectedly: "
            f"{len(tile_data) // 32} tiles instead of 18"
        )
    return (
        bytes(tile_data),
        bytes(palette_data),
        preview,
        tuple(layouts),
    )


def repack_obj_tiles(
    snes_tiles: bytes,
    source_metadata: dict[str, int],
    explosion_tiles: bytes,
    reward_tiles: bytes,
    digit_tiles: bytes,
    digit_advances: tuple[int, ...],
    pause_tiles: bytes,
    pause_advances: tuple[int, ...],
    game_over_tiles: bytes,
    game_over_advances: tuple[int, ...],
    secret_level_tiles: bytes,
    secret_level_advances: tuple[int, ...],
    insert_coin_tiles: bytes,
    insert_coin_advances: tuple[int, ...],
    boss_bar_tiles: bytes,
) -> tuple[bytes, dict[str, int]]:
    source_count = len(snes_tiles) // 32
    decoded = [
        decode_snes_4bpp(snes_tiles[index * 32 : index * 32 + 32])
        for index in range(source_count)
    ]
    output = bytearray()
    metadata: dict[str, int] = {}

    def append_asset(
        name: str,
        width_tiles: int,
        height_tiles: int,
        shift: tuple[int, int] = (0, 0),
    ) -> None:
        source_base = source_metadata[f"OBJ_TILE_{name}"]
        metadata[f"OBJ_TILE_{name}"] = len(output) // 32
        metadata[f"OBJ_PAL_{name}"] = source_metadata[f"OBJ_PAL_{name}"]
        canvas = np.zeros(
            (height_tiles * 8, width_tiles * 8),
            dtype=np.uint8,
        )
        for tile_y in range(height_tiles):
            for tile_x in range(width_tiles):
                source_index = (
                    source_base + tile_y * ATLAS_STRIDE_TILES + tile_x
                )
                canvas[
                    tile_y * 8 : tile_y * 8 + 8,
                    tile_x * 8 : tile_x * 8 + 8,
                ] = decoded[source_index]
        shift_x, shift_y = shift
        if shift_x or shift_y:
            shifted = np.zeros_like(canvas)
            source_x = max(0, -shift_x)
            source_y = max(0, -shift_y)
            target_x = max(0, shift_x)
            target_y = max(0, shift_y)
            width = canvas.shape[1] - abs(shift_x)
            height = canvas.shape[0] - abs(shift_y)
            if width <= 0 or height <= 0:
                raise ValueError(f"OBJ anchor shift exceeds canvas: {shift}")
            shifted[
                target_y : target_y + height,
                target_x : target_x + width,
            ] = canvas[
                source_y : source_y + height,
                source_x : source_x + width,
            ]
            canvas = shifted
        for tile_y in range(height_tiles):
            for tile_x in range(width_tiles):
                output.extend(
                    encode_gba_4bpp(
                        canvas[
                            tile_y * 8 : tile_y * 8 + 8,
                            tile_x * 8 : tile_x * 8 + 8,
                        ]
                    )
                )

    # The shared atlas builder crops each alpha bbox before centring.  Undo
    # only that translation for the two retained 24x28 player source cells:
    # graphic 233 needs +0,+1 and graphic 235 needs +1,+1 to match a fixed
    # (4,2) source canvas.  Enemy frames use preserve_sprite_canvas above.
    append_asset("PLAYER_0", 4, 4, (0, 1))
    append_asset("PLAYER_1", 4, 4, (1, 1))
    append_asset("BOSS_0", 8, 8)

    explosion_frame_bytes = 4 * 32
    expected_explosion_bytes = (
        len(EXPLOSION_SOURCE_SEQUENCES)
        * EXPLOSION_FRAMES_PER_SEQUENCE
        * explosion_frame_bytes
    )
    if len(explosion_tiles) != expected_explosion_bytes:
        raise ValueError(
            "unexpected GBA explosion animation size: "
            f"{len(explosion_tiles)} != {expected_explosion_bytes}"
        )
    explosion_base = len(output) // 32
    metadata["OBJ_TILE_EXPLOSION"] = explosion_base
    metadata["OBJ_PAL_EXPLOSION"] = 7
    metadata["OBJ_EXPLOSION_SEQUENCE_COUNT"] = len(
        EXPLOSION_SOURCE_SEQUENCES
    )
    metadata["OBJ_EXPLOSION_FRAME_COUNT"] = EXPLOSION_FRAMES_PER_SEQUENCE
    metadata["OBJ_EXPLOSION_TILES_PER_FRAME"] = 4
    output.extend(explosion_tiles)

    reward_frame_bytes = 4 * 32
    expected_reward_bytes = (
        len(REWARD_SOURCE_SEQUENCES)
        * REWARD_FRAMES_PER_SEQUENCE
        * reward_frame_bytes
    )
    if len(reward_tiles) != expected_reward_bytes:
        raise ValueError(
            "unexpected GBA reward animation size: "
            f"{len(reward_tiles)} != {expected_reward_bytes}"
        )
    metadata["OBJ_TILE_REWARD"] = len(output) // 32
    metadata["OBJ_PAL_REWARD"] = 8
    metadata["OBJ_REWARD_SEQUENCE_COUNT"] = len(REWARD_SOURCE_SEQUENCES)
    metadata["OBJ_REWARD_FRAME_COUNT"] = REWARD_FRAMES_PER_SEQUENCE
    metadata["OBJ_REWARD_TILES_PER_FRAME"] = 4
    output.extend(reward_tiles)

    if len(digit_tiles) != 10 * 32:
        raise ValueError("GBA cash digit bank must contain ten 8x8 tiles")
    if len(digit_advances) != 10:
        raise ValueError("GBA cash digit bank must contain ten advances")
    metadata["OBJ_TILE_SCORE_DIGITS"] = len(output) // 32
    metadata["OBJ_PAL_SCORE_DIGITS"] = 9
    metadata["OBJ_SCORE_DIGIT_COUNT"] = len(digit_advances)
    for digit, advance in enumerate(digit_advances):
        metadata[f"OBJ_SCORE_DIGIT_ADVANCE_{digit}"] = advance
    output.extend(digit_tiles)

    if len(pause_tiles) != len(PAUSE_TEXT) * 2 * 32:
        raise ValueError("GBA PAUSED text must contain two tiles per glyph")
    if len(pause_advances) != len(PAUSE_TEXT):
        raise ValueError("GBA PAUSED text advance count changed")
    metadata["OBJ_TILE_PAUSE_TEXT"] = len(output) // 32
    metadata["OBJ_PAL_PAUSE_TEXT"] = 14
    metadata["OBJ_PAUSE_GLYPH_COUNT"] = len(PAUSE_TEXT)
    for index, advance in enumerate(pause_advances):
        metadata[f"OBJ_PAUSE_ADVANCE_{index}"] = advance
    output.extend(pause_tiles)

    append_asset("PLAYER_SHOT", 2, 2)
    # Keep the former 18-tile range as empty runtime VRAM. Enemy shots are
    # decoded from ROMFS tyrian.shp sections 8/12 into this reserve instead
    # of generating a level-1 projectile atlas.
    output.extend(b"\0" * (18 * 32))
    if len(boss_bar_tiles) != 4 * 32:
        raise ValueError("PC-style boss bar must occupy exactly four OBJ tiles")
    metadata["OBJ_TILE_BOSS_BAR"] = len(output) // 32
    metadata["OBJ_PAL_BOSS_BAR"] = 13
    output.extend(boss_bar_tiles)

    static_tile_count = len(output) // 32
    # Runtime C now decodes ROMFS Sprite2 streams straight into split 8bpp
    # OBJ caches. Keep the cartridge-side static atlas padded to the full
    # hardware window so all time-shared VRAM regions have deterministic
    # backing, without generating any per-enemy frame catalog here.
    output.extend(b"\0" * (1024 * 32 - len(output)))
    expected_game_over_bytes = len(GAME_OVER_TEXT) * 2 * 32
    if len(game_over_tiles) != expected_game_over_bytes:
        raise ValueError(
            "GBA GAME OVER text must contain two tiles per glyph"
        )
    if len(game_over_advances) != len(GAME_OVER_TEXT):
        raise ValueError("GBA GAME OVER text advance count changed")
    game_over_start = GAME_OVER_SOURCE_TILE * 32
    game_over_end = game_over_start + len(game_over_tiles)
    if game_over_end > len(output):
        raise ValueError("GBA GAME OVER source bank exceeds OBJ backing")
    output[game_over_start:game_over_end] = game_over_tiles
    metadata["OBJ_TILE_GAME_OVER_SOURCE"] = GAME_OVER_SOURCE_TILE
    metadata["OBJ_TILE_GAME_OVER_RUNTIME"] = GAME_OVER_RUNTIME_TILE
    metadata["OBJ_PAL_GAME_OVER"] = 14
    metadata["OBJ_GAME_OVER_GLYPH_COUNT"] = len(GAME_OVER_TEXT)
    metadata["OBJ_GAME_OVER_TILE_COUNT"] = len(game_over_tiles) // 32
    metadata["OBJ_GAME_OVER_WORD_GAP"] = GAME_OVER_WORD_GAP
    for index, advance in enumerate(game_over_advances):
        metadata[f"OBJ_GAME_OVER_ADVANCE_{index}"] = advance
    expected_secret_level_bytes = (
        len(SECRET_LEVEL_UNIQUE_TEXT) * 2 * 32
    )
    if len(secret_level_tiles) != expected_secret_level_bytes:
        raise ValueError(
            "GBA SECRET LEVEL text must contain eight unique 8x16 glyphs"
        )
    if len(secret_level_advances) != len(SECRET_LEVEL_UNIQUE_TEXT):
        raise ValueError("GBA SECRET LEVEL advance count changed")
    secret_level_start = SECRET_LEVEL_SOURCE_TILE * 32
    secret_level_end = secret_level_start + len(secret_level_tiles)
    if secret_level_end > len(output):
        raise ValueError("GBA SECRET LEVEL source bank exceeds OBJ backing")
    output[secret_level_start:secret_level_end] = secret_level_tiles
    metadata["OBJ_TILE_SECRET_LEVEL_SOURCE"] = SECRET_LEVEL_SOURCE_TILE
    metadata["OBJ_TILE_SECRET_LEVEL_RUNTIME"] = SECRET_LEVEL_RUNTIME_TILE
    metadata["OBJ_PAL_SECRET_LEVEL"] = 15
    metadata["OBJ_SECRET_LEVEL_UNIQUE_GLYPH_COUNT"] = len(
        SECRET_LEVEL_UNIQUE_TEXT
    )
    metadata["OBJ_SECRET_LEVEL_TILE_COUNT"] = (
        len(secret_level_tiles) // 32
    )
    metadata["OBJ_SECRET_LEVEL_WORD_GAP"] = SECRET_LEVEL_WORD_GAP
    for index, advance in enumerate(secret_level_advances):
        metadata[f"OBJ_SECRET_LEVEL_ADVANCE_{index}"] = advance
    expected_insert_coin_bytes = (
        len(INSERT_COIN_UNIQUE_TEXT) * 2 * 32
    )
    if len(insert_coin_tiles) != expected_insert_coin_bytes:
        raise ValueError(
            "GBA INSERT COIN text must contain eight unique 8x16 glyphs"
        )
    if len(insert_coin_advances) != len(INSERT_COIN_UNIQUE_TEXT):
        raise ValueError("GBA INSERT COIN advance count changed")
    insert_coin_start = INSERT_COIN_SOURCE_TILE * 32
    insert_coin_end = insert_coin_start + len(insert_coin_tiles)
    if insert_coin_end > len(output):
        raise ValueError("GBA INSERT COIN source bank exceeds OBJ backing")
    output[insert_coin_start:insert_coin_end] = insert_coin_tiles
    metadata["OBJ_TILE_INSERT_COIN_SOURCE"] = INSERT_COIN_SOURCE_TILE
    metadata["OBJ_TILE_INSERT_COIN_RUNTIME"] = INSERT_COIN_RUNTIME_TILE
    metadata["OBJ_PAL_INSERT_COIN"] = 14
    metadata["OBJ_INSERT_COIN_UNIQUE_GLYPH_COUNT"] = len(
        INSERT_COIN_UNIQUE_TEXT
    )
    metadata["OBJ_INSERT_COIN_TILE_COUNT"] = len(insert_coin_tiles) // 32
    for index, advance in enumerate(insert_coin_advances):
        metadata[f"OBJ_INSERT_COIN_ADVANCE_{index}"] = advance
    tile_count = len(output) // 32
    if tile_count > 1024:
        raise ValueError(f"GBA OBJ atlas exceeds 1024 tiles: {tile_count}")
    metadata["OBJ_STATIC_TILE_COUNT"] = static_tile_count
    metadata["OBJ_TILE_COUNT"] = tile_count
    return bytes(output), metadata


def reconstruct_gba_window(
    tile_binary: bytes,
    map_binary: bytes,
    palette_binary: bytes,
    row_start: int,
    row_count: int = 20,
) -> Image.Image:
    palettes = np.frombuffer(palette_binary, dtype="<u2").reshape(-1, 16)
    words = np.frombuffer(map_binary, dtype="<u2").reshape(
        -1, GBA_BG_MAP_COLUMNS
    )
    output = Image.new(
        "RGB", (GBA_BG_MAP_WIDTH, row_count * 8), (0, 0, 0)
    )
    pixels = output.load()
    for local_row in range(row_count):
        source_row = min(len(words) - 1, row_start + local_row)
        for tile_x, source_word in enumerate(words[source_row]):
            word = int(source_word)
            tile_index = word & 0x03FF
            palette_index = (word >> 12) & 0x0F
            tile = tile_binary[tile_index * 32 : tile_index * 32 + 32]
            values = np.empty((8, 8), dtype=np.uint8)
            for y in range(8):
                for pair in range(4):
                    packed = tile[y * 4 + pair]
                    values[y, pair * 2] = packed & 0x0F
                    values[y, pair * 2 + 1] = packed >> 4
            if word & (1 << 10):
                values = np.fliplr(values)
            if word & (1 << 11):
                values = np.flipud(values)
            for y in range(8):
                for x in range(8):
                    colour = int(palettes[palette_index, values[y, x]])
                    pixels[tile_x * 8 + x, local_row * 8 + y] = (
                        (colour & 31) << 3,
                        ((colour >> 5) & 31) << 3,
                        ((colour >> 10) & 31) << 3,
                    )
    return output


def pack_pc_background_layer(
    source: Image.Image,
    height: int,
) -> Image.Image:
    """Place an unscaled PC map raster in a 512-pixel GBA tilemap canvas."""
    if source.width > GBA_BG_MAP_WIDTH or source.height > height:
        raise ValueError(
            "PC background does not fit the GBA tilemap canvas: "
            f"source={source.size}, canvas={(GBA_BG_MAP_WIDTH, height)}"
        )
    output = Image.new(
        "RGBA", (GBA_BG_MAP_WIDTH, height), (0, 0, 0, 0)
    )
    output.alpha_composite(source.convert("RGBA"), (0, 0))
    return output


def quantize_gba_background_layer(
    snes: ModuleType,
    image: Image.Image,
    palette_count: int,
) -> tuple[
    bytes,
    bytes,
    list[list[tuple[int, int, int]]],
    dict[str, int],
    np.ndarray,
]:
    """Quantize one 512-wide layer through the 256-wide shared helper.

    Stack the left and right screen blocks vertically for quantization so
    both halves select one common palette and 512-pattern bank, then restore
    the hardware's 64-column row layout.
    """
    if image.width != GBA_BG_MAP_WIDTH or image.height % 8:
        raise ValueError(f"invalid GBA background canvas: {image.size}")
    stacked = Image.new(
        "RGBA", (256, image.height * 2), (0, 0, 0, 0)
    )
    stacked.alpha_composite(image.crop((0, 0, 256, image.height)), (0, 0))
    stacked.alpha_composite(
        image.crop((256, 0, 512, image.height)),
        (0, image.height),
    )
    tiles, stacked_map, palettes, report, assignments = (
        snes.quantize_mode1_layer(stacked, palette_count, 0)
    )
    rows = image.height // 8
    halves = np.frombuffer(stacked_map, dtype="<u2").reshape(rows * 2, 32)
    combined = np.concatenate((halves[:rows], halves[rows:]), axis=1)
    report = dict(report)
    report["rows"] = rows
    return tiles, combined.astype("<u2").tobytes(), palettes, report, assignments


def save_initial_background_preview(
    path: Path,
    tile_binary: bytes,
    map_binary: bytes,
    palette_binary: bytes,
    scroll: int,
    horizontal_offset: int,
) -> None:
    row_start = scroll // 8
    pixel_offset = scroll & 7
    row_count = (pixel_offset + SCREEN_HEIGHT + 7) // 8
    reconstruct_gba_window(
        tile_binary,
        map_binary,
        palette_binary,
        row_start,
        row_count,
    ).crop((
        horizontal_offset,
        pixel_offset,
        horizontal_offset + SCREEN_WIDTH,
        pixel_offset + SCREEN_HEIGHT,
    )).save(path)


def write_signed_pcm_wav(path: Path, pcm: bytes, rate: int) -> None:
    unsigned = bytes(value ^ 0x80 for value in pcm)
    with wave.open(str(path), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(1)
        output.setframerate(rate)
        output.writeframes(unsigned)


def extract_tyrian_sfx_entry(sound_file: Path, index: int) -> bytes:
    data = sound_file.read_bytes()
    count = struct.unpack_from("<H", data, 0)[0]
    if not 0 <= index < count:
        raise ValueError(f"Tyrian SFX index outside archive: {index}/{count}")
    offsets = list(struct.unpack_from(f"<{count}I", data, 2))
    offsets.append(len(data))
    return data[offsets[index] : offsets[index + 1]]


def split_packed_it_rows(packed: bytes, rows: int) -> list[bytes]:
    """Split our deterministic IT packer output into individual row records."""
    records: list[bytes] = []
    offset = 0
    for _ in range(rows):
        start = offset
        while True:
            if offset >= len(packed):
                raise ValueError("truncated packed IT row")
            channel = packed[offset]
            offset += 1
            if channel == 0:
                break
            if not channel & 0x80 or offset >= len(packed):
                raise ValueError("unsupported packed IT channel reuse")
            mask = packed[offset]
            offset += 1
            offset += (
                (1 if mask & 0x01 else 0) +
                (1 if mask & 0x02 else 0) +
                (1 if mask & 0x04 else 0) +
                (2 if mask & 0x08 else 0)
            )
            if offset > len(packed):
                raise ValueError("truncated packed IT cell")
        records.append(packed[start:offset])
    if offset != len(packed):
        raise ValueError("trailing packed IT pattern data")
    return records


def remap_it_position_jumps(
    packed: bytes,
    order_starts: list[int],
) -> bytes:
    """Retarget Bxx order jumps after an oversized pattern is segmented."""
    output = bytearray(packed)
    offset = 0
    while offset < len(output):
        channel = output[offset]
        offset += 1
        if channel == 0:
            continue
        if not channel & 0x80 or offset >= len(output):
            raise ValueError("unsupported packed IT channel reuse")
        mask = output[offset]
        offset += 1
        if mask & 0x01:
            offset += 1
        if mask & 0x02:
            offset += 1
        if mask & 0x04:
            offset += 1
        if mask & 0x08:
            if offset + 1 >= len(output):
                raise ValueError("truncated packed IT effect")
            if output[offset] == 2:
                target = output[offset + 1]
                if target >= len(order_starts):
                    raise ValueError(
                        f"IT Bxx target outside order list: {target}"
                    )
                output[offset + 1] = order_starts[target]
            offset += 2
    return bytes(output)


def disable_it_position_jumps(packed: bytes) -> tuple[bytes, int]:
    """Neutralize source-loop Bxx commands for finite Maxmod cue modules.

    ``MM_PLAY_ONCE`` only controls what Maxmod does after the IT order list
    ends.  A Bxx command inside the module jumps before that boundary and
    therefore loops forever.  Keep the ordinary catalog module untouched for
    Jukebox/level use and build a second, finite module for source cues that
    OpenTyrian lets end naturally.
    """
    output = bytearray(packed)
    offset = 0
    disabled = 0
    while offset < len(output):
        channel = output[offset]
        offset += 1
        if channel == 0:
            continue
        if not channel & 0x80 or offset >= len(output):
            raise ValueError("unsupported packed IT channel reuse")
        mask = output[offset]
        offset += 1
        if mask & 0x01:
            offset += 1
        if mask & 0x02:
            offset += 1
        if mask & 0x04:
            offset += 1
        if mask & 0x08:
            if offset + 1 >= len(output):
                raise ValueError("truncated packed IT effect")
            if output[offset] == 2:
                output[offset] = 0
                output[offset + 1] = 0
                disabled += 1
            offset += 2
        if offset > len(output):
            raise ValueError("truncated packed IT cell")
    return bytes(output), disabled


def build_it_module_with_segmented_patterns(
    original_builder: object,
    workspace: Path,
    name: str,
    samples: list[tuple[str, bytes, int, bool, int]],
    patterns: list[tuple[int, bytes]],
    orders: list[int],
    speed: int = 6,
    tempo: int = 125,
    channel_pans: list[int] | None = None,
) -> bytes:
    """Adapt the shared SNES writer when a TYM intro exceeds 200 rows.

    The IT format limits a pattern to 200 rows.  The shared writer already
    segments loop bodies, but represents the complete pre-loop introduction
    as one pattern.  Several of Tyrian's 41 songs have longer introductions.
    Segment every pattern here, expand repeated orders, and retarget the Bxx
    loop jump without changing any source event or timing.
    """
    if not callable(original_builder):
        raise TypeError("shared IT builder is not callable")

    pattern_chunks: list[list[list[bytes]]] = []
    expanded_pattern_ids: list[list[int]] = []
    next_pattern = 0
    for rows, packed in patterns:
        row_records = split_packed_it_rows(packed, rows)
        chunks = [
            row_records[start : start + 200]
            for start in range(0, rows, 200)
        ]
        pattern_chunks.append(chunks)
        expanded_pattern_ids.append(
            list(range(next_pattern, next_pattern + len(chunks)))
        )
        next_pattern += len(chunks)

    expanded_orders: list[int] = []
    order_starts: list[int] = []
    for pattern_id in orders:
        if pattern_id >= len(expanded_pattern_ids):
            raise ValueError(f"IT order pattern outside table: {pattern_id}")
        order_starts.append(len(expanded_orders))
        expanded_orders.extend(expanded_pattern_ids[pattern_id])
    if len(expanded_orders) > 200:
        raise ValueError(
            f"segmented IT order count out of range: {len(expanded_orders)}"
        )

    expanded_patterns: list[tuple[int, bytes]] = []
    for chunks in pattern_chunks:
        for chunk in chunks:
            packed = remap_it_position_jumps(
                b"".join(chunk),
                order_starts,
            )
            expanded_patterns.append((len(chunk), packed))
    if len(expanded_patterns) > 200:
        raise ValueError(
            "segmented IT pattern count out of range: "
            f"{len(expanded_patterns)}"
        )

    return original_builder(
        workspace,
        name,
        samples,
        expanded_patterns,
        expanded_orders,
        speed,
        tempo,
        channel_pans,
    )


def build_sparse_tym_tracker_it(
    snes: ModuleType,
    workspace: Path,
    tym_path: Path,
    *,
    finite: bool = False,
) -> tuple[bytes, dict[str, object]]:
    """Use the SNES tracker writer with fewer than eight audible channels.

    Its original calibration loader requires exactly eight non-null sources,
    but short Tyrian cues legitimately use fewer (End of Level uses seven).
    Preserve every calibrated source/gain pair and append sentinel sources
    that can never match a TYM event. This changes no audible mapping while
    satisfying the writer's fixed eight-voice interface.
    """
    original_loader = snes.load_snes_calibration
    original_it_builder = snes.build_it_module
    disabled_position_jumps = 0

    def load_sparse_calibration(
        inner_workspace: Path,
        track_number: int,
    ) -> tuple[list[int], list[float]]:
        calibration_path = (
            inner_workspace /
            "org" /
            "TyrianAudioLab" /
            "Music" /
            "channel-calibration.json"
        )
        catalog = json.loads(
            calibration_path.read_text(encoding="utf-8")
        )
        track = next(
            item for item in catalog["tracks"]
            if item["trackNumber"] == track_number
        )
        profile = next(
            item for item in track["profiles"]
            if item["profile"] == "SuperNintendo"
        )
        pairs = [
            (int(source), 10.0 ** (float(db) / 20.0))
            for source, db in zip(
                profile["sourceChannels"][:8],
                profile["gainDb"][:8],
                strict=True,
            )
            if source is not None
        ]
        if not 1 <= len(pairs) <= 8:
            raise ValueError(
                f"track {track_number} has no usable SNES calibration"
            )
        sources = [source for source, _ in pairs]
        gains = [gain for _, gain in pairs]
        sentinel = 0x100
        while len(sources) < 8:
            sources.append(sentinel)
            gains.append(1.0)
            sentinel += 1
        return sources, gains

    def build_segmented_it(
        inner_workspace: Path,
        name: str,
        samples: list[tuple[str, bytes, int, bool, int]],
        patterns: list[tuple[int, bytes]],
        orders: list[int],
        speed: int = 6,
        tempo: int = 125,
        channel_pans: list[int] | None = None,
    ) -> bytes:
        nonlocal disabled_position_jumps

        if finite:
            finite_patterns: list[tuple[int, bytes]] = []
            for rows, packed in patterns:
                packed, disabled = disable_it_position_jumps(packed)
                disabled_position_jumps += disabled
                finite_patterns.append((rows, packed))
            patterns = finite_patterns
        return build_it_module_with_segmented_patterns(
            original_it_builder,
            inner_workspace,
            name,
            samples,
            patterns,
            orders,
            speed,
            tempo,
            channel_pans,
        )

    snes.load_snes_calibration = load_sparse_calibration
    snes.build_it_module = build_segmented_it
    try:
        module, report = snes.build_tym_tracker_it(workspace, tym_path)
        report = dict(report)
        report["finite"] = finite
        report["disabled_position_jumps"] = disabled_position_jumps
        if finite and disabled_position_jumps == 0:
            raise ValueError(
                f"finite cue has no IT Bxx loop to remove: {tym_path.name}"
            )
        return module, report
    finally:
        snes.load_snes_calibration = original_loader
        snes.build_it_module = original_it_builder


def load_default_player_shot(
    hdt_path: Path,
    image_root: Path,
) -> tuple[Image.Image, dict[str, int | str]]:
    """Resolve the requested max-power Pulse Cannon through tyrian.hdt."""
    data = hdt_path.read_bytes()
    weapon_size = 80
    weapon_count = 781
    port_size = 82
    item_base = struct.unpack_from("<i", data, 0)[0] + 14
    port_table = item_base + weapon_count * weapon_size

    # GBA validation keeps stock front port ID 1 but selects power level 11.
    port_offset = port_table + port_size
    name_length = data[port_offset]
    port_name = (
        data[port_offset + 1 : port_offset + 1 + min(name_length, 30)]
        .decode("latin1")
        .rstrip()
    )
    weapon_record = struct.unpack_from(
        "<H",
        data,
        port_offset + 32 + 10 * 2,
    )[0]
    weapon_offset = item_base + weapon_record * weapon_size
    shot_repeat = data[weapon_offset + 2]
    multi = data[weapon_offset + 3]
    animation_frames = struct.unpack_from("<H", data, weapon_offset + 4)[0] + 1
    sequence_max = data[weapon_offset + 6]
    vertical_speed = struct.unpack_from("<b", data, weapon_offset + 34)[0]
    graphic = struct.unpack_from("<H", data, weapon_offset + 58)[0]

    rendered_graphic = graphic % 1000 if graphic > 1000 else graphic
    if rendered_graphic > 500:
        sheet = "12_player_shots2"
        sprite_number = rendered_graphic - 500
    else:
        sheet = "08_player_shots"
        sprite_number = rendered_graphic
    source_path = image_root / "sheets" / sheet / f"{sprite_number:03d}.png"
    if not source_path.is_file():
        raise FileNotFoundError(
            f"Pulse Cannon graphic {graphic} is missing: {source_path}"
        )
    if (
        port_name != "Pulse-Cannon"
        or weapon_record != 165
        or graphic != 62
        or multi != 5
        or sequence_max != 5
    ):
        raise ValueError(
            "unexpected Tyrian max-power Pulse Cannon layout: "
            f"{port_name=}, {weapon_record=}, {graphic=}, "
            f"{multi=}, {sequence_max=}"
        )
    report: dict[str, int | str] = {
        "port_name": port_name,
        "weapon_record": weapon_record,
        "graphic": graphic,
        "sheet": sheet,
        "sprite_number": sprite_number,
        "shot_repeat": shot_repeat,
        "vertical_speed": vertical_speed,
        "animation_frames": animation_frames,
    }
    return Image.open(source_path).convert("RGBA"), report


def write_meta_header(
    output: Path,
    metadata: dict[str, int],
) -> None:
    lines = [
        "#ifndef TYRIAN_GBA_ASSET_META_H",
        "#define TYRIAN_GBA_ASSET_META_H",
        "",
    ]
    lines.extend(f"#define {name} {value}u" for name, value in sorted(metadata.items()))
    lines.extend(("", "#endif", ""))
    (output / "asset_meta.h").write_text("\n".join(lines), encoding="ascii")


def hdt_enemy_table_offset(data: bytes) -> int:
    offset = struct.unpack_from("<i", data, 0)[0] + 14
    for count, record_size in (
        (781, 80),  # weapons
        (43, 82),   # ports
        (47, 37),   # specials
        (7, 37),    # power systems
        (14, 41),   # ships
        (31, 86),   # options
        (11, 37),   # shields
    ):
        offset += count * record_size
    if offset + 851 * 77 != len(data):
        raise ValueError("unexpected tyrian.hdt item/enemy table layout")
    return offset


def read_hdt_enemy_for_frames(
    data: bytes,
    enemy_table: int,
    enemy_id: int,
) -> dict[str, int | tuple[int, ...]]:
    if not 0 <= enemy_id < 851:
        raise ValueError(f"enemy definition outside HDT: {enemy_id}")
    record = data[
        enemy_table + enemy_id * 77 :
        enemy_table + (enemy_id + 1) * 77
    ]
    return {
        "id": enemy_id,
        "animation": record[0],
        "size": record[20],
        "graphics": struct.unpack_from("<20H", record, 21),
        "shape_table": record[63],
        "damaged_graphic": struct.unpack_from("<H", record, 66)[0],
        "launch_type": struct.unpack_from("<H", record, 71)[0] % 1000,
        "value": struct.unpack_from("<h", record, 73)[0],
        "enemy_die": struct.unpack_from("<H", record, 75)[0],
    }


def first_level_random_enemy_ids(level_path: Path) -> tuple[int, ...]:
    data = level_path.read_bytes()
    # OpenTyrian lvlPos[(lvlFileNum - 1) * 2], with first level file number 9.
    table_index = (9 - 1) * 2
    table_offset = 2 + table_index * 4
    if table_offset + 4 > len(data):
        raise ValueError("tyrian1.lvl offset table is truncated")
    level_offset = struct.unpack_from("<I", data, table_offset)[0]
    if level_offset + 10 > len(data):
        raise ValueError("tyrian1.lvl first-level section is truncated")
    enemy_count = struct.unpack_from("<H", data, level_offset + 8)[0]
    start = level_offset + 10
    end = start + enemy_count * 2
    if end > len(data):
        raise ValueError("tyrian1.lvl random enemy pool is truncated")
    return struct.unpack_from(f"<{enemy_count}H", data, start)


def collect_first_level_enemy_definitions(
    nes: ModuleType,
    events: list[tuple[int, int, int, int, int, int, int, int]],
    level_path: Path,
    hdt_data: bytes,
    enemy_table: int,
) -> tuple[list[dict[str, int | tuple[int, ...]]], list[tuple[int, int, int]]]:
    """Close every first-level spawn, launch and death edge over HDT.

    This deliberately scans all 1,009 source records, not only the current
    position-5400 handoff.  The resulting catalog therefore already contains
    the source boss/end-section graphics needed by the next direct-port step.
    """
    enemy_ids = set(first_level_random_enemy_ids(level_path))
    for event in events:
        _, event_type, event_data, _, _, _, _, _ = event
        if event_type in nes.LEVEL_SPAWN_TYPES:
            if event_type == 12:
                enemy_ids.update(event_data + offset for offset in range(4))
            else:
                enemy_ids.add(event_data)
        elif event_type == 33:
            enemy_ids.add(event_data)
            if event_data == 533:
                enemy_ids.update(range(829, 835))

    queue = list(enemy_ids)
    definitions: dict[int, dict[str, int | tuple[int, ...]]] = {}
    while queue:
        enemy_id = queue.pop()
        if not 0 <= enemy_id < 851 or enemy_id in definitions:
            continue
        definition = read_hdt_enemy_for_frames(
            hdt_data, enemy_table, enemy_id
        )
        definitions[enemy_id] = definition
        for child_key in ("launch_type", "enemy_die"):
            child = int(definition[child_key])
            if child and child not in definitions:
                enemy_ids.add(child)
                queue.append(child)

    frame_keys: set[tuple[int, int, int]] = set()
    for definition in definitions.values():
        shape_table = int(definition["shape_table"])
        size = int(definition["size"])
        for graphic in definition["graphics"]:
            graphic = int(graphic)
            if graphic not in (0, 999):
                frame_keys.add((shape_table, graphic, size))
        damaged = int(definition["damaged_graphic"])
        if damaged not in (0, 999):
            frame_keys.add((shape_table, damaged, size))
    return (
        [definitions[key] for key in sorted(definitions)],
        sorted(frame_keys),
    )


def enemy_component_path(
    image_root: Path,
    shape_table: int,
    graphic: int,
) -> Path:
    if shape_table == 21:
        directory = image_root / "sheets" / "11_coins_cubes"
    elif shape_table == 26:
        directory = image_root / "sheets" / "10_powerups"
    else:
        if not 1 <= shape_table <= len(SHAPE_TABLE_CHARACTERS):
            raise ValueError(f"shape table outside OpenTyrian table: {shape_table}")
        character = SHAPE_TABLE_CHARACTERS[shape_table - 1].lower()
        directory = image_root / "sheets_newsh" / f"newsh_{character}"
    path = directory / f"{graphic:03d}.png"
    if not path.is_file():
        raise FileNotFoundError(
            f"source Sprite2 component is missing: table={shape_table}, "
            f"graphic={graphic}, path={path}"
        )
    return path


def compose_exact_enemy_frame(
    snes: ModuleType,
    image_root: Path,
    shape_table: int,
    graphic: int,
    size: int,
) -> Image.Image:
    """Translate JE_drawEnemy()/blit_enemy's exact Sprite2 composition."""
    if size == 1:
        frame = Image.new("RGBA", (24, 28), (0, 0, 0, 0))
        for component, x, y in (
            (graphic, 0, 0),
            (graphic + 1, 12, 0),
            (graphic + 19, 0, 14),
            (graphic + 20, 12, 14),
        ):
            source = snes.normalize_sprite(
                Image.open(
                    enemy_component_path(
                        image_root, shape_table, component
                    )
                ).convert("RGBA")
            )
            frame.alpha_composite(source, (x, y))
        container_offset = (4, 2)
    else:
        frame = snes.normalize_sprite(
            Image.open(
                enemy_component_path(image_root, shape_table, graphic)
            ).convert("RGBA")
        )
        if frame.width > 12 or frame.height > 14:
            raise ValueError(
                "single Sprite2 component exceeds its PC source cell: "
                f"table={shape_table}, graphic={graphic}, size={frame.size}"
            )
        container_offset = (10, 9)
    # A 32x32 GBA OBJ is only the presentation container.  Preserve the
    # complete 24x28 or 12x14 source cell at a fixed offset: cropping each
    # alpha bbox would move transparent source margins and make animation
    # frames jitter even though their OpenTyrian ex/ey is unchanged.
    return preserve_sprite_canvas(
        snes,
        frame,
        (32, 32),
        container_offset,
    )


def build_exact_enemy_frame_catalog(
    snes: ModuleType,
    nes: ModuleType,
    events: list[tuple[int, int, int, int, int, int, int, int]],
    level_path: Path,
    hdt_path: Path,
    palette_path: Path,
    image_root: Path,
) -> tuple[
    bytes,
    bytes,
    dict[int, bytes],
    list[str],
    Image.Image,
]:
    hdt_data = hdt_path.read_bytes()
    enemy_table = hdt_enemy_table_offset(hdt_data)
    definitions, frame_keys = collect_first_level_enemy_definitions(
        nes, events, level_path, hdt_data, enemy_table
    )
    unsupported_tables = sorted(
        {
            shape_table
            for shape_table, _, _ in frame_keys
            if shape_table not in ENEMY_FRAME_PALETTE_GROUPS
        }
    )
    if unsupported_tables:
        raise ValueError(
            f"first-level frame palette mapping is missing: {unsupported_tables}"
        )

    images = {
        key: compose_exact_enemy_frame(snes, image_root, *key)
        for key in frame_keys
    }
    grouped_pixels: dict[int, list[np.ndarray]] = collections.defaultdict(list)
    for key, image in images.items():
        palette_bank = enemy_frame_palette_bank(key)
        rgba = np.asarray(image, dtype=np.uint8)
        mask = rgba[:, :, 3] >= 80
        if mask.any():
            grouped_pixels[palette_bank].append(rgba[mask, :3])

    palette_colours: dict[int, list[tuple[int, int, int]]] = {}
    palette_bytes: dict[int, bytes] = {}
    if not ENEMY_STRUCTURE_FRAME_KEYS.issubset(images):
        missing = sorted(ENEMY_STRUCTURE_FRAME_KEYS.difference(images))
        raise ValueError(
            f"destructible structure palette frames are missing: {missing}"
        )
    for palette_bank in sorted(grouped_pixels):
        pixels = np.concatenate(grouped_pixels[palette_bank], axis=0)
        colours = snes.adaptive_palette(pixels)
        palette = ([(0, 0, 0)] + colours)[:16]
        palette.extend([(0, 0, 0)] * (16 - len(palette)))
        palette_colours[palette_bank] = palette
        palette_bytes[palette_bank] = snes.snes_palette_bytes([palette])

    tyrian_palette = load_tyrian_palette(palette_path)
    filter_palette = [
        (0, 0, 0),
        *[tyrian_palette[0x70 | index] for index in range(1, 16)],
    ]
    palette_bytes[ENEMY_FILTER_PALETTE_BANK] = (
        snes.snes_palette_bytes([filter_palette])
    )

    structure_pixels = np.concatenate(
        grouped_pixels[ENEMY_STRUCTURE_PALETTE_BANK],
        axis=0,
    ).astype(np.float32)
    legacy_table1_pixels = np.concatenate(
        [
            np.asarray(image, dtype=np.uint8)[
                np.asarray(image, dtype=np.uint8)[:, :, 3] >= 80,
                :3,
            ]
            for key, image in images.items()
            if key[0] == 1
        ],
        axis=0,
    )
    legacy_table1_colours = snes.adaptive_palette(
        legacy_table1_pixels
    )

    def palette_rgb_rmse(
        pixels: np.ndarray,
        colours: list[tuple[int, int, int]],
    ) -> float:
        palette_array = np.asarray(colours, dtype=np.float32)
        squared_error = (
            (pixels[:, None, :] - palette_array[None, :, :]) ** 2
        ).sum(axis=2)
        return float(np.sqrt(squared_error.min(axis=1).mean() / 3.0))

    structure_palette_rmse = palette_rgb_rmse(
        structure_pixels,
        palette_colours[ENEMY_STRUCTURE_PALETTE_BANK][1:],
    )
    legacy_structure_palette_rmse = palette_rgb_rmse(
        structure_pixels,
        legacy_table1_colours,
    )
    if structure_palette_rmse >= legacy_structure_palette_rmse:
        raise ValueError(
            "dedicated structure palette did not improve PC-source colour "
            f"error: dedicated={structure_palette_rmse:.4f}, "
            f"shared={legacy_structure_palette_rmse:.4f}"
        )

    tiles = bytearray()
    records = bytearray()
    quantized_previews: list[Image.Image] = []
    audit = [
        "OpenTyrian first-level exact enemy frame catalog",
        f"source_commit={OPENTYRIAN_SOURCE_COMMIT}",
        f"enemy_definitions={len(definitions)}",
        f"frame_count={len(frame_keys)}",
        (
            "structure_palette_dedicated_rgb_rmse="
            f"{structure_palette_rmse:.4f}"
        ),
        (
            "structure_palette_legacy_shared_rgb_rmse="
            f"{legacy_structure_palette_rmse:.4f}"
        ),
        "frame_index,shape_table,graphic,size,palette_bank",
    ]
    for frame_index, key in enumerate(frame_keys):
        shape_table, graphic, size = key
        palette_bank = enemy_frame_palette_bank(key)
        palette = palette_colours[palette_bank]
        palette_array = np.asarray(palette[1:], dtype=np.int32)
        rgba = np.asarray(images[key], dtype=np.uint8)
        mask = rgba[:, :, 3] >= 80
        values = np.zeros((32, 32), dtype=np.uint8)
        if mask.any():
            pixels = rgba[mask, :3].astype(np.int32)
            values[mask] = (
                ((pixels[:, None, :] - palette_array[None, :, :]) ** 2)
                .sum(axis=2)
                .argmin(axis=1)
                .astype(np.uint8)
                + 1
            )
        for tile_y in range(4):
            for tile_x in range(4):
                tiles.extend(
                    encode_gba_4bpp(
                        values[
                            tile_y * 8 : tile_y * 8 + 8,
                            tile_x * 8 : tile_x * 8 + 8,
                        ]
                    )
                )
        preview = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
        preview_pixels = preview.load()
        for y in range(32):
            for x in range(32):
                value = int(values[y, x])
                if value:
                    preview_pixels[x, y] = (*palette[value], 255)
        quantized_previews.append(preview)
        records.extend(
            struct.pack(
                "<BBHBBH",
                shape_table,
                size,
                graphic,
                palette_bank,
                0,
                frame_index,
            )
        )
        audit.append(
            f"{frame_index},{shape_table},{graphic},{size},{palette_bank}"
        )

    if len(tiles) != len(frame_keys) * ENEMY_FRAME_BYTES:
        raise AssertionError("exact enemy frame tile packing changed")
    header = struct.pack(
        "<4sHHHHI",
        ENEMY_FRAME_MAGIC,
        ENEMY_FRAME_VERSION,
        len(frame_keys),
        ENEMY_FRAME_RECORD_BYTES,
        ENEMY_FRAME_TILES,
        len(tiles),
    )
    catalog = header + bytes(records)

    columns = 8
    rows = (len(frame_keys) + columns - 1) // columns
    preview_sheet = Image.new(
        "RGBA", (columns * 64, rows * 48), (16, 16, 20, 255)
    )
    draw = ImageDraw.Draw(preview_sheet)
    for index, (key, preview) in enumerate(
        zip(frame_keys, quantized_previews, strict=True)
    ):
        x = (index % columns) * 64
        y = (index // columns) * 48
        preview_sheet.alpha_composite(preview, (x + 16, y))
        draw.text(
            (x + 1, y + 33),
            f"{key[0]}:{key[1]}/{key[2]}",
            fill=(220, 220, 224, 255),
        )
    return bytes(tiles), catalog, palette_bytes, audit, preview_sheet


def reward_code_for_value(value: int) -> int:
    try:
        return REWARD_VALUES.index(value) + 1
    except ValueError:
        return 0


def encode_gba_level_events(
    nes: ModuleType,
    snes: ModuleType,
    events: list[tuple[int, int, int, int, int, int, int, int]],
    hdt_path: Path,
) -> tuple[bytes, int, int, dict[str, int | str], list[str]]:
    """Add source-HDT cash, reward and three-slot weapon data to GBA spawns.

    OpenTyrian credits each destroyed enemy's positive ``value`` directly,
    and uses ``eenemydie`` only where a separate physical score item is
    authored. Preserve both fields independently. First-level event type 33
    overrides ``eenemydie`` dynamically and is merged in a later pass.

    The old POC discarded ``tur[3]``/``freq[3]`` and replaced them with one
    hand-authored downward shot. Preserve all six HDT bytes per spawn and all
    three frequency bytes from event type 31 so the runtime can execute the PC
    firing slots without guessing.
    """
    source_enemy_ids: list[int] = []
    spawn_specs: list[dict[str, int]] = []
    original_archetype = nes.enemy_archetype

    def mapped_archetype(enemy_id: int) -> int:
        source_enemy_ids.append(enemy_id)
        return snes.SNES_ENEMY_ARCHETYPE.get(enemy_id, 0)

    nes.enemy_archetype = mapped_archetype
    try:
        encoded, spawn_count, control_count = nes.encode_level_events(events)
    finally:
        nes.enemy_archetype = original_archetype
    if len(source_enemy_ids) != spawn_count:
        raise ValueError(
            "source enemy/reward audit does not match encoded spawn count: "
            f"{len(source_enemy_ids)} != {spawn_count}"
        )

    hdt = hdt_path.read_bytes()
    enemy_table = hdt_enemy_table_offset(hdt)

    def scaled_x(value: int) -> int:
        return max(4, min(236, (value * 4 + 2) // 5))

    for (
        event_time,
        event_type,
        event_data,
        event_data_2,
        event_data_3,
        event_data_5,
        event_data_6,
        event_data_4,
    ) in events:
        if event_time >= 4900:
            break
        if event_type not in nes.LEVEL_SPAWN_TYPES:
            continue

        pool = {
            6: 1,
            7: 2,
            10: 3,
            15: 0,
            17: 1,
            18: 0,
            23: 2,
            32: 2,
            49: 1,
            50: 0,
            51: 2,
            52: 3,
            56: 3,
        }.get(event_type, 0)
        fixed_move = event_data_6

        if event_type == 12:
            pool = {
                0: 1,
                1: 1,
                2: 0,
                3: 2,
                4: 3,
            }.get(event_data_6, 1)
            fixed_move = 0
            base_x = scaled_x(event_data_2)
            for enemy_offset, x_add, y_add in (
                (0, 0, 0),
                (1, 24, 0),
                (2, 0, -28),
                (3, 24, -28),
            ):
                spawn_specs.append({
                    "enemy_id": event_data + enemy_offset,
                    "x": base_x + x_add,
                    "y": -28 + event_data_5 + y_add,
                    "pool": pool,
                    "y_speed": event_data_3,
                    "fixed_move": fixed_move,
                    "link": event_data_4,
                })
            continue

        x = scaled_x(event_data_2)
        # IDs 6/7/8/9 and 13/14 are authored as 24-pixel left/right halves
        # of the first-level small tanks.  Scaling each centre independently
        # compressed the pair to 19 pixels and visibly split both tank rows.
        if event_data in (7, 9, 14):
            x = scaled_x(event_data_2 - 24) + 24

        if event_type in (17, 18):
            y = 190 + event_data_5
        elif event_type == 23:
            y = 180 + event_data_5
        elif event_type in (32, 56):
            y = 190
        else:
            y = -28 + event_data_5
        spawn_specs.append({
            "enemy_id": event_data,
            "x": x,
            "y": y,
            "pool": pool,
            "y_speed": event_data_3,
            "fixed_move": fixed_move,
            "link": event_data_4,
        })

    if [spec["enemy_id"] for spec in spawn_specs] != source_enemy_ids:
        raise ValueError("GBA world-coordinate spawn expansion changed source order")

    def enemy_fields(
        enemy_id: int,
    ) -> tuple[
        int,
        int,
        int,
        tuple[int, ...],
        tuple[int, ...],
        int,
        int,
    ]:
        if not 0 <= enemy_id < 851:
            raise ValueError(f"enemy ID outside tyrian.hdt: {enemy_id}")
        offset = enemy_table + enemy_id * 77
        armor = hdt[offset + 19]
        value = struct.unpack_from("<h", hdt, offset + 73)[0]
        enemy_die = struct.unpack_from("<H", hdt, offset + 75)[0]
        turrets = tuple(hdt[offset + 1 : offset + 4])
        frequencies = tuple(hdt[offset + 4 : offset + 7])
        x_move = struct.unpack_from("<b", hdt, offset + 7)[0]
        y_move = struct.unpack_from("<b", hdt, offset + 8)[0]
        return (
            armor,
            value,
            enemy_die,
            turrets,
            frequencies,
            x_move,
            y_move,
        )

    fire_overrides = [
        (
            max(0, min(255, event_data)),
            max(0, min(255, event_data_2)),
            max(0, min(255, event_data_3)),
            event_data_4 & 0xFF,
        )
        for (
            event_time,
            event_type,
            event_data,
            event_data_2,
            event_data_3,
            _,
            _,
            event_data_4,
        ) in events
        if event_time < 4900 and event_type == 31
    ]

    output = bytearray()
    cursor = 0
    spawn_index = 0
    fire_override_index = 0
    reward_counts = [0] * (len(REWARD_VALUES) + 1)
    explicit_hdt_drops = 0
    direct_value_records = 0
    direct_value_authored_total = 0
    used_weapon_ids: set[int] = set()
    while cursor + 1 < len(encoded):
        delta = encoded[cursor]
        opcode = encoded[cursor + 1]
        output.extend((delta, opcode))
        if opcode == nes.EVENT_END:
            cursor += 2
            break
        if opcode == nes.EVENT_WAIT:
            cursor += 2
            continue
        if opcode < 24:
            if cursor + 5 > len(encoded):
                raise ValueError("truncated shared spawn command")
            enemy_id = source_enemy_ids[spawn_index]
            (
                source_armor,
                source_value,
                enemy_die,
                turrets,
                frequencies,
                x_move,
                y_move,
            ) = enemy_fields(enemy_id)
            spec = spawn_specs[spawn_index]
            reward_code = 0
            kill_value = (
                source_value if 0 < source_value < 10000 else 0
            )
            if kill_value:
                direct_value_records += 1
                direct_value_authored_total += kill_value
            if enemy_die:
                target_armor, target_value, _, _, _, _, _ = enemy_fields(
                    enemy_die
                )
                if target_armor == 0 and target_value != 0:
                    reward_code = reward_code_for_value(target_value)
                    if reward_code:
                        explicit_hdt_drops += 1
            output.extend(struct.pack(
                "<hhBbbbBBBH",
                spec["x"],
                spec["y"],
                spec["pool"],
                x_move,
                max(-128, min(127, y_move + spec["y_speed"])),
                spec["fixed_move"],
                source_armor if source_armor else 255,
                spec["link"],
                reward_code,
                kill_value,
            ))
            output.extend(turrets)
            output.extend(frequencies)
            used_weapon_ids.update(weapon for weapon in turrets if weapon)
            reward_counts[reward_code] += 1
            spawn_index += 1
            cursor += 5
            continue
        if opcode == nes.EVENT_FIRE:
            if fire_override_index >= len(fire_overrides):
                raise ValueError("more encoded fire overrides than source records")
            freq1, freq2, freq3, source_link = fire_overrides[
                fire_override_index
            ]
            encoded_link = encoded[cursor + 2]
            if encoded_link != source_link:
                raise ValueError(
                    "fire override link mismatch: "
                    f"encoded={encoded_link}, source={source_link}"
                )
            output.extend((encoded_link, freq1, freq2, freq3))
            fire_override_index += 1
            cursor += 4
            continue
        if opcode in (
            nes.EVENT_MOVE,
            nes.EVENT_ACCEL,
            nes.EVENT_REVERSE,
        ):
            length = 4
        elif opcode == nes.EVENT_FOREGROUND:
            length = 2
        else:
            raise ValueError(f"unknown shared level opcode 0x{opcode:02X}")
        output.extend(encoded[cursor + 2 : cursor + length])
        cursor += length

    if (
        cursor != len(encoded)
        or spawn_index != spawn_count
        or fire_override_index != len(fire_overrides)
    ):
        raise ValueError(
            "GBA HDT bytecode conversion did not consume its source: "
            f"cursor={cursor}/{len(encoded)}, spawns={spawn_index}/{spawn_count}, "
            f"fire={fire_override_index}/{len(fire_overrides)}"
        )
    report = {
        "eligible": sum(reward_counts[1:]),
        "value_25": reward_counts[1],
        "value_50": reward_counts[2],
        "value_75": reward_counts[3],
        "value_100": reward_counts[4],
        "value_250": reward_counts[5],
        "explicit_hdt": explicit_hdt_drops,
        "direct_value_records": direct_value_records,
        "direct_value_authored_total": direct_value_authored_total,
        "weapon_records": ",".join(str(value) for value in sorted(used_weapon_ids)),
        "fire_override_records": len(fire_overrides),
        "world_spawn_records": len(spawn_specs),
        "destructible_assemblies": sum(
            1
            for event in events
            if event[0] < 4900 and event[1] == 12
        ),
        "tank_component_records": sum(
            1
            for spec in spawn_specs
            if spec["enemy_id"] in (6, 7, 8, 9, 13, 14)
        ),
    }
    spawn_counts = collections.Counter(source_enemy_ids)
    audit_lines = [
        "Tyrian GBA first-level enemy projectile audit",
        "enemy_id,spawn_count,tur1,tur2,tur3,freq1,freq2,freq3",
    ]
    for enemy_id in sorted(spawn_counts):
        _, _, _, turrets, frequencies, _, _ = enemy_fields(enemy_id)
        audit_lines.append(
            f"{enemy_id},{spawn_counts[enemy_id]},"
            + ",".join(str(value) for value in (*turrets, *frequencies))
        )
    audit_lines.extend((
        "",
        "projectile_graphics=runtime ROMFS tyrian.shp sections 8/12",
        "enemy_weapon_records="
        + ",".join(str(value) for value in sorted(used_weapon_ids)),
        f"event31_three_slot_records={len(fire_overrides)}",
    ))
    return bytes(output), spawn_count, control_count, report, audit_lines


def add_background_motion_events(
    nes: ModuleType,
    encoded: bytes,
    source_events: list[tuple[int, int, int, int, int, int, int, int]],
    hdt_path: Path,
) -> tuple[bytes, int, dict[str, int | str]]:
    """Merge PC layer motion and dynamic ``enemydie`` controls in source order."""
    event_scroll = 0x85
    event_reward = 0x86
    records: list[tuple[int, int, int, int, bytes]] = []
    cursor = 0
    absolute_time = 0
    encoded_record_index = 0

    source_record_orders: list[tuple[int, int, int]] = []
    for source_index, source_event in enumerate(source_events):
        event_time, event_type = source_event[:2]
        if event_time >= 4900:
            break
        if event_type in nes.LEVEL_SPAWN_TYPES:
            repeat = 4 if event_type == 12 else 1
            source_record_orders.extend(
                (event_time, source_index, sub_order)
                for sub_order in range(repeat)
            )
        elif event_type in nes.LEVEL_CONTROL_TYPES:
            source_record_orders.append((event_time, source_index, 0))

    while cursor + 1 < len(encoded):
        delta = encoded[cursor]
        opcode = encoded[cursor + 1]
        absolute_time += delta
        if opcode == nes.EVENT_END:
            break
        if opcode == nes.EVENT_WAIT:
            cursor += 2
            continue
        if encoded_record_index >= len(source_record_orders):
            raise ValueError("encoded event stream has extra source records")
        source_time, source_index, sub_order = source_record_orders[
            encoded_record_index
        ]
        if source_time != absolute_time:
            raise ValueError(
                "encoded/source event time mismatch: "
                f"{absolute_time} != {source_time}"
            )
        if opcode < 24:
            length = 21
        elif opcode in (
            nes.EVENT_MOVE,
            nes.EVENT_ACCEL,
            nes.EVENT_REVERSE,
        ):
            length = 4
        elif opcode == nes.EVENT_FIRE:
            length = 6
        elif opcode == nes.EVENT_FOREGROUND:
            length = 2
        else:
            raise ValueError(f"unknown shared level opcode 0x{opcode:02X}")
        records.append(
            (
                absolute_time,
                source_index,
                sub_order,
                opcode,
                encoded[cursor + 2 : cursor + length],
            )
        )
        encoded_record_index += 1
        cursor += length
    if encoded_record_index != len(source_record_orders):
        raise ValueError(
            "encoded event stream is missing source records: "
            f"{encoded_record_index} != {len(source_record_orders)}"
        )

    hdt = hdt_path.read_bytes()
    enemy_table = hdt_enemy_table_offset(hdt)

    def reward_target(enemy_id: int) -> tuple[int, int, int]:
        if not 0 <= enemy_id < 851:
            raise ValueError(f"event 33 target outside tyrian.hdt: {enemy_id}")
        offset = enemy_table + enemy_id * 77
        armor = hdt[offset + 19]
        value = struct.unpack_from("<h", hdt, offset + 73)[0]
        code = reward_code_for_value(value) if armor == 0 else 0
        return code, armor, value

    motion_records: list[tuple[int, int, int, int, bytes]] = []
    reward_records: list[tuple[int, int, int, int, bytes]] = []
    reward_target_counts: collections.Counter[int] = collections.Counter()
    cash_reward_records = 0
    reward_audit_lines = [
        "Tyrian GBA first-level dynamic reward audit",
        "event_time,link,target_enemy_id,target_armor,target_value,reward_code",
    ]
    for source_index, (
        event_time,
        event_type,
        event_data,
        event_data_2,
        event_data_3,
        _,
        _,
        event_data_4,
    ) in enumerate(source_events):
        if event_time >= 4900:
            break
        if event_type in (2, 30):
            speeds = (
                max(0, min(7, event_data)),
                max(0, min(7, event_data_2)),
                max(0, min(7, event_data_3)),
            )
            delays = (1, 1)
        elif event_type == 3:
            speeds = (1, 1, 1)
            delays = (3, 2)
        else:
            speeds = None
        if speeds is not None:
            motion_records.append((
                event_time,
                source_index,
                0,
                event_scroll,
                bytes((*speeds, *delays)),
            ))
        if event_type == 33:
            code, armor, value = reward_target(event_data)
            reward_records.append((
                event_time,
                source_index,
                0,
                event_reward,
                bytes((event_data_4 & 0xFF, code)),
            ))
            reward_target_counts[value] += 1
            if code:
                cash_reward_records += 1
            reward_audit_lines.append(
                f"{event_time},{event_data_4},{event_data},"
                f"{armor},{value},{code}"
            )

    merged = motion_records + reward_records + records
    merged.sort(key=lambda record: (record[0], record[1], record[2]))
    output = bytearray()
    time_cursor = 0
    for event_time, _, _, opcode, payload in merged:
        delta = event_time - time_cursor
        while delta > 254:
            output.extend((254, nes.EVENT_WAIT))
            time_cursor += 254
            delta -= 254
        output.extend((delta, opcode))
        output.extend(payload)
        time_cursor = event_time
    output.extend((0, nes.EVENT_END))
    reward_report: dict[str, int | str] = {
        "dynamic_records": len(reward_records),
        "dynamic_cash_records": cash_reward_records,
        "dynamic_non_cash_records": len(reward_records) - cash_reward_records,
        "dynamic_value_25": reward_target_counts[25],
        "dynamic_value_50": reward_target_counts[50],
        "dynamic_value_75": reward_target_counts[75],
        "dynamic_value_100": reward_target_counts[100],
        "dynamic_value_250": reward_target_counts[250],
        "audit": "\n".join(reward_audit_lines) + "\n",
    }
    return bytes(output), len(motion_records), reward_report


def audit_opentyrian_level1_source_data(
    nes: ModuleType,
    events: list[tuple[int, int, int, int, int, int, int, int]],
    hdt_path: Path,
) -> tuple[bytes, bytes, dict[str, int | str], list[str]]:
    """Audit the unmodified first-level records used by the direct C port.

    The returned byte strings exist only long enough to produce deterministic
    hashes and human-readable dependency reports.  v15 no longer writes or
    embeds them: the source-parity runtime reads the same records directly
    from ROMFS tyrian1.lvl and tyrian.hdt.
    """
    event_record_bytes = 11
    packed_events = bytearray(b"OTL1")
    packed_events.extend(struct.pack("<HBB", len(events), event_record_bytes, 1))
    event_audit = [
        "index,eventtime,eventtype,eventdat,eventdat2,"
        "eventdat3,eventdat5,eventdat6,eventdat4"
    ]
    for index, event in enumerate(events):
        (
            event_time,
            event_type,
            event_data,
            event_data_2,
            event_data_3,
            event_data_5,
            event_data_6,
            event_data_4,
        ) = event
        packed_events.extend(struct.pack(
            "<HBhhbbbB",
            event_time,
            event_type,
            event_data,
            event_data_2,
            event_data_3,
            event_data_5,
            event_data_6,
            event_data_4,
        ))
        event_audit.append(
            f"{index},{event_time},{event_type},{event_data},{event_data_2},"
            f"{event_data_3},{event_data_5},{event_data_6},{event_data_4}"
        )

    hdt = hdt_path.read_bytes()
    enemy_table = hdt_enemy_table_offset(hdt)
    enemy_ids: set[int] = set()
    for event in events:
        event_type = event[1]
        enemy_id = event[2]
        if event_type in nes.LEVEL_SPAWN_TYPES:
            if event_type == 12:
                enemy_ids.update(range(enemy_id, enemy_id + 4))
            elif event_type not in (49, 50, 51, 52):
                enemy_ids.add(enemy_id)
        elif event_type == 33:
            enemy_ids.add(enemy_id)

    # Follow the two source-level enemy references.  This includes physical
    # score items, launched enemies and their transitive dependencies.
    pending = list(enemy_ids)
    while pending:
        enemy_id = pending.pop()
        if not 0 <= enemy_id < 851:
            raise ValueError(f"first-level enemy dependency outside HDT: {enemy_id}")
        offset = enemy_table + enemy_id * 77
        launch_frequency = hdt[offset + 70]
        launch_type = struct.unpack_from("<H", hdt, offset + 71)[0]
        enemy_die = struct.unpack_from("<H", hdt, offset + 75)[0]
        dependencies = (
            launch_type if launch_frequency else 0,
            enemy_die,
        )
        for dependency in dependencies:
            if dependency and dependency not in enemy_ids:
                enemy_ids.add(dependency)
                pending.append(dependency)

    enemy_record_bytes = 79
    packed_enemies = bytearray(b"OTE1")
    packed_enemies.extend(struct.pack(
        "<HBB", len(enemy_ids), enemy_record_bytes, 77
    ))
    enemy_audit = [
        "enemy_id,ani,tur1,tur2,tur3,freq1,freq2,freq3,armor,esize,"
        "shapebank,launchfreq,launchtype,value,enemydie"
    ]
    for enemy_id in sorted(enemy_ids):
        offset = enemy_table + enemy_id * 77
        record = hdt[offset : offset + 77]
        packed_enemies.extend(struct.pack("<H", enemy_id))
        packed_enemies.extend(record)
        enemy_audit.append(
            f"{enemy_id},{record[0]},"
            + ",".join(str(value) for value in record[1:7])
            + f",{record[19]},{record[20]},{record[63]},{record[70]},"
            + f"{struct.unpack_from('<H', record, 71)[0]},"
            + f"{struct.unpack_from('<h', record, 73)[0]},"
            + f"{struct.unpack_from('<H', record, 75)[0]}"
        )

    report: dict[str, int | str] = {
        "event_count": len(events),
        "event_record_bytes": event_record_bytes,
        "event_bytes": len(packed_events),
        "event_before_legacy_cutoff": sum(
            event[0] < 4900 for event in events
        ),
        "event_sha256": hashlib.sha256(packed_events).hexdigest(),
        "enemy_count": len(enemy_ids),
        "enemy_record_bytes": enemy_record_bytes,
        "enemy_bytes": len(packed_enemies),
        "enemy_sha256": hashlib.sha256(packed_enemies).hexdigest(),
    }
    audit_lines = [
        "OpenTyrian source-parity first-level export",
        f"source_commit={OPENTYRIAN_SOURCE_COMMIT}",
        *(f"{key}={value}" for key, value in report.items()),
        "",
        "[events]",
        *event_audit,
        "",
        "[enemy_dependencies]",
        *enemy_audit,
    ]
    return bytes(packed_events), bytes(packed_enemies), report, audit_lines


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--workspace", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--preview-dir", type=Path, required=True)
    args = parser.parse_args()

    workspace = args.workspace.resolve()
    output = args.output.resolve()
    preview = args.preview_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    preview.mkdir(parents=True, exist_ok=True)

    snes = load_snes_builder(workspace)
    nes = snes.load_nes_asset_module(workspace)
    image_root = workspace / "org" / "AprCSTyrian" / "image"
    data_root = workspace / "org" / "AprCSTyrian" / "Build" / "data"
    opentyrian_root = workspace / "org" / "opentyrian"
    source_commit = read_git_head(opentyrian_root)
    if source_commit != OPENTYRIAN_SOURCE_COMMIT:
        raise ValueError(
            "OpenTyrian source revision changed; audit the direct port before "
            f"updating {OPENTYRIAN_SOURCE_COMMIT} to {source_commit}"
        )

    (
        frontend_frames,
        frontend_palettes,
        frontend_glyphs,
        frontend_cube,
        frontend_metadata,
        frontend_report,
    ) = build_frontend_mode4_assets(data_root, preview)
    (output / "frontend_frames.bin").write_bytes(frontend_frames)
    (output / "frontend_palettes.bin").write_bytes(frontend_palettes)
    (output / "frontend_glyphs.bin").write_bytes(frontend_glyphs)
    (output / "frontend_cube.bin").write_bytes(frontend_cube)
    (output / "frontend_mode4_audit.txt").write_text(
        "\n".join(frontend_report) + "\n",
        encoding="utf-8",
    )
    (
        jukebox_assets,
        jukebox_metadata,
        jukebox_report,
    ) = build_jukebox_assets(data_root, opentyrian_root, preview)
    for name, data in jukebox_assets.items():
        (output / name).write_bytes(data)

    title = build_title(nes, image_root)
    title.save(preview / "title_gba.png")

    sprite2_raw, sprite2_raw_report = build_sprite2_raw_components(
        data_root
    )
    (output / "sprite2_raw_components.bin").write_bytes(sprite2_raw)
    write_sprite2_raw_header(output, sprite2_raw_report)
    (output / "sprite2_raw_audit.txt").write_text(
        "\n".join(
            f"{key}={value}"
            for key, value in sprite2_raw_report.items()
        ) + "\n",
        encoding="utf-8",
    )

    # Level-specific LVL/HDT/SHP preprocessing intentionally stops here.
    # Every selected level is parsed from the stock files in cartridge
    # ROMFS by src/opentyrian_data.c and src/opentyrian_level_port.c.
    player_shot_source, player_shot_report = load_default_player_shot(
        data_root / "tyrian.hdt",
        image_root,
    )
    player_shot_source.save(preview / "player_shot_062_source.png")
    player_dir = image_root / "sheets" / "09_player_ships"
    player_anchor_boxes = {
        233: (3, 2, 21, 27),
        235: (5, 2, 21, 27),
    }
    for graphic, expected_box in player_anchor_boxes.items():
        actual_box = compose_sprite_2x2(player_dir, graphic).getbbox()
        if actual_box != expected_box:
            raise ValueError(
                "player source alpha anchor changed: "
                f"graphic={graphic}, actual={actual_box}, "
                f"expected={expected_box}"
            )
    snes_obj_tiles, obj_palette, source_metadata, obj_preview = (
        snes.build_obj_assets(nes, image_root, player_shot_source)
    )
    (
        explosion_tiles,
        explosion_palette,
        explosion_preview,
        explosion_composite_preview,
    ) = (
        build_explosion_animation(snes, image_root)
    )
    reward_tiles, reward_palette, reward_preview = build_reward_animation(
        snes, image_root
    )
    (
        digit_tiles,
        digit_palette,
        digit_preview,
        digit_advances,
    ) = build_cash_digits(snes, image_root, data_root / "palette.dat")
    (
        pause_tiles,
        pause_palette,
        pause_preview,
        pause_advances,
    ) = build_gameplay_status_text(
        snes,
        image_root,
        data_root / "palette.dat",
        PAUSE_TEXT,
        PAUSE_TEXT_SOURCE_IDS,
    )
    (
        game_over_tiles,
        game_over_palette,
        game_over_preview,
        game_over_advances,
    ) = build_gameplay_status_text(
        snes,
        image_root,
        data_root / "palette.dat",
        GAME_OVER_TEXT,
        GAME_OVER_TEXT_SOURCE_IDS,
    )
    if game_over_palette != pause_palette:
        raise ValueError("PAUSED and GAME OVER must share one OBJ palette")
    (
        secret_level_tiles,
        secret_level_palettes,
        secret_level_preview,
        secret_level_advances,
    ) = build_secret_level_status(
        snes,
        image_root,
        data_root / "palette.dat",
    )
    (
        insert_coin_tiles,
        insert_coin_palette,
        insert_coin_preview,
        insert_coin_advances,
    ) = build_gameplay_status_text(
        snes,
        image_root,
        data_root / "palette.dat",
        INSERT_COIN_UNIQUE_TEXT,
        INSERT_COIN_SOURCE_IDS,
        source_sheet="01_smallfont",
        native_size=True,
    )
    (
        boss_bar_tiles,
        boss_bar_palette,
        boss_bar_preview,
        boss_bar_flash_colours,
    ) = build_boss_bar_assets(snes, data_root / "palette.dat")
    obj_palette = bytearray(obj_palette).ljust(512, b"\0")
    obj_palette[7 * 32 : 8 * 32] = explosion_palette
    obj_palette[8 * 32 : 9 * 32] = reward_palette
    obj_palette[9 * 32 : 10 * 32] = digit_palette
    obj_palette[13 * 32 : 14 * 32] = boss_bar_palette
    obj_palette[14 * 32 : 15 * 32] = pause_palette
    obj_tiles, obj_metadata = repack_obj_tiles(
        snes_obj_tiles,
        source_metadata,
        explosion_tiles,
        reward_tiles,
        digit_tiles,
        digit_advances,
        pause_tiles,
        pause_advances,
        game_over_tiles,
        game_over_advances,
        secret_level_tiles,
        secret_level_advances,
        insert_coin_tiles,
        insert_coin_advances,
        boss_bar_tiles,
    )
    obj_metadata.update(frontend_metadata)
    obj_metadata.update(jukebox_metadata)
    for flash, (bottom, middle, top) in enumerate(boss_bar_flash_colours):
        obj_metadata[f"BOSS_BAR_FLASH_{flash}_BOTTOM"] = bottom
        obj_metadata[f"BOSS_BAR_FLASH_{flash}_MIDDLE"] = middle
        obj_metadata[f"BOSS_BAR_FLASH_{flash}_TOP"] = top
    for obsolete in (
        output / "enemy_structure_palette.bin",
        output / "enemy_frame_tiles.bin",
        output / "enemy_frame_catalog.bin",
        output / "enemy_frame_audit.csv",
        output / "opentyrian_level1_source_audit.txt",
        output / "reward_drop_audit.txt",
        output / "reward_event33_audit.csv",
        output / "enemy_projectile_audit.txt",
        output / "sprite_mapping_audit.txt",
        preview / "enemy_frames_exact_catalog.png",
    ):
        obsolete.unlink(missing_ok=True)
    (output / "obj_tiles.bin").write_bytes(obj_tiles)
    (output / "obj_palette.bin").write_bytes(obj_palette)
    (output / "secret_level_palettes.bin").write_bytes(
        secret_level_palettes
    )
    (output / "insert_coin_palette.bin").write_bytes(
        insert_coin_palette
    )
    obj_preview.resize((256, 512), Image.Resampling.NEAREST).save(
        preview / "obj_gba_source_atlas.png"
    )
    explosion_preview.resize((384, 288), Image.Resampling.NEAREST).save(
        preview / "explosion_animations_small_air_ground.png"
    )
    explosion_composite_preview.resize(
        (
            explosion_composite_preview.width * 4,
            explosion_composite_preview.height * 4,
        ),
        Image.Resampling.NEAREST,
    ).save(preview / "explosion_large_composite_air_ground.png")
    reward_preview.resize(
        (reward_preview.width * 4, reward_preview.height * 4),
        Image.Resampling.NEAREST,
    ).save(preview / "reward_coins_25_50_75_100_250.png")
    digit_preview.resize(
        (digit_preview.width * 4, digit_preview.height * 4),
        Image.Resampling.NEAREST,
    ).save(preview / "cash_tiny_font_digits.png")
    pause_preview.resize(
        (pause_preview.width * 8, pause_preview.height * 8),
        Image.Resampling.NEAREST,
    ).save(preview / "paused_font_shapes.png")
    game_over_preview.resize(
        (game_over_preview.width * 8, game_over_preview.height * 8),
        Image.Resampling.NEAREST,
    ).save(preview / "game_over_font_shapes.png")
    secret_level_preview.resize(
        (
            secret_level_preview.width * 8,
            secret_level_preview.height * 8,
        ),
        Image.Resampling.NEAREST,
    ).save(preview / "secret_level_font_shapes.png")
    insert_coin_preview.resize(
        (
            insert_coin_preview.width * 8,
            insert_coin_preview.height * 8,
        ),
        Image.Resampling.NEAREST,
    ).save(preview / "insert_coin_small_font_shapes.png")
    (preview / "enemy_projectiles_pc_source.png").unlink(missing_ok=True)
    boss_bar_preview.resize(
        (boss_bar_preview.width * 6, boss_bar_preview.height * 6),
        Image.Resampling.NEAREST,
    ).save(preview / "boss_bar_pc_style.png")

    music_root = workspace / "org" / "TyrianAudioLab" / "Music"
    music_paths = sorted(music_root.glob("[0-9][0-9]_*.tym"))
    if len(music_paths) != JUKEBOX_MUSIC_COUNT:
        raise ValueError(
            "Tyrian TYM catalog changed: "
            f"{len(music_paths)} != {JUKEBOX_MUSIC_COUNT}"
        )
    music_modules: list[bytes] = []
    music_reports: list[dict[str, object]] = []
    for source_index, music_path in enumerate(music_paths):
        expected_number = source_index + 1
        if int(music_path.name[:2]) != expected_number:
            raise ValueError(
                "Tyrian TYM catalog is not contiguous at "
                f"{music_path.name}"
            )
        module, module_report = build_sparse_tym_tracker_it(
            snes,
            workspace,
            music_path,
        )
        if int(module_report["track_number"]) != expected_number:
            raise ValueError(
                "TYM metadata track order changed: "
                f"{music_path.name}"
            )
        (output / f"tyrian_music_{source_index:02d}.it").write_bytes(
            module
        )
        music_modules.append(module)
        music_reports.append(module_report)
    finite_cue_reports: dict[int, dict[str, object]] = {}
    for source_index in (9, 10, 30):
        module, module_report = build_sparse_tym_tracker_it(
            snes,
            workspace,
            music_paths[source_index],
            finite=True,
        )
        (output / f"tyrian_music_{source_index:02d}_once.it").write_bytes(
            module
        )
        finite_cue_reports[source_index] = module_report
    for obsolete in (
        "tyrian_title_full.it",
        "tyrian_level_full.it",
        "tyrian_end_level_full.it",
        "tyrian_game_over_full.it",
    ):
        (output / obsolete).unlink(missing_ok=True)
    title_music = music_modules[29]
    title_report = music_reports[29]
    level_music = music_modules[17]
    level_report = music_reports[17]
    end_level_music = music_modules[9]
    end_level_report = music_reports[9]
    game_over_music = music_modules[10]
    game_over_report = music_reports[10]
    sound_file = data_root / "tyrian.snd"
    voice_file = data_root / "voices.snd"
    ordinary_sound_count = struct.unpack_from(
        "<H",
        sound_file.read_bytes(),
        0,
    )[0]
    voice_sound_count = struct.unpack_from(
        "<H",
        voice_file.read_bytes(),
        0,
    )[0]
    if ordinary_sound_count != 29 or voice_sound_count != 9:
        raise ValueError(
            "Tyrian source sound catalog changed: "
            f"{ordinary_sound_count} ordinary + "
            f"{voice_sound_count} voices"
        )
    for sound_id in range(1, ordinary_sound_count + 1):
        write_signed_pcm_wav(
            output / f"source_sound_{sound_id:02d}.wav",
            extract_tyrian_sfx_entry(sound_file, sound_id - 1),
            11_025,
        )
    for voice_index in range(voice_sound_count):
        pcm = extract_tyrian_sfx_entry(voice_file, voice_index)
        # nortsong.c JE_loadSndFile() removes the corrupt 100-byte tail from
        # every voices.snd entry, not only V_LEVEL_END.
        if len(pcm) < 100:
            raise ValueError(
                f"Tyrian voice {voice_index + 1} is shorter than its trim"
            )
        write_signed_pcm_wav(
            output / (
                f"source_sound_"
                f"{ordinary_sound_count + voice_index + 1:02d}.wav"
            ),
            pcm[:-100],
            11_025,
        )
    for obsolete_sound in (
        "weapon_1.wav",
        "enemy_hit.wav",
        "explosion_9.wav",
        "explosion_11.wav",
        "explosion_22.wav",
        "item.wav",
        "enemy_shot_4.wav",
        "enemy_shot_6.wav",
        "enemy_shot_13.wav",
        "level_complete.wav",
    ):
        (output / obsolete_sound).unlink(missing_ok=True)

    write_meta_header(output, obj_metadata)
    report_lines = [
        "profile=GBA runtime ROMFS Tyrian MAP1 + MAP2 + MAP3",
        f"opentyrian_source_commit={source_commit}",
        *frontend_report,
        *jukebox_report,
        "display_hz=59.7275",
        "logic_hz=34.7826",
        "background_layers=3 (runtime tyrianN.lvl + shapes?.dat + palette.dat)",
        "background_generated_files=0",
        "level_event_source=runtime ROMFS tyrianN.lvl",
        "level_event_generated_files=0",
        "level_enemy_source=runtime ROMFS tyrian.hdt",
        "level_enemy_generated_catalogs=0",
        "level_route_source=runtime ROMFS levelsN.dat",
        "spawn_coordinate_mode=PC initial Y + HDT motion + source pool scroll",
        "reward_source=runtime HDT evalue/eenemydie plus LVL event33",
        f"obj_tiles={len(obj_tiles) // 32}",
        "obj_enemy_archetypes=0 (removed; no gameplay ID aliases)",
        "obj_enemy_preconverted_frames=0",
        "obj_enemy_runtime_source=ROMFS newsh*.shp/tyrian.shp",
        "obj_enemy_runtime_decoder=build-time lossless raw + RLE fallback",
        "obj_enemy_raw_scope=all logical banks and all components",
        (
            "obj_enemy_raw_components="
            f"{sprite2_raw_report['component_count']}"
        ),
        f"obj_enemy_raw_bytes={sprite2_raw_report['raw_bytes']}",
        f"obj_enemy_raw_crc32={sprite2_raw_report['raw_crc32']}",
        (
            "obj_enemy_raw_source_stream_crc32="
            f"{sprite2_raw_report['source_stream_crc32']}"
        ),
        (
            "obj_enemy_raw_roundtrip_components="
            f"{sprite2_raw_report['roundtrip_components']}"
        ),
        "obj_enemy_runtime_format=8bpp 32x32 split VRAM cache",
        "obj_enemy_frame_key=shape_table/egr[enemycycle-1]/size/filter",
        "obj_enemy_large_composition=graphic+0,+1,+19,+20",
        "obj_enemy_anchor=12x14@(10,9),24x28@(4,2) in 32x32 OBJ",
        "obj_enemy_palette=PC palette5 16 hues x 8 brightness levels",
        "obj_enemy_source_scope=all ROMFS Sprite2 banks; not event-catalog limited",
        "boss_bar_source=OpenTyrian event79/draw_boss_bar",
        "boss_bar_pc_geometry=single x155 y7 width51 height6 armor254",
        "boss_bar_gba_geometry=single x96..135 y6..11 centered fill",
        "boss_bar_damage_flash=PC palette indices 117..125",
        f"explosion_animation_sequences={len(EXPLOSION_SOURCE_SEQUENCES)}",
        f"explosion_frames_per_sequence={EXPLOSION_FRAMES_PER_SEQUENCE}",
        "explosion_small_sources=122-133",
        "explosion_air_sources=3-14,41-52,22-33,60-71",
        "explosion_ground_sources=192-203,154-165,211-222,173-184",
        "explosion_anchor_mode=native_top_left",
        "explosion_quadrant_stride=12x14",
        f"explosion_animation_tiles={len(explosion_tiles) // 32}",
        "reward_sources_25=HDT391 keyframes 7,9,11",
        "reward_sources_50=HDT392 keyframes 26,28,30",
        "reward_sources_75=HDT393 keyframes 20,22,24",
        "reward_sources_100=HDT394 keyframes 32,34,36",
        "reward_sources_250=HDT395 keyframes 14,16,18",
        "reward_sprite_outline=none",
        f"reward_animation_tiles={len(reward_tiles) // 32}",
        "cash_digit_source_sprites=79,70-78",
        "cash_digit_style=PC hue 2 brightness 4 FULL_SHADE",
        f"cash_digit_tiles={len(digit_tiles) // 32}",
        "pause_text=PAUSED",
        "pause_text_source=JE_dString FONT_SHAPES hue15 brightness-3",
        "pause_text_source_sprites=15,0,20,18,4,3",
        "pause_text_anchor=PC game_screen (120,90) -> GBA crop (84,78)",
        f"pause_text_tiles={len(pause_tiles) // 32}",
        "enemy_projectile_source=runtime ROMFS tyrian.shp sections 8/12",
        "enemy_projectile_weapon_source=runtime ROMFS tyrian.hdt",
        "enemy_projectile_generated_tiles=0",
        "enemy_projectile_cache=8 runtime 8bpp 16x16 slots",
        "enemy_projectile_anchor=PC Sprite2 top-left",
        "enemy_fire_slots=HDT tur[3]/freq[3] plus event31 three-slot overrides",
        f"player_shot_port={player_shot_report['port_name']}",
        f"player_shot_weapon_record={player_shot_report['weapon_record']}",
        f"player_shot_graphic={player_shot_report['graphic']}",
        f"player_shot_sheet={player_shot_report['sheet']}",
        f"player_shot_sprite_number={player_shot_report['sprite_number']}",
        "player_sprite_anchor=24x28@(4,2) in 32x32 OBJ",
        f"player_shot_repeat={player_shot_report['shot_repeat']}",
        f"player_shot_vertical_speed={player_shot_report['vertical_speed']}",
        f"player_shot_animation_frames={player_shot_report['animation_frames']}",
        f"music_catalog_modules={len(music_modules)}",
        f"music_catalog_it_bytes={sum(map(len, music_modules))}",
        "music_catalog_profile=SuperNintendo calibrated tracker adapter",
        f"title_music_it_bytes={len(title_music)}",
        f"title_music_seconds={title_report['tracker_duration_seconds']:.6f}",
        f"level_music_it_bytes={len(level_music)}",
        f"level_music_pass_seconds={level_report['tracker_duration_seconds']:.6f}",
        f"level_music_laid_out_seconds={level_report['module_play_seconds']:.6f}",
        f"end_level_music_it_bytes={len(end_level_music)}",
        (
            "end_level_music_seconds="
            f"{end_level_report['tracker_duration_seconds']:.6f}"
        ),
        f"game_over_music_it_bytes={len(game_over_music)}",
        (
            "game_over_music_seconds="
            f"{game_over_report['tracker_duration_seconds']:.6f}"
        ),
        "finite_music_cues=9,10,30",
        *[
            (
                f"finite_music_{source_index:02d}_disabled_position_jumps="
                f"{finite_cue_reports[source_index]['disabled_position_jumps']}"
            )
            for source_index in (9, 10, 30)
        ],
        *[
            (
                f"finite_music_{source_index:02d}_it_bytes="
                f"{(output / f'tyrian_music_{source_index:02d}_once.it').stat().st_size}"
            )
            for source_index in (9, 10, 30)
        ],
        (
            "level_complete_voice_pcm_bytes="
            f"{len(extract_tyrian_sfx_entry(voice_file, 4)) - 100}"
        ),
        (
            "audio_sfx_samples="
            f"{ordinary_sound_count + voice_sound_count}"
        ),
    ]
    (output / "asset_report.txt").write_text(
        "\n".join(report_lines) + "\n",
        encoding="utf-8",
    )
    (output / "assets.stamp").write_text(
        "Generated by TyrianGbaPoc/tools/build_assets.py\n",
        encoding="ascii",
    )
    print("\n".join(report_lines))


if __name__ == "__main__":
    main()
