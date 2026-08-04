param(
    [ValidateSet("low", "normal", "high", "pentium")]
    [string]$DetailLevel = "pentium",
    [ValidateSet(
        "baseline",
        "no_collision",
        "no_render",
        "precache_cull",
        "active_mask",
        "active_mask_range",
        "active_mask_fast",
        "active_mask_fast_lazy",
        "active_mask_fast_lazy_packed",
        "active_mask_fast_defer",
        "active_mask_fast_wall",
        "active_mask_fast_wall_lazy",
        "active_mask_fast_wall_lazy_no_recovery",
        "active_mask_fast_wall_lazy_packed",
        "active_mask_fast_wall_full",
        "active_mask_fast_wall_bg_live",
        "active_mask_range_fast"
    )]
    [string]$Variant = "active_mask_fast_wall_lazy_packed",
    [ValidateRange(1, 4)]
    [int]$Episode = 2,
    [ValidateRange(1, 65535)]
    [int]$Section = 1,
    [ValidateRange(0, 65535)]
    [int]$EndPosition = 0,
    [ValidateRange(1, 20000)]
    [int]$DurationVBlanks = 3600,
    [string]$ScreenshotPath = "",
    [switch]$NoFire,
    [switch]$NoBuild
)

$ErrorActionPreference = "Stop"

