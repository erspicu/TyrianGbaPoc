#!/usr/bin/env python3
"""GBA-native image quantization and OBJ asset helpers.

This module deliberately owns the cartridge-side formats it emits.  It reads
the project-local PC Tyrian extracts, but never routes data through another
console's tile, palette, mapper, or asset-builder rules.
"""

from __future__ import annotations

import collections
import struct
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw


def encode_gba_4bpp(values: np.ndarray) -> bytes:
    """Pack one 8x8 tile in native GBA 4bpp nibble order."""
    flat = values.reshape(8, 8)
    output = bytearray(32)
    cursor = 0
    for y in range(8):
        for x in range(0, 8, 2):
            output[cursor] = (
                int(flat[y, x]) |
                (int(flat[y, x + 1]) << 4)
            )
            cursor += 1
    return bytes(output)


def gba_palette_bytes(
    palettes: list[list[tuple[int, int, int]]],
) -> bytes:
    """Encode RGB888 palettes as native little-endian GBA BGR555."""
    output = bytearray()
    for palette in palettes:
        padded = (palette + [(0, 0, 0)] * 16)[:16]
        for red, green, blue in padded:
            word = (
                (red >> 3) |
                ((green >> 3) << 5) |
                ((blue >> 3) << 10)
            )
            output.extend(struct.pack("<H", word))
    return bytes(output)


def farthest_centroids(features: np.ndarray, count: int) -> np.ndarray:
    if len(features) == 0:
        return np.zeros((count, 3), dtype=np.float32)
    luminance = features @ np.array(
        [0.299, 0.587, 0.114], dtype=np.float32
    )
    selected = [int(np.argmin(luminance))]
    while len(selected) < count:
        current = features[selected]
        distance = (
            (features[:, None, :] - current[None, :, :]) ** 2
        ).sum(axis=2)
        selected.append(int(np.argmax(distance.min(axis=1))))
    return features[selected].astype(np.float32)


