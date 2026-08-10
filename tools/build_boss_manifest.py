#!/usr/bin/env python3
"""Build a compact Boss-spawn manifest from the stock Tyrian LVL files.

Tyrian bosses are assembled by the ordinary enemy event interpreter.  Event
79 only enables a health bar, often long after the component spawn events
have run.  This tool finds the latest authored spawn cohort which leads to
each health-bar link and emits stable (episode, LVL number, event index)
keys.  The GBA runtime can therefore classify a component at spawn time,
without waiting for a visible health bar or guessing from current position.

The output is derived metadata only.  It does not copy graphics, enemy
definitions, or hand-authored per-level replacements.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


EVENT_RECORD = struct.Struct("<HBhhbbbB")
EVENT_RECORD_BYTES = EVENT_RECORD.size

# Number of JE_createNewEventEnemy() calls made by each spawn event.  A bit
# in the generated mask corresponds to enemy_type_offset 0..3.
SPAWN_MASK_BY_TYPE = {
    6: 0x01,
    7: 0x01,
    10: 0x01,
    12: 0x0F,
    15: 0x01,
    17: 0x01,
    18: 0x01,
    23: 0x01,
    32: 0x01,
    49: 0x01,
    50: 0x01,
    51: 0x01,
    52: 0x01,
    56: 0x01,
}

# Boss parts are normally authored as a dense spawn block.  Apply one
# project-wide construction window to every Episode/LVL pair; there are no
# per-level names, links, event indices, or enemy IDs in the classifier.
# The generated audit keeps the exact interval and source events reviewable.
COHORT_RADIUS_TICKS = 48

# These event types address an enemy group through eventdat4.  Identical
# commands at the same event time are useful audit evidence that several
# links form one authored assembly.  Classification remains anchored to the
# spawn cohort, so a shared movement command cannot pull old level enemies
# into the manifest by itself.
LINK_CONTROL_EVENT_TYPES = {
    19,
    20,
    24,
    25,
    27,
    31,
    33,
    45,
    47,
    55,
    60,
    74,
}


@dataclass(frozen=True)
class Event:
    index: int
    time: int
    event_type: int
    dat: int
    dat2: int
    dat3: int
    dat5: int
    dat6: int
    dat4: int

    @property
    def spawn_mask(self) -> int:
        return SPAWN_MASK_BY_TYPE.get(self.event_type, 0)


@dataclass(frozen=True)
class Level:
    episode: int
    number: int
    events: tuple[Event, ...]


@dataclass(frozen=True)
class Anchor:
    bar_link: int
    bar_event_index: int
    bar_time: int
    spawn_time: int
    aliases: tuple[int, ...]


@dataclass
class Cohort:
    start_time: int
    end_time: int
    first_bar_time: int
    anchors: list[Anchor]


@dataclass(frozen=True)
class ManifestEntry:
    episode: int
    level: int
    event_index: int
    spawn_mask: int

    @property
    def key(self) -> int:
        if not 1 <= self.episode <= 4:
            raise ValueError(f"episode does not fit manifest key: {self}")
        if not 1 <= self.level <= 0x0FFF:
            raise ValueError(f"LVL number does not fit manifest key: {self}")
        if not 0 <= self.event_index <= 0xFFFF:
            raise ValueError(f"event index does not fit manifest key: {self}")
        return (
            (self.episode << 28)
            | (self.level << 16)
            | self.event_index
        )


def read_u16(data: bytes, offset: int) -> int:
    if offset < 0 or offset + 2 > len(data):
        raise ValueError(f"u16 outside file at {offset}")
    return struct.unpack_from("<H", data, offset)[0]


def parse_level_file(path: Path, episode: int) -> list[Level]:
    data = path.read_bytes()
    offset_count = read_u16(data, 0)
    table_bytes = 2 + offset_count * 4
    if offset_count < 2 or table_bytes > len(data):
        raise ValueError(f"invalid LVL offset table: {path}")
    offsets = list(struct.unpack_from(f"<{offset_count}I", data, 2))
    if offsets != sorted(offsets):
        raise ValueError(f"non-monotonic LVL offsets: {path}")
    if offsets[-1] > len(data):
        raise ValueError(f"LVL offset outside file: {path}")

    levels: list[Level] = []
    # OpenTyrian selects offset (level_number - 1) * 2.  Episode 4's odd
    # final offset is its embedded item database and is not a playable LVL.
    for level_number in range(1, offset_count // 2 + 1):
        offset_index = (level_number - 1) * 2
        start = offsets[offset_index]
        end = (
            offsets[offset_index + 2]
            if offset_index + 2 < offset_count
            else len(data)
        )
        if start + 12 > end:
            raise ValueError(
                f"truncated EP{episode} LVL{level_number} header"
            )
        enemy_count = read_u16(data, start + 8)
        position = start + 10 + enemy_count * 2
        event_count = read_u16(data, position)
        position += 2
        event_bytes = event_count * EVENT_RECORD_BYTES
        if position + event_bytes > end:
            raise ValueError(
                f"truncated EP{episode} LVL{level_number} events"
            )
        events: list[Event] = []
        previous_time = 0
        for index in range(event_count):
            fields = EVENT_RECORD.unpack_from(
                data,
                position + index * EVENT_RECORD_BYTES,
            )
            event = Event(index, *fields)
            if index != 0 and event.time < previous_time:
                raise ValueError(
                    f"unsorted events in EP{episode} LVL{level_number}"
                )
            previous_time = event.time
            events.append(event)
        levels.append(Level(episode, level_number, tuple(events)))
    return levels


def alias_closure(
    events: tuple[Event, ...],
    link: int,
    before_event_index: int,
) -> set[int]:
    aliases = {link}
    changed = True
    while changed:
        changed = False
        for event in events[:before_event_index]:
            if event.event_type != 39:
                continue
            if event.dat in aliases or event.dat2 in aliases:
                previous_count = len(aliases)
                if event.dat != 0:
                    aliases.add(event.dat)
                if event.dat2 != 0:
                    aliases.add(event.dat2)
                changed = changed or len(aliases) != previous_count
    return aliases


def bar_activations(level: Level) -> list[tuple[int, Event]]:
    result: list[tuple[int, Event]] = []
    for event in level.events:
        if event.event_type != 79:
            continue
        for link in (event.dat, event.dat2):
            if link != 0:
                result.append((link, event))
    return result


def find_anchors(level: Level) -> tuple[list[Anchor], list[dict]]:
    anchors: list[Anchor] = []
    unresolved: list[dict] = []
    # A stock level may reuse the same bar link for two separate encounters
    # (Episode 2 LVL 7 uses link 254 twice).  Every Event 79 occurrence is an
    # anchor; overlapping occurrences naturally merge into one cohort.
    for link, bar_event in bar_activations(level):
        aliases = alias_closure(level.events, link, bar_event.index)
        candidates = [
            event
            for event in level.events[:bar_event.index]
            if event.spawn_mask != 0 and event.dat4 in aliases
        ]
        if not candidates:
            unresolved.append(
                {
                    "barLink": link,
                    "barEventIndex": bar_event.index,
                    "barTime": bar_event.time,
                    "aliases": sorted(aliases),
                }
            )
            continue
        latest_time = max(event.time for event in candidates)
        anchors.append(
            Anchor(
                bar_link=link,
                bar_event_index=bar_event.index,
                bar_time=bar_event.time,
                spawn_time=latest_time,
                aliases=tuple(sorted(aliases)),
            )
        )
    return anchors, unresolved


def merge_anchor_cohorts(anchors: Iterable[Anchor]) -> list[Cohort]:
    raw = sorted(
        (
            Cohort(
                max(0, anchor.spawn_time - COHORT_RADIUS_TICKS),
                anchor.spawn_time + COHORT_RADIUS_TICKS,
                anchor.bar_time,
                [anchor],
            )
            for anchor in anchors
        ),
        key=lambda cohort: (cohort.start_time, cohort.end_time),
    )
    merged: list[Cohort] = []
    for cohort in raw:
        if not merged or cohort.start_time > merged[-1].end_time:
            merged.append(cohort)
            continue
        current = merged[-1]
        current.end_time = max(current.end_time, cohort.end_time)
        current.first_bar_time = min(
            current.first_bar_time,
            cohort.first_bar_time,
        )
        current.anchors.extend(cohort.anchors)
    return merged


def control_link_groups(
    level: Level,
    cohort: Cohort,
) -> list[set[int]]:
    signatures: dict[tuple[int, ...], set[int]] = {}
    for event in level.events:
        if (
            event.event_type not in LINK_CONTROL_EVENT_TYPES
            or event.dat4 == 0
            or event.time < cohort.start_time
            or event.time > cohort.first_bar_time
        ):
            continue
        signature = (
            event.time,
            event.event_type,
            event.dat,
            event.dat2,
            event.dat3,
            event.dat5,
            event.dat6,
        )
        signatures.setdefault(signature, set()).add(event.dat4)
    return [links for links in signatures.values() if len(links) > 1]


def derive_level_entries(level: Level) -> tuple[list[ManifestEntry], dict]:
    anchors, unresolved = find_anchors(level)
    cohorts = merge_anchor_cohorts(anchors)
    selected: dict[int, int] = {}
    cohort_audit: list[dict] = []

    for cohort_id, cohort in enumerate(cohorts, 1):
        known_links: set[int] = set()
        for anchor in cohort.anchors:
            known_links.update(anchor.aliases)

        cohort_spawns = [
            event
            for event in level.events
            if (
                event.spawn_mask != 0
                and cohort.start_time <= event.time <= cohort.end_time
                and event.time <= cohort.first_bar_time
            )
        ]
        known_links.update(
            event.dat4 for event in cohort_spawns if event.dat4 != 0
        )

        # Record synchronized control evidence and close its link graph.  We
        # deliberately do not add arbitrary old spawn events for new links;
        # only members of the anchored construction-time cohort are emitted.
        groups = control_link_groups(level, cohort)
        changed = True
        while changed:
            changed = False
            for links in groups:
                if known_links.intersection(links):
                    previous_count = len(known_links)
                    known_links.update(links)
                    changed = changed or len(known_links) != previous_count

        for event in cohort_spawns:
            selected[event.index] = (
                selected.get(event.index, 0) | event.spawn_mask
            )

        cohort_audit.append(
            {
                "cohort": cohort_id,
                "startTime": cohort.start_time,
                "endTime": cohort.end_time,
                "firstBarTime": cohort.first_bar_time,
                "anchors": [
                    {
                        "barLink": anchor.bar_link,
                        "barEventIndex": anchor.bar_event_index,
                        "barTime": anchor.bar_time,
                        "spawnTime": anchor.spawn_time,
                        "aliases": list(anchor.aliases),
                    }
                    for anchor in sorted(
                        cohort.anchors,
                        key=lambda item: (item.spawn_time, item.bar_link),
                    )
                ],
                "confirmedLinks": sorted(known_links),
                "spawnEventIndices": [
                    event.index for event in cohort_spawns
                ],
            }
        )

    entries = [
        ManifestEntry(
            level.episode,
            level.number,
            event_index,
            spawn_mask,
        )
        for event_index, spawn_mask in sorted(selected.items())
    ]
    event_by_index = {event.index: event for event in level.events}
    audit = {
        "episode": level.episode,
        "level": level.number,
        "eventCount": len(level.events),
        "barEventCount": sum(
            event.event_type == 79 and (event.dat != 0 or event.dat2 != 0)
            for event in level.events
        ),
        "manifestSpawnCount": len(entries),
        "cohorts": cohort_audit,
        "unresolvedBarLinks": unresolved,
        "entries": [
            {
                "eventIndex": entry.event_index,
                "eventTime": event_by_index[entry.event_index].time,
                "eventType": event_by_index[entry.event_index].event_type,
                "enemyDefinition": event_by_index[entry.event_index].dat,
                "link": event_by_index[entry.event_index].dat4,
                "spawnMask": entry.spawn_mask,
            }
            for entry in entries
        ],
    }
    return entries, audit


def validate_manifest(
    levels: list[Level],
    entries: list[ManifestEntry],
) -> None:
    level_map = {(level.episode, level.number): level for level in levels}
    selected_by_level: dict[tuple[int, int], set[int]] = {}
    seen: set[int] = set()
    for entry in entries:
        if entry.key in seen:
            raise ValueError(f"duplicate manifest key 0x{entry.key:08x}")
        seen.add(entry.key)
        level = level_map[(entry.episode, entry.level)]
        event = level.events[entry.event_index]
        if event.spawn_mask == 0 or entry.spawn_mask != event.spawn_mask:
            raise ValueError(f"manifest points at a non-spawn event: {entry}")
        selected_by_level.setdefault(
            (entry.episode, entry.level),
            set(),
        ).add(entry.event_index)

    # Corpus-wide invariants: every authored non-zero Event 79 link must
    # resolve to a prior spawn anchor, and the latest matching spawn for each
    # activation must be present.  Conversely, levels without a bar cannot
    # acquire manifest entries.  These checks replace per-Boss fixtures and
    # run uniformly over every stock Episode/LVL pair.
    episodes_with_boss_data: set[int] = set()
    for level in levels:
        level_key = (level.episode, level.number)
        selected = selected_by_level.get(level_key, set())
        anchors, unresolved = find_anchors(level)

        if unresolved:
            raise ValueError(
                f"unresolved Event 79 links in EP{level.episode} "
                f"LVL{level.number}: {unresolved}"
            )
        if not anchors:
            if selected:
                raise ValueError(
                    "manifest entries in a level without Event 79: "
                    f"EP{level.episode} LVL{level.number}"
                )
            continue
        episodes_with_boss_data.add(level.episode)
        for anchor in anchors:
            latest_matching = {
                event.index
                for event in level.events[:anchor.bar_event_index]
                if (
                    event.spawn_mask != 0
                    and event.time == anchor.spawn_time
                    and event.dat4 in anchor.aliases
                )
            }
            if not latest_matching or not latest_matching.intersection(selected):
                raise ValueError(
                    "latest Boss spawn anchor missing from manifest: "
                    f"EP{level.episode} LVL{level.number} "
                    f"bar event {anchor.bar_event_index} link {anchor.bar_link}"
                )

    if episodes_with_boss_data != {1, 2, 3, 4}:
        raise ValueError(
            "stock Boss manifest does not cover all Episodes: "
            f"{sorted(episodes_with_boss_data)}"
        )


def write_header(path: Path, entries: list[ManifestEntry]) -> None:
    lines = [
        "/* Generated by tools/build_boss_manifest.py; do not edit. */",
        "#ifndef TYRIAN_GBA_BOSS_MANIFEST_H",
        "#define TYRIAN_GBA_BOSS_MANIFEST_H",
        "",
        "#include <stdint.h>",
        "",
        (
            "#define OT_BOSS_MANIFEST_KEY(episode, level, event_index) "
            "\\"
        ),
        (
            "    ((((uint32_t)(episode)) << 28) | "
            "(((uint32_t)(level)) << 16) | (uint32_t)(event_index))"
        ),
        f"#define OT_BOSS_MANIFEST_ENTRY_COUNT {len(entries)}u",
        "",
        (
            "static const uint32_t "
            "ot_boss_manifest_keys[OT_BOSS_MANIFEST_ENTRY_COUNT] = {"
        ),
    ]
    for entry in entries:
        lines.append(f"    0x{entry.key:08x}u,")
    lines.extend(
        [
            "};",
            "",
            (
                "static const uint8_t "
                "ot_boss_manifest_spawn_masks[OT_BOSS_MANIFEST_ENTRY_COUNT] = {"
            ),
        ]
    )
    for entry in entries:
        lines.append(f"    0x{entry.spawn_mask:02x}u,")
    lines.extend(["};", "", "#endif", ""])
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines), encoding="ascii", newline="\n")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data-root", type=Path, required=True)
    parser.add_argument("--output-header", type=Path, required=True)
    parser.add_argument("--audit", type=Path, required=True)
    args = parser.parse_args()

    levels: list[Level] = []
    source_hashes: dict[str, str] = {}
    for episode in range(1, 5):
        path = args.data_root / f"tyrian{episode}.lvl"
        source_hashes[path.name] = hashlib.sha256(path.read_bytes()).hexdigest()
        levels.extend(parse_level_file(path, episode))

    entries: list[ManifestEntry] = []
    level_audits: list[dict] = []
    for level in levels:
        level_entries, level_audit = derive_level_entries(level)
        entries.extend(level_entries)
        if level_audit["barEventCount"] != 0:
            level_audits.append(level_audit)
    entries.sort(key=lambda entry: entry.key)
    validate_manifest(levels, entries)
    write_header(args.output_header, entries)

    unresolved_count = sum(
        len(level["unresolvedBarLinks"]) for level in level_audits
    )
    audit = {
        "format": "TyrianGbaPoc Boss spawn manifest v1",
        "method": "event79 latest-spawn cohorts",
        "cohortRadiusTicks": COHORT_RADIUS_TICKS,
        "eventRecordBytes": EVENT_RECORD_BYTES,
        "sourceSha256": source_hashes,
        "levelCount": len(levels),
        "bossLevelCount": len(level_audits),
        "entryCount": len(entries),
        "unresolvedBarLinkCount": unresolved_count,
        "levels": level_audits,
    }
    args.audit.parent.mkdir(parents=True, exist_ok=True)
    args.audit.write_text(
        json.dumps(audit, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    print(
        "Boss manifest: "
        f"{len(entries)} spawn events across {len(level_audits)} levels; "
        f"{unresolved_count} unresolved bar links"
    )


if __name__ == "__main__":
    main()
