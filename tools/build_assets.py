#!/usr/bin/env python3
"""Build GBA-native Tyrian title, Mode-0, OBJ, event, and audio assets."""

from __future__ import annotations

import argparse
import collections
import hashlib
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
ENEMY_PROJECTILE_SOURCE_IDS = (58, 112, 113, 145, 146, 147, 201, 202)
ENEMY_PROJECTILE_WEAPON_IDS = (2, 3, 4, 59, 62, 78, 115, 116, 125, 126)
BOSS_PROJECTILE_WEAPON_IDS = (59, 127)
ENEMY_PROJECTILE_PALETTE_GROUPS = (
    (10, (112, 113)),       # animated red aimed/spread shot
    (11, (58, 201, 202)),   # orange dart and diagonal variants
    (12, (145, 146, 147)),  # purple left/down/right laser variants
)
OPENTYRIAN_SOURCE_COMMIT = "1c34d1bddac8c8f2de834229d04b5a729525c944"


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


def build_pause_text(
    snes: ModuleType,
    image_root: Path,
    palette_file: Path,
) -> tuple[bytes, bytes, Image.Image, tuple[int, ...]]:
    """Recreate JE_dString(PAUSED, FONT_SHAPES) at GBA display scale."""
    source_dir = image_root / "sprites" / "00_font"
    tyrian_palette = load_tyrian_palette(palette_file)
    font_colour_indices = {
        tyrian_palette[index]: index
        for index in range(0x10, 0x20)
    }
    frames: list[Image.Image] = []
    advances: list[int] = []

    for character, source_id in zip(
        PAUSE_TEXT,
        PAUSE_TEXT_SOURCE_IDS,
        strict=True,
    ):
        source = Image.open(source_dir / f"{source_id:03d}.png").convert("RGBA")
        if source.height != 15 or source.width not in (11, 12):
            raise ValueError(
                "unexpected Tyrian FONT_SHAPES pause glyph canvas: "
                f"{character}/{source_id} is {source.size}"
            )
        advances.append(((source.width + 1) * 3 + 2) // 4)
        rgba = np.asarray(source, dtype=np.uint8)
        transformed = np.zeros_like(rgba)
        for y, x in np.argwhere(rgba[:, :, 3] >= 80):
            colour = tuple(int(component) for component in rgba[y, x, :3])
            if colour not in font_colour_indices:
                raise ValueError(
                    "unexpected Tyrian FONT_SHAPES pause glyph colour: "
                    f"{character}/{source_id} contains {colour}"
                )
            source_index = font_colour_indices[colour]
            output_index = 0xF0 + ((source_index & 0x0F) - 3)
            transformed[y, x, :3] = tyrian_palette[output_index]
            transformed[y, x, 3] = 255

        # PC 320x200 -> GBA 240x160.  The 11/12x15 FONT_SHAPES glyphs
        # therefore become 8x12 and fit one 8x16 tall OBJ each.
        foreground = Image.fromarray(transformed, "RGBA").resize(
            (8, 12),
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
    projectile_tiles: bytes,
    projectile_layouts: tuple[dict[str, int], ...],
    boss_bar_tiles: bytes,
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
    projectile_base = len(output) // 32
    for layout in projectile_layouts:
        suffix = f"{layout['source_id']:03d}"
        metadata[f"OBJ_TILE_PROJECTILE_{suffix}"] = (
            projectile_base + layout["start_tile"]
        )
        metadata[f"OBJ_PAL_PROJECTILE_{suffix}"] = layout["palette_bank"]
        metadata[f"OBJ_PROJECTILE_OFFSET_X_{suffix}"] = layout["offset_x"]
        metadata[f"OBJ_PROJECTILE_OFFSET_Y_{suffix}"] = layout["offset_y"]
        metadata[f"OBJ_PROJECTILE_WIDTH_{suffix}"] = layout["width"]
        metadata[f"OBJ_PROJECTILE_HEIGHT_{suffix}"] = layout["height"]
    metadata["OBJ_PROJECTILE_SOURCE_COUNT"] = len(projectile_layouts)
    metadata["OBJ_PROJECTILE_TILE_COUNT"] = len(projectile_tiles) // 32
    output.extend(projectile_tiles)
    if len(boss_bar_tiles) != 4 * 32:
        raise ValueError("PC-style boss bar must occupy exactly four OBJ tiles")
    metadata["OBJ_TILE_BOSS_BAR"] = len(output) // 32
    metadata["OBJ_PAL_BOSS_BAR"] = 13
    output.extend(boss_bar_tiles)

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
    unsupported = used_weapon_ids.difference(ENEMY_PROJECTILE_WEAPON_IDS)
    if unsupported:
        raise ValueError(
            "first-level enemy weapon has no GBA projectile implementation: "
            + ",".join(str(value) for value in sorted(unsupported))
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
        "projectile_graphics="
        + ",".join(str(value) for value in ENEMY_PROJECTILE_SOURCE_IDS),
        "enemy_weapon_records="
        + ",".join(str(value) for value in sorted(used_weapon_ids)),
        "boss_weapon_records="
        + ",".join(str(value) for value in BOSS_PROJECTILE_WEAPON_IDS),
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


def build_opentyrian_level1_source_data(
    nes: ModuleType,
    events: list[tuple[int, int, int, int, int, int, int, int]],
    hdt_path: Path,
) -> tuple[bytes, bytes, dict[str, int | str], list[str]]:
    """Pack the unmodified first-level records needed by the direct C port.

    This is deliberately separate from ``level_events.bin``.  That older
    bytecode is the v11 visual proof's simplified runtime format.  The
    source-parity port consumes the original JE_EventRecType field values and
    exact 77-byte JE_EnemyDat records instead of reverse engineering the
    simplified stream.
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

    title = build_title(nes, image_root)
    (output / "title_bitmap.bin").write_bytes(bitmap_555(title))
    title.save(preview / "title_gba.png")

    lookups, maps, source_events = nes.parse_first_level(data_root / "tyrian1.lvl")
    (
        source_level_events,
        source_level_enemies,
        source_parity_report,
        source_parity_audit,
    ) = build_opentyrian_level1_source_data(
        nes,
        source_events,
        data_root / "tyrian.hdt",
    )
    (output / "opentyrian_level1_events.bin").write_bytes(source_level_events)
    (output / "opentyrian_level1_enemies.bin").write_bytes(source_level_enemies)
    (output / "opentyrian_level1_source_audit.txt").write_text(
        "\n".join(source_parity_audit) + "\n",
        encoding="utf-8",
    )
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
        projectile_audit_lines,
    ) = encode_gba_level_events(
        nes,
        snes,
        source_events,
        data_root / "tyrian.hdt",
    )
    level_events, background_control_count, dynamic_reward_report = (
        add_background_motion_events(
            nes,
            shared_level_events,
            source_events,
            data_root / "tyrian.hdt",
        )
    )
    (output / "level_events.bin").write_bytes(level_events)
    (output / "reward_drop_audit.txt").write_text(
        "\n".join((
            "policy=PC evalue direct cash plus event33 physical score items",
            f"static_eenemydie_reward_records={reward_report['eligible']}",
            f"direct_value_spawn_records={reward_report['direct_value_records']}",
            (
                "direct_value_authored_total="
                f"{reward_report['direct_value_authored_total']}"
            ),
            f"static_value_25_records={reward_report['value_25']}",
            f"static_value_50_records={reward_report['value_50']}",
            f"static_value_75_records={reward_report['value_75']}",
            f"static_value_100_records={reward_report['value_100']}",
            f"static_value_250_records={reward_report['value_250']}",
            f"explicit_eenemydie_records={reward_report['explicit_hdt']}",
            (
                "dynamic_event33_records="
                f"{dynamic_reward_report['dynamic_records']}"
            ),
            (
                "dynamic_cash_reward_records="
                f"{dynamic_reward_report['dynamic_cash_records']}"
            ),
            (
                "dynamic_non_cash_target_records="
                f"{dynamic_reward_report['dynamic_non_cash_records']}"
            ),
            f"dynamic_value_25_records={dynamic_reward_report['dynamic_value_25']}",
            f"dynamic_value_50_records={dynamic_reward_report['dynamic_value_50']}",
            f"dynamic_value_75_records={dynamic_reward_report['dynamic_value_75']}",
            (
                "dynamic_value_100_records="
                f"{dynamic_reward_report['dynamic_value_100']}"
            ),
            (
                "dynamic_value_250_records="
                f"{dynamic_reward_report['dynamic_value_250']}"
            ),
        )) + "\n",
        encoding="utf-8",
    )
    (output / "reward_event33_audit.csv").write_text(
        str(dynamic_reward_report["audit"]),
        encoding="utf-8",
    )
    (output / "enemy_projectile_audit.txt").write_text(
        "\n".join(projectile_audit_lines) + "\n",
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
    ) = build_pause_text(snes, image_root, data_root / "palette.dat")
    (
        projectile_tiles,
        projectile_palettes,
        projectile_preview,
        projectile_layouts,
    ) = build_enemy_projectiles(snes, image_root)
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
    if len(projectile_palettes) != 3 * 32:
        raise ValueError("enemy projectile palette bank count changed")
    obj_palette[10 * 32 : 13 * 32] = projectile_palettes
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
        projectile_tiles,
        projectile_layouts,
        boss_bar_tiles,
    )
    for flash, (bottom, middle, top) in enumerate(boss_bar_flash_colours):
        obj_metadata[f"BOSS_BAR_FLASH_{flash}_BOTTOM"] = bottom
        obj_metadata[f"BOSS_BAR_FLASH_{flash}_MIDDLE"] = middle
        obj_metadata[f"BOSS_BAR_FLASH_{flash}_TOP"] = top
    obj_metadata["OPENTYRIAN_LEVEL1_EVENT_COUNT"] = int(
        source_parity_report["event_count"]
    )
    obj_metadata["OPENTYRIAN_LEVEL1_EVENT_RECORD_BYTES"] = int(
        source_parity_report["event_record_bytes"]
    )
    obj_metadata["OPENTYRIAN_LEVEL1_EVENT_BYTES"] = int(
        source_parity_report["event_bytes"]
    )
    obj_metadata["OPENTYRIAN_LEVEL1_EVENTS_BEFORE_LEGACY_CUTOFF"] = int(
        source_parity_report["event_before_legacy_cutoff"]
    )
    obj_metadata["OPENTYRIAN_LEVEL1_ENEMY_COUNT"] = int(
        source_parity_report["enemy_count"]
    )
    obj_metadata["OPENTYRIAN_LEVEL1_ENEMY_RECORD_BYTES"] = int(
        source_parity_report["enemy_record_bytes"]
    )
    obj_metadata["OPENTYRIAN_LEVEL1_ENEMY_BYTES"] = int(
        source_parity_report["enemy_bytes"]
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
    ).save(preview / "reward_coins_25_50_75_100_250.png")
    digit_preview.resize(
        (digit_preview.width * 4, digit_preview.height * 4),
        Image.Resampling.NEAREST,
    ).save(preview / "cash_tiny_font_digits.png")
    pause_preview.resize(
        (pause_preview.width * 8, pause_preview.height * 8),
        Image.Resampling.NEAREST,
    ).save(preview / "paused_font_shapes.png")
    projectile_preview.resize(
        (projectile_preview.width * 6, projectile_preview.height * 6),
        Image.Resampling.NEAREST,
    ).save(preview / "enemy_projectiles_pc_source.png")
    boss_bar_preview.resize(
        (boss_bar_preview.width * 6, boss_bar_preview.height * 6),
        Image.Resampling.NEAREST,
    ).save(preview / "boss_bar_pc_style.png")

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
    # WeaponType.sound is one-based. These are the three additional effects
    # used by the exact level-1 enemy/boss weapon records (sound 1 already
    # exists as weapon_1.wav).
    for sound_id in (4, 6, 13):
        sfx.append((
            f"enemy_shot_{sound_id}",
            extract_tyrian_sfx_entry(sound_file, sound_id - 1),
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
        f"opentyrian_source_commit={source_commit}",
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
        (
            "source_parity_event_record_bytes="
            f"{source_parity_report['event_record_bytes']}"
        ),
        (
            "source_parity_event_bytes="
            f"{source_parity_report['event_bytes']}"
        ),
        (
            "source_parity_event_sha256="
            f"{source_parity_report['event_sha256']}"
        ),
        (
            "source_parity_enemy_dependency_records="
            f"{source_parity_report['enemy_count']}"
        ),
        (
            "source_parity_enemy_bytes="
            f"{source_parity_report['enemy_bytes']}"
        ),
        (
            "source_parity_enemy_sha256="
            f"{source_parity_report['enemy_sha256']}"
        ),
        f"level_event_spawn_records={spawn_count}",
        f"level_event_control_records={control_count}",
        f"level_background_control_records={background_control_count}",
        f"level_event_bytes={len(level_events)}",
        "level_event_clock=PC curLoc / MAP1 effective scroll",
        "spawn_coordinate_mode=PC initial Y + HDT motion + source pool scroll",
        (
            "spawn_world_coordinate_records="
            f"{reward_report['world_spawn_records']}"
        ),
        (
            "destructible_2x2_assemblies="
            f"{reward_report['destructible_assemblies']}"
        ),
        (
            "small_tank_component_records="
            f"{reward_report['tank_component_records']}"
        ),
        f"reward_static_spawn_records={reward_report['eligible']}",
        (
            "reward_direct_value_spawn_records="
            f"{reward_report['direct_value_records']}"
        ),
        (
            "reward_direct_value_authored_total="
            f"{reward_report['direct_value_authored_total']}"
        ),
        f"reward_static_value_25_records={reward_report['value_25']}",
        f"reward_static_value_50_records={reward_report['value_50']}",
        f"reward_static_value_75_records={reward_report['value_75']}",
        f"reward_static_value_100_records={reward_report['value_100']}",
        f"reward_static_value_250_records={reward_report['value_250']}",
        f"reward_explicit_eenemydie_records={reward_report['explicit_hdt']}",
        (
            "reward_dynamic_event33_records="
            f"{dynamic_reward_report['dynamic_records']}"
        ),
        (
            "reward_dynamic_cash_records="
            f"{dynamic_reward_report['dynamic_cash_records']}"
        ),
        (
            "reward_dynamic_non_cash_records="
            f"{dynamic_reward_report['dynamic_non_cash_records']}"
        ),
        (
            "reward_dynamic_value_25_records="
            f"{dynamic_reward_report['dynamic_value_25']}"
        ),
        (
            "reward_dynamic_value_50_records="
            f"{dynamic_reward_report['dynamic_value_50']}"
        ),
        (
            "reward_dynamic_value_75_records="
            f"{dynamic_reward_report['dynamic_value_75']}"
        ),
        (
            "reward_dynamic_value_100_records="
            f"{dynamic_reward_report['dynamic_value_100']}"
        ),
        (
            "reward_dynamic_value_250_records="
            f"{dynamic_reward_report['dynamic_value_250']}"
        ),
        f"obj_tiles={len(obj_tiles) // 32}",
        "obj_enemy_archetypes=24",
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
        "pause_text_scale=PC 320x200 to GBA 240x160 (8x12 in 8x16 OBJ)",
        f"pause_text_tiles={len(pause_tiles) // 32}",
        "enemy_projectile_source_graphics="
        + ",".join(str(value) for value in ENEMY_PROJECTILE_SOURCE_IDS),
        "enemy_projectile_weapon_records="
        + str(reward_report["weapon_records"]),
        "boss_projectile_weapon_records="
        + ",".join(str(value) for value in BOSS_PROJECTILE_WEAPON_IDS),
        f"enemy_projectile_tiles={len(projectile_tiles) // 32}",
        "enemy_projectile_palette_banks=10:red,11:dart,12:laser",
        "enemy_projectile_anchor=PC top-left canvas via generated crop offsets",
        "enemy_fire_slots=HDT tur[3]/freq[3] plus event31 three-slot overrides",
        f"enemy_fire_override_records={reward_report['fire_override_records']}",
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
