#!/usr/bin/env python3
"""Build GBA-native Tyrian title, Mode-0, OBJ, event, and audio assets."""

from __future__ import annotations

import argparse
import importlib.util
import struct
import wave
from pathlib import Path
from types import ModuleType

import numpy as np
from PIL import Image, ImageDraw


SCREEN_WIDTH = 240
SCREEN_HEIGHT = 160
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
    tuple(range(26, 32)),          # HDT 392: 50-credit spinning coin
    (32, 33, 34, 35, 36, 35),     # HDT 394: 100-credit silver coin
    (39, 41, 43, 45, 47, 49),     # HDT 397: 1000-credit 2x2 pickup
)
REWARD_VALUES = (50, 100, 1000)
REWARD_FRAMES_PER_SEQUENCE = 6


def load_snes_builder(workspace: Path) -> ModuleType:
    path = workspace / "org" / "TyrianSnesPoc" / "tools" / "build_assets.py"
    spec = importlib.util.spec_from_file_location("tyrian_snes_assets_for_gba", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"could not load SNES asset builder: {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


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


def bitmap_555(image: Image.Image) -> bytes:
    pixels = np.asarray(image.convert("RGB"), dtype=np.uint16)
    words = (
        (pixels[:, :, 0] >> 3)
        | ((pixels[:, :, 1] >> 3) << 5)
        | ((pixels[:, :, 2] >> 3) << 10)
    ).astype("<u2")
    return words.tobytes()


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


def add_sprite_outline(
    image: Image.Image,
    colour: tuple[int, int, int] = (255, 240, 144),
) -> Image.Image:
    """Add a one-pixel readability outline without moving source pixels."""
    rgba = np.asarray(image.convert("RGBA"), dtype=np.uint8).copy()
    opaque = rgba[:, :, 3] >= 80
    padded = np.pad(opaque, 1)
    expanded = np.zeros_like(opaque)
    for offset_y in range(3):
        for offset_x in range(3):
            expanded |= padded[
                offset_y : offset_y + opaque.shape[0],
                offset_x : offset_x + opaque.shape[1],
            ]
    outline = expanded & ~opaque
    rgba[outline, :3] = colour
    rgba[outline, 3] = 255
    return Image.fromarray(rgba, "RGBA")


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
    """Build three original spriteSheet11 coin animations for rewards."""
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
            if sequence_index == 2:
                source = snes.fit_sprite(
                    compose_sprite_2x2(source_dir, source_id),
                    (16, 16),
                )
            else:
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
            frame = add_sprite_outline(source)
            frames.append(frame)
            preview.alpha_composite(
                frame,
                (frame_index * 16, sequence_index * 16),
            )
    tile_data, palette = quantize_sprite_frames(snes, frames)
    return tile_data, palette, preview


DIGIT_PATTERNS = (
    ("01110", "10001", "10011", "10101", "11001", "10001", "01110"),
    ("00100", "01100", "00100", "00100", "00100", "00100", "01110"),
    ("01110", "10001", "00001", "00010", "00100", "01000", "11111"),
    ("11110", "00001", "00001", "01110", "00001", "00001", "11110"),
    ("00010", "00110", "01010", "10010", "11111", "00010", "00010"),
    ("11111", "10000", "10000", "11110", "00001", "00001", "11110"),
    ("01110", "10000", "10000", "11110", "10001", "10001", "01110"),
    ("11111", "00001", "00010", "00100", "01000", "01000", "01000"),
    ("01110", "10001", "10001", "01110", "10001", "10001", "01110"),
    ("01110", "10001", "10001", "01111", "00001", "00001", "01110"),
)


def build_score_digits(snes: ModuleType) -> tuple[bytes, bytes, Image.Image]:
    """Build compact outlined 8x8 digits for the reward counter."""
    tile_data = bytearray()
    palette = [
        (0, 0, 0),
        (8, 16, 28),
        (255, 232, 88),
    ]
    preview = Image.new("RGBA", (80, 8), (0, 0, 0, 0))
    preview_pixels = preview.load()
    for digit, pattern in enumerate(DIGIT_PATTERNS):
        values = np.zeros((8, 8), dtype=np.uint8)
        points = {
            (x + 1, y)
            for y, row in enumerate(pattern)
            for x, value in enumerate(row)
            if value == "1"
        }
        for x, y in points:
            for offset_y in (-1, 0, 1):
                for offset_x in (-1, 0, 1):
                    outline_x = x + offset_x
                    outline_y = y + offset_y
                    if 0 <= outline_x < 8 and 0 <= outline_y < 8:
                        values[outline_y, outline_x] = 1
        for x, y in points:
            values[y, x] = 2
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
    )


