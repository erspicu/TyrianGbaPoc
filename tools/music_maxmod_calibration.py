#!/usr/bin/env python3
"""Calibrate Tyrian's TYM voices for the GBA Maxmod IT adapter.

The project-local GBA OPL reference measures every original OPL2 stem over
one complete loop.  This module retains all nine original voices and orders
them by authored percussion priority and source RMS solely for stable tracker
channel placement.  No other console's voice map or mixer gain is used as an
input.

This module measures the actual synthesized PCM/IT event model, solves one
fixed gain per selected OPL source, and verifies the quantized 8-bit result.
The comparison is deliberately made after a mono fold-down: Tyrian's OPL2
reference is mono, Maxmod's linear pan has L+R == 1, and a real GBA has a mono
internal speaker.  Stereo pan remains intact in the emitted IT module.
"""

from __future__ import annotations

import json
import math
from pathlib import Path
from typing import Callable

import numpy as np


PROFILE_NAME = "GbaMaxmod"
PROFILE_DESCRIPTION = (
    "GBA Maxmod 15.768 kHz native-rate tonal/percussion IT adapter; exact "
    "TYM event-volume timeline, "
    "quantized signed 8-bit synthesized samples, fixed OPL stem RMS target, "
    "mono L+R reference, one catalog-wide +3 dB presentation gain, and no "
    "per-song maximum-gain normalization; all nine original OPL2 source "
    "channels retained, with source-RMS ordering used only for tracker voices"
)
MAXMOD_OUTPUT_RATE = 15_768
MAXMOD_MODULE_VOLUME = 896
MAXMOD_NORMAL_VOLUME = 1024
MAXMOD_MODULE_SCALE = MAXMOD_MODULE_VOLUME / MAXMOD_NORMAL_VOLUME
# A single catalog-wide +3 dB presentation lift keeps the calibrated music
# practical beside Maxmod SFX without recreating the old per-song loudness
# drift.  It is part of the declared target, not normalization.
PLAYBACK_REFERENCE_GAIN_DB = 3.0
PLAYBACK_REFERENCE_GAIN = 10.0 ** (PLAYBACK_REFERENCE_GAIN_DB / 20.0)
IT_CHANNEL_PANS = (23, 41, 28, 36, 26, 38, 32, 32, 32)
MAX_GBA_MUSIC_VOICES = 9
MAX_SAMPLE_GAIN = 1.075
GAIN_REFINEMENT_PASSES = 3
ERROR_LIMIT_DB = 1.0
PERCUSSION_PEAK_CEILING_RATIO = 1.60

Song = dict[str, object]
SampleSynth = Callable[
    [bytes, int, int, set[int], float],
    tuple[bytes, int, bool, int],
]


def it_volume_from_tym_velocity(velocity: int) -> int:
    """Mirror the TYM -> IT volume-column quantizer used by the asset writer."""
    attenuation_db = (63 - max(0, min(63, velocity))) * 0.75
    return max(
        1,
        min(
            64,
            int(round(64.0 * 10.0 ** (-attenuation_db / 20.0))),
        ),
    )


def _db_ratio(actual: float, target: float) -> float:
    if actual <= 0.0 and target <= 0.0:
        return 0.0
    if actual <= 0.0 or target <= 0.0:
        return -120.0
    return 20.0 * math.log10(actual / target)


