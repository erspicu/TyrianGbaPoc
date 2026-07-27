param(
    [ValidateSet("low", "normal", "high", "pentium")]
    [string]$DetailLevel = "pentium",
    [ValidateSet(
        "baseline",
        "no_collision",
        "no_render",
        "precache_cull",
        "active_mask"
    )]
    [string]$Variant = "active_mask",
    [switch]$NoBuild
)

$ErrorActionPreference = "Stop"

$projectRoot = [IO.Path]::GetFullPath(
    (Join-Path $PSScriptRoot "..")
)
$workspaceRoot = [IO.Path]::GetFullPath(
    (Join-Path $projectRoot "..\..")
)
$buildDir = Join-Path $projectRoot "build"
$bash = Join-Path $workspaceRoot "tools\msys64\usr\bin\bash.exe"
$headless = Join-Path (
    $workspaceRoot
) "org\mgba\build-ucrt-headless\mgba-headless.exe"
$ucrtBin = Join-Path $workspaceRoot "tools\msys64\ucrt64\bin"
$python = "/c/Python314/python.exe"
$name = (
    "tyrian_gba_full_loadout_sprite_stress_ep2_v34_" +
    "${Variant}_detail_${DetailLevel}_speed_normal"
)
$rom = Join-Path $buildDir "$name.gba"
$save = Join-Path $buildDir "$name.sav"
$stdout = Join-Path $buildDir "${name}_mgba_stdout.txt"
$stderr = Join-Path $buildDir "${name}_mgba_stderr.txt"
$json = Join-Path $buildDir "${name}_telemetry.json"

