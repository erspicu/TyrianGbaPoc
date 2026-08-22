#!/usr/bin/env python3
"""Regression tests for the Tyrian TYM -> GBA OPL sample lifecycle."""

from __future__ import annotations

import json
import unittest
from pathlib import Path

import gba_music_builder as music
import opl_sample_renderer as renderer


WORKSPACE = Path(__file__).resolve().parents[1]
MUSIC_ROOT = WORKSPACE / "vendor" / "audio" / "Music"
CALIBRATION = WORKSPACE / "res" / "music_maxmod_calibration.json"
CATALOG_PCM_BUDGET = 6_950_000


def track_path(number: int) -> Path:
    matches = list(MUSIC_ROOT.glob(f"{number:02d}_*.tym"))
    if len(matches) != 1:
        raise AssertionError(f"track {number} path count changed: {matches}")
    return matches[0]


def pair_zones(
    song: dict[str, object],
    source: int,
    instrument_index: int,
) -> tuple[renderer.OplRenderedSample, ...]:
    counts = music.collect_pair_note_counts(song)[(source, instrument_index)]
    holds = music.collect_pair_note_holds(song)[(source, instrument_index)]
    instruments = song["instruments"]
    metadata = song["metadata"]
    assert isinstance(instruments, list)
    assert isinstance(metadata, dict)
    instrument = instruments[instrument_index]
    percussion_sources = {
        int(value)
        for value in metadata["arrangement"]["percussionSources"]
    }
    percussion = music.is_percussion_patch(
        instrument,
        source,
        percussion_sources,
    )
    roots, requirements, _ = music.choose_render_roots(
        instrument,
        counts,
        holds,
        percussion,
    )
    return music._render_pair_zones(
        instrument,
        roots,
        percussion,
        requirements,
    )


class OplSampleLifecycleTests(unittest.TestCase):
    def test_01_envelope_state_classification(self) -> None:
        instrument = bytearray(renderer.INSTRUMENT_BYTES)
        instrument[7] = 0x50
        instrument[8] = 0xF5
        self.assertTrue(renderer.tonal_patch_is_indefinite(bytes(instrument)))

        instrument[7] = 0xF1
        instrument[8] = 0xF6
        self.assertFalse(renderer.tonal_patch_is_indefinite(bytes(instrument)))

        instrument[5] |= 0x20
        self.assertTrue(renderer.tonal_patch_is_indefinite(bytes(instrument)))

        instrument[5] &= ~0x20
        instrument[10] = 1
        instrument[2] = 0x50
        instrument[3] = 0xF5
        self.assertTrue(renderer.tonal_patch_is_indefinite(bytes(instrument)))

    def test_02_game_over_long_notes_are_sustain_loops(self) -> None:
        song = music.parse_tym(track_path(11))
        counts = music.collect_pair_note_counts(song)
        for source, instrument_index in counts:
            zones = pair_zones(song, source, instrument_index)
            self.assertTrue(zones)
            self.assertTrue(all(zone.loop for zone in zones))
            self.assertTrue(all(
                zone.required_hold_seconds > 1.9 for zone in zones
            ))

    def test_03_long_finite_tails_are_not_cut_at_420_ms(self) -> None:
        gygese = music.parse_tym(track_path(15))
        long_tail = pair_zones(gygese, 7, 5)
        self.assertEqual(len(long_tail), 1)
        self.assertEqual(long_tail[0].lifecycle, "finite_tonal_one_shot")
        self.assertGreater(long_tail[0].required_hold_seconds, 9.0)
        self.assertGreater(long_tail[0].rendered_seconds, 9.0)

        halloween = music.parse_tym(track_path(16))
        natural_decay = pair_zones(halloween, 5, 15)
        self.assertEqual(len(natural_decay), 1)
        self.assertEqual(
            natural_decay[0].lifecycle,
            "finite_tonal_one_shot",
        )
        self.assertGreater(natural_decay[0].rendered_seconds, 2.5)
        self.assertLess(
            natural_decay[0].rendered_seconds,
            natural_decay[0].required_hold_seconds,
        )

    def test_04_catalog_lifecycle_and_pcm_budget(self) -> None:
        calibration = json.loads(CALIBRATION.read_text(encoding="utf-8"))
        totals = {
            "pcm": 0,
            "sustain": 0,
            "finite": 0,
            "percussion": 0,
            "collapsed": 0,
            "maximum_root_distance": 0,
        }

        def discard_module(*args: object, **kwargs: object) -> bytes:
            del args, kwargs
            return b""

        for track in calibration["tracks"]:
            number = int(track["trackNumber"])
            _, report = music.build_tym_tracker_it(
                WORKSPACE,
                track_path(number),
                [int(value) for value in track["sourceChannels"]],
                [1.0] * len(track["sourceChannels"]),
                module_builder=discard_module,
                voice_volume_gains=[1.0] * len(track["sourceChannels"]),
            )
            totals["pcm"] += int(report["sample_pcm_bytes"])
            totals["sustain"] += int(report["sustain_loop_zones"])
            totals["finite"] += int(report["finite_tonal_zones"])
            totals["percussion"] += int(report["percussion_zones"])
            totals["collapsed"] += int(
                report["finite_root_collapsed_pairs"]
            )
            totals["maximum_root_distance"] = max(
                totals["maximum_root_distance"],
                int(report["maximum_root_distance_semitones"]),
            )

        self.assertLessEqual(totals["pcm"], CATALOG_PCM_BUDGET)
        self.assertEqual(totals["sustain"], 512)
        self.assertEqual(totals["finite"], 301)
        self.assertEqual(totals["percussion"], 127)
        self.assertEqual(totals["collapsed"], 47)
        self.assertLessEqual(totals["maximum_root_distance"], 15)


if __name__ == "__main__":
    unittest.main()