def _voice_metrics(
    song: Song,
    source: int,
    percussion_sources: set[int],
    gain: float,
    synthesize: SampleSynth,
) -> dict[str, float | int]:
    """Measure one source over the complete tracker pass.

    Tonal samples are periodic, so their mean-square value is invariant under
    pitch transposition.  Percussion is one-shot and is integrated only until
    the next source event, matching tracker retrigger/note-off semantics.
    """
    instruments = song["instruments"]
    events = song["events"]
    if not isinstance(instruments, list) or not isinstance(events, list):
        raise TypeError("invalid parsed TYM collections")
    duration = int(song["duration"])
    row_rate = 174.0 / 2.5
    total_seconds = duration / row_rate
    if duration <= 0 or total_seconds <= 0.0:
        raise ValueError("TYM duration must be positive")

    transitions = [
        (int(tick), changes[source])
        for tick, changes in events
        if source in changes
    ]
    sample_cache: dict[
        int,
        tuple[np.ndarray, int, bool],
    ] = {}

    def sample_for(instrument_index: int) -> tuple[np.ndarray, int, bool]:
        cached = sample_cache.get(instrument_index)
        if cached is not None:
            return cached
        if not 0 <= instrument_index < len(instruments):
            raise ValueError(
                f"TYM instrument {instrument_index} is outside the table"
            )
        pcm, rate, loop, _ = synthesize(
            instruments[instrument_index],
            source,
            instrument_index,
            percussion_sources,
            gain,
        )
        signed = (
            np.frombuffer(pcm, dtype=np.int8)
            .astype(np.float64) /
            128.0
        )
        cached = (signed, int(rate), bool(loop))
        sample_cache[instrument_index] = cached
        return cached

    energy_seconds = 0.0
    peak = 0.0
    active_seconds = 0.0
    note_count = 0
    current: tuple[int, int, int, int] | None = None
    current_tick = 0

    def integrate(
        state: tuple[int, int, int, int] | None,
        begin_tick: int,
        end_tick: int,
    ) -> None:
        nonlocal energy_seconds, peak, active_seconds
        if state is None or end_tick <= begin_tick:
            return
        pitch_q8, instrument_index, velocity, _ = state
        if pitch_q8 == -32768:
            return
        pcm, rate, loop = sample_for(instrument_index)
        if pcm.size == 0 or rate <= 0:
            return
        volume = it_volume_from_tym_velocity(velocity) / 64.0
        interval_seconds = (end_tick - begin_tick) / row_rate
        if loop:
            mean_square = float(np.mean(np.square(pcm)))
            energy_seconds += mean_square * volume * volume * interval_seconds
            peak = max(peak, float(np.max(np.abs(pcm))) * volume)
            active_seconds += interval_seconds
            return

        sample_count = min(
            pcm.size,
            max(0, int(round(interval_seconds * rate))),
        )
        if sample_count == 0:
            return
        window = pcm[:sample_count]
        energy_seconds += (
            float(np.sum(np.square(window))) /
            rate *
            volume *
            volume
        )
        peak = max(peak, float(np.max(np.abs(window))) * volume)
        active_seconds += sample_count / rate

    for tick, state in transitions:
        tick = max(0, min(duration, tick))
        integrate(current, current_tick, tick)
        current = state
        current_tick = tick
        if state[0] != -32768:
            note_count += 1
    integrate(current, current_tick, duration)

    rms = math.sqrt(max(0.0, energy_seconds) / total_seconds)
    sample_peak = max(
        (
            float(np.max(np.abs(sample)))
            for sample, _, _ in sample_cache.values()
            if sample.size
        ),
        default=0.0,
    )
    clipped_samples = sum(
        int(np.count_nonzero(np.abs(sample) >= 1.0))
        for sample, _, _ in sample_cache.values()
    )
    return {
        "rms": rms,
        "peak": peak,
        "sample_peak": sample_peak,
        "sample_count": len(sample_cache),
        "clipped_samples": clipped_samples,
        "active_seconds": active_seconds,
        "note_count": note_count,
    }


def _load_track(
    calibration_path: Path,
    track_number: int,
) -> dict[str, object]:
    catalog = json.loads(calibration_path.read_text(encoding="utf-8"))
    return next(
        track
        for track in catalog["tracks"]
        if int(track["trackNumber"]) == track_number
    )


