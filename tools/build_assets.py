#!/usr/bin/env python3
"""Build GBA-native Tyrian title, Mode-0, OBJ, event, and audio assets."""

from __future__ import annotations

import argparse
import collections
import hashlib
import importlib.util
import itertools
import json
import re
import struct
import sys
import wave
import zlib
from pathlib import Path
from types import ModuleType

import numpy as np
from PIL import Image, ImageDraw, ImageFont

import gba_asset_support as gba_assets
import gba_anm_builder as gba_anm
import gba_music_builder as gba_music


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
PC_BG2_FIRST_ROW = 0
PC_BG3_FIRST_ROW = 14
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
GBA_BG2_PACK_HEIGHT = (
    (PC_BG23_LAST_ROW - PC_BG2_FIRST_ROW + 1) * PC_MAP_CELL_HEIGHT
)
GBA_BG3_PACK_HEIGHT = (
    (PC_BG23_LAST_ROW - PC_BG3_FIRST_ROW + 1) * PC_MAP_CELL_HEIGHT
)
GBA_BG1_ROWS = GBA_BG1_PACK_HEIGHT // 8
GBA_BG2_ROWS = GBA_BG2_PACK_HEIGHT // 8
GBA_BG3_ROWS = GBA_BG3_PACK_HEIGHT // 8
GBA_BG1_INITIAL_SCROLL = (
    (PC_BG1_INITIAL_ROW - PC_BG1_FIRST_ROW) * PC_MAP_CELL_HEIGHT
    + GBA_VIEW_CROP_Y
)
GBA_BG2_INITIAL_SCROLL = (
    (PC_BG23_INITIAL_ROW - PC_BG2_FIRST_ROW) * PC_MAP_CELL_HEIGHT
    + GBA_VIEW_CROP_Y
)
GBA_BG3_INITIAL_SCROLL = (
    (PC_BG23_INITIAL_ROW - PC_BG3_FIRST_ROW) * PC_MAP_CELL_HEIGHT
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
FRONTEND_MENU_SOURCE_CROP_X = 0
FRONTEND_MENU_SOURCE_WIDTH = 300
# The 300-pixel crop deliberately keeps the PC menu's left equipment panel,
# but it also removes the bevel at source x=311..316.  Re-composite that
# narrow, palette-native strip at the final GBA edge so static menus retain a
# visually closed right border without changing their established layout.
FRONTEND_MENU_RIGHT_BORDER_SOURCE_X = (311, 312, 313, 314, 315, 316)
FRONTEND_PCX_PALETTES = (0, 7, 5, 8, 10, 5, 18, 19, 19, 20, 21, 22, 5)
FRONTEND_NAV_OBJ_SCALE_PHASES = 5
FRONTEND_NAV_OBJ_PHASE_COUNT = (
    FRONTEND_NAV_OBJ_SCALE_PHASES * FRONTEND_NAV_OBJ_SCALE_PHASES
)
FRONTEND_NAV_OBJ_META_BYTES = 12
FRONTEND_NAV_OBJ_PLANET_CATALOG_COUNT = 151
FRONTEND_NAV_OBJ_DOT_DIM = 8
FRONTEND_NAV_OBJ_VRAM_BYTES = 0x4000
FRONTEND_NAV_BITMAP_WIDTH = 126
FRONTEND_NAV_BITMAP_HEIGHT = 138
FRONTEND_NAV_BITMAP_STRIDE = 128
FRONTEND_NAV_BITMAP_BLOCK_ROWS = 2
FRONTEND_NAV_GRID_PHASES = 15
FRONTEND_NAV_BITMAP_PAGE_BYTES = (
    FRONTEND_NAV_BITMAP_STRIDE * FRONTEND_NAV_BITMAP_HEIGHT
)
FRONTEND_NAV_BITMAP_BLOCK_BYTES = (
    FRONTEND_NAV_BITMAP_STRIDE * FRONTEND_NAV_BITMAP_BLOCK_ROWS
)
FRONTEND_NAV_PLANET_GRAPHICS = (
    4, 1, 2, 3, 20, 36, 52, 68, 84, 100, 116,
    132, 151, 151, 151, 151, 52, 52, 1, 2, 4,
)
FRONTEND_NAV_PLANET_ANIMATED = (
    1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 1,
)
FRONTEND_NATIVE_FONT_CHARACTERS = (
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,!?'/:-%"
)
FRONTEND_NATIVE_FONT_HEIGHT = 7
FRONTEND_NATIVE_FONT_WIDTH = 6
FRONTEND_NATIVE_FONT_SPACE = 3
FRONTEND_NATIVE_FONT_SHADOW = 240
FRONTEND_PREGAME_FONT_CHARACTERS = (
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789.,!?'/:-%;[]+^"
)
FRONTEND_PREGAME_FONT_HEIGHT = 8
FRONTEND_PREGAME_FONT_WIDTH = 8
FRONTEND_PREGAME_FONT_SPACE = 4
FRONTEND_PREGAME_FONT_SHADOW = 240
FRONTEND_MENU_FONT_WIDTH = 7
FRONTEND_MENU_FONT_SPACE = 3
FRONTEND_SMALL_MIXED_FONT_WIDTH = 4
FRONTEND_SMALL_MIXED_FONT_SPACE = 2
BACKGROUND_PALETTE_BANK_COUNT = 16
BACKGROUND_PALETTE_COLOURS_PER_BANK = 16
BACKGROUND_PALETTE_SOURCE_COLOURS = 256
BACKGROUND_PALETTE_MASK_COUNT = 1 << 16
BACKGROUND_PALETTE_SHAPE_FILE_IDS = (")", "w", "x", "y", "z")
BACKGROUND_MIXED_MASK_GROUPS = (
    (0x0402,),                         # blue/teal + warm green
    (0x0401, 0x0403),                 # neutral + green
    (0x0082, 0x0480),                 # blue/teal + purple
    (0x1004, 0x1005, 0x0005),         # TORM rock/water boundaries
    (0x0048, 0x0108, 0x0148, 0x0041, 0x0441),
)
FRONTEND_STATS_FONT_GLYPH_COUNT = len(FRONTEND_NATIVE_FONT_CHARACTERS)
FRONTEND_STATS_FONT_TILES_PER_GLYPH = 4
FRONTEND_STATS_FONT_BRIGHTNESS_BIAS = 6
FRONTEND_STATS_GLYPH_TILE_COUNT = (
    FRONTEND_STATS_FONT_GLYPH_COUNT *
    FRONTEND_STATS_FONT_TILES_PER_GLYPH
)
FRONTEND_STATS_CUBE_TILE_OFFSET = (
    (FRONTEND_STATS_GLYPH_TILE_COUNT + 15) & ~15
)
FRONTEND_STATS_CUBE_TILE_COUNT = 16
FRONTEND_STATS_TILE_COUNT = (
    FRONTEND_STATS_CUBE_TILE_OFFSET +
    FRONTEND_STATS_CUBE_TILE_COUNT
)
FRONTEND_STATS_TILE_BYTES = FRONTEND_STATS_TILE_COUNT * 32
FRONTEND_STATIC_MENU_PANEL_X = 120
FRONTEND_STATIC_MENU_PANEL_Y = 0
FRONTEND_STATIC_MENU_PANEL_WIDTH = 120
FRONTEND_STATIC_MENU_PANEL_HEIGHT = 120
FRONTEND_STATIC_MENU_PANEL_BYTES = (
    FRONTEND_STATIC_MENU_PANEL_WIDTH *
    FRONTEND_STATIC_MENU_PANEL_HEIGHT
)
FRONTEND_STATIC_GAME_MENU_COUNT = 6
FRONTEND_STATIC_UPGRADE_MENU_COUNT = 8
FRONTEND_STATIC_OPTIONS_MENU_COUNT = 1
FRONTEND_STATIC_SAVE_MENU_COUNT = 2
FRONTEND_STATIC_SAVE_NAME_MENU_COUNT = 0
FRONTEND_STATIC_SOURCE_HELP_STRIP_COUNT = 34
FRONTEND_STATIC_OPTIONS_HELP_STRIP_COUNT = 0
FRONTEND_STATIC_SAVE_HELP_STRIP_COUNT = 0
FRONTEND_STATIC_SAVE_NAME_HELP_STRIP_COUNT = 0
FRONTEND_STATIC_HELP_STRIP_COUNT = (
    FRONTEND_STATIC_SOURCE_HELP_STRIP_COUNT +
    FRONTEND_STATIC_OPTIONS_HELP_STRIP_COUNT +
    FRONTEND_STATIC_SAVE_HELP_STRIP_COUNT +
    FRONTEND_STATIC_SAVE_NAME_HELP_STRIP_COUNT
)
FRONTEND_SOURCE_STAMP_SCALE_PHASES = 5
FRONTEND_SOURCE_STAMP_PHASE_COUNT = (
    FRONTEND_SOURCE_STAMP_SCALE_PHASES *
    FRONTEND_SOURCE_STAMP_SCALE_PHASES
)
FRONTEND_SOURCE_STAMP_SHP_RANGES = (
    # FACE_SHAPES 0..11: every stock Data Cube portrait.
    (4, 0, 12),
    # OPTION_SHAPES 12..14: JE_weaponViewFrame plus the two power arrows.
    (5, 12, 3),
    # OPTION_SHAPES 17: the stock disabled simulator message.
    (5, 17, 1),
    # OPTION_SHAPES 20..34: Data/Ship pages plus ship/shield menu art.
    (5, 20, 15),
    (6, 0, 22),
)
FRONTEND_SOURCE_STAMP_COMP_TABLES = (38, 39)
FRONTEND_SOURCE_STAMP_COMP_GRAPHIC_COUNT = 284

assert GBA_VIEW_CROP_X == 12
assert GBA_VIEW_CROP_Y == 12
assert GBA_BG_MAP_COLUMNS == 64
assert GBA_BG1_SOURCE_HEIGHT == 8316
assert GBA_BG1_ROWS == 1040
assert GBA_BG2_ROWS == 2100
assert GBA_BG3_ROWS == 2051
assert GBA_BG1_INITIAL_SCROLL == 8104
assert GBA_BG2_INITIAL_SCROLL == 16588
assert GBA_BG3_INITIAL_SCROLL == 16196
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
# OpenTyrian loads tyrianc.shp as a complete replacement, but binary audit
# proves that only its player shots (8), ships/options (9) and power-ups (10)
# differ.  Keep one lossless alternate raw copy for only those logical banks.
SPRITE2_XMAS_RAW_TABLES = (26, 36, 38)
JUKEBOX_MUSIC_COUNT = 41
JUKEBOX_TITLE_BYTES = 48
JUKEBOX_BACKDROP_TILE_COUNT = 16
JUKEBOX_STAR_TILE_COUNT = 3
JUKEBOX_RECIPROCAL_MAX_Z = 500


def load_frontend_native_font(path: Path) -> np.ndarray:
    characters: list[str] = []
    rows: list[list[int]] = []

    for line_number, raw_line in enumerate(
        path.read_text(encoding="ascii").splitlines(),
        start=1,
    ):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        fields = line.split()
        if len(fields) != FRONTEND_NATIVE_FONT_HEIGHT + 1:
            raise ValueError(
                "native font row must contain one glyph and seven bytes: "
                f"{path}:{line_number}"
            )
        character = fields[0]
        if len(character) != 1:
            raise ValueError(
                f"native font glyph key is not one character: {character!r}"
            )
        values = [int(field, 16) for field in fields[1:]]
        if any(value < 0 or value > 0x1f for value in values):
            raise ValueError(
                f"native font row exceeds five occupied bits: {character!r}"
            )
        characters.append(character)
        rows.append(values)
    if "".join(characters) != FRONTEND_NATIVE_FONT_CHARACTERS:
        raise ValueError(
            "native font character order changed: "
            f"{''.join(characters)!r}"
        )
    return np.asarray(rows, dtype=np.uint8)


def load_frontend_pregame_font(path: Path) -> np.ndarray:
    """Load the project-specific mixed-case setup-menu bitmap face."""
    characters: list[str] = []
    rows: list[list[int]] = []

    for line_number, raw_line in enumerate(
        path.read_text(encoding="ascii").splitlines(),
        start=1,
    ):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        fields = line.split()
        if len(fields) != FRONTEND_PREGAME_FONT_HEIGHT + 1:
            raise ValueError(
                "pre-game font row must contain one glyph and eight bytes: "
                f"{path}:{line_number}"
            )
        character = fields[0]
        if len(character) != 1:
            raise ValueError(
                f"pre-game font glyph key is not one character: {character!r}"
            )
        values = [int(field, 16) for field in fields[1:]]
        if any(value < 0 or value > 0x3f for value in values):
            raise ValueError(
                f"pre-game font row exceeds six source bits: {character!r}"
            )
        characters.append(character)
        rows.append(values)
    if "".join(characters) != FRONTEND_PREGAME_FONT_CHARACTERS:
        raise ValueError(
            "pre-game font character order changed: "
            f"{''.join(characters)!r}"
        )
    font = np.asarray(rows, dtype=np.uint8)

    def compact_rows(character: str) -> tuple[int, ...]:
        glyph = font[FRONTEND_PREGAME_FONT_CHARACTERS.index(character)]
        occupied = 0
        for value in glyph:
            occupied |= int(value)
        source_width = max(1, occupied.bit_length())
        target_width = min(FRONTEND_SMALL_MIXED_FONT_WIDTH, source_width)
        output: list[int] = []

        for value in glyph:
            packed = 0
            for column in range(target_width):
                source_column = (
                    0 if target_width == 1 else
                    column * (source_width - 1) // (target_width - 1)
                )
                if int(value) & (1 << (source_width - source_column - 1)):
                    packed |= 1 << (target_width - column - 1)
            output.append(packed)
        return tuple(output)

    compact_two = compact_rows("2")
    compact_four = compact_rows("4")
    if (
        any(row == 0 for row in compact_two[:7]) or
        compact_two[0] != 0x0F or
        compact_two[6] != 0x0F or
        compact_two[7] != 0
    ):
        raise ValueError(
            f"small mixed digit 2 lost cap/baseline strokes: {compact_two!r}"
        )
    if (
        any(row == 0 for row in compact_four[:7]) or
        compact_four[3] != 0x0F or
        any((row & 1) == 0 for row in compact_four[4:7]) or
        compact_four[7] != 0
    ):
        raise ValueError(
            f"small mixed digit 4 lost its vertical stroke: {compact_four!r}"
        )
    if compact_rows("m") == compact_rows("n"):
        raise ValueError("small mixed m/n glyphs collapsed to the same shape")
    return font


FRONTEND_LAYOUT_KEYS = (
    "TYRIAN_GBA_LAYOUT_TITLE_MENU_CENTER_X",
    "TYRIAN_GBA_LAYOUT_TITLE_MENU_FIRST_Y",
    "TYRIAN_GBA_LAYOUT_TITLE_MENU_ROW_STEP",
    "TYRIAN_GBA_LAYOUT_SETUP_HEADER_CENTER_X",
    "TYRIAN_GBA_LAYOUT_SETUP_HEADER_Y",
    "TYRIAN_GBA_LAYOUT_SETUP_CHOICE_CENTER_X",
    "TYRIAN_GBA_LAYOUT_SETUP_CHOICE_FIRST_Y",
    "TYRIAN_GBA_LAYOUT_SETUP_CHOICE_ROW_STEP",
    "TYRIAN_GBA_LAYOUT_EPISODE_CHOICE_X",
    "TYRIAN_GBA_LAYOUT_EPISODE_CHOICE_RIGHT",
    "TYRIAN_GBA_LAYOUT_EPISODE_CHOICE_FIRST_Y",
    "TYRIAN_GBA_LAYOUT_EPISODE_CHOICE_ROW_STEP",
    "TYRIAN_GBA_LAYOUT_GAME_MENU_TITLE_CENTER_X",
    "TYRIAN_GBA_LAYOUT_GAME_MENU_TITLE_Y",
    "TYRIAN_GBA_LAYOUT_GAME_MENU_ITEM_X",
    "TYRIAN_GBA_LAYOUT_GAME_MENU_ITEM_RIGHT",
    "TYRIAN_GBA_LAYOUT_GAME_MENU_FIRST_SOURCE_Y",
    "TYRIAN_GBA_LAYOUT_GAME_MENU_SOURCE_ROW_STEP",
    "TYRIAN_GBA_LAYOUT_GAME_MENU_QUIT_SOURCE_GAP",
    "TYRIAN_GBA_LAYOUT_GAME_MENU_HELP_X",
    "TYRIAN_GBA_LAYOUT_GAME_MENU_HELP_Y",
    "TYRIAN_GBA_LAYOUT_GAME_MENU_HELP_RIGHT",
    "TYRIAN_GBA_LAYOUT_UPGRADE_TITLE_CENTER_X",
    "TYRIAN_GBA_LAYOUT_UPGRADE_TITLE_Y",
    "TYRIAN_GBA_LAYOUT_UPGRADE_ITEM_X",
    "TYRIAN_GBA_LAYOUT_UPGRADE_ITEM_RIGHT",
    "TYRIAN_GBA_LAYOUT_UPGRADE_ITEM_FIRST_Y",
    "TYRIAN_GBA_LAYOUT_UPGRADE_ITEM_ROW_STEP",
    "TYRIAN_GBA_LAYOUT_QUIT_QUESTION_X",
    "TYRIAN_GBA_LAYOUT_QUIT_QUESTION_Y",
    "TYRIAN_GBA_LAYOUT_QUIT_QUESTION_RIGHT",
    "TYRIAN_GBA_LAYOUT_QUIT_HELP_X",
    "TYRIAN_GBA_LAYOUT_QUIT_HELP_Y",
    "TYRIAN_GBA_LAYOUT_QUIT_HELP_RIGHT",
    "TYRIAN_GBA_LAYOUT_QUIT_OK_CENTER_X",
    "TYRIAN_GBA_LAYOUT_QUIT_CANCEL_CENTER_X",
    "TYRIAN_GBA_LAYOUT_QUIT_CHOICES_Y",
)


def load_frontend_layout(path: Path) -> dict[str, int]:
    """Read literal user layout defaults shared by build-time static assets."""
    values: dict[str, int] = {}
    pattern = re.compile(
        r"^\s*#define\s+(TYRIAN_GBA_LAYOUT_[A-Z0-9_]+)"
        r"\s+(-?(?:0[xX][0-9a-fA-F]+|\d+))\s*$"
    )
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        match = pattern.match(raw_line)
        if match:
            values[match.group(1)] = int(match.group(2), 0)
    missing = [name for name in FRONTEND_LAYOUT_KEYS if name not in values]
    if missing:
        raise ValueError(
            "Configure.h is missing literal static-layout definitions: "
            + ", ".join(missing)
        )
    return values


def enemy_frame_palette_bank(key: tuple[int, int, int]) -> int:
    if key in ENEMY_STRUCTURE_FRAME_KEYS:
        return ENEMY_STRUCTURE_PALETTE_BANK
    return ENEMY_FRAME_PALETTE_GROUPS[key[0]]


def load_background_palette_trainer() -> ModuleType:
    path = Path(__file__).with_name("background_palette_training.py")
    module_name = "tyrian_gba_background_palette_training"
    spec = importlib.util.spec_from_file_location(module_name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError(
            f"could not load background palette trainer: {path}"
        )
    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    try:
        spec.loader.exec_module(module)
    except Exception:
        sys.modules.pop(module_name, None)
        raise
    return module


def load_music_maxmod_calibrator() -> ModuleType:
    path = Path(__file__).with_name("music_maxmod_calibration.py")
    module_name = "tyrian_gba_music_maxmod_calibration"
    spec = importlib.util.spec_from_file_location(module_name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError(
            f"could not load Maxmod music calibrator: {path}"
        )
    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    try:
        spec.loader.exec_module(module)
    except Exception:
        sys.modules.pop(module_name, None)
        raise
    return module


def read_git_head(repo: Path) -> str:
    """Read a local Git HEAD without depending on git.exe being on PATH."""
    git_dir = repo / ".git"
    revision = repo / "REVISION"
    if not git_dir.exists() and revision.is_file():
        head = revision.read_text(encoding="ascii").strip()
        if (
            len(head) == 40 and
            all(char in "0123456789abcdef" for char in head)
        ):
            return head
        raise ValueError(f"unexpected source REVISION value: {head}")
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
    """Resolve every gameplay Sprite2 bank using OpenTyrian's table rules."""
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


def build_sprite2_xmas_raw_components(
    data_root: Path,
) -> tuple[bytes, dict[str, int | str]]:
    """Losslessly expand the three tyrianc.shp banks that differ."""
    tyrian_shp = (data_root / "tyrianc.shp").read_bytes()
    output = bytearray()
    encoded_crc32 = 0
    encoded_bytes = 0
    component_count = 0

    for shape_table in SPRITE2_XMAS_RAW_TABLES:
        bank = sprite2_logical_bank(data_root, tyrian_shp, shape_table)
        first_offset = struct.unpack_from("<H", bank, 0)[0]
        sprite_count = first_offset // 2
        if sprite_count != SPRITE2_RAW_COMPONENTS_PER_TABLE:
            raise ValueError(
                f"Christmas Sprite2 table {shape_table} count changed: "
                f"{sprite_count} != {SPRITE2_RAW_COMPONENTS_PER_TABLE}"
            )
        for sprite_number in range(1, sprite_count + 1):
            encoded = sprite2_component_stream(bank, sprite_number)
            raw = decode_sprite2_raw_component(encoded)
            if len(raw) != SPRITE2_RAW_COMPONENT_BYTES:
                raise AssertionError(
                    "Christmas Sprite2 raw component stride changed"
                )
            output.extend(raw)
            encoded_crc32 = zlib.crc32(encoded, encoded_crc32)
            encoded_bytes += len(encoded)
            component_count += 1

    expected_components = (
        len(SPRITE2_XMAS_RAW_TABLES) *
        SPRITE2_RAW_COMPONENTS_PER_TABLE
    )
    expected_bytes = expected_components * SPRITE2_RAW_COMPONENT_BYTES
    if component_count != expected_components or len(output) != expected_bytes:
        raise AssertionError(
            "Christmas Sprite2 raw catalog changed: "
            f"{component_count=}, bytes={len(output)}, "
            f"expected={expected_bytes}"
        )
    report: dict[str, int | str] = {
        "version": SPRITE2_RAW_VERSION,
        "table_count": len(SPRITE2_XMAS_RAW_TABLES),
        "table_ids": ",".join(str(v) for v in SPRITE2_XMAS_RAW_TABLES),
        "components_per_table": SPRITE2_RAW_COMPONENTS_PER_TABLE,
        "component_count": component_count,
        "raw_bytes": len(output),
        "raw_crc32": f"{zlib.crc32(output) & 0xffffffff:08x}",
        "raw_sha256": hashlib.sha256(output).hexdigest(),
        "source_stream_bytes": encoded_bytes,
        "source_stream_crc32": f"{encoded_crc32 & 0xffffffff:08x}",
        "roundtrip_components": component_count,
    }
    return bytes(output), report


def train_sprite2_palette_brightness_samples(
    data_root: Path,
    sprite2_raw: bytes,
    sprite2_xmas_raw: bytes,
) -> tuple[tuple[tuple[int, ...], ...], dict[str, int]]:
    """Choose the best eight source brightnesses independently per hue.

    GBA 8bpp OBJ pixels share palette memory with the player's 4bpp actors,
    HUD, effects and compact Boss cache.  The Sprite2 runtime consequently
    owns 128 entries: eight colours for each of Tyrian's sixteen hue ramps.
    Train those eight medoids from the complete normal and Christmas raw
    catalogs instead of assuming one evenly spaced ramp fits every hue.
    """
    sample_count = 8
    source = np.frombuffer(
        sprite2_raw + sprite2_xmas_raw,
        dtype=np.uint8,
    )
    histogram = np.bincount(
        source[source != 0],
        minlength=256,
    ).reshape(16, 16).astype(np.int64)
    palette_bytes = (data_root / "palette.dat").read_bytes()
    palette_count = len(palette_bytes) // (256 * 3)
    palette = np.frombuffer(
        palette_bytes,
        dtype=np.uint8,
    ).reshape(palette_count, 256, 3)[5].astype(np.int32)
    candidates = np.asarray(
        list(itertools.combinations(range(16), sample_count)),
        dtype=np.intp,
    )
    baseline = np.asarray((0, 2, 4, 6, 8, 10, 12, 15), dtype=np.intp)
    trained: list[tuple[int, ...]] = []
    baseline_error = 0
    trained_error = 0

    for hue in range(16):
        colours = palette[hue * 16 : hue * 16 + 16]
        distance = (
            (colours[:, None, :] - colours[None, :, :]) ** 2
        ).sum(axis=2)
        baseline_error += int(
            (
                histogram[hue] *
                distance[:, baseline].min(axis=1)
            ).sum()
        )
        errors = (
            histogram[hue, None, :] *
            distance[:, candidates].min(axis=2).T
        ).sum(axis=1)
        best_index = int(errors.argmin())
        trained.append(
            tuple(int(value) for value in candidates[best_index])
        )
        trained_error += int(errors[best_index])

    if trained_error >= baseline_error:
        raise ValueError(
            "Sprite2 brightness training did not improve the baseline: "
            f"{baseline_error=} {trained_error=}"
        )
    improvement_basis_points = (
        (baseline_error - trained_error) * 10000 // baseline_error
    )
    return tuple(trained), {
        "sample_count": sample_count,
        "baseline_error": baseline_error,
        "trained_error": trained_error,
        "improvement_basis_points": improvement_basis_points,
    }


def write_sprite2_raw_header(
    output: Path,
    report: dict[str, int | str],
    xmas_report: dict[str, int | str],
    brightness_samples: tuple[tuple[int, ...], ...],
    palette_report: dict[str, int],
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
        (
            "#define SPRITE2_XMAS_RAW_TABLE_COUNT "
            f"{xmas_report['table_count']}u"
        ),
        "#define SPRITE2_XMAS_RAW_TABLE_0 26u",
        "#define SPRITE2_XMAS_RAW_TABLE_1 36u",
        "#define SPRITE2_XMAS_RAW_TABLE_2 38u",
        (
            "#define SPRITE2_XMAS_RAW_COMPONENT_COUNT "
            f"{xmas_report['component_count']}u"
        ),
        (
            "#define SPRITE2_XMAS_RAW_DATA_BYTES "
            f"{xmas_report['raw_bytes']}u"
        ),
        (
            "#define SPRITE2_XMAS_RAW_DATA_CRC32 "
            f"0x{xmas_report['raw_crc32']}u"
        ),
        (
            "#define SPRITE2_XMAS_RAW_SOURCE_STREAM_CRC32 "
            f"0x{xmas_report['source_stream_crc32']}u"
        ),
        (
            "#define SPRITE2_XMAS_RAW_ROUNDTRIP_COMPONENTS "
            f"{xmas_report['roundtrip_components']}u"
        ),
        "",
        (
            "#define SPRITE2_PALETTE_BRIGHTNESS_SAMPLE_COUNT "
            f"{palette_report['sample_count']}u"
        ),
        (
            "#define SPRITE2_PALETTE_BASELINE_ERROR "
            f"{palette_report['baseline_error']}u"
        ),
        (
            "#define SPRITE2_PALETTE_TRAINED_ERROR "
            f"{palette_report['trained_error']}u"
        ),
        (
            "#define SPRITE2_PALETTE_IMPROVEMENT_BASIS_POINTS "
            f"{palette_report['improvement_basis_points']}u"
        ),
        "",
    ]
    for hue, samples in enumerate(brightness_samples):
        values = ", ".join(f"{value}u" for value in samples)
        lines.append(
            f"#define SPRITE2_PALETTE_BRIGHTNESS_HUE_{hue:X} "
            f"{{ {values} }}"
        )
    lines.extend([
        "",
        "#endif",
        "",
    ])
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


def decode_gba_4bpp(tile: bytes) -> np.ndarray:
    if len(tile) != 32:
        raise ValueError("GBA 4bpp tile must be 32 bytes")
    values = np.zeros((8, 8), dtype=np.uint8)
    for y in range(8):
        for pair in range(4):
            packed = tile[y * 4 + pair]
            values[y, pair * 2] = packed & 0x0F
            values[y, pair * 2 + 1] = packed >> 4
    return values


def build_title(image_root: Path) -> Image.Image:
    """Build the GBA preview title directly from project-local PC artwork."""
    planet = Image.open(image_root / "pics" / "pic_04.png").convert("RGB")
    planet = planet.resize(
        (SCREEN_WIDTH, 113),
        Image.Resampling.LANCZOS,
    )
    logo = Image.open(
        image_root / "sprites" / "03_planet" / "146.png"
    ).convert("RGBA")
    logo.thumbnail((224, 72), Image.Resampling.LANCZOS)

    output = Image.new("RGB", (SCREEN_WIDTH, SCREEN_HEIGHT), (0, 0, 0))
    output.paste(planet, (0, 0))
    output.paste(logo, ((SCREEN_WIDTH - logo.width) // 2, 6), logo)
    draw = ImageDraw.Draw(output)
    draw.rectangle(
        (0, 108, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1),
        fill=(0, 0, 0),
    )

    def centred(
        y: int,
        text: str,
        colour: tuple[int, int, int],
    ) -> None:
        font = ImageFont.load_default()
        box = draw.textbbox((0, 0), text, font=font)
        width = box[2] - box[0]
        draw.text(((SCREEN_WIDTH - width) // 2, y), text, font=font, fill=colour)

    centred(116, "PRESS START", (255, 255, 255))
    centred(145, "APR TYRIAN GBA", (104, 208, 255))
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
    misc_text_b = read_group(5)
    skip_group(11)
    title_menu = read_group(7)
    event_text = read_group(9)
    skip_group(6)
    main_menu_help = read_group(34)
    full_game_menu = read_group(7)
    upgrade_menu = read_group(9)
    options_menu = read_group(8)
    skip_group(6)
    skip_group(6)
    skip_group(5)
    episode_name = read_group(6)
    difficulty_name = read_group(7)
    gameplay_name = read_group(5)
    return {
        "planet_name": planet_name,
        "misc_text": misc_text,
        "misc_text_b": misc_text_b,
        "title_menu": title_menu,
        "event_text": event_text,
        "main_menu_help": main_menu_help,
        "full_game_menu": full_game_menu,
        "upgrade_menu": upgrade_menu,
        "options_menu": options_menu,
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
        return self.palette_rgb_index(
            FRONTEND_PCX_PALETTES[picture_number - 1]
        )

    def palette_rgb_index(self, palette_number: int) -> np.ndarray:
        if not 0 <= palette_number < 23:
            raise ValueError(
                f"palette number outside source file: {palette_number}"
            )
        offset = palette_number * 256 * 3
        return np.frombuffer(
            self.palette_data[offset : offset + 256 * 3],
            dtype=np.uint8,
        ).reshape(256, 3)

    def palette_gba(self, picture_number: int) -> bytes:
        return self.palette_gba_index(
            FRONTEND_PCX_PALETTES[picture_number - 1]
        )

    def palette_gba_index(self, palette_number: int) -> bytes:
        rgb = self.palette_rgb_index(palette_number).astype(np.uint16)
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

    def menu_picture_frame(self, picture_number: int) -> np.ndarray:
        """Crop 320x200 menu art to 300x200 before the 240x160 resize."""
        picture = self.decode_picture(picture_number)
        source_x = (
            FRONTEND_MENU_SOURCE_CROP_X +
            np.arange(FRONTEND_FRAME_WIDTH) *
            FRONTEND_MENU_SOURCE_WIDTH //
            FRONTEND_FRAME_WIDTH
        )
        source_y = (
            np.arange(FRONTEND_FRAME_HEIGHT) *
            200 //
            FRONTEND_FRAME_HEIGHT
        )
        frame = picture[np.ix_(source_y, source_x)].copy()
        right_border_x = np.asarray(
            FRONTEND_MENU_RIGHT_BORDER_SOURCE_X,
            dtype=np.intp,
        )
        frame[:, -right_border_x.size:] = picture[
            np.ix_(source_y, right_border_x)
        ]
        return frame

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


def build_frontend_stats_assets(
    data_root: Path,
    cube_pixels: bytes,
) -> tuple[bytes, bytes, dict[str, int], list[str]]:
    """Bake the source TINY_FONT summary glyphs into native OBJ tiles.

    JE_endLevelAni's text palette still glows at runtime, but neither the
    glyph RLE nor the 16x16 outline/tile conversion changes between levels.
    Moving that immutable work to the build removes the only remaining
    multi-VBlank spike in the post-level summary.
    """

    source = FrontendSourceRenderer(data_root)
    tiles = bytearray(FRONTEND_STATS_TILE_BYTES)
    widths = bytearray()

    def source_glyph(character: str) -> int:
        if "a" <= character <= "z":
            character = character.upper()
        if "A" <= character <= "Z":
            return ord(character) - ord("A")
        if "1" <= character <= "9":
            return 69 + ord(character) - ord("0")
        return {
            "0": 79,
            "!": 26,
            "?": 27,
            ".": 28,
            ",": 29,
            ":": 31,
            "'": 32,
            "%": 62,
            "-": 83,
            "/": 80,
        }.get(character, 27)

    def write_4bpp(
        target: bytearray,
        base: int,
        tile_columns: int,
        x: int,
        y: int,
        colour: int,
    ) -> None:
        offset = (
            base +
            ((y >> 3) * tile_columns + (x >> 3)) * 32 +
            (y & 7) * 4 +
            (x & 7) // 2
        )
        shift = 4 if x & 1 else 0
        mask = 0x0F if shift else 0xF0
        target[offset] = (
            (target[offset] & mask) |
            ((colour & 0x0F) << shift)
        )

    for slot, character in enumerate(FRONTEND_NATIVE_FONT_CHARACTERS):
        sprite = source.sprite(2, source_glyph(character))
        if (
            sprite is None or
            sprite.shape[0] == 0 or
            sprite.shape[1] == 0 or
            sprite.shape[0] > 14 or
            sprite.shape[1] > 14
        ):
            raise ValueError(
                "front-end stats glyph source changed: "
                f"{character!r}, "
                f"{None if sprite is None else sprite.shape}"
            )
        height, width = sprite.shape
        pixels = np.zeros((16, 16), dtype=np.uint8)
        for y in range(height):
            for x in range(width):
                raw = int(sprite[y, x]) & 0x0F
                if int(sprite[y, x]) != 0xFF and raw >= 2:
                    pixels[y + 1, x + 1] = min(
                        raw + FRONTEND_STATS_FONT_BRIGHTNESS_BIAS,
                        15,
                    ) - 2
        for y in range(1, height + 1):
            for x in range(1, width + 1):
                if pixels[y, x] in (0, 14):
                    continue
                if pixels[y, x - 1] == 0:
                    pixels[y, x - 1] = 14
                if pixels[y, x + 1] == 0:
                    pixels[y, x + 1] = 14
                if pixels[y - 1, x] == 0:
                    pixels[y - 1, x] = 14
                if pixels[y + 1, x] == 0:
                    pixels[y + 1, x] = 14
        glyph_base = (
            slot * FRONTEND_STATS_FONT_TILES_PER_GLYPH * 32
        )
        for y in range(16):
            for x in range(16):
                colour = int(pixels[y, x])
                if colour:
                    write_4bpp(tiles, glyph_base, 2, x, y, colour)
        widths.append(width)

    cube = np.frombuffer(cube_pixels, dtype=np.uint8).reshape(22, 19)
    cube_base = FRONTEND_STATS_CUBE_TILE_OFFSET * 32
    for y in range(cube.shape[0]):
        for x in range(cube.shape[1]):
            pixel = int(cube[y, x])
            if pixel == 0xFF:
                continue
            colour = pixel & 0x0F
            write_4bpp(
                tiles,
                cube_base,
                4,
                x,
                y,
                colour if colour else 1,
            )

    metadata = {
        "FRONTEND_STATS_PREBAKED_GLYPH_COUNT":
            FRONTEND_STATS_FONT_GLYPH_COUNT,
        "FRONTEND_STATS_PREBAKED_WIDTH_BYTES": len(widths),
        "FRONTEND_STATS_PREBAKED_TILE_COUNT":
            FRONTEND_STATS_TILE_COUNT,
        "FRONTEND_STATS_PREBAKED_TILE_BYTES": len(tiles),
        "FRONTEND_STATS_PREBAKED_CUBE_TILE_OFFSET":
            FRONTEND_STATS_CUBE_TILE_OFFSET,
    }
    report = [
        "frontend_stats_tiles_source=stock TINY_FONT + data cube",
        (
            "frontend_stats_tiles_strategy="
            "build-time lossless RLE decode + outline + native 4bpp OBJ"
        ),
        f"frontend_stats_tiles_bytes={len(tiles)}",
        f"frontend_stats_width_bytes={len(widths)}",
        f"frontend_stats_tiles_crc32={zlib.crc32(tiles):08x}",
        f"frontend_stats_widths_crc32={zlib.crc32(widths):08x}",
        "frontend_stats_runtime_shp_decode=0",
    ]
    return bytes(tiles), bytes(widths), metadata, report


def build_frontend_source_stamp_assets(
    data_root: Path,
) -> tuple[bytes, bytes, dict[str, int], list[str]]:
    """Predecode the complete stock-derived static-menu art catalog.

    OpenTyrian's front end positions source SHP and Sprite2 art on a
    300x200 crop which the GBA presents at 240x160.  The old runtime path
    replayed RLE and divided once per opaque pixel every time a static menu
    was entered.  Here the immutable decode and scale work is moved to the
    build, while runtime still selects and layers graphics from the stock
    HDT item definitions.

    Five horizontal and five vertical source-coordinate phases are enough
    to reproduce floor(source * 4 / 5) for every possible placement.  Each
    phase is stored as word-aligned opaque scanline runs with raw palette
    indices.  This preserves runtime hue/brightness filters while avoiding
    roughly two MiB of transparent dense-rectangle padding.
    """

    source = FrontendSourceRenderer(data_root)
    offsets = bytearray()
    streams = bytearray()
    source_stream_crc = 0
    stamp_count = 0
    opaque_pixel_total = 0
    run_count_total = 0
    padded_pixel_total = 0
    max_stream_bytes = 0

    def encode_stamp(
        pixels: np.ndarray,
        transparent: int,
        phase_x: int,
        phase_y: int,
    ) -> bytes:
        # Row-major assignment intentionally matches the old per-pixel
        # renderer when several source pixels collapse onto one GBA pixel.
        mapped: dict[tuple[int, int], int] = {}
        source_y_values, source_x_values = np.where(pixels != transparent)
        base_x = phase_x * 4 // 5
        base_y = phase_y * 4 // 5

        for source_y, source_x in zip(
            source_y_values,
            source_x_values,
            strict=True,
        ):
            target_x = (
                (phase_x + int(source_x)) * 4 // 5 - base_x
            )
            target_y = (
                (phase_y + int(source_y)) * 4 // 5 - base_y
            )
            mapped[(target_y, target_x)] = int(
                pixels[source_y, source_x]
            )

        if not mapped:
            raise ValueError("front-end source stamp unexpectedly has no art")
        runs: list[tuple[int, int, bytes]] = []
        for target_y in sorted({position[0] for position in mapped}):
            columns = sorted(
                position[1]
                for position in mapped
                if position[0] == target_y
            )
            start = columns[0]
            prior = start
            pixels = bytearray([mapped[(target_y, start)]])
            for target_x in columns[1:]:
                if target_x != prior + 1:
                    runs.append((target_y, start, bytes(pixels)))
                    start = target_x
                    pixels = bytearray()
                pixels.append(mapped[(target_y, target_x)])
                prior = target_x
            runs.append((target_y, start, bytes(pixels)))

        if len(runs) > 0xFFFF:
            raise ValueError("frontend source stamp has too many runs")
        stream = bytearray(struct.pack("<HH", len(runs), 0))
        for target_y, target_x, pixels in runs:
            if (
                target_y > 0xFF or
                target_x > 0xFF or
                len(pixels) > 0xFFFF
            ):
                raise ValueError(
                    "frontend source stamp exceeds sparse-run bounds"
                )
            stream.extend(
                struct.pack("<BBH", target_y, target_x, len(pixels))
            )
            stream.extend(pixels)
            stream.extend(b"\0" * ((-len(pixels)) & 3))

        # Independent replay verifies that run packing preserves every
        # final scaled opaque pixel and never invents transparency.
        replay: dict[tuple[int, int], int] = {}
        cursor = 4
        for _ in range(len(runs)):
            target_y, target_x, length = struct.unpack_from(
                "<BBH",
                stream,
                cursor,
            )
            cursor += 4
            for pixel_index in range(length):
                replay[(target_y, target_x + pixel_index)] = stream[
                    cursor + pixel_index
                ]
            cursor += (length + 3) & ~3
        if replay != mapped or cursor != len(stream):
            raise ValueError(
                "frontend source stamp sparse round-trip changed pixels"
            )
        return bytes(stream)

    def append_phases(pixels: np.ndarray, transparent: int) -> None:
        nonlocal stamp_count
        nonlocal opaque_pixel_total
        nonlocal run_count_total
        nonlocal padded_pixel_total
        nonlocal max_stream_bytes

        for phase_y in range(FRONTEND_SOURCE_STAMP_SCALE_PHASES):
            for phase_x in range(FRONTEND_SOURCE_STAMP_SCALE_PHASES):
                stream = encode_stamp(
                    pixels,
                    transparent,
                    phase_x,
                    phase_y,
                )
                offsets.extend(struct.pack("<I", len(streams)))
                streams.extend(stream)
                stamp_count += 1
                run_count = struct.unpack_from("<H", stream, 0)[0]
                cursor = 4
                for _ in range(run_count):
                    length = struct.unpack_from(
                        "<H",
                        stream,
                        cursor + 2,
                    )[0]
                    opaque_pixel_total += length
                    padded_pixel_total += (length + 3) & ~3
                    cursor += 4 + ((length + 3) & ~3)
                if cursor != len(stream):
                    raise AssertionError(
                        "frontend source stamp run accounting changed"
                    )
                run_count_total += run_count
                max_stream_bytes = max(max_stream_bytes, len(stream))

    shp_key_count = 0
    for table, first_sprite, sprite_count in (
        FRONTEND_SOURCE_STAMP_SHP_RANGES
    ):
        for sprite_index in range(
            first_sprite,
            first_sprite + sprite_count,
        ):
            sprite = source.sprite(table, sprite_index)
            if sprite is None:
                raise ValueError(
                    "front-end SHP stamp source is empty: "
                    f"{table=}, {sprite_index=}"
                )
            append_phases(sprite, 0xFF)
            shp_key_count += 1

    tyrian_shp = (data_root / "tyrian.shp").read_bytes()
    comp_key_count = 0
    for shape_table in FRONTEND_SOURCE_STAMP_COMP_TABLES:
        bank = (
            sprite2_logical_bank(data_root, tyrian_shp, shape_table)
            if shape_table != 39 else
            (data_root / "newsh1.shp").read_bytes()
        )
        component_count = struct.unpack_from("<H", bank, 0)[0] // 2
        if component_count != SPRITE2_RAW_COMPONENTS_PER_TABLE:
            raise ValueError(
                "front-end Sprite2 bank component count changed: "
                f"{shape_table=}, {component_count=}"
            )
        components = [
            np.frombuffer(
                decode_sprite2_raw_component(
                    sprite2_component_stream(bank, component)
                ),
                dtype=np.uint8,
            ).reshape(
                SPRITE2_RAW_COMPONENT_HEIGHT,
                SPRITE2_RAW_COMPONENT_WIDTH,
            )
            for component in range(1, component_count + 1)
        ]
        source_stream_crc = zlib.crc32(bank, source_stream_crc)
        for graphic in range(
            1,
            FRONTEND_SOURCE_STAMP_COMP_GRAPHIC_COUNT + 1,
        ):
            frame = np.zeros((28, 24), dtype=np.uint8)
            for component, (origin_x, origin_y) in zip(
                (
                    graphic,
                    graphic + 1,
                    graphic + 19,
                    graphic + 20,
                ),
                ((0, 0), (12, 0), (0, 14), (12, 14)),
                strict=True,
            ):
                raw = components[component - 1]
                destination = frame[
                    origin_y : origin_y + 14,
                    origin_x : origin_x + 12,
                ]
                opaque = raw != 0
                destination[opaque] = raw[opaque]
            append_phases(frame, 0)
            comp_key_count += 1

    key_count = shp_key_count + comp_key_count
    expected_stamp_count = (
        key_count * FRONTEND_SOURCE_STAMP_PHASE_COUNT
    )
    if stamp_count != expected_stamp_count:
        raise AssertionError(
            "frontend source stamp catalog count changed: "
            f"{stamp_count} != {expected_stamp_count}"
        )
    if len(offsets) != stamp_count * 4:
        raise AssertionError("frontend source stamp offset packing changed")

    metadata = {
        "FRONTEND_SOURCE_STAMP_VERSION": 1,
        "FRONTEND_SOURCE_STAMP_SCALE_PHASES":
            FRONTEND_SOURCE_STAMP_SCALE_PHASES,
        "FRONTEND_SOURCE_STAMP_PHASE_COUNT":
            FRONTEND_SOURCE_STAMP_PHASE_COUNT,
        "FRONTEND_SOURCE_STAMP_SHP_KEY_COUNT": shp_key_count,
        "FRONTEND_SOURCE_STAMP_SHP_TABLE4_FIRST": 0,
        "FRONTEND_SOURCE_STAMP_SHP_TABLE4_COUNT": 12,
        "FRONTEND_SOURCE_STAMP_SHP_TABLE5_SIM_FIRST": 12,
        "FRONTEND_SOURCE_STAMP_SHP_TABLE5_SIM_COUNT": 3,
        "FRONTEND_SOURCE_STAMP_SHP_TABLE5_DISABLED_FIRST": 17,
        "FRONTEND_SOURCE_STAMP_SHP_TABLE5_DISABLED_COUNT": 1,
        "FRONTEND_SOURCE_STAMP_SHP_TABLE5_FIRST": 20,
        "FRONTEND_SOURCE_STAMP_SHP_TABLE5_COUNT": 15,
        "FRONTEND_SOURCE_STAMP_SHP_TABLE6_FIRST": 0,
        "FRONTEND_SOURCE_STAMP_SHP_TABLE6_COUNT": 22,
        "FRONTEND_SOURCE_STAMP_COMP_TABLE_FIRST":
            FRONTEND_SOURCE_STAMP_COMP_TABLES[0],
        "FRONTEND_SOURCE_STAMP_COMP_TABLE_COUNT":
            len(FRONTEND_SOURCE_STAMP_COMP_TABLES),
        "FRONTEND_SOURCE_STAMP_COMP_GRAPHIC_COUNT":
            FRONTEND_SOURCE_STAMP_COMP_GRAPHIC_COUNT,
        "FRONTEND_SOURCE_STAMP_COMP_KEY_COUNT": comp_key_count,
        "FRONTEND_SOURCE_STAMP_KEY_COUNT": key_count,
        "FRONTEND_SOURCE_STAMP_COUNT": stamp_count,
        "FRONTEND_SOURCE_STAMP_OFFSET_BYTES": len(offsets),
        "FRONTEND_SOURCE_STAMP_DATA_BYTES": len(streams),
        "FRONTEND_SOURCE_STAMP_MAX_STREAM_BYTES": max_stream_bytes,
    }
    report = [
        (
            "frontend_source_stamp_source="
            "stock tyrian.shp tables4/5/6 + complete options/shop Sprite2"
        ),
        (
            "frontend_source_stamp_strategy="
            "build-time lossless decode + 25 scale phases + aligned sparse runs"
        ),
        f"frontend_source_stamp_shp_keys={shp_key_count}",
        f"frontend_source_stamp_comp_keys={comp_key_count}",
        f"frontend_source_stamp_count={stamp_count}",
        f"frontend_source_stamp_opaque_pixels={opaque_pixel_total}",
        f"frontend_source_stamp_run_count={run_count_total}",
        f"frontend_source_stamp_padded_pixels={padded_pixel_total}",
        f"frontend_source_stamp_offset_bytes={len(offsets)}",
        f"frontend_source_stamp_data_bytes={len(streams)}",
        f"frontend_source_stamp_max_stream_bytes={max_stream_bytes}",
        (
            "frontend_source_stamp_offsets_crc32="
            f"{zlib.crc32(offsets):08x}"
        ),
        (
            "frontend_source_stamp_data_crc32="
            f"{zlib.crc32(streams):08x}"
        ),
        (
            "frontend_source_stamp_comp_source_crc32="
            f"{source_stream_crc:08x}"
        ),
        "frontend_source_stamp_runtime_rle_decode=0",
        "frontend_source_stamp_runtime_coordinate_division=2_per_stamp",
    ]
    return bytes(offsets), bytes(streams), metadata, report


def build_frontend_nav_obj_assets(
    source: FrontendSourceRenderer,
    preview: Path,
) -> tuple[
    bytes,
    bytes,
    bytes,
    bytes,
    bytes,
    bytes,
    bytes,
    dict[str, int],
    list[str],
]:
    """Pre-scale the source navigation sprites for bitmap-mode OBJ VRAM.

    Mode 4 leaves 16 KiB at 0x06014000 for OBJ characters.  Planet animation
    used to decode SHP sprites and redraw the complete 126x138 navigation
    rectangle every four display frames.  This atlas keeps original palette
    indices, including the source dark shadow, so runtime only streams the
    current OBJ characters and updates OAM.

    The 300->240 and 200->160 menu transform is exactly 4/5.  A sprite's
    rasterization can differ by one output pixel depending on source_x/y
    modulo five, so all 25 position phases are generated instead of accepting
    animation shimmer.
    """

    dot_catalog_base = FRONTEND_NAV_OBJ_PLANET_CATALOG_COUNT
    catalog_count = dot_catalog_base + 2
    metadata = bytearray(
        catalog_count *
        FRONTEND_NAV_OBJ_PHASE_COUNT *
        FRONTEND_NAV_OBJ_META_BYTES
    )
    tiles = bytearray()
    planet_indices: set[int] = set()
    for graphic, animated in zip(
        FRONTEND_NAV_PLANET_GRAPHICS,
        FRONTEND_NAV_PLANET_ANIMATED,
        strict=True,
    ):
        base = graphic - 1
        planet_indices.update(
            range(base, base + (15 if animated else 1))
        )
    if max(planet_indices) >= FRONTEND_NAV_OBJ_PLANET_CATALOG_COUNT:
        raise ValueError("navigation planet catalog exceeds generated metadata")

    sources: dict[int, tuple[np.ndarray, bool, str]] = {}
    for sprite_index in sorted(planet_indices):
        sprite = source.sprite(3, sprite_index)
        if sprite is None:
            raise ValueError(
                f"navigation planet SHP sprite is empty: {sprite_index}"
            )
        sources[sprite_index] = (
            sprite,
            True,
            f"planet_table3_{sprite_index}",
        )
    for dot_offset, sprite_index in enumerate((29, 30)):
        sprite = source.sprite(5, sprite_index)
        if sprite is None:
            raise ValueError(
                f"navigation route-dot SHP sprite is empty: {sprite_index}"
            )
        sources[dot_catalog_base + dot_offset] = (
            sprite,
            False,
            f"route_dot_table5_{sprite_index}",
        )

    def mapped(phase: int, coordinate: int) -> int:
        return (
            (phase + coordinate) * 4 // 5 -
            phase * 4 // 5
        )

    def axis_chunks(extent: int) -> list[int]:
        chunks: list[int] = []
        remaining = extent
        while remaining > 0:
            if remaining > 24:
                size = 32
            elif remaining > 8:
                size = 16
            else:
                size = 8
            chunks.append(size)
            remaining -= size
        return chunks

    def compose(
        sprite: np.ndarray,
        shadow: bool,
        phase_x: int,
        phase_y: int,
    ) -> np.ndarray:
        source_height, source_width = sprite.shape
        extra = 3 if shadow else 0
        width = mapped(phase_x, source_width - 1 + extra) + 1
        height = mapped(phase_y, source_height - 1 + extra) + 1
        canvas = np.zeros((height, width), dtype=np.uint8)
        opaque_y, opaque_x = np.where(sprite != 0xFF)

        if shadow:
            for y, x in zip(opaque_y, opaque_x, strict=True):
                pixel = int(sprite[y, x])
                dark = (pixel & 0xF0) | max(0, (pixel & 0x0F) - 4)
                canvas[
                    mapped(phase_y, int(y) + 3),
                    mapped(phase_x, int(x) + 3),
                ] = dark + 1
        for y, x in zip(opaque_y, opaque_x, strict=True):
            pixel = int(sprite[y, x])
            canvas[
                mapped(phase_y, int(y)),
                mapped(phase_x, int(x)),
            ] = pixel + 1
        return canvas

    def pack_chunks(canvas: np.ndarray) -> bytes:
        chunk_data = bytearray()
        y = 0
        for chunk_height in axis_chunks(canvas.shape[0]):
            x = 0
            for chunk_width in axis_chunks(canvas.shape[1]):
                padded = np.zeros(
                    (chunk_height, chunk_width),
                    dtype=np.uint8,
                )
                height = min(chunk_height, canvas.shape[0] - y)
                width = min(chunk_width, canvas.shape[1] - x)
                padded[:height, :width] = canvas[
                    y : y + height,
                    x : x + width,
                ]
                for tile_y in range(chunk_height // 8):
                    for tile_x in range(chunk_width // 8):
                        chunk_data.extend(
                            padded[
                                tile_y * 8 : tile_y * 8 + 8,
                                tile_x * 8 : tile_x * 8 + 8,
                            ].tobytes()
                        )
                x += chunk_width
            y += chunk_height
        return bytes(chunk_data)

    preview_dir = preview / "frontend_nav_obj"
    preview_dir.mkdir(parents=True, exist_ok=True)
    palette_rgb = np.minimum(
        source.palette_rgb_index(17).astype(np.uint16) * 4,
        255,
    ).astype(np.uint8)
    phase_tile_bytes: dict[tuple[int, int], int] = {}
    for catalog_index, (sprite, shadow, name) in sources.items():
        for phase_y in range(FRONTEND_NAV_OBJ_SCALE_PHASES):
            for phase_x in range(FRONTEND_NAV_OBJ_SCALE_PHASES):
                phase = (
                    phase_y * FRONTEND_NAV_OBJ_SCALE_PHASES +
                    phase_x
                )
                canvas = compose(sprite, shadow, phase_x, phase_y)
                packed = pack_chunks(canvas)
                if len(packed) % 64 != 0:
                    raise AssertionError(
                        "8bpp navigation OBJ stream lost character alignment"
                    )
                tile_offset = len(tiles)
                tiles.extend(packed)
                record_offset = (
                    (
                        catalog_index * FRONTEND_NAV_OBJ_PHASE_COUNT +
                        phase
                    ) *
                    FRONTEND_NAV_OBJ_META_BYTES
                )
                struct.pack_into(
                    "<IHBBBBH",
                    metadata,
                    record_offset,
                    tile_offset,
                    len(packed),
                    sprite.shape[1],
                    sprite.shape[0],
                    canvas.shape[1],
                    canvas.shape[0],
                    0,
                )
                phase_tile_bytes[(catalog_index, phase)] = len(packed)

                if phase == 0 and (
                    catalog_index in {
                        graphic - 1
                        for graphic in FRONTEND_NAV_PLANET_GRAPHICS
                    } or
                    catalog_index >= dot_catalog_base
                ):
                    rgba = np.zeros(
                        (canvas.shape[0], canvas.shape[1], 4),
                        dtype=np.uint8,
                    )
                    opaque = canvas != 0
                    rgba[opaque, :3] = palette_rgb[
                        canvas[opaque].astype(np.uint16) - 1
                    ]
                    rgba[opaque, 3] = 255
                    Image.fromarray(rgba, "RGBA").resize(
                        (
                            canvas.shape[1] * 4,
                            canvas.shape[0] * 4,
                        ),
                        Image.Resampling.NEAREST,
                    ).save(preview_dir / f"{name}.png")

    # OBJ index zero is transparent; shift all 255 usable source colours by
    # one while preserving their exact palette-17 RGB555 values.
    source_palette = np.frombuffer(
        source.palette_gba_index(17),
        dtype="<u2",
    )
    obj_palette = np.zeros(256, dtype="<u2")
    obj_palette[1:] = source_palette[:255]

    def planet_reserve(planet_index: int) -> int:
        graphic = FRONTEND_NAV_PLANET_GRAPHICS[planet_index] - 1
        frames = 15 if FRONTEND_NAV_PLANET_ANIMATED[planet_index] else 1
        return max(
            phase_tile_bytes[(graphic + frame, phase)]
            for frame in range(frames)
            for phase in range(FRONTEND_NAV_OBJ_PHASE_COUNT)
        )

    # The stock script has at most two destinations.  Runtime always draws
    # planets 1..11 plus at most the origin and two distinct >11 entries.
    fixed_bytes = sum(planet_reserve(index) for index in range(11))
    extra_reserves = sorted(
        (planet_reserve(index) for index in range(11, 21)),
        reverse=True,
    )
    worst_planet_bytes = fixed_bytes + sum(extra_reserves[:3])
    dot_bytes = FRONTEND_NAV_OBJ_DOT_DIM ** 2 * 2
    if worst_planet_bytes + dot_bytes > FRONTEND_NAV_OBJ_VRAM_BYTES:
        raise ValueError(
            "navigation OBJ worst case exceeds bitmap-mode OBJ VRAM: "
            f"{worst_planet_bytes + dot_bytes} > "
            f"{FRONTEND_NAV_OBJ_VRAM_BYTES}; "
            f"{fixed_bytes=}, {extra_reserves[:3]=}"
        )

    asset_metadata = {
        "FRONTEND_NAV_OBJ_CATALOG_COUNT": catalog_count,
        "FRONTEND_NAV_OBJ_PLANET_CATALOG_COUNT":
            FRONTEND_NAV_OBJ_PLANET_CATALOG_COUNT,
        "FRONTEND_NAV_OBJ_DOT_OFF_CATALOG": dot_catalog_base,
        "FRONTEND_NAV_OBJ_DOT_ON_CATALOG": dot_catalog_base + 1,
        "FRONTEND_NAV_OBJ_SCALE_PHASES":
            FRONTEND_NAV_OBJ_SCALE_PHASES,
        "FRONTEND_NAV_OBJ_PHASE_COUNT": FRONTEND_NAV_OBJ_PHASE_COUNT,
        "FRONTEND_NAV_OBJ_META_BYTES": FRONTEND_NAV_OBJ_META_BYTES,
        "FRONTEND_NAV_OBJ_TILE_BYTES": len(tiles),
        "FRONTEND_NAV_OBJ_PALETTE_BYTES": len(obj_palette.tobytes()),
        "FRONTEND_NAV_OBJ_DOT_BYTES": dot_bytes,
        "FRONTEND_NAV_OBJ_WORST_PLANET_BYTES": worst_planet_bytes,
        "FRONTEND_NAV_OBJ_VRAM_BYTES": FRONTEND_NAV_OBJ_VRAM_BYTES,
    }
    report = [
        "frontend_nav_animation=Mode4 BG2 + hardware OBJ/OAM",
        "frontend_nav_planet_source=tyrian.shp table3 palette17",
        "frontend_nav_route_dot_source=tyrian.shp table5 sprites29/30",
        "frontend_nav_obj_colour=source palette index shifted +1",
        (
            "frontend_nav_obj_scale_phases="
            f"{FRONTEND_NAV_OBJ_PHASE_COUNT}"
        ),
        f"frontend_nav_obj_catalog_entries={catalog_count}",
        f"frontend_nav_obj_populated_sprites={len(sources)}",
        f"frontend_nav_obj_tile_bytes={len(tiles)}",
        f"frontend_nav_obj_meta_bytes={len(metadata)}",
        f"frontend_nav_obj_palette_bytes={len(obj_palette.tobytes())}",
        f"frontend_nav_obj_worst_planet_bytes={worst_planet_bytes}",
        f"frontend_nav_obj_route_dot_bytes={dot_bytes}",
        f"frontend_nav_obj_vram_bytes={FRONTEND_NAV_OBJ_VRAM_BYTES}",
        (
            "frontend_nav_idle_bitmap_redraw="
            "0 (planet/dot animation updates OAM only)"
        ),
    ]
    return (
        bytes(tiles),
        bytes(metadata),
        obj_palette.tobytes(),
        asset_metadata,
        report,
    )


def build_frontend_nav_bitmap_pages(
    source: FrontendSourceRenderer,
    preview: Path,
) -> tuple[bytes, bytes, dict[str, int], list[str]]:
    """Bake every repeating grid phase plus the fixed OPTION_SHAPES frame.

    The source grid is spaced every 15 pixels.  Its camera term is
    ``nav_coordinate >> 1``, therefore the complete bitmap background has
    only 15x15 distinct rasters regardless of level/episode.  Baking this
    stock-derived global table removes the runtime SHP decode, 300->240
    coordinate divisions, grid plotting and chrome restore from the
    selection/camera hot path without introducing per-level resources.
    """

    chrome = source.menu_picture_frame(1)
    overlay = source.sprite(5, 28)
    if overlay is None:
        raise ValueError("OPTION_SHAPES navigation frame 28 is empty")

    overlay_frame = np.full(
        (FRONTEND_FRAME_HEIGHT, FRONTEND_FRAME_WIDTH),
        0xFF,
        dtype=np.uint8,
    )
    opaque_y, opaque_x = np.where(overlay != 0xFF)
    for source_y, source_x in zip(opaque_y, opaque_x, strict=True):
        if (
            source_x < FRONTEND_MENU_SOURCE_CROP_X or
            source_x >=
                FRONTEND_MENU_SOURCE_CROP_X + FRONTEND_MENU_SOURCE_WIDTH or
            source_y >= 200
        ):
            continue
        target_x = (
            (int(source_x) - FRONTEND_MENU_SOURCE_CROP_X) *
            FRONTEND_FRAME_WIDTH //
            FRONTEND_MENU_SOURCE_WIDTH
        )
        target_y = int(source_y) * FRONTEND_FRAME_HEIGHT // 200
        overlay_frame[target_y, target_x] = overlay[source_y, source_x]

    def screen_x(source_x: int) -> int:
        return (
            (source_x - FRONTEND_MENU_SOURCE_CROP_X) *
            FRONTEND_FRAME_WIDTH //
            FRONTEND_MENU_SOURCE_WIDTH
        )

    def screen_y(source_y: int) -> int:
        return source_y * FRONTEND_FRAME_HEIGHT // 200

    inner_x0 = screen_x(19)
    inner_x1 = screen_x(136)
    inner_y0 = screen_y(16)
    inner_y1 = screen_y(170)
    wide_x1 = screen_x(161)
    pages = bytearray()
    representative: np.ndarray | None = None

    for phase_y in range(FRONTEND_NAV_GRID_PHASES):
        for phase_x in range(FRONTEND_NAV_GRID_PHASES):
            frame = chrome.copy()
            frame[inner_y0:inner_y1, inner_x0:inner_x1] = 2
            for index in range(1, 21):
                x = index * 15 - phase_x
                if 18 < x < 135:
                    target_x = screen_x(x)
                    frame[inner_y0:inner_y1, target_x + 1] = 1
                    frame[inner_y0:inner_y1, target_x] = 5
            for index in range(1, 21):
                y = index * 15 - phase_y
                if 15 < y < 169:
                    target_y = screen_y(y)
                    frame[target_y + 1, inner_x0:inner_x1] = 1
                    frame[target_y, 0:wide_x1] = 5
                    for x_index in range(1, 21):
                        x = x_index * 15 - phase_x
                        if 18 < x < 135:
                            frame[target_y, screen_x(x)] = 7
            overlay_opaque = overlay_frame != 0xFF
            frame[overlay_opaque] = overlay_frame[overlay_opaque]
            page = frame[
                :FRONTEND_NAV_BITMAP_HEIGHT,
                :FRONTEND_NAV_BITMAP_WIDTH,
            ]
            padded_page = np.zeros(
                (
                    FRONTEND_NAV_BITMAP_HEIGHT,
                    FRONTEND_NAV_BITMAP_STRIDE,
                ),
                dtype=np.uint8,
            )
            padded_page[:, :FRONTEND_NAV_BITMAP_WIDTH] = page
            pages.extend(padded_page.tobytes())
            if phase_x == 5 and phase_y == 5:
                representative = page.copy()

    page_count = FRONTEND_NAV_GRID_PHASES ** 2
    expected_bytes = page_count * FRONTEND_NAV_BITMAP_PAGE_BYTES
    if len(pages) != expected_bytes:
        raise AssertionError(
            "navigation bitmap phase table packing changed: "
            f"{len(pages)} != {expected_bytes}"
        )
    if representative is not None:
        palette = np.minimum(
            source.palette_rgb_index(17).astype(np.uint16) * 4,
            255,
        ).astype(np.uint8)
        Image.fromarray(palette[representative], "RGB").save(
            preview / "frontend_nav_bitmap_phase_05_05.png"
        )

    if (
        FRONTEND_NAV_BITMAP_HEIGHT %
        FRONTEND_NAV_BITMAP_BLOCK_ROWS
    ):
        raise AssertionError(
            "navigation bitmap height must be an exact block multiple"
        )
    blocks_per_page = (
        FRONTEND_NAV_BITMAP_HEIGHT //
        FRONTEND_NAV_BITMAP_BLOCK_ROWS
    )
    block_catalog: dict[bytes, int] = {}
    block_data = bytearray()
    block_indices = bytearray()
    for page in range(page_count):
        page_start = page * FRONTEND_NAV_BITMAP_PAGE_BYTES
        for block in range(blocks_per_page):
            block_start = (
                page_start +
                block * FRONTEND_NAV_BITMAP_BLOCK_BYTES
            )
            payload = bytes(
                pages[
                    block_start:
                    block_start + FRONTEND_NAV_BITMAP_BLOCK_BYTES
                ]
            )
            block_id = block_catalog.get(payload)
            if block_id is None:
                block_id = len(block_catalog)
                if block_id > 0xFFFF:
                    raise ValueError(
                        "navigation bitmap block catalog exceeds u16"
                    )
                block_catalog[payload] = block_id
                block_data.extend(payload)
            block_indices.extend(struct.pack("<H", block_id))
    packed_bytes = len(block_data) + len(block_indices)

    # Reconstruct every source page from the serialized dictionary and index
    # stream.  This is deliberately independent of the catalog dictionary so
    # a future packing change cannot silently alter even one menu pixel.
    roundtrip_cursor = 0
    for index_offset in range(0, len(block_indices), 2):
        block_id = struct.unpack_from("<H", block_indices, index_offset)[0]
        block_start = block_id * FRONTEND_NAV_BITMAP_BLOCK_BYTES
        block_end = block_start + FRONTEND_NAV_BITMAP_BLOCK_BYTES
        source_end = roundtrip_cursor + FRONTEND_NAV_BITMAP_BLOCK_BYTES
        if (
            block_end > len(block_data) or
            block_data[block_start:block_end] !=
                pages[roundtrip_cursor:source_end]
        ):
            raise ValueError(
                "navigation bitmap block dictionary round-trip changed "
                f"source block {index_offset // 2}"
            )
        roundtrip_cursor = source_end
    if roundtrip_cursor != len(pages):
        raise AssertionError(
            "navigation bitmap block dictionary round-trip length changed"
        )

    metadata = {
        "FRONTEND_NAV_BITMAP_WIDTH": FRONTEND_NAV_BITMAP_WIDTH,
        "FRONTEND_NAV_BITMAP_HEIGHT": FRONTEND_NAV_BITMAP_HEIGHT,
        "FRONTEND_NAV_BITMAP_STRIDE": FRONTEND_NAV_BITMAP_STRIDE,
        "FRONTEND_NAV_BITMAP_BLOCK_ROWS":
            FRONTEND_NAV_BITMAP_BLOCK_ROWS,
        "FRONTEND_NAV_BITMAP_BLOCK_BYTES":
            FRONTEND_NAV_BITMAP_BLOCK_BYTES,
        "FRONTEND_NAV_BITMAP_BLOCKS_PER_PAGE": blocks_per_page,
        "FRONTEND_NAV_BITMAP_BLOCK_COUNT": len(block_catalog),
        "FRONTEND_NAV_BITMAP_BLOCK_DATA_BYTES": len(block_data),
        "FRONTEND_NAV_BITMAP_INDEX_BYTES": len(block_indices),
        "FRONTEND_NAV_GRID_PHASES": FRONTEND_NAV_GRID_PHASES,
        "FRONTEND_NAV_BITMAP_PAGE_COUNT": page_count,
        "FRONTEND_NAV_BITMAP_PAGE_BYTES": FRONTEND_NAV_BITMAP_PAGE_BYTES,
        "FRONTEND_NAV_BITMAP_RAW_BYTES": len(pages),
        "FRONTEND_NAV_BITMAP_PACKED_BYTES": packed_bytes,
    }
    report = [
        "frontend_nav_bitmap_source=stock PIC1 + SHP table5 sprite28",
        (
            "frontend_nav_bitmap_strategy="
            "build-time 15x15 phases + lossless 2-row block dictionary"
        ),
        f"frontend_nav_bitmap_page_count={page_count}",
        (
            "frontend_nav_bitmap_page_dimensions="
            f"{FRONTEND_NAV_BITMAP_WIDTH}x{FRONTEND_NAV_BITMAP_HEIGHT},"
            f"stride={FRONTEND_NAV_BITMAP_STRIDE}"
        ),
        f"frontend_nav_bitmap_page_bytes={FRONTEND_NAV_BITMAP_PAGE_BYTES}",
        f"frontend_nav_bitmap_raw_bytes={len(pages)}",
        f"frontend_nav_bitmap_block_rows={FRONTEND_NAV_BITMAP_BLOCK_ROWS}",
        f"frontend_nav_bitmap_block_count={len(block_catalog)}",
        f"frontend_nav_bitmap_block_data_bytes={len(block_data)}",
        f"frontend_nav_bitmap_index_bytes={len(block_indices)}",
        f"frontend_nav_bitmap_packed_bytes={packed_bytes}",
        f"frontend_nav_bitmap_saved_bytes={len(pages) - packed_bytes}",
        "frontend_nav_bitmap_roundtrip_verified=1",
        f"frontend_nav_bitmap_raw_crc32={zlib.crc32(pages):08x}",
        (
            "frontend_nav_bitmap_block_data_crc32="
            f"{zlib.crc32(block_data):08x}"
        ),
        (
            "frontend_nav_bitmap_index_crc32="
            f"{zlib.crc32(block_indices):08x}"
        ),
        "frontend_nav_camera_runtime_shp_decode=0",
        "frontend_nav_camera_runtime_grid_plot=0",
    ]
    return bytes(block_data), bytes(block_indices), metadata, report


def build_frontend_static_menu_panels(
    source: FrontendSourceRenderer,
    native_font: np.ndarray,
    pregame_font: np.ndarray,
    layout: dict[str, int],
    preview: Path,
) -> tuple[
    bytes,
    bytes,
    bytes,
    bytes,
    bytes,
    bytes,
    bytes,
    dict[str, int],
    list[str],
]:
    """Bake final-resolution text panels shared by the static menu family."""
    font_index = {
        character: index
        for index, character in enumerate(FRONTEND_NATIVE_FONT_CHARACTERS)
    }
    fallback_index = font_index["?"]
    pregame_font_index = {
        character: index
        for index, character in enumerate(FRONTEND_PREGAME_FONT_CHARACTERS)
    }
    pregame_fallback_index = pregame_font_index["?"]
    menu_chrome = source.menu_picture_frame(1)
    panels: list[np.ndarray] = []
    names: list[str] = []
    pre_game_frames: list[np.ndarray] = []
    pre_game_names: list[str] = []
    preview_dir = preview / "frontend_static_menu_panels"
    preview_dir.mkdir(parents=True, exist_ok=True)

    def highlight_colour(colour: int) -> int:
        """Mirror OpenTyrian's '~' +4 brightness with saturation."""
        return (colour & 0xF0) | min(15, (colour & 0x0F) + 4)

    def glyph_index(character: str) -> int | None:
        if character == " ":
            return None
        return font_index.get(character.upper(), fallback_index)

    def glyph_width(index: int) -> int:
        occupied = 0
        for value in native_font[index]:
            occupied |= int(value)
        return max(1, occupied.bit_length())

    def glyph_advance(
        index: int | None,
        maximum_width: int = FRONTEND_NATIVE_FONT_WIDTH,
        space_width: int = FRONTEND_NATIVE_FONT_SPACE,
    ) -> int:
        if index is None:
            return space_width
        advance = glyph_width(index)
        if advance < maximum_width:
            advance += 1
        return min(maximum_width, advance)

    def text_width(
        text: str,
        maximum_width: int = FRONTEND_NATIVE_FONT_WIDTH,
        space_width: int = FRONTEND_NATIVE_FONT_SPACE,
    ) -> int:
        return sum(
            glyph_advance(
                glyph_index(character),
                maximum_width,
                space_width,
            )
            for character in text
            if character != "~"
        )

    def draw_glyph(
        frame: np.ndarray,
        index: int | None,
        x: int,
        y: int,
        colour: int,
    ) -> None:
        if index is None:
            return
        natural_width = glyph_width(index)
        for row, value in enumerate(native_font[index]):
            target_y = y + row
            if target_y < 0 or target_y >= FRONTEND_FRAME_HEIGHT:
                continue
            for column in range(natural_width):
                target_x = x + column
                if (
                    0 <= target_x < FRONTEND_FRAME_WIDTH and
                    int(value) & (1 << (natural_width - column - 1))
                ):
                    frame[target_y, target_x] = colour

    def draw_text(
        frame: np.ndarray,
        text: str,
        x: int,
        y: int,
        right: int,
        colour: int,
        maximum_width: int = FRONTEND_NATIVE_FONT_WIDTH,
        space_width: int = FRONTEND_NATIVE_FONT_SPACE,
    ) -> None:
        highlight = False
        for character in text:
            if character == "~":
                highlight = not highlight
                continue
            index = glyph_index(character)
            advance = glyph_advance(index, maximum_width, space_width)
            if x >= right or x + advance > right:
                break
            draw_glyph(
                frame,
                index,
                x + 1,
                y + 1,
                FRONTEND_NATIVE_FONT_SHADOW,
            )
            draw_glyph(
                frame,
                index,
                x,
                y,
                highlight_colour(colour) if highlight else colour,
            )
            x += advance

    def draw_centered(
        frame: np.ndarray,
        text: str,
        center_x: int,
        y: int,
        colour: int,
        maximum_width: int = FRONTEND_NATIVE_FONT_WIDTH,
        space_width: int = FRONTEND_NATIVE_FONT_SPACE,
    ) -> None:
        draw_text(
            frame,
            text,
            center_x -
                text_width(text, maximum_width, space_width) // 2,
            y,
            FRONTEND_FRAME_WIDTH,
            colour,
            maximum_width,
            space_width,
        )

    def pregame_glyph_index(character: str) -> int | None:
        if character == " ":
            return None
        return pregame_font_index.get(character, pregame_fallback_index)

    def pregame_row(index: int, row: int) -> int:
        """Return the authored mixed-case row without synthetic thickening."""
        return int(pregame_font[index, row])

    def pregame_glyph_width(index: int) -> int:
        occupied = 0
        for row in range(FRONTEND_PREGAME_FONT_HEIGHT):
            occupied |= pregame_row(index, row)
        return max(1, occupied.bit_length())

    def pregame_glyph_advance(index: int | None) -> int:
        if index is None:
            return FRONTEND_PREGAME_FONT_SPACE
        return min(
            FRONTEND_PREGAME_FONT_WIDTH,
            pregame_glyph_width(index) + 2,
        )

    def pregame_text_width(text: str) -> int:
        return sum(
            pregame_glyph_advance(pregame_glyph_index(character))
            for character in text
            if character != "~"
        )

    def draw_pregame_glyph(
        frame: np.ndarray,
        index: int | None,
        x: int,
        y: int,
        colour: int,
    ) -> None:
        if index is None:
            return
        natural_width = pregame_glyph_width(index)
        for row in range(FRONTEND_PREGAME_FONT_HEIGHT):
            value = pregame_row(index, row)
            target_y = y + row
            if target_y < 0 or target_y >= FRONTEND_FRAME_HEIGHT:
                continue
            for column in range(natural_width):
                target_x = x + column
                if (
                    0 <= target_x < FRONTEND_FRAME_WIDTH and
                    value & (1 << (natural_width - column - 1))
                ):
                    frame[target_y, target_x] = colour

    def draw_pregame_text(
        frame: np.ndarray,
        text: str,
        x: int,
        y: int,
        right: int,
        colour: int,
    ) -> None:
        highlight = False
        for character in text:
            if character == "~":
                highlight = not highlight
                continue
            index = pregame_glyph_index(character)
            advance = pregame_glyph_advance(index)
            if x >= right or x + advance > right:
                break
            draw_pregame_glyph(
                frame,
                index,
                x + 1,
                y + 1,
                FRONTEND_PREGAME_FONT_SHADOW,
            )
            draw_pregame_glyph(
                frame,
                index,
                x,
                y,
                highlight_colour(colour) if highlight else colour,
            )
            x += advance

    def draw_pregame_centered(
        frame: np.ndarray,
        text: str,
        center_x: int,
        y: int,
        colour: int,
    ) -> None:
        draw_pregame_text(
            frame,
            text,
            center_x - pregame_text_width(text) // 2,
            y,
            FRONTEND_FRAME_WIDTH,
            colour,
        )

    def menu_glyph_advance(index: int | None) -> int:
        if index is None:
            return FRONTEND_MENU_FONT_SPACE
        return min(
            FRONTEND_MENU_FONT_WIDTH,
            pregame_glyph_width(index) + 1,
        )

    def menu_text_width(text: str) -> int:
        return sum(
            menu_glyph_advance(pregame_glyph_index(character))
            for character in text
            if character != "~"
        )

    def draw_menu_text(
        frame: np.ndarray,
        text: str,
        x: int,
        y: int,
        right: int,
        colour: int,
    ) -> None:
        highlight = False
        for character in text:
            if character == "~":
                highlight = not highlight
                continue
            index = pregame_glyph_index(character)
            advance = menu_glyph_advance(index)
            if x >= right or x + advance > right:
                break
            draw_pregame_glyph(
                frame,
                index,
                x + 1,
                y + 1,
                FRONTEND_PREGAME_FONT_SHADOW,
            )
            draw_pregame_glyph(
                frame,
                index,
                x,
                y,
                highlight_colour(colour) if highlight else colour,
            )
            x += advance

    def draw_menu_centered(
        frame: np.ndarray,
        text: str,
        center_x: int,
        y: int,
        colour: int,
    ) -> None:
        draw_menu_text(
            frame,
            text,
            center_x - menu_text_width(text) // 2,
            y,
            FRONTEND_FRAME_WIDTH,
            colour,
        )

    def small_mixed_glyph_width(index: int) -> int:
        return min(
            FRONTEND_SMALL_MIXED_FONT_WIDTH,
            pregame_glyph_width(index),
        )

    def small_mixed_glyph_advance(index: int | None) -> int:
        if index is None:
            return FRONTEND_SMALL_MIXED_FONT_SPACE
        return small_mixed_glyph_width(index) + 1

    def small_mixed_text_width(text: str) -> int:
        return sum(
            small_mixed_glyph_advance(
                pregame_glyph_index(character)
            )
            for character in text
            if character != "~"
        )

    def draw_small_mixed_glyph(
        frame: np.ndarray,
        index: int | None,
        x: int,
        y: int,
        colour: int,
    ) -> None:
        if index is None:
            return
        source_width = pregame_glyph_width(index)
        target_width = small_mixed_glyph_width(index)
        for row in range(FRONTEND_PREGAME_FONT_HEIGHT):
            value = pregame_row(index, row)
            target_y = y + row
            if target_y < 0 or target_y >= FRONTEND_FRAME_HEIGHT:
                continue
            for column in range(target_width):
                source_column = (
                    0 if target_width == 1 else
                    column * (source_width - 1) //
                        (target_width - 1)
                )
                target_x = x + column
                if (
                    0 <= target_x < FRONTEND_FRAME_WIDTH and
                    value &
                        (1 << (source_width - source_column - 1))
                ):
                    frame[target_y, target_x] = colour

    def draw_small_mixed_text(
        frame: np.ndarray,
        text: str,
        x: int,
        y: int,
        right: int,
        colour: int,
        shadow_colour: int = FRONTEND_PREGAME_FONT_SHADOW,
    ) -> None:
        highlight = False
        for character in text:
            if character == "~":
                highlight = not highlight
                continue
            index = pregame_glyph_index(character)
            advance = small_mixed_glyph_advance(index)
            if x >= right or x + advance > right:
                break
            draw_small_mixed_glyph(
                frame,
                index,
                x + 1,
                y + 1,
                shadow_colour,
            )
            draw_small_mixed_glyph(
                frame,
                index,
                x,
                y,
                highlight_colour(colour) if highlight else colour,
            )
            x += advance

    def draw_small_mixed_wrapped(
        frame: np.ndarray,
        text: str,
        x: int,
        y: int,
        right: int,
        line_height: int,
        max_lines: int,
        colour: int,
    ) -> None:
        words = text.split()
        line = ""
        line_index = 0
        for word in words:
            candidate = f"{line} {word}" if line else word
            if (
                line and
                small_mixed_text_width(candidate) > right - x
            ):
                draw_small_mixed_text(
                    frame,
                    line,
                    x,
                    y + line_index * line_height,
                    right,
                    colour,
                )
                line_index += 1
                if line_index >= max_lines:
                    return
                line = word
            else:
                line = candidate
        if line and line_index < max_lines:
            draw_small_mixed_text(
                frame,
                line,
                x,
                y + line_index * line_height,
                right,
                colour,
            )

    def draw_wrapped(
        frame: np.ndarray,
        text: str,
        x: int,
        y: int,
        right: int,
        line_height: int,
        max_lines: int,
        colour: int,
        maximum_width: int,
        space_width: int,
    ) -> None:
        line = ""
        line_index = 0
        for word in text.split():
            candidate = f"{line} {word}" if line else word
            if (
                line and
                text_width(
                    candidate,
                    maximum_width,
                    space_width,
                ) > right - x
            ):
                draw_text(
                    frame,
                    line,
                    x,
                    y + line_index * line_height,
                    right,
                    colour,
                    maximum_width,
                    space_width,
                )
                line_index += 1
                if line_index >= max_lines:
                    return
                line = word
            else:
                line = candidate
        if line and line_index < max_lines:
            draw_text(
                frame,
                line,
                x,
                y + line_index * line_height,
                right,
                colour,
                maximum_width,
                space_width,
            )

    def add(name: str, frame: np.ndarray) -> None:
        panel = frame[
            FRONTEND_STATIC_MENU_PANEL_Y:
                FRONTEND_STATIC_MENU_PANEL_Y +
                FRONTEND_STATIC_MENU_PANEL_HEIGHT,
            FRONTEND_STATIC_MENU_PANEL_X:
                FRONTEND_STATIC_MENU_PANEL_X +
                FRONTEND_STATIC_MENU_PANEL_WIDTH,
        ].copy()
        if panel.size != FRONTEND_STATIC_MENU_PANEL_BYTES:
            raise AssertionError("static menu panel dimensions changed")
        panels.append(panel)
        names.append(name)

        rgb = np.minimum(
            source.palette_rgb_index(0).astype(np.uint16) * 4,
            255,
        ).astype(np.uint8)
        Image.fromarray(rgb[frame], "RGB").save(
            preview_dir / f"{len(panels) - 1:02d}_{name}.png"
        )

    def add_pre_game(
        name: str,
        frame: np.ndarray,
        palette_index: int,
    ) -> None:
        pre_game_frames.append(frame.copy())
        pre_game_names.append(name)
        rgb = np.minimum(
            source.palette_rgb_index(palette_index).astype(np.uint16) * 4,
            255,
        ).astype(np.uint8)
        Image.fromarray(rgb[frame], "RGB").save(
            preview_dir /
            f"pre_game_{len(pre_game_frames) - 1:02d}_{name}.png"
        )

    title_menu = source.text["title_menu"]
    title_fallback = (
        "Start New Game",
        "Load Game",
        "Demo",
        "Jukebox",
    )
    for selection in range(4):
        frame = source.picture_frame(4)
        source.draw_logo(frame)
        labels = (
            title_menu[0] or title_fallback[0],
            title_menu[1] or title_fallback[1],
            title_menu[5] or title_fallback[2],
            title_fallback[3],
        )
        for index, label in enumerate(labels):
            colour = 0xfe if index == selection else 0xfa
            draw_pregame_centered(
                frame,
                label,
                layout["TYRIAN_GBA_LAYOUT_TITLE_MENU_CENTER_X"],
                layout["TYRIAN_GBA_LAYOUT_TITLE_MENU_FIRST_Y"] +
                    index *
                    layout["TYRIAN_GBA_LAYOUT_TITLE_MENU_ROW_STEP"],
                colour,
            )
        add_pre_game(f"title_{selection}", frame, 8)

    for selection in range(2):
        frame = source.picture_frame(2)
        draw_pregame_centered(
            frame,
            "Play Mode",
            layout["TYRIAN_GBA_LAYOUT_SETUP_HEADER_CENTER_X"],
            layout["TYRIAN_GBA_LAYOUT_SETUP_HEADER_Y"],
            0xfb,
        )
        for index, label in enumerate(("Full Game", "Arcade")):
            draw_pregame_centered(
                frame,
                label,
                layout["TYRIAN_GBA_LAYOUT_SETUP_CHOICE_CENTER_X"],
                layout["TYRIAN_GBA_LAYOUT_SETUP_CHOICE_FIRST_Y"] +
                    index *
                    layout["TYRIAN_GBA_LAYOUT_SETUP_CHOICE_ROW_STEP"],
                0xfe if index == selection else 0xfa,
            )
        add_pre_game(f"play_mode_{selection}", frame, 7)

    episode_names = source.text["episode_name"]
    episode_fallback = (
        "Select an Episode",
        "Episode 1: Escape",
        "Episode 2: Treachery",
        "Episode 3: Mission: Suicide",
        "Episode 4: An End to Fate",
    )
    for selection in range(4):
        frame = source.picture_frame(2)
        draw_pregame_centered(
            frame,
            episode_names[0] or episode_fallback[0],
            layout["TYRIAN_GBA_LAYOUT_SETUP_HEADER_CENTER_X"],
            layout["TYRIAN_GBA_LAYOUT_SETUP_HEADER_Y"],
            0xfb,
        )
        for index in range(4):
            draw_pregame_text(
                frame,
                episode_names[index + 1] or episode_fallback[index + 1],
                layout["TYRIAN_GBA_LAYOUT_EPISODE_CHOICE_X"],
                layout["TYRIAN_GBA_LAYOUT_EPISODE_CHOICE_FIRST_Y"] +
                    index *
                    layout["TYRIAN_GBA_LAYOUT_EPISODE_CHOICE_ROW_STEP"],
                layout["TYRIAN_GBA_LAYOUT_EPISODE_CHOICE_RIGHT"],
                0xfe if index == selection else 0xfa,
            )
        add_pre_game(f"episode_{selection}", frame, 7)

    difficulty_names = source.text["difficulty_name"]
    difficulty_fallback = (
        "Difficulty Level",
        "Easy",
        "Normal",
        "Hard",
    )
    for selection in range(3):
        frame = source.picture_frame(2)
        draw_pregame_centered(
            frame,
            difficulty_names[0] or difficulty_fallback[0],
            layout["TYRIAN_GBA_LAYOUT_SETUP_HEADER_CENTER_X"],
            layout["TYRIAN_GBA_LAYOUT_SETUP_HEADER_Y"],
            0xfb,
        )
        for index in range(3):
            draw_pregame_centered(
                frame,
                difficulty_names[index + 1] or
                    difficulty_fallback[index + 1],
                layout["TYRIAN_GBA_LAYOUT_SETUP_CHOICE_CENTER_X"],
                layout["TYRIAN_GBA_LAYOUT_SETUP_CHOICE_FIRST_Y"] +
                    index *
                    layout["TYRIAN_GBA_LAYOUT_SETUP_CHOICE_ROW_STEP"],
                0xfe if index == selection else 0xfa,
            )
        add_pre_game(f"difficulty_{selection}", frame, 7)

    # mainint.c:JE_loadScreen() is the title-screen Load entry.  It owns a
    # full-width PIC 2 page and is not game_menu.c:MENU_LOAD_SAVE.  Keep the
    # one-player source header and GBA controls here; all twelve rows remain
    # dynamic so a clean background is always restored before recolouring.
    save_background = source.picture_frame(2)
    frame = save_background.copy()
    # mainint.c uses FONT_LARGE at source (160, 5).  Render that exact SHP
    # face before runtime rather than substituting the compact GBA menu font.
    source.draw_text(
        frame,
        source.text["misc_text"][38] or "One Player Saved Games",
        160,
        5,
        0,
        "center",
        15,
        -3,
        2,
    )
    draw_small_mixed_text(
        frame,
        "A: Load   B: Back",
        layout["TYRIAN_GBA_LAYOUT_SAVE_FOOTER_X"],
        layout["TYRIAN_GBA_LAYOUT_SAVE_FOOTER_Y"],
        layout["TYRIAN_GBA_LAYOUT_SAVE_SLOT_RIGHT"],
        0xea,
        0xe2,
    )
    add_pre_game("title_load_slots_base", frame, 7)

    full_game_menu = source.text["full_game_menu"]
    game_fallback = (
        "Game Menu",
        "Data",
        "Ship Specs",
        "Upgrade Ship",
        "Options",
        "Play Next Level",
        "Quit Game",
    )
    for selection in range(FRONTEND_STATIC_GAME_MENU_COUNT):
        frame = menu_chrome.copy()
        title = full_game_menu[0] or game_fallback[0]

        draw_menu_centered(
            frame,
            title,
            layout["TYRIAN_GBA_LAYOUT_GAME_MENU_TITLE_CENTER_X"],
            layout["TYRIAN_GBA_LAYOUT_GAME_MENU_TITLE_Y"],
            0xfb,
        )
        for index in range(FRONTEND_STATIC_GAME_MENU_COUNT):
            disabled = False
            colour = (
                0xf8 if index == selection else 0xf4
            ) if disabled else (
                0xfe if index == selection else 0xfa
            )
            source_y = (
                layout["TYRIAN_GBA_LAYOUT_GAME_MENU_FIRST_SOURCE_Y"] +
                index *
                    layout["TYRIAN_GBA_LAYOUT_GAME_MENU_SOURCE_ROW_STEP"] +
                (
                    layout["TYRIAN_GBA_LAYOUT_GAME_MENU_QUIT_SOURCE_GAP"]
                    if index == 5 else 0
                )
            )
            draw_menu_text(
                frame,
                full_game_menu[index + 1] or game_fallback[index + 1],
                layout["TYRIAN_GBA_LAYOUT_GAME_MENU_ITEM_X"],
                source_y * FRONTEND_FRAME_HEIGHT // 200,
                layout["TYRIAN_GBA_LAYOUT_GAME_MENU_ITEM_RIGHT"],
                colour,
            )
        add(f"game_menu_{selection}", frame)

    upgrade_menu = source.text["upgrade_menu"]
    upgrade_fallback = (
        "Upgrade Ship",
        "Ship Type",
        "Front Gun",
        "Rear Gun",
        "Shield",
        "Generator",
        "Left Sidekick",
        "Right Sidekick",
        "Done",
    )
    for selection in range(FRONTEND_STATIC_UPGRADE_MENU_COUNT):
        frame = menu_chrome.copy()

        draw_menu_centered(
            frame,
            upgrade_menu[0] or upgrade_fallback[0],
            layout["TYRIAN_GBA_LAYOUT_UPGRADE_TITLE_CENTER_X"],
            layout["TYRIAN_GBA_LAYOUT_UPGRADE_TITLE_Y"],
            0xfb,
        )
        for index in range(FRONTEND_STATIC_UPGRADE_MENU_COUNT):
            draw_menu_text(
                frame,
                upgrade_menu[index + 1] or upgrade_fallback[index + 1],
                layout["TYRIAN_GBA_LAYOUT_UPGRADE_ITEM_X"],
                layout["TYRIAN_GBA_LAYOUT_UPGRADE_ITEM_FIRST_Y"] +
                    index *
                    layout["TYRIAN_GBA_LAYOUT_UPGRADE_ITEM_ROW_STEP"],
                layout["TYRIAN_GBA_LAYOUT_UPGRADE_ITEM_RIGHT"],
                0xfe if index == selection else 0xfa,
            )
        add(f"upgrade_menu_{selection}", frame)

    # game_menu.c keeps Options and MENU_LOAD_SAVE on PIC 1.  The slot rows
    # are patched from live SRAM at runtime, but the immutable right-panel
    # title comes directly from HDT Menu 3 rather than a captured screenshot.
    options_menu = source.text["options_menu"]
    frame = menu_chrome.copy()
    draw_menu_centered(
        frame,
        options_menu[0] or "Options",
        layout["TYRIAN_GBA_LAYOUT_OPTIONS_TITLE_CENTER_X"],
        layout["TYRIAN_GBA_LAYOUT_OPTIONS_TITLE_Y"],
        0xfb,
    )
    for index, label in enumerate((
        options_menu[1] or "Load",
        options_menu[2] or "Save",
        options_menu[7] or "Done",
    )):
        draw_menu_centered(
            frame,
            label,
            layout["TYRIAN_GBA_LAYOUT_OPTIONS_CENTER_X"],
            layout["TYRIAN_GBA_LAYOUT_OPTIONS_FIRST_Y"] +
                index * layout["TYRIAN_GBA_LAYOUT_OPTIONS_ROW_STEP"],
            0xfa,
        )
    add("options_menu_base", frame)

    for mode, fallback in enumerate(("Load", "Save")):
        frame = menu_chrome.copy()
        draw_menu_centered(
            frame,
            options_menu[mode + 1] or fallback,
            layout["TYRIAN_GBA_LAYOUT_GAME_SAVE_TITLE_CENTER_X"],
            layout["TYRIAN_GBA_LAYOUT_GAME_SAVE_TITLE_Y"],
            0xfb,
        )
        add(
            "game_save_slots_load_base" if mode == 0 else
                "game_save_slots_save_base",
            frame,
        )

    # Every mainMenuHelp string is immutable stock HDT text.  Baking the
    # final 240-pixel strip avoids thousands of runtime glyph divisions on
    # ARM7TDMI and keeps page changes inside one VBlank/audio budget.
    help_strip_y = max(
        0,
        layout["TYRIAN_GBA_LAYOUT_GAME_MENU_HELP_Y"] - 1,
    )
    help_strip_height = FRONTEND_FRAME_HEIGHT - help_strip_y
    help_strips: list[np.ndarray] = []
    main_menu_help = source.text["main_menu_help"]
    if len(main_menu_help) != FRONTEND_STATIC_SOURCE_HELP_STRIP_COUNT:
        raise ValueError(
            "stock main-menu help count changed: "
            f"{len(main_menu_help)}"
        )
    for text in main_menu_help:
        frame = menu_chrome.copy()
        draw_small_mixed_text(
            frame,
            text,
            layout["TYRIAN_GBA_LAYOUT_GAME_MENU_HELP_X"],
            layout["TYRIAN_GBA_LAYOUT_GAME_MENU_HELP_Y"],
            layout["TYRIAN_GBA_LAYOUT_GAME_MENU_HELP_RIGHT"],
            0xEA,
            0xE2,
        )
        help_strips.append(frame[help_strip_y:, :].copy())
    if len(help_strips) != FRONTEND_STATIC_HELP_STRIP_COUNT:
        raise AssertionError("static help strip catalog changed")
    help_strip_bytes = b"".join(
        strip.tobytes() for strip in help_strips
    )

    # The quit dialog is a transparent overlay on the player's current
    # ship/menu state, so it cannot be baked as an opaque full frame.  Store
    # exact row runs after the one-time SHP decode and 300x200 -> 240x160
    # coordinate mapping.  Runtime then shades the live background and
    # performs only sequential ROM copies; the two short choice labels stay
    # dynamic so cursor changes remain a tiny dirty-rectangle update.
    quit_overlay = np.full(
        (FRONTEND_FRAME_HEIGHT, FRONTEND_FRAME_WIDTH),
        0xFF,
        dtype=np.uint8,
    )
    quit_sprite = source.sprite(5, 35)
    if quit_sprite is None:
        raise ValueError("OPTION_SHAPES quit dialog sprite 35 is empty")
    for sprite_y in range(quit_sprite.shape[0]):
        for sprite_x in range(quit_sprite.shape[1]):
            pixel = int(quit_sprite[sprite_y, sprite_x])
            if pixel == 0xFF:
                continue
            source_x = 50 + sprite_x
            source_y = 50 + sprite_y
            if (
                source_x < FRONTEND_MENU_SOURCE_CROP_X or
                source_x >=
                    FRONTEND_MENU_SOURCE_CROP_X +
                    FRONTEND_MENU_SOURCE_WIDTH or
                source_y < 0 or
                source_y >= 200
            ):
                continue
            target_x = (
                (source_x - FRONTEND_MENU_SOURCE_CROP_X) *
                FRONTEND_FRAME_WIDTH //
                FRONTEND_MENU_SOURCE_WIDTH
            )
            target_y = source_y * FRONTEND_FRAME_HEIGHT // 200
            quit_overlay[target_y, target_x] = pixel

    misc_text = source.text["misc_text"]
    # JE_operation(performSave) uses the same OPTION_SHAPES 35 message box
    # as Quit, but with its own source text and live level/name fields.  Bake
    # only immutable source pixels/text; runtime reapplies this overlay before
    # drawing the current level and gamepad-edited pilot name.
    save_name_overlay = quit_overlay.copy()
    draw_small_mixed_text(
        save_name_overlay,
        misc_text[0] or "Last Level Completed",
        48,
        44,
        192,
        0xFA,
    )
    draw_centered(
        save_name_overlay,
        misc_text[9] or "OK",
        layout["TYRIAN_GBA_LAYOUT_QUIT_OK_CENTER_X"],
        layout["TYRIAN_GBA_LAYOUT_QUIT_CHOICES_Y"],
        0xFE,
    )
    draw_centered(
        save_name_overlay,
        misc_text[10] or "CANCEL",
        layout["TYRIAN_GBA_LAYOUT_QUIT_CANCEL_CENTER_X"],
        layout["TYRIAN_GBA_LAYOUT_QUIT_CHOICES_Y"],
        0xF6,
    )

    draw_small_mixed_text(
        quit_overlay,
        misc_text[28] or "Are you sure you want to exit?",
        layout["TYRIAN_GBA_LAYOUT_QUIT_QUESTION_X"],
        layout["TYRIAN_GBA_LAYOUT_QUIT_QUESTION_Y"],
        layout["TYRIAN_GBA_LAYOUT_QUIT_QUESTION_RIGHT"],
        0xFE,
    )
    draw_small_mixed_wrapped(
        quit_overlay,
        misc_text[30] or "You will be returned to the main menu.",
        layout["TYRIAN_GBA_LAYOUT_QUIT_HELP_X"],
        layout["TYRIAN_GBA_LAYOUT_QUIT_HELP_Y"],
        layout["TYRIAN_GBA_LAYOUT_QUIT_HELP_RIGHT"],
        9,
        3,
        0xFA,
    )
    def encode_sparse(
        canvas: np.ndarray,
        mask: np.ndarray,
        magic: bytes,
    ) -> tuple[bytes, int]:
        runs: list[tuple[int, bytes]] = []

        if len(magic) != 4:
            raise AssertionError("sparse overlay magic must be four bytes")
        for target_y in range(FRONTEND_FRAME_HEIGHT):
            opaque = np.flatnonzero(mask[target_y])
            if not opaque.size:
                continue
            run_start = int(opaque[0])
            previous = run_start
            for value in opaque[1:]:
                target_x = int(value)
                if target_x != previous + 1:
                    runs.append(
                        (
                            target_y * FRONTEND_FRAME_WIDTH + run_start,
                            canvas[
                                target_y,
                                run_start:previous + 1,
                            ].tobytes(),
                        )
                    )
                    run_start = target_x
                previous = target_x
            runs.append(
                (
                    target_y * FRONTEND_FRAME_WIDTH + run_start,
                    canvas[
                        target_y,
                        run_start:previous + 1,
                    ].tobytes(),
                )
            )
        stream = bytearray(magic)
        stream.extend(struct.pack("<HH", 1, len(runs)))
        for offset, pixels in runs:
            stream.extend(struct.pack("<HH", offset, len(pixels)))
            stream.extend(pixels)
            stream.extend(b"\x00" * (-len(pixels) & 3))
        return bytes(stream), len(runs)

    quit_dense_x = 36
    quit_dense_y = 40
    quit_dense_width = 156
    quit_dense_height = 81
    quit_dense_mask = quit_overlay != 0xFF
    outside_dense = quit_dense_mask.copy()
    outside_dense[
        quit_dense_y:quit_dense_y + quit_dense_height,
        quit_dense_x:quit_dense_x + quit_dense_width,
    ] = False
    if outside_dense.any():
        raise AssertionError("quit overlay escaped its dense rectangle")
    quit_dense = quit_overlay[
        quit_dense_y:quit_dense_y + quit_dense_height,
        quit_dense_x:quit_dense_x + quit_dense_width,
    ].copy()
    quit_stream = bytearray(b"OTQF")
    quit_stream.extend(
        struct.pack(
            "<6H",
            1,
            quit_dense_x,
            quit_dense_y,
            quit_dense_width,
            quit_dense_height,
            0,
        )
    )
    quit_stream.extend(quit_dense.tobytes())
    quit_stream = bytes(quit_stream)

    save_name_dense_mask = save_name_overlay != 0xFF
    save_name_outside_dense = save_name_dense_mask.copy()
    save_name_outside_dense[
        quit_dense_y:quit_dense_y + quit_dense_height,
        quit_dense_x:quit_dense_x + quit_dense_width,
    ] = False
    if save_name_outside_dense.any():
        raise AssertionError("save-name overlay escaped its dense rectangle")
    save_name_dense = save_name_overlay[
        quit_dense_y:quit_dense_y + quit_dense_height,
        quit_dense_x:quit_dense_x + quit_dense_width,
    ].copy()
    save_name_stream = bytearray(b"OTSN")
    save_name_stream.extend(
        struct.pack(
            "<6H",
            1,
            quit_dense_x,
            quit_dense_y,
            quit_dense_width,
            quit_dense_height,
            0,
        )
    )
    save_name_stream.extend(save_name_dense.tobytes())
    save_name_stream = bytes(save_name_stream)
    choice_frames: list[np.ndarray] = []
    for yes_selected in (True, False):
        choice_frame = quit_overlay.copy()
        draw_centered(
            choice_frame,
            misc_text[9] or "OK",
            layout["TYRIAN_GBA_LAYOUT_QUIT_OK_CENTER_X"],
            layout["TYRIAN_GBA_LAYOUT_QUIT_CHOICES_Y"],
            0xFE if yes_selected else 0xF6,
        )
        draw_centered(
            choice_frame,
            misc_text[10] or "CANCEL",
            layout["TYRIAN_GBA_LAYOUT_QUIT_CANCEL_CENTER_X"],
            layout["TYRIAN_GBA_LAYOUT_QUIT_CHOICES_Y"],
            0xF6 if yes_selected else 0xFE,
        )
        choice_frames.append(choice_frame)
    choice_mask = (
        (choice_frames[0] != quit_overlay) |
        (choice_frames[1] != quit_overlay)
    )
    choice_streams: list[bytes] = []
    choice_run_count = 0
    for choice_frame in choice_frames:
        stream, run_count = encode_sparse(
            choice_frame,
            choice_mask,
            b"OTQC",
        )
        if choice_streams and (
            len(stream) != len(choice_streams[0]) or
            run_count != choice_run_count
        ):
            raise AssertionError("quit choice sparse layouts diverged")
        choice_streams.append(stream)
        choice_run_count = run_count
    choice_bytes = b"".join(choice_streams)
    shade_x0 = 65 * FRONTEND_FRAME_WIDTH // FRONTEND_MENU_SOURCE_WIDTH
    shade_x1 = 256 * FRONTEND_FRAME_WIDTH // FRONTEND_MENU_SOURCE_WIDTH
    shade_y0 = 55 * FRONTEND_FRAME_HEIGHT // 200
    shade_y1 = 156 * FRONTEND_FRAME_HEIGHT // 200
    shade_mask = np.zeros_like(quit_dense_mask)
    shade_mask[shade_y0:shade_y1, shade_x0:shade_x1] = True
    visible_shade_mask = shade_mask & ~quit_dense_mask
    shade_runs: list[tuple[int, int]] = []
    for target_y in range(FRONTEND_FRAME_HEIGHT):
        visible = np.flatnonzero(visible_shade_mask[target_y])
        if not visible.size:
            continue
        run_start = int(visible[0])
        previous = run_start
        for value in visible[1:]:
            target_x = int(value)
            if target_x != previous + 1:
                shade_runs.append(
                    (
                        target_y * FRONTEND_FRAME_WIDTH + run_start,
                        previous - run_start + 1,
                    )
                )
                run_start = target_x
            previous = target_x
        shade_runs.append(
            (
                target_y * FRONTEND_FRAME_WIDTH + run_start,
                previous - run_start + 1,
            )
        )
    shade_stream = bytearray(b"OTQS")
    shade_stream.extend(struct.pack("<HH", 1, len(shade_runs)))
    for offset, length in shade_runs:
        shade_stream.extend(struct.pack("<HH", offset, length))
    shade_stream = bytes(shade_stream)

    quit_preview = menu_chrome.copy()
    shade = quit_preview[shade_y0:shade_y1, shade_x0:shade_x1]
    low = shade & 0x0F
    shade[:] = (shade & 0xF0) | np.maximum(low, 3) - 3
    overlay_opaque = quit_overlay != 0xFF
    quit_preview[overlay_opaque] = quit_overlay[overlay_opaque]
    quit_preview[choice_mask] = choice_frames[0][choice_mask]
    rgb = np.minimum(
        source.palette_rgb_index(0).astype(np.uint16) * 4,
        255,
    ).astype(np.uint8)
    Image.fromarray(rgb[quit_preview], "RGB").save(
        preview_dir / "quit_dialog_static_overlay.png"
    )

    panel_bytes = b"".join(panel.tobytes() for panel in panels)
    pre_game_bytes = b"".join(
        frame.tobytes() for frame in pre_game_frames
    )
    metadata = {
        "FRONTEND_NATIVE_FONT_GLYPH_COUNT": native_font.shape[0],
        "FRONTEND_NATIVE_FONT_HEIGHT": native_font.shape[1],
        "FRONTEND_NATIVE_FONT_BYTES": native_font.size,
        "FRONTEND_PREGAME_FONT_GLYPH_COUNT": pregame_font.shape[0],
        "FRONTEND_PREGAME_FONT_HEIGHT": pregame_font.shape[1],
        "FRONTEND_PREGAME_FONT_BYTES": pregame_font.size,
        "FRONTEND_STATIC_MENU_PANEL_X": FRONTEND_STATIC_MENU_PANEL_X,
        "FRONTEND_STATIC_MENU_PANEL_Y": FRONTEND_STATIC_MENU_PANEL_Y,
        "FRONTEND_STATIC_MENU_PANEL_WIDTH":
            FRONTEND_STATIC_MENU_PANEL_WIDTH,
        "FRONTEND_STATIC_MENU_PANEL_HEIGHT":
            FRONTEND_STATIC_MENU_PANEL_HEIGHT,
        "FRONTEND_STATIC_MENU_PANEL_BYTES":
            FRONTEND_STATIC_MENU_PANEL_BYTES,
        "FRONTEND_STATIC_GAME_MENU_BASE": 0,
        "FRONTEND_STATIC_GAME_MENU_COUNT":
            FRONTEND_STATIC_GAME_MENU_COUNT,
        "FRONTEND_STATIC_UPGRADE_MENU_BASE":
            FRONTEND_STATIC_GAME_MENU_COUNT,
        "FRONTEND_STATIC_UPGRADE_MENU_COUNT":
            FRONTEND_STATIC_UPGRADE_MENU_COUNT,
        "FRONTEND_STATIC_OPTIONS_MENU_BASE":
            FRONTEND_STATIC_GAME_MENU_COUNT +
                FRONTEND_STATIC_UPGRADE_MENU_COUNT,
        "FRONTEND_STATIC_OPTIONS_MENU_COUNT":
            FRONTEND_STATIC_OPTIONS_MENU_COUNT,
        "FRONTEND_STATIC_SAVE_MENU_BASE":
            FRONTEND_STATIC_GAME_MENU_COUNT +
                FRONTEND_STATIC_UPGRADE_MENU_COUNT +
                FRONTEND_STATIC_OPTIONS_MENU_COUNT,
        "FRONTEND_STATIC_SAVE_MENU_COUNT":
            FRONTEND_STATIC_SAVE_MENU_COUNT,
        "FRONTEND_STATIC_SAVE_NAME_MENU_BASE":
            FRONTEND_STATIC_GAME_MENU_COUNT +
                FRONTEND_STATIC_UPGRADE_MENU_COUNT +
                FRONTEND_STATIC_OPTIONS_MENU_COUNT +
                FRONTEND_STATIC_SAVE_MENU_COUNT,
        "FRONTEND_STATIC_SAVE_NAME_MENU_COUNT":
            FRONTEND_STATIC_SAVE_NAME_MENU_COUNT,
        "FRONTEND_STATIC_HELP_STRIP_COUNT":
            FRONTEND_STATIC_HELP_STRIP_COUNT,
        "FRONTEND_STATIC_SOURCE_HELP_STRIP_BASE": 0,
        "FRONTEND_STATIC_SOURCE_HELP_STRIP_COUNT":
            FRONTEND_STATIC_SOURCE_HELP_STRIP_COUNT,
        "FRONTEND_STATIC_OPTIONS_HELP_STRIP_BASE":
            FRONTEND_STATIC_SOURCE_HELP_STRIP_COUNT,
        "FRONTEND_STATIC_OPTIONS_HELP_STRIP_COUNT":
            FRONTEND_STATIC_OPTIONS_HELP_STRIP_COUNT,
        "FRONTEND_STATIC_SAVE_HELP_STRIP_BASE":
            FRONTEND_STATIC_SOURCE_HELP_STRIP_COUNT +
                FRONTEND_STATIC_OPTIONS_HELP_STRIP_COUNT,
        "FRONTEND_STATIC_SAVE_HELP_STRIP_COUNT":
            FRONTEND_STATIC_SAVE_HELP_STRIP_COUNT,
        "FRONTEND_STATIC_SAVE_NAME_HELP_STRIP_BASE":
            FRONTEND_STATIC_SOURCE_HELP_STRIP_COUNT +
                FRONTEND_STATIC_OPTIONS_HELP_STRIP_COUNT +
                FRONTEND_STATIC_SAVE_HELP_STRIP_COUNT,
        "FRONTEND_STATIC_SAVE_NAME_HELP_STRIP_COUNT":
            FRONTEND_STATIC_SAVE_NAME_HELP_STRIP_COUNT,
        "FRONTEND_STATIC_HELP_STRIP_Y": help_strip_y,
        "FRONTEND_STATIC_HELP_STRIP_WIDTH": FRONTEND_FRAME_WIDTH,
        "FRONTEND_STATIC_HELP_STRIP_HEIGHT": help_strip_height,
        "FRONTEND_STATIC_HELP_STRIP_BYTES":
            FRONTEND_FRAME_WIDTH * help_strip_height,
        "FRONTEND_STATIC_HELP_STRIPS_BYTES": len(help_strip_bytes),
        "FRONTEND_STATIC_MENU_PANEL_COUNT": len(panels),
        "FRONTEND_STATIC_MENU_PANELS_BYTES": len(panel_bytes),
        "FRONTEND_STATIC_PRE_GAME_TITLE_BASE": 0,
        "FRONTEND_STATIC_PRE_GAME_TITLE_COUNT": 4,
        "FRONTEND_STATIC_PRE_GAME_PLAY_MODE_BASE": 4,
        "FRONTEND_STATIC_PRE_GAME_PLAY_MODE_COUNT": 2,
        "FRONTEND_STATIC_PRE_GAME_EPISODE_BASE": 6,
        "FRONTEND_STATIC_PRE_GAME_EPISODE_COUNT": 4,
        "FRONTEND_STATIC_PRE_GAME_DIFFICULTY_BASE": 10,
        "FRONTEND_STATIC_PRE_GAME_DIFFICULTY_COUNT": 3,
        "FRONTEND_STATIC_PRE_GAME_SAVE_SLOTS_BASE": 13,
        "FRONTEND_STATIC_PRE_GAME_SAVE_SLOTS_COUNT": 1,
        "FRONTEND_STATIC_PRE_GAME_SAVE_NAME_BASE": 14,
        "FRONTEND_STATIC_PRE_GAME_SAVE_NAME_COUNT": 0,
        "FRONTEND_STATIC_PRE_GAME_FRAME_COUNT": len(pre_game_frames),
        "FRONTEND_STATIC_PRE_GAME_FRAMES_BYTES": len(pre_game_bytes),
        "FRONTEND_STATIC_SAVE_NAME_OVERLAY_VERSION": 1,
        "FRONTEND_STATIC_SAVE_NAME_OVERLAY_X": quit_dense_x,
        "FRONTEND_STATIC_SAVE_NAME_OVERLAY_Y": quit_dense_y,
        "FRONTEND_STATIC_SAVE_NAME_OVERLAY_WIDTH": quit_dense_width,
        "FRONTEND_STATIC_SAVE_NAME_OVERLAY_HEIGHT": quit_dense_height,
        "FRONTEND_STATIC_SAVE_NAME_OVERLAY_HEADER_BYTES": 16,
        "FRONTEND_STATIC_SAVE_NAME_OVERLAY_PIXEL_BYTES":
            save_name_dense.size,
        "FRONTEND_STATIC_SAVE_NAME_OVERLAY_BYTES":
            len(save_name_stream),
        "FRONTEND_STATIC_QUIT_OVERLAY_VERSION": 1,
        "FRONTEND_STATIC_QUIT_OVERLAY_X": quit_dense_x,
        "FRONTEND_STATIC_QUIT_OVERLAY_Y": quit_dense_y,
        "FRONTEND_STATIC_QUIT_OVERLAY_WIDTH": quit_dense_width,
        "FRONTEND_STATIC_QUIT_OVERLAY_HEIGHT": quit_dense_height,
        "FRONTEND_STATIC_QUIT_OVERLAY_HEADER_BYTES": 16,
        "FRONTEND_STATIC_QUIT_OVERLAY_PIXEL_BYTES": quit_dense.size,
        "FRONTEND_STATIC_QUIT_OVERLAY_BYTES": len(quit_stream),
        "FRONTEND_STATIC_QUIT_CHOICE_VERSION": 1,
        "FRONTEND_STATIC_QUIT_CHOICE_COUNT": len(choice_streams),
        "FRONTEND_STATIC_QUIT_CHOICE_RUN_COUNT": choice_run_count,
        "FRONTEND_STATIC_QUIT_CHOICE_VARIANT_BYTES":
            len(choice_streams[0]),
        "FRONTEND_STATIC_QUIT_CHOICES_BYTES": len(choice_bytes),
        "FRONTEND_STATIC_QUIT_SHADE_VERSION": 1,
        "FRONTEND_STATIC_QUIT_SHADE_RUN_COUNT": len(shade_runs),
        "FRONTEND_STATIC_QUIT_SHADE_PIXEL_COUNT":
            int(visible_shade_mask.sum()),
        "FRONTEND_STATIC_QUIT_SHADE_BYTES": len(shade_stream),
    }
    report = [
        (
            "frontend_static_menu_panel_source="
            "stock PIC/HDT text + project mixed-case 6x8 font"
        ),
        (
            "frontend_static_menu_panel_strategy="
            "build-time right-panel bake; runtime aligned ROM copy"
        ),
        f"frontend_native_font_bytes={native_font.size}",
        (
            "frontend_pregame_font="
            "project authored mixed-case 6x8 strokes"
        ),
        f"frontend_pregame_font_bytes={pregame_font.size}",
        f"frontend_static_menu_panel_count={len(panels)}",
        (
            "frontend_static_menu_panel_dimensions="
            f"{FRONTEND_STATIC_MENU_PANEL_WIDTH}x"
            f"{FRONTEND_STATIC_MENU_PANEL_HEIGHT}"
        ),
        f"frontend_static_menu_panel_bytes={len(panel_bytes)}",
        (
            "frontend_static_help_strategy="
            "build-time stock HDT mixed-case strips; aligned ROM copy"
        ),
        (
            "frontend_static_help_dimensions="
            f"{FRONTEND_FRAME_WIDTH}x{help_strip_height}"
        ),
        f"frontend_static_help_count={len(help_strips)}",
        f"frontend_static_help_bytes={len(help_strip_bytes)}",
        (
            "frontend_static_help_crc32="
            f"{zlib.crc32(help_strip_bytes):08x}"
        ),
        (
            "frontend_static_menu_panel_crc32="
            f"{zlib.crc32(panel_bytes):08x}"
        ),
        f"frontend_static_pre_game_frame_count={len(pre_game_frames)}",
        f"frontend_static_pre_game_frames_bytes={len(pre_game_bytes)}",
        (
            "frontend_static_pre_game_frames_crc32="
            f"{zlib.crc32(pre_game_bytes):08x}"
        ),
        (
            "frontend_static_save_name_overlay_strategy="
            "stock OPTION_SHAPES 35 + HDT labels; live level/name patch"
        ),
        f"frontend_static_save_name_overlay_bytes={len(save_name_stream)}",
        (
            "frontend_static_save_name_overlay_crc32="
            f"{zlib.crc32(save_name_stream):08x}"
        ),
        (
            "frontend_static_quit_overlay_strategy="
            "build-time dense aligned rectangle + transparent word mask"
        ),
        (
            "frontend_static_quit_overlay_dimensions="
            f"{quit_dense_width}x{quit_dense_height}"
        ),
        (
            "frontend_static_quit_overlay_transparent_pixels="
            f"{int((quit_dense == 0xFF).sum())}"
        ),
        f"frontend_static_quit_overlay_bytes={len(quit_stream)}",
        (
            "frontend_static_quit_overlay_crc32="
            f"{zlib.crc32(quit_stream):08x}"
        ),
        (
            "frontend_static_quit_choice_strategy="
            "build-time exact sparse colour patches"
        ),
        f"frontend_static_quit_choice_runs={choice_run_count}",
        (
            "frontend_static_quit_choice_variant_bytes="
            f"{len(choice_streams[0])}"
        ),
        f"frontend_static_quit_choices_bytes={len(choice_bytes)}",
        (
            "frontend_static_quit_choices_crc32="
            f"{zlib.crc32(choice_bytes):08x}"
        ),
        (
            "frontend_static_quit_shade_strategy="
            "only pixels visible outside the opaque dialog"
        ),
        f"frontend_static_quit_shade_runs={len(shade_runs)}",
        (
            "frontend_static_quit_shade_pixels="
            f"{int(visible_shade_mask.sum())}"
        ),
        f"frontend_static_quit_shade_bytes={len(shade_stream)}",
        (
            "frontend_static_quit_shade_crc32="
            f"{zlib.crc32(shade_stream):08x}"
        ),
        *(
            f"frontend_static_menu_panel_{index:02d}={name},"
            f"crc32={zlib.crc32(panels[index].tobytes()):08x}"
            for index, name in enumerate(names)
        ),
        *(
            f"frontend_static_pre_game_frame_{index:02d}={name},"
            f"crc32={zlib.crc32(pre_game_frames[index].tobytes()):08x}"
            for index, name in enumerate(pre_game_names)
        ),
    ]
    return (
        panel_bytes,
        pre_game_bytes,
        save_name_stream,
        quit_stream,
        choice_bytes,
        shade_stream,
        help_strip_bytes,
        metadata,
        report,
    )


def build_frontend_mode4_assets(
    data_root: Path,
    preview: Path,
    project_root: Path,
) -> tuple[
    bytes,
    bytes,
    bytes,
    bytes,
    bytes,
    bytes,
    bytes,
    bytes,
    bytes,
    bytes,
    bytes,
    bytes,
    bytes,
    bytes,
    bytes,
    bytes,
    bytes,
    bytes,
    dict[str, int],
    list[str],
]:
    source = FrontendSourceRenderer(data_root)
    native_font = load_frontend_native_font(
        Path(__file__).with_name("frontend_native_font.txt")
    )
    pregame_font = load_frontend_pregame_font(
        Path(__file__).with_name("frontend_pregame_font.txt")
    )
    layout = load_frontend_layout(project_root / "Configure.h")
    (
        static_menu_panels,
        static_pre_game_frames,
        static_save_name_overlay,
        static_quit_overlay,
        static_quit_choices,
        static_quit_shade,
        static_help_strips,
        static_menu_metadata,
        static_menu_report,
    ) = build_frontend_static_menu_panels(
        source,
        native_font,
        pregame_font,
        layout,
        preview,
    )
    (
        nav_obj_tiles,
        nav_obj_meta,
        nav_obj_palette,
        nav_obj_metadata,
        nav_obj_report,
    ) = build_frontend_nav_obj_assets(source, preview)
    (
        nav_bitmap_blocks,
        nav_bitmap_indices,
        nav_bitmap_metadata,
        nav_bitmap_report,
    ) = build_frontend_nav_bitmap_pages(source, preview)
    frames: list[np.ndarray] = []
    palettes: list[bytes] = []
    names: list[str] = []
    metadata: dict[str, int] = {
        "FRONTEND_FRAME_BYTES": FRONTEND_FRAME_BYTES,
    }
    frontend_preview = preview / "frontend_mode4"
    frontend_preview.mkdir(parents=True, exist_ok=True)

    def add(
        name: str,
        frame: np.ndarray,
        picture_number: int,
        palette_index: int | None = None,
    ) -> int:
        if frame.shape != (FRONTEND_FRAME_HEIGHT, FRONTEND_FRAME_WIDTH):
            raise ValueError(f"front-end frame has invalid shape: {frame.shape}")
        index = len(frames)
        frames.append(frame.copy())
        if palette_index is None:
            palette_index = FRONTEND_PCX_PALETTES[picture_number - 1]
        palettes.append(source.palette_gba_index(palette_index))
        names.append(name)
        metadata[f"FRONTEND_FRAME_{name.upper()}"] = index
        rgb = np.minimum(
            source.palette_rgb_index(palette_index).astype(np.uint16) * 4,
            255,
        ).astype(np.uint8)
        Image.fromarray(rgb[frame], "RGB").save(
            frontend_preview / f"{index:02d}_{name}.png"
        )
        return index

    def render_title_chrome() -> np.ndarray:
        frame = source.picture_frame(4)
        source.draw_logo(frame)
        return frame

    def render_title(selection: int) -> np.ndarray:
        frame = render_title_chrome()
        labels = ("Start New Game", "Load Game", "Demo", "JukeBox")
        for index, label in enumerate(labels):
            brightness = -1 if selection == index else -4
            source.draw_text(
                frame,
                label,
                160,
                108 + index * 12,
                1,
                "center",
                15,
                brightness,
                2,
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
            disabled = index in (0, 1, 3)
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
    add("menu_chrome", source.menu_picture_frame(1), 1)
    add("title_chrome", render_title_chrome(), 4)
    add("select_chrome", source.picture_frame(2), 2)
    metadata["FRONTEND_MENU_SOURCE_CROP_X"] = FRONTEND_MENU_SOURCE_CROP_X
    metadata["FRONTEND_MENU_SOURCE_WIDTH"] = FRONTEND_MENU_SOURCE_WIDTH
    metadata["FRONTEND_NEXT_LEVEL_PALETTE_INDEX"] = 17
    metadata["FRONTEND_FRAME_TITLE_BASE"] = len(frames)
    for selection in range(4):
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
        add(
            f"next_level_{selection}",
            render_next_level(selection),
            1,
            palette_index=17,
        )

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
    design_frame_bytes = b"".join(frame.tobytes() for frame in frames)
    design_palette_bytes = b"".join(palettes)
    if len(design_frame_bytes) != len(frames) * FRONTEND_FRAME_BYTES:
        raise AssertionError("Mode 4 front-end frame packing changed")
    if len(design_palette_bytes) != len(frames) * 512:
        raise AssertionError("Mode 4 front-end palette packing changed")

    # The current runtime composes every interactive menu from one of the
    # three chrome frames plus smaller static panels.  Keeping the superseded
    # per-selection and statistics canvases in the cartridge duplicated 29
    # complete 240x160 surfaces.  They remain above for previews and dirty-
    # rectangle validation, but only the six frames with a live full-frame
    # consumer are emitted to ROM.
    runtime_frame_indices = (
        metadata["FRONTEND_FRAME_INTRO_LOGO_1"],
        metadata["FRONTEND_FRAME_INTRO_LOGO_2"],
        metadata["FRONTEND_FRAME_MENU_CHROME"],
        metadata["FRONTEND_FRAME_TITLE_CHROME"],
        metadata["FRONTEND_FRAME_SELECT_CHROME"],
        metadata["FRONTEND_FRAME_GAME_OVER"],
    )
    frame_bytes = b"".join(
        frames[index].tobytes()
        for index in runtime_frame_indices
    )
    metadata["FRONTEND_RUNTIME_FRAME_COUNT"] = len(runtime_frame_indices)
    for slot, index in enumerate(runtime_frame_indices):
        metadata[
            f"FRONTEND_RUNTIME_FRAME_{names[index].upper()}_SLOT"
        ] = slot

    # Thirty-five logical screens use only six exact palettes.  Preserve the
    # first-occurrence order so the C adapter can map semantic frame groups to
    # compact slots without a generated lookup blob.
    runtime_palettes: list[bytes] = []
    palette_slots: list[int] = []
    for palette in palettes:
        try:
            slot = runtime_palettes.index(palette)
        except ValueError:
            slot = len(runtime_palettes)
            runtime_palettes.append(palette)
        palette_slots.append(slot)
    palette_bytes = b"".join(runtime_palettes)
    metadata["FRONTEND_RUNTIME_PALETTE_COUNT"] = len(runtime_palettes)
    for name in (
        "INTRO_LOGO_1",
        "INTRO_LOGO_2",
        "MENU_CHROME",
        "TITLE_CHROME",
        "SELECT_CHROME",
        "NEXT_LEVEL_0",
    ):
        frame_index = metadata[f"FRONTEND_FRAME_{name}"]
        metadata[f"FRONTEND_RUNTIME_PALETTE_{name}_SLOT"] = (
            palette_slots[frame_index]
        )
    if len(runtime_frame_indices) != 6 or len(runtime_palettes) != 6:
        raise AssertionError("front-end runtime compaction contract changed")

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
            "title", metadata["FRONTEND_FRAME_TITLE_BASE"], 4,
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
        f"frontend_runtime_frame_count={len(runtime_frame_indices)}",
        f"frontend_frame_bytes={FRONTEND_FRAME_BYTES}",
        f"frontend_frames_design_raw_bytes={len(design_frame_bytes)}",
        f"frontend_frames_runtime_bytes={len(frame_bytes)}",
        (
            "frontend_frames_omitted_duplicate_bytes="
            f"{len(design_frame_bytes) - len(frame_bytes)}"
        ),
        f"frontend_palettes_design_bytes={len(design_palette_bytes)}",
        f"frontend_palettes_bytes={len(palette_bytes)}",
        (
            "frontend_palettes_omitted_duplicate_bytes="
            f"{len(design_palette_bytes) - len(palette_bytes)}"
        ),
        f"frontend_data_cube_stamp_bytes={cube_stamp.size}",
        (
            "frontend_menu_crop="
            f"{FRONTEND_MENU_SOURCE_CROP_X},0,"
            f"{FRONTEND_MENU_SOURCE_WIDTH},200"
        ),
        "frontend_next_level_palette_index=17",
        f"frontend_full_state_transfer_bytes={FRONTEND_FRAME_BYTES + 512}",
        (
            "frontend_zlib_reference_bytes="
            f"{len(zlib.compress(design_frame_bytes, 9))}"
        ),
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
    metadata.update(nav_obj_metadata)
    metadata.update(nav_bitmap_metadata)
    metadata.update(static_menu_metadata)
    report.extend(nav_obj_report)
    report.extend(nav_bitmap_report)
    report.extend(static_menu_report)
    return (
        frame_bytes,
        palette_bytes,
        cube_stamp.tobytes(),
        native_font.tobytes(),
        pregame_font.tobytes(),
        static_menu_panels,
        static_pre_game_frames,
        static_save_name_overlay,
        static_quit_overlay,
        static_quit_choices,
        static_quit_shade,
        static_help_strips,
        nav_obj_tiles,
        nav_obj_meta,
        nav_obj_palette,
        nav_bitmap_blocks,
        nav_bitmap_indices,
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
    opentyrian_root: Path,
    preview: Path,
) -> tuple[dict[str, bytes], dict[str, int], list[str]]:
    """Build a tile/OAM adapter for OpenTyrian's Jukebox and starlib."""
    titles = parse_opentyrian_music_titles(
        opentyrian_root / "src" / "musmast.c"
    )

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

    assets = {
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
        "JUKEBOX_BACKDROP_TILE_COUNT": JUKEBOX_BACKDROP_TILE_COUNT,
        "JUKEBOX_STAR_TILE_COUNT": JUKEBOX_STAR_TILE_COUNT,
        "JUKEBOX_RECIPROCAL_MAX_Z": JUKEBOX_RECIPROCAL_MAX_Z,
        "JUKEBOX_SINE_COUNT": len(sine),
    }
    report = [
        "jukebox_source=OpenTyrian jukebox.c/starlib.c/musmast.c",
        f"jukebox_music_titles={len(titles)}",
        "jukebox_font=shared gameplay fine 4x8 proportional compositor",
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
    gba: ModuleType,
    image: Image.Image,
    size: tuple[int, int],
    offset: tuple[int, int] = (0, 0),
) -> Image.Image:
    """Place a sprite without cropping away its source-space anchor."""
    source = gba.normalize_sprite(image)
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
    gba: ModuleType,
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
    colours = gba.adaptive_palette(rgba[:, :, :, :3][opaque])
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
    return bytes(tile_data), gba.gba_palette_bytes([palette])


def build_explosion_animation(
    gba: ModuleType,
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
        frames.append(preserve_sprite_canvas(gba, source, (16, 16)))
    rgba = np.stack(
        [np.asarray(frame, dtype=np.uint8) for frame in frames],
        axis=0,
    )
    opaque = rgba[:, :, :, 3] >= 80
    colours = gba.adaptive_palette(rgba[:, :, :, :3][opaque])
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
        gba.gba_palette_bytes([palette]),
        preview,
        composite,
    )


def build_reward_animation(
    gba: ModuleType,
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
                gba, source, (16, 16), (2, 1)
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
    tile_data, palette = quantize_sprite_frames(gba, frames)
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


def build_background_palette_assets(
    data_root: Path,
    preview_root: Path,
) -> tuple[bytes, bytes, bytes, dict[str, int], list[str]]:
    """Build shape-bank-specific 4bpp palette adapters for stock MAP tiles.

    Tyrian's 8-bit palette uses the high nibble as a hue family.  A GBA text
    background tile can select only one 16-colour bank, so the former
    "dominant hue" conversion erased narrow authored materials in mixed
    rock/water/ground tiles.  Keep the eleven common single-hue banks exact,
    but train the five mixed-material banks independently for each of the
    five stock shapes?.dat banks.  Runtime selects the matching generated
    adapter from the source shape-file ID, while LVL maps and pixels remain
    direct ROMFS data.  This removes cross-bank training dilution without
    per-level art or hand-authored correction tables.
    """
    palette_data = (data_root / "palette.dat").read_bytes()
    if len(palette_data) < BACKGROUND_PALETTE_SOURCE_COLOURS * 3:
        raise ValueError("palette.dat is truncated")
    source_rgb6 = np.frombuffer(
        palette_data[: BACKGROUND_PALETTE_SOURCE_COLOURS * 3],
        dtype=np.uint8,
    ).reshape(BACKGROUND_PALETTE_SOURCE_COLOURS, 3)
    source_rgb8 = (
        (source_rgb6.astype(np.uint16) << 2) |
        (source_rgb6.astype(np.uint16) >> 4)
    ).astype(np.float64)
    mask_histogram = np.zeros(
        (
            BACKGROUND_PALETTE_MASK_COUNT,
            BACKGROUND_PALETTE_SOURCE_COLOURS,
        ),
        dtype=np.float64,
    )
    shape_mask_histograms: list[dict[int, np.ndarray]] = []
    shape_file_ids: list[str] = []
    shape_file_count = 0
    shape_tile_count = 0

    for shape_path in sorted(data_root.glob("shapes*.dat")):
        source = shape_path.read_bytes()
        position = 0
        local_histogram: dict[int, np.ndarray] = {}
        shape_file_id = shape_path.stem[-1].lower()
        shape_file_count += 1
        for shape_index in range(600):
            if position >= len(source):
                raise ValueError(
                    f"background shape bank is truncated: {shape_path}"
                )
            blank = source[position]
            position += 1
            if blank:
                pixels = np.zeros((28, 24), dtype=np.uint8)
            else:
                if position + 28 * 24 > len(source):
                    raise ValueError(
                        f"background shape is truncated: "
                        f"{shape_path}:{shape_index + 1}"
                    )
                pixels = np.frombuffer(
                    source[position : position + 28 * 24],
                    dtype=np.uint8,
                ).reshape(28, 24)
                position += 28 * 24
            for phase in range(0, 28, 4):
                for source_x in range(0, 24, 8):
                    values = pixels[
                        phase : min(phase + 8, 28),
                        source_x : source_x + 8,
                    ].reshape(-1)
                    values = values[values != 0]
                    if values.size == 0:
                        continue
                    mask = sum(
                        1 << int(hue)
                        for hue in np.unique(values >> 4)
                    )
                    counts = np.bincount(
                        values,
                        minlength=BACKGROUND_PALETTE_SOURCE_COLOURS,
                    )
                    mask_histogram[mask] += counts
                    if mask not in local_histogram:
                        local_histogram[mask] = np.zeros(
                            BACKGROUND_PALETTE_SOURCE_COLOURS,
                            dtype=np.float64,
                        )
                    local_histogram[mask] += counts
                    shape_tile_count += 1
        shape_file_ids.append(shape_file_id)
        shape_mask_histograms.append(local_histogram)

    if (
        shape_file_count != len(BACKGROUND_PALETTE_SHAPE_FILE_IDS) or
        tuple(shape_file_ids) != BACKGROUND_PALETTE_SHAPE_FILE_IDS or
        shape_tile_count == 0
    ):
        raise ValueError(
            "unexpected stock background shape-bank coverage: "
            f"ids={shape_file_ids}, files={shape_file_count}, "
            f"tiles={shape_tile_count}"
        )

    def gba_expand(values: np.ndarray) -> np.ndarray:
        values = values.astype(np.uint16)
        return ((values << 3) | (values >> 2)).astype(np.float64)

    def quantize_centres(values: np.ndarray) -> np.ndarray:
        five_bit = np.clip(
            np.rint(values * 31.0 / 255.0),
            0,
            31,
        ).astype(np.uint8)
        return gba_expand(five_bit)

    def hue_balanced_weights(histogram: np.ndarray) -> np.ndarray:
        weights = histogram.astype(np.float64, copy=True)
        for hue in range(16):
            start = hue * 16
            end = start + 16
            total = weights[start:end].sum()
            if total:
                weights[start:end] /= total
        weights[0] = 0
        return weights

    def train_mixed_bank(histogram: np.ndarray) -> np.ndarray:
        weights = hue_balanced_weights(histogram)
        used = np.flatnonzero(weights)
        if used.size == 0:
            raise ValueError("mixed background palette has no training pixels")
        points = source_rgb8[used]
        point_weights = weights[used]
        centres = [points[np.argmin(points.sum(axis=1))]]
        while len(centres) < 15:
            current = np.asarray(centres)
            distance = (
                (points[:, None, :] - current[None, :, :]) ** 2
            ).sum(axis=2).min(axis=1)
            centres.append(
                points[
                    np.argmax(
                        distance * np.sqrt(point_weights + 1.0e-12)
                    )
                ]
            )
        result = np.asarray(centres, dtype=np.float64)
        for _ in range(30):
            assignment = (
                (points[:, None, :] - result[None, :, :]) ** 2
            ).sum(axis=2).argmin(axis=1)
            updated = result.copy()
            for colour in range(15):
                selected = assignment == colour
                if selected.any():
                    selected_weights = point_weights[selected]
                    updated[colour] = (
                        points[selected] * selected_weights[:, None]
                    ).sum(axis=0) / selected_weights.sum()
            updated = quantize_centres(updated)
            if np.array_equal(updated, result):
                break
            result = updated
        return result

    def grouped_histogram(
        histograms: dict[int, np.ndarray] | np.ndarray,
        masks: tuple[int, ...],
    ) -> np.ndarray:
        result = np.zeros(
            BACKGROUND_PALETTE_SOURCE_COLOURS,
            dtype=np.float64,
        )
        for mask in masks:
            if isinstance(histograms, dict):
                values = histograms.get(mask)
                if values is not None:
                    result += values
            else:
                result += histograms[mask]
        return result

    global_centres = np.empty(
        (
            BACKGROUND_PALETTE_BANK_COUNT,
            BACKGROUND_PALETTE_COLOURS_PER_BANK - 1,
            3,
        ),
        dtype=np.float64,
    )
    for bank in range(11):
        source_five_bit = (
            source_rgb6[bank * 16 + 1 : bank * 16 + 16] >> 1
        )
        global_centres[bank] = gba_expand(source_five_bit)
    for bank, masks in enumerate(BACKGROUND_MIXED_MASK_GROUPS, start=11):
        global_centres[bank] = train_mixed_bank(
            grouped_histogram(mask_histogram, masks)
        )

    def palette_mapping(
        centres: np.ndarray,
    ) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
        distance = (
            source_rgb8[None, :, None, :] -
            centres[:, None, :, :]
        )
        distance = (distance * distance).sum(axis=3)
        nearest_zero_based = distance.argmin(axis=2).astype(np.uint8)
        nearest = nearest_zero_based + 1
        nearest[:, 0] = 0
        error = np.take_along_axis(
            distance,
            nearest_zero_based[:, :, None],
            axis=2,
        )[:, :, 0]
        hue_error = np.zeros((16, 16), dtype=np.float64)
        for hue in range(16):
            start = hue * 16 + 1
            hue_error[hue] = error[:, start : start + 15].mean(axis=1)
        return nearest, error, hue_error

    def active_assignment(
        error: np.ndarray,
        histograms: dict[int, np.ndarray],
    ) -> dict[int, int]:
        return {
            mask: int(
                (
                    hue_balanced_weights(histogram) @ error.T
                ).argmin()
            )
            for mask, histogram in histograms.items()
        }

    def assignment_error(
        error: np.ndarray,
        assignment: dict[int, int],
        histograms: dict[int, np.ndarray],
    ) -> float:
        return sum(
            float(histograms[mask] @ error[bank])
            for mask, bank in assignment.items()
        )

    def full_mask_table(
        hue_error: np.ndarray,
        assignment: dict[int, int],
    ) -> np.ndarray:
        table = np.zeros(
            BACKGROUND_PALETTE_MASK_COUNT,
            dtype=np.uint8,
        )
        for mask in range(1, BACKGROUND_PALETTE_MASK_COUNT):
            if mask in assignment:
                table[mask] = assignment[mask]
            else:
                hues = [
                    hue
                    for hue in range(16)
                    if mask & (1 << hue)
                ]
                table[mask] = int(
                    hue_error[hues].sum(axis=0).argmin()
                )
        return table

    def palette_words_for(centres: np.ndarray) -> np.ndarray:
        words = np.zeros(
            (
                BACKGROUND_PALETTE_BANK_COUNT,
                BACKGROUND_PALETTE_COLOURS_PER_BANK,
            ),
            dtype="<u2",
        )
        for bank in range(BACKGROUND_PALETTE_BANK_COUNT):
            for colour in range(
                1,
                BACKGROUND_PALETTE_COLOURS_PER_BANK,
            ):
                red, green, blue = centres[bank, colour - 1]
                red5 = int(round(red * 31.0 / 255.0))
                green5 = int(round(green * 31.0 / 255.0))
                blue5 = int(round(blue * 31.0 / 255.0))
                words[bank, colour] = (
                    red5 | (green5 << 5) | (blue5 << 10)
                )
        return words

    def save_palette_preview(
        centres: np.ndarray,
        shape_file_id: str,
    ) -> None:
        preview = Image.new("RGB", (15 * 8, 16 * 8), (0, 0, 0))
        preview_pixels = preview.load()
        for bank in range(16):
            for colour in range(15):
                rgb = tuple(
                    int(round(component))
                    for component in centres[bank, colour]
                )
                for y in range(bank * 8, bank * 8 + 8):
                    for x in range(colour * 8, colour * 8 + 8):
                        preview_pixels[x, y] = rgb
        safe_id = "paren" if shape_file_id == ")" else shape_file_id
        preview.save(
            preview_root /
            f"background_mixed_palette_{safe_id}.png"
        )

    global_nearest, global_error, _ = palette_mapping(global_centres)
    del global_nearest
    palette_variants: list[bytes] = []
    nearest_variants: list[bytes] = []
    mask_variants: list[bytes] = []
    variant_reports: list[str] = []
    critical_masks = (0x1004, 0x1005)
    critical_banks: list[int] = []

    for shape_file_id, histograms in zip(
        shape_file_ids,
        shape_mask_histograms,
        strict=True,
    ):
        local_centres = global_centres.copy()
        for bank, masks in enumerate(
            BACKGROUND_MIXED_MASK_GROUPS,
            start=11,
        ):
            local_training = grouped_histogram(histograms, masks)
            if local_training.sum() != 0:
                local_centres[bank] = train_mixed_bank(local_training)

        nearest, error, hue_error = palette_mapping(local_centres)
        assignment = active_assignment(error, histograms)
        local_objective = assignment_error(
            error,
            assignment,
            histograms,
        )
        global_assignment = active_assignment(global_error, histograms)
        global_objective = assignment_error(
            global_error,
            global_assignment,
            histograms,
        )
        if local_objective > global_objective:
            local_centres = global_centres.copy()
            nearest, error, hue_error = palette_mapping(local_centres)
            assignment = global_assignment
            local_objective = global_objective

        mask_bank = full_mask_table(
            hue_error,
            assignment,
        )
        palette_words = palette_words_for(local_centres)
        palette_variants.append(palette_words.tobytes())
        nearest_variants.append(nearest.tobytes())
        mask_variants.append(mask_bank.tobytes())
        save_palette_preview(local_centres, shape_file_id)

        improvement = (
            0.0
            if global_objective == 0
            else
            (global_objective - local_objective) *
                100.0 / global_objective
        )
        variant_reports.extend((
            (
                f"background_palette_v53_shape_{shape_file_id}_active_masks="
                f"{len(histograms)}"
            ),
            (
                f"background_palette_v53_shape_{shape_file_id}_global_error="
                f"{global_objective:.0f}"
            ),
            (
                f"background_palette_v53_shape_{shape_file_id}_local_error="
                f"{local_objective:.0f}"
            ),
            (
                f"background_palette_v53_shape_{shape_file_id}_improvement="
                f"{improvement:.4f}%"
            ),
        ))

    palette_bytes = b"".join(palette_variants)
    nearest_bytes = b"".join(nearest_variants)
    mask_bytes = b"".join(mask_variants)
    trainer = load_background_palette_trainer()
    baseline_assets = trainer.PaletteAssets(
        words=np.frombuffer(
            palette_bytes,
            dtype="<u2",
        ).reshape(
            len(shape_file_ids),
            BACKGROUND_PALETTE_BANK_COUNT,
            BACKGROUND_PALETTE_COLOURS_PER_BANK,
        ).copy(),
        nearest=np.frombuffer(
            nearest_bytes,
            dtype=np.uint8,
        ).reshape(
            len(shape_file_ids),
            BACKGROUND_PALETTE_BANK_COUNT,
            BACKGROUND_PALETTE_SOURCE_COLOURS,
        ).copy(),
        mask_bank=np.frombuffer(
            mask_bytes,
            dtype=np.uint8,
        ).reshape(
            len(shape_file_ids),
            BACKGROUND_PALETTE_MASK_COUNT,
        ).copy(),
    )
    (
        runtime_datasets,
        runtime_shapes,
        runtime_dataset_metadata,
    ) = trainer.build_runtime_datasets(data_root)
    perceptual_source_rgb = trainer.load_source_rgb(data_root)
    perceptual_candidate_rgb = trainer.bgr555_rgb()
    source_oklab = trainer.rgb_code_to_oklab(perceptual_source_rgb)
    candidate_oklab = trainer.rgb_code_to_oklab(
        perceptual_candidate_rgb
    )
    source_cielab = trainer.rgb_code_to_cielab(
        perceptual_source_rgb
    )
    candidate_cielab = trainer.rgb_code_to_cielab(
        perceptual_candidate_rgb
    )
    trained = trainer.train_assets_safe_unused(
        runtime_datasets,
        baseline_assets,
        source_oklab,
        candidate_oklab,
        source_cielab,
        candidate_cielab,
        30,
        20,
    )
    (
        palette_bytes,
        nearest_bytes,
        mask_bytes,
    ) = trainer.assets_to_bytes(trained.assets)
    runtime_reports: list[str] = []
    training_summary: dict[str, dict[str, int]] = {}
    for record in trained.iterations:
        profile = str(record["profile"])
        if "trainable_banks" in record:
            training_summary.setdefault(profile, {}).update({
                "protected_banks": int(record["protected_banks"]),
                "trainable_banks": int(record["trainable_banks"]),
                "pareto_protected_banks":
                    int(record["pareto_protected_banks"]),
            })
        if "safe_active_mask_changes" in record:
            training_summary.setdefault(profile, {}).update({
                "safe_active_mask_changes":
                    int(record["safe_active_mask_changes"]),
            })
    for profile_index, shape_file_id in enumerate(shape_file_ids):
        dataset = runtime_datasets[shape_file_id]
        baseline_oklab = trainer.evaluate_profile(
            dataset,
            baseline_assets.words[profile_index],
            baseline_assets.nearest[profile_index],
            baseline_assets.mask_bank[profile_index],
            source_oklab,
            candidate_oklab,
        )
        candidate_oklab_evaluation = trainer.evaluate_profile(
            dataset,
            trained.assets.words[profile_index],
            trained.assets.nearest[profile_index],
            trained.assets.mask_bank[profile_index],
            source_oklab,
            candidate_oklab,
        )
        baseline_cie_error = trainer.palette_metric_error(
            baseline_assets.words[profile_index],
            baseline_assets.nearest[profile_index],
            source_cielab,
            candidate_cielab,
            "ciede2000",
        )
        candidate_cie_error = trainer.palette_metric_error(
            trained.assets.words[profile_index],
            trained.assets.nearest[profile_index],
            source_cielab,
            candidate_cielab,
            "ciede2000",
        )
        baseline_cie = trainer.evaluate_profile_error(
            dataset,
            baseline_assets.mask_bank[profile_index],
            baseline_cie_error,
        )
        candidate_cie = trainer.evaluate_profile_error(
            dataset,
            trained.assets.mask_bank[profile_index],
            candidate_cie_error,
        )
        oklab_delta = (
            candidate_oklab_evaluation.key_errors -
            baseline_oklab.key_errors
        )
        cie_delta = (
            candidate_cie.key_errors -
            baseline_cie.key_errors
        )
        oklab_regressions = int(
            np.count_nonzero(oklab_delta > 1.0e-12)
        )
        cie_regressions = int(
            np.count_nonzero(cie_delta > 1.0e-8)
        )
        oklab_improvement = trainer.improvement_percent(
            baseline_oklab.mean_squared,
            candidate_oklab_evaluation.mean_squared,
        )
        cie_improvement = trainer.improvement_percent(
            baseline_cie.mean_squared,
            candidate_cie.mean_squared,
        )
        baseline_ramp = trainer.ramp_report(
            dataset,
            profile_index,
            baseline_assets,
            candidate_oklab,
        )
        candidate_ramp = trainer.ramp_report(
            dataset,
            profile_index,
            trained.assets,
            candidate_oklab,
        )
        if (
            oklab_regressions != 0 or
            cie_regressions != 0 or
            oklab_improvement <= 0 or
            cie_improvement <= 0 or
            candidate_ramp["lightness_inversions"] >
                baseline_ramp["lightness_inversions"] or
            candidate_ramp["palette_collisions"] >
                baseline_ramp["palette_collisions"]
        ):
            raise ValueError(
                "background palette perceptual non-regression failed: "
                f"{shape_file_id}: OKLab={oklab_improvement:.6f}%/"
                f"{oklab_regressions}, CIEDE2000={cie_improvement:.6f}%/"
                f"{cie_regressions}, ramp={baseline_ramp}->"
                f"{candidate_ramp}"
            )
        safe_summary = training_summary[shape_file_id]
        runtime_reports.extend((
            (
                f"background_palette_shape_{shape_file_id}_levels="
                f"{dataset.level_count}"
            ),
            (
                f"background_palette_shape_{shape_file_id}_runtime_keys="
                f"{len(dataset.keys)}"
            ),
            (
                f"background_palette_shape_{shape_file_id}_active_masks="
                f"{len(np.unique(dataset.masks))}"
            ),
            (
                f"background_palette_shape_{shape_file_id}_dataset_sha256="
                f"{dataset.checksum}"
            ),
            (
                f"background_palette_shape_{shape_file_id}_oklab_improvement="
                f"{oklab_improvement:.6f}%"
            ),
            (
                f"background_palette_shape_{shape_file_id}_"
                f"ciede2000_improvement={cie_improvement:.6f}%"
            ),
            (
                f"background_palette_shape_{shape_file_id}_"
                f"oklab_regressed_keys={oklab_regressions}"
            ),
            (
                f"background_palette_shape_{shape_file_id}_"
                f"ciede2000_regressed_keys={cie_regressions}"
            ),
            (
                f"background_palette_shape_{shape_file_id}_"
                f"ramp_inversions="
                f"{baseline_ramp['lightness_inversions']}->"
                f"{candidate_ramp['lightness_inversions']}"
            ),
            (
                f"background_palette_shape_{shape_file_id}_"
                f"ramp_collisions="
                f"{baseline_ramp['palette_collisions']}->"
                f"{candidate_ramp['palette_collisions']}"
            ),
            (
                f"background_palette_shape_{shape_file_id}_"
                f"trainable_banks={safe_summary['trainable_banks']}"
            ),
            (
                f"background_palette_shape_{shape_file_id}_"
                f"safe_mask_changes="
                f"{safe_summary['safe_active_mask_changes']}"
            ),
        ))
        save_palette_preview(
            perceptual_candidate_rgb[
                trained.assets.words[profile_index, :, 1:]
            ],
            shape_file_id,
        )

    profile_palette_bytes = palette_bytes
    profile_nearest_bytes = nearest_bytes
    profile_mask_bytes = mask_bytes
    (
        level_datasets,
        _,
        level_dataset_metadata,
    ) = trainer.build_level_runtime_datasets(
        data_root,
        runtime_shapes,
    )
    all_active_masks = sorted({
        int(mask)
        for _, dataset in level_datasets
        for mask in dataset.masks
    })
    if (
        len(level_datasets) !=
            runtime_dataset_metadata["logical_levels"] or
        len(all_active_masks) !=
            runtime_dataset_metadata["active_masks"] or
        len(all_active_masks) >= 0xff
    ):
        raise ValueError(
            "compact per-level palette index coverage changed: "
            f"levels={len(level_datasets)}, masks={len(all_active_masks)}"
        )
    active_mask_index = np.full(
        BACKGROUND_PALETTE_MASK_COUNT,
        0xff,
        dtype=np.uint8,
    )
    for index, mask in enumerate(all_active_masks):
        active_mask_index[mask] = index

    level_palette_variants: list[bytes] = []
    level_nearest_variants: list[bytes] = []
    level_mask_variants: list[bytes] = []
    level_reports: list[str] = []
    episode_offsets: list[int] = []
    episode_counts: list[int] = []
    level_cursor = 0
    for episode in range(1, 5):
        episode_levels = [
            record
            for record in level_datasets
            if record[0].episode == episode
        ]
        if (
            not episode_levels or
            [record[0].level for record in episode_levels] !=
                list(range(1, len(episode_levels) + 1))
        ):
            raise ValueError(
                f"non-contiguous palette level catalog: episode {episode}"
            )
        episode_offsets.append(level_cursor)
        episode_counts.append(len(episode_levels))
        level_cursor += len(episode_levels)

    for level, dataset in level_datasets:
        profile_index = shape_file_ids.index(level.shape_file)
        baseline_words = trained.assets.words[profile_index]
        baseline_nearest = trained.assets.nearest[profile_index]
        baseline_mask = trained.assets.mask_bank[profile_index]
        (
            level_words,
            level_nearest,
            level_mask,
            level_history,
        ) = trainer.train_profile_safe_unused(
            dataset,
            baseline_words,
            baseline_nearest,
            baseline_mask,
            source_oklab,
            candidate_oklab,
            source_cielab,
            candidate_cielab,
            30,
            20,
        )
        (
            level_words,
            level_nearest,
            level_mask,
            counterexample_history,
        ) = trainer.refine_profile_counterexamples(
            dataset,
            baseline_words,
            baseline_nearest,
            baseline_mask,
            level_words,
            level_nearest,
            level_mask,
            source_oklab,
            candidate_oklab,
            source_cielab,
            candidate_cielab,
            20,
        )
        level_history.extend(counterexample_history)
        if len(dataset.keys) == 0:
            oklab_regressions = 0
            cie_regressions = 0
            oklab_improvement = 0.0
            cie_improvement = 0.0
        else:
            baseline_ok = trainer.evaluate_profile(
                dataset,
                baseline_words,
                baseline_nearest,
                baseline_mask,
                source_oklab,
                candidate_oklab,
            )
            candidate_ok = trainer.evaluate_profile(
                dataset,
                level_words,
                level_nearest,
                level_mask,
                source_oklab,
                candidate_oklab,
            )
            baseline_cie_error = trainer.palette_metric_error(
                baseline_words,
                baseline_nearest,
                source_cielab,
                candidate_cielab,
                "ciede2000",
            )
            candidate_cie_error = trainer.palette_metric_error(
                level_words,
                level_nearest,
                source_cielab,
                candidate_cielab,
                "ciede2000",
            )
            baseline_cie = trainer.evaluate_profile_error(
                dataset,
                baseline_mask,
                baseline_cie_error,
            )
            candidate_cie = trainer.evaluate_profile_error(
                dataset,
                level_mask,
                candidate_cie_error,
            )
            oklab_regressions = int(np.count_nonzero(
                candidate_ok.key_errors >
                    baseline_ok.key_errors + 1.0e-12
            ))
            cie_regressions = int(np.count_nonzero(
                candidate_cie.key_errors >
                    baseline_cie.key_errors + 1.0e-8
            ))
            oklab_improvement = trainer.improvement_percent(
                baseline_ok.mean_squared,
                candidate_ok.mean_squared,
            )
            cie_improvement = trainer.improvement_percent(
                baseline_cie.mean_squared,
                candidate_cie.mean_squared,
            )
        if (
            oklab_regressions != 0 or
            cie_regressions != 0 or
            oklab_improvement < -1.0e-8 or
            cie_improvement < -1.0e-8
        ):
            raise ValueError(
                "per-level background palette non-regression failed: "
                f"E{level.episode}L{level.level}: "
                f"OKLab={oklab_improvement:.6f}%/{oklab_regressions}, "
                f"CIEDE2000={cie_improvement:.6f}%/{cie_regressions}"
            )
        active_set = {
            int(mask)
            for mask in dataset.masks
        }
        compact_mask_banks = np.asarray(
            [
                (
                    level_mask[mask]
                    if mask in active_set
                    else baseline_mask[mask]
                )
                for mask in all_active_masks
            ],
            dtype=np.uint8,
        )
        level_palette_variants.append(
            level_words.astype("<u2", copy=False).tobytes()
        )
        level_nearest_variants.append(
            level_nearest.astype(np.uint8, copy=False).tobytes()
        )
        level_mask_variants.append(compact_mask_banks.tobytes())
        safe_mask_changes = next(
            (
                int(record["safe_active_mask_changes"])
                for record in reversed(level_history)
                if "safe_active_mask_changes" in record
            ),
            0,
        )
        counterexample_refined_masks = next(
            (
                int(record["counterexample_refined_masks"])
                for record in reversed(level_history)
                if "counterexample_refined_masks" in record
            ),
            0,
        )
        level_reports.extend((
            (
                f"background_palette_level_e{level.episode}_"
                f"l{level.level}_shape={level.shape_file}"
            ),
            (
                f"background_palette_level_e{level.episode}_"
                f"l{level.level}_runtime_keys={len(dataset.keys)}"
            ),
            (
                f"background_palette_level_e{level.episode}_"
                f"l{level.level}_active_masks="
                f"{len(active_set)}"
            ),
            (
                f"background_palette_level_e{level.episode}_"
                f"l{level.level}_oklab_improvement="
                f"{oklab_improvement:.6f}%"
            ),
            (
                f"background_palette_level_e{level.episode}_"
                f"l{level.level}_ciede2000_improvement="
                f"{cie_improvement:.6f}%"
            ),
            (
                f"background_palette_level_e{level.episode}_"
                f"l{level.level}_safe_mask_changes="
                f"{safe_mask_changes}"
            ),
            (
                f"background_palette_level_e{level.episode}_"
                f"l{level.level}_counterexample_refined_masks="
                f"{counterexample_refined_masks}"
            ),
        ))
        for record in level_history:
            if (
                int(record.get("counterexample_success", 0)) == 0
            ):
                continue
            level_reports.extend((
                (
                    f"background_palette_level_e{level.episode}_"
                    f"l{level.level}_counterexample_mask="
                    f"0x{int(record['counterexample_mask']):04x}"
                ),
                (
                    f"background_palette_level_e{level.episode}_"
                    f"l{level.level}_counterexample_bank="
                    f"{int(record['counterexample_bank'])}"
                ),
                (
                    f"background_palette_level_e{level.episode}_"
                    f"l{level.level}_counterexample_oklab_improvement="
                    f"{(1.0 - float(record['counterexample_oklab_ratio'])) * 100.0:.6f}%"
                ),
                (
                    f"background_palette_level_e{level.episode}_"
                    f"l{level.level}_counterexample_ciede2000_improvement="
                    f"{(1.0 - float(record['counterexample_ciede2000_ratio'])) * 100.0:.6f}%"
                ),
                (
                    f"background_palette_level_e{level.episode}_"
                    f"l{level.level}_counterexample_ramp_collisions="
                    f"{int(record['counterexample_ramp_collisions_before'])}->"
                    f"{int(record['counterexample_ramp_collisions_after'])}"
                ),
            ))
        if level.episode == 4 and level.level == 4:
            save_palette_preview(
                perceptual_candidate_rgb[level_words[:, 1:]],
                "episode4_surface_physical_level4",
            )

    profile_variant_count = len(shape_file_ids)
    level_variant_count = len(level_datasets)
    variant_count = profile_variant_count + level_variant_count
    level_palette_bytes = b"".join(level_palette_variants)
    level_nearest_bytes = b"".join(level_nearest_variants)
    level_mask_bank_bytes = b"".join(level_mask_variants)
    palette_bytes = profile_palette_bytes + level_palette_bytes
    nearest_bytes = profile_nearest_bytes + level_nearest_bytes
    mask_id_offset = len(profile_mask_bytes)
    level_mask_bank_offset = (
        mask_id_offset + active_mask_index.nbytes
    )
    mask_bytes = (
        profile_mask_bytes +
        active_mask_index.tobytes() +
        level_mask_bank_bytes
    )

    if tuple(shape_file_ids) != tuple(trainer.PROFILE_IDS):
        raise ValueError(
            "palette trainer profile ordering changed: "
            f"{shape_file_ids} != {trainer.PROFILE_IDS}"
        )
    x_profile_index = shape_file_ids.index("x")
    critical_banks = [
        int(
            trained.assets.mask_bank[
                x_profile_index,
                mask,
            ]
        )
        for mask in critical_masks
    ]
    if len(critical_banks) != len(critical_masks):
        raise ValueError("TORM shape-bank palette audit was not generated")

    metadata = {
        "BACKGROUND_PALETTE_VARIANT_COUNT": variant_count,
        "BACKGROUND_PALETTE_PROFILE_VARIANT_COUNT":
            profile_variant_count,
        "BACKGROUND_PALETTE_LEVEL_VARIANT_COUNT":
            level_variant_count,
        "BACKGROUND_GBA_PALETTE_VARIANT_BYTES":
            BACKGROUND_PALETTE_BANK_COUNT *
            BACKGROUND_PALETTE_COLOURS_PER_BANK * 2,
        "BACKGROUND_GBA_PALETTE_BYTES": len(palette_bytes),
        "BACKGROUND_PALETTE_NEAREST_VARIANT_BYTES":
            BACKGROUND_PALETTE_BANK_COUNT *
            BACKGROUND_PALETTE_SOURCE_COLOURS,
        "BACKGROUND_PALETTE_NEAREST_BYTES": len(nearest_bytes),
        "BACKGROUND_PALETTE_MASK_BANK_VARIANT_BYTES":
            BACKGROUND_PALETTE_MASK_COUNT,
        "BACKGROUND_PALETTE_MASK_BANK_BYTES": len(mask_bytes),
        "BACKGROUND_PALETTE_MASK_ID_OFFSET": mask_id_offset,
        "BACKGROUND_PALETTE_LEVEL_MASK_BANK_OFFSET":
            level_mask_bank_offset,
        "BACKGROUND_PALETTE_ACTIVE_MASK_COUNT":
            len(all_active_masks),
        "BACKGROUND_PALETTE_LEVEL_MASK_BANK_VARIANT_BYTES":
            len(all_active_masks),
        "BACKGROUND_PALETTE_EPISODE_1_OFFSET": episode_offsets[0],
        "BACKGROUND_PALETTE_EPISODE_1_COUNT": episode_counts[0],
        "BACKGROUND_PALETTE_EPISODE_2_OFFSET": episode_offsets[1],
        "BACKGROUND_PALETTE_EPISODE_2_COUNT": episode_counts[1],
        "BACKGROUND_PALETTE_EPISODE_3_OFFSET": episode_offsets[2],
        "BACKGROUND_PALETTE_EPISODE_3_COUNT": episode_counts[2],
        "BACKGROUND_PALETTE_EPISODE_4_OFFSET": episode_offsets[3],
        "BACKGROUND_PALETTE_EPISODE_4_COUNT": episode_counts[3],
        "BACKGROUND_PALETTE_CRITICAL_BANK": critical_banks[0],
    }
    report = [
        (
            "background_palette_mode="
            "runtime-key safe-unused + counterexample refinement"
        ),
        f"background_palette_shape_files={shape_file_count}",
        (
            "background_palette_shape_file_ids=" +
            ",".join(shape_file_ids)
        ),
        (
            "background_palette_runtime_logical_levels="
            f"{runtime_dataset_metadata['logical_levels']}"
        ),
        (
            "background_palette_runtime_map_tiles="
            f"{runtime_dataset_metadata['map_tiles_including_blank']}"
        ),
        (
            "background_palette_runtime_unique_keys="
            f"{runtime_dataset_metadata['unique_nonblank_keys']}"
        ),
        (
            "background_palette_runtime_active_masks="
            f"{runtime_dataset_metadata['active_masks']}"
        ),
        (
            "background_palette_training_policy="
            "preserve runtime-used v53 banks; train unused banks; "
            "accept only per-key OKLab+CIEDE2000 non-regressions; "
            "constraint-generate severe mixed-hue masks; "
            "ramp collisions are telemetry, inversions are gated"
        ),
        "background_palette_ciede2000_reference_vectors=3",
        (
            "background_palette_palette_sha256="
            f"{hashlib.sha256(palette_bytes).hexdigest()}"
        ),
        (
            "background_palette_nearest_sha256="
            f"{hashlib.sha256(nearest_bytes).hexdigest()}"
        ),
        (
            "background_palette_mask_sha256="
            f"{hashlib.sha256(mask_bytes).hexdigest()}"
        ),
        (
            "background_palette_torm_shape_x_masks="
            + ",".join(f"0x{mask:04x}" for mask in critical_masks)
            + "->"
            + ",".join(f"bank{bank}" for bank in critical_banks)
        ),
        (
            "background_palette_level_specific_tables="
            f"{level_variant_count}"
        ),
        "background_palette_shape_bank_specific_tables=5",
        (
            "background_palette_level_active_masks="
            f"{len(all_active_masks)}"
        ),
        (
            "background_palette_level_compact_bytes="
            f"{len(level_palette_bytes) + len(level_nearest_bytes) + len(level_mask_bank_bytes) + active_mask_index.nbytes}"
        ),
        (
            "background_palette_level_dataset_unique_keys="
            f"{level_dataset_metadata['unique_nonblank_keys']}"
        ),
        *level_reports,
        *runtime_reports,
        *variant_reports,
    ]
    return (
        palette_bytes,
        nearest_bytes,
        mask_bytes,
        metadata,
        report,
    )


def build_boss_bar_assets(
    gba: ModuleType,
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
    return tiles, gba.gba_palette_bytes([palette]), preview, flash_colours


def build_cash_digits(
    gba: ModuleType,
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
        gba.gba_palette_bytes([palette]),
        preview,
        tuple(advances),
    )


def build_hud_digit_palettes(
    gba: ModuleType,
    palette_file: Path,
) -> tuple[bytes, bytes, bytes]:
    """Build PC-sidebar blue, brown and gold palettes for shared digit tiles."""
    tyrian_palette = load_tyrian_palette(palette_file)

    def digit_palette(dark: int, bright: int) -> bytes:
        return gba.gba_palette_bytes([[
            (0, 0, 0),       # OBJ colour 0: transparent
            (0, 0, 0),       # FULL_SHADE outline
            tyrian_palette[dark],
            tyrian_palette[bright],
        ]])

    # JE_dBar3 starts shield/armor at base+2 and climbs that hue ramp.
    # The power bar uses palette 113..125. Mid/high samples keep the tiny
    # digits legible while remaining representative of the original panel.
    return (
        digit_palette(0x97, 0x9B),  # shield: blue 0x90 family
        digit_palette(0xE7, 0xEB),  # armor: brown 0xe0 family
        digit_palette(0x77, 0x7B),  # generator: gold 0x70 family
    )


def build_gameplay_status_text(
    gba: ModuleType,
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
        # Advance by the glyph that will actually be rendered.  Wide PC
        # FONT_SHAPES letters such as M are reduced to an 8-pixel GBA OBJ;
        # retaining their pre-clamp scaled width left a conspicuous false
        # space in labels such as "GAM E OVER".
        advances.append(foreground.width + 1)
        shadow = Image.new("RGBA", (8, 16), (0, 0, 0, 0))
        shadow_mask = foreground.getchannel("A")
        shadow_shape = Image.new("RGBA", foreground.size, (8, 8, 8, 255))
        shadow_shape.putalpha(shadow_mask)
        shadow.alpha_composite(shadow_shape, (1, 2))
        shadow.alpha_composite(foreground, (0, 0))
        frames.append(shadow)

    tile_data, palette = quantize_sprite_frames(gba, frames)
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
    gba: ModuleType,
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
        palette_data.extend(gba.gba_palette_bytes([palette]))
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
    gba: ModuleType,
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
        colours = gba.adaptive_palette(np.concatenate(opaque_colours, axis=0))
        palette = [(0, 0, 0)] + colours
        palette_arrays[palette_bank] = np.asarray(
            palette[1:],
            dtype=np.int32,
        )
        palette_data.extend(gba.gba_palette_bytes([palette]))
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
    gba_tiles: bytes,
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
    source_count = len(gba_tiles) // 32
    decoded = [
        decode_gba_4bpp(gba_tiles[index * 32 : index * 32 + 32])
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
    metadata["OBJ_PAL_HUD_SHIELD"] = 10
    metadata["OBJ_PAL_HUD_ARMOR"] = 11
    metadata["OBJ_PAL_HUD_GENERATOR"] = 12
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
    # Bank 13 is part of the runtime 8bpp Sprite2 palette (PC hues 12/13).
    # The old assignment let Boss-bar flash colours overwrite deep-green
    # enemy/projectile shades.  Bank 6 is reserved by the retired static
    # PLAYER_SHOT atlas and is not used by the source-parity renderer.
    metadata["OBJ_PAL_BOSS_BAR"] = 6
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
    gba: ModuleType,
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
        gba.quantize_mode1_layer(stacked, palette_count, 0)
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
    instrument_maps: list[object] | None = None,
) -> bytes:
    """Adapt the shared GBA writer when a TYM intro exceeds 200 rows.

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
        instrument_maps,
    )


def build_sparse_tym_tracker_it(
    music: ModuleType,
    workspace: Path,
    tym_path: Path,
    calibration: dict[str, object],
    *,
    finite: bool = False,
) -> tuple[bytes, dict[str, object]]:
    """Build a GBA Maxmod module with the measured per-track calibration.

    The tracker writer accepts all nine OPL2 sources.  Short Tyrian cues
    legitimately use fewer, so append inaudible sentinels while preserving
    every measured GBA source/gain pair.
    """
    original_it_builder = music.build_it_module
    disabled_position_jumps = 0
    sources = [int(source) for source in calibration["sourceChannels"]]
    gains = [float(gain) for gain in calibration["gains"]]
    volume_gains = [
        float(gain) for gain in calibration["eventVolumeGains"]
    ]
    if (
        not 1 <= len(sources) <= music.MAX_GBA_MUSIC_VOICES
        or len(sources) != len(gains)
        or len(sources) != len(volume_gains)
        or len(set(sources)) != len(sources)
    ):
        raise ValueError(
            f"track {calibration['trackNumber']} has invalid Maxmod calibration"
        )
    sentinel = 0x100
    while len(sources) < music.MAX_GBA_MUSIC_VOICES:
        sources.append(sentinel)
        gains.append(1.0)
        volume_gains.append(1.0)
        sentinel += 1

    def build_segmented_it(
        inner_workspace: Path,
        name: str,
        samples: list[tuple[str, bytes, int, bool, int]],
        patterns: list[tuple[int, bytes]],
        orders: list[int],
        speed: int = 6,
        tempo: int = 125,
        channel_pans: list[int] | None = None,
        instrument_maps: list[object] | None = None,
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
            instrument_maps,
        )

    module, report = music.build_tym_tracker_it(
        workspace,
        tym_path,
        sources,
        gains,
        module_builder=build_segmented_it,
        voice_volume_gains=volume_gains,
    )
    report = dict(report)
    report["finite"] = finite
    report["disabled_position_jumps"] = disabled_position_jumps
    report["calibration_profile"] = calibration["profile"]
    report["calibrated_mean_absolute_error_db"] = calibration[
        "calibratedMeanAbsoluteErrorDb"
    ]
    report["legacy_mean_absolute_error_db"] = calibration[
        "legacyMeanAbsoluteErrorDb"
    ]
    if finite and disabled_position_jumps == 0:
        raise ValueError(
            f"finite cue has no IT Bxx loop to remove: {tym_path.name}"
        )
    return module, report


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


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--project-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--preview-dir", type=Path, required=True)
    args = parser.parse_args()

    workspace = args.project_root.resolve()
    output = args.output.resolve()
    preview = args.preview_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    preview.mkdir(parents=True, exist_ok=True)
    # Purge outputs retired by newer runtime adapters.  These files are
    # generated or historical build products, never source inputs.  Removing
    # them here keeps an incremental res/ tree from looking as if both the old
    # and current cartridge representations are still required.
    obsolete_outputs = (
        "frontend_nav_bitmap_pages.bin",
        "frontend_glyphs.bin",
        "frontend_cube.bin",
        "bg_palette.bin",
        "bg1_map.bin",
        "bg1_tiles.bin",
        "bg2_map.bin",
        "bg2_tiles.bin",
        "bg3_map.bin",
        "bg3_tiles.bin",
    )
    for name in obsolete_outputs:
        (output / name).unlink(missing_ok=True)

    gba = gba_assets
    image_root = workspace / "vendor" / "tyrian" / "image"
    data_root = workspace / "vendor" / "tyrian" / "data"
    opentyrian_root = workspace / "vendor" / "opentyrian"
    source_commit = read_git_head(opentyrian_root)
    if source_commit != OPENTYRIAN_SOURCE_COMMIT:
        raise ValueError(
            "OpenTyrian source revision changed; audit the direct port before "
            f"updating {OPENTYRIAN_SOURCE_COMMIT} to {source_commit}"
        )

    (
        tyrend_gba_frames,
        tyrend_gba_palette,
        tyrend_gba_metadata,
        tyrend_gba_report,
    ) = gba_anm.build_gba_anm_assets(data_root / "tyrend.anm")
    (output / "tyrend_gba_frames.bin").write_bytes(tyrend_gba_frames)
    (output / "tyrend_gba_palette.bin").write_bytes(tyrend_gba_palette)

    (
        frontend_frames,
        frontend_palettes,
        frontend_cube,
        frontend_native_font,
        frontend_pregame_font,
        frontend_static_menu_panels,
        frontend_static_pre_game_frames,
        frontend_static_save_name_overlay,
        frontend_static_quit_overlay,
        frontend_static_quit_choices,
        frontend_static_quit_shade,
        frontend_static_help_strips,
        frontend_nav_obj_tiles,
        frontend_nav_obj_meta,
        frontend_nav_obj_palette,
        frontend_nav_bitmap_blocks,
        frontend_nav_bitmap_indices,
        frontend_metadata,
        frontend_report,
    ) = build_frontend_mode4_assets(data_root, preview, workspace)
    (
        background_gba_palette,
        background_palette_nearest,
        background_palette_mask_bank,
        background_palette_metadata,
        background_palette_report,
    ) = build_background_palette_assets(data_root, preview)
    (output / "background_gba_palette.bin").write_bytes(
        background_gba_palette
    )
    (output / "background_palette_nearest.bin").write_bytes(
        background_palette_nearest
    )
    (output / "background_palette_mask_bank.bin").write_bytes(
        background_palette_mask_bank
    )
    (output / "frontend_frames.bin").write_bytes(frontend_frames)
    (output / "frontend_palettes.bin").write_bytes(frontend_palettes)
    (
        frontend_stats_tiles,
        frontend_stats_widths,
        frontend_stats_metadata,
        frontend_stats_report,
    ) = build_frontend_stats_assets(data_root, frontend_cube)
    (output / "frontend_stats_tiles.bin").write_bytes(
        frontend_stats_tiles
    )
    (output / "frontend_stats_widths.bin").write_bytes(
        frontend_stats_widths
    )
    frontend_metadata.update(frontend_stats_metadata)
    frontend_metadata.update(background_palette_metadata)
    frontend_report.extend(frontend_stats_report)
    (output / "frontend_native_font.bin").write_bytes(
        frontend_native_font
    )
    (output / "frontend_pregame_font.bin").write_bytes(
        frontend_pregame_font
    )
    (output / "frontend_static_menu_panels.bin").write_bytes(
        frontend_static_menu_panels
    )
    (output / "frontend_static_pre_game_frames.bin").write_bytes(
        frontend_static_pre_game_frames
    )
    (output / "frontend_static_save_name_overlay.bin").write_bytes(
        frontend_static_save_name_overlay
    )
    (output / "frontend_static_quit_overlay.bin").write_bytes(
        frontend_static_quit_overlay
    )
    (output / "frontend_static_quit_choices.bin").write_bytes(
        frontend_static_quit_choices
    )
    (output / "frontend_static_quit_shade.bin").write_bytes(
        frontend_static_quit_shade
    )
    (output / "frontend_static_help_strips.bin").write_bytes(
        frontend_static_help_strips
    )
    (output / "frontend_nav_obj_tiles.bin").write_bytes(
        frontend_nav_obj_tiles
    )
    (output / "frontend_nav_obj_meta.bin").write_bytes(
        frontend_nav_obj_meta
    )
    (output / "frontend_nav_obj_palette.bin").write_bytes(
        frontend_nav_obj_palette
    )
    (output / "frontend_nav_bitmap_blocks.bin").write_bytes(
        frontend_nav_bitmap_blocks
    )
    (output / "frontend_nav_bitmap_indices.bin").write_bytes(
        frontend_nav_bitmap_indices
    )
    (
        frontend_source_stamp_offsets,
        frontend_source_stamp_data,
        frontend_source_stamp_metadata,
        frontend_source_stamp_report,
    ) = build_frontend_source_stamp_assets(data_root)
    (output / "frontend_source_stamp_offsets.bin").write_bytes(
        frontend_source_stamp_offsets
    )
    (output / "frontend_source_stamp_data.bin").write_bytes(
        frontend_source_stamp_data
    )
    frontend_metadata.update(frontend_source_stamp_metadata)
    frontend_report.extend(frontend_source_stamp_report)
    (output / "frontend_mode4_audit.txt").write_text(
        "\n".join(frontend_report) + "\n",
        encoding="utf-8",
    )
    (
        jukebox_assets,
        jukebox_metadata,
        jukebox_report,
    ) = build_jukebox_assets(opentyrian_root, preview)
    for name, data in jukebox_assets.items():
        (output / name).write_bytes(data)

    title = build_title(image_root)
    title.save(preview / "title_gba.png")

    sprite2_raw, sprite2_raw_report = build_sprite2_raw_components(
        data_root
    )
    sprite2_xmas_raw, sprite2_xmas_raw_report = (
        build_sprite2_xmas_raw_components(data_root)
    )
    (
        sprite2_palette_brightness_samples,
        sprite2_palette_report,
    ) = train_sprite2_palette_brightness_samples(
        data_root,
        sprite2_raw,
        sprite2_xmas_raw,
    )
    (output / "sprite2_raw_components.bin").write_bytes(sprite2_raw)
    (output / "sprite2_xmas_raw_components.bin").write_bytes(
        sprite2_xmas_raw
    )
    write_sprite2_raw_header(
        output,
        sprite2_raw_report,
        sprite2_xmas_raw_report,
        sprite2_palette_brightness_samples,
        sprite2_palette_report,
    )
    (output / "sprite2_raw_audit.txt").write_text(
        "\n".join(
            f"{key}={value}"
            for key, value in sprite2_raw_report.items()
        ) + "\n" + "\n".join(
            f"xmas_{key}={value}"
            for key, value in sprite2_xmas_raw_report.items()
        ) + "\n" + "\n".join(
            f"palette_{key}={value}"
            for key, value in sprite2_palette_report.items()
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
    gba_obj_tiles, obj_palette, source_metadata, obj_preview = (
        gba.build_obj_assets(image_root, player_shot_source)
    )
    (
        nort_ship_tiles,
        nort_ship_palette,
        nort_ship_preview,
    ) = gba.build_nort_ship_assets(image_root)
    (
        explosion_tiles,
        explosion_palette,
        explosion_preview,
        explosion_composite_preview,
    ) = (
        build_explosion_animation(gba, image_root)
    )
    reward_tiles, reward_palette, reward_preview = build_reward_animation(
        gba, image_root
    )
    (
        digit_tiles,
        digit_palette,
        digit_preview,
        digit_advances,
    ) = build_cash_digits(gba, image_root, data_root / "palette.dat")
    (
        hud_shield_palette,
        hud_armor_palette,
        hud_generator_palette,
    ) = build_hud_digit_palettes(
        gba,
        data_root / "palette.dat",
    )
    (
        pause_tiles,
        pause_palette,
        pause_preview,
        pause_advances,
    ) = build_gameplay_status_text(
        gba,
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
        gba,
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
        gba,
        image_root,
        data_root / "palette.dat",
    )
    (
        insert_coin_tiles,
        insert_coin_palette,
        insert_coin_preview,
        insert_coin_advances,
    ) = build_gameplay_status_text(
        gba,
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
    ) = build_boss_bar_assets(gba, data_root / "palette.dat")
    obj_palette = bytearray(obj_palette).ljust(512, b"\0")
    obj_palette[7 * 32 : 8 * 32] = explosion_palette
    obj_palette[8 * 32 : 9 * 32] = reward_palette
    obj_palette[9 * 32 : 10 * 32] = digit_palette
    obj_palette[10 * 32 : 11 * 32] = hud_shield_palette
    obj_palette[11 * 32 : 12 * 32] = hud_armor_palette
    obj_palette[12 * 32 : 13 * 32] = hud_generator_palette
    obj_palette[6 * 32 : 7 * 32] = boss_bar_palette
    obj_palette[14 * 32 : 15 * 32] = pause_palette
    obj_tiles, obj_metadata = repack_obj_tiles(
        gba_obj_tiles,
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
    obj_metadata.update(tyrend_gba_metadata)
    obj_metadata["NORT_SHIP_SOURCE_GRAPHIC"] = 1
    obj_metadata["NORT_SHIP_FRAME_COUNT"] = 5
    obj_metadata["NORT_SHIP_NEUTRAL_FRAME"] = 2
    obj_metadata["NORT_SHIP_FRAME_TILES"] = 32
    obj_metadata["NORT_SHIP_FRAME_BYTES"] = 1024
    for flash, (bottom, middle, top) in enumerate(boss_bar_flash_colours):
        obj_metadata[f"BOSS_BAR_FLASH_{flash}_BOTTOM"] = bottom
        obj_metadata[f"BOSS_BAR_FLASH_{flash}_MIDDLE"] = middle
        obj_metadata[f"BOSS_BAR_FLASH_{flash}_TOP"] = top
    for obsolete in (
        output / "jukebox_font_tiles.bin",
        output / "enemy_structure_palette.bin",
        output / "enemy_frame_tiles.bin",
        output / "enemy_frame_catalog.bin",
        output / "enemy_frame_audit.csv",
        output / "opentyrian_level1_source_audit.txt",
        output / "reward_drop_audit.txt",
        output / "reward_event33_audit.csv",
        output / "enemy_projectile_audit.txt",
        output / "sprite_mapping_audit.txt",
        preview / "jukebox_pc_font_tiles.png",
        preview / "enemy_frames_exact_catalog.png",
    ):
        obsolete.unlink(missing_ok=True)
    (output / "obj_tiles.bin").write_bytes(obj_tiles)
    (output / "obj_palette.bin").write_bytes(obj_palette)
    (output / "player_nort_tiles.bin").write_bytes(nort_ship_tiles)
    (output / "player_nort_palette.bin").write_bytes(nort_ship_palette)
    (output / "secret_level_palettes.bin").write_bytes(
        secret_level_palettes
    )
    (output / "insert_coin_palette.bin").write_bytes(
        insert_coin_palette
    )
    obj_preview.resize((256, 512), Image.Resampling.NEAREST).save(
        preview / "obj_gba_source_atlas.png"
    )
    nort_ship_preview.resize(
        (
            nort_ship_preview.width * 4,
            nort_ship_preview.height * 4,
        ),
        Image.Resampling.NEAREST,
    ).save(preview / "nort_ship_source_banking.png")
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

    music_root = workspace / "vendor" / "audio" / "Music"
    music_paths = sorted(music_root.glob("[0-9][0-9]_*.tym"))
    if len(music_paths) != JUKEBOX_MUSIC_COUNT:
        raise ValueError(
            "Tyrian TYM catalog changed: "
            f"{len(music_paths)} != {JUKEBOX_MUSIC_COUNT}"
        )
    maxmod_calibrator = load_music_maxmod_calibrator()
    source_calibration_path = music_root / "gba-opl-reference.json"
    maxmod_calibrations: list[dict[str, object]] = []
    for expected_number, music_path in enumerate(music_paths, start=1):
        parsed_song = gba_music.parse_tym(music_path)
        calibration = maxmod_calibrator.calibrate_track(
            parsed_song,
            source_calibration_path,
            gba_music.make_track_sample_synthesizer(parsed_song),
        )
        if int(calibration["trackNumber"]) != expected_number:
            raise ValueError(
                "Maxmod calibration track order changed at "
                f"{music_path.name}"
            )
        maxmod_calibrations.append(calibration)
    maxmod_catalog = maxmod_calibrator.write_catalog(
        output / "music_maxmod_calibration.json",
        maxmod_calibrations,
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
            gba_music,
            workspace,
            music_path,
            maxmod_calibrations[source_index],
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
            gba_music,
            workspace,
            music_paths[source_index],
            maxmod_calibrations[source_index],
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
    xmas_voice_file = data_root / "voicesc.snd"
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
    xmas_voice_sound_count = struct.unpack_from(
        "<H",
        xmas_voice_file.read_bytes(),
        0,
    )[0]
    if (
        ordinary_sound_count != 29 or
        voice_sound_count != 9 or
        xmas_voice_sound_count != voice_sound_count
    ):
        raise ValueError(
            "Tyrian source sound catalog changed: "
            f"{ordinary_sound_count} ordinary + "
            f"{voice_sound_count} voices + "
            f"{xmas_voice_sound_count} Christmas voices"
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
    for voice_index in range(xmas_voice_sound_count):
        pcm = extract_tyrian_sfx_entry(xmas_voice_file, voice_index)
        # Same nortsong.c 100-byte corrupt-tail rule as ordinary voices.snd.
        if len(pcm) < 100:
            raise ValueError(
                "Tyrian Christmas voice "
                f"{voice_index + 1} is shorter than its trim"
            )
        write_signed_pcm_wav(
            output / f"source_xmas_voice_{voice_index + 1:02d}.wav",
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
        *background_palette_report,
        *jukebox_report,
        *tyrend_gba_report,
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
        "nort_ship_source=Sprite2 220/222 + banking 39/40/58/59",
        f"nort_ship_frames={len(nort_ship_tiles) // 1024}",
        f"nort_ship_tile_bytes={len(nort_ship_tiles)}",
        "obj_enemy_archetypes=0 (removed; no gameplay ID aliases)",
        "obj_enemy_preconverted_frames=0",
        (
            "obj_enemy_runtime_source=complete build-time lossless raw "
            "catalog from vendor newsh*.shp/tyrian.shp"
        ),
        "obj_enemy_runtime_decoder=lossless raw component compositor",
        "obj_enemy_newsh_romfs_duplicate_retained=0",
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
        (
            "music_catalog_pcm_bytes="
            f"{sum(int(report['sample_pcm_bytes']) for report in music_reports)}"
        ),
        (
            "music_catalog_profile=GbaMaxmod nine-channel adaptive-root "
            "original OPL2 patch adapter"
        ),
        (
            "music_calibration_source_count="
            f"{maxmod_catalog['summary']['sourceCount']}"
        ),
        (
            "music_calibration_gain_min="
            f"{maxmod_catalog['summary']['gainMin']:.9f}"
        ),
        (
            "music_calibration_gain_max="
            f"{maxmod_catalog['summary']['gainMax']:.9f}"
        ),
        (
            "music_calibration_event_volume_gain_max="
            f"{maxmod_catalog['summary']['eventVolumeGainMax']:.9f}"
        ),
        (
            "music_calibration_legacy_mean_abs_error_db="
            f"{maxmod_catalog['summary']['legacyMeanAbsoluteErrorDb']:.6f}"
        ),
        (
            "music_calibration_mean_abs_error_db="
            f"{maxmod_catalog['summary']['calibratedMeanAbsoluteErrorDb']:.6f}"
        ),
        (
            "music_calibration_sample_clip_count="
            f"{maxmod_catalog['summary']['sampleClipCount']}"
        ),
        (
            "music_calibration_peak_limited_sources="
            f"{maxmod_catalog['summary']['peakLimitedSourceCount']}"
        ),
        (
            "music_calibration_maximum_peak_ratio="
            f"{maxmod_catalog['summary']['maximumPeakRatio']:.6f}"
        ),
        (
            "music_calibration_percussion_peak_ceiling_ratio="
            f"{maxmod_catalog['summary']['percussionPeakCeilingRatio']:.3f}"
        ),
        (
            "music_calibration_tonal_peak_ceiling_ratio="
            f"{maxmod_catalog['summary']['tonalTransientPeakCeilingRatio']:.3f}"
        ),
        (
            "music_calibration_reference_gain_db="
            f"{maxmod_catalog['playbackReferenceGainDb']:.3f}"
        ),
        "music_calibration_per_song_maximum_normalization=0",
        "music_calibration_reference_fold_down=mono_L_plus_R",
        "music_opl_renderer=vendored_OpenTyrian_DOSBox_core",
        "music_opl_native_render_rate_hz=49716",
        "music_opl_downsample=127tap_Blackman_sinc_to_15768Hz",
        "music_opl_patch_features=ADSR_KSL_KSR_feedback_waveform_HW_and_LDS_LFO",
        "music_opl_note_model=generation_aware_attack_plus_adaptive_root_zones",
        "music_opl_percussion=original_patch_original_pitch_one_shot",
        (
            "music_opl_tonal_zones="
            f"{sum(int(report['tonal_zones']) for report in music_reports)}"
        ),
        (
            "music_opl_percussion_zones="
            f"{sum(int(report['percussion_zones']) for report in music_reports)}"
        ),
        (
            "music_opl_hardware_lfo_zones="
            f"{sum(int(report['hardware_lfo_zones']) for report in music_reports)}"
        ),
        (
            "music_opl_software_lfo_zones="
            f"{sum(int(report['software_lfo_zones']) for report in music_reports)}"
        ),
        "music_opl_source_channels=9_complete",
        "music_tonal_pcm_rate_hz=15768",
        "music_opl_percussion_rate_hz=15768",
        "audio_source_sfx_rate_hz=11025_source_native",
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
