#!/usr/bin/env python3
"""GBA Maxmod music builder for project-local Tyrian TYM sources.

The builder writes Impulse Tracker modules directly for Maxmod.  It contains
no adapter policy, loop workaround, or data path from another console.
"""

from __future__ import annotations

import json
import struct
import zlib
from pathlib import Path
from typing import Callable

import numpy as np

TRACKER_TEMPO = 174
TRACKER_SPEED = 1
TRACKER_C5_SPEED = 16_744
MAX_GBA_MUSIC_VOICES = 9
PROCEDURAL_PERCUSSION_RATE = 15_768
PROCEDURAL_PERCUSSION_SOURCE_RATE = 11_025
TRACKER_CHANNEL_PANS = (23, 41, 28, 36, 26, 38, 32, 32, 32)


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
    count = len(samples)
    if not count or count > 255:
        raise ValueError(f"IT sample count out of range: {count}")
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
        count,
        count,
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
        count * 4 +
        count * 4 +
        len(patterns) * 4
    )
    cursor = 192 + table_size
    instruments: list[bytes] = []
    instrument_offsets: list[int] = []
    for sample_number, (sample_name, _, _, _, _) in enumerate(samples, start=1):
        instrument = bytearray(instrument_template)
        instrument[32:58] = sample_name.encode("ascii", "replace")[:26].ljust(26, b"\0")
        instrument[30] = 1
        for note in range(120):
            instrument[64 + note * 2] = note
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

def opl_wave(phase: np.ndarray, waveform: int) -> np.ndarray:
    phase = phase - np.floor(phase)
    sine = np.sin(phase * 2.0 * np.pi)
    mode = waveform & 3
    if mode == 1:
        return np.where(sine > 0, sine, 0.0)
    if mode == 2:
        return np.abs(sine) * 2.0 - 1.0
    if mode == 3:
        return np.where(
            phase < 0.25,
            np.sin(phase * 2.0 * np.pi),
            0.0,
        )
    return sine

