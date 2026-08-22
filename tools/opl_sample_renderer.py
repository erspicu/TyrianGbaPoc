#!/usr/bin/env python3
"""Deterministic offline OPL2 patch renderer for GBA music assets.

The host DLL wraps the exact OpenTyrian/DOSBox OPL core.  This Python layer
performs the one operation that should not run on the GBA: band-limited
conversion from the OPL native rate to Maxmod's fixed 15,768 Hz source rate,
plus stable sustain-loop and one-shot boundary preparation.
"""

from __future__ import annotations

import ctypes
import functools
import math
from dataclasses import dataclass
from pathlib import Path

import numpy as np


OPL_NATIVE_RATE = 49_716
MAXMOD_SOURCE_RATE = 15_768
BRIDGE_ABI_VERSION = 2
INSTRUMENT_BYTES = 46
SILENCE_FLOOR_DB = -58.0
MINIMUM_ONE_SHOT_SECONDS = 0.045
ONE_SHOT_TAIL_GUARD_SECONDS = 0.012
TONAL_HOLD_GUARD_SECONDS = 0.022
MINIMUM_LOOP_ANALYSIS_SECONDS = 1.50
MAXIMUM_LOOP_ANALYSIS_SECONDS = 3.00


@dataclass(frozen=True)
class OplRenderedSample:
    signal: np.ndarray
    loop: bool
    loop_start: int
    source_rate: int
    root_pitch_q8: int
    peak_before_quantize: float
    loop_boundary_error: float
    software_lfo: bool
    hardware_lfo: bool
    lifecycle: str
    required_hold_seconds: float
    rendered_seconds: float


def _bridge_path() -> Path:
    return Path(__file__).with_name("opl_renderer") / "tyrian_opl_bridge.dll"


@functools.lru_cache(maxsize=1)
def _load_bridge() -> ctypes.CDLL:
    path = _bridge_path()
    if not path.is_file():
        raise FileNotFoundError(
            f"Tyrian OPL renderer is missing: {path}. "
            "Run tools/opl_renderer/rebuild.ps1 on Windows with LLVM."
        )
    library = ctypes.CDLL(str(path.resolve()))
    library.tyrian_opl_abi_version.argtypes = []
    library.tyrian_opl_abi_version.restype = ctypes.c_uint32
    actual_abi = int(library.tyrian_opl_abi_version())
    if actual_abi != BRIDGE_ABI_VERSION:
        raise RuntimeError(
            "Tyrian OPL renderer ABI mismatch: "
            f"{actual_abi} != {BRIDGE_ABI_VERSION}"
        )
    library.tyrian_opl_render_patch.argtypes = [
        ctypes.POINTER(ctypes.c_uint8),
        ctypes.c_uint32,
        ctypes.c_int32,
        ctypes.c_uint32,
        ctypes.c_uint32,
        ctypes.POINTER(ctypes.c_int16),
        ctypes.c_uint32,
    ]
    library.tyrian_opl_render_patch.restype = ctypes.c_int32
    return library


def _render_native(
    instrument: bytes,
    root_pitch_q8: int,
    sustain_seconds: float,
    release_seconds: float,
) -> np.ndarray:
    if len(instrument) != INSTRUMENT_BYTES:
        raise ValueError(
            f"OPL instrument size changed: {len(instrument)} != {INSTRUMENT_BYTES}"
        )
    sustain_samples = max(1, int(round(sustain_seconds * OPL_NATIVE_RATE)))
    release_samples = max(0, int(round(release_seconds * OPL_NATIVE_RATE)))
    total = sustain_samples + release_samples
    patch = (ctypes.c_uint8 * INSTRUMENT_BYTES).from_buffer_copy(instrument)
    output = (ctypes.c_int16 * total)()
    rendered = int(_load_bridge().tyrian_opl_render_patch(
        patch,
        INSTRUMENT_BYTES,
        int(root_pitch_q8),
        sustain_samples,
        release_samples,
        output,
        total,
    ))
    if rendered != total:
        raise RuntimeError(
            f"OPL renderer returned {rendered} samples, expected {total}"
        )
    return np.ctypeslib.as_array(output).astype(np.float64) / 32768.0


