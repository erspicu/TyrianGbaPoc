#!/usr/bin/env python3
"""Build SNES Mode-1, OBJ, event, SNESMOD, and preview assets."""

from __future__ import annotations

import argparse
import collections
import importlib.util
import json
import math
import struct
import wave
import zlib
from pathlib import Path
from types import ModuleType

import numpy as np
from PIL import Image, ImageDraw


MAP_CHUNK_ROWS = 128
BG1_PALETTES = 5
BG2_PALETTES = 3
BG1_ROWS = 1015
BG2_ROWS = 2030
LEVEL_BOSS_TICK = 5400
LEVEL_END_TICK = 5580
MUSIC_SAMPLE_RATE = 9_000
MUSIC_SECONDS = 5.0
TRACKER_TEMPO = 174
TRACKER_SPEED = 1
TRACKER_C5_SPEED = 16_744


# First-level enemy IDs are records in tyrian.hdt, not direct image numbers.
# This table was audited against each record's shapebank/egraphic fields.
# Closely related left/right or consecutive structure pieces share one of the
# 24 SNES visual slots; unrelated shape banks never alias each other.
SNES_ENEMY_ARCHETYPE: dict[int, int] = {
    4: 0,
    3: 1,
    5: 2,
    10: 3,
    12: 3,
    15: 4,
    6: 5,
    13: 5,
    7: 6,
    14: 6,
    8: 7,
    9: 8,
    17: 9,
    24: 10,
    25: 11,
    26: 12,
    28: 12,
    27: 13,
    29: 13,
    30: 14,
    31: 15,
    32: 16,
    33: 16,
    34: 17,
    35: 17,
    36: 18,
    37: 18,
    38: 2,
    39: 3,
    40: 19,
    41: 23,
    42: 23,
    43: 23,
    44: 23,
    45: 23,
}
SNES_ENEMY_ARCHETYPE.update({enemy_id: 21 for enemy_id in range(66, 80)})
SNES_ENEMY_ARCHETYPE.update({enemy_id: 20 for enemy_id in range(121, 129)})
SNES_ENEMY_ARCHETYPE.update({enemy_id: 22 for enemy_id in range(516, 528)})

SNES_ENEMY_REPRESENTATIVES = (
    (1, 159),
    (1, 171),
    (1, 229),
    (1, 191),
    (1, 91),
    (1, 85),
    (1, 87),
    (1, 123),
    (1, 125),
    (1, 267),
    (1, 269),
    (1, 271),
    (1, 49),
    (1, 51),
    (1, 89),
    (1, 127),
    (1, 273),
    (1, 277),
    (1, 281),
    (1, 153),
    (1, 115),
    (2, 153),
    (9, 1),
    (1, 169),
)


def load_nes_asset_module(workspace: Path) -> ModuleType:
    path = workspace / "vendor" / "builders" / "nes" / "build_assets.py"
    spec = importlib.util.spec_from_file_location("tyrian_nes_assets", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"could not load NES asset module: {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def load_hdt_enemy_visuals(
    hdt_path: Path,
) -> dict[int, tuple[int, int]]:
    """Return enemy ID -> (shape bank, first graphic) from tyrian.hdt."""
    data = hdt_path.read_bytes()
    item_offset = struct.unpack_from("<i", data, 0)[0] + 14
    for count, record_size in (
        (781, 80),  # weapons
        (43, 82),   # ports
        (47, 37),   # specials
        (7, 37),    # power systems
        (14, 41),   # ships
        (31, 86),   # options
        (11, 37),   # shields
    ):
        item_offset += count * record_size
    enemy_record_size = 77
    enemy_count = 851
    if item_offset + enemy_count * enemy_record_size != len(data):
        raise ValueError("unexpected tyrian.hdt item/enemy table layout")
    result: dict[int, tuple[int, int]] = {}
    for enemy_id in range(enemy_count):
        offset = item_offset + enemy_id * enemy_record_size
        first_graphic = struct.unpack_from("<H", data, offset + 21)[0]
        shape_bank = data[offset + 63]
        result[enemy_id] = (shape_bank, first_graphic)
    return result


def encode_snes_level_events(
    nes: ModuleType,
    events: list[tuple[int, int, int, int, int, int, int, int]],
) -> tuple[bytes, int, int]:
    """Use the audited 24-entry SNES visual map with the shared bytecode."""
    original = nes.enemy_archetype
    nes.enemy_archetype = lambda enemy_id: SNES_ENEMY_ARCHETYPE.get(enemy_id, 0)
    try:
        return nes.encode_level_events(events)
    finally:
        nes.enemy_archetype = original


def audit_sprite_mapping(
    nes: ModuleType,
    events: list[tuple[int, int, int, int, int, int, int, int]],
    hdt_path: Path,
) -> tuple[list[str], dict[str, int]]:
    visuals = load_hdt_enemy_visuals(hdt_path)
    counts: collections.Counter[int] = collections.Counter()
    for (
        event_time,
        event_type,
        enemy_id,
        _,
        _,
        _,
        _,
        _,
    ) in events:
        if event_time >= 4900:
            break
        if event_type not in nes.LEVEL_SPAWN_TYPES:
            continue
        if event_type == 12:
            counts.update(enemy_id + offset for offset in range(4))
        else:
            counts[enemy_id] += 1

    lines = [
        "Tyrian SNES first-level sprite mapping audit",
        "enemy_id,spawn_count,hdt_bank,hdt_first_graphic,archetype,"
        "representative_bank,representative_graphic,exact_graphic",
    ]
    unknown = 0
    bank_mismatch = 0
    exact_spawns = 0
    for enemy_id in sorted(counts):
        archetype = SNES_ENEMY_ARCHETYPE.get(enemy_id)
        if archetype is None:
            unknown += counts[enemy_id]
            archetype = 0
        source_bank, source_graphic = visuals[enemy_id]
        rep_bank, rep_graphic = SNES_ENEMY_REPRESENTATIVES[archetype]
        if source_bank != rep_bank:
            bank_mismatch += counts[enemy_id]
        exact = source_bank == rep_bank and source_graphic == rep_graphic
        if exact:
            exact_spawns += counts[enemy_id]
        lines.append(
            f"{enemy_id},{counts[enemy_id]},{source_bank},{source_graphic},"
            f"{archetype},{rep_bank},{rep_graphic},{str(exact).lower()}"
        )
    stats = {
        "spawn_records": sum(counts.values()),
        "source_ids": len(counts),
        "unknown_spawns": unknown,
        "bank_mismatch_spawns": bank_mismatch,
        "exact_graphic_spawns": exact_spawns,
    }
    return lines, stats


def encode_snes_4bpp(values: np.ndarray) -> bytes:
    output = bytearray(32)
    flat = values.reshape(8, 8)
    for y in range(8):
        planes = [0, 0, 0, 0]
        for x in range(8):
            value = int(flat[y, x])
            bit = 7 - x
            for plane in range(4):
                planes[plane] |= ((value >> plane) & 1) << bit
        output[y * 2] = planes[0]
        output[y * 2 + 1] = planes[1]
        output[16 + y * 2] = planes[2]
        output[16 + y * 2 + 1] = planes[3]
    return bytes(output)


