#!/usr/bin/env python3
"""GBA Maxmod music builder for project-local Tyrian TYM sources.

The builder writes Impulse Tracker modules directly for Maxmod.  It contains
no adapter policy, loop workaround, or data path from another console.
"""

from __future__ import annotations

import json
import struct
import zlib
from collections import Counter
from dataclasses import dataclass
from pathlib import Path
from typing import Callable

import numpy as np

import opl_sample_renderer

TRACKER_TEMPO = 174
TRACKER_SPEED = 1
MAXMOD_OUTPUT_RATE = 15_768
TRACKER_C5_SPEED = MAXMOD_OUTPUT_RATE
MAX_GBA_MUSIC_VOICES = 9
OPL_SAMPLE_RATE = MAXMOD_OUTPUT_RATE
TRACKER_CHANNEL_PANS = (23, 41, 28, 36, 26, 38, 32, 32, 32)


@dataclass(frozen=True)
class ItInstrumentMap:
    name: str
    note_map: tuple[int, ...]
    sample_map: tuple[int, ...]

    def __post_init__(self) -> None:
        if len(self.note_map) != 120 or len(self.sample_map) != 120:
            raise ValueError("IT instrument maps must contain 120 notes")


def read_it_templates(workspace: Path) -> tuple[bytearray, bytearray, bytes]:
    del workspace
    path = Path(__file__).with_name("templates") / "gba_maxmod_base.it"
    data = path.read_bytes()
    order_count, instrument_count, sample_count, _ = struct.unpack_from("<4H", data, 32)
    offset = 192 + order_count
    instrument_offsets = struct.unpack_from(f"<{instrument_count}I", data, offset)
    offset += instrument_count * 4
    sample_offsets = struct.unpack_from(f"<{sample_count}I", data, offset)
    first_instrument = data[instrument_offsets[0] : instrument_offsets[1]]
    first_sample_header = data[sample_offsets[0] : sample_offsets[0] + 80]
    return bytearray(data[:192]), bytearray(first_instrument), first_sample_header