def adaptive_palette(
    pixels: np.ndarray,
    colour_count: int = 15,
) -> list[tuple[int, int, int]]:
    if len(pixels) == 0:
        return [(0, 0, 0)] * colour_count
    if len(pixels) > 180_000:
        step = max(1, len(pixels) // 180_000)
        pixels = pixels[::step][:180_000]
    strip = Image.fromarray(
        pixels.reshape(1, -1, 3).astype(np.uint8), "RGB"
    )
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
    colours.sort(
        key=lambda rgb: (
            77 * rgb[0] + 150 * rgb[1] + 29 * rgb[2],
            rgb,
        )
    )
    return colours


def palette_errors(
    tiles: np.ndarray,
    opaque: np.ndarray,
    palette: list[tuple[int, int, int]],
) -> np.ndarray:
    result = np.zeros(len(tiles), dtype=np.float64)
    colours = np.asarray(palette, dtype=np.int32)
    for start in range(0, len(tiles), 256):
        end = min(len(tiles), start + 256)
        pixels = tiles[start:end, :, :3].astype(np.int32)
        diff = pixels[:, :, None, :] - colours[None, None, :, :]
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
) -> tuple[
    bytes,
    bytes,
    list[list[tuple[int, int, int]]],
    dict[str, int],
    np.ndarray,
]:
    """Quantize a 256-wide image directly to GBA text-BG data."""
    rgba = np.asarray(image.convert("RGBA"), dtype=np.uint8)
    if rgba.shape[1] != 256 or rgba.shape[0] % 8:
        raise ValueError(
            "GBA text layer must be 256 pixels wide and tile aligned: "
            f"{image.size}"
        )
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
            (
                tiles[nonempty, :, :3].astype(np.float32)
                * opaque[nonempty, :, None]
            ).sum(axis=1)
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
        colours = np.asarray(palettes[group][1:], dtype=np.int32)
        for start in range(0, len(selected), 256):
            subset = selected[start : start + 256]
            pixels = tiles[subset, :, :3].astype(np.int32)
            diff = pixels[:, :, None, :] - colours[None, None, :, :]
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
    missing = [
        pattern for pattern in frequency if pattern not in pattern_index
    ]
    selected_array = np.frombuffer(
        b"".join(selected_patterns), dtype=np.uint8
    ).reshape(len(selected_patterns), 64)
    for start in range(0, len(missing), 32):
        batch = missing[start : start + 32]
        values = np.frombuffer(
            b"".join(batch), dtype=np.uint8
        ).reshape(len(batch), 64)
        distance = np.count_nonzero(
            values[:, None, :] != selected_array[None, :, :],
            axis=2,
        )
        nearest = distance.argmin(axis=1)
        for pattern, index in zip(batch, nearest, strict=True):
            pattern_index[pattern] = int(index)

    tile_words = np.empty(len(tiles), dtype="<u2")
    for index, pattern in enumerate(packed_patterns):
        palette = palette_base + int(assignments[index])
        if not 0 <= palette <= 15:
            raise ValueError(f"GBA palette bank outside 0..15: {palette}")
        tile_words[index] = pattern_index[pattern] | (palette << 12)

    tile_binary = bytearray()
    for pattern in selected_patterns:
        tile_binary.extend(
            encode_gba_4bpp(np.frombuffer(pattern, dtype=np.uint8))
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
    box = image.getchannel("A").getbbox()
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
        draw.polygon(
            ((8, 0), (12, 11), (8, 15), (4, 11)),
            fill=(64, 220, 255, 255),
        )
        draw.rectangle((7, 3, 8, 12), fill=(255, 255, 255, 255))
    return canvas


def compose_sprite_2x2(directory: Path, start: int) -> Image.Image:
    """Compose the PC Sprite2 2x2 component layout without rescaling."""
    canvas = Image.new("RGBA", (24, 28), (0, 0, 0, 0))
    for index, x, y in (
        (start, 0, 0),
        (start + 1, 12, 0),
        (start + 19, 0, 14),
        (start + 20, 12, 14),
    ):
        path = directory / f"{index:03d}.png"
        if not path.is_file():
            raise FileNotFoundError(path)
        canvas.alpha_composite(Image.open(path).convert("RGBA"), (x, y))
    return canvas


def build_nort_ship_assets(
    image_root: Path,
) -> tuple[bytes, bytes, Image.Image]:
    """Build OpenTyrian's special ``shipgraphic == 1`` presentation.

    ``mainint.c::JE_playerMovement()`` does not draw Nort Ship through the
    ordinary one-frame player path.  It places Sprite2 frames 220 and 222
    24 pixels apart, then adds one of components 39/40 or 58/59 while the
    ship banks.  Pack that exact 48x30 authored footprint into one GBA
    64x32 OBJ.  Five build-time frames retain all banking states while one
    runtime OAM entry and the existing player tile window are sufficient.
    """
    player_dir = image_root / "sheets" / "09_player_ships"
    frames: list[Image.Image] = []
    banking_additions = {
        -2: (59, 36, 16),
        -1: (58, 36, 16),
         1: (39, 0, 16),
         2: (40, 0, 16),
    }

    for banking in range(-2, 3):
        frame = Image.new("RGBA", (64, 32), (0, 0, 0, 0))
        frame.alpha_composite(compose_sprite_2x2(player_dir, 220), (0, 2))
        frame.alpha_composite(compose_sprite_2x2(player_dir, 222), (24, 2))
        addition = banking_additions.get(banking)
        if addition is not None:
            sprite, x, y = addition
            frame.alpha_composite(
                Image.open(player_dir / f"{sprite:03d}.png").convert("RGBA"),
                (x, y),
            )
        frames.append(frame)

    rgba_frames = [np.asarray(frame, dtype=np.uint8) for frame in frames]
    samples = np.concatenate(
        [
            rgba[:, :, :3][rgba[:, :, 3] >= 80]
            for rgba in rgba_frames
        ],
        axis=0,
    )
    colours = adaptive_palette(samples)
    palette = [(0, 0, 0)] + colours
    palette_array = np.asarray(colours, dtype=np.int32)
    tile_data = bytearray()
    preview = Image.new("RGBA", (64 * len(frames), 32), (0, 0, 0, 0))

    for frame_index, (frame, rgba) in enumerate(zip(frames, rgba_frames)):
        opaque = rgba[:, :, 3] >= 80
        values = np.zeros((32, 64), dtype=np.uint8)
        if opaque.any():
            pixels = rgba[opaque, :3].astype(np.int32)
            values[opaque] = (
                (
                    (pixels[:, None, :] - palette_array[None, :, :]) ** 2
                )
                .sum(axis=2)
                .argmin(axis=1)
                .astype(np.uint8)
                + 1
            )
        for tile_y in range(4):
            for tile_x in range(8):
                tile_data.extend(
                    encode_gba_4bpp(
                        values[
                            tile_y * 8 : tile_y * 8 + 8,
                            tile_x * 8 : tile_x * 8 + 8,
                        ]
                    )
                )

        # Show the actual quantized result, not the source PNG, in audits.
        quantized = np.zeros((32, 64, 4), dtype=np.uint8)
        quantized[opaque, :3] = np.asarray(palette, dtype=np.uint8)[
            values[opaque]
        ]
        quantized[opaque, 3] = 255
        preview.alpha_composite(
            Image.fromarray(quantized, "RGBA"),
            (frame_index * 64, 0),
        )

    expected_bytes = len(frames) * 64 * 32 // 2
    if len(tile_data) != expected_bytes:
        raise AssertionError(
            "Nort Ship 64x32 banking atlas changed size: "
            f"{len(tile_data)} != {expected_bytes}"
        )
    return bytes(tile_data), gba_palette_bytes([palette]), preview


def compose_first_level_boss(
    directory: Path,
    core_start: int = 1,
) -> Image.Image:
    """Rebuild the stock first-level boss's 5x4 Sprite2 component grid."""
    canvas = Image.new("RGBA", (120, 112), (0, 0, 0, 0))
    rows = (
        (10, 12, 14, 16, 18),
        (48, 50, core_start, 54, 56),
        (86, 88, 90, 92, 94),
        (124, 126, 128, 130, 132),
    )
    for row, starts in enumerate(rows):
        for column, start in enumerate(starts):
            canvas.alpha_composite(
                compose_sprite_2x2(directory, start),
                (column * 24, row * 28),
            )
    return canvas


def build_obj_assets(
    image_root: Path,
    player_shot_source: Image.Image | None = None,
) -> tuple[bytes, bytes, dict[str, int], Image.Image]:
    """Build the bootstrap OBJ atlas directly in GBA tile order."""
    player_dir = image_root / "sheets" / "09_player_ships"
    newsh2 = image_root / "sheets_newsh" / "newsh_2"
    newsh4 = image_root / "sheets_newsh" / "newsh_4"
    newshe = image_root / "sheets_newsh" / "newsh_e"
    newsh6 = image_root / "sheets_newsh" / "newsh_6"

    representatives = (
        (newsh2, 159), (newsh2, 171), (newsh2, 229),
        (newsh2, 191), (newsh2, 91), (newsh2, 85),
        (newsh2, 87), (newsh2, 123), (newsh2, 125),
        (newsh2, 267), (newsh2, 269), (newsh2, 271),
        (newsh2, 49), (newsh2, 51), (newsh2, 89),
        (newsh2, 127), (newsh2, 273), (newsh2, 277),
        (newsh2, 281), (newsh2, 153), (newsh2, 115),
        (newsh4, 153), (newshe, 1), (newsh2, 169),
    )
    enemies = [
        (
            Image.open(directory / f"{graphic:03d}.png")
            if directory == newsh2 and graphic == 171
            else compose_sprite_2x2(directory, graphic)
        )
        for directory, graphic in representatives
    ]
    if len(enemies) != 24:
        raise AssertionError("GBA bootstrap atlas must contain 24 archetypes")

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

    place_block("PLAYER_0", 0, compose_sprite_2x2(player_dir, 233), 0)
    place_block("PLAYER_1", 1, compose_sprite_2x2(player_dir, 235), 0)
    for index, enemy in enumerate(enemies):
        place_block(f"ENEMY_{index}", index + 2, enemy, 1 + index // 4)

    boss_x = 64
    boss_y = 192
    boss = fit_sprite(compose_first_level_boss(newsh4, 1), (64, 64))
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
        player_shot_source
        if player_shot_source is not None
        else procedural_shot(False),
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
                encode_gba_4bpp(
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
    return (
        bytes(tile_data),
        gba_palette_bytes(palettes),
        metadata,
        preview,
    )
