# Tyrian GBA First-Level Technical Demo

This project is an independent Game Boy Advance proof of concept built from
the original Tyrian data already present in this workspace.  It is not a
binary conversion of the NES or SNES ROM.

The current scope is deliberately narrow:

- Tyrian opening screen and complete opening music
- Start enters the first level directly
- three independently scrolling Mode-0 background layers (MAP1, MAP2 and MAP3)
- five original first-level background speed changes, including slow and
  accelerated parallax sections
- all 1,009 decoded first-level source events
- 24 audited visual archetypes covering 69 source enemy IDs
- the complete 128-entry GBA hardware OAM table
- player shots, enemy shots, collisions, effects and an invincible player
- the original Pulse-Cannon power-1 sprite resolved through HDT weapon record
  155 to player-shot graphic 59, including its repeat rate and vertical speed
- stable USP Talon neutral/left/right banking poses instead of alternating
  unrelated poses every logic update
- original 12-frame small and four-quadrant air/ground enemy explosions,
  backed by a 48-entry effect pool with drop telemetry; v7 preserves each
  quadrant's native 12x14 anchor so the centre seam is closed
- HDT-derived 50/100/1000 reward drops using the original coin/gem sheet,
  pickup collision, cash counter and original item sound
- a simplified first-level boss and return to the opening screen
- complete first-level tracker music and four converted Tyrian sound effects
- original Normal-speed fixed-step target (about 34.78 game updates/second)

## Controls

- `Start`: enter the first level
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
build/tyrian_gba_level1_tech_demo_v7.gba
```

`build.ps1` also builds a deterministic auto-test ROM, runs the entire route
under mGBA, reads its SRAM telemetry, and writes `build/verification.txt`.

The GBA toolchain is kept under `tools/gba-sdk`.  Generated native resources
are under `res`, previews and ROMs under `build`, and the reproducible source
asset conversion is `tools/build_assets.py`.

## Repository policy

Generated resources in `res/`, build intermediates in `build/`, emulator
state, and all `.gba` ROM images are intentionally excluded from Git.
Milestone ROMs are published separately through tagged GitHub Releases only
after a demonstrable result is ready.

The build currently expects this repository at
`AprTyrianNes/repo/TyrianGbaPoc` alongside the source data and helper projects
under `AprTyrianNes/org`, with the GBA SDK under `AprTyrianNes/tools/gba-sdk`.