def repack_obj_tiles(
    snes_tiles: bytes,
    source_metadata: dict[str, int],
    explosion_tiles: bytes,
    reward_tiles: bytes,
    digit_tiles: bytes,
) -> tuple[bytes, dict[str, int]]:
    source_count = len(snes_tiles) // 32
    decoded = [
        decode_snes_4bpp(snes_tiles[index * 32 : index * 32 + 32])
        for index in range(source_count)
    ]
    output = bytearray()
    metadata: dict[str, int] = {}

    def append_asset(name: str, width_tiles: int, height_tiles: int) -> None:
        source_base = source_metadata[f"OBJ_TILE_{name}"]
        metadata[f"OBJ_TILE_{name}"] = len(output) // 32
        metadata[f"OBJ_PAL_{name}"] = source_metadata[f"OBJ_PAL_{name}"]
        for tile_y in range(height_tiles):
            for tile_x in range(width_tiles):
                source_index = (
                    source_base + tile_y * ATLAS_STRIDE_TILES + tile_x
                )
                output.extend(encode_gba_4bpp(decoded[source_index]))

    append_asset("PLAYER_0", 4, 4)
    append_asset("PLAYER_1", 4, 4)
    for index in range(24):
        append_asset(f"ENEMY_{index}", 4, 4)
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
        raise ValueError("GBA score digit bank must contain ten 8x8 tiles")
    metadata["OBJ_TILE_SCORE_DIGITS"] = len(output) // 32
    metadata["OBJ_PAL_SCORE_DIGITS"] = 9
    output.extend(digit_tiles)

    append_asset("PLAYER_SHOT", 2, 2)
    append_asset("ENEMY_SHOT", 2, 2)
    append_asset("BOSS_BAR", 2, 2)

    tile_count = len(output) // 32
    if tile_count > 1024:
        raise ValueError(f"GBA OBJ atlas exceeds 1024 tiles: {tile_count}")
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
    words = np.frombuffer(map_binary, dtype="<u2").reshape(-1, 32)
    output = Image.new("RGB", (256, row_count * 8), (0, 0, 0))
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


