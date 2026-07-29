# Tyrian GBA Source-Parity Port

This project is a progressively complete Game Boy Advance port of Tyrian,
built from the original game data and the OpenTyrian source in this
workspace. It is not a binary conversion of the NES or SNES ROM.

The `opentyrian-source-parity-port` branch treats OpenTyrian as the gameplay
specification. Selected-level events, four 25-entry enemy pools, movement,
projectiles, collision, damage, linked death, rewards, authored boss groups,
end-level flight and statistics run from stock LVL/HDT data. Platform changes
are confined to GBA presentation, input, audio and storage adapters. A final
1:1 central 240x160 crop is taken from the original 264x184 gameplay
viewport; the adapter never rescales or writes presentation positions back
into gameplay.

ROMFS v1 embeds 68 stock Tyrian runtime files behind a seekable, stdio-like
read-only API. Gameplay loaders parse MUS, SHP, PIC, HDT and LVL directly from
the memory-mapped image. Interactive front-end PIC/SHP/HDT composition is
performed at build time into GBA Mode-4 frames so cursor movement never
re-decodes source assets.

The currently integrated and continuously checked scope includes:

- two original intro logos; Start New Game, Demo and JukeBox; Play Mode,
  Episode, Difficulty, Game Menu and Next Level flow
- all five stock demo recordings, including source equipment headers and
  input streams, plus the original 30-second title-idle attract-mode trigger
- Mode-4 double-buffered menu transitions and selection-row-only updates;
  invalid keys perform no redraw
- three independently scrolling Mode-0 background layers (MAP1, MAP2 and MAP3)
- all source records remain directly addressable; the regression-only
  max-power Pulse-Cannon route consumes 892 records through the authored boss
  exit (888 applied and four skipped)
- PC `curLoc` timing and source enemy identity, pool, position, movement,
  armor, animation, link, turret and death fields
- the original four 25-slot enemy groups: 473/473 event spawns, 21 successful
  death spawns, a peak of 39 active objects and no pool-full loss
- a concrete 60-entry OpenTyrian projectile pool: 86 source shots, peak 8,
  5,198 movement updates and zero projectile drops on the max-weapon route
- source player-shot ordering and collision formulas, armor damage,
  `dlevel=-1` fixed remnants, linked destruction, direct `evalue` credit and
  `eenemydie` children
- 22 physical pickups spawned, nine collected and two Data Cubes acquired by
  the deterministic max-weapon route
- a complete, lossless build-time expansion of all 37 stock Sprite2 banks;
  runtime still selects `(shape_table, egr[enemycycle - 1], size, filter)`
  from LVL/HDT and applies the active PC palette
- a 24-slot OBJ L1 backed by a 64-slot palette-aware EWRAM L2, with zero
  Sprite2 failures, drops or RLE fallbacks, plus all 128 GBA OAM entries
- the complete OpenTyrian background2/background3, ground, sky, top, player,
  projectile and effect draw order translated to dynamic GBA BG/OBJ priority
  and reverse OAM emission; all 252 layer relations pass an exhaustive test
- exact OpenTyrian map-reference bounds for all three background layers:
  stock sentinel 71 is transparent on layer 2, while 70 and 71 are
  transparent on layer 3, instead of becoming unrelated repeated shapes
- direct use of the active PC level palette: every Sprite2 retains its source
  hue and is mapped to eight brightness samples, replacing the former
  per-table and per-structure 15-colour approximations
- exact PC player inertia, background parallax and enemy map offsets; the GBA
  crops `game_screen x=36..275, y=12..171` and restricts the player to source
  `y=17..152`, keeping the complete 24x28 ship inside that visible crop
- stock new-game Port 1 Pulse-Cannon at power 1 from HDT weapon 155; weapon
  ID/power changes dynamically rebind through `weaponPort.op[0][power-1]`, so
  source power-up pickups are no longer masked by a fixed power-11 adapter
- shield-to-armor player damage, explosion and Game Over flow; the playable
  ROM defaults to `TYRIAN_GBA_DEV_PLAYER_INVINCIBLE=0`
- the linked source boss group, PC-style boss health bar, end-level flight,
  level statistics and return to Game Menu
- complete tracker music plus all 29 Tyrian SFX and nine voices, with the
  original Normal-speed target of about 34.78 logic updates/second

