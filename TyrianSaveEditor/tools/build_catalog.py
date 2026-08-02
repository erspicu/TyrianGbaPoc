#!/usr/bin/env python3
"""Build the Save Editor catalog from the project's stock Tyrian data."""

from __future__ import annotations

import json
import re
import struct
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from build_textres import read_records  # noqa: E402


COUNT_BYTES = 14
LAYOUT = (
    ("weapons", 781, 80),
    ("weaponPorts", 43, 82),
    ("specialWeapons", 47, 37),
    ("generators", 7, 37),
    ("ships", 14, 41),
    ("sidekicks", 31, 86),
    ("shields", 11, 37),
    ("enemies", 851, 77),
)


def item_database(episode: int, data_root: Path) -> bytes:
    if episode < 4:
        source = (data_root / "tyrian.hdt").read_bytes()
        offset = struct.unpack_from("<I", source, 0)[0]
    else:
        source = (data_root / "tyrian4.lvl").read_bytes()
        count = struct.unpack_from("<H", source, 0)[0]
        offset = struct.unpack_from("<I", source, 2 + (count - 1) * 4)[0]
    expected = COUNT_BYTES + sum(count * width for _, count, width in LAYOUT)
    block = source[offset:]
    if len(block) != expected:
        raise ValueError(
            f"Episode {episode}: item database is {len(block)} bytes, "
            f"expected {expected}"
        )
    return block


def decode_name(record: bytes, item_id: int) -> str:
    length = min(record[0], 30)
    name = record[1 : 1 + length].decode("cp437", errors="replace").strip()
    if item_id == 0 and not name:
        return "None"
    return name or f"(unused {item_id})"


def build_items(block: bytes) -> dict[str, list[dict[str, object]]]:
    cursor = COUNT_BYTES
    result: dict[str, list[dict[str, object]]] = {}
    for category, count, width in LAYOUT:
        records: list[dict[str, object]] = []
        for item_id in range(count):
            record = block[cursor + item_id * width : cursor + (item_id + 1) * width]
            if category == "enemies" or category == "weapons":
                continue
            item: dict[str, object] = {
                "id": item_id,
                "name": decode_name(record, item_id),
            }
            if category == "weaponPorts":
                item["cost"] = struct.unpack_from("<H", record, 76)[0]
            elif category == "generators":
                item["cost"] = struct.unpack_from("<H", record, 35)[0]
            elif category == "ships":
                item["armor"] = record[37]
                item["cost"] = struct.unpack_from("<H", record, 38)[0]
            elif category == "sidekicks":
                item["cost"] = struct.unpack_from("<H", record, 34)[0]
            elif category == "shields":
                item["maximum"] = record[32]
                item["cost"] = struct.unpack_from("<H", record, 35)[0]
            records.append(item)
        if category not in ("enemies", "weapons"):
            result[category] = records
        cursor += count * width
    return result


def parse_jump(text: str) -> int | None:
    match = re.match(r"^\]J\s+(\d{3})\[", text)
    return int(match.group(1)) if match else None


def parse_level(text: str) -> tuple[int, str] | None:
    if not text.startswith("]L[") or len(text) < 22:
        return None
    try:
        return int(text[9:12]), text[13:22].strip()
    except ValueError:
        return None


def build_progress(episode: int, path: Path) -> list[dict[str, object]]:
    sections: dict[int, list[str]] = {}
    section = 0
    next_sections = {1}
    for record in read_records(path):
        if record.text.startswith("*"):
            section += 1
            sections.setdefault(section, [])
            continue
        sections.setdefault(section, []).append(record.text)
        parsed = parse_level(record.text)
        if parsed:
            next_sections.add(parsed[0])

    def first_route(section_number: int) -> str | None:
        for text in sections.get(section_number, []):
            if text.startswith(("]J", "]G", "]L", "]Q")):
                return text
        return None

    def level_names(section_number: int, seen: set[int]) -> list[str]:
        if section_number in seen or section_number not in sections:
            return []
        seen = set(seen)
        seen.add(section_number)
        directive = first_route(section_number)
        if directive is None:
            return []
        parsed_level = parse_level(directive)
        if parsed_level:
            return [parsed_level[1]]
        jump = parse_jump(directive)
        if jump is not None:
            return level_names(jump, seen)
        if directive.startswith("]G["):
            numbers = [int(value) for value in re.findall(r"\d+", directive)]
            if len(numbers) < 4:
                return []
            choice_count = numbers[1]
            names: list[str] = []
            for choice in range(choice_count):
                target_index = 3 + choice * 2
                if target_index < len(numbers):
                    names.extend(level_names(numbers[target_index], seen))
            return names
        if directive.startswith("]Q["):
            return ["Episode ending / secret route"]
        return []

    result = []
    for section_number in sorted(next_sections):
        names = list(dict.fromkeys(level_names(section_number, set())))
        label = " / ".join(names) if names else f"Section {section_number}"
        level_name = names[0][:10] if names else f"SEC {section_number}"[:10]
        result.append(
            {
                "mainSection": section_number,
                "label": label,
                "levelName": level_name,
            }
        )
    return result


def main() -> int:
    data_root = ROOT / "vendor" / "tyrian" / "data"
    episodes = []
    initial_cash = (10000, 15000, 20000, 30000)
    for episode in range(1, 5):
        items = build_items(item_database(episode, data_root))
        ship = items["ships"][1]
        shield = items["shields"][4]
        episodes.append(
            {
                "episode": episode,
                "initialCash": initial_cash[episode - 1],
                "defaultArmor": ship.get("armor", 10),
                "defaultShield": shield.get("maximum", 0),
                "defaultShieldMaximum": int(shield.get("maximum", 0)) * 2,
                "progress": build_progress(
                    episode, data_root / f"levels{episode}.dat"
                ),
                **items,
            }
        )

    output = Path(__file__).resolve().parents[1] / "Resources" / "catalog.json"
    output.write_text(
        json.dumps({"schema": 1, "episodes": episodes}, indent=2, ensure_ascii=False)
        + "\n",
        encoding="utf-8",
        newline="\n",
    )
    print(f"Save Editor catalog: {len(episodes)} episodes -> {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