def load_default_player_shot(
    hdt_path: Path,
    image_root: Path,
) -> tuple[Image.Image, dict[str, int | str]]:
    """Resolve new-game Pulse Cannon power 1 through tyrian.hdt."""
    data = hdt_path.read_bytes()
    weapon_size = 80
    weapon_count = 781
    port_size = 82
    item_base = struct.unpack_from("<i", data, 0)[0] + 14
    port_table = item_base + weapon_count * weapon_size

    # OpenTyrian starts player 1 with front port ID 1 at power level 1.
    port_offset = port_table + port_size
    name_length = data[port_offset]
    port_name = (
        data[port_offset + 1 : port_offset + 1 + min(name_length, 30)]
        .decode("latin1")
        .rstrip()
    )
    weapon_record = struct.unpack_from("<H", data, port_offset + 32)[0]
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
        or weapon_record != 155
        or graphic != 59
        or multi != 1
        or sequence_max != 1
    ):
        raise ValueError(
            "unexpected Tyrian new-game Pulse Cannon layout: "
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
    *,
    bg1_rows: int,
    bg2_rows: int,
    bg3_rows: int,
    event_bytes: int,
    boss_tick: int,
    end_tick: int,
) -> None:
    # 34.78259095 Hz / the GBA's 59.72750057 Hz display rate, expressed
    # with the original 1,193,182 Hz PIT numerator.
    lines = [
        "#ifndef TYRIAN_GBA_ASSET_META_H",
        "#define TYRIAN_GBA_ASSET_META_H",
        "",
        f"#define BG1_ROWS {bg1_rows}u",
        f"#define BG2_ROWS {bg2_rows}u",
        f"#define BG3_ROWS {bg3_rows}u",
        f"#define LEVEL_EVENT_BYTES {event_bytes}u",
        f"#define LEVEL_BOSS_TICK {boss_tick}u",
        f"#define LEVEL_END_TICK {end_tick}u",
        "#define ORIGINAL_LOGIC_NUMERATOR 1193182ul",
        "#define ORIGINAL_LOGIC_DENOMINATOR 2048892ul",
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


def reward_code_for_value(value: int) -> int:
    if 1000 <= value < 10000:
        return 3
    if 100 <= value < 1000:
        return 2
    if 50 <= value < 100:
        return 1
    return 0


def encode_gba_level_events(
    nes: ModuleType,
    snes: ModuleType,
    events: list[tuple[int, int, int, int, int, int, int, int]],
    hdt_path: Path,
) -> tuple[bytes, int, int, dict[str, int]]:
    """Add a source-HDT-derived reward byte to every GBA spawn command.

    OpenTyrian normally credits each destroyed enemy's positive ``value``
    directly, and uses ``eenemydie`` only where a physical score item is
    authored.  Level 1 contains no ``eenemydie`` score items, so this hardware
    demo turns only its high-value (50+) enemies into visible pickups while
    retaining the exact 50/100/1000 denominations found in those HDT records.
    """
    source_enemy_ids: list[int] = []
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

    def enemy_fields(enemy_id: int) -> tuple[int, int, int]:
        if not 0 <= enemy_id < 851:
            raise ValueError(f"enemy ID outside tyrian.hdt: {enemy_id}")
        offset = enemy_table + enemy_id * 77
        armor = hdt[offset + 19]
        value = struct.unpack_from("<h", hdt, offset + 73)[0]
        enemy_die = struct.unpack_from("<H", hdt, offset + 75)[0]
        return armor, value, enemy_die

    output = bytearray()
    cursor = 0
    spawn_index = 0
    reward_counts = [0, 0, 0, 0]
    explicit_hdt_drops = 0
    adapted_high_value_drops = 0
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
            _, source_value, enemy_die = enemy_fields(enemy_id)
            reward_code = 0
            if enemy_die:
                target_armor, target_value, _ = enemy_fields(enemy_die)
                if target_armor == 0 and target_value != 0:
                    reward_code = reward_code_for_value(target_value)
                    if reward_code:
                        explicit_hdt_drops += 1
            if not reward_code:
                reward_code = reward_code_for_value(source_value)
                if reward_code:
                    adapted_high_value_drops += 1
            output.extend(encoded[cursor + 2 : cursor + 5])
            output.append(reward_code)
            reward_counts[reward_code] += 1
            spawn_index += 1
            cursor += 5
            continue
        if opcode in (
            nes.EVENT_MOVE,
            nes.EVENT_ACCEL,
            nes.EVENT_REVERSE,
            nes.EVENT_FIRE,
        ):
            length = 4
        elif opcode == nes.EVENT_FOREGROUND:
            length = 2
        else:
            raise ValueError(f"unknown shared level opcode 0x{opcode:02X}")
        output.extend(encoded[cursor + 2 : cursor + length])
        cursor += length

    if cursor != len(encoded) or spawn_index != spawn_count:
        raise ValueError(
            "GBA reward bytecode conversion did not consume its source: "
            f"cursor={cursor}/{len(encoded)}, spawns={spawn_index}/{spawn_count}"
        )
    report = {
        "eligible": sum(reward_counts[1:]),
        "value_50": reward_counts[1],
        "value_100": reward_counts[2],
        "value_1000": reward_counts[3],
        "explicit_hdt": explicit_hdt_drops,
        "adapted_high_value": adapted_high_value_drops,
    }
    return bytes(output), spawn_count, control_count, report


def add_background_motion_events(
    nes: ModuleType,
    encoded: bytes,
    source_events: list[tuple[int, int, int, int, int, int, int, int]],
) -> tuple[bytes, int]:
    """Merge the original three-layer speed changes into shared bytecode."""
    event_scroll = 0x85
    records: list[tuple[int, int, bytes]] = []
    cursor = 0
    absolute_time = 0

    while cursor + 1 < len(encoded):
        delta = encoded[cursor]
        opcode = encoded[cursor + 1]
        absolute_time += delta
        if opcode == nes.EVENT_END:
            break
        if opcode == nes.EVENT_WAIT:
            cursor += 2
            continue
        if opcode < 24:
            length = 6
        elif opcode in (
            nes.EVENT_MOVE,
            nes.EVENT_ACCEL,
            nes.EVENT_REVERSE,
            nes.EVENT_FIRE,
        ):
            length = 4
        elif opcode == nes.EVENT_FOREGROUND:
            length = 2
        else:
            raise ValueError(f"unknown shared level opcode 0x{opcode:02X}")
        records.append(
            (absolute_time, opcode, encoded[cursor + 2 : cursor + length])
        )
        cursor += length

    motion_records: list[tuple[int, int, bytes]] = []
    for (
        event_time,
        event_type,
        event_data,
        event_data_2,
        event_data_3,
        _,
        _,
        _,
    ) in source_events:
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
            continue
        motion_records.append(
            (event_time, event_scroll, bytes((*speeds, *delays)))
        )

    # Motion records sort before gameplay records at an identical timestamp,
    # matching OpenTyrian's event-before-background update order.
    merged = motion_records + records
    merged.sort(key=lambda record: record[0])
    output = bytearray()
    time_cursor = 0
    for event_time, opcode, payload in merged:
        delta = event_time - time_cursor
        while delta > 254:
            output.extend((254, nes.EVENT_WAIT))
            time_cursor += 254
            delta -= 254
        output.extend((delta, opcode))
        output.extend(payload)
        time_cursor = event_time
    output.extend((0, nes.EVENT_END))
    return bytes(output), len(motion_records)


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

    title = build_title(nes, image_root)
    (output / "title_bitmap.bin").write_bytes(bitmap_555(title))
    title.save(preview / "title_gba.png")

    lookups, maps, source_events = nes.parse_first_level(data_root / "tyrian1.lvl")
    layer1, _ = nes.render_map_layer(image_root, lookups[0], maps[0], 14, 3, 292)
    layer1 = layer1.crop((40, 0, 296, snes.BG1_ROWS * 8)).convert("RGBA")
    layer2, layer2_nonblank = nes.render_map_layer(
        image_root, lookups[1], maps[1], 14, 14, 593
    )
    layer2 = layer2.crop((40, 0, 296, snes.BG2_ROWS * 8)).convert("RGBA")
    layer3, layer3_nonblank = nes.render_map_layer(
        image_root, lookups[2], maps[2], 15, 14, 593
    )
    layer3 = layer3.crop((52, 0, 308, snes.BG2_ROWS * 8)).convert("RGBA")

    bg1_snes_tiles, bg1_snes_map, bg1_palettes, bg1_report, _ = (
        snes.quantize_mode1_layer(layer1, snes.BG1_PALETTES, 0)
    )
    bg2_snes_tiles, bg2_snes_map, bg2_palettes, bg2_report, _ = (
        snes.quantize_mode1_layer(layer2, snes.BG1_PALETTES, 0)
    )
    bg3_snes_tiles, bg3_snes_map, bg3_palettes, bg3_report, _ = (
        snes.quantize_mode1_layer(layer3, snes.BG1_PALETTES, 0)
    )
    palette_bytes = snes.snes_palette_bytes(
        bg1_palettes + bg2_palettes + bg3_palettes
    ).ljust(512, b"\0")
    if len(palette_bytes) != 512:
        raise ValueError(
            f"three-layer GBA palette must fit 512 bytes: {len(palette_bytes)}"
        )
    bg1_tiles = convert_tile_bank(bg1_snes_tiles)
    bg2_tiles = convert_tile_bank(bg2_snes_tiles)
    bg3_tiles = convert_tile_bank(bg3_snes_tiles)
    bg1_map = convert_tilemap(bg1_snes_map, 0)
    bg2_map = convert_tilemap(bg2_snes_map, snes.BG1_PALETTES)
    bg3_map = convert_tilemap(bg3_snes_map, snes.BG1_PALETTES * 2)
    (output / "bg1_tiles.bin").write_bytes(bg1_tiles)
    (output / "bg2_tiles.bin").write_bytes(bg2_tiles)
    (output / "bg3_tiles.bin").write_bytes(bg3_tiles)
    (output / "bg_palette.bin").write_bytes(palette_bytes)
    (output / "bg1_map.bin").write_bytes(bg1_map)
    (output / "bg2_map.bin").write_bytes(bg2_map)
    (output / "bg3_map.bin").write_bytes(bg3_map)
    reconstruct_gba_window(
        bg1_tiles, bg1_map, palette_bytes, snes.BG1_ROWS - 20
    ).crop((8, 0, 248, 160)).save(preview / "bg1_start_gba.png")
    reconstruct_gba_window(
        bg2_tiles, bg2_map, palette_bytes, snes.BG2_ROWS - 20
    ).crop((8, 0, 248, 160)).save(preview / "bg2_start_gba.png")
    reconstruct_gba_window(
        bg3_tiles, bg3_map, palette_bytes, snes.BG2_ROWS - 20
    ).crop((8, 0, 248, 160)).save(preview / "bg3_start_gba.png")

    (
        shared_level_events,
        spawn_count,
        control_count,
        reward_report,
    ) = encode_gba_level_events(
        nes,
        snes,
        source_events,
        data_root / "tyrian.hdt",
    )
    level_events, background_control_count = add_background_motion_events(
        nes, shared_level_events, source_events
    )
    (output / "level_events.bin").write_bytes(level_events)
    (output / "reward_drop_audit.txt").write_text(
        "\n".join((
            "policy=first-level HDT value >= 50, preserving 50/100/1000 tiers",
            f"eligible_spawn_records={reward_report['eligible']}",
            f"value_50_records={reward_report['value_50']}",
            f"value_100_records={reward_report['value_100']}",
            f"value_1000_records={reward_report['value_1000']}",
            f"explicit_eenemydie_records={reward_report['explicit_hdt']}",
            (
                "adapted_high_value_records="
                f"{reward_report['adapted_high_value']}"
            ),
        )) + "\n",
        encoding="utf-8",
    )

    sprite_audit_lines, sprite_audit = snes.audit_sprite_mapping(
        nes, source_events, data_root / "tyrian.hdt"
    )
    (output / "sprite_mapping_audit.txt").write_text(
        "\n".join(sprite_audit_lines) + "\n",
        encoding="utf-8",
    )
    player_shot_source, player_shot_report = load_default_player_shot(
        data_root / "tyrian.hdt",
        image_root,
    )
    player_shot_source.save(preview / "player_shot_059_source.png")
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
    digit_tiles, digit_palette, digit_preview = build_score_digits(snes)
    obj_palette = bytearray(obj_palette).ljust(512, b"\0")
    obj_palette[7 * 32 : 8 * 32] = explosion_palette
    obj_palette[8 * 32 : 9 * 32] = reward_palette
    obj_palette[9 * 32 : 10 * 32] = digit_palette
    obj_tiles, obj_metadata = repack_obj_tiles(
        snes_obj_tiles,
        source_metadata,
        explosion_tiles,
        reward_tiles,
        digit_tiles,
    )
    (output / "obj_tiles.bin").write_bytes(obj_tiles)
    (output / "obj_palette.bin").write_bytes(obj_palette)
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
    ).save(preview / "reward_coins_50_100_1000.png")
    digit_preview.resize(
        (digit_preview.width * 4, digit_preview.height * 4),
        Image.Resampling.NEAREST,
    ).save(preview / "reward_score_digits.png")

    title_music, title_report = snes.build_tym_tracker_it(
        workspace,
        workspace / "org" / "TyrianAudioLab" / "Music" / "30_tyrian_the_song.tym",
    )
    level_music, level_report = snes.build_tym_tracker_it(
        workspace,
        workspace / "org" / "TyrianAudioLab" / "Music" / "18_tyrian_the_level.tym",
    )
    (output / "tyrian_title_full.it").write_bytes(title_music)
    (output / "tyrian_level_full.it").write_bytes(level_music)
    sound_file = data_root / "tyrian.snd"
    sfx = snes.extract_tyrian_sfx(sound_file)
    # S_ITEM is one-based sample 18 in OpenTyrian, hence archive index 17.
    sfx.append((
        "item",
        extract_tyrian_sfx_entry(sound_file, 17),
        11_025,
        False,
    ))
    for name, pcm, rate, _ in sfx:
        write_signed_pcm_wav(output / f"{name}.wav", pcm, rate)

    write_meta_header(
        output,
        obj_metadata,
        bg1_rows=snes.BG1_ROWS,
        bg2_rows=snes.BG2_ROWS,
        bg3_rows=snes.BG2_ROWS,
        event_bytes=len(level_events),
        boss_tick=snes.LEVEL_BOSS_TICK,
        end_tick=snes.LEVEL_END_TICK,
    )
    report_lines = [
        "profile=GBA Mode 0 / complete Tyrian MAP1 + MAP2 + MAP3",
        "display_hz=59.7275",
        "logic_hz=34.7826",
        "background_layers=3 (Tyrian MAP1 + MAP2 + MAP3)",
        f"bg1_rows={snes.BG1_ROWS}",
        f"bg1_tiles={len(bg1_tiles) // 32}",
        f"bg1_source_unique_tiles={bg1_report['source_unique_tiles']}",
        f"bg1_approximated_tiles={bg1_report['approximated_tiles']}",
        f"bg2_rows={snes.BG2_ROWS}",
        f"bg2_tiles={len(bg2_tiles) // 32}",
        f"bg2_nonblank_source_cells={layer2_nonblank}",
        f"bg2_source_unique_tiles={bg2_report['source_unique_tiles']}",
        f"bg2_approximated_tiles={bg2_report['approximated_tiles']}",
        f"bg3_rows={snes.BG2_ROWS}",
        f"bg3_tiles={len(bg3_tiles) // 32}",
        f"bg3_nonblank_source_cells={layer3_nonblank}",
        f"bg3_source_unique_tiles={bg3_report['source_unique_tiles']}",
        f"bg3_approximated_tiles={bg3_report['approximated_tiles']}",
        f"level_event_source_records={len(source_events)}",
        f"level_event_spawn_records={spawn_count}",
        f"level_event_control_records={control_count}",
        f"level_background_control_records={background_control_count}",
        f"level_event_bytes={len(level_events)}",
        f"reward_eligible_spawn_records={reward_report['eligible']}",
        f"reward_value_50_records={reward_report['value_50']}",
        f"reward_value_100_records={reward_report['value_100']}",
        f"reward_value_1000_records={reward_report['value_1000']}",
        f"reward_explicit_eenemydie_records={reward_report['explicit_hdt']}",
        (
            "reward_adapted_high_value_records="
            f"{reward_report['adapted_high_value']}"
        ),
        f"obj_tiles={len(obj_tiles) // 32}",
        "obj_enemy_archetypes=24",
        f"explosion_animation_sequences={len(EXPLOSION_SOURCE_SEQUENCES)}",
        f"explosion_frames_per_sequence={EXPLOSION_FRAMES_PER_SEQUENCE}",
        "explosion_small_sources=122-133",
        "explosion_air_sources=3-14,41-52,22-33,60-71",
        "explosion_ground_sources=192-203,154-165,211-222,173-184",
        "explosion_anchor_mode=native_top_left",
        "explosion_quadrant_stride=12x14",
        f"explosion_animation_tiles={len(explosion_tiles) // 32}",
        "reward_sources_50=26-31",
        "reward_sources_100=32-36",
        "reward_sources_1000=HDT397 2x2 bases 39,41,43,45,47,49",
        f"reward_animation_tiles={len(reward_tiles) // 32}",
        f"reward_digit_tiles={len(digit_tiles) // 32}",
        f"player_shot_port={player_shot_report['port_name']}",
        f"player_shot_weapon_record={player_shot_report['weapon_record']}",
        f"player_shot_graphic={player_shot_report['graphic']}",
        f"player_shot_sheet={player_shot_report['sheet']}",
        f"player_shot_sprite_number={player_shot_report['sprite_number']}",
        f"player_shot_repeat={player_shot_report['shot_repeat']}",
        f"player_shot_vertical_speed={player_shot_report['vertical_speed']}",
        f"player_shot_animation_frames={player_shot_report['animation_frames']}",
        f"sprite_source_ids={sprite_audit['source_ids']}",
        f"sprite_unknown_spawns={sprite_audit['unknown_spawns']}",
        f"sprite_bank_mismatch_spawns={sprite_audit['bank_mismatch_spawns']}",
        f"sprite_exact_graphic_spawns={sprite_audit['exact_graphic_spawns']}",
        f"title_music_it_bytes={len(title_music)}",
        f"title_music_seconds={title_report['tracker_duration_seconds']:.6f}",
        f"level_music_it_bytes={len(level_music)}",
        f"level_music_pass_seconds={level_report['tracker_duration_seconds']:.6f}",
        f"level_music_laid_out_seconds={level_report['module_play_seconds']:.6f}",
        f"audio_sfx_samples={len(sfx)}",
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