The playable build is mortal. Deterministic gameplay tests bypass damage and
request power 11 only through an `AUTOTEST`-restricted compile-time override,
so their established boss/cache goldens do not alter the release. A dedicated
forced-death regression verifies the release-like power-1/HDT-155 binding,
normal death, and the stock Game Over/menu music transitions.

The v29 full route passes schema-25 SRAM telemetry with 100 source enemy kills
and the boss group fully cleared. Its Sprite2 L2 reduces first-level missed
VBlanks from 625 to 13 and the authored Boss interval from 437 to 4, while the
L1 miss/eviction workload remains unchanged.

The v30 background fix removes the Episode 2 and 4 first-level corruption
without per-level data or exceptions. Pixel-identical Episode 1 and 3
captures, corrected Episode 2 and 4 captures, and the complete 62-section
ROMFS matrix all pass.

The v31 background working-set fix keeps the 32-row hardware tilemap but
references only the 21 rows which can intersect the GBA viewport, plus one
prefetch/transition row. Episode 2 level 1 missed VBlanks fall from 553 to 30
and lossy background approximations from 472 to 28, with identical gameplay,
collision and Sprite2 telemetry. ARM/IWRAM collision/cache paths, grouped
Sprite2 stores and an explicit Episode 2 performance regression are included.
Disabling all music and sound after this fix changes 30 to only 29, so the
full Maxmod soundtrack remains enabled instead of being degraded to PSG.

The v32 collision-safe pattern index removes false duplicate allocations:
Episode 2 level 1 now reaches zero background approximations and only three
missed VBlanks at Low Detail/Normal Speed, with identical gameplay and
Sprite2 workload. A full 62-level scan also replaces the fixed 512/512/512
character caches with one source-wide 576/480/480 partition. It fits the
known 533/434/454 per-layer maxima without per-level tables, load-time map
scans or generated GBA-only backgrounds.

The v37 release returns from the six-system stress fixture to the stock
Port 1 Pulse-Cannon at power 11 and defaults to High Detail. It packs the
complete 38-entry source sound catalog, restores gameplay voices and
explosions, translates the Secret Level portal response, moves the cash
readout down 8 pixels, and resets all four enemy shape slots at every level
boundary exactly like OpenTyrian. Episode 4 section 1 completes with zero
unknown visuals, decode failures, cache drops or RLE fallbacks.

The v38 release makes Secret, Game Over and end-level presentation follow
the source flow. Secret music returns to the level theme, transition songs
are one-shot, and the Game Over scene continues moving. End-level statistics
are drawn over the last gameplay frame using the ROMFS `SMALL_FONT_SHAPES`,
the source coordinates and hue-15 glow/final colour. A brightness-weighted
4bpp background palette selector also fixes dark-green corruption on mixed
mechanical tiles without violating the Episode 2 frame budget.

The v39 release removes the fixed HDT-165/power-11 player weapon adapter.
Playable levels now start from OpenTyrian's Pulse-Cannon power 1 state and
resolve the active HDT weapon only when weapon ID or power changes. Power-up
pickups therefore change the actual volley on the next shot. Release
invincibility is off; the normal shield, armor, explosion and Game Over path
is active.

The v40 release closes the title/demo, transition-audio, statistics and
Arcade-pickup gaps against OpenTyrian. Victory, Game Over and Secret cues use
finite variants whose source Bxx loop is removed at build time, and runtime
tests require each cue to stop naturally exactly once. End-level statistics
stay over the live gameplay frame, use the smaller TINY font, preserve the
source hue-15 text treatment, and draw Data Cubes with `JE_drawCube` hue 9.
Arcade mode now consumes and applies front weapon, rear weapon, sidekick,
special and purple-ball drops through the source value ranges.

The v43 release moves the Next Level planets and route dots from Mode-4
software composition to true 8bpp OBJ/OAM, retaining stock SHP graphics,
palette 17, shadows, animation and ornate-frame clipping. A global,
stock-derived 15x15 navigation phase table removes runtime grid and SHP
decoding during camera movement. Its 128-byte aligned IWRAM/ARM copy path
passes 600-frame idle, 120 cursor switches and 40 camera transitions with
zero missed VBlanks while Maxmod remains active.

