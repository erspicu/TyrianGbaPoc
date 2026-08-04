#!/usr/bin/env python3
"""Audit every stock HUD Special Weapon against the GBA runtime contract.

This is deliberately a source-data audit, not another converted asset list.
It walks only the live ``0..weapon.max-1`` HDT positions, follows authored
chain reactions recursively, and reports every rendering dependency used by
the 24 Special items that have an upper-left HUD icon.
"""

from __future__ import annotations

import argparse
import struct
from dataclasses import dataclass
from pathlib import Path

from export_special_weapon_reference import item_database, special_records


ROOT = Path(__file__).resolve().parents[1]
WEAPON_COUNT = 781
WEAPON_BYTES = 80
EXPECTED_ICON_IDS = (
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
    13, 14, 15, 16, 17, 18, 19, 37, 40, 41, 44, 45,
)
SUPPORTED_OPTION_SHAPES = {21, 33}
SUPPORTED_ICON_TYPES = set(range(1, 13))


@dataclass(frozen=True)
class WeaponRecord:
    weapon_id: int
    multi: int
    maximum: int
    attack: tuple[int, ...]
    delay: tuple[int, ...]
    graphic: tuple[int, ...]


def weapon_record(database: bytes, weapon_id: int) -> WeaponRecord:
    if not 0 < weapon_id < WEAPON_COUNT:
        raise ValueError(f"weapon id outside stock table: {weapon_id}")
    start = 14 + weapon_id * WEAPON_BYTES
    raw = database[start : start + WEAPON_BYTES]
    if len(raw) != WEAPON_BYTES:
        raise ValueError(f"weapon {weapon_id} is truncated")
    maximum = raw[6]
    if not 1 <= maximum <= 8:
        raise ValueError(f"weapon {weapon_id} has invalid max={maximum}")
    if not 1 <= raw[3] <= 8:
        raise ValueError(f"weapon {weapon_id} has invalid multi={raw[3]}")
    return WeaponRecord(
        weapon_id=weapon_id,
        multi=raw[3],
        maximum=maximum,
        attack=tuple(raw[10:18]),
        delay=tuple(raw[18:26]),
        graphic=struct.unpack_from("<8H", raw, 58),
    )


def special_weapon_roots(special_type: int, weapon: int) -> tuple[int, ...]:
    if special_type == 1 or 5 <= special_type <= 11 or special_type == 16:
        return (weapon,)
    # JE_specialComplete() launches weapon 707 in supported Super Arcade
    # modes in addition to granting invulnerability.
    if special_type == 12:
        return (707,)
    return ()


def walk_weapon(
    database: bytes,
    weapon_id: int,
    visited: set[int],
    option_shapes: set[int],
    superpixel_hues: set[int],
) -> None:
    if weapon_id in visited:
        return
    visited.add(weapon_id)
    weapon = weapon_record(database, weapon_id)
    for position in range(weapon.maximum):
        graphic = weapon.graphic[position]
        if graphic > 60000:
            option_shapes.add(graphic - 60001)
        elif graphic > 1000:
            hue = graphic // 1000
            base_graphic = graphic % 1000
            if not 1 <= hue <= 15:
                raise ValueError(
                    f"weapon {weapon_id} position {position} has "
                    f"unsupported superpixel hue {hue}"
                )
            if base_graphic == 0:
                raise ValueError(
                    f"weapon {weapon_id} position {position} has an empty "
                    "superpixel base graphic"
                )
            superpixel_hues.add(hue)

        attack = weapon.attack[position]
        if 99 < attack < 250:
            walk_weapon(
                database,
                attack - 100,
                visited,
                option_shapes,
                superpixel_hues,
            )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    data_root = ROOT / "vendor" / "tyrian" / "data"
    episodes = [special_records(index, data_root) for index in range(1, 5)]
    icon_ids = tuple(
        record.item_id for record in episodes[0] if record.itemgraphic != 0
    )
    if icon_ids != EXPECTED_ICON_IDS:
        raise ValueError(
            f"HUD Special set changed: expected {EXPECTED_ICON_IDS}, got {icon_ids}"
        )

    baseline = tuple(
        (record.item_id, record.name, record.itemgraphic)
        for record in episodes[0]
    )
    for episode, records in enumerate(episodes[1:], start=2):
        artwork = tuple(
            (record.item_id, record.name, record.itemgraphic)
            for record in records
        )
        if artwork != baseline:
            raise ValueError(f"Episode {episode} HUD Special artwork differs")

    all_weapons: set[tuple[int, int]] = set()
    all_option_shapes: set[int] = set()
    all_superpixel_hues: set[int] = set()
    per_special: list[str] = []
    for episode, records in enumerate(episodes, start=1):
        database = item_database(episode, data_root)
        for item_id in icon_ids:
            special = records[item_id]
            if special.special_type not in SUPPORTED_ICON_TYPES:
                raise ValueError(
                    f"Episode {episode} Special {item_id} uses unsupported "
                    f"type {special.special_type}"
                )
            visited: set[int] = set()
            option_shapes: set[int] = set()
            superpixel_hues: set[int] = set()
            for root in special_weapon_roots(
                special.special_type,
                special.weapon,
            ):
                walk_weapon(
                    database,
                    root,
                    visited,
                    option_shapes,
                    superpixel_hues,
                )
            all_weapons.update((episode, weapon) for weapon in visited)
            all_option_shapes.update(option_shapes)
            all_superpixel_hues.update(superpixel_hues)
            if args.verbose:
                per_special.append(
                    f"E{episode} {item_id:02d} {special.name}: "
                    f"type={special.special_type}, roots="
                    f"{list(special_weapon_roots(special.special_type, special.weapon))}, "
                    f"reachable={sorted(visited)}, option={sorted(option_shapes)}, "
                    f"superpixel_hues={sorted(superpixel_hues)}"
                )

    unsupported = all_option_shapes - SUPPORTED_OPTION_SHAPES
    if unsupported:
        raise ValueError(
            f"Special Weapon OPTION_SHAPES lack a GBA adapter: {sorted(unsupported)}"
        )
    if all_option_shapes != SUPPORTED_OPTION_SHAPES:
        raise ValueError(
            "Special Weapon OPTION_SHAPES contract changed: "
            f"expected {sorted(SUPPORTED_OPTION_SHAPES)}, "
            f"got {sorted(all_option_shapes)}"
        )

    if args.verbose:
        print("\n".join(per_special))
    print("Special Weapon runtime audit: PASS")
    print(f"episodes=4")
    print(f"hud_specials={len(icon_ids)} ids={','.join(map(str, icon_ids))}")
    print(f"reachable_episode_weapon_records={len(all_weapons)}")
    print(f"option_shapes={','.join(map(str, sorted(all_option_shapes)))}")
    print(
        "superpixel_hues="
        + (",".join(map(str, sorted(all_superpixel_hues))) or "none")
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
