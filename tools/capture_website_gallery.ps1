param(
    [ValidateSet("low", "normal", "high", "pentium", "custom")]
    [string]$DetailLevel = "low",
    [ValidateSet("low", "normal")]
    [string]$GameSpeed = "normal",
    [string]$OutputDirectory = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$buildDir = Join-Path $projectRoot "build"
$galleryDir = if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    Join-Path $projectRoot "Website\assets\images\gallery"
} else {
    [IO.Path]::GetFullPath($OutputDirectory)
}
$bash = Join-Path $projectRoot "tools\portable-msys2\usr\bin\bash.exe"
$headless = Join-Path $projectRoot "vendor\mgba\mgba-headless.exe"
$python = Join-Path $projectRoot ".venv\Scripts\python.exe"
$armBin = Join-Path $projectRoot ".toolchain\arm-gnu-toolchain\bin"
$sdkTools = Join-Path $projectRoot "vendor\gba-sdk\tools\bin"

function Convert-ToMsysPath {
    param([Parameter(Mandatory)][string]$Path)

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

function Assert-Png240x160 {
    param([Parameter(Mandatory)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "mGBA did not create the expected screenshot: $Path"
    }
    $bytes = [IO.File]::ReadAllBytes($Path)
    if (
        $bytes.Length -lt 24 -or
        $bytes[0] -ne 0x89 -or
        $bytes[1] -ne 0x50 -or
        $bytes[2] -ne 0x4e -or
        $bytes[3] -ne 0x47
    ) {
        throw "Capture is not a PNG file: $Path"
    }
    $width = (
        ($bytes[16] -shl 24) -bor
        ($bytes[17] -shl 16) -bor
        ($bytes[18] -shl 8) -bor
        $bytes[19]
    )
    $height = (
        ($bytes[20] -shl 24) -bor
        ($bytes[21] -shl 16) -bor
        ($bytes[22] -shl 8) -bor
        $bytes[23]
    )
    if ($width -ne 240 -or $height -ne 160) {
        throw "Unexpected capture dimensions ${width}x${height}: $Path"
    }
}

foreach ($required in @($bash, $headless, $python)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Required project-local tool is missing: $required"
    }
}

New-Item -ItemType Directory -Force -Path $buildDir, $galleryDir | Out-Null

$captures = @(
    [pscustomobject]@{ State = 0; File = "title-current.png" },
    [pscustomobject]@{ State = 7; File = "game-menu-current.png" },
    [pscustomobject]@{ State = 8; File = "next-level-current.png" },
    [pscustomobject]@{ State = 12; File = "upgrade-ship-current.png" },
    [pscustomobject]@{ State = 15; File = "data-cubes-current.png" },
    [pscustomobject]@{ State = 9; File = "level-stats-current.png" }
)

$projectMsys = Convert-ToMsysPath $projectRoot
$pythonMsys = Convert-ToMsysPath $python
$armMsys = Convert-ToMsysPath $armBin
$sdkToolsMsys = Convert-ToMsysPath $sdkTools
$suffix = "detail_${DetailLevel}_speed_${GameSpeed}_detailasm1"

foreach ($capture in $captures) {
    $state = [int]$capture.State
    $targetName = "tyrian_gba_frontend_capture_state${state}_${suffix}"
    $romPath = Join-Path $buildDir "$targetName.gba"
    $destination = Join-Path $galleryDir $capture.File
    $command = @"
set -e
export PATH="/usr/bin:${armMsys}:${sdkToolsMsys}:`$PATH"
cd "${projectMsys}"
make PYTHON="${pythonMsys}" DETAIL_LEVEL="${DetailLevel}" GAME_SPEED="${GameSpeed}" CAPTURE_STATE="${state}" frontend-capture
"@

    Write-Host "Building front-end capture state $state..."
    & $bash -lc $command
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $romPath)) {
        throw "Front-end capture build failed for state $state"
    }

    Write-Host "Capturing $($capture.File)..."
    $process = Start-Process `
        -FilePath $headless `
        -ArgumentList @(
            "-l", "0",
            "-O", $destination,
            "-S", "3",
            (Split-Path -Leaf $romPath)
        ) `
        -WorkingDirectory $buildDir `
        -WindowStyle Hidden `
        -Wait `
        -PassThru
    if ($process.ExitCode -ne 0) {
        throw "mGBA capture failed for state $state"
    }
    Assert-Png240x160 -Path $destination
}

Write-Host ""
Write-Host "$DetailLevel-detail website front-end captures updated: $galleryDir"
