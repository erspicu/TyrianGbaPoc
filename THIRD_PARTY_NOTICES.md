# Third-party notices

TyrianGbaPoc includes or uses the following third-party material. This file is
an attribution and source-location index; the authoritative license text is
the license file shipped beside each component.

## Tyrian 2.1 game data

The stock game data under `vendor/tyrian/data/` is the freeware Tyrian 2.1
data referenced by OpenTyrian. Tyrian and its original assets remain the
property of their respective rights holders. Upstream data location:
<https://camanis.net/tyrian/tyrian21.zip>.

## OpenTyrian

`vendor/opentyrian/` is a pinned source snapshot from
<https://github.com/opentyrian/opentyrian>. It is licensed under GNU GPL
version 2; see `vendor/opentyrian/COPYING`. The recorded upstream revision is
in `vendor/opentyrian/REVISION`.

## libgba and devkitPro-compatible GBA runtime files

The required GBA headers, libraries and CRT/spec files are under
`vendor/gba-sdk/`. libgba is distributed under its LGPL license and
static-linking exception; see
`vendor/gba-sdk/libgba/libgba_license.txt`. Upstream project:
<https://github.com/devkitPro/libgba>.

## Maxmod and mmutil

Maxmod runtime files, headers and host tools are under
`vendor/gba-sdk/maxmod/` and `vendor/gba-sdk/tools/`. See
`vendor/gba-sdk/maxmod/maxmod_license.txt`. Upstream project:
<https://github.com/devkitPro/maxmod>.

## mGBA

The headless/performance test executables and their runtime DLLs under
`vendor/mgba/` come from mGBA 0.10.5. mGBA is licensed under Mozilla Public
License 2.0; see `vendor/mgba/LICENSE`. Upstream project:
<https://github.com/mgba-emu/mgba>.

The bundled runtime DLLs retain their upstream terms: zlib uses the zlib
license, libpng uses the libpng license, and libepoxy uses the MIT license.
Their corresponding upstream projects are
<https://github.com/madler/zlib>, <https://github.com/pnggroup/libpng>, and
<https://github.com/anholt/libepoxy>.

## Portable MSYS2 build subset

`tools/portable-msys2/` contains the minimum MSYS2 Bash/Make runtime needed by
the Windows Makefile. Bash and GNU Make are GNU GPL software; the MSYS2
runtime is maintained at <https://github.com/msys2/msys2-runtime>. Package
sources and build recipes are available from
<https://github.com/msys2/MSYS2-packages>. This subset is supplied only to
make the repository path-independent; it is not used by the GBA ROM itself.

## Arm GNU Toolchain

`tools/bootstrap.ps1` downloads the official Arm GNU Toolchain
15.2.Rel1 Windows `arm-none-eabi` archive from Arm and verifies its pinned
SHA-256 before extracting it to the ignored `.toolchain/` directory. The
toolchain is not stored in Git. License files distributed in the official
archive apply. Product page:
<https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads>.

## Python packages

NumPy and Pillow are installed into the ignored `.venv/` directory from
`requirements.txt`; their packages are not vendored in this repository.
Their upstream projects are <https://numpy.org/> and
<https://python-pillow.org/>.
