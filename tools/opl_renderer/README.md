# Tyrian OPL sample renderer

`tyrian_opl_bridge.dll` is the deterministic Windows host bridge used by the
GBA asset builder. It renders the original 46-byte LDS/OPL2 patches through
the unmodified OpenTyrian/DOSBox core in `vendor/opentyrian/src/opl.c`.

Normal ROM builds use the checked-in DLL and do not require a C compiler. To
rebuild it, install LLVM for Windows and run:

```powershell
powershell -ExecutionPolicy Bypass -File tools/opl_renderer/rebuild.ps1
```

The DLL is a separate dynamically linked LGPL component. Its corresponding
source is this bridge plus the vendored `opl.c`/`opl.h`; see
`THIRD_PARTY_NOTICES.md` and `vendor/opentyrian/COPYING`.