def calibrate_track(
    song: Song,
    calibration_path: Path,
    synthesize: SampleSynth,
) -> dict[str, object]:
    """Return fixed Maxmod gains and an old/new objective audit."""
    metadata = song["metadata"]
    if not isinstance(metadata, dict):
        raise TypeError("TYM metadata must be an object")
    track_number = int(metadata["trackNumber"])
    reference = _load_track(calibration_path, track_number)
    percussion_sources = {
        int(source)
        for source in metadata["arrangement"]["percussionSources"]
    }
    events = song["events"]
    if not isinstance(events, list):
        raise TypeError("TYM event collection must be a list")
    reference_rms = [
        float(value) for value in reference["originalChannelRms"]
    ]
    active_sources = {
        int(source)
        for _, changes in events
        for source, state in changes.items()
        if state[0] != -32768 and reference_rms[int(source)] > 0.0
    }
    required = sorted(
        active_sources.intersection(percussion_sources),
        key=lambda source: (-reference_rms[source], source),
    )
    if len(required) > MAX_GBA_MUSIC_VOICES:
        required = required[:MAX_GBA_MUSIC_VOICES]
    tonal = sorted(
        active_sources.difference(required),
        key=lambda source: (-reference_rms[source], source),
    )
    selected = set(
        required +
        tonal[: MAX_GBA_MUSIC_VOICES - len(required)]
    )
    sources = sorted(
        selected,
        key=lambda source: (-reference_rms[source], source),
    )
    if not 1 <= len(sources) <= MAX_GBA_MUSIC_VOICES:
        raise ValueError(
            f"track {track_number} has no usable nine-voice mapping"
        )
    # The comparison baseline is the former GBA Maxmod unity adapter, not a
    # profile, gain table, or mixer model from another console.
    legacy_gains = [1.0] * len(sources)

    source_reports: list[dict[str, object]] = []
    gains: list[float] = []
    old_abs_errors: list[float] = []
    new_abs_errors: list[float] = []
    target_rss_square = 0.0
    old_rss_square = 0.0
    new_rss_square = 0.0
    target_peak_bound = 0.0
    old_peak_bound = 0.0
    new_peak_bound = 0.0

    for voice, source in enumerate(sources):
        reference_rms = float(reference["originalChannelRms"][source])
        reference_peak = float(reference["originalChannelPeak"][source])
        target_rms = reference_rms * PLAYBACK_REFERENCE_GAIN
        target_peak = reference_peak * PLAYBACK_REFERENCE_GAIN
        unity = _voice_metrics(
            song,
            source,
            percussion_sources,
            1.0,
            synthesize,
        )
        if target_rms <= 0.0 or float(unity["rms"]) <= 0.0:
            raise ValueError(
                f"track {track_number} source {source} has no RMS target"
            )

        # L+R is invariant under Maxmod's linear pan.  Include the actual
        # runtime module-volume ceiling so the played ROM, not merely the IT
        # file in isolation, matches the original OPL stem reference.
        gain = target_rms / (
            float(unity["rms"]) *
            MAXMOD_MODULE_SCALE
        )
        if gain > MAX_SAMPLE_GAIN:
            raise ValueError(
                f"track {track_number} source {source} needs gain "
                f"{gain:.6f}, above the clip-safe fixed reference"
            )
        realized: dict[str, float | int] = unity
        for _ in range(GAIN_REFINEMENT_PASSES):
            realized = _voice_metrics(
                song,
                source,
                percussion_sources,
                gain,
                synthesize,
            )
            projected_rms = (
                float(realized["rms"]) *
                MAXMOD_MODULE_SCALE
            )
            if projected_rms <= 0.0:
                raise ValueError(
                    f"track {track_number} source {source} quantized to zero"
                )
            correction = target_rms / projected_rms
            if abs(_db_ratio(projected_rms, target_rms)) <= 0.02:
                break
            gain = min(MAX_SAMPLE_GAIN, gain * correction)

        realized = _voice_metrics(
            song,
            source,
            percussion_sources,
            gain,
            synthesize,
        )
        peak_limited = False
        projected_peak = float(realized["peak"]) * MAXMOD_MODULE_SCALE
        if (
            source in percussion_sources and
            target_peak > 0.0 and
            projected_peak >
                target_peak * PERCUSSION_PEAK_CEILING_RATIO
        ):
            gain *= (
                target_peak *
                PERCUSSION_PEAK_CEILING_RATIO /
                projected_peak
            )
            realized = _voice_metrics(
                song,
                source,
                percussion_sources,
                gain,
                synthesize,
            )
            peak_limited = True
        legacy = _voice_metrics(
            song,
            source,
            percussion_sources,
            legacy_gains[voice],
            synthesize,
        )
        projected_rms = float(realized["rms"]) * MAXMOD_MODULE_SCALE
        projected_peak = float(realized["peak"]) * MAXMOD_MODULE_SCALE
        legacy_rms = float(legacy["rms"]) * MAXMOD_MODULE_SCALE
        legacy_peak = float(legacy["peak"]) * MAXMOD_MODULE_SCALE
        error_db = _db_ratio(projected_rms, target_rms)
        legacy_error_db = _db_ratio(legacy_rms, target_rms)
        if (
            not peak_limited and
            abs(error_db) > ERROR_LIMIT_DB
        ):
            raise ValueError(
                f"track {track_number} source {source} Maxmod RMS error "
                f"{error_db:+.3f} dB exceeds {ERROR_LIMIT_DB:.2f} dB"
            )
        if peak_limited and not -4.5 <= error_db <= 0.25:
            raise ValueError(
                f"track {track_number} source {source} peak-limited RMS "
                f"error {error_db:+.3f} dB is outside the safety objective"
            )
        if int(realized["clipped_samples"]) != 0:
            raise ValueError(
                f"track {track_number} source {source} clips synthesized PCM"
            )

        pan = IT_CHANNEL_PANS[voice]
        source_reports.append({
            "voice": voice,
            "sourceChannel": source,
            "pan": pan,
            "referenceRms": reference_rms,
            "referencePeak": reference_peak,
            "targetRms": target_rms,
            "targetPeak": target_peak,
            "unityAdapterRms": float(unity["rms"]),
            "gain": gain,
            "gainDb": 20.0 * math.log10(gain),
            "realizedRms": projected_rms,
            "realizedPeak": projected_peak,
            "peakRatio": (
                projected_peak / target_peak
                if target_peak > 0.0
                else 0.0
            ),
            "peakLimited": peak_limited,
            "errorDb": error_db,
            "legacyGain": legacy_gains[voice],
            "legacyRealizedRms": legacy_rms,
            "legacyRealizedPeak": legacy_peak,
            "legacyErrorDb": legacy_error_db,
            "samplePeak": float(realized["sample_peak"]),
            "sampleCount": int(realized["sample_count"]),
            "clippedSamples": int(realized["clipped_samples"]),
            "activeSeconds": float(realized["active_seconds"]),
            "noteCount": int(realized["note_count"]),
            "percussion": source in percussion_sources,
        })
        gains.append(gain)
        old_abs_errors.append(abs(legacy_error_db))
        new_abs_errors.append(abs(error_db))
        target_rss_square += target_rms * target_rms
        old_rss_square += legacy_rms * legacy_rms
        new_rss_square += projected_rms * projected_rms
        target_peak_bound += target_peak
        old_peak_bound += legacy_peak
        new_peak_bound += projected_peak

    return {
        "trackNumber": track_number,
        "title": str(reference["title"]),
        "profile": PROFILE_NAME,
        "sourceChannels": sources,
        "gains": gains,
        "gainDb": [20.0 * math.log10(gain) for gain in gains],
        "moduleVolume": MAXMOD_MODULE_VOLUME,
        "moduleScale": MAXMOD_MODULE_SCALE,
        "playbackReferenceGainDb": PLAYBACK_REFERENCE_GAIN_DB,
        "referenceFoldDown": "mono L+R",
        "sourceReports": source_reports,
        "targetRssRms": math.sqrt(target_rss_square),
        "legacyRssRms": math.sqrt(old_rss_square),
        "calibratedRssRms": math.sqrt(new_rss_square),
        "legacyMeanAbsoluteErrorDb": sum(old_abs_errors) / len(old_abs_errors),
        "calibratedMeanAbsoluteErrorDb": sum(new_abs_errors) / len(new_abs_errors),
        "targetPeakSumBound": target_peak_bound,
        "legacyPeakSumBound": old_peak_bound,
        "calibratedPeakSumBound": new_peak_bound,
    }


