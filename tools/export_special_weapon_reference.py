#!/usr/bin/env python3
"""Export the stock Tyrian Special Weapon icons used by the HUD reference.

The source records come from the Episode item databases.  Icon pixels are
composed from the already losslessly extracted ``tyrian.shp`` power-up bank,
using the same ``graphic,+1,+19,+20`` layout as OpenTyrian's
``blit_sprite2x2()``.  No GBA-only redraw or resampling is applied to the
individual icon files.
"""

from __future__ import annotations

import argparse
import re
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from gba_asset_support import compose_sprite_2x2  # noqa: E402


COUNT_BYTES = 14
WEAPON_COUNT = 781
WEAPON_BYTES = 80
PORT_COUNT = 43
PORT_BYTES = 82
SPECIAL_COUNT = 47
SPECIAL_BYTES = 37


@dataclass(frozen=True)
class SpecialRecord:
    item_id: int
    name: str
    itemgraphic: int
    power: int
    special_type: int
    weapon: int


def item_database(episode: int, data_root: Path) -> bytes:
    if episode < 4:
        source = (data_root / "tyrian.hdt").read_bytes()
        offset = struct.unpack_from("<I", source, 0)[0]
    else:
        source = (data_root / "tyrian4.lvl").read_bytes()
        level_count = struct.unpack_from("<H", source, 0)[0]
        offset = struct.unpack_from(
            "<I", source, 2 + (level_count - 1) * 4
        )[0]
    return source[offset:]


def special_records(episode: int, data_root: Path) -> tuple[SpecialRecord, ...]:
    block = item_database(episode, data_root)
    start = (
        COUNT_BYTES
        + WEAPON_COUNT * WEAPON_BYTES
        + PORT_COUNT * PORT_BYTES
    )
    result: list[SpecialRecord] = []
    for item_id in range(SPECIAL_COUNT):
        record = block[
            start + item_id * SPECIAL_BYTES :
            start + (item_id + 1) * SPECIAL_BYTES
        ]
        if len(record) != SPECIAL_BYTES:
            raise ValueError(
                f"Episode {episode} Special {item_id} is truncated"
            )
        length = min(record[0], 30)
        name = record[1 : 1 + length].decode(
            "cp437", errors="replace"
        ).strip()
        result.append(
            SpecialRecord(
                item_id=item_id,
                name=name or ("None" if item_id == 0 else f"unused {item_id}"),
                itemgraphic=struct.unpack_from("<H", record, 31)[0],
                power=record[33],
                special_type=record[34],
                weapon=struct.unpack_from("<H", record, 35)[0],
            )
        )
    return tuple(result)


def slug(text: str) -> str:
    value = re.sub(r"[^a-z0-9]+", "-", text.lower()).strip("-")
    return value or "unnamed"


def icon_filename(record: SpecialRecord) -> str:
    return f"special-{record.item_id:02d}-{slug(record.name)}.png"


def build_atlas(
    icons: list[tuple[SpecialRecord, Image.Image]],
    output: Path,
) -> None:
    columns = 4
    card_width = 176
    card_height = 76
    rows = (len(icons) + columns - 1) // columns
    atlas = Image.new(
        "RGBA",
        (columns * card_width, rows * card_height),
        (18, 20, 28, 255),
    )
    draw = ImageDraw.Draw(atlas)
    font = ImageFont.load_default()
    for index, (record, icon) in enumerate(icons):
        column = index % columns
        row = index // columns
        left = column * card_width
        top = row * card_height
        draw.rounded_rectangle(
            (left + 3, top + 3, left + card_width - 4, top + card_height - 4),
            radius=5,
            fill=(30, 34, 46, 255),
            outline=(75, 88, 112, 255),
        )
        preview = icon.resize((48, 56), Image.Resampling.NEAREST)
        atlas.alpha_composite(preview, (left + 10, top + 10))
        draw.text(
            (left + 66, top + 17),
            f"{record.item_id:02d}  {record.name}",
            font=font,
            fill=(244, 215, 114, 255),
        )
        draw.text(
            (left + 66, top + 37),
            f"graphic {record.itemgraphic}",
            font=font,
            fill=(175, 194, 222, 255),
        )
        draw.text(
            (left + 66, top + 52),
            f"pwr {record.power} / type {record.special_type} / wpn {record.weapon}",
            font=font,
            fill=(151, 164, 184, 255),
        )
    atlas.save(output)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output",
        type=Path,
        default=ROOT / "MD" / "Rule" / "assets" / "special-weapons",
    )
    args = parser.parse_args()

    data_root = ROOT / "vendor" / "tyrian" / "data"
    powerup_sheet = ROOT / "vendor" / "tyrian" / "image" / "sheets" / "10_powerups"
    player_sheet = ROOT / "vendor" / "tyrian" / "image" / "sheets" / "09_player_ships"
    episodes = [special_records(index, data_root) for index in range(1, 5)]
    baseline_art = tuple(
        (record.item_id, record.name, record.itemgraphic)
        for record in episodes[0]
    )
    if any(
        tuple(
            (record.item_id, record.name, record.itemgraphic)
            for record in records
        ) != baseline_art
        for records in episodes[1:]
    ):
        raise ValueError("Episode Special Weapon names or icon graphics differ")

    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    icons: list[tuple[SpecialRecord, Image.Image]] = []
    for record in episodes[0]:
        if record.itemgraphic == 0:
            continue
        icon = compose_sprite_2x2(powerup_sheet, record.itemgraphic)
        icon.save(output / icon_filename(record))
        icons.append((record, icon))

    # JE_inGameDisplays() uses graphic 304 once per carried Super Bomb.
    Image.open(player_sheet / "304.png").convert("RGBA").save(
        output / "superbomb-stock-graphic-304.png"
    )
    # JE_doSpecialShot() uses 94 when ready and 93 while cooling down.
    Image.open(player_sheet / "094.png").convert("RGBA").save(
        output / "special-ready-graphic-094.png"
    )
    Image.open(player_sheet / "093.png").convert("RGBA").save(
        output / "special-cooldown-graphic-093.png"
    )
    build_atlas(icons, output / "special-weapons-atlas.png")
    print(
        f"Exported {len(icons)} stock Special Weapon icons and atlas to {output}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
