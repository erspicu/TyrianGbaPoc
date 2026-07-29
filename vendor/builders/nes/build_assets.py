#!/usr/bin/env python3
"""Build NES-ready Tyrian title, level, sprite, and audio assets."""

from __future__ import annotations

import argparse
import collections
import re
import struct
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


NES_RGB = (
    (84, 84, 84), (0, 30, 116), (8, 16, 144), (48, 0, 136),
    (68, 0, 100), (92, 0, 48), (84, 4, 0), (60, 24, 0),
    (32, 42, 0), (8, 58, 0), (0, 64, 0), (0, 60, 0),
    (0, 50, 60), (0, 0, 0), (0, 0, 0), (0, 0, 0),
    (152, 150, 152), (8, 76, 196), (48, 50, 236), (92, 30, 228),
    (136, 20, 176), (160, 20, 100), (152, 34, 32), (120, 60, 0),
    (84, 90, 0), (40, 114, 0), (8, 124, 0), (0, 118, 40),
    (0, 102, 120), (0, 0, 0), (0, 0, 0), (0, 0, 0),
    (236, 238, 236), (76, 154, 236), (120, 124, 236), (176, 98, 236),
    (228, 84, 236), (236, 88, 180), (236, 106, 100), (212, 136, 32),
    (160, 170, 0), (116, 196, 0), (76, 208, 32), (56, 204, 108),
    (56, 180, 204), (60, 60, 60), (0, 0, 0), (0, 0, 0),
    (236, 238, 236), (168, 204, 236), (188, 188, 236), (212, 178, 236),
    (236, 174, 236), (236, 174, 212), (236, 180, 176), (228, 196, 144),
    (204, 210, 120), (180, 222, 120), (168, 226, 144), (152, 226, 180),
    (160, 214, 228), (160, 162, 160), (0, 0, 0), (0, 0, 0),
)

TITLE_PALETTE = (0x0F, 0x06, 0x16, 0x30)
LEVEL_PALETTE = (0x0F, 0x07, 0x17, 0x27)
LEVEL_PALETTES = (
    LEVEL_PALETTE,                 # rock / soil
    (0x0F, 0x0C, 0x2C, 0x30),     # MAP 2 ice / water
    (0x0F, 0x0A, 0x1A, 0x2A),     # green terrain
    (0x0F, 0x00, 0x10, 0x20),     # metal / neutral highlights
)
SPRITE_PALETTES = (
    # High-contrast remaps are deliberate: the original bronze sprites
    # otherwise disappear into level 1's brown/orange background.
    (0x0F, 0x0C, 0x21, 0x30),  # player: cyan, blue, white
    (0x0F, 0x01, 0x16, 0x30),  # enemies: navy, red, white
    (0x0F, 0x04, 0x24, 0x30),  # boss: purple, magenta, white
    # Ground components plus shots/explosions: soil bronze, orange, white.
    (0x0F, 0x07, 0x17, 0x30),
)

LEVEL_SOURCE_FIRST_ROW = 3
LEVEL_SOURCE_LAST_ROW = 292
LEVEL_LAYER2_FIRST_ROW = 14
LEVEL_LAYER2_LAST_ROW = 593
LEVEL_MAP_ROWS = 1015
LEVEL_BOSS_FRAME = 5400
LEVEL_END_FRAME = 8100
MMC5_LEVEL_CHR_FIRST_BANK = 1
MMC5_LEVEL_CHR_MAX_BANKS = 16
LEVEL_SPAWN_TYPES = {6, 7, 10, 12, 15, 17, 18, 23, 32, 49, 50, 51, 52, 56}
LEVEL_CONTROL_TYPES = {19, 20, 21, 27, 31}

EVENT_MOVE = 0x80
EVENT_ACCEL = 0x81
EVENT_REVERSE = 0x82
EVENT_FIRE = 0x83
EVENT_FOREGROUND = 0x84
EVENT_WAIT = 0xFE
EVENT_END = 0xFF

LAYER_SKY = 0
LAYER_GROUND = 1
LAYER_TOP = 2
LAYER_GROUND_2 = 3
LAYER_BOTTOM = 4


def nearest_colour(rgb: tuple[int, int, int], choices: list[tuple[int, int, int]]) -> int:
    return min(
        range(len(choices)),
        key=lambda index: sum(
            (rgb[channel] - choices[index][channel]) ** 2
            for channel in range(3)
        ),
    )


def nearest_luminance(
    rgb: tuple[int, int, int],
    choices: list[tuple[int, int, int]],
) -> int:
    source_luma = 77 * rgb[0] + 150 * rgb[1] + 29 * rgb[2]
    return min(
        range(len(choices)),
        key=lambda index: abs(
            source_luma -
            (
                77 * choices[index][0] +
                150 * choices[index][1] +
                29 * choices[index][2]
            )
        ),
    )


def encode_tile(values: tuple[int, ...]) -> bytes:
    output = bytearray(16)
    for y in range(8):
        low = 0
        high = 0
        for x in range(8):
            value = values[y * 8 + x]
            bit = 7 - x
            low |= (value & 1) << bit
            high |= ((value >> 1) & 1) << bit
        output[y] = low
        output[y + 8] = high
    return bytes(output)


def decode_tile(tile: bytes) -> tuple[int, ...]:
    values: list[int] = []
    for y in range(8):
        for x in range(8):
            bit = 7 - x
            values.append(
                ((tile[y] >> bit) & 1) |
                (((tile[y + 8] >> bit) & 1) << 1)
            )
    return tuple(values)


