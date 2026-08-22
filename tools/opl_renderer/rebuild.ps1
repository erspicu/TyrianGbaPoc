param(
    [string]$Clang = "C:\Program Files\LLVM\bin\clang.exe"
)

$ErrorActionPreference = "Stop"
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
$source = Join-Path $PSScriptRoot "tyrian_opl_bridge.c"
$oplSource = Join-Path $root "vendor\opentyrian\src\opl.c"
$oplInclude = Join-Path $root "vendor\opentyrian\src"
$output = Join-Path $PSScriptRoot "tyrian_opl_bridge.dll"
$importLibrary = Join-Path $PSScriptRoot "tyrian_opl_bridge.lib"

if (-not (Test-Path -LiteralPath $Clang -PathType Leaf)) {
    throw "LLVM clang was not found: $Clang"
}

& $Clang `
    -std=c11 `
    -O2 `
    -shared `
    -fuse-ld=lld `
    -I $oplInclude `
    $source `
    $oplSource `
    -o $output
if ($LASTEXITCODE -ne 0) {
    throw "OPL renderer build failed with exit code $LASTEXITCODE"
}

# lld-link emits an import library next to a DLL that exports symbols.  The
# Python asset builder loads the DLL through ctypes and never links against
# that file, so keep the reproducible source tree free of this by-product.
if (Test-Path -LiteralPath $importLibrary -PathType Leaf) {
    Remove-Item -LiteralPath $importLibrary -Force
}

Write-Host "Built $output"