def build_it_module(
    workspace: Path,
    name: str,
    samples: list[tuple[str, bytes, int, bool, int]],
    patterns: list[tuple[int, bytes]],
    orders: list[int],
    speed: int = 6,
    tempo: int = 125,
    channel_pans: list[int] | None = None,
    instrument_maps: list[ItInstrumentMap] | None = None,
) -> bytes:
    header, instrument_template, sample_template = read_it_templates(workspace)
    active_channel_count = (
        len(channel_pans)
        if channel_pans is not None
        else MAX_GBA_MUSIC_VOICES
    )
    if not 1 <= active_channel_count <= 64:
        raise ValueError(
            f"IT active channel count out of range: {active_channel_count}"
        )
    sample_count = len(samples)
    if not sample_count or sample_count > 255:
        raise ValueError(f"IT sample count out of range: {sample_count}")
    if instrument_maps is None:
        instrument_maps = []
        for sample_number, (sample_name, _, _, _, _) in enumerate(
            samples,
            start=1,
        ):
            instrument_maps.append(ItInstrumentMap(
                sample_name,
                tuple(range(120)),
                (sample_number,) * 120,
            ))
    instrument_count = len(instrument_maps)
    if not instrument_count or instrument_count > 255:
        raise ValueError(
            f"IT instrument count out of range: {instrument_count}"
        )
    if not patterns or len(patterns) > 200:
        raise ValueError(f"IT pattern count out of range: {len(patterns)}")
    if not orders or len(orders) > 200:
        raise ValueError(f"IT order count out of range: {len(orders)}")
    header[4:30] = name.encode("ascii", "replace")[:26].ljust(26, b"\0")
    struct.pack_into(
        "<4H",
        header,
        32,
        len(orders),
        instrument_count,
        sample_count,
        len(patterns),
    )
    struct.pack_into("<H", header, 46, 0)
    header[48] = 128
    header[49] = 64
    header[50] = speed
    header[51] = tempo
    struct.pack_into("<HI", header, 54, 0, 0)
    for channel in range(64):
        header[64 + channel] = (
            channel_pans[channel]
            if channel_pans is not None and channel < len(channel_pans)
            else (32 if channel < active_channel_count else 128)
        )
        header[128 + channel] = (
            64 if channel < active_channel_count else 0
        )

    table_size = (
        len(orders) +
        instrument_count * 4 +
        sample_count * 4 +
        len(patterns) * 4
    )
    cursor = 192 + table_size
    instruments: list[bytes] = []
    instrument_offsets: list[int] = []
    for instrument_map in instrument_maps:
        instrument = bytearray(instrument_template)
        instrument[32:58] = (
            instrument_map.name.encode("ascii", "replace")[:26]
            .ljust(26, b"\0")
        )
        instrument[30] = 1
        for note in range(120):
            mapped_note = int(instrument_map.note_map[note])
            sample_number = int(instrument_map.sample_map[note])
            if not 0 <= mapped_note < 120:
                raise ValueError(f"IT mapped note out of range: {mapped_note}")
            if not 1 <= sample_number <= sample_count:
                raise ValueError(
                    f"IT mapped sample out of range: {sample_number}"
                )
            instrument[64 + note * 2] = mapped_note
            instrument[64 + note * 2 + 1] = sample_number
        instrument_offsets.append(cursor)
        instruments.append(bytes(instrument))
        cursor += len(instrument)

    sample_headers: list[bytearray] = []
    sample_offsets: list[int] = []
    for sample_name, pcm, rate, loop, loop_start in samples:
        sample_offsets.append(cursor)
        sample_header = bytearray(sample_template)
        sample_header[4:16] = sample_name.encode("ascii", "replace")[:12].ljust(12, b" ")
        sample_header[20:46] = sample_name.encode("ascii", "replace")[:26].ljust(26, b"\0")
        sample_header[17] = 64
        sample_header[18] = 0x11 if loop else 0x01
        sample_header[19] = 64
        sample_header[46] = 1
        sample_header[47] = 32
        struct.pack_into(
            "<IIIIII",
            sample_header,
            48,
            len(pcm),
            loop_start if loop else 0,
            len(pcm) if loop else 0,
            rate,
            0,
            0,
        )
        sample_headers.append(sample_header)
        cursor += 80

    encoded_patterns: list[bytes] = []
    pattern_offsets: list[int] = []
    for rows, packed_pattern in patterns:
        if rows < 1 or rows > 200:
            raise ValueError(f"IT pattern row count out of range: {rows}")
        pattern = (
            struct.pack("<HHI", len(packed_pattern), rows, 0) +
            packed_pattern
        )
        pattern_offsets.append(cursor)
        encoded_patterns.append(pattern)
        cursor += len(pattern)

    sample_data_offsets: list[int] = []
    for _, pcm, _, _, _ in samples:
        sample_data_offsets.append(cursor)
        cursor += len(pcm)
    for sample_header, pointer in zip(sample_headers, sample_data_offsets, strict=True):
        struct.pack_into("<I", sample_header, 72, pointer)

    output = bytearray(header)
    output.extend(bytes(orders))
    for pointer in instrument_offsets:
        output.extend(struct.pack("<I", pointer))
    for pointer in sample_offsets:
        output.extend(struct.pack("<I", pointer))
    for pointer in pattern_offsets:
        output.extend(struct.pack("<I", pointer))
    for instrument in instruments:
        output.extend(instrument)
    for sample_header in sample_headers:
        output.extend(sample_header)
    for pattern in encoded_patterns:
        output.extend(pattern)
    for _, pcm, _, _, _ in samples:
        output.extend(pcm)
    if len(output) != cursor:
        raise AssertionError(f"IT layout mismatch: {len(output)} != {cursor}")
    return bytes(output)

