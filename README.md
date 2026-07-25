# Tyrian GBA First-Level Technical Demo

This project is an independent Game Boy Advance proof of concept built from
the original Tyrian data already present in this workspace.  It is not a
binary conversion of the NES or SNES ROM.

The `opentyrian-source-parity-port` branch is now replacing the reconstructed
gameplay with a line-oriented translation of the OpenTyrian first-level loop.
Stage 2 embeds all original first-level event records and the exact transitive
HDT enemy dependency set, then runs the translated event state,
`JE_makeEnemy()` and OpenTyrian's 100-entry enemy pool beside the v11 loop as
a measured shadow runtime.  Movement, collision and rendering still use v11,
so this milestone does not yet claim gameplay parity.

ROMFS v1 now also embeds 68 stock Tyrian runtime files behind a seekable,
stdio-like read-only API.  The 9.85 MB image remains memory-mapped in cartridge
ROM instead of consuming WRAM, giving later line-by-line ports a common loader
for music, sound, shapes, text and level data.

The current scope is deliberately narrow:

- Tyrian opening screen and complete opening music
- Start enters the first level directly
- three independently scrolling Mode-0 background layers (MAP1, MAP2 and MAP3)
- five original first-level background speed changes, including slow and
  accelerated parallax sections
- all 1,009 decoded first-level source events
- PC `curLoc` event timing driven by effective MAP1 movement, with each spawn
  retaining its source pool, initial Y, HDT velocity, fixed movement and armor
- nine destructible 2x2 terrain assemblies locked to their source background
  scroll, plus native 24-pixel small-tank component spacing and spawn timing
- 24 audited visual archetypes covering 69 source enemy IDs
- the complete 128-entry GBA hardware OAM table
- PC-synchronised enemy projectiles: all three HDT turret/frequency slots,
  event-31 slot overrides, weapon spawn offsets, slot rotation, aim,
  multiposition spreads, velocity, animation and source graphics, split over
  dedicated red-shot, orange-dart and purple-laser OBJ palettes
- the first-level boss's original dual weapon-59 aimed ports and weapon-127
  five-way spread cadence; event-79 254-armor PC boss bar geometry and damage
  flash are also retained, while the boss body movement remains POC-level
- a 60-entry enemy-shot pool matching OpenTyrian, with spawn/drop/peak
  telemetry
- player shots, projectile collisions, effects and an invincible player
- a 48-entry enemy pool (30 peak on the complete route) with a zero-replacement
  regression invariant
- a separate 100-entry source-parity shadow pool, split into the original four
  25-slot groups, with exact spawn/control/skip/RNG telemetry
- the original Pulse-Cannon power-1 sprite resolved through HDT weapon record
  155 to player-shot graphic 59, including its repeat rate and vertical speed
- stable USP Talon neutral/left/right banking poses instead of alternating
  unrelated poses every logic update
- original 12-frame small and four-quadrant air/ground enemy explosions,
  backed by a 48-entry effect pool with drop telemetry; v7 preserves each
  quadrant's native 12x14 anchor so the centre seam is closed
- PC-synchronised rewards: every enemy retains its exact positive HDT
  `evalue` for immediate kill credit, while all 33 first-level event-33
  `enemydie` overrides run by link at runtime; the 25/50/75/100/250 physical
  score items use their original coin/gem graphics, pickup collision and item
  sound, with cumulative cash rendered in Tyrian's lower-left TINY_FONT
- PC-style pause: `Start` freezes the world, shows the original FONT_SHAPES
  `PAUSED` label, keeps music playing at half volume, then resumes on `Start`
- a simplified first-level boss body and return to the opening screen
- complete first-level tracker music and seven converted Tyrian sound effects,
  including the original enemy weapon sounds used by this route
- original Normal-speed fixed-step target (about 34.78 game updates/second)

## Controls

- `Start`: enter the first level; during play, pause/resume
- D-pad: move
- `A` or `B`: fire
- `Select` or `L`: development shortcut to the boss
- `R`: development shortcut to defeat the boss

## Build

From PowerShell:

```powershell
.\build.ps1
```

The release ROM is written to:

```text
build/tyrian_gba_level1_source_parity_romfs_v14.gba
```

`build.ps1` also builds a deterministic auto-test ROM, runs the entire route
under mGBA, and checks its SRAM telemetry.  After a successful run, historical
and test ROMs move to `Backup`, rebuildable intermediates are removed, and
`build` retains only the latest release ROM.  Pass `-KeepIntermediates` when
debugging requires ELF, map, log, save, preview and verification files.

The GBA toolchain is kept under `tools/gba-sdk`.  Generated native resources
are under `res`, the current release ROM is under `build`, historical ROMs are
under `Backup`, and the reproducible source asset conversions are
`tools/build_assets.py` and `tools/build_romfs.py`.

## Documentation

- [Source-parity first-level port](MD/Tyrian-GBA-Source-Parity-Port.md)
- [Cartridge ROMFS format and porting API](MD/Tyrian-GBA-ROMFS.md)
- [First-level technical demo](MD/Tyrian-GBA-First-Level-Tech-Demo.md)
- [GBA toolchain and runtime setup](MD/Tyrian-GBA-Toolchain-Setup.md)
- [Game Boy/GBC and GBA audio research](MD/Tyrian-Audio-Lab-GameBoy-GBA.md)
- [Track 18 GB/GBA percussion correction](MD/Tyrian-Audio-Lab-Track18-Percussion-Fix.md)

## Repository policy

Generated resources in `res/`, build intermediates in `build/`, emulator
state, and all `.gba` ROM images are intentionally excluded from Git.
Milestone ROMs are published separately through tagged GitHub Releases only
after a demonstrable result is ready.

Directly translated OpenTyrian code is licensed under
`GPL-2.0-or-later`; see [COPYING](COPYING).

The build currently expects this repository at
`AprTyrianNes/repo/TyrianGbaPoc` alongside the source data and helper projects
under `AprTyrianNes/org`, with the GBA SDK under `AprTyrianNes/tools/gba-sdk`.