def quantize_background(
    image: Image.Image,
    palette_indices: tuple[int, int, int, int],
    rows: int,
    priority_rows: range | None = None,
) -> tuple[bytes, bytes, Image.Image, int]:
    image = image.convert("RGB").resize((256, rows * 8), Image.Resampling.NEAREST)
    colours = [NES_RGB[index] for index in palette_indices]
    pixels = [
        nearest_colour(pixel, colours)
        for pixel in image.get_flattened_data()
    ]

    tiles: list[tuple[int, ...]] = []
    for tile_y in range(rows):
        for tile_x in range(32):
            values = tuple(
                pixels[(tile_y * 8 + y) * 256 + tile_x * 8 + x]
                for y in range(8)
                for x in range(8)
            )
            tiles.append(values)

    frequency = collections.Counter(tiles)
    unique_count = len(frequency)
    selected: list[tuple[int, ...]] = []
    if priority_rows is not None:
        for tile_y in priority_rows:
            for tile_x in range(32):
                tile = tiles[tile_y * 32 + tile_x]
                if tile not in selected:
                    selected.append(tile)
    for tile, _ in frequency.most_common():
        if tile not in selected:
            selected.append(tile)
        if len(selected) >= 256:
            break
    selected = selected[:256]
    if not selected:
        selected = [tuple([0] * 64)]

    index_by_tile = {tile: index for index, tile in enumerate(selected)}
    selected_planes: list[tuple[int, int]] = []
    for tile in selected:
        low = 0
        high = 0
        for pixel_index, value in enumerate(tile):
            low |= (value & 1) << pixel_index
            high |= ((value >> 1) & 1) << pixel_index
        selected_planes.append((low, high))
    for tile in frequency:
        if tile in index_by_tile:
            continue
        low = 0
        high = 0
        for pixel_index, value in enumerate(tile):
            low |= (value & 1) << pixel_index
            high |= ((value >> 1) & 1) << pixel_index
        best = min(
            range(len(selected)),
            key=lambda index: (
                (low ^ selected_planes[index][0]) |
                (high ^ selected_planes[index][1])
            ).bit_count(),
        )
        index_by_tile[tile] = best

    nametable = bytes(index_by_tile[tile] for tile in tiles)
    chr_data = b"".join(encode_tile(tile) for tile in selected)
    chr_data = chr_data.ljust(4096, b"\x00")

    reconstructed = Image.new("RGB", (256, rows * 8))
    output_pixels = reconstructed.load()
    for tile_pos, tile_index in enumerate(nametable):
        tile = selected[tile_index]
        tile_x = (tile_pos % 32) * 8
        tile_y = (tile_pos // 32) * 8
        for y in range(8):
            for x in range(8):
                output_pixels[tile_x + x, tile_y + y] = colours[tile[y * 8 + x]]

    return chr_data, nametable, reconstructed, unique_count


def quantize_mmc5_background(
    image: Image.Image,
    palette_sets: tuple[tuple[int, int, int, int], ...],
    rows: int,
    first_chr_bank: int,
    max_chr_banks: int,
    overlay_palette_hint: Image.Image | None = None,
) -> tuple[bytes, bytes, bytes, Image.Image, int, collections.Counter[int]]:
    """Quantize a background with MMC5 per-8x8 palette/CHR attributes."""
    if (
        not 0 <= first_chr_bank < 64 or
        max_chr_banks < 1 or
        first_chr_bank + max_chr_banks > 64
    ):
        raise ValueError("MMC5 extended-attribute CHR bank range is invalid")

    image = image.convert("RGB").resize((256, rows * 8), Image.Resampling.NEAREST)
    pixels = list(image.get_flattened_data())
    overlay_alpha: list[int] | None = None
    if overlay_palette_hint is not None:
        overlay_alpha = list(
            overlay_palette_hint.convert("RGBA")
            .resize((256, rows * 8), Image.Resampling.NEAREST)
            .getchannel("A")
            .get_flattened_data()
        )
    palette_colours = [
        [NES_RGB[index] for index in palette]
        for palette in palette_sets
    ]

    # Shape graphics reuse a fairly small source palette.  Cache both the
    # nearest NES colour and its error rather than performing the RGB search
    # four times for every one of the level's two million pixels.
    matches: list[dict[tuple[int, int, int], tuple[int, int]]] = []
    unique_pixels = set(pixels)
    for colours in palette_colours:
        palette_matches: dict[tuple[int, int, int], tuple[int, int]] = {}
        for pixel in unique_pixels:
            index = nearest_colour(pixel, colours)
            error = sum(
                (pixel[channel] - colours[index][channel]) ** 2
                for channel in range(3)
            )
            palette_matches[pixel] = (index, error)
        matches.append(palette_matches)

    tiles: list[tuple[int, ...]] = []
    tile_palettes: list[int] = []
    for tile_y in range(rows):
        for tile_x in range(32):
            source_pixels = tuple(
                pixels[(tile_y * 8 + y) * 256 + tile_x * 8 + x]
                for y in range(8)
                for x in range(8)
            )
            palette_ids: range | tuple[int, ...] = range(len(matches))
            if overlay_alpha is not None:
                overlay_coverage = sum(
                    overlay_alpha[
                        (tile_y * 8 + y) * 256 + tile_x * 8 + x
                    ] >= 96
                    for y in range(8)
                    for x in range(8)
                )
                if overlay_coverage >= 32:
                    palette_ids = (1,)
            candidates: list[tuple[int, tuple[int, ...], int]] = []
            for palette_id in palette_ids:
                palette_matches = matches[palette_id]
                values = tuple(
                    palette_matches[pixel][0]
                    for pixel in source_pixels
                )
                error = sum(
                    palette_matches[pixel][1]
                    for pixel in source_pixels
                )
                candidates.append((error, values, palette_id))
            _, values, palette_id = min(candidates, key=lambda item: item[0])
            tiles.append(values)
            tile_palettes.append(palette_id)

    frequency = collections.Counter(tiles)
    unique_count = len(frequency)
    selected = [
        tile
        for tile, _ in frequency.most_common(max_chr_banks * 256)
    ]
    if not selected:
        selected = [tuple([0] * 64)]

    index_by_tile = {tile: index for index, tile in enumerate(selected)}
    selected_planes: list[tuple[int, int]] = []
    for tile in selected:
        low = 0
        high = 0
        for pixel_index, value in enumerate(tile):
            low |= (value & 1) << pixel_index
            high |= ((value >> 1) & 1) << pixel_index
        selected_planes.append((low, high))
    for tile in frequency:
        if tile in index_by_tile:
            continue
        low = 0
        high = 0
        for pixel_index, value in enumerate(tile):
            low |= (value & 1) << pixel_index
            high |= ((value >> 1) & 1) << pixel_index
        index_by_tile[tile] = min(
            range(len(selected)),
            key=lambda index: (
                (low ^ selected_planes[index][0]) |
                (high ^ selected_planes[index][1])
            ).bit_count(),
        )

    tile_indices = [index_by_tile[tile] for tile in tiles]
    nametable = bytes(tile_index & 0xFF for tile_index in tile_indices)
    exattributes = bytes(
        (
            first_chr_bank +
            (tile_index >> 8) |
            (palette_id << 6)
        )
        for tile_index, palette_id in zip(tile_indices, tile_palettes)
    )
    chr_data = b"".join(encode_tile(tile) for tile in selected)
    chr_banks = max(1, (len(selected) + 255) // 256)
    chr_data = chr_data.ljust(chr_banks * 4096, b"\x00")

    reconstructed = Image.new("RGB", (256, rows * 8))
    output_pixels = reconstructed.load()
    for tile_pos, tile_index in enumerate(tile_indices):
        tile = selected[tile_index]
        palette_id = tile_palettes[tile_pos]
        colours = palette_colours[palette_id]
        tile_x = (tile_pos % 32) * 8
        tile_y = (tile_pos // 32) * 8
        for y in range(8):
            for x in range(8):
                output_pixels[tile_x + x, tile_y + y] = colours[tile[y * 8 + x]]

    return (
        chr_data,
        nametable,
        exattributes,
        reconstructed,
        unique_count,
        collections.Counter(tile_palettes),
    )


def find_font(size: int) -> ImageFont.ImageFont:
    candidates = (
        Path(r"C:\Windows\Fonts\consolab.ttf"),
        Path(r"C:\Windows\Fonts\arialbd.ttf"),
    )
    for candidate in candidates:
        if candidate.is_file():
            return ImageFont.truetype(str(candidate), size)
    return ImageFont.load_default()


def centred_text(
    draw: ImageDraw.ImageDraw,
    y: int,
    text: str,
    font: ImageFont.ImageFont,
    fill: tuple[int, int, int, int],
) -> None:
    box = draw.textbbox((0, 0), text, font=font)
    width = box[2] - box[0]
    draw.text(((256 - width) // 2, y), text, font=font, fill=fill)


def build_title(image_root: Path) -> Image.Image:
    planet = Image.open(image_root / "pics" / "pic_04.png").convert("RGBA")
    logo = Image.open(
        image_root / "sprites" / "03_planet" / "146.png"
    ).convert("RGBA")

    canvas = Image.new("RGBA", (256, 240), (0, 0, 0, 255))
    planet = planet.resize((256, 160), Image.Resampling.LANCZOS)
    canvas.alpha_composite(planet, (0, 80))

    logo.thumbnail((240, 78), Image.Resampling.LANCZOS)
    canvas.alpha_composite(logo, ((256 - logo.width) // 2, 24))

    draw = ImageDraw.Draw(canvas)
    draw.rectangle((0, 166, 255, 239), fill=(0, 0, 0, 255))
    centred_text(draw, 176, "PRESS START", find_font(18), (255, 255, 255, 255))
    centred_text(
        draw, 214, "MMC5 COMPLETE LEVEL 1", find_font(10),
        (216, 120, 96, 255),
    )
    return canvas.convert("RGB")


def parse_first_level(
    level_path: Path,
) -> tuple[
    list[tuple[int, ...]],
    list[bytes],
    list[tuple[int, int, int, int, int, int, int, int]],
]:
    data = level_path.read_bytes()
    level_count = struct.unpack_from("<H", data, 0)[0]
    offsets = struct.unpack_from(f"<{level_count}i", data, 2)
    position = offsets[(9 - 1) * 2]
    position += 8
    enemy_count = struct.unpack_from("<H", data, position)[0]
    position += 2 + enemy_count * 2
    event_count = struct.unpack_from("<H", data, position)[0]
    position += 2

    events = [
        struct.unpack_from("<HBhhbbbB", data, position + index * 11)
        for index in range(event_count)
    ]
    position += event_count * 11

    lookups = [
        struct.unpack_from(">128H", data, position + layer * 256)
        for layer in range(3)
    ]
    position += 3 * 256
    maps: list[bytes] = []
    for size in (14 * 300, 14 * 600, 15 * 600):
        layer = data[position:position + size]
        if len(layer) != size:
            raise ValueError("truncated first-level map layer")
        maps.append(layer)
        position += size
    return lookups, maps, events


def render_map_layer(
    image_root: Path,
    lookup: tuple[int, ...],
    map_data: bytes,
    map_width: int,
    first_row: int,
    last_row: int,
) -> tuple[Image.Image, int]:
    output_rows = last_row - first_row + 1
    source = Image.new(
        "RGBA", (map_width * 24, output_rows * 28), (0, 0, 0, 0)
    )
    tile_root = image_root / "tiles" / "shapes_z"
    shape_cache: dict[int, Image.Image | None] = {}
    nonblank_cells = 0

    for output_row, map_row in enumerate(
        range(first_row, last_row + 1)
    ):
        for column in range(map_width):
            map_index = map_data[map_row * map_width + column]
            shape_id = lookup[map_index]
            if shape_id not in shape_cache:
                tile_path = tile_root / f"{shape_id - 1:03d}.png"
                shape_cache[shape_id] = (
                    Image.open(tile_path).convert("RGBA")
                    if shape_id and tile_path.is_file()
                    else None
                )
            tile = shape_cache[shape_id]
            if tile is not None:
                source.alpha_composite(tile, (column * 24, output_row * 28))
                nonblank_cells += 1
    return source, nonblank_cells


def build_level(
    image_root: Path,
    level_path: Path,
) -> tuple[
    Image.Image,
    Image.Image,
    Image.Image,
    list[tuple[int, int, int, int, int, int, int, int]],
    int,
]:
    lookups, maps, events = parse_first_level(level_path)
    layer_1, _ = render_map_layer(
        image_root,
        lookups[0],
        maps[0],
        14,
        LEVEL_SOURCE_FIRST_ROW,
        LEVEL_SOURCE_LAST_ROW,
    )
    layer_2, layer_2_nonblank = render_map_layer(
        image_root,
        lookups[1],
        maps[1],
        14,
        LEVEL_LAYER2_FIRST_ROW,
        LEVEL_LAYER2_LAST_ROW,
    )

    # MAP 2 has twice as many rows and normally moves at twice MAP 1's
    # vertical speed.  Rows 14..593 align original row 592 exactly with
    # MAP 1 row 292 at level start, so reducing it to the same 8,120-pixel
    # timeline preserves its intended parallax phase for this NES POC.
    layer_2 = layer_2.resize(
        (14 * 24, LEVEL_MAP_ROWS * 8),
        Image.Resampling.NEAREST,
    )

    # Crop the 336-pixel DOS playfield to the NES viewport without shortening
    # the complete first-level route.
    layer_1_view = layer_1.crop((40, 0, 296, LEVEL_MAP_ROWS * 8))
    layer_2_view = layer_2.crop((40, 0, 296, LEVEL_MAP_ROWS * 8))
    composite = Image.new(
        "RGBA", (256, LEVEL_MAP_ROWS * 8), (0, 0, 0, 255)
    )
    composite.alpha_composite(layer_1_view)
    composite.alpha_composite(layer_2_view)
    return (
        composite.convert("RGB"),
        layer_1_view.convert("RGB"),
        layer_2_view,
        events,
        layer_2_nonblank,
    )


def enemy_archetype(enemy_id: int) -> int:
    """Map first-level HDT IDs to sixteen NES visual/gameplay archetypes."""
    if enemy_id == 4:
        return 0
    if enemy_id == 3:
        return 1
    if enemy_id == 5:
        return 2
    if enemy_id in (10, 12):
        return 3
    if enemy_id == 15:
        return 4
    if enemy_id in (6, 7, 8, 9, 13, 14, 17):
        return 5
    if enemy_id == 24:
        return 6
    if enemy_id == 25:
        return 7
    if enemy_id in (26, 28):
        return 8
    if enemy_id in (27, 29):
        return 9
    if enemy_id == 30:
        return 10
    if 31 <= enemy_id <= 39:
        return 11
    if enemy_id == 40:
        return 12
    if 41 <= enemy_id <= 45:
        return 13
    if enemy_id in (121, 125):
        return 14
    if 516 <= enemy_id <= 527:
        # Source uses a multi-part rock installation.  Cycle the three closest
        # NES ground archetypes instead of repeating one rectangular tile.
        return 13 + ((enemy_id - 516) % 3)
    if 66 <= enemy_id <= 79:
        # These fourteen NEWSH4 records form wide, multi-part foreground
        # structures.  The NES only keeps six entities, so reuse eight
        # mechanical archetypes to retain component variety.
        return 8 + ((enemy_id - 66) & 7)
    return enemy_id % 3


def event_layer(event_type: int, fixed_move: int) -> int:
    if event_type == 15:
        return LAYER_SKY
    if event_type in (6, 17, 49):
        return LAYER_GROUND
    if event_type in (7, 23, 32, 51):
        return LAYER_TOP
    if event_type in (10, 56, 52):
        return LAYER_GROUND_2
    if event_type in (18, 50):
        return LAYER_BOTTOM
    if event_type == 12:
        return {
            0: LAYER_GROUND,
            1: LAYER_GROUND,
            2: LAYER_SKY,
            3: LAYER_TOP,
            4: LAYER_GROUND_2,
        }.get(fixed_move, LAYER_GROUND)
    return LAYER_SKY


def packed_signed_pair(first: int, second: int) -> int:
    """Pack -7..7 as 0..14 and reserve nibble 15 for OpenTyrian's -99."""
    def encode(value: int) -> int:
        if value == -99:
            return 15
        return max(-7, min(7, value)) + 7

    return (encode(first) << 4) | encode(second)


def encode_level_events(
    events: list[tuple[int, int, int, int, int, int, int, int]],
) -> tuple[bytes, int, int]:
    """Compile Tyrian's event list to a compact delta-timed 6502 bytecode.

    Spawn commands retain HDT-derived archetypes, source layer, X position,
    Y-speed hint and link number.  The movement/acceleration/reversal/fire
    commands that drive the first level's linked formations are retained too.
    """
    records: list[tuple[int, int, bytes]] = []
    spawn_count = 0
    control_count = 0

    def append_spawn(
        event_time: int,
        enemy_id: int,
        x_position: int,
        y_speed: int,
        layer: int,
        row_delay: bool,
        link_number: int,
    ) -> None:
        nonlocal spawn_count
        nes_x = max(4, min(236, (x_position * 4 + 2) // 5))
        speed_code = max(-7, min(7, y_speed)) + 7
        motion = (
            (layer & 0x07) |
            (0x08 if row_delay else 0) |
            (speed_code << 4)
        )
        records.append((
            event_time,
            enemy_archetype(enemy_id),
            bytes((nes_x, motion, link_number & 0xFF)),
        ))
        spawn_count += 1

    for (
        event_time, event_type, event_data, event_data_2,
        event_data_3, event_data_5, event_data_6, event_data_4,
    ) in events:
        if event_time >= 4900:
            break

        if event_type in LEVEL_SPAWN_TYPES:
            layer = event_layer(event_type, event_data_6)
            if event_type == 12:
                # OpenTyrian expands this event into a destructible 2×2 block.
                for offset, x_add, extra_row_delay in (
                    (0, 0, False),
                    (1, 24, False),
                    (2, 0, True),
                    (3, 24, True),
                ):
                    append_spawn(
                        event_time,
                        event_data + offset,
                        event_data_2 + x_add,
                        event_data_3,
                        layer,
                        event_data_5 < 0 or extra_row_delay,
                        event_data_4,
                    )
            else:
                if event_type in (17, 18, 23, 32, 56):
                    layer = LAYER_BOTTOM
                append_spawn(
                    event_time,
                    event_data,
                    event_data_2,
                    event_data_3,
                    layer,
                    event_data_5 < -8,
                    event_data_4,
                )
            continue

        if event_type not in LEVEL_CONTROL_TYPES:
            continue

        if event_type == 19:
            records.append((
                event_time,
                EVENT_MOVE,
                bytes((
                    event_data_4,
                    packed_signed_pair(event_data, event_data_2),
                )),
            ))
        elif event_type == 20:
            records.append((
                event_time,
                EVENT_ACCEL,
                bytes((
                    event_data_4,
                    packed_signed_pair(event_data, event_data_2),
                )),
            ))
        elif event_type == 27:
            records.append((
                event_time,
                EVENT_REVERSE,
                bytes((
                    event_data_4,
                    packed_signed_pair(event_data, event_data_2),
                )),
            ))
        elif event_type == 31:
            records.append((
                event_time,
                EVENT_FIRE,
                bytes((
                    event_data_4,
                    max(
                        0,
                        min(255, max(event_data, event_data_2, event_data_3)),
                    ),
                )),
            ))
        else:
            records.append((event_time, EVENT_FOREGROUND, b""))
        control_count += 1

    output = bytearray()
    time_cursor = 0
    for event_time, opcode, payload in records:
        delta = event_time - time_cursor
        while delta > 254:
            output.extend((254, EVENT_WAIT))
            time_cursor += 254
            delta -= 254
        output.extend((delta, opcode))
        output.extend(payload)
        time_cursor = event_time
    output.extend((0, EVENT_END))
    return bytes(output), spawn_count, control_count


def build_initial_ring(level_map: bytes, rows: int) -> bytes:
    if len(level_map) != rows * 32:
        raise ValueError("level tile stream has an unexpected size")
    physical = bytearray(60 * 32)
    last_row = rows - 1
    for physical_row in range(60):
        world_row = last_row - ((last_row - physical_row) % 60)
        if world_row < 0:
            continue
        source = world_row * 32
        target = physical_row * 32
        physical[target:target + 32] = level_map[source:source + 32]
    return bytes(physical)


def build_initial_exram(level_exattributes: bytes, rows: int) -> bytes:
    """Build the 30-row ExRAM view corresponding to the initial 60-row ring."""
    ring = build_initial_ring(level_exattributes, rows)
    top_physical_row = ((rows - 30) % 60)
    exram = bytearray(30 * 32)
    for visible_row in range(30):
        physical_row = (top_physical_row + visible_row) % 60
        exram_row = physical_row % 30
        source = physical_row * 32
        target = exram_row * 32
        exram[target:target + 32] = ring[source:source + 32]
    return bytes(exram)


def transparent_bbox(image: Image.Image) -> tuple[int, int, int, int]:
    rgba = image.convert("RGBA")
    alpha = rgba.getchannel("A")
    if alpha.getextrema() == (255, 255):
        pixels = rgba.load()
        mask = Image.new("L", rgba.size, 0)
        mask_pixels = mask.load()
        for y in range(rgba.height):
            for x in range(rgba.width):
                r, g, b, _ = pixels[x, y]
                if r + g + b > 8:
                    mask_pixels[x, y] = 255
        box = mask.getbbox()
    else:
        box = alpha.getbbox()
    return box or (0, 0, image.width, image.height)


def prepare_sprite(
    source_value: Path | Image.Image,
    size: tuple[int, int],
    palette_number: int,
    preserve_aspect: bool = True,
    crop_transparent: bool = True,
    resample: Image.Resampling = Image.Resampling.NEAREST,
) -> tuple[bytes, Image.Image]:
    if isinstance(source_value, Path):
        source = Image.open(source_value).convert("RGBA")
    else:
        source = source_value.convert("RGBA")
    if crop_transparent:
        source = source.crop(transparent_bbox(source))
    if preserve_aspect:
        source.thumbnail(size, resample)
    else:
        source = source.resize(size, resample)

    canvas = Image.new("RGBA", size, (0, 0, 0, 0))
    canvas.alpha_composite(
        source,
        ((size[0] - source.width) // 2, (size[1] - source.height) // 2),
    )

    palette = SPRITE_PALETTES[palette_number]
    colours = [NES_RGB[index] for index in palette]
    values: list[int] = []
    preview = Image.new("RGBA", size, (0, 0, 0, 0))
    source_pixels = canvas.load()
    preview_pixels = preview.load()
    for y in range(size[1]):
        for x in range(size[0]):
            r, g, b, a = source_pixels[x, y]
            if a < 96 or (r + g + b < 8 and source.getchannel("A").getextrema() == (255, 255)):
                value = 0
            else:
                # The target palette may intentionally change hue to separate
                # a sprite from the terrain.  Match luminance so original
                # Tyrian highlights, panels and silhouettes survive that
                # artistic remap.
                value = 1 + nearest_luminance((r, g, b), colours[1:])
            values.append(value)
            if value:
                preview_pixels[x, y] = (*colours[value], 255)

    tiles = bytearray()
    for tile_y in range(size[1] // 8):
        for tile_x in range(size[0] // 8):
            tile = tuple(
                values[(tile_y * 8 + y) * size[0] + tile_x * 8 + x]
                for y in range(8)
                for x in range(8)
            )
            tiles.extend(encode_tile(tile))
    return bytes(tiles), preview


def compose_sprite_2x2(directory: Path, start: int) -> Image.Image:
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


def compose_first_level_boss(
    directory: Path,
    core_start: int = 1,
) -> Image.Image:
    """Rebuild the original 5-by-4 first-level boss component grid.

    tyrian.hdt enemy records 46..65 point at these NEWSH4 2x2 graphics.
    The animated centre component uses graphics 1, 3, 5, ... instead of 52.
    """
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


def encode_sprite_8x16(
    source_value: Path | Image.Image,
    size: tuple[int, int],
    palette_number: int,
    preserve_aspect: bool = True,
    crop_transparent: bool = True,
    resample: Image.Resampling = Image.Resampling.NEAREST,
) -> tuple[bytes, Image.Image]:
    """Encode a metasprite in the tile order required by PPU 8x16 mode."""
    if size[0] % 8 or size[1] % 16:
        raise ValueError(f"8x16 sprite size must be aligned: {size}")
    row_major, preview = prepare_sprite(
        source_value,
        size,
        palette_number,
        preserve_aspect,
        crop_transparent,
        resample,
    )
    tile_columns = size[0] // 8
    tile_rows = size[1] // 8
    tiles = [
        row_major[index * 16:(index + 1) * 16]
        for index in range(tile_columns * tile_rows)
    ]
    output = bytearray()
    for block_y in range(size[1] // 16):
        for tile_x in range(tile_columns):
            top = (block_y * 2) * tile_columns + tile_x
            output.extend(tiles[top])
            output.extend(tiles[top + tile_columns])
    return bytes(output), preview


def procedural_pattern_8x16(kind: str) -> bytes:
    pixels = [0] * 128
    if kind == "player_shot":
        for y in range(1, 13):
            pixels[y * 8 + 3] = 3
            pixels[y * 8 + 4] = 3
            if y in (2, 10):
                pixels[y * 8 + 2] = 2
                pixels[y * 8 + 5] = 2
    elif kind == "enemy_shot":
        for y in range(4, 11):
            for x in range(2, 6):
                if abs(x - 3.5) + abs(y - 7) < 4:
                    pixels[y * 8 + x] = 2
    elif kind == "bar":
        for y in range(2, 6):
            for x in range(8):
                pixels[y * 8 + x] = 2 if x < 6 else 1
    return (
        encode_tile(tuple(pixels[:64])) +
        encode_tile(tuple(pixels[64:]))
    )


def build_sprites(
    image_root: Path,
) -> tuple[bytes, Image.Image, Image.Image, dict[str, int]]:
    player_directory = image_root / "sheets" / "09_player_ships"
    newsh2 = image_root / "sheets_newsh" / "newsh_2"
    newsh4 = image_root / "sheets_newsh" / "newsh_4"
    newsh6 = image_root / "sheets_newsh" / "newsh_6"
    specifications = (
        # The HDT ship record identifies graphic 233 as the USP Talon.
        (0, "PLAYER_0", compose_sprite_2x2(player_directory, 233),
         (24, 32), 0, True, False, Image.Resampling.NEAREST),
        (0, "PLAYER_1", compose_sprite_2x2(player_directory, 235),
         (24, 32), 0, True, False, Image.Resampling.NEAREST),
        # Representative first-level NEWSH2 graphics taken from the HDT
        # egraphic lists: 159/161, 171/190 and 229/231.
        (0, "ENEMY_1_0", compose_sprite_2x2(newsh2, 159),
         (24, 32), 1, True, False, Image.Resampling.NEAREST),
        (0, "ENEMY_1_1", compose_sprite_2x2(newsh2, 161),
         (24, 32), 1, True, False, Image.Resampling.NEAREST),
        (0, "ENEMY_2_0", newsh2 / "171.png",
         (16, 16), 1, True, False, Image.Resampling.NEAREST),
        (0, "ENEMY_2_1", newsh2 / "190.png",
         (16, 16), 1, True, False, Image.Resampling.NEAREST),
        (0, "ENEMY_3_0", compose_sprite_2x2(newsh2, 229),
         (24, 32), 1, True, False, Image.Resampling.NEAREST),
        (0, "ENEMY_3_1", compose_sprite_2x2(newsh2, 231),
         (24, 32), 1, True, False, Image.Resampling.NEAREST),
        (0, "BOSS_0", compose_first_level_boss(newsh4, 1),
         (56, 64), 2, True, True, Image.Resampling.LANCZOS),
        (0, "BOSS_1", compose_first_level_boss(newsh4, 3),
         (56, 64), 2, True, True, Image.Resampling.LANCZOS),
        (0, "EXPLOSION", newsh6 / "010.png",
         (16, 16), 3, True, False, Image.Resampling.NEAREST),
        # MMC5 CHR set A exposes a second independent 4 KiB pattern table to
        # 8x16 sprites.  Spend it on first-level HDT archetypes instead of
        # redrawing every source enemy as one of three generic ships.
        (1, "ENEMY_4_0", compose_sprite_2x2(newsh2, 191),
         (24, 32), 1, True, False, Image.Resampling.NEAREST),
        (1, "ENEMY_4_1", compose_sprite_2x2(newsh2, 193),
         (24, 32), 1, True, False, Image.Resampling.NEAREST),
        (1, "ENEMY_5_0", compose_sprite_2x2(newsh2, 91),
         (24, 32), 1, True, False, Image.Resampling.NEAREST),
        (1, "ENEMY_5_1", compose_sprite_2x2(newsh2, 45),
         (24, 32), 1, True, False, Image.Resampling.NEAREST),
        (1, "ENEMY_6_0", compose_sprite_2x2(newsh2, 267),
         (24, 32), 1, True, False, Image.Resampling.NEAREST),
        (1, "ENEMY_7_0", compose_sprite_2x2(newsh2, 269),
         (24, 32), 1, True, False, Image.Resampling.NEAREST),
        (1, "ENEMY_8_0", compose_sprite_2x2(newsh2, 271),
         (24, 32), 1, True, False, Image.Resampling.NEAREST),
        (1, "ENEMY_9_0", compose_sprite_2x2(newsh2, 49),
         (24, 32), 2, True, False, Image.Resampling.NEAREST),
        (1, "ENEMY_10_0", compose_sprite_2x2(newsh2, 51),
         (24, 32), 2, True, False, Image.Resampling.NEAREST),
        (1, "ENEMY_11_0", compose_sprite_2x2(newsh2, 89),
         (24, 32), 2, True, False, Image.Resampling.NEAREST),
        (1, "ENEMY_12_0", compose_sprite_2x2(newsh2, 273),
         (24, 32), 2, True, False, Image.Resampling.NEAREST),
        (1, "ENEMY_13_0", compose_sprite_2x2(newsh2, 153),
         (24, 32), 2, True, False, Image.Resampling.NEAREST),
        (1, "ENEMY_13_1", compose_sprite_2x2(newsh2, 155),
         (24, 32), 2, True, False, Image.Resampling.NEAREST),
        (1, "ENEMY_14_0", compose_sprite_2x2(newsh2, 169),
         (24, 32), 2, True, False, Image.Resampling.NEAREST),
        (1, "ENEMY_15_0", compose_sprite_2x2(newsh2, 115),
         (24, 32), 2, True, False, Image.Resampling.NEAREST),
        (1, "ENEMY_15_1", compose_sprite_2x2(newsh2, 119),
         (24, 32), 2, True, False, Image.Resampling.NEAREST),
        (1, "ENEMY_16_0", compose_sprite_2x2(newsh4, 153),
         (24, 32), 2, True, False, Image.Resampling.NEAREST),
        (1, "ENEMY_16_1", compose_sprite_2x2(newsh4, 155),
         (24, 32), 2, True, False, Image.Resampling.NEAREST),
    )

    chr_banks = (bytearray(), bytearray())
    preview = Image.new("RGB", (960, 560), (20, 20, 24))
    draw = ImageDraw.Draw(preview)
    metadata: dict[str, int] = {}
    x = 8
    y = 22
    for (
        bank, name, source, size, palette, preserve_aspect,
        crop_transparent, resample,
    ) in specifications:
        if x + size[0] * 3 + 16 > preview.width:
            x = 8
            y += 104
        chr_data = chr_banks[bank]
        if len(chr_data) % 32:
            raise ValueError("8x16 sprite pattern did not start on an even tile")
        metadata[f"SPR_TILE_{name}"] = (len(chr_data) // 16) | bank
        metadata[f"SPR_COLS_{name}"] = size[0] // 8
        metadata[f"SPR_ROWS_{name}"] = size[1] // 16
        encoded, sprite_preview = encode_sprite_8x16(
            source,
            size,
            palette,
            preserve_aspect,
            crop_transparent,
            resample,
        )
        chr_data.extend(encoded)
        scaled = sprite_preview.resize(
            (size[0] * 3, size[1] * 3),
            Image.Resampling.NEAREST,
        )
        preview.paste(scaled, (x, y), scaled)
        draw.text(
            (x, y - 16), f"{name} / CHR {bank}",
            fill=(240, 240, 240), font=find_font(10),
        )
        x += max(size[0] * 3 + 16, 70)

    for name, kind in (
        ("PLAYER_SHOT", "player_shot"),
        ("ENEMY_SHOT", "enemy_shot"),
        ("BOSS_BAR", "bar"),
    ):
        if len(chr_banks[0]) % 32:
            raise ValueError("procedural 8x16 pattern is not pair aligned")
        metadata[f"SPR_TILE_{name}"] = len(chr_banks[0]) // 16
        chr_banks[0].extend(procedural_pattern_8x16(kind))

    # Static source graphics share their frame-zero pattern.
    for enemy_number in (6, 7, 8, 9, 10, 11, 12, 14):
        metadata[f"SPR_TILE_ENEMY_{enemy_number}_1"] = metadata[
            f"SPR_TILE_ENEMY_{enemy_number}_0"
        ]

    for bank, chr_data in enumerate(chr_banks):
        if len(chr_data) > 4096:
            raise ValueError(
                f"8x16 sprite CHR bank {bank} uses {len(chr_data)} bytes; "
                "4 KiB bank overflow"
            )
    metadata["SPR_CHR_LO_USED_BYTES"] = len(chr_banks[0])
    metadata["SPR_CHR_HI_USED_BYTES"] = len(chr_banks[1])
    metadata["SPR_CHR_USED_BYTES"] = sum(len(bank) for bank in chr_banks)
    metadata["SPR_8X16_PATTERN_COUNT"] = (
        metadata["SPR_CHR_USED_BYTES"] // 32
    )

    source_preview = Image.new("RGBA", (960, 360), (18, 18, 22, 255))
    source_draw = ImageDraw.Draw(source_preview)
    source_items = (
        ("USP TALON 233", compose_sprite_2x2(player_directory, 233)),
        ("ENEMY 159", compose_sprite_2x2(newsh2, 159)),
        ("ENEMY 171", Image.open(newsh2 / "171.png").convert("RGBA")),
        ("ENEMY 229", compose_sprite_2x2(newsh2, 229)),
        ("BOSS 5x4", compose_first_level_boss(newsh4, 1)),
    )
    x = 10
    for name, image in source_items:
        scale = 3 if image.width < 100 else 2
        scaled = image.resize(
            (image.width * scale, image.height * scale),
            Image.Resampling.NEAREST,
        )
        source_preview.alpha_composite(scaled, (x, 36))
        source_draw.text(
            (x, 12), name, fill=(240, 240, 240, 255), font=find_font(12)
        )
        x += scaled.width + 30

    packed_chr = (
        bytes(chr_banks[0]).ljust(4096, b"\x00") +
        bytes(chr_banks[1]).ljust(4096, b"\x00")
    )
    return packed_chr, preview, source_preview, metadata


def build_contact_sheet(directory: Path, limit: int = 96) -> Image.Image:
    paths = sorted(directory.glob("*.png"))[:limit]
    columns = 12
    cell_w = 58
    cell_h = 72
    rows = (len(paths) + columns - 1) // columns
    sheet = Image.new("RGB", (columns * cell_w, rows * cell_h), (18, 18, 22))
    draw = ImageDraw.Draw(sheet)
    font = find_font(9)
    for index, path in enumerate(paths):
        image = Image.open(path).convert("RGBA")
        image = image.resize(
            (image.width * 3, image.height * 3),
            Image.Resampling.NEAREST,
        )
        x = (index % columns) * cell_w
        y = (index // columns) * cell_h
        sheet.paste(image, (x + (cell_w - image.width) // 2, y + 14), image)
        draw.text((x + 2, y + 2), path.stem, fill=(230, 230, 230), font=font)
    return sheet


def build_composite_contact_sheet(
    directory: Path,
    starts: tuple[int, ...],
) -> Image.Image:
    cell_w = 112
    cell_h = 118
    columns = 5
    rows = (len(starts) + columns - 1) // columns
    sheet = Image.new("RGB", (columns * cell_w, rows * cell_h), (18, 18, 22))
    draw = ImageDraw.Draw(sheet)
    font = find_font(10)
    for position, start in enumerate(starts):
        image = compose_sprite_2x2(directory, start)
        scaled = image.resize((96, 112), Image.Resampling.NEAREST)
        x = (position % columns) * cell_w
        y = (position // columns) * cell_h
        sheet.paste(scaled, (x + 8, y + 4), scaled)
        draw.text((x + 4, y + 2), str(start), fill=(255, 255, 255), font=font)
    return sheet


def parse_table(include_text: str, name: str) -> bytes:
    match = re.search(
        rf"^{re.escape(name)}:\s*\n((?:\s+\.db[^\n]*\n)+)",
        include_text,
        re.MULTILINE,
    )
    if not match:
        raise ValueError(f"missing generated audio table {name}")
    values = [
        int(value, 16)
        for value in re.findall(r"\$([0-9A-Fa-f]{2})", match.group(1))
    ]
    if len(values) != 128:
        raise ValueError(f"{name} has {len(values)} values instead of 128")
    return bytes(values)


def copy_music_assets(music_build: Path, output: Path) -> dict[str, int]:
    tracks = {
        "title": music_build / "mmc5_rom_tracks" / "30_tyrian_the_song.tnm5",
        "level": music_build / "mmc5_rom_tracks" / "18_tyrian_the_level.tnm5",
    }
    metadata: dict[str, int] = {}
    for name, path in tracks.items():
        data = path.read_bytes()
        if data[:4] != b"TNM5" or len(data) < 9:
            raise ValueError(f"invalid TNM5 track: {path}")
        loop_offset, stream_size = struct.unpack_from("<HH", data, 4)
        stream = data[8:]
        if len(stream) != stream_size or len(stream) > 8192:
            raise ValueError(f"TNM5 stream does not fit one 8 KiB page: {path}")
        (output / f"{name}_stream.bin").write_bytes(stream)
        metadata[f"{name}_bytes"] = len(stream)
        metadata[f"{name}_loop"] = 0x8000 + loop_offset

    (output / "music_meta.inc").write_text(
        (
            "; Generated by tools/build_assets.py\n"
            f"TITLE_LOOP_ADDR = ${metadata['title_loop']:04X}\n"
            f"LEVEL_LOOP_ADDR = ${metadata['level_loop']:04X}\n"
        ),
        encoding="ascii",
    )

    generated = (music_build / "mmc5_generated.inc").read_text(encoding="utf-8")
    for table in (
        "note_period_lo",
        "note_period_hi",
        "triangle_period_lo",
        "triangle_period_hi",
    ):
        (output / f"{table}.bin").write_bytes(parse_table(generated, table))

    dpcm = (music_build / "mmc5_dpcm_sample_bank.bin").read_bytes()
    (output / "dpcm_sample_bank.bin").write_bytes(dpcm)
    metadata["dpcm_bytes"] = len(dpcm)
    return metadata


def palette_binary(
    background_palettes: tuple[tuple[int, int, int, int], ...],
) -> bytes:
    if len(background_palettes) != 4:
        raise ValueError("NES palette binary requires four background palettes")
    data: list[int] = []
    for palette in background_palettes:
        data.extend(palette)
    for palette in SPRITE_PALETTES:
        data.extend(palette)
    return bytes(data)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--image-root", type=Path, required=True)
    parser.add_argument("--data-dir", type=Path, required=True)
    parser.add_argument("--music-build", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--preview-dir", type=Path, required=True)
    args = parser.parse_args()

    args.output.mkdir(parents=True, exist_ok=True)
    args.preview_dir.mkdir(parents=True, exist_ok=True)

    title_source = build_title(args.image_root)
    title_chr, title_nt, title_preview, title_unique = quantize_background(
        title_source, TITLE_PALETTE, 30, priority_rows=range(20, 30)
    )

    (
        level_source,
        level_layer_1_source,
        level_layer_2_source,
        source_events,
        level_layer_2_nonblank,
    ) = build_level(
        args.image_root, args.data_dir / "tyrian1.lvl"
    )
    (
        level_chr,
        level_nt,
        level_exattributes,
        level_preview,
        level_unique,
        level_palette_counts,
    ) = quantize_mmc5_background(
        level_source,
        LEVEL_PALETTES,
        LEVEL_MAP_ROWS,
        MMC5_LEVEL_CHR_FIRST_BANK,
        MMC5_LEVEL_CHR_MAX_BANKS,
        level_layer_2_source,
    )
    level_chr_banks = len(level_chr) // 4096
    sprite_chr_a_lo_bank = MMC5_LEVEL_CHR_FIRST_BANK + level_chr_banks
    sprite_chr_a_hi_bank = sprite_chr_a_lo_bank + 1
    if sprite_chr_a_hi_bank >= 32:
        raise ValueError("level and sprite graphics exceed 128 KiB MMC5 CHR ROM")
    initial_ring = build_initial_ring(level_nt, LEVEL_MAP_ROWS)
    initial_exram = build_initial_exram(level_exattributes, LEVEL_MAP_ROWS)
    (
        level_events,
        level_event_spawn_count,
        level_event_control_count,
    ) = encode_level_events(source_events)
    (
        sprite_chr,
        sprite_preview,
        sprite_source_preview,
        sprite_metadata,
    ) = build_sprites(args.image_root)

    (args.output / "title_nametable.bin").write_bytes(title_nt)
    (args.output / "game_nametable_0.bin").write_bytes(initial_ring[:960])
    (args.output / "game_nametable_1.bin").write_bytes(initial_ring[960:])
    (args.output / "game_exattr.bin").write_bytes(initial_exram)
    (args.output / "level_map.bin").write_bytes(level_nt)
    (args.output / "level_exattr.bin").write_bytes(level_exattributes)
    (args.output / "level_events.bin").write_bytes(level_events)
    (args.output / "level_meta.h").write_text(
        "\n".join((
            "#ifndef TYRIAN_NES_LEVEL_META_H",
            "#define TYRIAN_NES_LEVEL_META_H",
            "",
            f"#define LEVEL_MAP_ROWS {LEVEL_MAP_ROWS}u",
            f"#define LEVEL_START_SCROLL {(LEVEL_MAP_ROWS - 30) * 8}u",
            f"#define LEVEL_BOSS_FRAME {LEVEL_BOSS_FRAME}u",
            f"#define LEVEL_END_FRAME {LEVEL_END_FRAME}u",
            f"#define LEVEL_EVENT_SPAWN_COUNT {level_event_spawn_count}u",
            f"#define LEVEL_EVENT_CONTROL_COUNT {level_event_control_count}u",
            f"#define LEVEL_EVENT_BYTES {len(level_events)}u",
            "",
            "#endif",
            "",
        )),
        encoding="ascii",
    )
    sprite_header_lines = [
        "#ifndef TYRIAN_NES_SPRITE_META_H",
        "#define TYRIAN_NES_SPRITE_META_H",
        "",
        f"#define SPR_CHR_A_LO_BANK {sprite_chr_a_lo_bank}u",
        f"#define SPR_CHR_A_HI_BANK {sprite_chr_a_hi_bank}u",
    ]
    sprite_header_lines.extend(
        f"#define {name} {value}u"
        for name, value in sprite_metadata.items()
    )
    sprite_header_lines.extend(("", "#endif", ""))
    (args.output / "sprite_meta.h").write_text(
        "\n".join(sprite_header_lines),
        encoding="ascii",
    )
    (args.output / "title_palette.bin").write_bytes(
        palette_binary((TITLE_PALETTE,) * 4)
    )
    (args.output / "game_palette.bin").write_bytes(
        palette_binary(LEVEL_PALETTES)
    )

    chr_rom = (title_chr + level_chr + sprite_chr).ljust(128 * 1024, b"\xFF")
    (args.output / "tyrian_poc.chr").write_bytes(chr_rom)

    title_source.save(args.preview_dir / "title_source.png")
    title_preview.save(args.preview_dir / "title_preview.png")
    level_source.save(args.preview_dir / "level_source.png")
    level_layer_1_source.save(args.preview_dir / "level_layer1_source.png")
    level_layer_2_source.save(args.preview_dir / "level_layer2_source.png")
    level_preview.save(args.preview_dir / "level_preview.png")
    sprite_preview.save(args.preview_dir / "sprites_preview.png")
    sprite_source_preview.save(
        args.preview_dir / "sprites_original_reference.png"
    )
    contact_directories = {
        "contacts_player": args.image_root / "sheets" / "09_player_ships",
        "contacts_newsh2": args.image_root / "sheets_newsh" / "newsh_2",
        "contacts_newsh4": args.image_root / "sheets_newsh" / "newsh_4",
        "contacts_newshe": args.image_root / "sheets_newsh" / "newsh_e",
        "contacts_newshp": args.image_root / "sheets_newsh" / "newsh_p",
        "contacts_newsh6": args.image_root / "sheets_newsh" / "newsh_6",
    }
    for name, directory in contact_directories.items():
        build_contact_sheet(directory).save(args.preview_dir / f"{name}.png")
    build_composite_contact_sheet(
        args.image_root / "sheets_newsh" / "newsh_4",
        (1, 10, 12, 14, 16, 18, 48, 50, 54, 56,
         86, 88, 90, 92, 94, 124, 126, 128, 130, 132),
    ).save(args.preview_dir / "contacts_boss_composites.png")

    music_metadata = copy_music_assets(args.music_build, args.output)
    report = (
        f"title_unique_tiles={title_unique}\n"
        f"title_packed_tiles={len(title_chr) // 16}\n"
        f"level_unique_tiles={level_unique}\n"
        f"level_packed_tiles={len(level_chr) // 16}\n"
        f"level_chr_banks_4k={level_chr_banks}\n"
        f"level_map_rows={LEVEL_MAP_ROWS}\n"
        f"level_map_bytes={len(level_nt)}\n"
        f"level_exattr_bytes={len(level_exattributes)}\n"
        f"level_layer2_nonblank_cells={level_layer_2_nonblank}\n"
        f"level_palette_0_tiles={level_palette_counts[0]}\n"
        f"level_palette_1_tiles={level_palette_counts[1]}\n"
        f"level_palette_2_tiles={level_palette_counts[2]}\n"
        f"level_palette_3_tiles={level_palette_counts[3]}\n"
        f"level_event_source_records={len(source_events)}\n"
        f"level_event_spawn_records={level_event_spawn_count}\n"
        f"level_event_control_records={level_event_control_count}\n"
        f"level_event_bytecode_bytes={len(level_events)}\n"
        f"level_boss_frame={LEVEL_BOSS_FRAME}\n"
        f"level_end_frame={LEVEL_END_FRAME}\n"
        f"sprite_chr_bytes={len(sprite_chr)}\n"
        f"sprite_chr_used_bytes={sprite_metadata['SPR_CHR_USED_BYTES']}\n"
        f"sprite_chr_lower_used_bytes={sprite_metadata['SPR_CHR_LO_USED_BYTES']}\n"
        f"sprite_chr_upper_used_bytes={sprite_metadata['SPR_CHR_HI_USED_BYTES']}\n"
        f"sprite_8x16_patterns={sprite_metadata['SPR_8X16_PATTERN_COUNT']}\n"
        f"sprite_chr_a_lo_bank={sprite_chr_a_lo_bank}\n"
        f"sprite_chr_a_hi_bank={sprite_chr_a_hi_bank}\n"
        f"background_chr_b_title_bank=0\n"
        f"background_chr_b_level_bank=1\n"
        f"title_stream_bytes={music_metadata['title_bytes']}\n"
        f"level_stream_bytes={music_metadata['level_bytes']}\n"
        f"dpcm_bytes={music_metadata['dpcm_bytes']}\n"
    )
    (args.output / "asset_report.txt").write_text(report, encoding="utf-8")
    print(report, end="")


if __name__ == "__main__":
    main()
