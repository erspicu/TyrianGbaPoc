# Vendored build inputs

This directory contains the pinned inputs required to build and validate
TyrianGbaPoc without reaching outside the repository:

- `tyrian/`: Tyrian 2.1 stock data plus selected source image sheets.
- `opentyrian/`: the OpenTyrian source snapshot used as the behavioural
  specification. `REVISION` records its upstream commit.
- `audio/`: tracker sources and calibration metadata used by the GBA build.
- `builders/`: reusable NES/SNES-era asset parsers and the IT structure
  template used by the GBA audio pipeline.
- `gba-sdk/`: GBA CRT/specs, libgba, Maxmod and host-side resource tools.
- `mgba/`: the headless and performance runners used by regression scripts.

Generated GBA resources do not belong here. They are recreated under `res/`.
Licensing and upstream locations are listed in the repository-level
`THIRD_PARTY_NOTICES.md`.
