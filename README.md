# TyrianGbaPoc

TyrianGbaPoc is an in-progress Game Boy Advance port of Tyrian, driven by the
original Tyrian 2.1 data and OpenTyrian source behaviour.

This project originally began as a technical experiment: could the PC version
of Tyrian be brought to the GBA as faithfully as possible despite the
handheld's much tighter CPU, memory, video and audio limits? After overcoming
a long series of rendering, streaming, cache, timing and asset-pipeline
problems, the answer proved to be yes. The project therefore changed direction
from a one-level proof of concept to an effort to build a broadly complete,
maintainable and source-faithful GBA port.

It is still under active development. Some menus, campaign paths and less
common gameplay behaviours remain work in progress, but the project is no
longer designed as a throwaway demo.

## What already works

- Original logo, new-game setup, Game Menu, Options, eleven-slot SRAM
  Save/Load, Next Level, Demo and JukeBox flow.
- Stock MUS/SHP/PIC/HDT/LVL data loaded from a cartridge ROMFS.
- Source-driven level events, four enemy pools, movement, projectiles,
  collision, damage, linked destruction, rewards, bosses and end-level flow.
- Three independently scrolling background layers and the original gameplay
  draw-order semantics adapted to GBA BG/OBJ priorities.
- A 1:1 centre crop of the PC gameplay viewport: gameplay coordinates remain
  source coordinates and the GBA performs only the final 240x160 crop.
- All 128 GBA OAM entries, palette-aware Sprite2 caching and build-time
  lossless expansion of the stock shape banks.
- Tracker music, sound effects and voices through Maxmod.
- Fixed-timestep gameplay with controlled frame dropping under unusually heavy
  scenes, without slowing the game clock.
- Automated mGBA regressions for gameplay, episodes, menus, Demo, JukeBox,
  death/end-level flow, ROMFS coverage and performance telemetry.

The implementation treats OpenTyrian as the gameplay specification. Platform
changes are kept in GBA presentation, input, audio, storage and performance
adapters instead of replacing authored level behaviour with per-level tables.

## Build

On Windows 10/11 with Python 3.10 or newer, run:

```text
Build-GBA-ROM.bat
```

The first run installs a pinned ARM compiler inside the project. The final ROM
is written to `build/TyrianGBA.gba`; previous local ROMs are moved to
`Backup/`. No system-wide GBA SDK installation is required.

For environment details, directory layout, advanced options and full
regression commands, see [BUILDING.md](BUILDING.md).
Developer switches and all supported HUD/menu coordinates are documented
directly in the bilingual [Configure.h](Configure.h).

The optional Windows [TyrianSaveEditor](TyrianSaveEditor/README.md) provides
both a WinForms UI and CLI for creating, inspecting and editing emulator
`.sav` files without weakening the game's dual-bank/CRC format.

## Controls

- D-pad Up/Down: select menu items
- A or Start: confirm
- B: go back
- D-pad during play: move
- A or B during play: fire
- Start during play: pause/resume

Save-name editor:

- Up/Down: cycle the current character
- Hold R while pressing Up/Down: choose an uppercase letter
- A or Right: advance the cursor
- Left: move the cursor back
- B: erase; at the first character, return without saving
- Start: save
- Hold Select: clear the whole name

`Load Game` on the title page opens the real 11-slot SRAM browser. Loading an
occupied slot restores the campaign and continues at Game Menu; `B` returns to
the highlighted title item. The same load browser remains available through
`Game Menu > Options > Load`.

## Downloads

Playable milestone ROMs are published as GitHub Release assets rather than
committed to the source tree:

<https://github.com/erspicu/TyrianGbaPoc/releases>

Use legally obtained Tyrian data and an emulator or hardware environment
appropriate for your jurisdiction.

## Project layout

- `src/`, `main.c`: GBA runtime and source-parity translation.
- `vendor/`: pinned Tyrian data, OpenTyrian snapshot, SDK and build inputs.
- `tools/`: reproducible asset/build/test scripts.
- `TyrianSaveEditor/`: source-data-backed WinForms and CLI SRAM editor.
- `MD/`: detailed parity, architecture and performance research notes.
- `Website/`: local project website source.

Third-party licenses and upstream locations are listed in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). This project is based in
part on work from OpenTyrian, devkitPro/libgba, Maxmod and mGBA. Tyrian and
its original assets remain the property of their respective rights holders.