if (
    -not $projectRoot.StartsWith(
        [IO.Path]::GetFullPath($workspaceRoot),
        [StringComparison]::OrdinalIgnoreCase
    )
) {
    throw "Project root escaped the expected workspace: $projectRoot"
}
if (-not $NoBuild) {
    $msysProject = $projectRoot.Replace("\", "/").Replace("C:", "/c")
    $command = (
        "set -e; " +
        "export PATH=/ucrt64/bin:" +
        "/c/ai_project/AprTyrianNes/tools/gba-sdk/tools/bin:`$PATH; " +
        "cd '$msysProject'; " +
        "make -j2 PYTHON=$python " +
        "DETAIL_LEVEL=$DetailLevel GAME_SPEED=normal " +
        "STRESS_DIAGNOSTIC=$Variant full-loadout-stress"
    )
    & $bash -lc $command
    if ($LASTEXITCODE -ne 0) {
        throw "Stress ROM build failed with code $LASTEXITCODE"
    }
}
if (-not (Test-Path -LiteralPath $rom -PathType Leaf)) {
    throw "Stress ROM is missing: $rom"
}

foreach ($old in @($save, $stdout, $stderr, $json)) {
    if (Test-Path -LiteralPath $old -PathType Leaf) {
        Remove-Item -LiteralPath $old -Force
    }
}
$env:PATH = "$ucrtBin;$env:PATH"
$process = Start-Process `
    -FilePath $headless `
    -ArgumentList @("-S", "3", "$name.gba") `
    -WorkingDirectory $buildDir `
    -WindowStyle Hidden `
    -RedirectStandardOutput $stdout `
    -RedirectStandardError $stderr `
    -PassThru
if (-not $process.WaitForExit(120000)) {
    $process.Kill($true)
    throw "Stress runtime timed out: $name"
}
if ($process.ExitCode -ne 0) {
    throw "Stress runtime exited with code $($process.ExitCode): $name"
}
if (-not (Test-Path -LiteralPath $save -PathType Leaf)) {
    throw "Stress runtime did not produce SRAM: $save"
}
$runtimeErrors = @(
    Select-String `
        -Path $stdout, $stderr `
        -Pattern "Bad memory|Invalid|Illegal|Hard crash|Fatal|Failed|Error"
)
if ($runtimeErrors.Count -ne 0) {
    throw "mGBA reported $($runtimeErrors.Count) runtime errors"
}

$bytes = [IO.File]::ReadAllBytes($save)
if ($bytes.Length -lt 236) {
    throw "Stress SRAM is truncated: $($bytes.Length) bytes"
}
$magic = [Text.Encoding]::ASCII.GetString($bytes, 0, 4)
if ($magic -ne "TGW4") {
    throw "Unexpected stress telemetry schema: $magic"
}
function Read-U32 {
    param([int]$Offset)

    return [BitConverter]::ToUInt32($bytes, $Offset)
}

$logicUpdates = Read-U32 20
$logicCyclesTotal = Read-U32 176
$renderCyclesTotal = Read-U32 184
$collisionCyclesTotal = Read-U32 192
$telemetry = [ordered]@{
    schema = $magic
    variant = $Variant
    detail_level = Read-U32 8
    game_speed = Read-U32 12
    display_frames = Read-U32 16
    logic_updates = $logicUpdates
    missed_vblanks = Read-U32 24
    max_oam = Read-U32 28
    player_shot_spawns = Read-U32 32
    player_shot_drops = Read-U32 36
    player_shot_max_active = Read-U32 40
    player_chain_volleys = Read-U32 44
    projectile_cache_drops = Read-U32 48
    projectile_cache_max_visible_unique = Read-U32 52
    enemy_cache_drops = Read-U32 56
    enemy_cache_max_visible_unique = Read-U32 60
    sprite2_l2_drops = Read-U32 64
    effect_cache_drops = Read-U32 68
    pickup_explosion_drops = Read-U32 72
    pickup_explosion_max_active = Read-U32 76
    source_max_visible_enemies = Read-U32 80
    source_unknown_visuals = Read-U32 84
    background_approximations = Read-U32 88
    loadout_failures = Read-U32 92
    vblank_irqs = Read-U32 96
    level_position = Read-U32 100
    active_shots_at_finish = Read-U32 104
    projectile_cache_slots = Read-U32 108
    enemy_cache_slots = Read-U32 112
    player_shot_slots = Read-U32 116
    source_assets_valid = Read-U32 120
    option_blend_draws = Read-U32 128
    option_projectile_valid = Read-U32 132
    detail_lava_frames = Read-U32 136
    detail_water_frames = Read-U32 140
    detail_iced_frames = Read-U32 144
    detail_blur_frames = Read-U32 148
    detail_wild_frames = Read-U32 152
    stress_psg_triggers = Read-U32 156
    culled_offscreen_before_cache = Read-U32 160
    culled_oam_full_before_cache = Read-U32 164
    post_visibility_cache_acquires = Read-U32 168
    visible_cache_capacity_drops = Read-U32 172
    logic_cycles_total = $logicCyclesTotal
    logic_cycles_max = Read-U32 180
    render_cycles_total = $renderCyclesTotal
    render_cycles_max = Read-U32 188
    collision_cycles_total = $collisionCyclesTotal
    collision_cycles_max = Read-U32 196
    diagnostic_flags = Read-U32 200
    projectile_cache_hits = Read-U32 204
    projectile_cache_misses = Read-U32 208
    projectile_cache_evictions = Read-U32 212
    projectile_cache_uploads = Read-U32 216
    collision_mask_rebuilds = Read-U32 220
    collision_candidate_visits = Read-U32 224
    collision_linear_slot_visits = Read-U32 228
    collision_mask_active_at_finish = Read-U32 232
    logic_cycles_average = if ($logicUpdates) {
        [math]::Round($logicCyclesTotal / $logicUpdates, 2)
    } else {
        0
    }
    render_cycles_average = if ($logicUpdates) {
        [math]::Round($renderCyclesTotal / $logicUpdates, 2)
    } else {
        0
    }
    collision_cycles_average = if ($logicUpdates) {
        [math]::Round($collisionCyclesTotal / $logicUpdates, 2)
    } else {
        0
    }
}
if (
    $bytes[4] -ne 1 -or
    $bytes[5] -ne 1 -or
    $telemetry.loadout_failures -ne 0 -or
    $telemetry.source_assets_valid -ne 1
) {
    throw "Stress telemetry reported a validation failure"
}

$telemetry |
    ConvertTo-Json -Depth 3 |
    Set-Content -LiteralPath $json -Encoding utf8
[pscustomobject]$telemetry