@functools.lru_cache(maxsize=1)
def _lowpass_kernel() -> np.ndarray:
    # 127-tap Blackman-windowed sinc.  The 6% transition guard leaves the
    # GBA Nyquist edge clean before nearest-neighbour Maxmod playback.
    taps = 127
    center = (taps - 1) / 2.0
    positions = np.arange(taps, dtype=np.float64) - center
    cutoff = 0.5 * MAXMOD_SOURCE_RATE / OPL_NATIVE_RATE * 0.94
    kernel = 2.0 * cutoff * np.sinc(2.0 * cutoff * positions)
    kernel *= np.blackman(taps)
    kernel /= float(np.sum(kernel))
    return kernel


def _resample_to_maxmod(signal: np.ndarray) -> np.ndarray:
    if signal.size == 0:
        return signal.copy()
    filtered = np.convolve(signal, _lowpass_kernel(), mode="same")
    output_count = max(
        1,
        int(math.floor(signal.size * MAXMOD_SOURCE_RATE / OPL_NATIVE_RATE)),
    )
    source_positions = (
        np.arange(output_count, dtype=np.float64) *
        OPL_NATIVE_RATE /
        MAXMOD_SOURCE_RATE
    )
    return np.interp(
        source_positions,
        np.arange(filtered.size, dtype=np.float64),
        filtered,
    )


def _dc_block(signal: np.ndarray, coefficient: float = 0.995) -> np.ndarray:
    if signal.size == 0:
        return signal.copy()
    output = np.empty_like(signal)
    output[0] = 0.0
    previous_input = float(signal[0])
    previous_output = 0.0
    for index in range(1, signal.size):
        current_input = float(signal[index])
        current_output = (
            current_input - previous_input + coefficient * previous_output
        )
        output[index] = current_output
        previous_input = current_input
        previous_output = current_output
    return output


def _smooth_boundary(signal: np.ndarray, milliseconds: float) -> None:
    count = min(
        signal.size // 4,
        max(2, int(round(MAXMOD_SOURCE_RATE * milliseconds / 1000.0))),
    )
    if count <= 1:
        return
    phase = np.linspace(0.0, 1.0, count, endpoint=True)
    curve = phase * phase * (3.0 - 2.0 * phase)
    signal[:count] *= curve
    signal[-count:] *= curve[::-1]


def _fade_in(signal: np.ndarray, milliseconds: float) -> None:
    count = min(
        signal.size,
        max(2, int(round(MAXMOD_SOURCE_RATE * milliseconds / 1000.0))),
    )
    if count <= 1:
        return
    phase = np.linspace(0.0, 1.0, count, endpoint=True)
    signal[:count] *= phase * phase * (3.0 - 2.0 * phase)


