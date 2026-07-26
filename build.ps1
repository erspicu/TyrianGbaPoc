param(
    [switch]$KeepIntermediates
)

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$workspaceRoot = (Resolve-Path (Join-Path $projectRoot "..\..")).Path
$msysRoot = Join-Path $workspaceRoot "tools\msys64"
$sdkRoot = Join-Path $workspaceRoot "tools\gba-sdk"
$bash = Join-Path $msysRoot "usr\bin\bash.exe"
$ucrtBin = Join-Path $msysRoot "ucrt64\bin"
$headless = Join-Path $workspaceRoot "org\mgba\build-ucrt-headless\mgba-headless.exe"
$perf = Join-Path $workspaceRoot "org\mgba\build-ucrt-headless\mgba-perf.exe"
$buildDir = Join-Path $projectRoot "build"
$releaseName = "tyrian_gba_level1_pc_flow_mode4_romfs_v22"
$testName = "tyrian_gba_level1_pc_flow_mode4_autotest_romfs_v22"
$releaseRom = Join-Path $buildDir "$releaseName.gba"
$testRom = Join-Path $buildDir "$testName.gba"
$testSave = Join-Path $buildDir "$testName.sav"
$testStdout = Join-Path $buildDir "autotest_mgba_stdout.txt"
$testStderr = Join-Path $buildDir "autotest_mgba_stderr.txt"
$perfStdout = Join-Path $buildDir "release_boot_perf.csv"
$perfStderr = Join-Path $buildDir "release_boot_perf.stderr.txt"
$verificationPath = Join-Path $buildDir "verification.txt"
$backupDir = Join-Path $projectRoot "Backup"
$romfsImagePath = Join-Path $projectRoot "res\tyrian_romfs.bin"
$romfsAuditPath = Join-Path $projectRoot "res\tyrian_romfs_audit.json"
$python = (Get-Command python -ErrorAction Stop).Source

foreach ($required in @(
    $bash,
    $headless,
    $perf,
    (Join-Path $sdkRoot "libgba\lib\libgba.a"),
    (Join-Path $sdkRoot "maxmod\lib\libmm.a"),
    (Join-Path $sdkRoot "tools\bin\mmutil.exe")
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Required GBA build/runtime component is missing: $required"
    }
}

$drive = $projectRoot.Substring(0, 1).ToLowerInvariant()
$unixProject = "/$drive/" + $projectRoot.Substring(3).Replace("\", "/")
$pythonDrive = $python.Substring(0, 1).ToLowerInvariant()
$unixPython = "/$pythonDrive/" + $python.Substring(3).Replace("\", "/")
$buildCommand = @'
set -e
export PATH=/ucrt64/bin:/c/ai_project/AprTyrianNes/tools/gba-sdk/tools/bin:$PATH
cd "__PROJECT__"
make PYTHON="__PYTHON__" assets
make -j2 PYTHON="__PYTHON__" all autotest
'@.Replace("__PROJECT__", $unixProject).Replace("__PYTHON__", $unixPython)

& $bash -lc $buildCommand
if ($LASTEXITCODE -ne 0) {
    throw "GBA ROM build failed with exit code $LASTEXITCODE"
}

foreach ($romfsOutput in @($romfsImagePath, $romfsAuditPath)) {
    if (-not (Test-Path -LiteralPath $romfsOutput)) {
        throw "ROMFS build output is missing: $romfsOutput"
    }
}
$romfsAudit = Get-Content -LiteralPath $romfsAuditPath -Raw |
    ConvertFrom-Json
$romfsImageBytes = (Get-Item -LiteralPath $romfsImagePath).Length
$romfsImageSha256 = (
    Get-FileHash -LiteralPath $romfsImagePath -Algorithm SHA256
).Hash.ToLowerInvariant()
$romfsManifestCrc32 = [Convert]::ToUInt32(
    $romfsAudit.manifest_crc32,
    16
)
$romfsExpectedSelfTestChecks = 38 + 11 * $romfsAudit.probe_count
if (
    $romfsAudit.magic -ne "TYRVFS1" -or
    $romfsAudit.format_version -ne 1 -or
    $romfsAudit.entry_count -le 0 -or
    $romfsAudit.files.Count -ne $romfsAudit.entry_count -or
    $romfsAudit.probe_count -le 0 -or
    $romfsAudit.probes.Count -ne $romfsAudit.probe_count -or
    $romfsImageBytes -ne $romfsAudit.image_bytes -or
    $romfsAudit.payload_bytes -gt $romfsAudit.image_bytes -or
    $romfsImageSha256 -ne $romfsAudit.image_sha256
) {
    throw "ROMFS audit metadata does not match its packed image"
}

function Test-GbaRom {
    param(
        [Parameter(Mandatory)]
        [string]$Name,
        [Parameter(Mandatory)]
        [string]$Path,
        [Parameter(Mandatory)]
        [string]$ExpectedGameCode
    )

    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 192) {
        throw "ROM is too small to contain a GBA header: $Path"
    }
    if ($bytes.Length -gt 32MB) {
        throw "ROM exceeds the standard 32 MiB GBA address window: $Path"
    }

    $headerSum = 0
    for ($index = 0xA0; $index -le 0xBC; $index++) {
        $headerSum = ($headerSum + $bytes[$index]) -band 0xFF
    }
    $expectedComplement = (-(0x19 + $headerSum)) -band 0xFF
    if ($bytes[0xBD] -ne $expectedComplement) {
        throw "GBA header complement mismatch: $Path"
    }

    $title = [Text.Encoding]::ASCII.GetString($bytes, 0xA0, 12).
        Trim([char]0, [char]32)
    $gameCode = [Text.Encoding]::ASCII.GetString($bytes, 0xAC, 4)
    if ($gameCode -ne $ExpectedGameCode) {
        throw "Unexpected game code '$gameCode' in $Path"
    }

    [ordered]@{
        name = $Name
        path = $Path
        bytes = $bytes.Length
        kib = [math]::Round($bytes.Length / 1KB, 2)
        gba_32mib_percent = [math]::Round(100 * $bytes.Length / 32MB, 4)
        title = $title
        game_code = $gameCode
        header_complement = "0x$($bytes[0xBD].ToString('X2'))"
        sha256 = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).
            Hash.ToLowerInvariant()
    }
}