def parse_tym(path: Path) -> dict[str, object]:
    data = path.read_bytes()
    if len(data) < 16 or data[:4] != b"TYM1":
        raise ValueError(f"not a TYM1 file: {path}")
    header_size = struct.unpack_from("<H", data, 6)[0]
    chunk_count = struct.unpack_from("<I", data, 12)[0]
    chunks: dict[str, bytes] = {}
    offset = header_size
    for _ in range(chunk_count):
        if offset > len(data) - 16:
            raise ValueError(f"truncated TYM chunk header: {path}")
        chunk_id = data[offset : offset + 4].decode("ascii")
        size, expected_crc, reserved = struct.unpack_from("<III", data, offset + 4)
        offset += 16
        payload = data[offset : offset + size]
        if reserved or len(payload) != size:
            raise ValueError(f"invalid TYM chunk {chunk_id}: {path}")
        if zlib.crc32(payload) & 0xFFFFFFFF != expected_crc:
            raise ValueError(f"TYM chunk CRC mismatch: {chunk_id}")
        chunks[chunk_id] = payload
        offset += size + (-size & 3)
    if offset != len(data):
        raise ValueError(f"unexpected trailing TYM bytes: {path}")

    instrument_data = chunks["INST"]
    instrument_count, instrument_size = struct.unpack_from(
        "<HH", instrument_data, 0
    )
    if instrument_size != 46:
        raise ValueError("unsupported TYM instrument record size")
    instruments = [
        instrument_data[
            4 + index * instrument_size :
            4 + (index + 1) * instrument_size
        ]
        for index in range(instrument_count)
    ]

    event_data = chunks["EVNT"]
    numerator, denominator, duration, loop_start, event_count = (
        struct.unpack_from("<5I", event_data, 0)
    )
    position = 20
    events: list[tuple[int, dict[int, tuple[int, int, int, int]]]] = []
    for _ in range(event_count):
        tick, mask, reserved = struct.unpack_from("<IHH", event_data, position)
        position += 8
        if reserved:
            raise ValueError("invalid TYM event reserved field")
        changes: dict[int, tuple[int, int, int, int]] = {}
        for channel in range(9):
            if mask & (1 << channel):
                changes[channel] = struct.unpack_from(
                    "<hBBH", event_data, position
                )
                position += 6
        events.append((tick, changes))
    if position != len(event_data):
        raise ValueError("unexpected trailing TYM event bytes")
    return {
        "metadata": json.loads(chunks["META"]),
        "instruments": instruments,
        "events": events,
        "numerator": numerator,
        "denominator": denominator,
        "duration": duration,
        "loop_start": loop_start,
    }

def is_percussion_patch(
    instrument: bytes,
    source: int,
    percussion_sources: set[int],
) -> bool:
    return instrument[40] >= 128 or source in percussion_sources


def choose_adaptive_roots(
    note_counts: Counter[int],
    percussion: bool,
) -> tuple[int, ...]:
    """Choose one to three span-bounded roots without song-specific guesses."""
    del percussion
    if not note_counts:
        return (60,)
    notes = sorted(note_counts)
    span = notes[-1] - notes[0]
    if span <= 10:
        count = 1
    elif span <= 24:
        count = 2
    else:
        count = 3
    count = min(count, len(notes))
    if count == 1:
        midpoint = (notes[0] + notes[-1]) / 2.0
        roots = [min(
            notes,
            key=lambda candidate: (
                max(abs(note - candidate) for note in notes),
                sum(
                    abs(note - candidate) * note_counts[note]
                    for note in notes
                ),
                abs(candidate - midpoint),
                candidate,
            ),
        )]
    else:
        targets = [
            notes[0] + span * index / (count - 1)
            for index in range(count)
        ]
        roots = []
        for target in targets:
            root = min(
                (note for note in notes if note not in roots),
                key=lambda note: (abs(note - target), -note_counts[note], note),
            )
            roots.append(root)
        roots.sort()
    return tuple(roots)


def collect_pair_note_counts(
    song: dict[str, object],
    sources: set[int] | None = None,
) -> dict[tuple[int, int], Counter[int]]:
    events = song["events"]
    if not isinstance(events, list):
        raise TypeError("invalid TYM event collection")
    result: dict[tuple[int, int], Counter[int]] = {}
    active_generation: dict[int, int | None] = {}
    for _, changes in events:
        for source, state in changes.items():
            if sources is not None and source not in sources:
                continue
            if state[0] == -32768:
                active_generation[source] = None
                continue
            generation = int(state[3])
            if active_generation.get(source) == generation:
                continue
            active_generation[source] = generation
            note = max(0, min(119, int(round(state[0] / 256.0))))
            result.setdefault((source, state[1]), Counter())[note] += 1
    return result