def write_catalog(
    path: Path,
    tracks: list[dict[str, object]],
) -> dict[str, object]:
    source_reports = [
        source
        for track in tracks
        for source in track["sourceReports"]
    ]
    catalog: dict[str, object] = {
        "schema": "tyrian-gba-maxmod-calibration-v1",
        "profile": PROFILE_NAME,
        "description": PROFILE_DESCRIPTION,
        "maxmodOutputRate": MAXMOD_OUTPUT_RATE,
        "tonalPcmRate": MAXMOD_OUTPUT_RATE,
        "proceduralPercussionRate": MAXMOD_OUTPUT_RATE,
        "maxmodModuleVolume": MAXMOD_MODULE_VOLUME,
        "maxmodNormalVolume": MAXMOD_NORMAL_VOLUME,
        "playbackReferenceGainDb": PLAYBACK_REFERENCE_GAIN_DB,
        "referenceFoldDown": "mono L+R",
        "tracks": tracks,
        "summary": {
            "trackCount": len(tracks),
            "sourceCount": len(source_reports),
            "gainMin": min(float(source["gain"]) for source in source_reports),
            "gainMax": max(float(source["gain"]) for source in source_reports),
            "legacyMeanAbsoluteErrorDb": (
                sum(
                    abs(float(source["legacyErrorDb"]))
                    for source in source_reports
                ) /
                len(source_reports)
            ),
            "calibratedMeanAbsoluteErrorDb": (
                sum(abs(float(source["errorDb"])) for source in source_reports) /
                len(source_reports)
            ),
            "sampleClipCount": sum(
                int(source["clippedSamples"])
                for source in source_reports
            ),
            "peakLimitedSourceCount": sum(
                bool(source["peakLimited"])
                for source in source_reports
            ),
            "maximumPeakRatio": max(
                float(source["peakRatio"])
                for source in source_reports
            ),
            "percussionPeakCeilingRatio": PERCUSSION_PEAK_CEILING_RATIO,
            "perSongMaximumNormalization": False,
        },
    }
    path.write_text(
        json.dumps(catalog, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    return catalog