def _crossfade_loop_seam(signal: np.ndarray, loop_start: int) -> float:
    """Morph the loop tail into the samples immediately before loop_start."""
    loop_length = signal.size - loop_start
    count = min(256, loop_start, loop_length // 3)
    if count < 8:
        return 0.0
    preceding = signal[loop_start - count : loop_start].copy()
    tail = signal[-count:].copy()
    phase = np.linspace(0.0, 1.0, count, endpoint=True)
    curve = phase * phase * (3.0 - 2.0 * phase)
    signal[-count:] = tail * (1.0 - curve) + preceding * curve

    window = min(64, count)
    reference = signal[loop_start - window : loop_start]
    boundary = signal[-window:]
    reference_energy = float(np.mean(np.square(reference))) + 1e-12
    mismatch = float(np.mean(np.square(boundary - reference)))
    return math.sqrt(max(0.0, mismatch / reference_energy))


def _find_sustain_loop(
    signal: np.ndarray,
    lfo: bool,
) -> tuple[np.ndarray, int, float]:
    # Preserve the authored attack, then find a phase-compatible end for the
    # stable section.  LFO patches intentionally retain a longer loop so their
    # motion is not collapsed into a static wavetable.
    minimum_start = int(round(MAXMOD_SOURCE_RATE * 0.075))
    maximum_start = min(
        signal.size // 2,
        int(round(MAXMOD_SOURCE_RATE * 0.155)),
    )
    window = 64
    if maximum_start <= minimum_start + window:
        minimum_start = max(window, signal.size // 4)
        maximum_start = min(signal.size // 2, minimum_start + window + 1)

    # Keep the attack, then take the earliest locally stable RMS window.  The
    # loop itself must stay compact because every track embeds its own sample
    # set in the Maxmod bank; the longer analysis render below is only used to
    # validate the envelope and is not copied wholesale into the ROM.
    rms_window = max(window, int(round(MAXMOD_SOURCE_RATE * 0.018)))
    start = minimum_start
    for candidate in range(minimum_start, maximum_start, 32):
        a = signal[candidate : candidate + rms_window]
        b = signal[candidate + rms_window : candidate + rms_window * 2]
        if a.size != rms_window or b.size != rms_window:
            break
        rms_a = math.sqrt(float(np.mean(np.square(a))) + 1e-16)
        rms_b = math.sqrt(float(np.mean(np.square(b))) + 1e-16)
        if abs(rms_b - rms_a) / max(rms_a, rms_b, 1e-8) <= 0.08:
            start = candidate
            break

    minimum_length = 768 if lfo else 64
    preferred_length = 1536 if lfo else 384
    maximum_length = 2584 if lfo else 1024
    maximum_end = min(signal.size - window, start + maximum_length)
    minimum_end = min(maximum_end, start + minimum_length)
    if maximum_end <= minimum_end:
        start = max(0, signal.size // 3)
        minimum_end = min(signal.size - window, start + 64)
        maximum_end = signal.size - window
    reference = signal[start : start + window]
    reference_energy = float(np.mean(np.square(reference))) + 1e-12
    best_end = minimum_end
    best_score = float("inf")
    for end in range(minimum_end, maximum_end + 1):
        candidate = signal[end : end + window]
        if candidate.size != window:
            break
        mismatch = float(np.mean(np.square(candidate - reference)))
        length_penalty = abs((end - start) - preferred_length) / max(
            preferred_length,
            1,
        )
        score = mismatch / reference_energy + length_penalty * 0.002
        if score < best_score:
            best_score = score
            best_end = end
    if best_end <= start:
        raise RuntimeError("could not find a positive OPL sustain loop")
    output = signal[:best_end].copy()
    boundary_error = _crossfade_loop_seam(output, start)
    return output, start, boundary_error


def _trim_one_shot(signal: np.ndarray) -> np.ndarray:
    peak = max(1e-12, float(np.max(np.abs(signal))))
    threshold = peak * 10.0 ** (SILENCE_FLOOR_DB / 20.0)
    active = np.flatnonzero(np.abs(signal) >= threshold)
    minimum = int(round(MAXMOD_SOURCE_RATE * MINIMUM_ONE_SHOT_SECONDS))
    if active.size:
        end = int(active[-1]) + int(round(
            MAXMOD_SOURCE_RATE * ONE_SHOT_TAIL_GUARD_SECONDS
        ))
    else:
        end = minimum
    end = max(minimum, min(signal.size, end))
    output = signal[:end].copy()
    _smooth_boundary(output, 2.0)
    return output


def _operator_is_indefinite(
    misc: int,
    attack_decay: int,
    sustain_release: int,
) -> bool:
    """Mirror the OPL envelope states that cannot reach silence with key-on."""
    if misc & 0x20:
        return True
    decay_rate = attack_decay & 0x0f
    sustain_level = sustain_release >> 4
    release_rate = sustain_release & 0x0f
    # A zero decay rate never reaches the sustain/release state.  A zero
    # release rate at a non-zero sustain level also remains audible forever.
    return decay_rate == 0 or (release_rate == 0 and sustain_level < 15)


def tonal_patch_is_indefinite(instrument: bytes) -> bool:
    carrier = _operator_is_indefinite(
        instrument[5],
        instrument[7],
        instrument[8],
    )
    if carrier:
        return True
    # In additive mode the modulator is an independently audible operator;
    # in FM mode its lifetime is still bounded by the carrier envelope.
    additive = bool(instrument[10] & 0x01)
    return additive and _operator_is_indefinite(
        instrument[0],
        instrument[2],
        instrument[3],
    )


@functools.lru_cache(maxsize=4096)
def render_opl_patch(
    instrument: bytes,
    root_pitch_q8: int,
    percussion: bool,
    required_hold_seconds: float = 0.0,
) -> OplRenderedSample:
    hardware_lfo = bool((instrument[0] | instrument[5]) & 0xc0)
    software_lfo = bool(instrument[15] or instrument[17] or instrument[18])
    required_hold_seconds = max(0.0, float(required_hold_seconds))
    indefinite_tonal = not percussion and tonal_patch_is_indefinite(
        instrument
    )
    one_shot = percussion or not indefinite_tonal
    if one_shot:
        keyoff_ticks = int(instrument[11])
        if percussion:
            sustain_seconds = (
                min(0.48, max(0.025, keyoff_ticks / 69.5))
                if keyoff_ticks
                else 0.115
            )
            release_seconds = 0.62
            lifecycle = "percussion_one_shot"
        else:
            # Render through the longest authored hold assigned to this root.
            # The old fixed 420 ms cap cut still-audible OPL envelopes and
            # produced silence until the next note-on in sparse arrangements.
            sustain_seconds = max(
                MINIMUM_ONE_SHOT_SECONDS,
                required_hold_seconds + TONAL_HOLD_GUARD_SECONDS,
            )
            release_seconds = 0.0
            lifecycle = "finite_tonal_one_shot"
        native = _render_native(
            instrument,
            root_pitch_q8,
            sustain_seconds,
            release_seconds,
        )
        converted = _dc_block(_resample_to_maxmod(native))
        output = _trim_one_shot(converted)
        loop = False
        loop_start = 0
        boundary_error = 0.0
    else:
        analysis_seconds = min(
            MAXIMUM_LOOP_ANALYSIS_SECONDS,
            max(
                MINIMUM_LOOP_ANALYSIS_SECONDS,
                required_hold_seconds + TONAL_HOLD_GUARD_SECONDS,
            ),
        )
        native = _render_native(
            instrument,
            root_pitch_q8,
            analysis_seconds,
            0.0,
        )
        converted = _dc_block(_resample_to_maxmod(native))
        output, loop_start, boundary_error = _find_sustain_loop(
            converted,
            hardware_lfo or software_lfo,
        )
        _fade_in(output, 1.5)
        loop = True
        lifecycle = "sustain_loop"
    peak = max(1e-12, float(np.max(np.abs(output))))
    rendered_seconds = output.size / MAXMOD_SOURCE_RATE
    if (
        lifecycle == "finite_tonal_one_shot" and
        rendered_seconds + TONAL_HOLD_GUARD_SECONDS < required_hold_seconds
    ):
        # Ending early is legal only after the original OPL envelope has
        # naturally crossed the declared silence floor.
        converted_peak = max(1e-12, float(np.max(np.abs(converted))))
        threshold = converted_peak * 10.0 ** (SILENCE_FLOOR_DB / 20.0)
        required_samples = min(
            converted.size,
            int(math.ceil(required_hold_seconds * MAXMOD_SOURCE_RATE)),
        )
        if np.any(np.abs(converted[output.size : required_samples]) >= threshold):
            raise RuntimeError(
                "audible OPL one-shot was trimmed before its authored hold"
            )
    output.setflags(write=False)
    return OplRenderedSample(
        signal=output,
        loop=loop,
        loop_start=loop_start,
        source_rate=MAXMOD_SOURCE_RATE,
        root_pitch_q8=int(root_pitch_q8),
        peak_before_quantize=peak,
        loop_boundary_error=boundary_error,
        software_lfo=software_lfo,
        hardware_lfo=hardware_lfo,
        lifecycle=lifecycle,
        required_hold_seconds=required_hold_seconds,
        rendered_seconds=rendered_seconds,
    )
