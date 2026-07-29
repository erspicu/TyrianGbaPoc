param(
    [switch]$ForceToolchain,
    [switch]$SkipPythonPackages
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
$toolchainRoot = Join-Path $projectRoot ".toolchain"
$armRoot = Join-Path $toolchainRoot "arm-gnu-toolchain"
$armGcc = Join-Path $armRoot "bin\arm-none-eabi-gcc.exe"
$downloadRoot = Join-Path $toolchainRoot "downloads"
$archiveName =
    "arm-gnu-toolchain-15.2.rel1-mingw-w64-x86_64-arm-none-eabi.zip"
$archivePath = Join-Path $downloadRoot $archiveName
$archiveUrl =
    "https://developer.arm.com/-/media/Files/downloads/gnu/15.2.rel1/binrel/" +
    $archiveName
$archiveSha256 =
    "7936cac895611023ffb22a64b8e426098c7104cb689778c1894572ca840b9ece"
$venvRoot = Join-Path $projectRoot ".venv"
$venvPython = Join-Path $venvRoot "Scripts\python.exe"
$requirements = Join-Path $projectRoot "requirements.txt"

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

function Test-ArchiveHash {
    if (-not (Test-Path -LiteralPath $archivePath -PathType Leaf)) {
        return $false
    }
    $actual = Get-Sha256Hex $archivePath
    return $actual -eq $archiveSha256
}

foreach ($required in @(
    (Join-Path $projectRoot "tools\portable-msys2\usr\bin\bash.exe"),
    (Join-Path $projectRoot "tools\portable-msys2\usr\bin\make.exe"),
    (Join-Path $projectRoot "vendor\gba-sdk\libgba\lib\libgba.a"),
    (Join-Path $projectRoot "vendor\gba-sdk\maxmod\lib\libmm.a"),
    (Join-Path $projectRoot "vendor\gba-sdk\tools\bin\gbafix.exe"),
    (Join-Path $projectRoot "vendor\gba-sdk\tools\bin\mmutil.exe"),
    (Join-Path $projectRoot "vendor\tyrian\data\tyrian.hdt"),
    (Join-Path $projectRoot "vendor\opentyrian\REVISION")
)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Repository dependency is missing: $required"
    }
}

New-Item -ItemType Directory -Force -Path $toolchainRoot | Out-Null
New-Item -ItemType Directory -Force -Path $downloadRoot | Out-Null

if ($ForceToolchain -and (Test-Path -LiteralPath $armRoot)) {
    Assert-ChildPath -Path $armRoot -Parent $toolchainRoot
    Remove-Item -LiteralPath $armRoot -Recurse -Force
}

if (-not (Test-Path -LiteralPath $armGcc -PathType Leaf)) {
    if (-not (Test-ArchiveHash)) {
        if (Test-Path -LiteralPath $archivePath) {
            Assert-ChildPath -Path $archivePath -Parent $toolchainRoot
            Remove-Item -LiteralPath $archivePath -Force
        }
        Write-Host "Downloading pinned Arm GNU Toolchain 15.2.Rel1..."
        Invoke-WebRequest -Uri $archiveUrl -OutFile $archivePath
        if (-not (Test-ArchiveHash)) {
            throw "Arm toolchain archive SHA-256 validation failed."
        }
    }

    $extractRoot = Join-Path $toolchainRoot "extract-arm-15.2"
    if (Test-Path -LiteralPath $extractRoot) {
        Assert-ChildPath -Path $extractRoot -Parent $toolchainRoot
        Remove-Item -LiteralPath $extractRoot -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $extractRoot | Out-Null

    Write-Host "Extracting the Arm toolchain inside this project..."
    Expand-Archive -LiteralPath $archivePath -DestinationPath $extractRoot
    New-Item -ItemType Directory -Force -Path $armRoot | Out-Null
    foreach ($entry in Get-ChildItem -LiteralPath $extractRoot -Force) {
        Move-Item -LiteralPath $entry.FullName -Destination $armRoot
    }
    Assert-ChildPath -Path $extractRoot -Parent $toolchainRoot
    Remove-Item -LiteralPath $extractRoot -Recurse -Force
}

if (-not (Test-Path -LiteralPath $armGcc -PathType Leaf)) {
    throw "Arm compiler installation did not produce $armGcc"
}

if (-not $SkipPythonPackages) {
    if (-not (Test-Path -LiteralPath $venvPython -PathType Leaf)) {
        $hostPython = (Get-Command python -ErrorAction Stop).Source
        Write-Host "Creating the project-local Python environment..."
        & $hostPython -m venv $venvRoot
        if ($LASTEXITCODE -ne 0) {
            throw "Python virtual environment creation failed."
        }
    }

    Write-Host "Installing pinned Python build dependencies..."
    & $venvPython -m pip install --disable-pip-version-check `
        --requirement $requirements
    if ($LASTEXITCODE -ne 0) {
        throw "Python dependency installation failed."
    }
}

$compilerVersion = (& $armGcc --version | Select-Object -First 1)
Write-Host ""
Write-Host "TyrianGbaPoc build environment is ready."
Write-Host "  Compiler: $compilerVersion"
if (-not $SkipPythonPackages) {
    Write-Host "  Python:   $venvPython"
}
