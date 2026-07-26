# Tyrian GBA First-Level Technical Demo

This project is an independent Game Boy Advance proof of concept built from
the original Tyrian data already present in this workspace.  It is not a
binary conversion of the NES or SNES ROM.

The `opentyrian-source-parity-port` branch replaces reconstructed gameplay
with a line-oriented translation of the OpenTyrian first-level loop. Stage 4
makes that translation authoritative for the first-level body: events, four
25-entry enemy pools, movement, concrete projectiles, collision, damage,
linked death, death-spawned pickups and cash all run from stock LVL/HDT data.
A small GBA adapter takes a final 1:1 central 240x160 crop from the original
264x184 gameplay viewport; it never rescales or writes presentation positions
back into gameplay. The boss after source position 5400 intentionally remains
the existing simplified POC implementation and is the next major port boundary.

ROMFS v1 embeds 68 stock Tyrian runtime files behind a seekable, stdio-like
read-only API. Runtime loaders now parse MUS, SHP, PIC, HDT and LVL directly
from the memory-mapped image. The opening screen is built at boot from raw
`tyrian.pic`, `palette.dat` and `tyrian.shp`; the former generated title,
event and enemy blobs are no longer linked into the ROM.

The current scope is deliberately narrow:

- Tyrian opening screen and complete opening music; `Start` enters level one
  directly
- three independently scrolling Mode-0 background layers (MAP1, MAP2 and MAP3)
- all 1,009 source records remain directly addressable; the demo consumes the
  exact 878 records before its position-5400 boss handoff (869 applied, five
  deferred and four conditionally skipped)
- PC `curLoc` timing and source enemy identity, pool, position, movement,
  armor, animation, link, turret and death fields
- the original four 25-slot enemy groups: 473/473 event spawns, three successful
  death spawns, a peak of 39 active objects and no pool-full loss
- a concrete 60-entry OpenTyrian projectile pool: 185 source shots, peak nine,
  9,163 movement updates, 11 player contacts and zero drops
- source player-shot ordering and collision formulas, armor damage,
  `dlevel=-1` fixed remnants, linked destruction, direct `evalue` credit and
  `eenemydie` children
- three physical pickups spawned and two collected by the deterministic route;
  `JE_playerCollide()` reward branches, including the data-cube branch not
  crossed by this route, retain their fixed-single-player gameplay state
- runtime decoding of the stock ROMFS `newsh*.shp` and compact
  `tyrian.shp` Sprite2 streams, keyed by
  `(shape_table, egr[enemycycle - 1], size, filter)`; there is no event-limited
  Python enemy-frame catalog
- a 21-slot split 8bpp OBJ cache with zero Sprite2 decode failures or cache
  drops, plus all 128 GBA OAM entries with a measured peak of 43
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
- a development-stage player with no death/restart flow
- a simplified boss body and existing boss projectile adapter, followed by a
  return to the opening screen
- complete first-level tracker music, seven converted Tyrian sound effects and
  the original Normal-speed target of about 34.78 logic updates/second

The deterministic v21 route has no stream, effect, reward, projectile-pool,
Sprite2-decode or cache drops. It records 155 delayed VBlanks over 12,239
displayed frames (about 1.27%): 152 unique runtime Sprite2 decodes produce
44,926 subsequent cache hits. This deliberately exposes the cost of the raw
runtime path so it can be compared with a future universal row-packed provider.

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
build/tyrian_gba_level1_source_parity_runtime_sprite2_romfs_v21.gba
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