def _render_pair_zones(
    instrument: bytes,
    roots: tuple[int, ...],
    percussion: bool,
) -> tuple[opl_sample_renderer.OplRenderedSample, ...]:
    return tuple(
        opl_sample_renderer.render_opl_patch(
            instrument,
            root * 256,
            percussion,
        )
        for root in roots
    )


def _quantize_opl_sample(
    rendered: opl_sample_renderer.OplRenderedSample,
    shared_peak: float,
    gain_scale: float,
) -> tuple[bytes, int, bool, int]:
    if shared_peak <= 0.0:
        raise ValueError("OPL patch rendered silence")
    pcm = np.clip(
        np.rint(rendered.signal / shared_peak * 118.0 * gain_scale),
        -128,
        127,
    ).astype(np.int8)
    return (
        pcm.tobytes(),
        OPL_SAMPLE_RATE,
        rendered.loop,
        rendered.loop_start,
    )


def synthesize_tym_sample(
    instrument: bytes,
    source: int,
    instrument_index: int,
    percussion_sources: set[int],
    gain_scale: float,
) -> tuple[bytes, int, bool, int]:
    """Compatibility callback using a deterministic middle-C OPL root."""
    del instrument_index
    percussion = is_percussion_patch(
        instrument,
        source,
        percussion_sources,
    )
    rendered = opl_sample_renderer.render_opl_patch(
        instrument,
        60 * 256,
        percussion,
    )
    return _quantize_opl_sample(
        rendered,
        rendered.peak_before_quantize,
        gain_scale,
    )


def make_track_sample_synthesizer(
    song: dict[str, object],
) -> Callable[[bytes, int, int, set[int], float], tuple[bytes, int, bool, int]]:
    """Return the calibration view of the same adaptive OPL sample model."""
    instruments = song["instruments"]
    if not isinstance(instruments, list):
        raise TypeError("invalid TYM instrument collection")
    pair_notes = collect_pair_note_counts(song)
    rendered_cache: dict[
        tuple[int, int],
        tuple[
            tuple[int, ...],
            tuple[opl_sample_renderer.OplRenderedSample, ...],
            float,
        ],
    ] = {}

    def synthesize(
        instrument: bytes,
        source: int,
        instrument_index: int,
        percussion_sources: set[int],
        gain_scale: float,
    ) -> tuple[bytes, int, bool, int]:
        key = (source, instrument_index)
        cached = rendered_cache.get(key)
        if cached is None:
            counts = pair_notes.get(key, Counter({60: 1}))
            percussion = is_percussion_patch(
                instrument,
                source,
                percussion_sources,
            )
            roots = choose_adaptive_roots(counts, percussion)
            zones = _render_pair_zones(instrument, roots, percussion)
            shared_peak = max(
                zone.peak_before_quantize for zone in zones
            )
            cached = (roots, zones, shared_peak)
            rendered_cache[key] = cached
        roots, zones, shared_peak = cached
        counts = pair_notes.get(key, Counter({roots[0]: 1}))
        representative = max(
            range(len(roots)),
            key=lambda index: (
                sum(
                    count
                    for note, count in counts.items()
                    if min(roots, key=lambda root: (abs(note - root), root)) ==
                        roots[index]
                ),
                -index,
            ),
        )
        return _quantize_opl_sample(
            zones[representative],
            shared_peak,
            gain_scale,
        )

    return synthesize

def pack_it_pattern(
    rows: int,
    cells: dict[int, dict[int, dict[str, int]]],
) -> bytes:
    packed = bytearray()
    for row in range(rows):
        for channel in sorted(cells.get(row, {})):
            cell = cells[row][channel]
            mask = 0
            if "note" in cell:
                mask |= 0x01
            if "instrument" in cell:
                mask |= 0x02
            if "volume" in cell:
                mask |= 0x04
            if "effect" in cell:
                mask |= 0x08
            packed.extend((0x80 | (channel + 1), mask))
            if mask & 0x01:
                packed.append(cell["note"])
            if mask & 0x02:
                packed.append(cell["instrument"])
            if mask & 0x04:
                packed.append(cell["volume"])
            if mask & 0x08:
                packed.extend((cell["effect"], cell["parameter"]))
        packed.append(0)
    return bytes(packed)