function Start-TestProcess {
    param(
        [Parameter(Mandatory)]
        [string]$FilePath,
        [Parameter(Mandatory)]
        [string[]]$Arguments,
        [Parameter(Mandatory)]
        [string]$WorkingDirectory,
        [Parameter(Mandatory)]
        [string]$StandardOutput,
        [Parameter(Mandatory)]
        [string]$StandardError,
        [int]$TimeoutMilliseconds = 30000
    )

    $watch = [Diagnostics.Stopwatch]::StartNew()
    $process = Start-Process `
        -FilePath $FilePath `
        -ArgumentList $Arguments `
        -WorkingDirectory $WorkingDirectory `
        -WindowStyle Hidden `
        -PassThru `
        -RedirectStandardOutput $StandardOutput `
        -RedirectStandardError $StandardError
    if (-not $process.WaitForExit($TimeoutMilliseconds)) {
        Stop-Process -Id $process.Id -Force
        throw "Runtime verification timed out after $TimeoutMilliseconds ms"
    }
    $process.WaitForExit()
    $watch.Stop()
    if ($process.ExitCode -ne 0) {
        throw "Runtime verification exited with code $($process.ExitCode)"
    }
    return $watch.ElapsedMilliseconds
}

function Invoke-BuildArtifactPolicy {
    param(
        [Parameter(Mandatory)]
        [string]$BuildDirectory,
        [Parameter(Mandatory)]
        [string]$ReleaseRom,
        [Parameter(Mandatory)]
        [string]$BackupDirectory
    )

    $comparison = [StringComparison]::OrdinalIgnoreCase
    $separator = [IO.Path]::DirectorySeparatorChar.ToString()
    $buildFull = [IO.Path]::GetFullPath($BuildDirectory)
    $releaseFull = [IO.Path]::GetFullPath($ReleaseRom)
    $backupFull = [IO.Path]::GetFullPath($BackupDirectory)
    $buildPrefix = $buildFull
    $backupPrefix = $backupFull
    $archived = 0
    $deduplicated = 0
    $removedEntries = 0

    if (-not $buildPrefix.EndsWith($separator)) {
        $buildPrefix += $separator
    }
    if (-not $backupPrefix.EndsWith($separator)) {
        $backupPrefix += $separator
    }
    if (
        -not $releaseFull.StartsWith($buildPrefix, $comparison) -or
        $backupFull.Equals($buildFull, $comparison) -or
        $backupFull.StartsWith($buildPrefix, $comparison)
    ) {
        throw "Unsafe build artifact paths"
    }
    if (-not (Test-Path -LiteralPath $releaseFull -PathType Leaf)) {
        throw "The verified release ROM is missing before cleanup: $releaseFull"
    }

    New-Item -ItemType Directory -Path $backupFull -Force | Out-Null
    foreach (
        $rom in @(
            Get-ChildItem -LiteralPath $buildFull `
                -Recurse -File -Force -Filter "*.gba"
        )
    ) {
        $sourceFull = [IO.Path]::GetFullPath($rom.FullName)
        if ($sourceFull.Equals($releaseFull, $comparison)) {
            continue
        }
        if (-not $sourceFull.StartsWith($buildPrefix, $comparison)) {
            throw "Refusing to archive a ROM outside build: $sourceFull"
        }

        $sourceHash = (
            Get-FileHash -LiteralPath $sourceFull -Algorithm SHA256
        ).Hash.ToLowerInvariant()
        $destination = Join-Path $backupFull $rom.Name
        if (Test-Path -LiteralPath $destination -PathType Leaf) {
            $destinationHash = (
                Get-FileHash -LiteralPath $destination -Algorithm SHA256
            ).Hash.ToLowerInvariant()
            if ($sourceHash -eq $destinationHash) {
                Remove-Item -LiteralPath $sourceFull -Force
                $deduplicated++
                continue
            }

            $stem = [IO.Path]::GetFileNameWithoutExtension($rom.Name)
            $timestamp = $rom.LastWriteTime.ToString("yyyyMMdd-HHmmss")
            $suffix = $sourceHash.Substring(0, 8)
            $candidateIndex = 0
            do {
                $candidateTag = if ($candidateIndex -eq 0) {
                    "$timestamp-$suffix"
                } else {
                    "$timestamp-$suffix-$candidateIndex"
                }
                $destination = Join-Path $backupFull (
                    "$stem-$candidateTag.gba"
                )
                $candidateIndex++
            } while (Test-Path -LiteralPath $destination)
        }

        $destinationFull = [IO.Path]::GetFullPath($destination)
        if (-not $destinationFull.StartsWith($backupPrefix, $comparison)) {
            throw "Refusing to archive a ROM outside Backup: $destinationFull"
        }
        Move-Item -LiteralPath $sourceFull -Destination $destinationFull
        $archived++
    }

    foreach (
        $entry in @(
            Get-ChildItem -LiteralPath $buildFull -Force
        )
    ) {
        $entryFull = [IO.Path]::GetFullPath($entry.FullName)
        if ($entryFull.Equals($releaseFull, $comparison)) {
            continue
        }
        if (-not $entryFull.StartsWith($buildPrefix, $comparison)) {
            throw "Refusing to clean an entry outside build: $entryFull"
        }
        Remove-Item -LiteralPath $entryFull -Recurse -Force
        $removedEntries++
    }

    $remaining = @(Get-ChildItem -LiteralPath $buildFull -Force)
    if (
        $remaining.Count -ne 1 -or
        -not [IO.Path]::GetFullPath($remaining[0].FullName).Equals(
            $releaseFull,
            $comparison
        )
    ) {
        throw "Build cleanup did not retain exactly the release ROM"
    }

    return [pscustomobject]@{
        ArchivedRoms = $archived
        DeduplicatedRoms = $deduplicated
        RemovedEntries = $removedEntries
    }
}

