param(
    [ValidateSet("low", "normal", "high", "pentium", "custom")]
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
        "active_mask_fast_wall_lazy_packed_no_adaptive",
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
    [ValidateRange(0, 65535)]
    [int]$FastForwardPosition = 0,
    [ValidateRange(1, 16)]
    [int]$FastForwardTicks = 8,
    [ValidateRange(0, 20000)]
    [int]$BossWindowVBlanks = 0,
    [ValidateRange(30000, 600000)]
    [int]$RuntimeTimeoutMilliseconds = 120000,
    [ValidateSet(0, 1)]
    [int]$HotpathAsm = 1,
    [ValidateSet(0, 1)]
    [int]$DetailEffectAsm = 1,
    [ValidateSet(0, 1)]
    [int]$Sprite2ExactLookupAsm = 1,
    [ValidateSet(0, 1)]
    [int]$StarfieldBatchAsm = 1,
    [ValidateSet(0, 1)]
    [int]$ProjectileCacheHint = 1,
    [ValidateSet(0, 1)]
    [int]$ProjectileHintAsm = 0,
    [ValidateSet(0, 1)]
    [int]$EffectActiveMask = 1,
    [ValidateSet(0, 1)]
    [int]$PoolBitScanAsm = 0,
    [ValidateSet(0, 1)]
    [int]$PlayerShotFreeMask = 1,
    [ValidateSet(0, 1)]
    [int]$PlayerShotUpdateMask = 1,
    [ValidateSet(0, 1)]
    [int]$EnemyActiveMask = 1,
    [ValidateSet(0, 1)]
    [int]$CollisionSnapshot = 1,
    [ValidateSet(0, 1)]
    [int]$CollisionActiveDirectory = 1,
    [ValidateRange(0.0, 100.0)]
    [double]$MaxAudioFrameLossPercent = 1.0,
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
$fastForwardTag = if ($FastForwardPosition -gt 0) {
    "_ff${FastForwardPosition}x${FastForwardTicks}"
} else {
    ""
}
$bossTag = if ($BossWindowVBlanks -gt 0) {
    "_boss${BossWindowVBlanks}"
} else {
    ""
}
$inputTag = if ($NoFire) { "_nofire" } else { "" }
$stressFire = if ($NoFire) { 0 } else { 1 }
$name = (
    "tgw8_ep${Episode}_s${Section}_${stopTag}${fastForwardTag}${bossTag}${inputTag}_v88_" +
    "${Variant}_h${HotpathAsm}_detail_${DetailLevel}_speed_normal_" +
    "detailasm${DetailEffectAsm}_x${Sprite2ExactLookupAsm}_" +
    "s${StarfieldBatchAsm}_ph${ProjectileCacheHint}_" +
    "pa${ProjectileHintAsm}_fx${EffectActiveMask}_bs${PoolBitScanAsm}_" +
    "sf${PlayerShotFreeMask}_su${PlayerShotUpdateMask}_em${EnemyActiveMask}_" +
    "cs${CollisionSnapshot}_cd${CollisionActiveDirectory}"
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
        "HOTPATH_ASM=$HotpathAsm " +
        "DETAIL_EFFECT_ASM=$DetailEffectAsm " +
        "SPRITE2_EXACT_LOOKUP_ASM=$Sprite2ExactLookupAsm " +
        "STARFIELD_BATCH_ASM=$StarfieldBatchAsm " +
        "PROJECTILE_CACHE_HINT=$ProjectileCacheHint " +
        "PROJECTILE_HINT_ASM=$ProjectileHintAsm " +
        "EFFECT_ACTIVE_MASK=$EffectActiveMask " +
        "POOL_BIT_SCAN_ASM=$PoolBitScanAsm " +
        "PLAYER_SHOT_FREE_MASK=$PlayerShotFreeMask " +
        "PLAYER_SHOT_UPDATE_MASK=$PlayerShotUpdateMask " +
        "ENEMY_ACTIVE_MASK=$EnemyActiveMask " +
        "COLLISION_SNAPSHOT=$CollisionSnapshot " +
        "COLLISION_ACTIVE_DIRECTORY=$CollisionActiveDirectory " +
        "STRESS_EPISODE=$Episode STRESS_SECTION=$Section " +
        "STRESS_END_POSITION=$EndPosition " +
        "STRESS_DURATION_VBLANKS=$DurationVBlanks " +
        "STRESS_FAST_FORWARD_POSITION=$FastForwardPosition " +
        "STRESS_FAST_FORWARD_TICKS=$FastForwardTicks " +
        "STRESS_BOSS_WINDOW_VBLANKS=$BossWindowVBlanks " +
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
    -ArgumentList @("-l", "0", "-O", $screenshot, "-S", "3", "$name.gba") `
    -WorkingDirectory $buildDir `
    -WindowStyle Hidden `
    -RedirectStandardOutput $stdout `
    -RedirectStandardError $stderr `
    -PassThru
if (-not $process.WaitForExit($RuntimeTimeoutMilliseconds)) {
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
if ($bytes.Length -lt 592) {
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
$expectedAdaptive = if ($Variant -like "*_no_adaptive") { 0 } else { 1 }
$expectedEffectMaskConsistency = if ($EffectActiveMask -eq 1) { 3 } else { 0 }
$expectedPlayerShotMaskConsistency = if ($PlayerShotFreeMask -eq 1) { 1 } else { 0 }
$expectedPlayerShotUpdateMask = if (
    $PlayerShotFreeMask -eq 1 -and $PlayerShotUpdateMask -eq 1
) { 1 } else { 0 }
$expectedEnemyMaskConsistency = if ($EnemyActiveMask -eq 1) { 7 } else { 0 }
$expectedCollisionSnapshot = if (
    $CollisionSnapshot -eq 1 -and
    $Variant -like "active_mask_fast*" -and
    $Variant -ne "active_mask_fast"
) { 1 } else { 0 }
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
    sprite2_exact_differential = Read-U32 640
    sprite2_exact_c_cycles = Read-U32 644
    sprite2_exact_asm_cycles = Read-U32 648
    sprite2_exact_benchmark_calls = Read-U32 652
    sprite2_filter_admission_denials = Read-U32 656
    sprite2_filter_fallback_hits = Read-U32 660
    sprite2_filter_fallback_builds = Read-U32 664
    sprite2_filter_evictions = Read-U32 668
    sprite2_exact_lookup_probes = Read-U32 672
    sprite2_exact_lookup_hits = Read-U32 676
    sprite2_exact_lookup_asm = Read-U32 680
    hotpath_asm = Read-U32 684
    starfield_differential = Read-U32 9000
    starfield_divmod_c_cycles = Read-U32 9004
    starfield_divmod_asm_cycles = Read-U32 9008
    starfield_plot_c_cycles = Read-U32 9012
    starfield_plot_asm_cycles = Read-U32 9016
    starfield_benchmark_calls = Read-U32 9020
    starfield_batch_asm = Read-U32 9024
    projectile_hint_differential = Read-U32 9028
    projectile_hint_c_cycles = Read-U32 9032
    projectile_hint_asm_cycles = Read-U32 9036
    projectile_hint_benchmark_calls = Read-U32 9040
    projectile_cache_hint = Read-U32 9044
    projectile_hint_asm = Read-U32 9048
    projectile_hint_probes = Read-U32 9052
    projectile_hint_hits = Read-U32 9056
    projectile_hint_fallback_scans = Read-U32 9060
    pool_bit_scan_differential = Read-U32 9064
    pool_lowest_c_cycles = Read-U32 9068
    pool_lowest_asm_cycles = Read-U32 9072
    pool_highest_c_cycles = Read-U32 9076
    pool_highest_asm_cycles = Read-U32 9080
    pool_bit_scan_benchmark_calls = Read-U32 9084
    effect_active_mask = Read-U32 9088
    pool_bit_scan_asm = Read-U32 9092
    effect_pool_mask_consistency = Read-U32 9096
    player_shot_pool_consistency = Read-U32 9100
    player_shot_free_mask = Read-U32 9104
    player_shot_allocator_calls = Read-U32 9108
    player_shot_allocator_slot_probes = Read-U32 9112
    player_shot_allocator_mask_word_probes = Read-U32 9116
    player_shot_active_count_at_finish = Read-U32 9120
    enemy_active_mask = Read-U32 9124
    enemy_mask_consistency = Read-U32 9128
    enemy_pool_active_visits = Read-U32 9132
    enemy_pool_linear_visits = Read-U32 9136
    enemy_allocator_mask_word_probes = Read-U32 9140
    enemy_allocator_slot_probes = Read-U32 9144
    player_shot_update_mask = Read-U32 9148
    player_shot_update_cycles_total = Read-U32 9152
    player_shot_update_cycles_max = Read-U32 9156
    player_shot_update_active_visits = Read-U32 9160
    player_shot_update_linear_visits = Read-U32 9164
    player_shot_update_complicated_visits = Read-U32 9168
    player_shot_update_trail_visits = Read-U32 9172
    player_shot_update_aimed_visits = Read-U32 9176
    player_shot_update_superpixel_visits = Read-U32 9180
    fast_forward_position = Read-U32 9184
    fast_forward_ticks = Read-U32 9188
    boss_window_requested_vblanks = Read-U32 9192
    boss_started = Read-U32 9196
    boss_completed = Read-U32 9200
    boss_start_position = Read-U32 9204
    boss_end_position = Read-U32 9208
    boss_display_frames = Read-U32 9212
    boss_missed_vblanks = Read-U32 9216
    boss_logic_updates = Read-U32 9220
    boss_logic_cycles = Read-U32 9224
    boss_render_cycles = Read-U32 9228
    boss_collision_cycles = Read-U32 9232
    boss_player_shot_update_cycles = Read-U32 9236
    boss_collision_candidate_visits = Read-U32 9240
    boss_collision_hits = Read-U32 9244
    boss_shot_update_active_visits = Read-U32 9248
    boss_shot_update_linear_visits = Read-U32 9252
    boss_shot_update_trail_visits = Read-U32 9256
    boss_enemy_active_visits = Read-U32 9260
    boss_projectile_cache_hits = Read-U32 9264
    boss_projectile_cache_misses = Read-U32 9268
    boss_effect_cache_hits = Read-U32 9272
    boss_effect_cache_misses = Read-U32 9276
    boss_render_completed = Read-U32 9280
    boss_render_deferred = Read-U32 9284
    boss_player_shot_spawns = Read-U32 9288
    boss_sprite2_misses = Read-U32 9292
    boss_sprite2_evictions = Read-U32 9296
    boss_sprite2_upload_bytes = Read-U32 9300
    boss_sprite2_l2_hits = Read-U32 9304
    boss_sprite2_l2_misses = Read-U32 9308
    boss_filter_admission_denials = Read-U32 9312
    boss_filter_fallback_hits = Read-U32 9316
    boss_filter_fallback_builds = Read-U32 9320
    boss_effect_oam_culls = Read-U32 9324
    boss_player_shot_oam_culls = Read-U32 9328
    boss_enemy_kills = Read-U32 9332
    boss_audio_frames = Read-U32 9336
    boss_wall_vblanks = Read-U32 9340
    collision_snapshot = Read-U32 9344
    collision_active_directory = Read-U32 9348
    collision_hit_apply_calls = Read-U32 9352
    collision_status_link_visits = Read-U32 9356
    collision_kill_group_visits = Read-U32 9360
    collision_damaged_transition_visits = Read-U32 9364
    collision_player_contact_visits = Read-U32 9368
    collision_zinglon_visits = Read-U32 9372
    boss_hit_apply_calls = Read-U32 9376
    boss_status_link_visits = Read-U32 9380
    boss_kill_group_visits = Read-U32 9384
    boss_damaged_transition_visits = Read-U32 9388
    boss_player_contact_visits = Read-U32 9392
    boss_zinglon_visits = Read-U32 9396
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
    adaptive_dispatch_attempts = Read-U32 496
    adaptive_dispatch_entries = Read-U32 500
    adaptive_dispatch_severe_entries = Read-U32 504
    adaptive_dispatch_exits = Read-U32 508
    adaptive_dispatch_logic_busy_deferred = Read-U32 512
    adaptive_dispatch_idle_renders = Read-U32 516
    adaptive_dispatch_safety_forced = Read-U32 520
    adaptive_dispatch_active_at_finish = Read-U32 524
    adaptive_dispatch_severe_at_finish = Read-U32 528
    adaptive_dispatch_pressure_at_finish = Read-U32 532
    adaptive_dispatch_enabled = Read-U32 536
    hotpath_asm_self_test = Read-U32 540
    iwram_stack_canary_remaining = Read-U32 544
    iwram_stack_canary_filled = Read-U32 548
    level_port_asm_differential = Read-U32 552
    colour_distance_asm_differential = Read-U32 556
    overlay_distance_c_cycles = Read-U32 560
    overlay_distance_asm_cycles = Read-U32 564
    palette_distance_c_cycles = Read-U32 568
    palette_distance_asm_cycles = Read-U32 572
    axis_overlap_c_cycles = Read-U32 576
    axis_overlap_asm_cycles = Read-U32 580
    hotpath_benchmark_calls = Read-U32 584
    hotpath_benchmark_sink = Read-U32 588
    detail_effect_asm_differential = Read-U32 592
    detail_palette_c_cycles = Read-U32 596
    detail_palette_asm_cycles = Read-U32 600
    detail_palette_benchmark_calls = Read-U32 604
    detail_spotlight_c_cycles = Read-U32 608
    detail_spotlight_asm_cycles = Read-U32 612
    detail_spotlight_benchmark_calls = Read-U32 616
    detail_wave_c_cycles = Read-U32 620
    detail_wave_asm_cycles = Read-U32 624
    detail_wave_benchmark_calls = Read-U32 628
    detail_effect_asm_enabled = Read-U32 632
    detail_effect_benchmark_sink = Read-U32 636
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
    $telemetry.adaptive_dispatch_enabled -ne $expectedAdaptive -or
    $telemetry.detail_adapter_self_test -ne 1 -or
    $telemetry.hotpath_asm_self_test -ne 1 -or
    $telemetry.level_port_asm_differential -ne 3 -or
    $telemetry.colour_distance_asm_differential -ne 3 -or
    $telemetry.starfield_differential -ne 3 -or
    $telemetry.starfield_benchmark_calls -ne 16384 -or
    $telemetry.starfield_batch_asm -ne $StarfieldBatchAsm -or
    $telemetry.projectile_hint_differential -ne 1 -or
    $telemetry.projectile_hint_benchmark_calls -ne 16384 -or
    $telemetry.projectile_cache_hint -ne $ProjectileCacheHint -or
    $telemetry.projectile_hint_asm -ne $ProjectileHintAsm -or
    $telemetry.pool_bit_scan_differential -ne 3 -or
    $telemetry.pool_bit_scan_benchmark_calls -ne 16384 -or
    $telemetry.effect_active_mask -ne $EffectActiveMask -or
    $telemetry.pool_bit_scan_asm -ne $PoolBitScanAsm -or
    $telemetry.effect_pool_mask_consistency -ne $expectedEffectMaskConsistency -or
    $telemetry.player_shot_pool_consistency -ne $expectedPlayerShotMaskConsistency -or
    $telemetry.player_shot_free_mask -ne $PlayerShotFreeMask -or
    ($stressFire -ne 0 -and $telemetry.player_shot_allocator_calls -eq 0) -or
    $telemetry.enemy_active_mask -ne $EnemyActiveMask -or
    $telemetry.collision_snapshot -ne $expectedCollisionSnapshot -or
    $telemetry.collision_active_directory -ne (
        $CollisionActiveDirectory -band $EnemyActiveMask
    ) -or
    $telemetry.enemy_mask_consistency -ne $expectedEnemyMaskConsistency -or
    $telemetry.player_shot_update_mask -ne $expectedPlayerShotUpdateMask -or
    $telemetry.hotpath_benchmark_calls -ne 16384 -or
    $telemetry.route_episode -ne $Episode -or
    $telemetry.route_section -ne $Section -or
    $telemetry.stop_end_position -ne $EndPosition -or
    $telemetry.stop_duration_vblanks -ne $DurationVBlanks -or
    $telemetry.fast_forward_position -ne $FastForwardPosition -or
    $telemetry.fast_forward_ticks -ne $FastForwardTicks -or
    $telemetry.boss_window_requested_vblanks -ne $BossWindowVBlanks -or
    ($BossWindowVBlanks -gt 0 -and $telemetry.boss_started -ne 1) -or
    (
        $EndPosition -gt 0 -and
        $telemetry.level_position -lt $EndPosition
    ) -or
    $telemetry.rng_benchmark_calls -ne 10000 -or
    $telemetry.rng_benchmark_cycles -eq 0
) {
    throw "Stress telemetry reported a validation failure"
}
if (
    ($diagnosticFlags -band 0x1000) -ne 0 -and
    $FastForwardPosition -eq 0
) {
    if (
        $displayFrames -ne $telemetry.wall_vblanks -or
        $vblankRecoveryLoops -ne $telemetry.missed_vblanks -or
        $audioFrames -gt $telemetry.wall_vblanks -or
        ($audioFrameLoss * 100.0) -gt
            ($displayFrames * $MaxAudioFrameLossPercent)
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
