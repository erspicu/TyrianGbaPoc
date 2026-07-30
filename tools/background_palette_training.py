#!/usr/bin/env python3
"""Offline training and evaluation for Tyrian's GBA 4bpp BG palettes.

The product renderer builds an 8x8 source tile from:

    (top_shape, bottom_shape, vertical phase, horizontal sub-x)

The former palette builder trained on isolated shape slices.  This module
reconstructs the exact cache keys from every stock LVL map, preserves each
unique tile histogram and its map-occurrence weight, and compares candidate
palette adapters against the v53 runtime assets.

Nothing in this file is required by the GBA runtime.  It is intentionally a
deterministic build-host experiment until the generated palettes demonstrate
a measurable and visual non-regression across all stock levels.
"""

from __future__ import annotations

import argparse
import collections
import hashlib
import json
import struct
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

import numpy as np
from PIL import Image, ImageDraw


PROFILE_IDS = (")", "w", "x", "y", "z")
PROFILE_INDEX = {value: index for index, value in enumerate(PROFILE_IDS)}
LAYER_COLUMNS = (14, 14, 15)
LAYER_ROWS = (300, 600, 600)
LAYER_FIRST_ROWS = (3, 14, 14)
LAYER_SHAPE_SLOTS = (72, 71, 70)
SHAPE_COUNT = 600
SHAPE_WIDTH = 24
SHAPE_HEIGHT = 28
SHAPE_PIXELS = SHAPE_WIDTH * SHAPE_HEIGHT
MAP_SHAPE_COUNT = 128
MAP_SHAPE_LAYER_BYTES = MAP_SHAPE_COUNT * 2
MAP_SHAPE_BYTES = MAP_SHAPE_LAYER_BYTES * 3
EVENT_BYTES = 11
PALETTE_BANKS = 16
PALETTE_COLOURS = 16
SOURCE_COLOURS = 256
MASK_TABLE_BYTES = 1 << 16


@dataclass
class LevelData:
    episode: int
    level: int
    shape_file: str
    lookups: tuple[np.ndarray, np.ndarray, np.ndarray]
    maps: tuple[np.ndarray, np.ndarray, np.ndarray]


@dataclass
class ProfileDataset:
    profile_id: str
    keys: np.ndarray
    histograms: np.ndarray
    masks: np.ndarray
    weights: np.ndarray
    level_count: int
    map_tile_count: int
    checksum: str


@dataclass
class PaletteAssets:
    words: np.ndarray
    nearest: np.ndarray
    mask_bank: np.ndarray


@dataclass
class ProfileEvaluation:
    mean_squared: float
    rms: float
    p95: float
    p99: float
    cvar95: float
    key_errors: np.ndarray
    key_raw_errors: np.ndarray


@dataclass
class TrainingResult:
    assets: PaletteAssets
    iterations: list[dict[str, float | int | str]]


def read_u16(source: bytes, offset: int) -> int:
    return struct.unpack_from("<H", source, offset)[0]


def read_u32(source: bytes, offset: int) -> int:
    return struct.unpack_from("<I", source, offset)[0]


def load_shapes(path: Path) -> np.ndarray:
    source = path.read_bytes()
    result = np.zeros(
        (SHAPE_COUNT, SHAPE_HEIGHT, SHAPE_WIDTH),
        dtype=np.uint8,
    )
    position = 0
    for shape in range(SHAPE_COUNT):
        if position >= len(source):
            raise ValueError(f"truncated shape flag: {path}:{shape + 1}")
        blank = source[position]
        position += 1
        if blank == 0:
            end = position + SHAPE_PIXELS
            if end > len(source):
                raise ValueError(
                    f"truncated shape pixels: {path}:{shape + 1}"
                )
            result[shape] = np.frombuffer(
                source[position:end],
                dtype=np.uint8,
            ).reshape(SHAPE_HEIGHT, SHAPE_WIDTH)
            position = end
    return result


def iter_levels(data_root: Path) -> Iterable[LevelData]:
    for episode in range(1, 5):
        path = data_root / f"tyrian{episode}.lvl"
        source = path.read_bytes()
        if len(source) < 2:
            raise ValueError(f"truncated LVL header: {path}")
        offset_count = read_u16(source, 0)
        if offset_count < 3 or offset_count % 2 == 0:
            raise ValueError(
                f"invalid LVL offset count: {path}:{offset_count}"
            )
        table_end = 2 + offset_count * 4
        if table_end > len(source):
            raise ValueError(f"truncated LVL table: {path}")
        offsets = [
            read_u32(source, 2 + index * 4)
            for index in range(offset_count)
        ]
        if offsets != sorted(offsets):
            raise ValueError(f"unordered LVL offsets: {path}")
        logical_count = offset_count // 2
        for level in range(1, logical_count + 1):
            table_index = (level - 1) * 2
            start = offsets[table_index]
            end = (
                offsets[table_index + 2]
                if table_index + 2 < offset_count
                else len(source)
            )
            if start + 10 > end or end > len(source):
                raise ValueError(
                    f"invalid LVL section: {path}:{level}"
                )
            shape_file = chr(source[start + 1]).lower()
            if shape_file not in PROFILE_INDEX:
                raise ValueError(
                    f"unsupported shape bank: {path}:{level}:{shape_file!r}"
                )
            enemy_count = read_u16(source, start + 8)
            position = start + 10 + enemy_count * 2
            if position + 2 > end:
                raise ValueError(
                    f"truncated enemy pool: {path}:{level}"
                )
            event_count = read_u16(source, position)
            position += 2 + event_count * EVENT_BYTES
            if position + MAP_SHAPE_BYTES > end:
                raise ValueError(
                    f"truncated map lookup: {path}:{level}"
                )
            lookup_bytes = source[position : position + MAP_SHAPE_BYTES]
            position += MAP_SHAPE_BYTES
            lookups: list[np.ndarray] = []
            for layer in range(3):
                layer_source = np.frombuffer(
                    lookup_bytes,
                    dtype=">u2",
                    count=MAP_SHAPE_COUNT,
                    offset=layer * MAP_SHAPE_LAYER_BYTES,
                ).astype(np.uint16)
                lookups.append(layer_source)
            maps: list[np.ndarray] = []
            for columns, rows in zip(
                LAYER_COLUMNS,
                LAYER_ROWS,
                strict=True,
            ):
                size = columns * rows
                if position + size > end:
                    raise ValueError(
                        f"truncated map pixels: {path}:{level}"
                    )
                maps.append(
                    np.frombuffer(
                        source[position : position + size],
                        dtype=np.uint8,
                    ).reshape(rows, columns)
                )
                position += size
            if position != end:
                raise ValueError(
                    f"unparsed LVL bytes: {path}:{level}:"
                    f"{end - position}"
                )
            yield LevelData(
                episode=episode,
                level=level,
                shape_file=shape_file,
                lookups=(lookups[0], lookups[1], lookups[2]),
                maps=(maps[0], maps[1], maps[2]),
            )


def level_shape_number(
    level: LevelData,
    shapes: np.ndarray,
    layer: int,
    row: int,
    column: int,
) -> int:
    if (
        row < 0 or
        row >= LAYER_ROWS[layer] or
        column < 0 or
        column >= LAYER_COLUMNS[layer]
    ):
        return 0
    map_index = int(level.maps[layer][row, column])
    if map_index >= LAYER_SHAPE_SLOTS[layer]:
        return 0
    shape_number = int(level.lookups[layer][map_index])
    if shape_number == 0 or shape_number > SHAPE_COUNT:
        return 0
    if not np.any(shapes[shape_number - 1]):
        return 0
    return shape_number