def snes_palette_bytes(palettes: list[list[tuple[int, int, int]]]) -> bytes:
    output = bytearray()
    for palette in palettes:
        padded = (palette + [(0, 0, 0)] * 16)[:16]
        for red, green, blue in padded:
            word = (red >> 3) | ((green >> 3) << 5) | ((blue >> 3) << 10)
            output.extend(struct.pack("<H", word))
    return bytes(output)


def farthest_centroids(features: np.ndarray, count: int) -> np.ndarray:
    if len(features) == 0:
        return np.zeros((count, 3), dtype=np.float32)
    luminance = features @ np.array([0.299, 0.587, 0.114], dtype=np.float32)
    selected = [int(np.argmin(luminance))]
    while len(selected) < count:
        current = features[selected]
        distance = ((features[:, None, :] - current[None, :, :]) ** 2).sum(axis=2)
        selected.append(int(np.argmax(distance.min(axis=1))))
    return features[selected].astype(np.float32)


def adaptive_palette(pixels: np.ndarray, colour_count: int = 15) -> list[tuple[int, int, int]]:
    if len(pixels) == 0:
        return [(0, 0, 0)] * colour_count
    if len(pixels) > 180_000:
        step = max(1, len(pixels) // 180_000)
        pixels = pixels[::step][:180_000]
    strip = Image.fromarray(pixels.reshape(1, -1, 3).astype(np.uint8), "RGB")
    quantized = strip.quantize(
        colors=colour_count,
        method=Image.Quantize.MEDIANCUT,
        dither=Image.Dither.NONE,
    )
    raw = quantized.getpalette()[: colour_count * 3]
    colours = [
        (raw[index], raw[index + 1], raw[index + 2])
        for index in range(0, len(raw), 3)
    ]
    while len(colours) < colour_count:
        colours.append(colours[-1] if colours else (0, 0, 0))
    colours.sort(key=lambda rgb: (77 * rgb[0] + 150 * rgb[1] + 29 * rgb[2], rgb))
    return colours


def palette_errors(
    tiles: np.ndarray,
    opaque: np.ndarray,
    palette: list[tuple[int, int, int]],
) -> np.ndarray:
    result = np.zeros(len(tiles), dtype=np.float64)
    colours = np.asarray(palette, dtype=np.int16)
    for start in range(0, len(tiles), 256):
        end = min(len(tiles), start + 256)
        pixels = tiles[start:end, :, :3].astype(np.int32)
        diff = pixels[:, :, None, :] - colours.astype(np.int32)[None, None, :, :]
        distance = (diff * diff).sum(axis=3)
        best = distance.min(axis=2)
        best[~opaque[start:end]] = 0
        result[start:end] = best.sum(axis=1)
    return result


def quantize_mode1_layer(
    image: Image.Image,
    palette_count: int,
    palette_base: int,
    max_tiles: int = 512,
) -> tuple[bytes, bytes, list[list[tuple[int, int, int]]], dict[str, int], np.ndarray]:
    rgba = np.asarray(image.convert("RGBA"), dtype=np.uint8)
    if rgba.shape[1] != 256 or rgba.shape[0] % 8:
        raise ValueError(f"Mode-1 layer must be 256 pixels wide and tile aligned: {image.size}")
    rows = rgba.shape[0] // 8
    tiles = (
        rgba.reshape(rows, 8, 32, 8, 4)
        .transpose(0, 2, 1, 3, 4)
        .reshape(rows * 32, 64, 4)
    )
    opaque = tiles[:, :, 3] >= 80
    counts = opaque.sum(axis=1)
    features = np.zeros((len(tiles), 3), dtype=np.float32)
    nonempty = counts > 0
    if np.any(nonempty):
        features[nonempty] = (
            (tiles[nonempty, :, :3].astype(np.float32) * opaque[nonempty, :, None])
            .sum(axis=1)
            / counts[nonempty, None]
        )

    centroids = farthest_centroids(features[nonempty], palette_count)
    assignments = np.zeros(len(tiles), dtype=np.uint8)
    if np.any(nonempty):
        distances = (
            (features[nonempty, None, :] - centroids[None, :, :]) ** 2
        ).sum(axis=2)
        assignments[nonempty] = distances.argmin(axis=1)

    palettes: list[list[tuple[int, int, int]]] = []
    for _ in range(2):
        palettes = []
        errors = np.empty((len(tiles), palette_count), dtype=np.float64)
        for group in range(palette_count):
            chosen = assignments == group
            pixels = tiles[chosen, :, :3][opaque[chosen]]
            palette = adaptive_palette(pixels)
            palettes.append([(0, 0, 0)] + palette)
            errors[:, group] = palette_errors(tiles, opaque, palette)
        assignments = errors.argmin(axis=1).astype(np.uint8)
        assignments[~nonempty] = 0

    patterns = np.zeros((len(tiles), 64), dtype=np.uint8)
    for group in range(palette_count):
        selected = np.flatnonzero(assignments == group)
        if len(selected) == 0:
            continue
        colours = np.asarray(palettes[group][1:], dtype=np.int16)
        for start in range(0, len(selected), 256):
            subset = selected[start : start + 256]
            pixels = tiles[subset, :, :3].astype(np.int32)
            diff = pixels[:, :, None, :] - colours.astype(np.int32)[None, None, :, :]
            distance = (diff * diff).sum(axis=3)
            values = distance.argmin(axis=2).astype(np.uint8) + 1
            values[~opaque[subset]] = 0
            patterns[subset] = values

    packed_patterns = [row.tobytes() for row in patterns]
    frequency = collections.Counter(packed_patterns)
    zero = bytes(64)
    selected_patterns = [zero]
    selected_patterns.extend(
        pattern
        for pattern, _ in frequency.most_common()
        if pattern != zero
    )
    selected_patterns = selected_patterns[:max_tiles]
    pattern_index = {
        pattern: index for index, pattern in enumerate(selected_patterns)
    }
    missing = [pattern for pattern in frequency if pattern not in pattern_index]
    selected_array = np.frombuffer(
        b"".join(selected_patterns), dtype=np.uint8
    ).reshape(len(selected_patterns), 64)
    for start in range(0, len(missing), 32):
        batch = missing[start : start + 32]
        values = np.frombuffer(b"".join(batch), dtype=np.uint8).reshape(len(batch), 64)
        distance = np.count_nonzero(
            values[:, None, :] != selected_array[None, :, :],
            axis=2,
        )
        nearest = distance.argmin(axis=1)
        for pattern, index in zip(batch, nearest, strict=True):
            pattern_index[pattern] = int(index)

    tile_words = np.empty(len(tiles), dtype="<u2")
    for index, pattern in enumerate(packed_patterns):
        tile_words[index] = (
            pattern_index[pattern]
            | ((palette_base + int(assignments[index])) << 10)
        )

    tile_binary = bytearray()
    for pattern in selected_patterns:
        tile_binary.extend(
            encode_snes_4bpp(np.frombuffer(pattern, dtype=np.uint8))
        )
    tile_binary.extend(b"\0" * (max_tiles * 32 - len(tile_binary)))
    report = {
        "rows": rows,
        "source_unique_tiles": len(frequency),
        "packed_tiles": len(selected_patterns),
        "approximated_tiles": len(missing),
        "nonblank_tiles": int(nonempty.sum()),
    }
    return (
        bytes(tile_binary),
        tile_words.tobytes(),
        palettes,
        report,
        np.asarray(selected_patterns, dtype="S64"),
    )


def reconstruct_window(
    tile_binary: bytes,
    map_binary: bytes,
    palettes: list[list[tuple[int, int, int]]],
    row_start: int,
    row_count: int = 28,
) -> Image.Image:
    words = np.frombuffer(map_binary, dtype="<u2").reshape(-1, 32)
    output = Image.new("RGB", (256, row_count * 8), (0, 0, 0))
    pixels = output.load()
    for local_row in range(row_count):
        source_row = min(len(words) - 1, row_start + local_row)
        for tile_x, word_value in enumerate(words[source_row]):
            word = int(word_value)
            tile = tile_binary[(word & 0x1FF) * 32 : (word & 0x1FF) * 32 + 32]
            palette = palettes[(word >> 10) & 7]
            for y in range(8):
                p0, p1 = tile[y * 2 : y * 2 + 2]
                p2, p3 = tile[16 + y * 2 : 18 + y * 2]
                for x in range(8):
                    bit = 7 - x
                    value = (
                        ((p0 >> bit) & 1)
                        | (((p1 >> bit) & 1) << 1)
                        | (((p2 >> bit) & 1) << 2)
                        | (((p3 >> bit) & 1) << 3)
                    )
                    pixels[tile_x * 8 + x, local_row * 8 + y] = palette[value]
    return output


def write_map_chunks(output: Path, prefix: str, data: bytes, rows: int) -> int:
    if len(data) != rows * 64:
        raise ValueError(f"{prefix} map length does not match {rows} rows")
    count = math.ceil(rows / MAP_CHUNK_ROWS)
    for chunk in range(count):
        start = chunk * MAP_CHUNK_ROWS * 64
        payload = data[start : start + MAP_CHUNK_ROWS * 64]
        payload = payload.ljust(MAP_CHUNK_ROWS * 64, b"\0")
        (output / f"{prefix}_map_{chunk:02d}.bin").write_bytes(payload)
    return count


def normalize_sprite(image: Image.Image) -> Image.Image:
    rgba = image.convert("RGBA")
    alpha = np.asarray(rgba.getchannel("A"))
    if alpha.min() == 255:
        array = np.asarray(rgba).copy()
        black = array[:, :, :3].sum(axis=2) < 10
        array[black, 3] = 0
        rgba = Image.fromarray(array, "RGBA")
    return rgba


def fit_sprite(image: Image.Image, size: tuple[int, int]) -> Image.Image:
    image = normalize_sprite(image)
    alpha = image.getchannel("A")
    box = alpha.getbbox()
    if box:
        image = image.crop(box)
    image.thumbnail(size, Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", size, (0, 0, 0, 0))
    canvas.alpha_composite(
        image,
        ((size[0] - image.width) // 2, (size[1] - image.height) // 2),
    )
    return canvas


def procedural_shot(enemy: bool = False) -> Image.Image:
    canvas = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    draw = ImageDraw.Draw(canvas)
    if enemy:
        draw.ellipse((4, 4, 11, 11), fill=(255, 80, 36, 255))
        draw.ellipse((6, 6, 9, 9), fill=(255, 240, 160, 255))
    else:
        draw.polygon(((8, 0), (12, 11), (8, 15), (4, 11)), fill=(64, 220, 255, 255))
        draw.rectangle((7, 3, 8, 12), fill=(255, 255, 255, 255))
    return canvas


def build_obj_assets(
    nes: ModuleType,
    image_root: Path,
    player_shot_source: Image.Image | None = None,
) -> tuple[bytes, bytes, dict[str, int], Image.Image]:
    player_dir = image_root / "sheets" / "09_player_ships"
    newsh2 = image_root / "sheets_newsh" / "newsh_2"
    newsh4 = image_root / "sheets_newsh" / "newsh_4"
    newshe = image_root / "sheets_newsh" / "newsh_e"
    newsh6 = image_root / "sheets_newsh" / "newsh_6"

    # (shape bank, first egraphic) representatives.  The previous table mixed
    # several unrelated HDT IDs (for example 6/8/13/17) into graphic 267.
    # These 24 slots preserve the actual first-level families and keep
    # NEWSH2, NEWSH4 and NEWSHE components in their correct source banks.
    enemies: list[Image.Image] = [
        nes.compose_sprite_2x2(newsh2, 159),
        Image.open(newsh2 / "171.png"),
        nes.compose_sprite_2x2(newsh2, 229),
        nes.compose_sprite_2x2(newsh2, 191),
        nes.compose_sprite_2x2(newsh2, 91),
        nes.compose_sprite_2x2(newsh2, 85),
        nes.compose_sprite_2x2(newsh2, 87),
        nes.compose_sprite_2x2(newsh2, 123),
        nes.compose_sprite_2x2(newsh2, 125),
        nes.compose_sprite_2x2(newsh2, 267),
        nes.compose_sprite_2x2(newsh2, 269),
        nes.compose_sprite_2x2(newsh2, 271),
        nes.compose_sprite_2x2(newsh2, 49),
        nes.compose_sprite_2x2(newsh2, 51),
        nes.compose_sprite_2x2(newsh2, 89),
        nes.compose_sprite_2x2(newsh2, 127),
        nes.compose_sprite_2x2(newsh2, 273),
        nes.compose_sprite_2x2(newsh2, 277),
        nes.compose_sprite_2x2(newsh2, 281),
        nes.compose_sprite_2x2(newsh2, 153),
        nes.compose_sprite_2x2(newsh2, 115),
        nes.compose_sprite_2x2(newsh4, 153),
        nes.compose_sprite_2x2(newshe, 1),
        nes.compose_sprite_2x2(newsh2, 169),
    ]
    if len(enemies) != 24:
        raise AssertionError("SNES first-level atlas must contain 24 archetypes")

    atlas = Image.new("RGBA", (128, 256), (0, 0, 0, 0))
    groups = np.full((256, 128), 255, dtype=np.uint8)
    metadata: dict[str, int] = {}

    def place_block(
        name: str,
        block: int,
        source: Image.Image,
        palette: int,
    ) -> None:
        x = (block % 4) * 32
        y = (block // 4) * 32
        fitted = fit_sprite(source, (32, 32))
        atlas.alpha_composite(fitted, (x, y))
        groups[y : y + 32, x : x + 32] = palette
        metadata[f"OBJ_TILE_{name}"] = (y // 8) * 16 + (x // 8)
        metadata[f"OBJ_PAL_{name}"] = palette

    def place_small(
        name: str,
        x: int,
        y: int,
        source: Image.Image,
        palette: int,
    ) -> None:
        fitted = fit_sprite(source, (16, 16))
        atlas.alpha_composite(fitted, (x, y))
        groups[y : y + 16, x : x + 16] = palette
        metadata[f"OBJ_TILE_{name}"] = (y // 8) * 16 + (x // 8)
        metadata[f"OBJ_PAL_{name}"] = palette

    place_block("PLAYER_0", 0, nes.compose_sprite_2x2(player_dir, 233), 0)
    place_block("PLAYER_1", 1, nes.compose_sprite_2x2(player_dir, 235), 0)
    for index, enemy in enumerate(enemies):
        place_block(f"ENEMY_{index}", index + 2, enemy, 1 + index // 4)

    # One correct 64x64 boss frame leaves enough OBJ patterns for eight more
    # audited enemy families than the old two-frame/16-family atlas.
    boss_x = 64
    boss_y = 192
    boss = fit_sprite(nes.compose_first_level_boss(newsh4, 1), (64, 64))
    atlas.alpha_composite(boss, (boss_x, boss_y))
    groups[boss_y : boss_y + 64, boss_x : boss_x + 64] = 5
    metadata["OBJ_TILE_BOSS_0"] = (boss_y // 8) * 16 + (boss_x // 8)
    metadata["OBJ_PAL_BOSS_0"] = 5

    bar = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    bar_draw = ImageDraw.Draw(bar)
    bar_draw.rectangle((1, 5, 14, 10), fill=(72, 24, 24, 255))
    bar_draw.rectangle((2, 6, 13, 9), fill=(255, 224, 80, 255))
    place_small("EXPLOSION", 0, 224, Image.open(newsh6 / "010.png"), 7)
    place_small(
        "PLAYER_SHOT",
        16,
        224,
        player_shot_source if player_shot_source is not None else procedural_shot(False),
        6,
    )
    place_small("ENEMY_SHOT", 0, 240, procedural_shot(True), 6)
    place_small("BOSS_BAR", 16, 240, bar, 6)

    rgba = np.asarray(atlas, dtype=np.uint8)
    palettes: list[list[tuple[int, int, int]]] = []
    for group in range(8):
        mask = (groups == group) & (rgba[:, :, 3] >= 80)
        colours = adaptive_palette(rgba[:, :, :3][mask])
        palettes.append([(0, 0, 0)] + colours)

    values = np.zeros((256, 128), dtype=np.uint8)
    for group in range(8):
        mask = (groups == group) & (rgba[:, :, 3] >= 80)
        coordinates = np.argwhere(mask)
        if len(coordinates) == 0:
            continue
        pixels = rgba[mask, :3].astype(np.int32)
        colours = np.asarray(palettes[group][1:], dtype=np.int32)
        nearest = (
            ((pixels[:, None, :] - colours[None, :, :]) ** 2)
            .sum(axis=2)
            .argmin(axis=1)
            .astype(np.uint8)
            + 1
        )
        values[coordinates[:, 0], coordinates[:, 1]] = nearest

    tile_data = bytearray()
    for tile_y in range(32):
        for tile_x in range(16):
            tile_data.extend(
                encode_snes_4bpp(
                    values[
                        tile_y * 8 : tile_y * 8 + 8,
                        tile_x * 8 : tile_x * 8 + 8,
                    ]
                )
            )

    preview = Image.new("RGB", atlas.size, (24, 24, 28))
    preview_pixels = preview.load()
    for y in range(256):
        for x in range(128):
            value = int(values[y, x])
            if value:
                preview_pixels[x, y] = palettes[int(groups[y, x])][value]
    return bytes(tile_data), snes_palette_bytes(palettes), metadata, preview


def render_title(nes: ModuleType, image_root: Path) -> Image.Image:
    source = nes.build_title(image_root).crop((0, 8, 256, 232)).convert("RGBA")
    draw = ImageDraw.Draw(source)
    draw.rectangle((0, 157, 255, 223), fill=(0, 0, 0, 255))
    nes.centred_text(draw, 169, "PRESS START", nes.find_font(18), (255, 255, 255, 255))
    nes.centred_text(
        draw,
        204,
        "SNES LOW DETAIL / NORMAL",
        nes.find_font(10),
        (104, 208, 255, 255),
    )
    return source


def read_it_templates(workspace: Path) -> tuple[bytearray, bytearray, bytes]:
    path = (
        workspace
        / "vendor"
        / "builders"
        / "snes"
        / "templates"
        / "pollen8.it"
    )
    data = path.read_bytes()
    order_count, instrument_count, sample_count, _ = struct.unpack_from("<4H", data, 32)
    offset = 192 + order_count
    instrument_offsets = struct.unpack_from(f"<{instrument_count}I", data, offset)
    offset += instrument_count * 4
    sample_offsets = struct.unpack_from(f"<{sample_count}I", data, offset)
    first_instrument = data[instrument_offsets[0] : instrument_offsets[1]]
    first_sample_header = data[sample_offsets[0] : sample_offsets[0] + 80]
    return bytearray(data[:192]), bytearray(first_instrument), first_sample_header


def build_it_module(
    workspace: Path,
    name: str,
    samples: list[tuple[str, bytes, int, bool, int]],
    patterns: list[tuple[int, bytes]],
    orders: list[int],
    speed: int = 6,
    tempo: int = 125,
    channel_pans: list[int] | None = None,
) -> bytes:
    header, instrument_template, sample_template = read_it_templates(workspace)
    count = len(samples)
    if not count or count > 255:
        raise ValueError(f"IT sample count out of range: {count}")
    if not patterns or len(patterns) > 200:
        raise ValueError(f"IT pattern count out of range: {len(patterns)}")
    if not orders or len(orders) > 200:
        raise ValueError(f"IT order count out of range: {len(orders)}")
    header[4:30] = name.encode("ascii", "replace")[:26].ljust(26, b"\0")
    struct.pack_into(
        "<4H",
        header,
        32,
        len(orders),
        count,
        count,
        len(patterns),
    )
    struct.pack_into("<H", header, 46, 0)
    header[48] = 128
    header[49] = 64
    header[50] = speed
    header[51] = tempo
    struct.pack_into("<HI", header, 54, 0, 0)
    for channel in range(64):
        header[64 + channel] = (
            channel_pans[channel]
            if channel_pans is not None and channel < len(channel_pans)
            else (32 if channel < 8 else 128)
        )
        header[128 + channel] = 64 if channel < 8 else 0

    table_size = (
        len(orders) +
        count * 4 +
        count * 4 +
        len(patterns) * 4
    )
    cursor = 192 + table_size
    instruments: list[bytes] = []
    instrument_offsets: list[int] = []
    for sample_number, (sample_name, _, _, _, _) in enumerate(samples, start=1):
        instrument = bytearray(instrument_template)
        instrument[32:58] = sample_name.encode("ascii", "replace")[:26].ljust(26, b"\0")
        instrument[30] = 1
        for note in range(120):
            instrument[64 + note * 2] = note
            instrument[64 + note * 2 + 1] = sample_number
        instrument_offsets.append(cursor)
        instruments.append(bytes(instrument))
        cursor += len(instrument)

    sample_headers: list[bytearray] = []
    sample_offsets: list[int] = []
    for sample_name, pcm, rate, loop, loop_start in samples:
        sample_offsets.append(cursor)
        sample_header = bytearray(sample_template)
        sample_header[4:16] = sample_name.encode("ascii", "replace")[:12].ljust(12, b" ")
        sample_header[20:46] = sample_name.encode("ascii", "replace")[:26].ljust(26, b"\0")
        sample_header[17] = 64
        sample_header[18] = 0x11 if loop else 0x01
        sample_header[19] = 64
        sample_header[46] = 1
        sample_header[47] = 32
        struct.pack_into(
            "<IIIIII",
            sample_header,
            48,
            len(pcm),
            loop_start if loop else 0,
            len(pcm) if loop else 0,
            rate,
            0,
            0,
        )
        sample_headers.append(sample_header)
        cursor += 80

    encoded_patterns: list[bytes] = []
    pattern_offsets: list[int] = []
    for rows, packed_pattern in patterns:
        if rows < 1 or rows > 200:
            raise ValueError(f"IT pattern row count out of range: {rows}")
        pattern = (
            struct.pack("<HHI", len(packed_pattern), rows, 0) +
            packed_pattern
        )
        pattern_offsets.append(cursor)
        encoded_patterns.append(pattern)
        cursor += len(pattern)

    sample_data_offsets: list[int] = []
    for _, pcm, _, _, _ in samples:
        sample_data_offsets.append(cursor)
        cursor += len(pcm)
    for sample_header, pointer in zip(sample_headers, sample_data_offsets, strict=True):
        struct.pack_into("<I", sample_header, 72, pointer)

    output = bytearray(header)
    output.extend(bytes(orders))
    for pointer in instrument_offsets:
        output.extend(struct.pack("<I", pointer))
    for pointer in sample_offsets:
        output.extend(struct.pack("<I", pointer))
    for pointer in pattern_offsets:
        output.extend(struct.pack("<I", pointer))
    for instrument in instruments:
        output.extend(instrument)
    for sample_header in sample_headers:
        output.extend(sample_header)
    for pattern in encoded_patterns:
        output.extend(pattern)
    for _, pcm, _, _, _ in samples:
        output.extend(pcm)
    if len(output) != cursor:
        raise AssertionError(f"IT layout mismatch: {len(output)} != {cursor}")
    return bytes(output)


def build_minimal_it(
    workspace: Path,
    name: str,
    samples: list[tuple[str, bytes, int, bool]],
) -> bytes:
    normalized = [
        (sample_name, pcm, rate, loop, 0)
        for sample_name, pcm, rate, loop in samples
    ]
    packed_pattern = bytearray((0x81, 0x03, 61, 1, 0))
    packed_pattern.extend(b"\0" * 63)
    return build_it_module(
        workspace,
        name,
        normalized,
        [(64, bytes(packed_pattern))],
        [0],
    )


def parse_tym(path: Path) -> dict[str, object]:
    data = path.read_bytes()
    if len(data) < 16 or data[:4] != b"TYM1":
        raise ValueError(f"not a TYM1 file: {path}")
    header_size = struct.unpack_from("<H", data, 6)[0]
    chunk_count = struct.unpack_from("<I", data, 12)[0]
    chunks: dict[str, bytes] = {}
    offset = header_size
    for _ in range(chunk_count):
        if offset > len(data) - 16:
            raise ValueError(f"truncated TYM chunk header: {path}")
        chunk_id = data[offset : offset + 4].decode("ascii")
        size, expected_crc, reserved = struct.unpack_from("<III", data, offset + 4)
        offset += 16
        payload = data[offset : offset + size]
        if reserved or len(payload) != size:
            raise ValueError(f"invalid TYM chunk {chunk_id}: {path}")
        if zlib.crc32(payload) & 0xFFFFFFFF != expected_crc:
            raise ValueError(f"TYM chunk CRC mismatch: {chunk_id}")
        chunks[chunk_id] = payload
        offset += size + (-size & 3)
    if offset != len(data):
        raise ValueError(f"unexpected trailing TYM bytes: {path}")

    instrument_data = chunks["INST"]
    instrument_count, instrument_size = struct.unpack_from(
        "<HH", instrument_data, 0
    )
    if instrument_size != 46:
        raise ValueError("unsupported TYM instrument record size")
    instruments = [
        instrument_data[
            4 + index * instrument_size :
            4 + (index + 1) * instrument_size
        ]
        for index in range(instrument_count)
    ]

    event_data = chunks["EVNT"]
    numerator, denominator, duration, loop_start, event_count = (
        struct.unpack_from("<5I", event_data, 0)
    )
    position = 20
    events: list[tuple[int, dict[int, tuple[int, int, int, int]]]] = []
    for _ in range(event_count):
        tick, mask, reserved = struct.unpack_from("<IHH", event_data, position)
        position += 8
        if reserved:
            raise ValueError("invalid TYM event reserved field")
        changes: dict[int, tuple[int, int, int, int]] = {}
        for channel in range(9):
            if mask & (1 << channel):
                changes[channel] = struct.unpack_from(
                    "<hBBH", event_data, position
                )
                position += 6
        events.append((tick, changes))
    if position != len(event_data):
        raise ValueError("unexpected trailing TYM event bytes")
    return {
        "metadata": json.loads(chunks["META"]),
        "instruments": instruments,
        "events": events,
        "numerator": numerator,
        "denominator": denominator,
        "duration": duration,
        "loop_start": loop_start,
    }


def load_snes_calibration(
    workspace: Path,
    track_number: int,
) -> tuple[list[int], list[float]]:
    path = (
        workspace /
        "vendor" /
        "audio" /
        "Music" /
        "channel-calibration.json"
    )
    catalog = json.loads(path.read_text(encoding="utf-8"))
    track = next(
        item for item in catalog["tracks"]
        if item["trackNumber"] == track_number
    )
    profile = next(
        item for item in track["profiles"]
        if item["profile"] == "SuperNintendo"
    )
    sources = [
        int(source)
        for source in profile["sourceChannels"][:8]
        if source is not None
    ]
    gains = [
        10.0 ** (float(db) / 20.0)
        for db in profile["gainDb"][:8]
    ]
    if len(sources) != 8 or len(set(sources)) != 8 or len(gains) != 8:
        raise ValueError(f"track {track_number} has invalid SNES calibration")
    return sources, gains


def opl_wave(phase: np.ndarray, waveform: int) -> np.ndarray:
    phase = phase - np.floor(phase)
    sine = np.sin(phase * 2.0 * np.pi)
    mode = waveform & 3
    if mode == 1:
        return np.where(sine > 0, sine, 0.0)
    if mode == 2:
        return np.abs(sine) * 2.0 - 1.0
    if mode == 3:
        return np.where(
            phase < 0.25,
            np.sin(phase * 2.0 * np.pi),
            0.0,
        )
    return sine


def synthesize_tym_sample(
    instrument: bytes,
    source: int,
    instrument_index: int,
    percussion_sources: set[int],
    gain_scale: float,
) -> tuple[bytes, int, bool, int]:
    midi_instrument = instrument[40]
    percussion = midi_instrument >= 128 or source in percussion_sources
    if percussion:
        drum_note = (
            midi_instrument - 128
            if midi_instrument >= 128
            else 35 + (instrument_index % 3) * 3
        )
        if drum_note in (35, 36):
            length = 1024
            rate = 11_025
            time = np.arange(length, dtype=np.float64) / rate
            phase = 2.0 * np.pi * (
                115.0 * time -
                52.0 * time * time
            )
            rng = np.random.default_rng(
                0x54594D + source * 257 + instrument_index
            )
            signal = (
                np.sin(phase) * np.exp(-time * 25.0) +
                rng.uniform(-1.0, 1.0, length) *
                np.exp(-time * 60.0) *
                0.16
            )
        elif drum_note in (38, 40):
            length = 1536
            rate = 11_025
            time = np.arange(length, dtype=np.float64) / rate
            rng = np.random.default_rng(
                0x534E52 + source * 257 + instrument_index
            )
            signal = (
                rng.uniform(-1.0, 1.0, length) *
                np.exp(-time * 22.0) *
                0.82 +
                np.sin(2.0 * np.pi * 185.0 * time) *
                np.exp(-time * 32.0) *
                0.28
            )
        else:
            rate = 11_025
            if drum_note in (42, 44):
                length = 768
                decay = 48.0
                difference = 0.78
            elif drum_note == 46:
                length = 2048
                decay = 16.0
                difference = 0.70
            else:
                # Open/crash/ride cymbals in the stock catalog are sparse,
                # long events.  Treating every one as a 46 ms closed hat
                # forced RMS calibration to amplify its transient by several
                # times (most visibly source 7 in track 41).
                length = 4096
                decay = 8.5
                difference = 0.62
            time = np.arange(length, dtype=np.float64) / rate
            rng = np.random.default_rng(
                0x484154 + source * 257 + instrument_index
            )
            noise = rng.uniform(-1.0, 1.0, length)
            signal = np.empty(length, dtype=np.float64)
            signal[0] = noise[0]
            signal[1:] = noise[1:] - noise[:-1] * difference
            signal *= np.exp(-time * decay)
        # The procedural one-shots are a fixed-reference PCM adapter, not a
        # new per-song master.  Remove residual DC and make both boundaries
        # meet silence so Maxmod retriggers/stops cannot create a click.
        dc_blocked = np.empty_like(signal)
        dc_blocked[0] = 0.0
        previous_input = float(signal[0])
        previous_output = 0.0
        for sample_index in range(1, length):
            current_input = float(signal[sample_index])
            current_output = (
                current_input -
                previous_input +
                0.995 * previous_output
            )
            dc_blocked[sample_index] = current_output
            previous_input = current_input
            previous_output = current_output
        signal = dc_blocked
        attack = min(length // 4, max(2, int(round(rate * 0.0015))))
        attack_phase = np.linspace(0.0, 1.0, attack, endpoint=True)
        signal[:attack] *= (
            attack_phase *
            attack_phase *
            (3.0 - 2.0 * attack_phase)
        )
        release = min(length // 4, max(2, int(round(rate * 0.005))))
        release_phase = np.linspace(1.0, 0.0, release, endpoint=True)
        signal[-release:] *= (
            release_phase *
            release_phase *
            (3.0 - 2.0 * release_phase)
        )
        peak = max(1e-9, float(np.max(np.abs(signal))))
        pcm = np.clip(
            np.rint(signal / peak * 118.0 * gain_scale),
            -128,
            127,
        ).astype(np.int8)
        return pcm.tobytes(), rate, False, 0

    phase = (np.arange(320, dtype=np.float64) % 64) / 64.0
    mod_level = 10.0 ** (-(instrument[1] & 0x3F) * 0.75 / 20.0)
    modulation = (
        0.2 +
        mod_level * (1.0 + (instrument[10] & 7) * 0.45)
    )
    modulator = opl_wave(phase, instrument[4])
    signal = opl_wave(
        phase + modulator * modulation / (2.0 * np.pi),
        instrument[9],
    )
    signal -= float(np.mean(signal))
    peak = max(1e-9, float(np.max(np.abs(signal))))
    pcm = np.clip(
        np.rint(signal / peak * 118.0 * gain_scale),
        -128,
        127,
    ).astype(np.int8)
    return pcm.tobytes(), TRACKER_C5_SPEED, True, 64


def pack_it_pattern(
    rows: int,
    cells: dict[int, dict[int, dict[str, int]]],
) -> bytes:
    packed = bytearray()
    for row in range(rows):
        for channel in sorted(cells.get(row, {})):
            cell = cells[row][channel]
            mask = 0
            if "note" in cell:
                mask |= 0x01
            if "instrument" in cell:
                mask |= 0x02
            if "volume" in cell:
                mask |= 0x04
            if "effect" in cell:
                mask |= 0x08
            packed.extend((0x80 | (channel + 1), mask))
            if mask & 0x01:
                packed.append(cell["note"])
            if mask & 0x02:
                packed.append(cell["instrument"])
            if mask & 0x04:
                packed.append(cell["volume"])
            if mask & 0x08:
                packed.extend((cell["effect"], cell["parameter"]))
        packed.append(0)
    return bytes(packed)


def build_tym_tracker_it(
    workspace: Path,
    tym_path: Path,
) -> tuple[bytes, dict[str, object]]:
    song = parse_tym(tym_path)
    metadata = song["metadata"]
    if not isinstance(metadata, dict):
        raise TypeError("TYM metadata must be an object")
    track_number = int(metadata["trackNumber"])
    sources, voice_gains = load_snes_calibration(workspace, track_number)
    instruments = song["instruments"]
    events = song["events"]
    if not isinstance(instruments, list) or not isinstance(events, list):
        raise TypeError("invalid parsed TYM collections")
    percussion_sources = {
        int(source)
        for source in metadata["arrangement"]["percussionSources"]
    }

    used_pairs: set[tuple[int, int]] = set()
    source_to_voice = {
        source: voice for voice, source in enumerate(sources)
    }
    for _, changes in events:
        for source, state in changes.items():
            if source in source_to_voice and state[0] != -32768:
                used_pairs.add((source_to_voice[source], state[1]))
    ordered_pairs = sorted(used_pairs)
    if len(ordered_pairs) > 255:
        raise ValueError("TYM tracker needs more than 255 voice/instrument pairs")
    pair_to_instrument = {
        pair: index + 1 for index, pair in enumerate(ordered_pairs)
    }

    maximum_gain = max(1.0, max(voice_gains))
    samples: list[tuple[str, bytes, int, bool, int]] = []
    for voice, instrument_index in ordered_pairs:
        source = sources[voice]
        pcm, rate, loop, loop_start = synthesize_tym_sample(
            instruments[instrument_index],
            source,
            instrument_index,
            percussion_sources,
            voice_gains[voice] / maximum_gain,
        )
        samples.append((
            f"v{voice}i{instrument_index:02d}",
            pcm,
            rate,
            loop,
            loop_start,
        ))

    absolute_cells: dict[int, dict[int, dict[str, int]]] = {}
    for tick, changes in events:
        for source, state in changes.items():
            voice = source_to_voice.get(source)
            if voice is None:
                continue
            pitch_q8, instrument_index, velocity, _ = state
            cell: dict[str, int] = {}
            if pitch_q8 == -32768:
                cell["note"] = 255
            else:
                percussion = (
                    instruments[instrument_index][40] >= 128 or
                    source in percussion_sources
                )
                cell["note"] = (
                    61
                    if percussion
                    else max(
                        1,
                        min(120, int(round(pitch_q8 / 256.0)) + 1),
                    )
                )
                cell["instrument"] = pair_to_instrument[
                    (voice, instrument_index)
                ]
                attenuation_db = (
                    63 - max(0, min(63, velocity))
                ) * 0.75
                cell["volume"] = max(
                    1,
                    min(
                        64,
                        int(round(
                            64.0 *
                            10.0 ** (-attenuation_db / 20.0)
                        )),
                    ),
                )
            absolute_cells.setdefault(tick, {})[voice] = cell

    duration = int(song["duration"])
    loop_start = int(song["loop_start"])
    # SNESMOD 1.4 continues playing Bxx jumps correctly, but after several
    # minutes its synchronous stop/load command can fail to acknowledge.
    # Level 1 lasts under four source-song passes, so lay those passes out in
    # the order list and never execute Bxx before the return to title.
    unrolled_loops = 4 if track_number == 18 else 1
    rows_per_pattern = 128 if unrolled_loops > 1 else 64
    segments: list[tuple[int, int]] = []
    if loop_start:
        segments.append((0, loop_start))
    for start in range(loop_start, duration, rows_per_pattern):
        segments.append((start, min(duration, start + rows_per_pattern)))
    loop_order = 1 if loop_start else 0
    if unrolled_loops == 1:
        final_tick = duration - 1
        absolute_cells.setdefault(final_tick, {}).setdefault(0, {}).update({
            "effect": 2,  # IT/SNESMOD Bxx: position jump
            "parameter": loop_order,
        })

    patterns: list[tuple[int, bytes]] = []
    for start, end in segments:
        local_cells = {
            tick - start: voices
            for tick, voices in absolute_cells.items()
            if start <= tick < end
        }
        patterns.append((
            end - start,
            pack_it_pattern(end - start, local_cells),
        ))
    if unrolled_loops > 1:
        intro_orders = [0] if loop_start else []
        loop_orders = list(range(loop_order, len(patterns)))
        orders = intro_orders + loop_orders * unrolled_loops
    else:
        orders = list(range(len(patterns)))
    pans = [23, 41, 28, 36, 26, 38, 32, 32]
    module = build_it_module(
        workspace,
        f"Tyrian {track_number:02d} Full",
        samples,
        patterns,
        orders,
        TRACKER_SPEED,
        TRACKER_TEMPO,
        pans,
    )
    source_rate = float(song["numerator"]) / float(song["denominator"])
    tracker_rate = TRACKER_TEMPO / (2.5 * TRACKER_SPEED)
    report: dict[str, object] = {
        "track_number": track_number,
        "title": metadata["title"],
        "source_ticks": duration,
        "loop_start_tick": loop_start,
        "source_tick_rate": source_rate,
        "source_duration_seconds": duration / source_rate,
        "tracker_row_rate": tracker_rate,
        "tracker_duration_seconds": duration / tracker_rate,
        "patterns": len(patterns),
        "orders": len(orders),
        "unrolled_loops": unrolled_loops,
        "module_play_seconds": (
            loop_start +
            (duration - loop_start) * unrolled_loops
        ) / tracker_rate,
        "samples": len(samples),
        "source_channels": ",".join(str(source) for source in sources),
        "it_bytes": len(module),
    }
    return module, report


def extract_tyrian_sfx(sound_file: Path) -> list[tuple[str, bytes, int, bool]]:
    data = sound_file.read_bytes()
    count = struct.unpack_from("<H", data, 0)[0]
    offsets = list(struct.unpack_from(f"<{count}I", data, 2))
    offsets.append(len(data))
    choices = (
        ("weapon_1", 0),
        ("enemy_hit", 2),
        ("explosion_9", 8),
    )
    result = []
    for name, index in choices:
        pcm = data[offsets[index] : offsets[index + 1]]
        result.append((name, pcm, 11_025, False))
    return result


def build_music_excerpt(wav_path: Path) -> bytes:
    with wave.open(str(wav_path), "rb") as source:
        channels = source.getnchannels()
        width = source.getsampwidth()
        source_rate = source.getframerate()
        frames = source.readframes(source.getnframes())
    if width != 2:
        raise ValueError("TyrianAudioLab SNES WAV must be signed 16-bit PCM")
    pcm = np.frombuffer(frames, dtype="<i2").astype(np.float64)
    pcm = pcm.reshape(-1, channels).mean(axis=1)
    source_count = min(len(pcm), int(MUSIC_SECONDS * source_rate))
    pcm = pcm[:source_count]
    output_count = int(round(len(pcm) * MUSIC_SAMPLE_RATE / source_rate))
    source_x = np.linspace(0.0, 1.0, len(pcm), endpoint=False)
    output_x = np.linspace(0.0, 1.0, output_count, endpoint=False)
    resampled = np.interp(output_x, source_x, pcm)
    peak = max(1.0, float(np.max(np.abs(resampled))))
    resampled *= 118.0 / peak
    fade = min(MUSIC_SAMPLE_RATE // 80, len(resampled) // 4)
    if fade:
        curve = np.linspace(0.0, 1.0, fade, endpoint=True)
        resampled[:fade] *= curve
        resampled[-fade:] *= curve[::-1]
    return np.clip(np.rint(resampled), -128, 127).astype(np.int8).tobytes()


def write_meta_header(
    output: Path,
    metadata: dict[str, int],
    event_bytes: int,
) -> None:
    lines = [
        "#ifndef TYRIAN_SNES_ASSET_META_H",
        "#define TYRIAN_SNES_ASSET_META_H",
        "",
        f"#define BG1_ROWS {BG1_ROWS}u",
        f"#define BG2_ROWS {BG2_ROWS}u",
        f"#define BG1_CHUNKS {math.ceil(BG1_ROWS / MAP_CHUNK_ROWS)}u",
        f"#define BG2_CHUNKS {math.ceil(BG2_ROWS / MAP_CHUNK_ROWS)}u",
        f"#define LEVEL_EVENT_BYTES {event_bytes}u",
        f"#define LEVEL_BOSS_TICK {LEVEL_BOSS_TICK}u",
        f"#define LEVEL_END_TICK {LEVEL_END_TICK}u",
        f"#define ORIGINAL_LOGIC_NUMERATOR 1193182ul",
        f"#define ORIGINAL_LOGIC_DENOMINATOR 2058240ul",
        "",
    ]
    lines.extend(f"#define {name} {value}u" for name, value in sorted(metadata.items()))
    lines.extend(("", "#endif", ""))
    (output / "asset_meta.h").write_text("\n".join(lines), encoding="ascii")


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
    nes = load_nes_asset_module(workspace)
    image_root = workspace / "vendor" / "tyrian" / "image"
    data_root = workspace / "vendor" / "tyrian" / "data"

    title_source = render_title(nes, image_root)
    title_tiles, title_map, title_palettes, title_report, _ = quantize_mode1_layer(
        title_source, 8, 0
    )
    (output / "title_tiles.bin").write_bytes(title_tiles)
    (output / "title_map.bin").write_bytes(title_map)
    (output / "title_palette.bin").write_bytes(snes_palette_bytes(title_palettes))
    title_source.convert("RGB").save(preview / "title_source.png")
    reconstruct_window(
        title_tiles, title_map, title_palettes, 0
    ).save(preview / "title_snes_preview.png")

    lookups, maps, source_events = nes.parse_first_level(data_root / "tyrian1.lvl")
    layer1, _ = nes.render_map_layer(
        image_root, lookups[0], maps[0], 14, 3, 292
    )
    layer1 = layer1.crop((40, 0, 296, BG1_ROWS * 8)).convert("RGBA")
    layer3, layer3_nonblank = nes.render_map_layer(
        image_root, lookups[2], maps[2], 15, 14, 593
    )
    layer3 = layer3.crop((52, 0, 308, BG2_ROWS * 8)).convert("RGBA")

    bg1_tiles, bg1_map, bg1_palettes, bg1_report, _ = quantize_mode1_layer(
        layer1, BG1_PALETTES, 0
    )
    bg2_tiles, bg2_map, bg2_local_palettes, bg2_report, _ = quantize_mode1_layer(
        layer3, BG2_PALETTES, BG1_PALETTES
    )
    bg_palettes = bg1_palettes + bg2_local_palettes
    (output / "bg1_tiles.bin").write_bytes(bg1_tiles)
    (output / "bg2_tiles.bin").write_bytes(bg2_tiles)
    (output / "bg_palette.bin").write_bytes(snes_palette_bytes(bg_palettes))
    bg1_chunks = write_map_chunks(output, "bg1", bg1_map, BG1_ROWS)
    bg2_chunks = write_map_chunks(output, "bg2", bg2_map, BG2_ROWS)
    layer1.crop((0, layer1.height - 224, 256, layer1.height)).save(
        preview / "bg1_start_source.png"
    )
    layer3.crop((0, layer3.height - 224, 256, layer3.height)).save(
        preview / "bg2_map3_start_source.png"
    )
    reconstruct_window(
        bg1_tiles, bg1_map, bg1_palettes, BG1_ROWS - 28
    ).save(preview / "bg1_start_snes.png")
    reconstruct_window(
        bg2_tiles,
        bg2_map,
        bg_palettes,
        BG2_ROWS - 28,
    ).save(preview / "bg2_map3_start_snes.png")

    level_events, spawn_count, control_count = encode_snes_level_events(
        nes,
        source_events,
    )
    (output / "level_events.bin").write_bytes(level_events)
    sprite_audit_lines, sprite_audit = audit_sprite_mapping(
        nes,
        source_events,
        data_root / "tyrian.hdt",
    )
    (output / "sprite_mapping_audit.txt").write_text(
        "\n".join(sprite_audit_lines) + "\n",
        encoding="utf-8",
    )

    obj_tiles, obj_palette, obj_metadata, obj_preview = build_obj_assets(nes, image_root)
    (output / "obj_tiles.bin").write_bytes(obj_tiles)
    (output / "obj_palette.bin").write_bytes(obj_palette)
    obj_preview.resize((256, 512), Image.Resampling.NEAREST).save(
        preview / "obj_snes_preview.png"
    )
    write_meta_header(output, obj_metadata, len(level_events))

    sfx = extract_tyrian_sfx(data_root / "tyrian.snd")
    title_music, title_music_report = build_tym_tracker_it(
        workspace,
        workspace /
        "vendor" /
        "audio" /
        "Music" /
        "30_tyrian_the_song.tym",
    )
    level_music, level_music_report = build_tym_tracker_it(
        workspace,
        workspace /
        "vendor" /
        "audio" /
        "Music" /
        "18_tyrian_the_level.tym",
    )
    (output / "tyrian_sfx.it").write_bytes(
        build_minimal_it(workspace, "Tyrian SFX", sfx)
    )
    (output / "tyrian_title_full.it").write_bytes(title_music)
    (output / "tyrian_level_full.it").write_bytes(level_music)

    report_lines = [
        "profile=OpenTyrian processorType 1 / Low Detail",
        "display_hz=60",
        "logic_hz=34.7826 (1193182 / (0x4300 * 2))",
        "original_low_detail_background2=false",
        "original_low_detail_displayScore=false",
        "original_low_detail_explosionTransparent=false",
        "original_low_detail_smoothScroll=true",
        f"title_source_unique_tiles={title_report['source_unique_tiles']}",
        f"title_packed_tiles={title_report['packed_tiles']}",
        f"bg1_rows={BG1_ROWS}",
        f"bg1_chunks={bg1_chunks}",
        f"bg1_source_unique_tiles={bg1_report['source_unique_tiles']}",
        f"bg1_packed_tiles={bg1_report['packed_tiles']}",
        f"bg1_approximated_tiles={bg1_report['approximated_tiles']}",
        f"bg2_source=Tyrian MAP3 (MAP2 omitted by Low Detail target)",
        f"bg2_rows={BG2_ROWS}",
        f"bg2_chunks={bg2_chunks}",
        f"bg2_nonblank_source_cells={layer3_nonblank}",
        f"bg2_source_unique_tiles={bg2_report['source_unique_tiles']}",
        f"bg2_packed_tiles={bg2_report['packed_tiles']}",
        f"bg2_approximated_tiles={bg2_report['approximated_tiles']}",
        f"level_event_source_records={len(source_events)}",
        f"level_event_spawn_records={spawn_count}",
        f"level_event_control_records={control_count}",
        f"level_event_bytes={len(level_events)}",
        f"obj_tiles={len(obj_tiles) // 32}",
        f"obj_enemy_archetypes=24",
        f"sprite_source_ids={sprite_audit['source_ids']}",
        f"sprite_unknown_spawns={sprite_audit['unknown_spawns']}",
        f"sprite_bank_mismatch_spawns={sprite_audit['bank_mismatch_spawns']}",
        f"sprite_exact_graphic_spawns={sprite_audit['exact_graphic_spawns']}",
        f"title_music_source=30_tyrian_the_song.tym / full EVNT tracker",
        f"title_music_source_seconds={title_music_report['source_duration_seconds']:.6f}",
        f"title_music_tracker_seconds={title_music_report['tracker_duration_seconds']:.6f}",
        f"title_music_patterns={title_music_report['patterns']}",
        f"title_music_orders={title_music_report['orders']}",
        f"title_music_unrolled_loops={title_music_report['unrolled_loops']}",
        f"title_music_samples={title_music_report['samples']}",
        f"title_music_it_bytes={title_music_report['it_bytes']}",
        f"title_music_snes_sources={title_music_report['source_channels']}",
        f"level_music_source=18_tyrian_the_level.tym / full EVNT tracker",
        f"level_music_source_seconds={level_music_report['source_duration_seconds']:.6f}",
        f"level_music_tracker_seconds={level_music_report['tracker_duration_seconds']:.6f}",
        f"level_music_module_play_seconds={level_music_report['module_play_seconds']:.6f}",
        f"level_music_patterns={level_music_report['patterns']}",
        f"level_music_orders={level_music_report['orders']}",
        f"level_music_unrolled_loops={level_music_report['unrolled_loops']}",
        f"level_music_samples={level_music_report['samples']}",
        f"level_music_it_bytes={level_music_report['it_bytes']}",
        f"level_music_snes_sources={level_music_report['source_channels']}",
        f"music_tracker_speed={TRACKER_SPEED}",
        f"music_tracker_tempo={TRACKER_TEMPO}",
        f"music_tracker_row_hz={TRACKER_TEMPO / (2.5 * TRACKER_SPEED):.6f}",
        f"sfx_samples={len(sfx)}",
    ]
    (output / "asset_report.txt").write_text(
        "\n".join(report_lines) + "\n", encoding="utf-8"
    )
    (output / "assets.stamp").write_text(
        "Generated by tools/build_assets.py\n", encoding="ascii"
    )
    print("\n".join(report_lines))


if __name__ == "__main__":
    main()