Player ship, generator and shield state now come from the selected episode's
HDT database, including Episode 4's embedded item records. Ordinary ships are
drawn from the source Sprite2 bank. A dedicated 1 KiB player VRAM cache keeps
banking animation from reducing the 24-frame enemy working set; this was
validated by a zero-drop four-level Episode 1 campaign.

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
build/tyrian_gba_level1_pc_flow_mode4_romfs_v40_detail_high_speed_normal.gba
```

`build.ps1` also builds deterministic gameplay, death, JukeBox, Demo, Arcade,
Episode 2/3/4 first-route, Episode 1 four-level campaign and all-62-section
ROMFS matrix tests. It runs every ROM under mGBA and checks SRAM telemetry,
finite audio completion, cache/drop invariants and GBA memory budgets. After
a successful run, historical and test ROMs move to `Backup`, rebuildable
intermediates are removed, and `build` retains only the latest release ROM.
Pass `-KeepIntermediates` when debugging requires ELF, map, log, save, preview
and verification files.

The Next Level presentation has two additional deterministic performance
targets:

```powershell
make frontend-nav-stress DETAIL_LEVEL=high GAME_SPEED=normal
make frontend-nav-camera-stress DETAIL_LEVEL=high GAME_SPEED=normal
```

They verify idle OBJ animation, Tyrian/Exit selection patches and genuine
camera easing independently of the gameplay regressions.

The GBA toolchain is kept under `tools/gba-sdk`.  Generated native resources
are under `res`, the current release ROM is under `build`, historical ROMs are
under `Backup`, and the reproducible source asset conversions are
`tools/build_assets.py` and `tools/build_romfs.py`.

## Documentation

- [v40 source-parity closure and QA](MD/Tyrian-GBA-Source-Parity-QA-v40.md)
- [v37 source audio, max single weapon and Episode 4 Sprite parity](MD/Tyrian-GBA-Source-Audio-Gameplay-Parity-v37.md)
- [v37 updated plan](MD/Tyrian-GBA-Updated-Plan-v37.md)
- [v38 Secret/end-flow/statistics/palette parity](MD/Tyrian-GBA-Secret-EndFlow-Stats-Palette-v38.md)
- [v38 updated plan](MD/Tyrian-GBA-Updated-Plan-v38.md)
- [v39 normal weapon and mortal-player release](MD/Tyrian-GBA-Normal-Weapon-Mortal-Player-v39.md)
- [v39 updated plan](MD/Tyrian-GBA-Updated-Plan-v39.md)
- [v36 hotpath and IWRAM evaluation](MD/Tyrian-GBA-Hotpath-Evaluation-v36.md)
- [v36 updated plan](MD/Tyrian-GBA-Updated-Plan-v36.md)
- [v35 fixed-timestep drop-frame and ARM7 evaluation](MD/Tyrian-GBA-Drop-Frame-ARM7-v35.md)
- [v35 updated plan](MD/Tyrian-GBA-Updated-Plan-v35.md)
- [v34 full-loadout optimization](MD/Tyrian-GBA-Full-Loadout-Optimization-v34.md)
- [v32 collision-safe background cache and VRAM partition](MD/Tyrian-GBA-Background-Cache-VRAM-v32.md)
- [v32 knowledgebase strategy evaluation](MD/Tyrian-GBA-Knowledgebase-Strategy-Evaluation-v32.md)
- [v31 Episode 2 background performance](MD/Tyrian-GBA-EP2-Background-Performance-v31.md)
- [v31 pre-implementation performance evaluation](MD/Tyrian-GBA-EP2-Performance-Evaluation-2026-07-27.md)
- [v31 updated plan](MD/Tyrian-GBA-Updated-Plan-v31.md)
- [v30 Episode background sentinel parity](MD/Tyrian-GBA-Episode-Background-Sentinel-v30.md)
- [v29 Boss Sprite2 raw/L2 performance](MD/Tyrian-GBA-Boss-Sprite2-L2-v29.md)
- [v29 updated plan](MD/Tyrian-GBA-Updated-Plan-v29.md)
- [v28 all-level ROMFS runtime](MD/Tyrian-GBA-ROMFS-All-Levels-v28.md)
- [Source-parity first-level port](MD/Tyrian-GBA-Source-Parity-Port.md)
- [v23 development invincibility and menu-music transition](MD/Tyrian-GBA-Dev-Invincibility-Music-v23.md)
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