def build_tym_tracker_it(
    workspace: Path,
    tym_path: Path,
    sources: list[int],
    voice_gains: list[float],
    module_builder: Callable[..., bytes] | None = None,
    voice_volume_gains: list[float] | None = None,
) -> tuple[bytes, dict[str, object]]:
    if voice_volume_gains is None:
        voice_volume_gains = [1.0] * len(sources)
    if (
        not 1 <= len(sources) <= MAX_GBA_MUSIC_VOICES
        or len(sources) != len(voice_gains)
        or len(sources) != len(voice_volume_gains)
        or len(set(sources)) != len(sources)
    ):
        raise ValueError(
            "GBA Maxmod voice map must contain "
            f"1..{MAX_GBA_MUSIC_VOICES} unique sources"
        )
    song = parse_tym(tym_path)
    metadata = song["metadata"]
    if not isinstance(metadata, dict):
        raise TypeError("TYM metadata must be an object")
    track_number = int(metadata["trackNumber"])
    instruments = song["instruments"]
    events = song["events"]
    if not isinstance(instruments, list) or not isinstance(events, list):
        raise TypeError("invalid parsed TYM collections")
    percussion_sources = {
        int(source)
        for source in metadata["arrangement"]["percussionSources"]
    }

    source_to_voice = {
        source: voice for voice, source in enumerate(sources)
    }
    pair_note_counts = collect_pair_note_counts(
        song,
        set(source_to_voice),
    )
    used_pairs = {
        (source_to_voice[source], instrument_index)
        for source, instrument_index in pair_note_counts
    }
    ordered_pairs = sorted(used_pairs)
    if len(ordered_pairs) > 255:
        raise ValueError("TYM tracker needs more than 255 voice/instrument pairs")
    pair_to_instrument = {
        pair: index + 1 for index, pair in enumerate(ordered_pairs)
    }

    samples: list[tuple[str, bytes, int, bool, int]] = []
    instrument_maps: list[ItInstrumentMap] = []
    tonal_zone_count = 0
    percussion_zone_count = 0
    hardware_lfo_zone_count = 0
    software_lfo_zone_count = 0
    maximum_loop_boundary_error = 0.0
    for voice, instrument_index in ordered_pairs:
        source = sources[voice]
        instrument = instruments[instrument_index]
        percussion = is_percussion_patch(
            instrument,
            source,
            percussion_sources,
        )
        roots = choose_adaptive_roots(
            pair_note_counts[(source, instrument_index)],
            percussion,
        )
        zones = _render_pair_zones(instrument, roots, percussion)
        shared_peak = max(zone.peak_before_quantize for zone in zones)
        sample_numbers: list[int] = []
        for root, zone in zip(roots, zones, strict=True):
            pcm, rate, loop, loop_start = _quantize_opl_sample(
                zone,
                shared_peak,
                voice_gains[voice],
            )
            samples.append((
                f"v{voice}i{instrument_index:02d}r{root:03d}",
                pcm,
                rate,
                loop,
                loop_start,
            ))
            sample_numbers.append(len(samples))
            if percussion:
                percussion_zone_count += 1
            else:
                tonal_zone_count += 1
            hardware_lfo_zone_count += int(zone.hardware_lfo)
            software_lfo_zone_count += int(zone.software_lfo)
            maximum_loop_boundary_error = max(
                maximum_loop_boundary_error,
                zone.loop_boundary_error,
            )
        note_map: list[int] = []
        sample_map: list[int] = []
        for note in range(120):
            zone_index = min(
                range(len(roots)),
                key=lambda index: (abs(note - roots[index]), roots[index]),
            )
            note_map.append(max(
                0,
                min(119, 60 + note - roots[zone_index]),
            ))
            sample_map.append(sample_numbers[zone_index])
        instrument_maps.append(ItInstrumentMap(
            f"v{voice}i{instrument_index:02d}",
            tuple(note_map),
            tuple(sample_map),
        ))

    absolute_cells: dict[int, dict[int, dict[str, int]]] = {}
    active_generations: dict[int, int | None] = {}
    active_velocities: dict[int, int] = {}
    for tick, changes in events:
        for source, state in changes.items():
            voice = source_to_voice.get(source)
            if voice is None:
                continue
            pitch_q8, instrument_index, velocity, _ = state
            cell: dict[str, int] = {}
            if pitch_q8 == -32768:
                if active_generations.get(source) is not None:
                    cell["note"] = 255
                active_generations[source] = None
                active_velocities.pop(source, None)
            else:
                generation = int(state[3])
                retrigger = active_generations.get(source) != generation
                attenuation_db = (
                    63 - max(0, min(63, velocity))
                ) * 0.75
                base_volume = max(
                    1,
                    min(
                        64,
                        int(round(
                            64.0 *
                            10.0 ** (-attenuation_db / 20.0)
                        )),
                    ),
                )
                mapped_volume = max(
                    1,
                    min(
                        64,
                        int(round(
                            base_volume * voice_volume_gains[voice]
                        )),
                    ),
                )
                if retrigger:
                    cell["note"] = max(
                        1,
                        min(120, int(round(pitch_q8 / 256.0)) + 1),
                    )
                    cell["instrument"] = pair_to_instrument[
                        (voice, instrument_index)
                    ]
                if retrigger or active_velocities.get(source) != velocity:
                    cell["volume"] = mapped_volume
                active_generations[source] = generation
                active_velocities[source] = velocity
            if cell:
                absolute_cells.setdefault(tick, {})[voice] = cell

    duration = int(song["duration"])
    loop_start = int(song["loop_start"])
    # Maxmod follows the authored Bxx loop.  Never duplicate compressed
    # patterns in the order list: doing so can retain channel-mask state on
    # a repeated pass (most visibly in stock track 18).
    unrolled_loops = 1
    rows_per_pattern = 64
    segments: list[tuple[int, int]] = []
    if loop_start:
        segments.append((0, loop_start))
    for start in range(loop_start, duration, rows_per_pattern):
        segments.append((start, min(duration, start + rows_per_pattern)))
    loop_order = 1 if loop_start else 0
    final_tick = duration - 1
    absolute_cells.setdefault(final_tick, {}).setdefault(0, {}).update({
        "effect": 2,  # IT Bxx: authored position jump for Maxmod
        "parameter": loop_order,
    })

    patterns: list[tuple[int, bytes]] = []
    for start, end in segments:
        local_cells = {
            tick - start: voices
            for tick, voices in absolute_cells.items()
            if start <= tick < end
        }
        patterns.append((
            end - start,
            pack_it_pattern(end - start, local_cells),
        ))
    orders = list(range(len(patterns)))
    pans = list(TRACKER_CHANNEL_PANS[:len(sources)])
    if module_builder is None:
        module_builder = build_it_module
    module = module_builder(
        workspace,
        f"Tyrian {track_number:02d} Full",
        samples,
        patterns,
        orders,
        TRACKER_SPEED,
        TRACKER_TEMPO,
        pans,
        instrument_maps,
    )
    source_rate = float(song["numerator"]) / float(song["denominator"])
    tracker_rate = TRACKER_TEMPO / (2.5 * TRACKER_SPEED)
    report: dict[str, object] = {
        "track_number": track_number,
        "title": metadata["title"],
        "source_ticks": duration,
        "loop_start_tick": loop_start,
        "source_tick_rate": source_rate,
        "source_duration_seconds": duration / source_rate,
        "tracker_row_rate": tracker_rate,
        "tracker_duration_seconds": duration / tracker_rate,
        "patterns": len(patterns),
        "orders": len(orders),
        "unrolled_loops": unrolled_loops,
        "module_play_seconds": (
            loop_start +
            (duration - loop_start) * unrolled_loops
        ) / tracker_rate,
        "samples": len(samples),
        "instruments": len(instrument_maps),
        "tonal_zones": tonal_zone_count,
        "percussion_zones": percussion_zone_count,
        "sample_pcm_bytes": sum(len(sample[1]) for sample in samples),
        "hardware_lfo_zones": hardware_lfo_zone_count,
        "software_lfo_zones": software_lfo_zone_count,
        "maximum_loop_boundary_error": maximum_loop_boundary_error,
        "maximum_event_volume_gain": max(voice_volume_gains),
        "source_channels": ",".join(str(source) for source in sources),
        "it_bytes": len(module),
    }
    return module, report
