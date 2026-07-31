# Vendored build inputs

This directory contains the pinned inputs required to build and validate
TyrianGbaPoc without reaching outside the repository:

- `tyrian/`: Tyrian 2.1 stock data plus selected source image sheets.
- `opentyrian/`: the OpenTyrian source snapshot used as the behavioural
  specification. `REVISION` records its upstream commit.
- `audio/`: Tyrian tracker sources and GBA-owned OPL reference measurements.
- `gba-sdk/`: GBA CRT/specs, libgba, Maxmod and host-side resource tools.
- `mgba/`: the headless and performance runners used by regression scripts.

All active asset conversion code and the Maxmod IT template live under
`tools/`.  The build has no dependency on another console project or asset
builder. Generated GBA resources do not belong here; they are recreated under `res/`.
Licensing and upstream locations are listed in the repository-level
`THIRD_PARTY_NOTICES.md`.