$releaseInfo = Test-GbaRom `
    -Name "release" `
    -Path $releaseRom `
    -ExpectedGameCode "TYGA"
$testInfo = Test-GbaRom `
    -Name "autotest" `
    -Path $testRom `
    -ExpectedGameCode "TYGT"

$env:PATH = "$ucrtBin;$env:PATH"
if (Test-Path -LiteralPath $testSave) {
    Remove-Item -LiteralPath $testSave -Force
}

# Use a relative ROM filename here.  The UCRT mGBA VFS treats forward-slash
# paths portably, while a Windows absolute backslash path prevents its
# directory helper from deriving the adjacent .sav filename.
$testElapsed = Start-TestProcess `
    -FilePath $headless `
    -Arguments @("-S", "3", "$testName.gba") `
    -WorkingDirectory $buildDir `
    -StandardOutput $testStdout `
    -StandardError $testStderr

if (-not (Test-Path -LiteralPath $testSave)) {
    throw "mGBA did not create the expected auto-test SRAM file: $testSave"
}
$runtimeErrors = @(
    Select-String `
        -Path $testStdout, $testStderr `
        -Pattern "Bad memory|Invalid|Illegal|Hard crash|Fatal|Failed|Error"
)
if ($runtimeErrors.Count -ne 0) {
    throw "mGBA reported $($runtimeErrors.Count) runtime error(s)"
}

$saveBytes = [System.IO.File]::ReadAllBytes($testSave)
if ($saveBytes.Length -lt 588) {
    throw "Auto-test SRAM telemetry is truncated"
}
$magic = [Text.Encoding]::ASCII.GetString($saveBytes, 0, 4)
if ($magic -ne "TGBA") {
    throw "Auto-test SRAM magic mismatch: '$magic'"
}

function Read-TelemetryU32 {
    param([int]$Offset)
    return [BitConverter]::ToUInt32($saveBytes, $Offset)
}

$telemetry = [ordered]@{
    version = $saveBytes[4]
    pass = $saveBytes[5]
    final_state = $saveBytes[6]
    title_music_active = $saveBytes[7]
    logic_updates = Read-TelemetryU32 8
    display_frames = Read-TelemetryU32 12
    vblank_irqs = Read-TelemetryU32 16
    missed_vblanks = Read-TelemetryU32 20
    spawn_events = Read-TelemetryU32 24
    control_events = Read-TelemetryU32 28
    collisions = Read-TelemetryU32 32
    streamed_map_rows = Read-TelemetryU32 36
    max_active_enemies = Read-TelemetryU32 40
    max_hardware_oam = Read-TelemetryU32 44
    stream_drops = Read-TelemetryU32 48
    final_source_event_index = Read-TelemetryU32 52
    final_level_tick = Read-TelemetryU32 56
    state_transitions = Read-TelemetryU32 60
    max_active_effects = Read-TelemetryU32 64
    effect_drops = Read-TelemetryU32 68
    reward_spawns = Read-TelemetryU32 72
    reward_pickups = Read-TelemetryU32 76
    max_active_rewards = Read-TelemetryU32 80
    reward_drops = Read-TelemetryU32 84
    final_cash = Read-TelemetryU32 88
    enemy_shots_spawned = Read-TelemetryU32 92
    enemy_shot_drops = Read-TelemetryU32 96
    max_active_enemy_shots = Read-TelemetryU32 100
    final_level_position = Read-TelemetryU32 104
    enemy_pool_replacements = Read-TelemetryU32 108
    direct_kill_cash = Read-TelemetryU32 112
    reward_control_events = Read-TelemetryU32 116
    reward_assignments = Read-TelemetryU32 120
    pause_toggles = Read-TelemetryU32 124
    paused_display_frames = Read-TelemetryU32 128
    source_parity_events = Read-TelemetryU32 132
    source_parity_events_applied = Read-TelemetryU32 136
    source_parity_events_deferred = Read-TelemetryU32 140
    source_parity_events_skipped = Read-TelemetryU32 144
    source_parity_spawn_attempts = Read-TelemetryU32 148
    source_parity_spawn_successes = Read-TelemetryU32 152
    source_parity_spawn_pool_full = Read-TelemetryU32 156
    source_parity_spawn_missing = Read-TelemetryU32 160
    source_parity_max_enemies = Read-TelemetryU32 164
    source_parity_control_writes = Read-TelemetryU32 168
    source_parity_rng_calls = Read-TelemetryU32 172
    romfs_entries = Read-TelemetryU32 176
    romfs_image_bytes = Read-TelemetryU32 180
    romfs_payload_bytes = Read-TelemetryU32 184
    romfs_self_test_checks = Read-TelemetryU32 188
    romfs_self_test_failures = Read-TelemetryU32 192
    romfs_manifest_crc32 = Read-TelemetryU32 196
    source_parity_motion_updates = Read-TelemetryU32 200
    source_parity_releases = Read-TelemetryU32 204
    source_parity_shot_triggers = Read-TelemetryU32 208
    source_parity_launch_attempts = Read-TelemetryU32 212
    source_parity_launch_successes = Read-TelemetryU32 216
    source_parity_random_attempts = Read-TelemetryU32 220
    source_parity_random_successes = Read-TelemetryU32 224
    source_parity_enemy_shots_spawned = Read-TelemetryU32 228
    source_parity_enemy_shot_drops = Read-TelemetryU32 232
    source_parity_max_enemy_shots = Read-TelemetryU32 236
    source_parity_enemy_shot_updates = Read-TelemetryU32 240
    source_parity_enemy_shot_releases = Read-TelemetryU32 244
    source_parity_enemy_shot_player_hits = Read-TelemetryU32 248
    source_parity_player_shot_hits = Read-TelemetryU32 252
    source_parity_enemy_kills = Read-TelemetryU32 256
    source_parity_direct_cash = Read-TelemetryU32 260
    source_parity_score_item_spawns = Read-TelemetryU32 264
    source_parity_score_item_pickups = Read-TelemetryU32 268
    source_parity_score_item_max_active = Read-TelemetryU32 272
    source_parity_death_spawn_attempts = Read-TelemetryU32 276
    source_parity_death_spawn_successes = Read-TelemetryU32 280
    source_parity_death_control_events = Read-TelemetryU32 284
    source_parity_death_assignments = Read-TelemetryU32 288
    source_parity_max_visible_enemies = Read-TelemetryU32 292
    source_parity_unknown_visuals = Read-TelemetryU32 296
    source_parity_player_contacts = Read-TelemetryU32 300
    source_parity_unsupported_pickups = Read-TelemetryU32 304
    source_parity_death_spawn_pool_full = Read-TelemetryU32 308
    source_parity_death_spawn_missing = Read-TelemetryU32 312
    source_parity_final_active_enemies = Read-TelemetryU32 316
    source_parity_final_active_enemy_shots = Read-TelemetryU32 320
    source_parity_data_cube_pickups = Read-TelemetryU32 324
    source_parity_front_weapon_powerups = Read-TelemetryU32 328
    source_parity_rear_weapon_powerups = Read-TelemetryU32 332
    final_bg1_scroll_speed = Read-TelemetryU32 336
    final_bg2_scroll_speed = Read-TelemetryU32 340
    final_bg3_scroll_speed = Read-TelemetryU32 344
    source_parity_assets_valid = Read-TelemetryU32 348
    final_game_paused = Read-TelemetryU32 352
    sprite2_decode_failures = Read-TelemetryU32 356
    sprite2_cache_hits = Read-TelemetryU32 360
    sprite2_cache_misses = Read-TelemetryU32 364
    sprite2_cache_evictions = Read-TelemetryU32 368
    sprite2_cache_drops = Read-TelemetryU32 372
    sprite2_uploads = Read-TelemetryU32 376
    sprite2_upload_bytes = Read-TelemetryU32 380
    sprite2_max_uploads_per_frame = Read-TelemetryU32 384
    sprite2_cache_slots = Read-TelemetryU32 388
    source_parity_powerup_consolation_cash = Read-TelemetryU32 392
    source_parity_orbiting_asteroid_pickups = Read-TelemetryU32 396
    source_parity_superbomb_pickups = Read-TelemetryU32 400
    source_parity_hotdog_pickups = Read-TelemetryU32 404
    source_parity_armor_pickups = Read-TelemetryU32 408
    source_parity_bonus_portal_pickups = Read-TelemetryU32 412
    source_parity_high_value_pickups = Read-TelemetryU32 416
    source_parity_front_weapon_id = Read-TelemetryU32 420
    source_parity_front_weapon_power = Read-TelemetryU32 424
    source_parity_rear_weapon_id = Read-TelemetryU32 428
    source_parity_rear_weapon_power = Read-TelemetryU32 432
    source_parity_superbombs = Read-TelemetryU32 436
    source_parity_armor = Read-TelemetryU32 440
    source_parity_weapon_mode = Read-TelemetryU32 444
    source_parity_special = Read-TelemetryU32 448
    source_parity_purple_balls_needed = Read-TelemetryU32 452
    source_parity_bonus_level = Read-TelemetryU32 456
    source_parity_next_level = Read-TelemetryU32 460
    source_parity_display_time = Read-TelemetryU32 464
    final_player_source_x = Read-TelemetryU32 468
    final_player_source_y = Read-TelemetryU32 472
    final_map_x_offset = Read-TelemetryU32 476
    final_map_x2_offset = Read-TelemetryU32 480
    final_map_x3_offset = Read-TelemetryU32 484
    final_bg1_horizontal_offset = Read-TelemetryU32 488
    final_bg2_horizontal_offset = Read-TelemetryU32 492
    final_bg3_horizontal_offset = Read-TelemetryU32 496
    final_bg1_source_scroll = Read-TelemetryU32 500
    final_bg2_source_scroll = Read-TelemetryU32 504
    final_bg3_source_scroll = Read-TelemetryU32 508
    presentation_crop_x = Read-TelemetryU32 512
    presentation_crop_y = Read-TelemetryU32 516
    layer_rule_checks = Read-TelemetryU32 520
    layer_rule_failures = Read-TelemetryU32 524
    final_background2_over = Read-TelemetryU32 528
    final_background3_over = Read-TelemetryU32 532
    final_top_enemy_over = Read-TelemetryU32 536
    final_sky_enemy_over_all = Read-TelemetryU32 540
    final_background2_priority = Read-TelemetryU32 544
    final_background3_priority = Read-TelemetryU32 548
    sprite2_max_visible_unique = Read-TelemetryU32 552
    effect_cache_hits = Read-TelemetryU32 556
    effect_cache_misses = Read-TelemetryU32 560
    effect_cache_evictions = Read-TelemetryU32 564
    effect_cache_drops = Read-TelemetryU32 568
    effect_cache_uploads = Read-TelemetryU32 572
    effect_cache_upload_bytes = Read-TelemetryU32 576
    effect_cache_max_uploads_per_frame = Read-TelemetryU32 580
    effect_cache_max_visible_unique = Read-TelemetryU32 584
}

$legacyStage4TelemetryChecks = [ordered]@{
    schema_version = $telemetry.version -eq 19
    rom_reported_pass = $telemetry.pass -eq 1
    returned_to_title = $telemetry.final_state -eq 0
    title_music_active = $telemetry.title_music_active -eq 1
    logic_updates = $telemetry.logic_updates -eq 7093
    display_frames = $telemetry.display_frames -eq 12239
    final_level_position = $telemetry.final_level_position -eq 5400
    final_source_event_index = $telemetry.final_source_event_index -eq 878
    final_level_tick = $telemetry.final_level_tick -eq 7093
    spawn_events = $telemetry.spawn_events -eq 476
    control_events = $telemetry.control_events -eq 2509
    collisions = $telemetry.collisions -eq 396
    streamed_map_rows = $telemetry.streamed_map_rows -eq 3590
    max_active_enemies = $telemetry.max_active_enemies -eq 39
    max_hardware_oam = $telemetry.max_hardware_oam -eq 43
    vblank_budget = $telemetry.missed_vblanks -le 160
    stream_drops = $telemetry.stream_drops -eq 0
    max_active_effects = $telemetry.max_active_effects -eq 31
    effect_drops = $telemetry.effect_drops -eq 0
    reward_spawns = $telemetry.reward_spawns -eq 3
    reward_pickups = $telemetry.reward_pickups -eq 2
    max_active_rewards = $telemetry.max_active_rewards -eq 3
    reward_drops = $telemetry.reward_drops -eq 0
    all_enemy_shots_spawned = $telemetry.enemy_shots_spawned -eq 251
    all_enemy_shot_drops = $telemetry.enemy_shot_drops -eq 0
    all_max_active_enemy_shots = $telemetry.max_active_enemy_shots -eq 20
    enemy_pool_replacements = $telemetry.enemy_pool_replacements -eq 0
    direct_kill_cash = $telemetry.direct_kill_cash -eq 1121
    final_cash = $telemetry.final_cash -eq 1171
    reward_control_events = $telemetry.reward_control_events -eq 32
    reward_assignments = $telemetry.reward_assignments -eq 60
    pause_toggles = $telemetry.pause_toggles -eq 2
    paused_display_frames = $telemetry.paused_display_frames -eq 60
    source_events = $telemetry.source_parity_events -eq 878
    source_events_applied = $telemetry.source_parity_events_applied -eq 869
    source_events_deferred = $telemetry.source_parity_events_deferred -eq 5
    source_events_skipped = $telemetry.source_parity_events_skipped -eq 4
    source_event_accounting = (
        $telemetry.source_parity_events_applied +
        $telemetry.source_parity_events_deferred +
        $telemetry.source_parity_events_skipped
    ) -eq $telemetry.source_parity_events
    source_spawn_attempts = $telemetry.source_parity_spawn_attempts -eq 473
    source_spawn_accounting = (
        $telemetry.source_parity_spawn_successes +
        $telemetry.source_parity_spawn_pool_full +
        $telemetry.source_parity_spawn_missing
    ) -eq $telemetry.source_parity_spawn_attempts
    source_spawn_successes = $telemetry.source_parity_spawn_successes -eq 473
    source_spawn_pool_full = $telemetry.source_parity_spawn_pool_full -eq 0
    source_spawn_missing = $telemetry.source_parity_spawn_missing -eq 0
    source_max_enemies = $telemetry.source_parity_max_enemies -eq 39
    source_control_writes = $telemetry.source_parity_control_writes -eq 2509
    source_rng_calls = $telemetry.source_parity_rng_calls -eq 1838
    source_motion_updates = $telemetry.source_parity_motion_updates -eq 57890
    source_releases = $telemetry.source_parity_releases -eq 456
    source_shot_triggers = $telemetry.source_parity_shot_triggers -eq 185
    source_launch_attempts = $telemetry.source_parity_launch_attempts -eq 0
    source_launch_successes = $telemetry.source_parity_launch_successes -eq 0
    source_random_attempts = $telemetry.source_parity_random_attempts -eq 0
    source_random_successes = $telemetry.source_parity_random_successes -eq 0
    source_enemy_shots_spawned = (
        $telemetry.source_parity_enemy_shots_spawned -eq 185
    )
    source_enemy_shot_drops = $telemetry.source_parity_enemy_shot_drops -eq 0
    source_max_enemy_shots = $telemetry.source_parity_max_enemy_shots -eq 9
    source_enemy_shot_updates = (
        $telemetry.source_parity_enemy_shot_updates -eq 9163
    )
    source_enemy_shot_releases = (
        $telemetry.source_parity_enemy_shot_releases -eq 185
    )
    source_enemy_shot_player_hits = (
        $telemetry.source_parity_enemy_shot_player_hits -eq 11
    )
    source_player_shot_hits = $telemetry.source_parity_player_shot_hits -eq 341
    source_player_contacts = $telemetry.source_parity_player_contacts -eq 39
    source_collision_accounting = (
        $telemetry.source_parity_player_shot_hits +
        $telemetry.source_parity_enemy_shot_player_hits +
        $telemetry.source_parity_player_contacts
    ) -eq 391
    legacy_boss_collision_delta = $telemetry.collisions - 391 -eq 5
    source_enemy_kills = $telemetry.source_parity_enemy_kills -eq 73
    source_direct_cash = $telemetry.source_parity_direct_cash -eq 1121
    source_score_item_spawns = (
        $telemetry.source_parity_score_item_spawns -eq 3
    )
    source_score_item_pickups = (
        $telemetry.source_parity_score_item_pickups -eq 2
    )
    source_score_item_max_active = (
        $telemetry.source_parity_score_item_max_active -eq 3
    )
    source_death_spawn_attempts = (
        $telemetry.source_parity_death_spawn_attempts -eq 3
    )
    source_death_spawn_successes = (
        $telemetry.source_parity_death_spawn_successes -eq 3
    )
    source_death_control_events = (
        $telemetry.source_parity_death_control_events -eq 32
    )
    source_death_assignments = (
        $telemetry.source_parity_death_assignments -eq 60
    )
    source_max_visible_enemies = (
        $telemetry.source_parity_max_visible_enemies -eq 30
    )
    source_unknown_visuals = $telemetry.source_parity_unknown_visuals -eq 0
    source_unsupported_pickups = (
        $telemetry.source_parity_unsupported_pickups -eq 0
    )
    source_death_spawn_pool_full = (
        $telemetry.source_parity_death_spawn_pool_full -eq 0
    )
    source_death_spawn_missing = (
        $telemetry.source_parity_death_spawn_missing -eq 0
    )
    source_final_active_enemies = (
        $telemetry.source_parity_final_active_enemies -eq 20
    )
    source_final_active_enemy_shots = (
        $telemetry.source_parity_final_active_enemy_shots -eq 0
    )
    source_data_cube_pickups = $telemetry.source_parity_data_cube_pickups -eq 0
    source_front_weapon_powerups = (
        $telemetry.source_parity_front_weapon_powerups -eq 0
    )
    source_rear_weapon_powerups = (
        $telemetry.source_parity_rear_weapon_powerups -eq 0
    )
    final_bg1_scroll_speed = $telemetry.final_bg1_scroll_speed -eq 1
    final_bg2_scroll_speed = $telemetry.final_bg2_scroll_speed -eq 2
    final_bg3_scroll_speed = $telemetry.final_bg3_scroll_speed -eq 0
    source_assets_valid = $telemetry.source_parity_assets_valid -eq 1
    final_game_paused = $telemetry.final_game_paused -eq 0
    sprite2_decode_failures = $telemetry.sprite2_decode_failures -eq 0
    sprite2_cache_hits = $telemetry.sprite2_cache_hits -eq 44926
    sprite2_cache_misses = $telemetry.sprite2_cache_misses -eq 152
    sprite2_cache_evictions = (
        $telemetry.sprite2_cache_evictions -eq 131
    )
    sprite2_cache_drops = $telemetry.sprite2_cache_drops -eq 0
    sprite2_uploads = $telemetry.sprite2_uploads -eq 152
    sprite2_max_uploads = (
        $telemetry.sprite2_max_uploads_per_frame -eq 7
    )
    sprite2_upload_accounting = (
        $telemetry.sprite2_upload_bytes -eq
            $telemetry.sprite2_uploads * 1024
    )
    sprite2_cache_geometry = (
        $telemetry.sprite2_cache_slots -eq 21 -and
        $telemetry.sprite2_max_visible_unique -eq 16
    )
    effect_cache_hits = $telemetry.effect_cache_hits -eq 4266
    effect_cache_misses = $telemetry.effect_cache_misses -eq 2382
    effect_cache_evictions = $telemetry.effect_cache_evictions -eq 2350
    effect_cache_drops = $telemetry.effect_cache_drops -eq 0
    effect_cache_uploads = $telemetry.effect_cache_uploads -eq 2382
    effect_cache_upload_accounting = (
        $telemetry.effect_cache_upload_bytes -eq
            $telemetry.effect_cache_uploads * 128
    )
    effect_cache_geometry = (
        $telemetry.effect_cache_max_uploads_per_frame -eq 11 -and
        $telemetry.effect_cache_max_visible_unique -eq 11
    )
    source_powerup_consolation_cash = (
        $telemetry.source_parity_powerup_consolation_cash -eq 0
    )
    source_orbiting_asteroid_pickups = (
        $telemetry.source_parity_orbiting_asteroid_pickups -eq 0
    )
    source_superbomb_pickups = (
        $telemetry.source_parity_superbomb_pickups -eq 0
    )
    source_hotdog_pickups = $telemetry.source_parity_hotdog_pickups -eq 0
    source_armor_pickups = $telemetry.source_parity_armor_pickups -eq 0
    source_bonus_portal_pickups = (
        $telemetry.source_parity_bonus_portal_pickups -eq 0
    )
    source_high_value_pickups = (
        $telemetry.source_parity_high_value_pickups -eq 0
    )
    source_front_weapon_state = (
        $telemetry.source_parity_front_weapon_id -eq 1 -and
        $telemetry.source_parity_front_weapon_power -eq 1
    )
    source_rear_weapon_state = (
        $telemetry.source_parity_rear_weapon_id -eq 0 -and
        $telemetry.source_parity_rear_weapon_power -eq 1
    )
    source_equipment_state = (
        $telemetry.source_parity_superbombs -eq 0 -and
        $telemetry.source_parity_armor -eq 10 -and
        $telemetry.source_parity_weapon_mode -eq 1 -and
        $telemetry.source_parity_special -eq 0 -and
        $telemetry.source_parity_purple_balls_needed -eq 1
    )
    source_bonus_state = (
        $telemetry.source_parity_bonus_level -eq 0 -and
        $telemetry.source_parity_next_level -eq 0 -and
        $telemetry.source_parity_display_time -eq 0
    )
    final_player_source_position = (
        $telemetry.final_player_source_x -eq 77 -and
        $telemetry.final_player_source_y -eq 17
    )
    final_source_parallax_offsets = (
        $telemetry.final_map_x_offset -eq 24 -and
        $telemetry.final_map_x2_offset -eq 49 -and
        $telemetry.final_map_x3_offset -eq 74
    )
    final_background_horizontal_offsets = (
        $telemetry.final_bg1_horizontal_offset -eq (
            84 - $telemetry.final_map_x_offset
        ) -and
        $telemetry.final_bg2_horizontal_offset -eq (
            84 - $telemetry.final_map_x2_offset
        ) -and
        $telemetry.final_bg3_horizontal_offset -eq (
            108 - $telemetry.final_map_x3_offset
        )
    )
    final_source_background_scroll = (
        $telemetry.final_bg1_source_scroll -eq 2363 -and
        $telemetry.final_bg2_source_scroll -eq 5225 -and
        $telemetry.final_bg3_source_scroll -eq 4198
    )
    presentation_is_central_1to1_crop = (
        $telemetry.presentation_crop_x -eq 36 -and
        $telemetry.presentation_crop_y -eq 12
    )
    layer_priority_exhaustive_checks = (
        $telemetry.layer_rule_checks -eq 252 -and
        $telemetry.layer_rule_failures -eq 0
    )
    final_pc_layer_flags = (
        $telemetry.final_background2_over -eq 1 -and
        $telemetry.final_background3_over -eq 1 -and
        $telemetry.final_top_enemy_over -eq 0 -and
        $telemetry.final_sky_enemy_over_all -eq 0
    )
    final_gba_background_priorities = (
        $telemetry.final_background2_priority -eq 2 -and
        $telemetry.final_background3_priority -eq 1
    )
    romfs_entries = $telemetry.romfs_entries -eq $romfsAudit.entry_count
    romfs_image_bytes = $telemetry.romfs_image_bytes -eq $romfsAudit.image_bytes
    romfs_payload_bytes = (
        $telemetry.romfs_payload_bytes -eq $romfsAudit.payload_bytes
    )
    romfs_self_test_checks = (
        $telemetry.romfs_self_test_checks -eq $romfsExpectedSelfTestChecks
    )
    romfs_self_test_failures = $telemetry.romfs_self_test_failures -eq 0
    romfs_manifest_crc32 = (
        $telemetry.romfs_manifest_crc32 -eq $romfsManifestCrc32
    )
    state_transitions = $telemetry.state_transitions -eq 5
}

# The legacy block above remains as an audit record for the position-5400
# proof.  The active regression contract follows the authored boss, end
# flight, stats screen and return to the PC-style Game Menu.
$telemetryChecks = [ordered]@{
    schema_version = $telemetry.version -eq 20
    rom_reported_pass = $telemetry.pass -eq 1
    returned_to_game_menu = $telemetry.final_state -eq 7
    title_music_active = $telemetry.title_music_active -eq 1
    full_level_logic_updates = $telemetry.logic_updates -eq 7828
    full_level_display_frames = $telemetry.display_frames -eq 13502
    authored_boss_exit_position = $telemetry.final_level_position -eq 6477
    authored_event_cursor = $telemetry.final_source_event_index -eq 935
    full_level_tick = $telemetry.final_level_tick -eq 7828
    frontend_and_level_transitions = $telemetry.state_transitions -eq 11
    vblank_budget = $telemetry.missed_vblanks -le 640
    hardware_oam_limit = $telemetry.max_hardware_oam -le 128
    no_map_stream_drops = $telemetry.stream_drops -eq 0
    no_reward_drops = $telemetry.reward_drops -eq 0
    no_enemy_pool_replacements = $telemetry.enemy_pool_replacements -eq 0
    pause_round_trip = (
        $telemetry.pause_toggles -eq 2 -and
        $telemetry.paused_display_frames -eq 60 -and
        $telemetry.final_game_paused -eq 0
    )
    source_event_accounting = (
        $telemetry.source_parity_events -eq 935 -and
        $telemetry.source_parity_events_applied -eq 926 -and
        $telemetry.source_parity_events_deferred -eq 5 -and
        $telemetry.source_parity_events_skipped -eq 4 -and
        $telemetry.source_parity_events_applied +
            $telemetry.source_parity_events_deferred +
            $telemetry.source_parity_events_skipped -eq
            $telemetry.source_parity_events
    )
    source_spawn_accounting = (
        $telemetry.source_parity_spawn_attempts -eq 473 -and
        $telemetry.source_parity_spawn_successes -eq 473 -and
        $telemetry.source_parity_spawn_pool_full -eq 0 -and
        $telemetry.source_parity_spawn_missing -eq 0
    )
    authored_enemy_kills = $telemetry.source_parity_enemy_kills -eq 100
    authored_boss_group_cleared = (
        $telemetry.source_parity_final_active_enemies -eq 0
    )
    data_cube_pickup = $telemetry.source_parity_data_cube_pickups -eq 1
    final_cash = $telemetry.final_cash -eq 1456
    source_assets_valid = $telemetry.source_parity_assets_valid -eq 1
    no_unknown_enemy_visuals = (
        $telemetry.source_parity_unknown_visuals -eq 0
    )
    sprite2_cache_accounting = (
        $telemetry.sprite2_decode_failures -eq 0 -and
        $telemetry.sprite2_cache_drops -eq 0 -and
        $telemetry.sprite2_uploads -eq
            $telemetry.sprite2_cache_misses -and
        $telemetry.sprite2_upload_bytes -eq
            $telemetry.sprite2_uploads * 1024
    )
    effect_cache_accounting = (
        $telemetry.effect_cache_drops -eq 0 -and
        $telemetry.effect_cache_uploads -eq
            $telemetry.effect_cache_misses -and
        $telemetry.effect_cache_upload_bytes -eq
            $telemetry.effect_cache_uploads * 128
    )
    presentation_is_central_1to1_crop = (
        $telemetry.presentation_crop_x -eq 36 -and
        $telemetry.presentation_crop_y -eq 12
    )
    layer_priority_exhaustive_checks = (
        $telemetry.layer_rule_checks -eq 252 -and
        $telemetry.layer_rule_failures -eq 0
    )
    final_gba_background_priorities = (
        $telemetry.final_background2_priority -eq 2 -and
        $telemetry.final_background3_priority -eq 1
    )
    romfs_entries = $telemetry.romfs_entries -eq $romfsAudit.entry_count
    romfs_image_bytes = $telemetry.romfs_image_bytes -eq $romfsAudit.image_bytes
    romfs_payload_bytes = (
        $telemetry.romfs_payload_bytes -eq $romfsAudit.payload_bytes
    )
    romfs_self_test_checks = (
        $telemetry.romfs_self_test_checks -eq $romfsExpectedSelfTestChecks
    )
    romfs_self_test_failures = $telemetry.romfs_self_test_failures -eq 0
    romfs_manifest_crc32 = (
        $telemetry.romfs_manifest_crc32 -eq $romfsManifestCrc32
    )
}
$failedTelemetryChecks = @(
    $telemetryChecks.GetEnumerator() |
        Where-Object { -not $_.Value } |
        ForEach-Object { $_.Key }
)
if ($failedTelemetryChecks.Count -ne 0) {
    throw (
        "Auto-test SRAM failed invariant(s): " +
        ($failedTelemetryChecks -join ", ")
    )
}

$perfElapsed = Start-TestProcess `
    -FilePath $perf `
    -Arguments @("-F", "600", "-P", "$releaseName.gba") `
    -WorkingDirectory $buildDir `
    -StandardOutput $perfStdout `
    -StandardError $perfStderr