$projectRoot = [IO.Path]::GetFullPath(
    (Join-Path $PSScriptRoot "..")
)
$buildDir = Join-Path $projectRoot "build"
$bash = Join-Path $projectRoot "tools\portable-msys2\usr\bin\bash.exe"
$headless = Join-Path $projectRoot "vendor\mgba\mgba-headless.exe"
$mgbaRoot = Join-Path $projectRoot "vendor\mgba"
$armBin = Join-Path $projectRoot ".toolchain\arm-gnu-toolchain\bin"
$sdkTools = Join-Path $projectRoot "vendor\gba-sdk\tools\bin"
$venvPython = Join-Path $projectRoot ".venv\Scripts\python.exe"
$pythonPath = if (Test-Path -LiteralPath $venvPython -PathType Leaf) {
    $venvPython
} else {
    (Get-Command python -ErrorAction Stop).Source
}
$stopTag = if ($EndPosition -gt 0) {
    "pos$EndPosition"
} else {
    "vb$DurationVBlanks"
}
$inputTag = if ($NoFire) { "_nofire" } else { "" }
$stressFire = if ($NoFire) { 0 } else { 1 }
$name = (
    "tyrian_gba_full_loadout_sprite_stress_" +
    "ep${Episode}_section${Section}_${stopTag}${inputTag}_v70_" +
    "${Variant}_detail_${DetailLevel}_speed_normal"
)
$rom = Join-Path $buildDir "$name.gba"
$save = Join-Path $buildDir "$name.sav"
$stdout = Join-Path $buildDir "${name}_mgba_stdout.txt"
$stderr = Join-Path $buildDir "${name}_mgba_stderr.txt"
$json = Join-Path $buildDir "${name}_telemetry.json"
$screenshot = if ([string]::IsNullOrWhiteSpace($ScreenshotPath)) {
    Join-Path $buildDir "${name}.png"
} else {
    [IO.Path]::GetFullPath($ScreenshotPath)
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

if (-not $NoBuild) {
    $msysProject = Convert-ToMsysPath $projectRoot
    $python = Convert-ToMsysPath $pythonPath
    $msysArmBin = Convert-ToMsysPath $armBin
    $msysSdkTools = Convert-ToMsysPath $sdkTools
    $command = (
        "set -e; " +
        "export PATH='/usr/bin:${msysArmBin}:${msysSdkTools}':`$PATH; " +
        "cd '$msysProject'; " +
        "make -j2 PYTHON=$python " +
        "DETAIL_LEVEL=$DetailLevel GAME_SPEED=normal " +
        "STRESS_EPISODE=$Episode STRESS_SECTION=$Section " +
        "STRESS_END_POSITION=$EndPosition " +
        "STRESS_DURATION_VBLANKS=$DurationVBlanks " +
        "STRESS_FIRE=$stressFire " +
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

foreach ($old in @($save, $stdout, $stderr, $json, $screenshot)) {
    if (Test-Path -LiteralPath $old -PathType Leaf) {
        Remove-Item -LiteralPath $old -Force
    }
}
$env:PATH = "$mgbaRoot;$armBin;$env:PATH"
$process = Start-Process `
    -FilePath $headless `
    -ArgumentList @("-O", $screenshot, "-S", "3", "$name.gba") `
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
if (-not (Test-Path -LiteralPath $screenshot -PathType Leaf)) {
    throw "Stress runtime did not produce a screenshot: $screenshot"
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
if ($bytes.Length -lt 496) {
    throw "Stress SRAM is truncated: $($bytes.Length) bytes"
}
$magic = [Text.Encoding]::ASCII.GetString($bytes, 0, 4)
if ($magic -ne "TGW8") {
    throw "Unexpected stress telemetry schema: $magic"
}
function Read-U32 {
    param([int]$Offset)

    return [BitConverter]::ToUInt32($bytes, $Offset)
}

$logicUpdates = Read-U32 20
$displayFrames = Read-U32 16
$vblankIrqs = Read-U32 96
$logicCyclesTotal = Read-U32 176
$renderCyclesTotal = Read-U32 184
$collisionCyclesTotal = Read-U32 192
$vblankIrqCyclesTotal = Read-U32 236
$commitCyclesTotal = Read-U32 244
$audioInputCyclesTotal = Read-U32 252
$prelogicCyclesTotal = Read-U32 260
$prefetchCyclesTotal = Read-U32 472
$loopWorkCyclesTotal = Read-U32 480
$renderCompleted = Read-U32 276
$diagnosticFlags = Read-U32 200
$vblankRecoveryLoops = Read-U32 328
$audioFrames = Read-U32 332
$commitFrames = $displayFrames - $vblankRecoveryLoops
$audioFrameLoss = [math]::Max(0, $displayFrames - $audioFrames)
$telemetry = [ordered]@{
    schema = $magic
    variant = $Variant
    no_fire = [bool]$NoFire
    detail_level = Read-U32 8
    game_speed = Read-U32 12
    display_frames = $displayFrames
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
    vblank_irqs = $vblankIrqs
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
    diagnostic_flags = $diagnosticFlags
    projectile_cache_hits = Read-U32 204
    projectile_cache_misses = Read-U32 208
    projectile_cache_evictions = Read-U32 212
    projectile_cache_uploads = Read-U32 216
    collision_mask_rebuilds = Read-U32 220
    collision_candidate_visits = Read-U32 224
    collision_linear_slot_visits = Read-U32 228
    collision_mask_active_at_finish = Read-U32 232
    vblank_irq_cycles_total = $vblankIrqCyclesTotal
    vblank_irq_cycles_max = Read-U32 240
    commit_cycles_total = $commitCyclesTotal
    commit_cycles_max = Read-U32 248
    audio_input_cycles_total = $audioInputCyclesTotal
    audio_input_cycles_max = Read-U32 256
    prelogic_cycles_total = $prelogicCyclesTotal
    prelogic_cycles_max = Read-U32 264
    wall_vblanks = Read-U32 268
    presentation_render_attempts = Read-U32 272
    presentation_render_completed = $renderCompleted
    presentation_render_deferred = Read-U32 280
    presentation_render_forced = Read-U32 284
    presentation_superseded = Read-U32 288
    presentation_pending_logic_max = Read-U32 292
    presentation_estimate_max = Read-U32 296
    presentation_estimate_final = Read-U32 300
    presentation_deadline_elapsed_max = Read-U32 304
    logic_catchup_updates = Read-U32 308
    logic_updates_per_loop_max = Read-U32 312
    logic_backlog_frames_max = Read-U32 316
    background_held_rows_max = Read-U32 320
    presentation_pending_at_finish = Read-U32 324
    vblank_recovery_loops = $vblankRecoveryLoops
    vblank_commit_frames = $commitFrames
    audio_frames = $audioFrames
    audio_frame_loss = $audioFrameLoss
    audio_frame_loss_percent = if ($displayFrames) {
        [math]::Round(100.0 * $audioFrameLoss / $displayFrames, 4)
    } else {
        0
    }
    rng_calls = Read-U32 336
    enemy_motion_updates = Read-U32 340
    enemy_shot_motion_updates = Read-U32 344
    round_ratio_calls = Read-U32 348
    enemy_shot_triggers = Read-U32 352
    enemy_launch_successes = Read-U32 356
    rng_benchmark_cycles = Read-U32 360
    rng_benchmark_sink = Read-U32 364
    rng_benchmark_calls = Read-U32 368
    detail_filter_hue_frames = Read-U32 372
    detail_palette_rebuilds = Read-U32 376
    detail_wave_frames = Read-U32 380
    detail_wild_dither_frames = Read-U32 384
    invincible_enabled = Read-U32 388
    stress_loadout_enabled = Read-U32 392
    detail_adapter_self_test = Read-U32 396
    route_episode = Read-U32 400
    route_section = Read-U32 404
    route_lvl_file_number = Read-U32 408
    stop_end_position = Read-U32 412
    stop_duration_vblanks = Read-U32 416
    lava_active_at_finish = Read-U32 420
    water_active_at_finish = Read-U32 424
    lava_data_at_finish = Read-U32 428
    water_data_at_finish = Read-U32 432
    detail_wave_attenuated_frames = Read-U32 436
    detail_wave_pressure_score_max = Read-U32 440
    detail_wave_strength_min_q8 = Read-U32 444
    wave_dispatch_scope_attempts = Read-U32 448
    wave_dispatch_entries = Read-U32 452
    wave_dispatch_exits = Read-U32 456
    wave_dispatch_logic_busy_deferred = Read-U32 460
    wave_dispatch_idle_renders = Read-U32 464
    wave_dispatch_safety_forced = Read-U32 468
    prefetch_cycles_total = $prefetchCyclesTotal
    prefetch_cycles_max = Read-U32 476
    loop_work_cycles_total = $loopWorkCyclesTotal
    loop_work_cycles_max = Read-U32 484
    wave_dispatch_active_at_finish = Read-U32 488
    wave_dispatch_pressure_at_finish = Read-U32 492
    screenshot = $screenshot
    rng_benchmark_cycles_per_call = [math]::Round(
        (Read-U32 360) / (Read-U32 368),
        2
    )
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
    render_cycles_average_completed = if ($renderCompleted) {
        [math]::Round($renderCyclesTotal / $renderCompleted, 2)
    } else {
        0
    }
    collision_cycles_average = if ($logicUpdates) {
        [math]::Round($collisionCyclesTotal / $logicUpdates, 2)
    } else {
        0
    }
    vblank_irq_cycles_average = if ($vblankIrqs) {
        [math]::Round(
            $vblankIrqCyclesTotal / $vblankIrqs,
            2
        )
    } else {
        0
    }
    commit_cycles_average = if ($commitFrames) {
        [math]::Round(
            $commitCyclesTotal / $commitFrames,
            2
        )
    } else {
        0
    }
    audio_input_cycles_average = if ($audioFrames) {
        [math]::Round(
            $audioInputCyclesTotal / $audioFrames,
            2
        )
    } else {
        0
    }
    prelogic_cycles_average = if ($displayFrames) {
        [math]::Round(
            $prelogicCyclesTotal / $displayFrames,
            2
        )
    } else {
        0
    }
    prefetch_cycles_average = if ($displayFrames) {
        [math]::Round($prefetchCyclesTotal / $displayFrames, 2)
    } else {
        0
    }
    loop_work_cycles_average = if ($displayFrames) {
        [math]::Round($loopWorkCyclesTotal / $displayFrames, 2)
    } else {
        0
    }
    exclusive_stage_cycles_total = (
        $commitCyclesTotal +
        $audioInputCyclesTotal +
        $logicCyclesTotal +
        $renderCyclesTotal +
        $prefetchCyclesTotal
    )
    dispatch_and_other_cycles_total = [math]::Max(
        0,
        $loopWorkCyclesTotal -
            $commitCyclesTotal -
            $audioInputCyclesTotal -
            $logicCyclesTotal -
            $renderCyclesTotal -
            $prefetchCyclesTotal
    )
}
if (
    $bytes[4] -ne 1 -or
    $bytes[5] -ne 1 -or
    $telemetry.loadout_failures -ne 0 -or
    $telemetry.source_assets_valid -ne 1 -or
    $telemetry.invincible_enabled -ne 1 -or
    $telemetry.stress_loadout_enabled -ne 1 -or
    $telemetry.detail_adapter_self_test -ne 1 -or
    $telemetry.route_episode -ne $Episode -or
    $telemetry.route_section -ne $Section -or
    $telemetry.stop_end_position -ne $EndPosition -or
    $telemetry.stop_duration_vblanks -ne $DurationVBlanks -or
    (
        $EndPosition -gt 0 -and
        $telemetry.level_position -lt $EndPosition
    ) -or
    $telemetry.rng_benchmark_calls -ne 10000 -or
    $telemetry.rng_benchmark_cycles -eq 0
) {
    throw "Stress telemetry reported a validation failure"
}
if (($diagnosticFlags -band 0x1000) -ne 0) {
    if (
        $displayFrames -ne $telemetry.wall_vblanks -or
        $vblankRecoveryLoops -ne $telemetry.missed_vblanks -or
        $audioFrames -gt $telemetry.wall_vblanks -or
        ($audioFrameLoss * 100) -gt $displayFrames
    ) {
        throw (
            "VBlank recovery lost timing parity: " +
            "audio=$audioFrames display=$displayFrames " +
            "wall=$($telemetry.wall_vblanks) " +
            "audioLoss=$audioFrameLoss " +
            "recoveries=$vblankRecoveryLoops " +
            "missed=$($telemetry.missed_vblanks)"
        )
    }
}

$telemetry |
    ConvertTo-Json -Depth 3 |
    Set-Content -LiteralPath $json -Encoding utf8
[pscustomobject]$telemetry
