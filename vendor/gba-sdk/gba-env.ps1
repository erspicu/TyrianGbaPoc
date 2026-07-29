$ErrorActionPreference = "Stop"

$sdkRoot = (Resolve-Path (Split-Path -Parent $MyInvocation.MyCommand.Path)).Path
$workspaceRoot = (Resolve-Path (Join-Path $sdkRoot "..\..")).Path
$msysRoot = Join-Path $workspaceRoot "tools\portable-msys2"
$devkitArm = Join-Path $workspaceRoot ".toolchain\arm-gnu-toolchain"

$requiredPaths = @(
    (Join-Path $devkitArm "bin")
    (Join-Path $sdkRoot "tools\bin")
    (Join-Path $msysRoot "usr\bin")
)
foreach ($requiredPath in $requiredPaths) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Required GBA SDK path is missing: $requiredPath"
    }
}

$existingPaths = @($env:Path -split ";" | Where-Object { $_ })
$remainingPaths = @(
    $existingPaths | Where-Object {
        $candidate = $_
        -not ($requiredPaths | Where-Object {
            $_.Equals($candidate, [StringComparison]::OrdinalIgnoreCase)
        })
    }
)
$env:Path = (@($requiredPaths) + $remainingPaths) -join ";"
$env:GBA_SDK_ROOT = $sdkRoot
$env:DEVKITPRO = $sdkRoot
$env:DEVKITARM = $devkitArm

Write-Host "GBA SDK environment ready"
Write-Host "  GBA_SDK_ROOT=$env:GBA_SDK_ROOT"
Write-Host "  DEVKITARM=$env:DEVKITARM"
Write-Host "  Compiler=$((Get-Command arm-none-eabi-gcc).Source)"
