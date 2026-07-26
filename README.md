# Tyrian GBA First-Level Technical Demo

This project is an independent Game Boy Advance proof of concept built from
the original Tyrian data already present in this workspace.  It is not a
binary conversion of the NES or SNES ROM.

The `opentyrian-source-parity-port` branch replaces reconstructed gameplay
with a line-oriented translation of the OpenTyrian first-level loop. Events,
four 25-entry enemy pools, movement, projectiles, collision, damage, linked
death, rewards, the authored boss group, end-level flight and statistics all
run from stock LVL/HDT data. A small GBA adapter takes a final 1:1 central
240x160 crop from the original 264x184 gameplay viewport; it never rescales
or writes presentation positions back into gameplay.

ROMFS v1 embeds 68 stock Tyrian runtime files behind a seekable, stdio-like
read-only API. Gameplay loaders parse MUS, SHP, PIC, HDT and LVL directly from
the memory-mapped image. Interactive front-end PIC/SHP/HDT composition is
performed at build time into GBA Mode-4 frames so cursor movement never
re-decodes source assets.

The current scope is deliberately narrow:

- two original intro logos and the requested Start New Game, Play Mode,
  Episode, Difficulty, Game Menu and Next Level flow
- Mode-4 double-buffered menu transitions and selection-row-only updates;
  invalid keys perform no redraw
- three independently scrolling Mode-0 background layers (MAP1, MAP2 and MAP3)
- all 1,009 source records remain directly addressable; the deterministic
  first-level route consumes 935 records through the authored boss exit
  (926 applied, five deferred and four conditionally skipped)
- PC `curLoc` timing and source enemy identity, pool, position, movement,
  armor, animation, link, turret and death fields
- the original four 25-slot enemy groups: 473/473 event spawns, three successful
  death spawns, a peak of 39 active objects and no pool-full loss
- a concrete 60-entry OpenTyrian projectile pool: 428 source shots, peak 32,
  17,466 movement updates and zero projectile drops
- source player-shot ordering and collision formulas, armor damage,
  `dlevel=-1` fixed remnants, linked destruction, direct `evalue` credit and
  `eenemydie` children
- three physical pickups spawned, two collected and the data-cube branch
  crossed by the deterministic route
- runtime decoding of the stock ROMFS `newsh*.shp` and compact
  `tyrian.shp` Sprite2 streams, keyed by
  `(shape_table, egr[enemycycle - 1], size, filter)`; there is no event-limited
  Python enemy-frame catalog
- a 21-slot split 8bpp OBJ cache with zero Sprite2 decode failures or cache
  drops, plus all 128 GBA OAM entries with a measured peak of 89
- the complete OpenTyrian background2/background3, ground, sky, top, player,
  projectile and effect draw order translated to dynamic GBA BG/OBJ priority
  and reverse OAM emission; all 252 layer relations pass an exhaustive test
- direct use of the active PC level palette: every Sprite2 retains its source
  hue and is mapped to eight brightness samples, replacing the former
  per-table and per-structure 15-colour approximations
- exact PC player inertia, background parallax and enemy map offsets; the GBA
  crops `game_screen x=36..275, y=12..171` and restricts the player to source
  `y=17..152`, keeping the complete 24x28 ship inside that visible crop
- original Pulse-Cannon graphic 59 from HDT weapon 155, stable USP Talon bank
  poses, original explosion and reward animation assets, TINY_FONT cash and
  FONT_SHAPES `PAUSED`
- shield-to-armor player damage, explosion and Game Over flow
- the linked source boss group, PC-style boss health bar, end-level flight,
  level statistics and return to Game Menu
- complete first-level tracker music, seven converted Tyrian sound effects and
  the original Normal-speed target of about 34.78 logic updates/second

The v22 front-end stress test changes the Title selection every frame for
600 frames. Its result is pixel-identical to the generated target frame and
adds zero delayed VBlanks over the no-redraw baseline. The full route passes
schema-20 SRAM telemetry with 100 source enemy kills and the boss group fully
cleared.

## Controls

- D-pad Up/Down: move through menus
- `A` or `Start`: confirm a menu choice
- `B`: return to the previous menu
- `Start` during play: pause/resume
- D-pad: move
- `A` or `B`: fire

## Build

From PowerShell:

```powershell
.\build.ps1
```

The release ROM is written to:

```text
build/tyrian_gba_level1_pc_flow_mode4_romfs_v22.gba
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
- [v22 Mode-4 front-end and benchmark](MD/Tyrian-GBA-Frontend-Mode4-v22.md)
- [v21 runtime raw Sprite2 pipeline](MD/Tyrian-GBA-Runtime-Sprite2-v21.md)
- [v20 PC layer order and structure palette](MD/Tyrian-GBA-PC-Layer-Order-Palette-v20.md)
- [v19 player crop-safe bounds](MD/Tyrian-GBA-Player-Crop-Bounds-v19.md)
- [v18 PC-coordinate 1:1 crop](MD/Tyrian-GBA-1to1-Crop-Source-Parity-v18.md)
- [v17 enemy/reward source-parity translation](MD/Tyrian-GBA-Enemy-Reward-Source-Parity-v17.md)
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