def synthesize_tym_sample(
    instrument: bytes,
    source: int,
    instrument_index: int,
    percussion_sources: set[int],
    gain_scale: float,
) -> tuple[bytes, int, bool, int]:
    midi_instrument = instrument[40]
    percussion = midi_instrument >= 128 or source in percussion_sources
    if percussion:
        def percussion_length(source_length: int) -> int:
            return int(round(
                source_length *
                PROCEDURAL_PERCUSSION_RATE /
                PROCEDURAL_PERCUSSION_SOURCE_RATE
            ))

        drum_note = (
            midi_instrument - 128
            if midi_instrument >= 128
            else 35 + (instrument_index % 3) * 3
        )
        if drum_note in (35, 36):
            length = percussion_length(1024)
            rate = PROCEDURAL_PERCUSSION_RATE
            time = np.arange(length, dtype=np.float64) / rate
            phase = 2.0 * np.pi * (
                115.0 * time -
                52.0 * time * time
            )
            rng = np.random.default_rng(
                0x54594D + source * 257 + instrument_index
            )
            signal = (
                np.sin(phase) * np.exp(-time * 25.0) +
                rng.uniform(-1.0, 1.0, length) *
                np.exp(-time * 60.0) *
                0.16
            )
        elif drum_note in (38, 40):
            length = percussion_length(1536)
            rate = PROCEDURAL_PERCUSSION_RATE
            time = np.arange(length, dtype=np.float64) / rate
            rng = np.random.default_rng(
                0x534E52 + source * 257 + instrument_index
            )
            signal = (
                rng.uniform(-1.0, 1.0, length) *
                np.exp(-time * 22.0) *
                0.82 +
                np.sin(2.0 * np.pi * 185.0 * time) *
                np.exp(-time * 32.0) *
                0.28
            )
        else:
            rate = PROCEDURAL_PERCUSSION_RATE
            if drum_note in (42, 44):
                length = percussion_length(768)
                decay = 48.0
                difference = 0.78
            elif drum_note == 46:
                length = percussion_length(2048)
                decay = 16.0
                difference = 0.70
            else:
                # Open/crash/ride cymbals in the stock catalog are sparse,
                # long events.  Treating every one as a 46 ms closed hat
                # forced RMS calibration to amplify its transient by several
                # times (most visibly source 7 in track 41).
                length = percussion_length(4096)
                decay = 8.5
                difference = 0.62
            time = np.arange(length, dtype=np.float64) / rate
            rng = np.random.default_rng(
                0x484154 + source * 257 + instrument_index
            )
            noise = rng.uniform(-1.0, 1.0, length)
            signal = np.empty(length, dtype=np.float64)
            signal[0] = noise[0]
            signal[1:] = noise[1:] - noise[:-1] * difference
            signal *= np.exp(-time * decay)
        # The procedural one-shots are a fixed-reference PCM adapter, not a
        # new per-song master.  Remove residual DC and make both boundaries
        # meet silence so Maxmod retriggers/stops cannot create a click.
        dc_blocked = np.empty_like(signal)
        dc_blocked[0] = 0.0
        previous_input = float(signal[0])
        previous_output = 0.0
        for sample_index in range(1, length):
            current_input = float(signal[sample_index])
            current_output = (
                current_input -
                previous_input +
                0.995 * previous_output
            )
            dc_blocked[sample_index] = current_output
            previous_input = current_input
            previous_output = current_output
        signal = dc_blocked
        attack = min(length // 4, max(2, int(round(rate * 0.0015))))
        attack_phase = np.linspace(0.0, 1.0, attack, endpoint=True)
        signal[:attack] *= (
            attack_phase *
            attack_phase *
            (3.0 - 2.0 * attack_phase)
        )
        release = min(length // 4, max(2, int(round(rate * 0.005))))
        release_phase = np.linspace(1.0, 0.0, release, endpoint=True)
        signal[-release:] *= (
            release_phase *
            release_phase *
            (3.0 - 2.0 * release_phase)
        )
        peak = max(1e-9, float(np.max(np.abs(signal))))
        pcm = np.clip(
            np.rint(signal / peak * 118.0 * gain_scale),
            -128,
            127,
        ).astype(np.int8)
        return pcm.tobytes(), rate, False, 0

    phase = (np.arange(320, dtype=np.float64) % 64) / 64.0
    mod_level = 10.0 ** (-(instrument[1] & 0x3F) * 0.75 / 20.0)
    modulation = (
        0.2 +
        mod_level * (1.0 + (instrument[10] & 7) * 0.45)
    )
    modulator = opl_wave(phase, instrument[4])
    signal = opl_wave(
        phase + modulator * modulation / (2.0 * np.pi),
        instrument[9],
    )
    signal -= float(np.mean(signal))
    peak = max(1e-9, float(np.max(np.abs(signal))))
    pcm = np.clip(
        np.rint(signal / peak * 118.0 * gain_scale),
        -128,
        127,
    ).astype(np.int8)
    return pcm.tobytes(), TRACKER_C5_SPEED, True, 64

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
) -> tuple[bytes, dict[str, object]]:
    if (
        not 1 <= len(sources) <= MAX_GBA_MUSIC_VOICES
        or len(sources) != len(voice_gains)
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

    used_pairs: set[tuple[int, int]] = set()
    source_to_voice = {
        source: voice for voice, source in enumerate(sources)
    }
    for _, changes in events:
        for source, state in changes.items():
            if source in source_to_voice and state[0] != -32768:
                used_pairs.add((source_to_voice[source], state[1]))
    ordered_pairs = sorted(used_pairs)
    if len(ordered_pairs) > 255:
        raise ValueError("TYM tracker needs more than 255 voice/instrument pairs")
    pair_to_instrument = {
        pair: index + 1 for index, pair in enumerate(ordered_pairs)
    }

    maximum_gain = max(1.0, max(voice_gains))
    samples: list[tuple[str, bytes, int, bool, int]] = []
    for voice, instrument_index in ordered_pairs:
        source = sources[voice]
        pcm, rate, loop, loop_start = synthesize_tym_sample(
            instruments[instrument_index],
            source,
            instrument_index,
            percussion_sources,
            voice_gains[voice] / maximum_gain,
        )
        samples.append((
            f"v{voice}i{instrument_index:02d}",
            pcm,
            rate,
            loop,
            loop_start,
        ))

    absolute_cells: dict[int, dict[int, dict[str, int]]] = {}
    for tick, changes in events:
        for source, state in changes.items():
            voice = source_to_voice.get(source)
            if voice is None:
                continue
            pitch_q8, instrument_index, velocity, _ = state
            cell: dict[str, int] = {}
            if pitch_q8 == -32768:
                cell["note"] = 255
            else:
                percussion = (
                    instruments[instrument_index][40] >= 128 or
                    source in percussion_sources
                )
                cell["note"] = (
                    61
                    if percussion
                    else max(
                        1,
                        min(120, int(round(pitch_q8 / 256.0)) + 1),
                    )
                )
                cell["instrument"] = pair_to_instrument[
                    (voice, instrument_index)
                ]
                attenuation_db = (
                    63 - max(0, min(63, velocity))
                ) * 0.75
                cell["volume"] = max(
                    1,
                    min(
                        64,
                        int(round(
                            64.0 *
                            10.0 ** (-attenuation_db / 20.0)
                        )),
                    ),
                )
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
        "source_channels": ",".join(str(source) for source in sources),
        "it_bytes": len(module),
    }
    return module, report
