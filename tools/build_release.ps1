param(
    [ValidateSet("config", "low", "normal", "high", "pentium", "custom")]
    [string]$DetailLevel = "config",
    [ValidateSet("low", "normal")]
    [string]$GameSpeed = "normal",
    [switch]$RebuildAssets
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Get-Sha256Hex {
    param(
        [Parameter(Mandatory)]
        [string]$Path
    )

    $stream = [IO.File]::OpenRead([IO.Path]::GetFullPath($Path))
    $sha256 = [Security.Cryptography.SHA256]::Create()
    try {
        return [BitConverter]::ToString(
            $sha256.ComputeHash($stream)
        ).Replace("-", "").ToLowerInvariant()
    } finally {
        $sha256.Dispose()
        $stream.Dispose()
    }
}

$projectRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$bootstrap = Join-Path $projectRoot "tools\bootstrap.ps1"
$toolchainRoot = Join-Path $projectRoot ".toolchain"
$armBin = Join-Path $toolchainRoot "arm-gnu-toolchain\bin"
$gcc = Join-Path $armBin "arm-none-eabi-gcc.exe"
$msysRoot = Join-Path $projectRoot "tools\portable-msys2"
$bash = Join-Path $msysRoot "usr\bin\bash.exe"
$sdkRoot = Join-Path $projectRoot "vendor\gba-sdk"
$sdkTools = Join-Path $sdkRoot "tools\bin"
$venvPython = Join-Path $projectRoot ".venv\Scripts\python.exe"
$buildDir = Join-Path $projectRoot "build"
$backupDir = Join-Path $projectRoot "Backup"
$stageRoot = Join-Path $toolchainRoot "release-staging"
$stageRom = Join-Path $stageRoot "TyrianGBA.gba"
$stageSave = Join-Path $stageRoot "TyrianGBA.sav"
$finalRom = Join-Path $buildDir "TyrianGBA.gba"
$finalSave = Join-Path $buildDir "TyrianGBA.sav"
$generatedName =
    "tyrian_gba_level1_pc_flow_mode4_romfs_v40_" +
    "detail_${DetailLevel}_speed_${GameSpeed}_detailasm1.gba"
$generatedRom = Join-Path $buildDir $generatedName

function Assert-ChildPath {
    param(
        [Parameter(Mandatory)]
        [string]$Path,
        [Parameter(Mandatory)]
        [string]$Parent
    )

    $fullPath = [IO.Path]::GetFullPath($Path)
    $fullParent = [IO.Path]::GetFullPath($Parent).TrimEnd(
        [IO.Path]::DirectorySeparatorChar
    )
    $prefix = $fullParent + [IO.Path]::DirectorySeparatorChar
    if (-not $fullPath.StartsWith(
        $prefix,
        [StringComparison]::OrdinalIgnoreCase
    )) {
        throw "Refusing to modify a path outside $fullParent`: $fullPath"
    }
}

function Convert-ToMsysPath {
    param(
        [Parameter(Mandatory)]
        [string]$Path
    )

    $full = [IO.Path]::GetFullPath($Path)
    if ($full -notmatch "^(?<drive>[A-Za-z]):\\(?<tail>.*)$") {
        throw "Only absolute Windows drive paths are supported: $full"
    }
    return (
        "/" +
        $Matches.drive.ToLowerInvariant() +
        "/" +
        $Matches.tail.Replace("\", "/")
    )
}

if (
    -not (Test-Path -LiteralPath $gcc -PathType Leaf) -or
    -not (Test-Path -LiteralPath $venvPython -PathType Leaf)
) {
    & $bootstrap
    if ($LASTEXITCODE -ne 0) {
        throw "Project bootstrap failed."
    }
}

foreach ($required in @(
    $bash,
    $gcc,
    $venvPython,
    (Join-Path $sdkTools "gbafix.exe"),
    (Join-Path $sdkTools "mmutil.exe")
)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Required build component is missing: $required"
    }
}

New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
New-Item -ItemType Directory -Force -Path $backupDir | Out-Null
New-Item -ItemType Directory -Force -Path $stageRoot | Out-Null

# A tester's SRAM is not a reproducible build product.  Protect it from the
# final build-directory cleanup and restore the exact bytes beside the ROM.
$preserveSave = Test-Path -LiteralPath $finalSave -PathType Leaf
if (Test-Path -LiteralPath $stageSave) {
    Assert-ChildPath -Path $stageSave -Parent $toolchainRoot
    Remove-Item -LiteralPath $stageSave -Force
}
if ($preserveSave) {
    Copy-Item -LiteralPath $finalSave -Destination $stageSave
}

# Preserve all prior playable ROMs before producing the new one.
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$ordinal = 0
foreach ($oldRom in Get-ChildItem -LiteralPath $buildDir -Filter "*.gba" -File) {
    $shortHash = (Get-Sha256Hex $oldRom.FullName).Substring(0, 8)
    do {
        $suffix = if ($ordinal -eq 0) { "" } else { "-$ordinal" }
        $archiveName =
            "$($oldRom.BaseName)-$stamp-$shortHash$suffix.gba"
        $archivePath = Join-Path $backupDir $archiveName
        $ordinal++
    } while (Test-Path -LiteralPath $archivePath)
    Move-Item -LiteralPath $oldRom.FullName -Destination $archivePath
}

$projectMsys = Convert-ToMsysPath $projectRoot
$pythonMsys = Convert-ToMsysPath $venvPython
$armMsys = Convert-ToMsysPath $armBin
$sdkToolsMsys = Convert-ToMsysPath $sdkTools
$cleanTarget = if ($RebuildAssets) { "distclean" } else { "clean" }
$command = @'
set -e
export PATH="/usr/bin:__ARM__:__SDK__:$PATH"
cd "__PROJECT__"
make PYTHON="__PYTHON__" DETAIL_LEVEL="__DETAIL__" GAME_SPEED="__SPEED__" __CLEAN__
make PYTHON="__PYTHON__" DETAIL_LEVEL="__DETAIL__" GAME_SPEED="__SPEED__" assets all
'@
$command = $command.Replace("__ARM__", $armMsys)
$command = $command.Replace("__SDK__", $sdkToolsMsys)
$command = $command.Replace("__PROJECT__", $projectMsys)
$command = $command.Replace("__PYTHON__", $pythonMsys)
$command = $command.Replace("__DETAIL__", $DetailLevel)
$command = $command.Replace("__SPEED__", $GameSpeed)
$command = $command.Replace("__CLEAN__", $cleanTarget)

Write-Host "Building the final Tyrian GBA ROM..."
& $bash -lc $command
if ($LASTEXITCODE -ne 0) {
    throw "GBA ROM build failed with exit code $LASTEXITCODE"
}
if (-not (Test-Path -LiteralPath $generatedRom -PathType Leaf)) {
    throw "Expected release ROM is missing: $generatedRom"
}

$romBytes = [IO.File]::ReadAllBytes($generatedRom)
if ($romBytes.Length -lt 192 -or $romBytes.Length -gt 32MB) {
    throw "ROM size is outside the valid GBA cartridge range."
}
$gameCode = [Text.Encoding]::ASCII.GetString($romBytes, 0xAC, 4)
if ($gameCode -ne "TYGA") {
    throw "Unexpected GBA game code '$gameCode' (expected TYGA)."
}

if (Test-Path -LiteralPath $stageRom) {
    Assert-ChildPath -Path $stageRom -Parent $toolchainRoot
    Remove-Item -LiteralPath $stageRom -Force
}
Copy-Item -LiteralPath $generatedRom -Destination $stageRom

# The project policy keeps exactly one final ROM under build/. Everything
# else is reproducible and is discarded after the successful staged copy.
Assert-ChildPath -Path $buildDir -Parent $projectRoot
foreach ($entry in Get-ChildItem -LiteralPath $buildDir -Force) {
    Assert-ChildPath -Path $entry.FullName -Parent $buildDir
    Remove-Item -LiteralPath $entry.FullName -Recurse -Force
}
Move-Item -LiteralPath $stageRom -Destination $finalRom
if ($preserveSave) {
    Move-Item -LiteralPath $stageSave -Destination $finalSave
}

$hash = Get-Sha256Hex $finalRom
$sizeMiB = [math]::Round((Get-Item -LiteralPath $finalRom).Length / 1MB, 2)

Write-Host ""
Write-Host "Final ROM created successfully."
Write-Host "  File:   $finalRom"
Write-Host "  Size:   $sizeMiB MiB"
Write-Host "  SHA256: $hash"