def level_tile_key(
    level: LevelData,
    shapes: np.ndarray,
    layer: int,
    tile_x: int,
    tile_y: int,
) -> int:
    canvas_x = tile_x * 8
    canvas_y = tile_y * 8
    if canvas_x >= LAYER_COLUMNS[layer] * SHAPE_WIDTH:
        return 1
    map_row = LAYER_FIRST_ROWS[layer] + canvas_y // SHAPE_HEIGHT
    map_column = canvas_x // SHAPE_WIDTH
    sub_x = (canvas_x % SHAPE_WIDTH) // 8
    phase = canvas_y % SHAPE_HEIGHT
    top_shape = level_shape_number(
        level,
        shapes,
        layer,
        map_row,
        map_column,
    )
    bottom_shape = 0
    if phase + 7 >= SHAPE_HEIGHT:
        bottom_shape = level_shape_number(
            level,
            shapes,
            layer,
            map_row + 1,
            map_column,
        )
    if top_shape == 0 and bottom_shape == 0:
        return 1
    packed = (
        top_shape |
        (bottom_shape << 10) |
        ((phase // 4) << 20) |
        (sub_x << 23)
    )
    return packed + 1


def render_key(key: int, shapes: np.ndarray) -> np.ndarray:
    source = np.zeros((8, 8), dtype=np.uint8)
    if key == 1:
        return source
    packed = key - 1
    top_shape = packed & 0x03FF
    bottom_shape = (packed >> 10) & 0x03FF
    phase = ((packed >> 20) & 7) * 4
    sub_x = (packed >> 23) & 3
    for y in range(8):
        source_y = phase + y
        shape_number = top_shape
        if source_y >= SHAPE_HEIGHT:
            source_y -= SHAPE_HEIGHT
            shape_number = bottom_shape
        if shape_number:
            source[y] = shapes[
                shape_number - 1,
                source_y,
                sub_x * 8 : sub_x * 8 + 8,
            ]
    return source


def level_runtime_counter(
    level: LevelData,
    shapes: np.ndarray,
) -> tuple[collections.Counter[int], int]:
    """Reconstruct every runtime background key used by one LVL section."""
    counter: collections.Counter[int] = collections.Counter()
    shape_present = np.any(shapes, axis=(1, 2))
    map_tiles = 0

    for layer in range(3):
        tile_columns = LAYER_COLUMNS[layer] * SHAPE_WIDTH // 8
        canvas_rows = (
            (
                LAYER_ROWS[layer] -
                LAYER_FIRST_ROWS[layer]
            ) * SHAPE_HEIGHT + 7
        ) // 8
        shape_for_map_index = np.zeros(256, dtype=np.uint16)
        for map_index in range(LAYER_SHAPE_SLOTS[layer]):
            shape_number = int(level.lookups[layer][map_index])
            if (
                shape_number != 0 and
                shape_number <= SHAPE_COUNT and
                shape_present[shape_number - 1]
            ):
                shape_for_map_index[map_index] = shape_number
        shape_map = shape_for_map_index[level.maps[layer]]
        tile_x = np.arange(tile_columns, dtype=np.uint16)
        map_columns = (tile_x * 8 // SHAPE_WIDTH).astype(np.uint8)
        sub_x = ((tile_x * 8 % SHAPE_WIDTH) // 8).astype(np.uint32)
        for tile_y in range(canvas_rows):
            canvas_y = tile_y * 8
            map_row = (
                LAYER_FIRST_ROWS[layer] +
                canvas_y // SHAPE_HEIGHT
            )
            phase = canvas_y % SHAPE_HEIGHT
            top_shape = shape_map[map_row, map_columns].astype(
                np.uint32
            )
            if (
                phase + 7 >= SHAPE_HEIGHT and
                map_row + 1 < LAYER_ROWS[layer]
            ):
                bottom_shape = shape_map[
                    map_row + 1,
                    map_columns,
                ].astype(np.uint32)
            else:
                bottom_shape = np.zeros_like(top_shape)
            packed = (
                top_shape |
                (bottom_shape << 10) |
                np.uint32((phase // 4) << 20) |
                (sub_x << 23)
            )
            row_keys = packed + 1
            row_keys[
                (top_shape == 0) & (bottom_shape == 0)
            ] = 1
            counter.update(int(value) for value in row_keys)
            map_tiles += tile_columns
    return counter, map_tiles


def dataset_from_counter(
    profile_id: str,
    counter: collections.Counter[int],
    shapes: np.ndarray,
    level_count: int,
) -> ProfileDataset:
    counter = counter.copy()
    counter.pop(1, None)
    rendered = [
        (key, render_key(int(key), shapes))
        for key in sorted(counter)
    ]
    rendered = [
        (key, pixels)
        for key, pixels in rendered
        if np.any(pixels)
    ]
    keys = np.asarray(
        [key for key, _ in rendered],
        dtype=np.uint32,
    )
    histograms = np.zeros(
        (len(keys), SOURCE_COLOURS),
        dtype=np.uint8,
    )
    masks = np.zeros(len(keys), dtype=np.uint16)
    weights = np.asarray(
        [counter[int(key)] for key in keys],
        dtype=np.float64,
    )
    for index, (_, rendered_pixels) in enumerate(rendered):
        pixels = rendered_pixels.reshape(-1)
        nonzero = pixels[pixels != 0]
        histograms[index] = np.bincount(
            nonzero,
            minlength=SOURCE_COLOURS,
        )
        mask = 0
        for hue in np.unique(nonzero >> 4):
            mask |= 1 << int(hue)
        masks[index] = mask
    digest = hashlib.sha256()
    digest.update(keys.tobytes())
    digest.update(histograms.tobytes())
    digest.update(weights.astype("<u8").tobytes())
    return ProfileDataset(
        profile_id=profile_id,
        keys=keys,
        histograms=histograms,
        masks=masks,
        weights=weights,
        level_count=level_count,
        map_tile_count=int(weights.sum()),
        checksum=digest.hexdigest(),
    )


def build_runtime_datasets(
    data_root: Path,
) -> tuple[dict[str, ProfileDataset], dict[str, np.ndarray], dict[str, int]]:
    shapes_by_profile = {
        profile: load_shapes(data_root / f"shapes{profile}.dat")
        for profile in PROFILE_IDS
    }
    counters = {
        profile: collections.Counter()
        for profile in PROFILE_IDS
    }
    level_sets = {
        profile: set()
        for profile in PROFILE_IDS
    }
    logical_levels = 0
    map_tiles = 0
    for level in iter_levels(data_root):
        logical_levels += 1
        profile = level.shape_file
        level_sets[profile].add((level.episode, level.level))
        shapes = shapes_by_profile[profile]
        level_counter, level_map_tiles = level_runtime_counter(
            level,
            shapes,
        )
        counters[profile].update(level_counter)
        map_tiles += level_map_tiles
    datasets: dict[str, ProfileDataset] = {}
    for profile in PROFILE_IDS:
        datasets[profile] = dataset_from_counter(
            profile_id=profile,
            counter=counters[profile],
            shapes=shapes_by_profile[profile],
            level_count=len(level_sets[profile]),
        )
    metadata = {
        "logical_levels": logical_levels,
        "map_tiles_including_blank": map_tiles,
        "unique_nonblank_keys": sum(
            len(dataset.keys)
            for dataset in datasets.values()
        ),
        "active_masks": len(
            set(
                int(mask)
                for dataset in datasets.values()
                for mask in dataset.masks
            )
        ),
    }
    return datasets, shapes_by_profile, metadata


def build_level_runtime_datasets(
    data_root: Path,
    shapes_by_profile: dict[str, np.ndarray] | None = None,
) -> tuple[
    list[tuple[LevelData, ProfileDataset]],
    dict[str, np.ndarray],
    dict[str, int],
]:
    """Build the exact runtime-key distribution for every stock LVL section."""
    if shapes_by_profile is None:
        shapes_by_profile = {
            profile: load_shapes(data_root / f"shapes{profile}.dat")
            for profile in PROFILE_IDS
        }
    result: list[tuple[LevelData, ProfileDataset]] = []
    map_tiles = 0
    active_masks: set[int] = set()

    for level in iter_levels(data_root):
        counter, level_map_tiles = level_runtime_counter(
            level,
            shapes_by_profile[level.shape_file],
        )
        dataset = dataset_from_counter(
            profile_id=level.shape_file,
            counter=counter,
            shapes=shapes_by_profile[level.shape_file],
            level_count=1,
        )
        result.append((level, dataset))
        map_tiles += level_map_tiles
        active_masks.update(int(mask) for mask in dataset.masks)
    metadata = {
        "logical_levels": len(result),
        "map_tiles_including_blank": map_tiles,
        "unique_nonblank_keys": sum(
            len(dataset.keys)
            for _, dataset in result
        ),
        "active_masks": len(active_masks),
    }
    return result, shapes_by_profile, metadata


def srgb_to_linear(values: np.ndarray) -> np.ndarray:
    values = np.asarray(values, dtype=np.float64)
    return np.where(
        values <= 0.04045,
        values / 12.92,
        ((values + 0.055) / 1.055) ** 2.4,
    )


def linear_srgb_to_oklab(values: np.ndarray) -> np.ndarray:
    values = np.asarray(values, dtype=np.float64)
    lms = values @ np.asarray(
        (
            (0.4122214708, 0.2119034982, 0.0883024619),
            (0.5363325363, 0.6806995451, 0.2817188376),
            (0.0514459929, 0.1073969566, 0.6299787005),
        ),
        dtype=np.float64,
    )
    lms_root = np.cbrt(lms)
    return lms_root @ np.asarray(
        (
            (0.2104542553, 1.9779984951, 0.0259040371),
            (0.7936177850, -2.4285922050, 0.7827717662),
            (-0.0040720468, 0.4505937099, -0.8086757660),
        ),
        dtype=np.float64,
    )


def rgb_code_to_oklab(values: np.ndarray) -> np.ndarray:
    return linear_srgb_to_oklab(
        srgb_to_linear(
            np.asarray(values, dtype=np.float64) / 255.0
        )
    )


def linear_srgb_to_cielab(values: np.ndarray) -> np.ndarray:
    values = np.asarray(values, dtype=np.float64)
    xyz = values @ np.asarray(
        (
            (0.4124564, 0.2126729, 0.0193339),
            (0.3575761, 0.7151522, 0.1191920),
            (0.1804375, 0.0721750, 0.9503041),
        ),
        dtype=np.float64,
    )
    xyz = xyz / np.asarray((0.95047, 1.0, 1.08883))
    delta = 6.0 / 29.0
    transformed = np.where(
        xyz > delta ** 3,
        np.cbrt(xyz),
        xyz / (3.0 * delta * delta) + 4.0 / 29.0,
    )
    return np.stack(
        (
            116.0 * transformed[..., 1] - 16.0,
            500.0 * (
                transformed[..., 0] -
                transformed[..., 1]
            ),
            200.0 * (
                transformed[..., 1] -
                transformed[..., 2]
            ),
        ),
        axis=-1,
    )


def rgb_code_to_cielab(values: np.ndarray) -> np.ndarray:
    return linear_srgb_to_cielab(
        srgb_to_linear(
            np.asarray(values, dtype=np.float64) / 255.0
        )
    )


def ciede2000(left: np.ndarray, right: np.ndarray) -> np.ndarray:
    """Vectorized CIEDE2000 with kL=kC=kH=1."""
    left = np.asarray(left, dtype=np.float64)
    right = np.asarray(right, dtype=np.float64)
    l1, a1, b1 = np.moveaxis(left, -1, 0)
    l2, a2, b2 = np.moveaxis(right, -1, 0)
    c1 = np.hypot(a1, b1)
    c2 = np.hypot(a2, b2)
    c_bar = (c1 + c2) * 0.5
    c_bar7 = c_bar ** 7
    g = 0.5 * (
        1.0 -
        np.sqrt(c_bar7 / (c_bar7 + 25.0 ** 7))
    )
    a1_prime = (1.0 + g) * a1
    a2_prime = (1.0 + g) * a2
    c1_prime = np.hypot(a1_prime, b1)
    c2_prime = np.hypot(a2_prime, b2)
    h1_prime = (
        np.degrees(np.arctan2(b1, a1_prime)) % 360.0
    )
    h2_prime = (
        np.degrees(np.arctan2(b2, a2_prime)) % 360.0
    )
    delta_l = l2 - l1
    delta_c = c2_prime - c1_prime
    delta_h_angle = h2_prime - h1_prime
    product = c1_prime * c2_prime
    delta_h_angle = np.where(product == 0, 0.0, delta_h_angle)
    delta_h_angle = np.where(
        (product != 0) & (delta_h_angle > 180.0),
        delta_h_angle - 360.0,
        delta_h_angle,
    )
    delta_h_angle = np.where(
        (product != 0) & (delta_h_angle < -180.0),
        delta_h_angle + 360.0,
        delta_h_angle,
    )
    delta_h = (
        2.0 *
        np.sqrt(product) *
        np.sin(np.radians(delta_h_angle) * 0.5)
    )
    l_bar = (l1 + l2) * 0.5
    c_prime_bar = (c1_prime + c2_prime) * 0.5
    h_sum = h1_prime + h2_prime
    h_difference = np.abs(h1_prime - h2_prime)
    h_bar = np.where(
        product == 0,
        h_sum,
        np.where(
            h_difference <= 180.0,
            h_sum * 0.5,
            np.where(
                h_sum < 360.0,
                (h_sum + 360.0) * 0.5,
                (h_sum - 360.0) * 0.5,
            ),
        ),
    )
    t = (
        1.0 -
        0.17 * np.cos(np.radians(h_bar - 30.0)) +
        0.24 * np.cos(np.radians(2.0 * h_bar)) +
        0.32 * np.cos(np.radians(3.0 * h_bar + 6.0)) -
        0.20 * np.cos(np.radians(4.0 * h_bar - 63.0))
    )
    delta_theta = (
        30.0 *
        np.exp(-((h_bar - 275.0) / 25.0) ** 2)
    )
    c_prime_bar7 = c_prime_bar ** 7
    r_c = 2.0 * np.sqrt(
        c_prime_bar7 /
        (c_prime_bar7 + 25.0 ** 7)
    )
    s_l = 1.0 + (
        0.015 * (l_bar - 50.0) ** 2 /
        np.sqrt(20.0 + (l_bar - 50.0) ** 2)
    )
    s_c = 1.0 + 0.045 * c_prime_bar
    s_h = 1.0 + 0.015 * c_prime_bar * t
    r_t = -np.sin(np.radians(2.0 * delta_theta)) * r_c
    l_term = delta_l / s_l
    c_term = delta_c / s_c
    h_term = delta_h / s_h
    return np.sqrt(
        np.maximum(
            0.0,
            l_term * l_term +
            c_term * c_term +
            h_term * h_term +
            r_t * c_term * h_term,
        )
    )


def validate_ciede2000() -> None:
    """Validate against the first three Sharma reference pairs."""
    pairs = (
        (
            (50.0, 2.6772, -79.7751),
            (50.0, 0.0, -82.7485),
            2.0425,
        ),
        (
            (50.0, 3.1571, -77.2803),
            (50.0, 0.0, -82.7485),
            2.8615,
        ),
        (
            (50.0, 2.8361, -74.0200),
            (50.0, 0.0, -82.7485),
            3.4412,
        ),
    )
    for left, right, expected in pairs:
        actual = float(
            ciede2000(
                np.asarray(left),
                np.asarray(right),
            )
        )
        if abs(actual - expected) > 5.0e-5:
            raise ValueError(
                "CIEDE2000 reference validation failed: "
                f"{actual:.8f} != {expected:.8f}"
            )


def load_source_rgb(data_root: Path) -> np.ndarray:
    source = np.frombuffer(
        (data_root / "palette.dat").read_bytes()[: SOURCE_COLOURS * 3],
        dtype=np.uint8,
    ).reshape(SOURCE_COLOURS, 3)
    if source.shape != (SOURCE_COLOURS, 3):
        raise ValueError("palette.dat is truncated")
    return (
        (source.astype(np.uint16) << 2) |
        (source.astype(np.uint16) >> 4)
    ).astype(np.uint8)


def bgr555_rgb() -> np.ndarray:
    words = np.arange(1 << 15, dtype=np.uint16)
    components = np.stack(
        (
            words & 31,
            (words >> 5) & 31,
            (words >> 10) & 31,
        ),
        axis=1,
    )
    return (
        (components << 3) |
        (components >> 2)
    ).astype(np.uint8)


def load_assets(directory: Path) -> PaletteAssets:
    words_source = (directory / "background_gba_palette.bin").read_bytes()
    nearest_source = (
        directory / "background_palette_nearest.bin"
    ).read_bytes()
    mask_source = (
        directory / "background_palette_mask_bank.bin"
    ).read_bytes()
    expected_words = len(PROFILE_IDS) * PALETTE_BANKS * PALETTE_COLOURS * 2
    expected_nearest = (
        len(PROFILE_IDS) * PALETTE_BANKS * SOURCE_COLOURS
    )
    expected_masks = len(PROFILE_IDS) * MASK_TABLE_BYTES
    if (
        len(words_source) != expected_words or
        len(nearest_source) != expected_nearest or
        len(mask_source) != expected_masks
    ):
        raise ValueError(
            "background palette asset sizes changed: "
            f"{len(words_source)}/{len(nearest_source)}/{len(mask_source)}"
        )
    return PaletteAssets(
        words=np.frombuffer(
            words_source,
            dtype="<u2",
        ).reshape(len(PROFILE_IDS), PALETTE_BANKS, PALETTE_COLOURS).copy(),
        nearest=np.frombuffer(
            nearest_source,
            dtype=np.uint8,
        ).reshape(len(PROFILE_IDS), PALETTE_BANKS, SOURCE_COLOURS).copy(),
        mask_bank=np.frombuffer(
            mask_source,
            dtype=np.uint8,
        ).reshape(len(PROFILE_IDS), MASK_TABLE_BYTES).copy(),
    )


def assets_to_bytes(assets: PaletteAssets) -> tuple[bytes, bytes, bytes]:
    return (
        assets.words.astype("<u2", copy=False).tobytes(),
        assets.nearest.astype(np.uint8, copy=False).tobytes(),
        assets.mask_bank.astype(np.uint8, copy=False).tobytes(),
    )


def palette_mapping_error(
    words: np.ndarray,
    nearest: np.ndarray,
    source_lab: np.ndarray,
    candidate_lab: np.ndarray,
) -> np.ndarray:
    mapped_codes = np.take_along_axis(words, nearest, axis=1)
    mapped_lab = candidate_lab[mapped_codes]
    delta = mapped_lab - source_lab[None, :, :]
    return np.sum(delta * delta, axis=2)


def palette_metric_error(
    words: np.ndarray,
    nearest: np.ndarray,
    source_values: np.ndarray,
    candidate_values: np.ndarray,
    metric: str,
) -> np.ndarray:
    mapped_codes = np.take_along_axis(words, nearest, axis=1)
    mapped = candidate_values[mapped_codes]
    source = source_values[None, :, :]
    if metric == "squared":
        delta = mapped - source
        return np.sum(delta * delta, axis=2)
    if metric == "euclidean":
        return np.sqrt(np.sum((mapped - source) ** 2, axis=2))
    if metric == "ciede2000":
        return ciede2000(
            np.broadcast_to(source, mapped.shape),
            mapped,
        )
    raise ValueError(f"unsupported metric: {metric}")


def nearest_mapping(
    words: np.ndarray,
    source_lab: np.ndarray,
    candidate_lab: np.ndarray,
) -> tuple[np.ndarray, np.ndarray]:
    centres = candidate_lab[words[:, 1:]]
    delta = (
        source_lab[None, :, None, :] -
        centres[:, None, :, :]
    )
    distance = np.sum(delta * delta, axis=3)
    nearest_zero = np.argmin(distance, axis=2).astype(np.uint8)
    nearest = nearest_zero + 1
    nearest[:, 0] = 0
    error = np.take_along_axis(
        distance,
        nearest_zero[:, :, None],
        axis=2,
    )[:, :, 0]
    error[:, 0] = 0
    return nearest, error


def weighted_quantile(
    values: np.ndarray,
    weights: np.ndarray,
    quantile: float,
) -> float:
    if not 0 <= quantile <= 1:
        raise ValueError(f"invalid quantile: {quantile}")
    order = np.argsort(values)
    sorted_values = values[order]
    sorted_weights = weights[order]
    threshold = quantile * sorted_weights.sum()
    index = int(
        np.searchsorted(
            np.cumsum(sorted_weights),
            threshold,
            side="left",
        )
    )
    return float(sorted_values[min(index, len(sorted_values) - 1)])


def weighted_tail_mean(
    values: np.ndarray,
    weights: np.ndarray,
    alpha: float,
) -> float:
    tail_mass = (1.0 - alpha) * float(weights.sum())
    if tail_mass <= 0:
        return float(values.max())
    order = np.argsort(values)[::-1]
    remaining = tail_mass
    total = 0.0
    for index in order:
        take = min(remaining, float(weights[index]))
        total += take * float(values[index])
        remaining -= take
        if remaining <= 1.0e-12:
            break
    return total / tail_mass


def evaluate_profile(
    dataset: ProfileDataset,
    words: np.ndarray,
    nearest: np.ndarray,
    mask_bank: np.ndarray,
    source_lab: np.ndarray,
    candidate_lab: np.ndarray,
) -> ProfileEvaluation:
    error = palette_mapping_error(
        words,
        nearest,
        source_lab,
        candidate_lab,
    )
    return evaluate_profile_error(dataset, mask_bank, error)


def evaluate_profile_error(
    dataset: ProfileDataset,
    mask_bank: np.ndarray,
    error: np.ndarray,
) -> ProfileEvaluation:
    banks = mask_bank[dataset.masks]
    key_error = np.zeros(len(dataset.keys), dtype=np.float64)
    for bank in range(PALETTE_BANKS):
        selected = banks == bank
        if selected.any():
            key_error[selected] = (
                dataset.histograms[selected].astype(np.float64) @
                error[bank]
            )
    nonzero = dataset.histograms.sum(axis=1).astype(np.float64)
    normalized = key_error / nonzero
    mean_squared = float(
        np.sum(dataset.weights * key_error) /
        np.sum(dataset.weights * nonzero)
    )
    return ProfileEvaluation(
        mean_squared=mean_squared,
        rms=float(np.sqrt(mean_squared)),
        p95=weighted_quantile(normalized, dataset.weights, 0.95),
        p99=weighted_quantile(normalized, dataset.weights, 0.99),
        cvar95=weighted_tail_mean(normalized, dataset.weights, 0.95),
        key_errors=normalized,
        key_raw_errors=key_error,
    )


def aggregate_mask_histograms(
    dataset: ProfileDataset,
    dynamic_weights: np.ndarray,
) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    masks, inverse = np.unique(dataset.masks, return_inverse=True)
    result = np.zeros((len(masks), SOURCE_COLOURS), dtype=np.float64)
    effective = dataset.weights * dynamic_weights
    np.add.at(
        result,
        inverse,
        dataset.histograms.astype(np.float64) * effective[:, None],
    )
    return masks, inverse, result


def choose_unique_candidate(
    mean_lab: np.ndarray,
    candidate_lab: np.ndarray,
    used: set[int],
) -> int:
    distance = np.sum(
        (candidate_lab - mean_lab[None, :]) ** 2,
        axis=1,
    )
    for candidate in np.argsort(distance)[:64]:
        value = int(candidate)
        if value != 0 and value not in used:
            return value
    for candidate in np.argsort(distance):
        value = int(candidate)
        if value != 0 and value not in used:
            return value
    raise ValueError("BGR555 candidate lattice exhausted")


def optimize_bank(
    source_weights: np.ndarray,
    initial_codes: np.ndarray,
    source_lab: np.ndarray,
    candidate_lab: np.ndarray,
    max_iterations: int,
) -> np.ndarray:
    active = np.flatnonzero(source_weights)
    active = active[active != 0]
    if active.size == 0:
        return initial_codes.copy()
    codes = initial_codes.astype(np.uint16, copy=True)
    if len(set(int(value) for value in codes)) != len(codes):
        codes = np.unique(codes)
        seeds = list(int(value) for value in codes if value)
        residual_order = active[
            np.argsort(source_weights[active])[::-1]
        ]
        for source_index in residual_order:
            code = choose_unique_candidate(
                source_lab[source_index],
                candidate_lab,
                set(seeds),
            )
            seeds.append(code)
            if len(seeds) == PALETTE_COLOURS - 1:
                break
        # A small per-level mask can expose fewer than fifteen source
        # indices. Keep filling otherwise-unused centres with the next
        # closest distinct RGB555 codes; they do not affect the active
        # objective, but optimize_bank() must retain a complete 15-colour
        # bank for runtime and for later assignment candidates.
        while len(seeds) < PALETTE_COLOURS - 1:
            source_index = int(
                residual_order[
                    len(seeds) % len(residual_order)
                ]
            )
            seeds.append(
                choose_unique_candidate(
                    source_lab[source_index],
                    candidate_lab,
                    set(seeds),
                )
            )
        codes = np.asarray(seeds[: PALETTE_COLOURS - 1], dtype=np.uint16)
    for _ in range(max_iterations):
        centre_lab = candidate_lab[codes]
        distance = np.sum(
            (
                source_lab[active, None, :] -
                centre_lab[None, :, :]
            ) ** 2,
            axis=2,
        )
        assignment = np.argmin(distance, axis=1)
        cluster_weight = np.asarray(
            [
                source_weights[active[assignment == cluster]].sum()
                for cluster in range(PALETTE_COLOURS - 1)
            ],
            dtype=np.float64,
        )
        order = np.argsort(cluster_weight)[::-1]
        updated = codes.copy()
        used: set[int] = set()
        residual = (
            source_weights[active] *
            np.min(distance, axis=1)
        )
        residual_order = active[np.argsort(residual)[::-1]]
        for cluster in order:
            selected = assignment == cluster
            if selected.any():
                selected_source = active[selected]
                weights = source_weights[selected_source]
                mean_lab = np.sum(
                    source_lab[selected_source] * weights[:, None],
                    axis=0,
                ) / weights.sum()
            else:
                seed_source = next(
                    (
                        int(value)
                        for value in residual_order
                        if int(value) not in used
                    ),
                    int(active[0]),
                )
                mean_lab = source_lab[seed_source]
            code = choose_unique_candidate(
                mean_lab,
                candidate_lab,
                used,
            )
            updated[cluster] = code
            used.add(code)
        if np.array_equal(updated, codes):
            break
        codes = updated
    return codes


def full_mask_table(
    active_masks: np.ndarray,
    active_assignments: np.ndarray,
    colour_error: np.ndarray,
) -> np.ndarray:
    hue_error = np.zeros((16, PALETTE_BANKS), dtype=np.float64)
    for hue in range(16):
        start = hue * 16 + 1
        hue_error[hue] = colour_error[:, start : start + 15].mean(axis=1)
    table = np.zeros(MASK_TABLE_BYTES, dtype=np.uint8)
    active = {
        int(mask): int(bank)
        for mask, bank in zip(
            active_masks,
            active_assignments,
            strict=True,
        )
    }
    for mask in range(1, MASK_TABLE_BYTES):
        assigned = active.get(mask)
        if assigned is not None:
            table[mask] = assigned
            continue
        hues = [
            hue
            for hue in range(16)
            if mask & (1 << hue)
        ]
        table[mask] = int(
            np.argmin(hue_error[hues].sum(axis=0))
        )
    return table


def top_tail_mask(
    errors: np.ndarray,
    weights: np.ndarray,
    alpha: float,
) -> np.ndarray:
    order = np.argsort(errors)[::-1]
    target_mass = (1.0 - alpha) * float(weights.sum())
    selected = np.zeros(len(errors), dtype=bool)
    consumed = 0.0
    for index in order:
        selected[index] = True
        consumed += float(weights[index])
        if consumed >= target_mass:
            break
    return selected


def train_profile(
    dataset: ProfileDataset,
    initial_words: np.ndarray,
    initial_mask_table: np.ndarray,
    source_lab: np.ndarray,
    candidate_lab: np.ndarray,
    fixed_banks: int,
    lambda_tail: float,
    alpha: float,
    outer_iterations: int,
    middle_iterations: int,
    bank_iterations: int,
) -> tuple[np.ndarray, np.ndarray, np.ndarray, list[dict[str, float | int]]]:
    words = initial_words.copy()
    nearest, colour_error = nearest_mapping(
        words,
        source_lab,
        candidate_lab,
    )
    active_masks = np.unique(dataset.masks)
    assignments = initial_mask_table[active_masks].copy()
    dynamic = np.ones(len(dataset.keys), dtype=np.float64)
    best_words = words.copy()
    best_assignments = assignments.copy()
    best_objective = float("inf")
    history: list[dict[str, float | int]] = []
    for outer in range(max(1, outer_iterations)):
        for middle in range(middle_iterations):
            masks, inverse, mask_histograms = aggregate_mask_histograms(
                dataset,
                dynamic,
            )
            if not np.array_equal(masks, active_masks):
                raise ValueError("active mask ordering changed")
            previous_words = words.copy()
            previous_assignments = assignments.copy()
            for bank in range(fixed_banks, PALETTE_BANKS):
                selected_masks = assignments == bank
                bank_histogram = (
                    mask_histograms[selected_masks].sum(axis=0)
                    if selected_masks.any()
                    else np.zeros(SOURCE_COLOURS, dtype=np.float64)
                )
                words[bank, 1:] = optimize_bank(
                    bank_histogram,
                    words[bank, 1:],
                    source_lab,
                    candidate_lab,
                    bank_iterations,
                )
            nearest, colour_error = nearest_mapping(
                words,
                source_lab,
                candidate_lab,
            )
            assignment_cost = mask_histograms @ colour_error.T
            assignments = np.argmin(
                assignment_cost,
                axis=1,
            ).astype(np.uint8)
            for mask_index, mask in enumerate(active_masks):
                value = int(mask)
                if (
                    value != 0 and
                    value & (value - 1) == 0
                ):
                    hue = value.bit_length() - 1
                    if hue < fixed_banks:
                        assignments[mask_index] = hue
            banks = assignments[inverse]
            key_raw = np.zeros(len(dataset.keys), dtype=np.float64)
            for bank in range(PALETTE_BANKS):
                selected = banks == bank
                if selected.any():
                    key_raw[selected] = (
                        dataset.histograms[selected].astype(np.float64) @
                        colour_error[bank]
                    )
            pixel_mass = dataset.histograms.sum(axis=1).astype(np.float64)
            mean_squared = float(
                np.sum(dataset.weights * key_raw) /
                np.sum(dataset.weights * pixel_mass)
            )
            normalized = key_raw / pixel_mass
            cvar = weighted_tail_mean(
                normalized,
                dataset.weights,
                alpha,
            )
            objective = mean_squared + lambda_tail * cvar
            history.append({
                "outer": outer,
                "middle": middle,
                "mean_squared": mean_squared,
                "cvar": cvar,
                "objective": objective,
                "assignment_changes": int(
                    np.count_nonzero(assignments != previous_assignments)
                ),
                "palette_changes": int(
                    np.count_nonzero(words != previous_words)
                ),
            })
            if objective < best_objective:
                best_objective = objective
                best_words = words.copy()
                best_assignments = assignments.copy()
            if (
                np.array_equal(words, previous_words) and
                np.array_equal(assignments, previous_assignments)
            ):
                break
        words = best_words.copy()
        assignments = best_assignments.copy()
        nearest, colour_error = nearest_mapping(
            words,
            source_lab,
            candidate_lab,
        )
        banks = assignments[
            np.searchsorted(active_masks, dataset.masks)
        ]
        raw = np.zeros(len(dataset.keys), dtype=np.float64)
        for bank in range(PALETTE_BANKS):
            selected = banks == bank
            if selected.any():
                raw[selected] = (
                    dataset.histograms[selected].astype(np.float64) @
                    colour_error[bank]
                )
        normalized = (
            raw /
            dataset.histograms.sum(axis=1).astype(np.float64)
        )
        if lambda_tail == 0:
            break
        tail = top_tail_mask(normalized, dataset.weights, alpha)
        target = np.ones_like(dynamic)
        target[tail] += lambda_tail / (1.0 - alpha)
        updated_dynamic = 0.5 * dynamic + 0.5 * target
        if np.allclose(updated_dynamic, dynamic, rtol=0, atol=1.0e-9):
            break
        dynamic = updated_dynamic
    words = best_words
    nearest, colour_error = nearest_mapping(
        words,
        source_lab,
        candidate_lab,
    )
    mask_table = full_mask_table(
        active_masks,
        best_assignments,
        colour_error,
    )
    return words, nearest, mask_table, history


def train_assets(
    datasets: dict[str, ProfileDataset],
    baseline: PaletteAssets,
    source_lab: np.ndarray,
    candidate_lab: np.ndarray,
    fixed_banks: int,
    lambda_tail: float,
    alpha: float,
    outer_iterations: int,
    middle_iterations: int,
    bank_iterations: int,
) -> TrainingResult:
    words = baseline.words.copy()
    nearest = baseline.nearest.copy()
    mask_bank = baseline.mask_bank.copy()
    reports: list[dict[str, float | int | str]] = []
    for profile_index, profile in enumerate(PROFILE_IDS):
        (
            words[profile_index],
            nearest[profile_index],
            mask_bank[profile_index],
            history,
        ) = train_profile(
            datasets[profile],
            baseline.words[profile_index],
            baseline.mask_bank[profile_index],
            source_lab,
            candidate_lab,
            fixed_banks,
            lambda_tail,
            alpha,
            outer_iterations,
            middle_iterations,
            bank_iterations,
        )
        for record in history:
            reports.append({"profile": profile, **record})
    return TrainingResult(
        assets=PaletteAssets(
            words=words,
            nearest=nearest,
            mask_bank=mask_bank,
        ),
        iterations=reports,
    )


def ramp_pair_counts(
    mask: int,
    bank: int,
    words: np.ndarray,
    nearest: np.ndarray,
    candidate_lab: np.ndarray,
) -> tuple[int, int]:
    inversions = 0
    collisions = 0
    for hue in range(16):
        if not mask & (1 << hue):
            continue
        source_indices = np.arange(
            hue * 16 + 1,
            hue * 16 + 16,
        )
        local = nearest[bank, source_indices]
        mapped_words = words[bank, local]
        mapped_lightness = candidate_lab[mapped_words, 0]
        inversions += int(
            np.count_nonzero(
                np.diff(mapped_lightness) < -1.0e-12
            )
        )
        collisions += int(
            np.count_nonzero(
                np.diff(mapped_words) == 0
            )
        )
    return inversions, collisions


def pareto_nearest_mapping(
    words: np.ndarray,
    baseline_nearest: np.ndarray,
    source_lab: np.ndarray,
    candidate_lab: np.ndarray,
    source_cielab: np.ndarray,
    candidate_cielab: np.ndarray,
) -> np.ndarray:
    """Choose mappings non-regressing in OKLab and CIEDE2000."""
    mapped_oklab = candidate_lab[words[:, 1:]][:, None, :, :]
    source_oklab = source_lab[None, :, None, :]
    oklab_error = np.sum(
        (mapped_oklab - source_oklab) ** 2,
        axis=3,
    )
    mapped_cielab = candidate_cielab[words[:, 1:]][:, None, :, :]
    source_cie = source_cielab[None, :, None, :]
    comparison_shape = (
        PALETTE_BANKS,
        SOURCE_COLOURS,
        PALETTE_COLOURS - 1,
        3,
    )
    cie_error = ciede2000(
        np.broadcast_to(source_cie, comparison_shape),
        np.broadcast_to(mapped_cielab, comparison_shape),
    )
    baseline_index = np.clip(
        baseline_nearest.astype(np.int16) - 1,
        0,
        PALETTE_COLOURS - 2,
    )
    baseline_oklab = np.take_along_axis(
        oklab_error,
        baseline_index[:, :, None],
        axis=2,
    )
    baseline_cie = np.take_along_axis(
        cie_error,
        baseline_index[:, :, None],
        axis=2,
    )
    viable = (
        (oklab_error <= baseline_oklab + 1.0e-12) &
        (cie_error <= baseline_cie + 1.0e-9)
    )
    result = (
        np.argmin(
            np.where(viable, cie_error, np.inf),
            axis=2,
        ).astype(np.uint8) +
        1
    )
    result[:, 0] = 0
    return result


def train_profile_safe_unused(
    dataset: ProfileDataset,
    baseline_words: np.ndarray,
    baseline_nearest: np.ndarray,
    baseline_mask_table: np.ndarray,
    source_lab: np.ndarray,
    candidate_lab: np.ndarray,
    source_cielab: np.ndarray,
    candidate_cielab: np.ndarray,
    middle_iterations: int,
    bank_iterations: int,
) -> tuple[
    np.ndarray,
    np.ndarray,
    np.ndarray,
    list[dict[str, float | int]],
]:
    """Train only banks unused by stock data, then accept safe masks.

    Every stock mask retains its original v53 bank as an exact fallback.
    A trained bank is selected only if every runtime key carrying the mask
    is non-regressing in both OKLab squared error and CIEDE2000, and its
    source ramps introduce neither extra lightness inversions nor adjacent
    palette collisions.
    """
    active_masks, inverse, mask_histograms = aggregate_mask_histograms(
        dataset,
        np.ones(len(dataset.keys), dtype=np.float64),
    )
    if active_masks.size == 0:
        return (
            baseline_words.copy(),
            baseline_nearest.copy(),
            baseline_mask_table.copy(),
            [{
                "protected_banks": 0,
                "trainable_banks": 0,
                "pareto_protected_banks": 0,
                "safe_active_mask_changes": 0,
                "active_masks": 0,
            }],
        )
    protected_banks = np.unique(
        baseline_mask_table[active_masks]
    ).astype(np.uint8)
    protected_set = {
        int(value)
        for value in protected_banks
    }
    trainable_banks = np.asarray(
        [
            bank
            for bank in range(PALETTE_BANKS)
            if bank not in protected_set
        ],
        dtype=np.uint8,
    )
    words = baseline_words.copy()
    nearest = baseline_nearest.copy()
    proposed_pareto = pareto_nearest_mapping(
        words,
        baseline_nearest,
        source_lab,
        candidate_lab,
        source_cielab,
        candidate_cielab,
    )
    pareto_banks = 0
    for bank_value in protected_banks:
        bank = int(bank_value)
        bank_masks = active_masks[
            baseline_mask_table[active_masks] == bank
        ]
        safe_ramps = True
        for mask_value in bank_masks:
            mask = int(mask_value)
            baseline_ramp = ramp_pair_counts(
                mask,
                bank,
                baseline_words,
                baseline_nearest,
                candidate_lab,
            )
            candidate_ramp = ramp_pair_counts(
                mask,
                bank,
                words,
                proposed_pareto,
                candidate_lab,
            )
            if (
                candidate_ramp[0] > baseline_ramp[0] or
                candidate_ramp[1] > baseline_ramp[1]
            ):
                safe_ramps = False
                break
        if safe_ramps:
            nearest[bank] = proposed_pareto[bank]
            pareto_banks += 1
    history: list[dict[str, float | int]] = [{
        "protected_banks": len(protected_banks),
        "trainable_banks": len(trainable_banks),
        "pareto_protected_banks": pareto_banks,
    }]
    if trainable_banks.size == 0:
        return (
            words,
            nearest,
            baseline_mask_table.copy(),
            history,
        )

    baseline_ok_error = palette_mapping_error(
        baseline_words,
        baseline_nearest,
        source_lab,
        candidate_lab,
    )
    baseline_banks = baseline_mask_table[active_masks]
    mask_pixel_mass = mask_histograms.sum(axis=1)
    baseline_mask_error = np.asarray(
        [
            float(
                mask_histograms[index] @
                baseline_ok_error[int(baseline_banks[index])]
            ) /
            float(mask_pixel_mass[index])
            for index in range(len(active_masks))
        ],
        dtype=np.float64,
    )
    seed_order = np.argsort(
        baseline_mask_error *
        np.bincount(
            inverse,
            weights=dataset.weights,
            minlength=len(active_masks),
        )
    )[::-1]
    for seed_index, bank_value in enumerate(trainable_banks):
        mask_index = int(seed_order[seed_index % len(seed_order)])
        bank = int(bank_value)
        words[bank, 1:] = optimize_bank(
            mask_histograms[mask_index],
            words[bank, 1:],
            source_lab,
            candidate_lab,
            bank_iterations,
        )

    assignments = baseline_banks.copy()
    for iteration in range(max(1, middle_iterations)):
        calculated_nearest, _ = nearest_mapping(
            words,
            source_lab,
            candidate_lab,
        )
        nearest[trainable_banks] = calculated_nearest[trainable_banks]
        colour_error = palette_mapping_error(
            words,
            nearest,
            source_lab,
            candidate_lab,
        )
        previous_words = words.copy()
        previous_assignments = assignments.copy()
        assignments = np.argmin(
            mask_histograms @ colour_error.T,
            axis=1,
        ).astype(np.uint8)
        for bank_value in trainable_banks:
            bank = int(bank_value)
            selected = assignments == bank
            if not selected.any():
                continue
            words[bank, 1:] = optimize_bank(
                mask_histograms[selected].sum(axis=0),
                words[bank, 1:],
                source_lab,
                candidate_lab,
                bank_iterations,
            )
        history.append({
            "middle": iteration,
            "assignment_changes": int(
                np.count_nonzero(
                    assignments != previous_assignments
                )
            ),
            "palette_changes": int(
                np.count_nonzero(words != previous_words)
            ),
        })
        if (
            np.array_equal(words, previous_words) and
            np.array_equal(assignments, previous_assignments)
        ):
            break

    calculated_nearest, _ = nearest_mapping(
        words,
        source_lab,
        candidate_lab,
    )
    nearest[trainable_banks] = calculated_nearest[trainable_banks]
    candidate_ok_error = palette_mapping_error(
        words,
        nearest,
        source_lab,
        candidate_lab,
    )
    baseline_cie_error = palette_metric_error(
        baseline_words,
        baseline_nearest,
        source_cielab,
        candidate_cielab,
        "ciede2000",
    )
    candidate_cie_error = palette_metric_error(
        words,
        nearest,
        source_cielab,
        candidate_cielab,
        "ciede2000",
    )
    baseline_ok_evaluation = evaluate_profile_error(
        dataset,
        baseline_mask_table,
        baseline_ok_error,
    )
    baseline_cie_evaluation = evaluate_profile_error(
        dataset,
        baseline_mask_table,
        baseline_cie_error,
    )
    mask_table = baseline_mask_table.copy()
    safe_changes = 0
    for mask_index, mask_value in enumerate(active_masks):
        mask = int(mask_value)
        selected = inverse == mask_index
        histograms = dataset.histograms[selected].astype(np.float64)
        pixel_mass = histograms.sum(axis=1)
        weights = dataset.weights[selected]
        baseline_ok = baseline_ok_evaluation.key_errors[selected]
        baseline_cie = baseline_cie_evaluation.key_errors[selected]
        baseline_bank = int(baseline_mask_table[mask])
        baseline_ramp = ramp_pair_counts(
            mask,
            baseline_bank,
            baseline_words,
            baseline_nearest,
            candidate_lab,
        )
        best_bank = baseline_bank
        best_score = 2.0
        baseline_ok_mean = max(
            float(np.average(baseline_ok, weights=weights)),
            1.0e-20,
        )
        baseline_cie_mean = max(
            float(np.average(baseline_cie, weights=weights)),
            1.0e-20,
        )
        for bank in range(PALETTE_BANKS):
            key_ok = (
                histograms @ candidate_ok_error[bank]
            ) / pixel_mass
            if np.any(key_ok > baseline_ok + 1.0e-12):
                continue
            key_cie = (
                histograms @ candidate_cie_error[bank]
            ) / pixel_mass
            if np.any(key_cie > baseline_cie + 1.0e-9):
                continue
            candidate_ramp = ramp_pair_counts(
                mask,
                bank,
                words,
                nearest,
                candidate_lab,
            )
            if (
                candidate_ramp[0] > baseline_ramp[0] or
                candidate_ramp[1] > baseline_ramp[1]
            ):
                continue
            score = (
                float(np.average(key_ok, weights=weights)) /
                baseline_ok_mean +
                float(np.average(key_cie, weights=weights)) /
                baseline_cie_mean
            )
            if score < best_score - 1.0e-12:
                best_score = score
                best_bank = bank
        mask_table[mask] = best_bank
        if best_bank != baseline_bank:
            safe_changes += 1

    # Stock data covers every runtime mask used by all 62 bundled levels.
    # For unknown/modded masks, never leave an altered formerly-unused bank
    # selected accidentally: choose only among preserved v53 banks.
    protected_list = [int(value) for value in protected_banks]
    hue_error = np.zeros(
        (16, len(protected_list)),
        dtype=np.float64,
    )
    for hue in range(16):
        source_indices = np.arange(
            hue * 16 + 1,
            hue * 16 + 16,
        )
        for index, bank in enumerate(protected_list):
            hue_error[hue, index] = float(
                baseline_ok_error[bank, source_indices].mean()
            )
    active_set = {
        int(value)
        for value in active_masks
    }
    trainable_set = {
        int(value)
        for value in trainable_banks
    }
    for mask in range(1, MASK_TABLE_BYTES):
        if (
            mask in active_set or
            int(mask_table[mask]) not in trainable_set
        ):
            continue
        hues = [
            hue
            for hue in range(16)
            if mask & (1 << hue)
        ]
        mask_table[mask] = protected_list[
            int(np.argmin(hue_error[hues].sum(axis=0)))
        ]
    history.append({
        "safe_active_mask_changes": safe_changes,
        "active_masks": len(active_masks),
    })
    return words, nearest, mask_table, history


def train_assets_safe_unused(
    datasets: dict[str, ProfileDataset],
    baseline: PaletteAssets,
    source_lab: np.ndarray,
    candidate_lab: np.ndarray,
    source_cielab: np.ndarray,
    candidate_cielab: np.ndarray,
    middle_iterations: int,
    bank_iterations: int,
) -> TrainingResult:
    validate_ciede2000()
    words = baseline.words.copy()
    nearest = baseline.nearest.copy()
    mask_bank = baseline.mask_bank.copy()
    reports: list[dict[str, float | int | str]] = []
    for profile_index, profile in enumerate(PROFILE_IDS):
        (
            words[profile_index],
            nearest[profile_index],
            mask_bank[profile_index],
            history,
        ) = train_profile_safe_unused(
            datasets[profile],
            baseline.words[profile_index],
            baseline.nearest[profile_index],
            baseline.mask_bank[profile_index],
            source_lab,
            candidate_lab,
            source_cielab,
            candidate_cielab,
            middle_iterations,
            bank_iterations,
        )
        for record in history:
            reports.append({"profile": profile, **record})
    return TrainingResult(
        assets=PaletteAssets(
            words=words,
            nearest=nearest,
            mask_bank=mask_bank,
        ),
        iterations=reports,
    )


def profile_report(
    dataset: ProfileDataset,
    baseline: ProfileEvaluation,
    candidate: ProfileEvaluation,
) -> dict[str, object]:
    delta = candidate.key_errors - baseline.key_errors
    tolerance = 1.0e-12
    regressed = delta > tolerance
    improved = delta < -tolerance
    occurrence_total = float(dataset.weights.sum())

    return {
        "profile": dataset.profile_id,
        "levels": dataset.level_count,
        "unique_keys": len(dataset.keys),
        "active_masks": len(np.unique(dataset.masks)),
        "map_tile_occurrences": dataset.map_tile_count,
        "dataset_sha256": dataset.checksum,
        "baseline_mean_squared": baseline.mean_squared,
        "candidate_mean_squared": candidate.mean_squared,
        "mean_improvement_percent": improvement_percent(
            baseline.mean_squared,
            candidate.mean_squared,
        ),
        "baseline_rms": baseline.rms,
        "candidate_rms": candidate.rms,
        "baseline_p95": baseline.p95,
        "candidate_p95": candidate.p95,
        "p95_improvement_percent": improvement_percent(
            baseline.p95,
            candidate.p95,
        ),
        "baseline_p99": baseline.p99,
        "candidate_p99": candidate.p99,
        "p99_improvement_percent": improvement_percent(
            baseline.p99,
            candidate.p99,
        ),
        "baseline_cvar95": baseline.cvar95,
        "candidate_cvar95": candidate.cvar95,
        "cvar95_improvement_percent": improvement_percent(
            baseline.cvar95,
            candidate.cvar95,
        ),
        "regressed_keys": int(np.count_nonzero(regressed)),
        "improved_keys": int(np.count_nonzero(improved)),
        "regressed_map_occurrence_percent": (
            100.0 * float(dataset.weights[regressed].sum()) /
            occurrence_total
        ),
        "improved_map_occurrence_percent": (
            100.0 * float(dataset.weights[improved].sum()) /
            occurrence_total
        ),
        "worst_key_regression": float(
            delta[regressed].max()
            if regressed.any()
            else 0.0
        ),
        "weighted_positive_regression": float(
            np.sum(
                dataset.weights *
                np.maximum(delta, 0.0)
            ) /
            occurrence_total
        ),
    }


def improvement_percent(baseline: float, candidate: float) -> float:
    if baseline == 0:
        return 0.0
    return (baseline - candidate) * 100.0 / baseline


def comparison_report(
    dataset: ProfileDataset,
    baseline: ProfileEvaluation,
    candidate: ProfileEvaluation,
) -> dict[str, float | int]:
    delta = candidate.key_errors - baseline.key_errors
    regressed = delta > 1.0e-9
    result: dict[str, float | int] = {
        "baseline_mean": baseline.mean_squared,
        "candidate_mean": candidate.mean_squared,
        "mean_improvement_percent": improvement_percent(
            baseline.mean_squared,
            candidate.mean_squared,
        ),
        "baseline_p95": baseline.p95,
        "candidate_p95": candidate.p95,
        "p95_improvement_percent": improvement_percent(
            baseline.p95,
            candidate.p95,
        ),
        "baseline_p99": baseline.p99,
        "candidate_p99": candidate.p99,
        "p99_improvement_percent": improvement_percent(
            baseline.p99,
            candidate.p99,
        ),
        "baseline_cvar95": baseline.cvar95,
        "candidate_cvar95": candidate.cvar95,
        "cvar95_improvement_percent": improvement_percent(
            baseline.cvar95,
            candidate.cvar95,
        ),
        "regressed_keys": int(np.count_nonzero(regressed)),
        "regressed_map_occurrence_percent": (
            100.0 * float(dataset.weights[regressed].sum()) /
            float(dataset.weights.sum())
        ),
        "worst_key_regression": float(
            delta[regressed].max()
            if regressed.any()
            else 0.0
        ),
    }
    return result


def ramp_report(
    dataset: ProfileDataset,
    profile_index: int,
    assets: PaletteAssets,
    candidate_lab: np.ndarray,
) -> dict[str, float | int]:
    masks, inverse = np.unique(
        dataset.masks,
        return_inverse=True,
    )
    mask_histograms = np.zeros(
        (len(masks), SOURCE_COLOURS),
        dtype=np.float64,
    )
    np.add.at(
        mask_histograms,
        inverse,
        dataset.histograms.astype(np.float64) *
        dataset.weights[:, None],
    )
    inversion_pairs = 0
    collision_pairs = 0
    pair_count = 0
    weighted_inversions = 0.0
    weighted_collisions = 0.0
    weighted_pairs = 0.0
    for mask_index, mask in enumerate(masks):
        mask_value = int(mask)
        bank = int(assets.mask_bank[profile_index, mask_value])
        for hue in range(16):
            if not mask_value & (1 << hue):
                continue
            source_indices = np.arange(
                hue * 16 + 1,
                hue * 16 + 16,
            )
            local = assets.nearest[
                profile_index,
                bank,
                source_indices,
            ]
            words = assets.words[profile_index, bank, local]
            lightness = candidate_lab[words, 0]
            inversions = np.diff(lightness) < -1.0e-12
            collisions = np.diff(words) == 0
            pair_weights = np.minimum(
                mask_histograms[mask_index, source_indices[:-1]],
                mask_histograms[mask_index, source_indices[1:]],
            )
            inversion_pairs += int(np.count_nonzero(inversions))
            collision_pairs += int(np.count_nonzero(collisions))
            pair_count += len(inversions)
            weighted_inversions += float(pair_weights[inversions].sum())
            weighted_collisions += float(pair_weights[collisions].sum())
            weighted_pairs += float(pair_weights.sum())
    return {
        "adjacent_pairs": pair_count,
        "lightness_inversions": inversion_pairs,
        "palette_collisions": collision_pairs,
        "weighted_lightness_inversion_percent": (
            0.0
            if weighted_pairs == 0
            else 100.0 * weighted_inversions / weighted_pairs
        ),
        "weighted_palette_collision_percent": (
            0.0
            if weighted_pairs == 0
            else 100.0 * weighted_collisions / weighted_pairs
        ),
    }


def mapped_tile_rgb(
    pixels: np.ndarray,
    mask: int,
    profile_index: int,
    assets: PaletteAssets,
    candidate_rgb: np.ndarray,
) -> np.ndarray:
    bank = int(assets.mask_bank[profile_index, mask])
    local = assets.nearest[profile_index, bank, pixels]
    words = assets.words[profile_index, bank, local]
    return candidate_rgb[words]


def write_worst_tile_preview(
    path: Path,
    datasets: dict[str, ProfileDataset],
    shapes_by_profile: dict[str, np.ndarray],
    source_rgb: np.ndarray,
    candidate_rgb: np.ndarray,
    baseline_assets: PaletteAssets,
    candidate_assets: PaletteAssets,
    baseline_evaluations: dict[str, ProfileEvaluation],
    candidate_evaluations: dict[str, ProfileEvaluation],
) -> None:
    improved_rows: list[tuple[str, int, float]] = []
    regressed_rows: list[tuple[str, int, float]] = []
    for profile in PROFILE_IDS:
        delta = (
            baseline_evaluations[profile].key_errors -
            candidate_evaluations[profile].key_errors
        )
        for index in np.argsort(delta)[::-1][:6]:
            if delta[index] > 0:
                improved_rows.append(
                    (profile, int(index), float(delta[index]))
                )
        for index in np.argsort(delta)[:6]:
            if delta[index] < -1.0e-12:
                regressed_rows.append(
                    (profile, int(index), float(delta[index]))
                )
    improved_rows.sort(key=lambda item: item[2], reverse=True)
    regressed_rows.sort(key=lambda item: item[2])
    rows: list[tuple[str, int, float] | None] = (
        [None] +
        improved_rows[:20] +
        [None] +
        regressed_rows[:20]
    )
    scale = 5
    cell = 8 * scale
    label_width = 312
    image = Image.new(
        "RGB",
        (label_width + cell * 3 + 24, len(rows) * (cell + 8) + 24),
        (20, 20, 24),
    )
    draw = ImageDraw.Draw(image)
    draw.text((label_width + 4, 4), "PC", fill=(255, 255, 255))
    draw.text((label_width + cell + 8, 4), "v53", fill=(255, 255, 255))
    draw.text(
        (label_width + cell * 2 + 12, 4),
        "candidate",
        fill=(255, 255, 255),
    )
    section = 0
    for row, item in enumerate(rows):
        y = 24 + row * (cell + 8)
        if item is None:
            heading = (
                "Largest OKLab improvements"
                if section == 0
                else "Worst OKLab regressions"
            )
            draw.text((4, y + 10), heading, fill=(255, 220, 120))
            section += 1
            continue
        profile, index, delta = item
        dataset = datasets[profile]
        key = int(dataset.keys[index])
        pixels = render_key(key, shapes_by_profile[profile])
        mask = int(dataset.masks[index])
        profile_index = PROFILE_INDEX[profile]
        source = source_rgb[pixels]
        before = mapped_tile_rgb(
            pixels,
            mask,
            profile_index,
            baseline_assets,
            candidate_rgb,
        )
        after = mapped_tile_rgb(
            pixels,
            mask,
            profile_index,
            candidate_assets,
            candidate_rgb,
        )
        draw.text(
            (4, y + 10),
            (
                f"{profile} key={key:08x} mask={mask:04x} "
                f"{'better' if delta >= 0 else 'worse'} "
                f"{delta:+.6f}"
            ),
            fill=(220, 240, 220) if delta >= 0 else (255, 180, 180),
        )
        for column, tile in enumerate((source, before, after)):
            tile_image = Image.fromarray(
                tile.astype(np.uint8),
                mode="RGB",
            ).resize((cell, cell), Image.Resampling.NEAREST)
            image.paste(
                tile_image,
                (label_width + column * (cell + 4), y),
            )
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--project-root", type=Path, required=True)
    parser.add_argument("--baseline-dir", type=Path)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument(
        "--strategy",
        choices=("joint", "safe-unused"),
        default="joint",
    )
    parser.add_argument("--fixed-banks", type=int, default=11)
    parser.add_argument("--lambda-tail", type=float, default=0.0)
    parser.add_argument("--alpha", type=float, default=0.95)
    parser.add_argument("--outer-iterations", type=int, default=4)
    parser.add_argument("--middle-iterations", type=int, default=30)
    parser.add_argument("--bank-iterations", type=int, default=20)
    args = parser.parse_args()

    project_root = args.project_root.resolve()
    data_root = project_root / "vendor" / "tyrian" / "data"
    baseline_dir = (
        args.baseline_dir.resolve()
        if args.baseline_dir
        else project_root / "res"
    )
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    if not 0 <= args.fixed_banks <= PALETTE_BANKS:
        raise ValueError("--fixed-banks must be between 0 and 16")
    if not 0 <= args.lambda_tail:
        raise ValueError("--lambda-tail cannot be negative")
    if not 0 < args.alpha < 1:
        raise ValueError("--alpha must be between zero and one")

    datasets, shapes_by_profile, dataset_metadata = (
        build_runtime_datasets(data_root)
    )
    source_rgb = load_source_rgb(data_root)
    source_lab = rgb_code_to_oklab(source_rgb)
    candidate_rgb = bgr555_rgb()
    candidate_lab = rgb_code_to_oklab(candidate_rgb)
    source_code = source_rgb.astype(np.float64) / 255.0
    candidate_code = candidate_rgb.astype(np.float64) / 255.0
    source_linear = srgb_to_linear(source_code)
    candidate_linear = srgb_to_linear(candidate_code)
    source_cielab = linear_srgb_to_cielab(source_linear)
    candidate_cielab = linear_srgb_to_cielab(candidate_linear)
    baseline = load_assets(baseline_dir)
    baseline_evaluations: dict[str, ProfileEvaluation] = {}
    for profile_index, profile in enumerate(PROFILE_IDS):
        baseline_evaluations[profile] = evaluate_profile(
            datasets[profile],
            baseline.words[profile_index],
            baseline.nearest[profile_index],
            baseline.mask_bank[profile_index],
            source_lab,
            candidate_lab,
        )

    if args.strategy == "safe-unused":
        training = train_assets_safe_unused(
            datasets,
            baseline,
            source_lab,
            candidate_lab,
            source_cielab,
            candidate_cielab,
            args.middle_iterations,
            args.bank_iterations,
        )
    else:
        training = train_assets(
            datasets,
            baseline,
            source_lab,
            candidate_lab,
            args.fixed_banks,
            args.lambda_tail,
            args.alpha,
            args.outer_iterations,
            args.middle_iterations,
            args.bank_iterations,
        )
    candidate_evaluations: dict[str, ProfileEvaluation] = {}
    profiles = []
    independent_metric_spaces = {
        "srgb_code_distance_squared": (
            source_code,
            candidate_code,
            "squared",
        ),
        "linear_rgb_distance_squared": (
            source_linear,
            candidate_linear,
            "squared",
        ),
        "oklab_delta_e": (
            source_lab,
            candidate_lab,
            "euclidean",
        ),
        "ciede2000": (
            source_cielab,
            candidate_cielab,
            "ciede2000",
        ),
    }
    for profile_index, profile in enumerate(PROFILE_IDS):
        dataset = datasets[profile]
        candidate_evaluations[profile] = evaluate_profile(
            dataset,
            training.assets.words[profile_index],
            training.assets.nearest[profile_index],
            training.assets.mask_bank[profile_index],
            source_lab,
            candidate_lab,
        )
        item = profile_report(
            dataset,
            baseline_evaluations[profile],
            candidate_evaluations[profile],
        )
        independent_metrics = {}
        for (
            metric_name,
            (source_values, candidate_values, metric),
        ) in independent_metric_spaces.items():
            baseline_error = palette_metric_error(
                baseline.words[profile_index],
                baseline.nearest[profile_index],
                source_values,
                candidate_values,
                metric,
            )
            candidate_error = palette_metric_error(
                training.assets.words[profile_index],
                training.assets.nearest[profile_index],
                source_values,
                candidate_values,
                metric,
            )
            independent_metrics[metric_name] = comparison_report(
                dataset,
                evaluate_profile_error(
                    dataset,
                    baseline.mask_bank[profile_index],
                    baseline_error,
                ),
                evaluate_profile_error(
                    dataset,
                    training.assets.mask_bank[profile_index],
                    candidate_error,
                ),
            )
        item["independent_metrics"] = independent_metrics
        item["ramp_quality"] = {
            "baseline": ramp_report(
                dataset,
                profile_index,
                baseline,
                candidate_lab,
            ),
            "candidate": ramp_report(
                dataset,
                profile_index,
                training.assets,
                candidate_lab,
            ),
        }
        profiles.append(item)

    palette_bytes, nearest_bytes, mask_bytes = assets_to_bytes(
        training.assets
    )
    (output / "background_gba_palette.bin").write_bytes(palette_bytes)
    (output / "background_palette_nearest.bin").write_bytes(nearest_bytes)
    (output / "background_palette_mask_bank.bin").write_bytes(mask_bytes)
    write_worst_tile_preview(
        output / "worst_tile_comparison.png",
        datasets,
        shapes_by_profile,
        source_rgb,
        candidate_rgb,
        baseline,
        training.assets,
        baseline_evaluations,
        candidate_evaluations,
    )
    report = {
        "mode": (
            "runtime-key safe-unused OKLab+CIEDE2000"
            if args.strategy == "safe-unused"
            else "runtime-key mask-constrained OKLab"
        ),
        "strategy": args.strategy,
        "fixed_banks": args.fixed_banks,
        "lambda_tail": args.lambda_tail,
        "alpha": args.alpha,
        "dataset": dataset_metadata,
        "profiles": profiles,
        "iterations": training.iterations,
        "palette_sha256": hashlib.sha256(palette_bytes).hexdigest(),
        "nearest_sha256": hashlib.sha256(nearest_bytes).hexdigest(),
        "mask_sha256": hashlib.sha256(mask_bytes).hexdigest(),
    }
    (output / "report.json").write_text(
        json.dumps(report, indent=2),
        encoding="utf-8",
    )
    print(json.dumps({
        "dataset": dataset_metadata,
        "profiles": profiles,
        "output": str(output),
    }, indent=2))


if __name__ == "__main__":
    main()