$perfLines = @(Get-Content -LiteralPath $perfStdout)
if (
    $perfLines.Count -lt 2 -or
    $perfLines[0] -ne "game_code,frames,duration,renderer" -or
    $perfLines[1] -notmatch "^AGB-TYGA,600,"
) {
    throw "Release-ROM 600-frame boot benchmark did not complete as expected"
}

$compilerVersion = (& (Join-Path $ucrtBin "arm-none-eabi-gcc.exe") `
    -dumpfullversion).Trim()
$mgbaVersion = (& $headless --version "version-probe.gba" |
    Select-Object -First 1).Trim()
$soundbankBytes = (Get-Item -LiteralPath (
    Join-Path $projectRoot "res\soundbank.bin"
)).Length

$verification = [System.Collections.Generic.List[string]]::new()
$verification.Add("result=PASS")
$verification.Add("verified_at=$([DateTime]::UtcNow.ToString('o'))")
$verification.Add("compiler=$compilerVersion")
$verification.Add("emulator=$mgbaVersion")
foreach ($info in @($releaseInfo, $testInfo)) {
    foreach ($entry in $info.GetEnumerator()) {
        $verification.Add("$($info.name)_$($entry.Key)=$($entry.Value)")
    }
}
$verification.Add("soundbank_bytes=$soundbankBytes")
$verification.Add("romfs_files=$($romfsAudit.entry_count)")
$verification.Add("romfs_probes=$($romfsAudit.probe_count)")
$verification.Add("romfs_payload_bytes=$($romfsAudit.payload_bytes)")
$verification.Add("romfs_image_bytes=$($romfsAudit.image_bytes)")
$verification.Add("romfs_overhead_bytes=$($romfsAudit.overhead_bytes)")
$verification.Add("romfs_manifest_crc32=$($romfsAudit.manifest_crc32)")
$verification.Add("romfs_metadata_crc32=$($romfsAudit.metadata_crc32)")
$verification.Add("romfs_payload_crc32=$($romfsAudit.payload_crc32)")
$verification.Add("romfs_image_sha256=$romfsImageSha256")
$verification.Add("autotest_host_elapsed_ms=$testElapsed")
$verification.Add("autotest_runtime_error_count=$($runtimeErrors.Count)")
foreach ($entry in $telemetry.GetEnumerator()) {
    $verification.Add("telemetry_$($entry.Key)=$($entry.Value)")
}
$verification.Add("release_boot_frames=600")
$verification.Add("release_boot_host_elapsed_ms=$perfElapsed")
$verification.Add("release_boot_csv=$($perfLines[1])")
$verification.Add(
    "artifact_policy=" +
    $(if ($KeepIntermediates) { "keep-intermediates" } else { "release-only" })
)
$verificationLines = [string[]]$verification
$verificationLines |
    Set-Content -LiteralPath $verificationPath -Encoding utf8

if ($KeepIntermediates) {
    $artifactResult = [pscustomobject]@{
        ArchivedRoms = 0
        DeduplicatedRoms = 0
        RemovedEntries = 0
    }
} else {
    $artifactResult = Invoke-BuildArtifactPolicy `
        -BuildDirectory $buildDir `
        -ReleaseRom $releaseRom `
        -BackupDirectory $backupDir
}

$verificationLines
"artifact_archived_roms=$($artifactResult.ArchivedRoms)"
"artifact_deduplicated_roms=$($artifactResult.DeduplicatedRoms)"
"artifact_removed_entries=$($artifactResult.RemovedEntries)"
"artifact_retained_rom=$releaseRom"
