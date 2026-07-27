param(
    [switch]$KeepIntermediates,
    [ValidateSet("low", "normal", "high", "pentium")]
    [string]$DetailLevel = "high",
    [ValidateSet("low", "normal")]
    [string]$GameSpeed = "normal"
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
$configSuffix = "detail_${DetailLevel}_speed_${GameSpeed}"
$releaseName = "tyrian_gba_level1_pc_flow_mode4_romfs_v37_$configSuffix"
$testName = "tyrian_gba_level1_pc_flow_mode4_autotest_romfs_v37_$configSuffix"
$deathTestName = "tyrian_gba_level1_pc_flow_mode4_death_autotest_romfs_v37_$configSuffix"
$jukeboxTestName = "tyrian_gba_jukebox_autotest_romfs_v37_$configSuffix"
$matrixTestName = "tyrian_gba_romfs_all_levels_matrix_v37_$configSuffix"
$campaignTestName = "tyrian_gba_campaign_smoke_ep1_section1_levels4_v37_$configSuffix"
$episode2TestName = "tyrian_gba_route_smoke_ep2_section1_v37_$configSuffix"
$releaseRom = Join-Path $buildDir "$releaseName.gba"
$testRom = Join-Path $buildDir "$testName.gba"
$deathTestRom = Join-Path $buildDir "$deathTestName.gba"
$jukeboxTestRom = Join-Path $buildDir "$jukeboxTestName.gba"
$matrixTestRom = Join-Path $buildDir "$matrixTestName.gba"
$campaignTestRom = Join-Path $buildDir "$campaignTestName.gba"
$episode2TestRom = Join-Path $buildDir "$episode2TestName.gba"
$testSave = Join-Path $buildDir "$testName.sav"
$deathTestSave = Join-Path $buildDir "$deathTestName.sav"
$jukeboxTestSave = Join-Path $buildDir "$jukeboxTestName.sav"
$matrixTestSave = Join-Path $buildDir "$matrixTestName.sav"
$campaignTestSave = Join-Path $buildDir "$campaignTestName.sav"
$episode2TestSave = Join-Path $buildDir "$episode2TestName.sav"
$testStdout = Join-Path $buildDir "autotest_mgba_stdout.txt"
$testStderr = Join-Path $buildDir "autotest_mgba_stderr.txt"
$deathTestStdout = Join-Path $buildDir "death_autotest_mgba_stdout.txt"
$deathTestStderr = Join-Path $buildDir "death_autotest_mgba_stderr.txt"
$jukeboxTestStdout = Join-Path $buildDir "jukebox_autotest_mgba_stdout.txt"
$jukeboxTestStderr = Join-Path $buildDir "jukebox_autotest_mgba_stderr.txt"
$matrixTestStdout = Join-Path $buildDir "matrix_autotest_mgba_stdout.txt"
$matrixTestStderr = Join-Path $buildDir "matrix_autotest_mgba_stderr.txt"
$campaignTestStdout = Join-Path $buildDir "campaign_autotest_mgba_stdout.txt"
$campaignTestStderr = Join-Path $buildDir "campaign_autotest_mgba_stderr.txt"
$episode2TestStdout = Join-Path $buildDir "episode2_autotest_mgba_stdout.txt"
$episode2TestStderr = Join-Path $buildDir "episode2_autotest_mgba_stderr.txt"
$perfStdout = Join-Path $buildDir "release_boot_perf.csv"
$perfStderr = Join-Path $buildDir "release_boot_perf.stderr.txt"
$verificationPath = Join-Path $buildDir "verification.txt"
$backupDir = Join-Path $projectRoot "Backup"
$romfsImagePath = Join-Path $projectRoot "res\tyrian_romfs.bin"
$romfsAuditPath = Join-Path $projectRoot "res\tyrian_romfs_audit.json"
$sprite2RawPath = Join-Path $projectRoot "res\sprite2_raw_components.bin"
$sprite2RawAuditPath = Join-Path $projectRoot "res\sprite2_raw_audit.txt"
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
make PYTHON="__PYTHON__" DETAIL_LEVEL="__DETAIL__" GAME_SPEED="__SPEED__" assets
make -j2 PYTHON="__PYTHON__" DETAIL_LEVEL="__DETAIL__" GAME_SPEED="__SPEED__" ROUTE_EPISODE=2 ROUTE_SECTION=1 all autotest death-autotest jukebox-autotest romfs-matrix-autotest route-smoke-autotest campaign-smoke-autotest
'@
$buildCommand = $buildCommand.Replace("__PROJECT__", $unixProject)
$buildCommand = $buildCommand.Replace("__PYTHON__", $unixPython)
$buildCommand = $buildCommand.Replace("__DETAIL__", $DetailLevel)
$buildCommand = $buildCommand.Replace("__SPEED__", $GameSpeed)

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

foreach ($rawOutput in @($sprite2RawPath, $sprite2RawAuditPath)) {
    if (-not (Test-Path -LiteralPath $rawOutput)) {
        throw "Sprite2 raw build output is missing: $rawOutput"
    }
}
$sprite2RawAudit = [ordered]@{}
foreach (
    $line in Get-Content -LiteralPath $sprite2RawAuditPath
) {
    $pair = $line.Split("=", 2)
    if ($pair.Count -eq 2) {
        $sprite2RawAudit[$pair[0]] = $pair[1]
    }
}
$sprite2RawBytes = (Get-Item -LiteralPath $sprite2RawPath).Length
$sprite2RawSha256 = (
    Get-FileHash -LiteralPath $sprite2RawPath -Algorithm SHA256
).Hash.ToLowerInvariant()
$sprite2RawCrc32 = [Convert]::ToUInt32("38f795b9", 16)
if (
    $sprite2RawAudit.version -ne "1" -or
    $sprite2RawAudit.table_count -ne "38" -or
    $sprite2RawAudit.components_per_table -ne "304" -or
    $sprite2RawAudit.component_count -ne "11552" -or
    $sprite2RawAudit.component_width -ne "12" -or
    $sprite2RawAudit.component_height -ne "14" -or
    $sprite2RawAudit.component_bytes -ne "168" -or
    $sprite2RawBytes -ne 1940736 -or
    [int64]$sprite2RawAudit.raw_bytes -ne $sprite2RawBytes -or
    $sprite2RawAudit.raw_crc32 -ne "38f795b9" -or
    $sprite2RawAudit.raw_sha256 -ne $sprite2RawSha256 -or
    $sprite2RawSha256 -ne
        "bbfddd080955fd639eeabe151b5cab0aacbbea5917b39bf56fc6693f048ceca4" -or
    $sprite2RawAudit.source_stream_bytes -ne "1151417" -or
    $sprite2RawAudit.source_stream_crc32 -ne "2a635936" -or
    $sprite2RawAudit.roundtrip_components -ne "11552"
) {
    throw "Sprite2 raw audit does not match the stock logical bank catalog"
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

function Test-GbaMemoryBudget {
    param(
        [Parameter(Mandatory)]
        [string]$Name,
        [Parameter(Mandatory)]
        [string]$MapPath
    )

    $mapText = Get-Content -LiteralPath $MapPath -Raw
    $ewramMatch = [regex]::Match(
        $mapText,
        "(?m)^\s*0x(?<address>02[0-9a-fA-F]{6})\s+__eheap_start\b"
    )
    $iwramMatch = [regex]::Match(
        $mapText,
        "(?m)^\s*0x(?<address>03[0-9a-fA-F]{6})\s+__iheap_start\b"
    )
    if (-not $ewramMatch.Success -or -not $iwramMatch.Success) {
        throw "Unable to read GBA memory limits from linker map: $MapPath"
    }

    $ewramStart = [Convert]::ToUInt32(
        $ewramMatch.Groups["address"].Value,
        16
    )
    $iwramStart = [Convert]::ToUInt32(
        $iwramMatch.Groups["address"].Value,
        16
    )
    $ewramFree = 0x02040000L - $ewramStart
    $iwramFree = 0x03008000L - $iwramStart
    if (
        $ewramFree -lt 48KB -or
        $iwramFree -lt 6KB
    ) {
        throw (
            "GBA memory safety margin regressed for ${Name}: " +
            "EWRAM free=$ewramFree, IWRAM free=$iwramFree"
        )
    }

    [ordered]@{
        name = $Name
        ewram_heap_start = "0x$($ewramStart.ToString('X8'))"
        ewram_free_bytes = $ewramFree
        iwram_heap_start = "0x$($iwramStart.ToString('X8'))"
        iwram_free_bytes = $iwramFree
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
$deathTestInfo = Test-GbaRom `
    -Name "death_autotest" `
    -Path $deathTestRom `
    -ExpectedGameCode "TYGD"
$jukeboxTestInfo = Test-GbaRom `
    -Name "jukebox_autotest" `
    -Path $jukeboxTestRom `
    -ExpectedGameCode "TYGJ"
$matrixTestInfo = Test-GbaRom `
    -Name "matrix_autotest" `
    -Path $matrixTestRom `
    -ExpectedGameCode "TYGM"
$campaignTestInfo = Test-GbaRom `
    -Name "campaign_autotest" `
    -Path $campaignTestRom `
    -ExpectedGameCode "TYGC"
$episode2TestInfo = Test-GbaRom `
    -Name "episode2_autotest" `
    -Path $episode2TestRom `
    -ExpectedGameCode "TYGR"
$memoryInfos = @(
    Test-GbaMemoryBudget `
        -Name "release" `
        -MapPath ([IO.Path]::ChangeExtension($releaseRom, ".map"))
    Test-GbaMemoryBudget `
        -Name "autotest" `
        -MapPath ([IO.Path]::ChangeExtension($testRom, ".map"))
    Test-GbaMemoryBudget `
        -Name "death_autotest" `
        -MapPath ([IO.Path]::ChangeExtension($deathTestRom, ".map"))
    Test-GbaMemoryBudget `
        -Name "jukebox_autotest" `
        -MapPath ([IO.Path]::ChangeExtension($jukeboxTestRom, ".map"))
    Test-GbaMemoryBudget `
        -Name "matrix_autotest" `
        -MapPath ([IO.Path]::ChangeExtension($matrixTestRom, ".map"))
    Test-GbaMemoryBudget `
        -Name "campaign_autotest" `
        -MapPath ([IO.Path]::ChangeExtension($campaignTestRom, ".map"))
    Test-GbaMemoryBudget `
        -Name "episode2_autotest" `
        -MapPath ([IO.Path]::ChangeExtension($episode2TestRom, ".map"))
)

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
if ($saveBytes.Length -lt 6312) {
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
    pickup_explosion_spawns = Read-TelemetryU32 588
    pickup_explosion_drops = Read-TelemetryU32 592
    pickup_explosion_max_active = Read-TelemetryU32 596
    configured_detail_level = Read-TelemetryU32 600
    configured_game_speed = Read-TelemetryU32 604
    final_background2_enabled = Read-TelemetryU32 608
    end_level_music_starts = Read-TelemetryU32 612
    end_level_trail_max = Read-TelemetryU32 616
    level_complete_voice_starts = Read-TelemetryU32 620
    stats_stage_advances = Read-TelemetryU32 624
    stats_cube_reveals = Read-TelemetryU32 628
    final_stats_stage = Read-TelemetryU32 632
    final_stats_cube_visible_count = Read-TelemetryU32 636
    final_player_end_warp = Read-TelemetryU32 640
    initial_player_end_warp = Read-TelemetryU32 644
    background_cache_approximations = Read-TelemetryU32 648
    background_cache_evictions = Read-TelemetryU32 652
    background_cache_uploads = Read-TelemetryU32 656
    background_cache_layer0_valid = Read-TelemetryU32 660
    background_cache_layer1_valid = Read-TelemetryU32 664
    background_cache_layer2_valid = Read-TelemetryU32 668
    background_rows_prefetched = Read-TelemetryU32 672
    background_rows_synchronous = Read-TelemetryU32 676
    background_rows_existing = Read-TelemetryU32 680
    background_prefetch_late_columns = Read-TelemetryU32 684
    background_tile_render_count = Read-TelemetryU32 688
    projectile_cache_hits = Read-TelemetryU32 692
    projectile_cache_misses = Read-TelemetryU32 696
    projectile_cache_evictions = Read-TelemetryU32 700
    projectile_cache_drops = Read-TelemetryU32 704
    projectile_cache_uploads = Read-TelemetryU32 708
    projectile_cache_max_uploads = Read-TelemetryU32 712
    projectile_cache_max_visible_unique = Read-TelemetryU32 716
    sprite2_compact_uploads = Read-TelemetryU32 6084
    source_sound_mask_low = Read-TelemetryU32 6088
    source_sound_mask_high = Read-TelemetryU32 6092
    secret_level_collision_pass = Read-TelemetryU32 6112
    boss_perf_started = Read-TelemetryU32 6200
    boss_perf_completed = Read-TelemetryU32 6204
    boss_perf_start_position = Read-TelemetryU32 6208
    boss_perf_end_position = Read-TelemetryU32 6212
    boss_perf_display_frames = Read-TelemetryU32 6216
    boss_perf_missed_vblanks = Read-TelemetryU32 6220
    boss_perf_sprite2_misses = Read-TelemetryU32 6224
    boss_perf_sprite2_evictions = Read-TelemetryU32 6228
    boss_perf_sprite2_upload_bytes = Read-TelemetryU32 6232
    boss_perf_projectile_misses = Read-TelemetryU32 6236
    waitcnt = Read-TelemetryU32 6240
    sprite2_l2_hits = Read-TelemetryU32 6244
    sprite2_l2_misses = Read-TelemetryU32 6248
    sprite2_l2_evictions = Read-TelemetryU32 6252
    sprite2_l2_drops = Read-TelemetryU32 6256
    sprite2_l2_flushes = Read-TelemetryU32 6260
    sprite2_l2_raw_builds = Read-TelemetryU32 6264
    sprite2_l2_rle_fallbacks = Read-TelemetryU32 6268
    sprite2_l2_max_visible_unique = Read-TelemetryU32 6272
    boss_perf_l2_hits = Read-TelemetryU32 6276
    boss_perf_l2_misses = Read-TelemetryU32 6280
    boss_perf_l2_evictions = Read-TelemetryU32 6284
    boss_perf_l2_raw_builds = Read-TelemetryU32 6288
    boss_perf_l2_fallbacks = Read-TelemetryU32 6292
    sprite2_raw_catalog_valid = Read-TelemetryU32 6296
    sprite2_raw_bytes = Read-TelemetryU32 6300
    sprite2_raw_crc32 = Read-TelemetryU32 6304
    sprite2_l2_slots = Read-TelemetryU32 6308
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
$expectedDetailLevel = switch ($DetailLevel) {
    "low" { 0 }
    "normal" { 1 }
    "high" { 2 }
    "pentium" { 3 }
    default { throw "Unsupported detail profile: $DetailLevel" }
}
$expectedGameSpeed = if ($GameSpeed -eq "low") { 0 } else { 1 }
$expectedDisplayFrames = if ($GameSpeed -eq "low") { $null } else { 12168 }
$expectedBossDisplayFrames = if ($GameSpeed -eq "low") { $null } else { 439 }
$telemetryChecks = [ordered]@{
    schema_version = $telemetry.version -eq 25
    rom_reported_pass = $telemetry.pass -eq 1
    returned_to_game_menu = $telemetry.final_state -eq 7
    title_music_active = $telemetry.title_music_active -eq 1
    full_level_logic_updates = $telemetry.logic_updates -eq 7051
    full_level_display_frames = (
        (
            $GameSpeed -eq "low" -and
            $telemetry.display_frames -gt 12168
        ) -or
        $telemetry.display_frames -eq $expectedDisplayFrames
    )
    authored_boss_exit_position = $telemetry.final_level_position -eq 5700
    authored_event_cursor = $telemetry.final_source_event_index -eq 892
    full_level_tick = $telemetry.final_level_tick -eq 7051
    frontend_and_level_transitions = $telemetry.state_transitions -eq 11
    vblank_budget = $telemetry.missed_vblanks -le 20
    hardware_oam_limit = $telemetry.max_hardware_oam -le 128
    no_map_stream_drops = $telemetry.stream_drops -eq 0
    no_reward_drops = $telemetry.reward_drops -eq 0
    source_pickup_explosion_labels = (
        $telemetry.pickup_explosion_spawns -eq
            $telemetry.source_parity_score_item_pickups -and
        $telemetry.pickup_explosion_drops -eq 0 -and
        $telemetry.pickup_explosion_max_active -le 32
    )
    source_end_level_flight = (
        $telemetry.end_level_music_starts -eq 1 -and
        $telemetry.initial_player_end_warp -eq 252 -and
        $telemetry.end_level_trail_max -eq 16 -and
        $telemetry.final_player_end_warp -eq 27
    )
    source_end_level_stats = (
        $telemetry.level_complete_voice_starts -eq 1 -and
        $telemetry.stats_stage_advances -eq 4 -and
        $telemetry.stats_cube_reveals -eq 2 -and
        $telemetry.final_stats_stage -eq 4 -and
        $telemetry.final_stats_cube_visible_count -eq 2
    )
    source_sound_catalog_coverage = (
        # First-level authored trace reaches SFX plus voices 30..37,
        # including Boss (31), Good Luck (33) and Data Cube (37).
        $telemetry.source_sound_mask_low -eq 0xE70211ACu -and
        $telemetry.source_sound_mask_high -eq 0x0000001F
    )
    source_secret_level_collision = (
        $telemetry.secret_level_collision_pass -eq 1
    )
    requested_port_configuration = (
        $telemetry.configured_detail_level -eq $expectedDetailLevel -and
        $telemetry.configured_game_speed -eq $expectedGameSpeed
    )
    low_detail_background2_state = (
        $DetailLevel -ne "low" -or
        $telemetry.final_background2_enabled -eq 0
    )
    no_enemy_pool_replacements = $telemetry.enemy_pool_replacements -eq 0
    pause_round_trip = (
        $telemetry.pause_toggles -eq 2 -and
        $telemetry.paused_display_frames -eq 60 -and
        $telemetry.final_game_paused -eq 0
    )
    source_event_accounting = (
        $telemetry.source_parity_events -eq 892 -and
        $telemetry.source_parity_events_applied -eq 888 -and
        $telemetry.source_parity_events_deferred -eq 0 -and
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
    authored_enemy_kills = $telemetry.source_parity_enemy_kills -eq 298
    authored_boss_group_cleared = (
        $telemetry.source_parity_final_active_enemies -eq 0
    )
    data_cube_pickup = $telemetry.source_parity_data_cube_pickups -eq 2
    final_cash = $telemetry.final_cash -eq 4573
    source_assets_valid = $telemetry.source_parity_assets_valid -eq 1
    no_unknown_enemy_visuals = (
        $telemetry.source_parity_unknown_visuals -eq 0
    )
    sprite2_cache_accounting = (
        $telemetry.sprite2_decode_failures -eq 0 -and
        $telemetry.sprite2_cache_drops -eq 0 -and
        $telemetry.sprite2_uploads -eq
            $telemetry.sprite2_cache_misses -and
        $telemetry.sprite2_compact_uploads -le
            $telemetry.sprite2_uploads -and
        $telemetry.sprite2_upload_bytes -eq
            (
                $telemetry.sprite2_uploads -
                    $telemetry.sprite2_compact_uploads
            ) * 1024 +
                $telemetry.sprite2_compact_uploads * 256 -and
        $telemetry.sprite2_cache_slots -eq 24
    )
    # v37 runs the stock five-shot, power-11 Pulse-Cannon.  This intentionally
    # replaces the old single power-1 bullet workload while retaining exact
    # deterministic cache goldens for the new source loadout.
    sprite2_workload_unchanged = (
        $telemetry.sprite2_cache_misses -eq 283 -and
        $telemetry.sprite2_cache_evictions -eq 259 -and
        $telemetry.sprite2_uploads -eq 283 -and
        $telemetry.sprite2_upload_bytes -eq 280576 -and
        $telemetry.sprite2_max_uploads_per_frame -eq 15 -and
        $telemetry.projectile_cache_misses -eq 6
    )
    projectile_cache_accounting = (
        $telemetry.projectile_cache_drops -eq 0 -and
        $telemetry.projectile_cache_uploads -eq
            $telemetry.projectile_cache_misses -and
        $telemetry.projectile_cache_max_visible_unique -le 8
    )
    sprite2_raw_catalog = (
        $telemetry.sprite2_raw_catalog_valid -eq 1 -and
        $telemetry.sprite2_raw_bytes -eq $sprite2RawBytes -and
        $telemetry.sprite2_raw_crc32 -eq $sprite2RawCrc32
    )
    sprite2_l2_accounting = (
        $telemetry.sprite2_l2_slots -eq 64 -and
        $telemetry.sprite2_l2_drops -eq 0 -and
        $telemetry.sprite2_l2_flushes -eq 1 -and
        $telemetry.sprite2_l2_rle_fallbacks -eq 0 -and
        $telemetry.sprite2_l2_raw_builds -eq
            $telemetry.sprite2_l2_misses -and
        $telemetry.sprite2_l2_hits +
            $telemetry.sprite2_l2_misses -eq
            $telemetry.sprite2_cache_misses +
                $telemetry.projectile_cache_misses -and
        $telemetry.sprite2_l2_max_visible_unique -le
            $telemetry.sprite2_l2_slots
    )
    sprite2_l2_golden = (
        $telemetry.sprite2_l2_hits -eq 102 -and
        $telemetry.sprite2_l2_misses -eq 187 -and
        $telemetry.sprite2_l2_evictions -eq 123 -and
        $telemetry.sprite2_l2_raw_builds -eq 187 -and
        $telemetry.sprite2_l2_max_visible_unique -eq 15
    )
    gamepak_prefetch_waitstate = $telemetry.waitcnt -eq 0x4317
    authored_boss_perf_window = (
        $telemetry.boss_perf_started -eq 1 -and
        $telemetry.boss_perf_completed -eq 1 -and
        $telemetry.boss_perf_start_position -eq 5401 -and
        $telemetry.boss_perf_end_position -eq 5657 -and
        (
            (
                $GameSpeed -eq "low" -and
                $telemetry.boss_perf_display_frames -gt 439
            ) -or
            $telemetry.boss_perf_display_frames -eq
                $expectedBossDisplayFrames
        ) -and
        $telemetry.boss_perf_sprite2_misses -eq 106 -and
        $telemetry.boss_perf_sprite2_evictions -eq 106 -and
        $telemetry.boss_perf_sprite2_upload_bytes -eq 101632 -and
        $telemetry.boss_perf_projectile_misses -eq 0
    )
    authored_boss_perf_budget = (
        $telemetry.boss_perf_missed_vblanks -le 8
    )
    authored_boss_l2_golden = (
        $telemetry.boss_perf_l2_hits -eq 85 -and
        $telemetry.boss_perf_l2_misses -eq 21 -and
        $telemetry.boss_perf_l2_evictions -eq 21 -and
        $telemetry.boss_perf_l2_raw_builds -eq 21 -and
        $telemetry.boss_perf_l2_fallbacks -eq 0
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

if (Test-Path -LiteralPath $deathTestSave) {
    Remove-Item -LiteralPath $deathTestSave -Force
}
$deathTestElapsed = Start-TestProcess `
    -FilePath $headless `
    -Arguments @("-S", "3", "$deathTestName.gba") `
    -WorkingDirectory $buildDir `
    -StandardOutput $deathTestStdout `
    -StandardError $deathTestStderr
if (-not (Test-Path -LiteralPath $deathTestSave)) {
    throw (
        "mGBA did not create the expected death auto-test SRAM file: " +
        $deathTestSave
    )
}
$deathRuntimeErrors = @(
    Select-String `
        -Path $deathTestStdout, $deathTestStderr `
        -Pattern "Bad memory|Invalid|Illegal|Hard crash|Fatal|Failed|Error"
)
if ($deathRuntimeErrors.Count -ne 0) {
    throw (
        "mGBA death auto-test reported " +
        "$($deathRuntimeErrors.Count) runtime error(s)"
    )
}
$deathSaveBytes = [System.IO.File]::ReadAllBytes($deathTestSave)
if ($deathSaveBytes.Length -lt 120) {
    throw "Death auto-test SRAM telemetry is truncated"
}
$deathMagic = [Text.Encoding]::ASCII.GetString($deathSaveBytes, 0, 4)
if ($deathMagic -ne "TGD2") {
    throw "Death auto-test SRAM magic mismatch: '$deathMagic'"
}
function Read-DeathTelemetryU32 {
    param([int]$Offset)
    return [BitConverter]::ToUInt32($deathSaveBytes, $Offset)
}
$deathTelemetry = [ordered]@{
    game_over_song = Read-DeathTelemetryU32 4
    game_over_music_active = Read-DeathTelemetryU32 8
    game_over_state = Read-DeathTelemetryU32 12
    dev_invincible = Read-DeathTelemetryU32 16
    player_alive = Read-DeathTelemetryU32 20
    exploding_ticks = Read-DeathTelemetryU32 24
    large_explosion_calls = Read-DeathTelemetryU32 28
    explosion_9_starts = Read-DeathTelemetryU32 32
    explosion_11_starts = Read-DeathTelemetryU32 36
    explosion_22_starts = Read-DeathTelemetryU32 40
    music_fade_steps = Read-DeathTelemetryU32 44
    game_over_music_starts = Read-DeathTelemetryU32 48
    game_over_overlay_frames = Read-DeathTelemetryU32 52
    rng_calls = Read-DeathTelemetryU32 56
    tile_upload_pending = Read-DeathTelemetryU32 60
    final_level_end = Read-DeathTelemetryU32 64
    game_over_mode4 = Read-DeathTelemetryU32 68
    active_effects = Read-DeathTelemetryU32 72
    effect_drops = Read-DeathTelemetryU32 76
    display_frames = Read-DeathTelemetryU32 80
    final_oam = Read-DeathTelemetryU32 84
    game_over_pass = Read-DeathTelemetryU32 88
    return_song = Read-DeathTelemetryU32 92
    return_state = Read-DeathTelemetryU32 96
    return_selection = Read-DeathTelemetryU32 100
    return_mode4 = Read-DeathTelemetryU32 104
    game_over_exits = Read-DeathTelemetryU32 108
    return_music_active = Read-DeathTelemetryU32 112
    full_pass = Read-DeathTelemetryU32 116
}
$deathChecks = [ordered]@{
    rom_reported_game_over_pass = $deathTelemetry.game_over_pass -eq 1
    rom_reported_full_pass = $deathTelemetry.full_pass -eq 1
    source_game_over_song = (
        $deathTelemetry.game_over_song -eq 10 -and
        $deathTelemetry.game_over_music_active -eq 1
    )
    live_mode0_game_over = (
        $deathTelemetry.game_over_state -eq 10 -and
        $deathTelemetry.game_over_mode4 -eq 0 -and
        $deathTelemetry.tile_upload_pending -eq 0 -and
        $deathTelemetry.game_over_overlay_frames -ge 4
    )
    release_invincibility_override = (
        $deathTelemetry.dev_invincible -eq 0
    )
    source_death_completion = (
        $deathTelemetry.player_alive -eq 0 -and
        $deathTelemetry.exploding_ticks -eq 0 -and
        $deathTelemetry.final_level_end -eq 0
    )
    source_double_large_explosions = (
        $deathTelemetry.large_explosion_calls -eq 120 -and
        $deathTelemetry.active_effects -eq 96 -and
        $deathTelemetry.effect_drops -eq 0
    )
    source_explosion_sound_cadence = (
        $deathTelemetry.explosion_9_starts -eq 3 -and
        $deathTelemetry.explosion_11_starts -eq 6 -and
        $deathTelemetry.explosion_22_starts -eq 1
    )
    source_music_fade = $deathTelemetry.music_fade_steps -eq 59
    shared_mt19937_stream = $deathTelemetry.rng_calls -eq 138
    gba_oam_limit = $deathTelemetry.final_oam -le 128
    return_to_game_menu = (
        $deathTelemetry.return_song -eq 29 -and
        $deathTelemetry.return_state -eq 7 -and
        $deathTelemetry.return_selection -eq 4 -and
        $deathTelemetry.return_mode4 -eq 1 -and
        $deathTelemetry.game_over_exits -eq 1 -and
        $deathTelemetry.return_music_active -eq 1
    )
}
$failedDeathChecks = @(
    $deathChecks.GetEnumerator() |
        Where-Object { -not $_.Value } |
        ForEach-Object { $_.Key }
)
if ($failedDeathChecks.Count -ne 0) {
    throw (
        "Death auto-test failed invariant(s): " +
        ($failedDeathChecks -join ", ")
    )
}

if (Test-Path -LiteralPath $jukeboxTestSave) {
    Remove-Item -LiteralPath $jukeboxTestSave -Force
}
$jukeboxTestElapsed = Start-TestProcess `
    -FilePath $headless `
    -Arguments @("-S", "3", "$jukeboxTestName.gba") `
    -WorkingDirectory $buildDir `
    -StandardOutput $jukeboxTestStdout `
    -StandardError $jukeboxTestStderr
if (-not (Test-Path -LiteralPath $jukeboxTestSave)) {
    throw (
        "mGBA did not create the expected Jukebox auto-test SRAM file: " +
        $jukeboxTestSave
    )
}
$jukeboxRuntimeErrors = @(
    Select-String `
        -Path $jukeboxTestStdout, $jukeboxTestStderr `
        -Pattern "Bad memory|Invalid|Illegal|Hard crash|Fatal|Failed|Error"
)
if ($jukeboxRuntimeErrors.Count -ne 0) {
    throw (
        "mGBA Jukebox auto-test reported " +
        "$($jukeboxRuntimeErrors.Count) runtime error(s)"
    )
}
$jukeboxSaveBytes = [System.IO.File]::ReadAllBytes($jukeboxTestSave)
if ($jukeboxSaveBytes.Length -lt 80) {
    throw "Jukebox auto-test SRAM telemetry is truncated"
}
$jukeboxMagic = [Text.Encoding]::ASCII.GetString($jukeboxSaveBytes, 0, 4)
if ($jukeboxMagic -ne "TGJ1") {
    throw "Jukebox auto-test SRAM magic mismatch: '$jukeboxMagic'"
}
function Read-JukeboxTelemetryU32 {
    param([int]$Offset)
    return [BitConverter]::ToUInt32($jukeboxSaveBytes, $Offset)
}
$jukeboxTelemetry = [ordered]@{
    pass = Read-JukeboxTelemetryU32 4
    selected_song = Read-JukeboxTelemetryU32 8
    final_state = Read-JukeboxTelemetryU32 12
    final_mode4 = Read-JukeboxTelemetryU32 16
    display_frames = Read-JukeboxTelemetryU32 20
    track_changes = Read-JukeboxTelemetryU32 24
    previous_wraps = Read-JukeboxTelemetryU32 28
    next_wraps = Read-JukeboxTelemetryU32 32
    text_toggles = Read-JukeboxTelemetryU32 36
    text_map_commits = Read-JukeboxTelemetryU32 40
    palette_commits = Read-JukeboxTelemetryU32 44
    max_hardware_oam = Read-JukeboxTelemetryU32 48
    exits = Read-JukeboxTelemetryU32 52
    music_active = Read-JukeboxTelemetryU32 56
    last_jukebox_song = Read-JukeboxTelemetryU32 60
    hide_text = Read-JukeboxTelemetryU32 64
    quitting = Read-JukeboxTelemetryU32 68
    fade_level = Read-JukeboxTelemetryU32 72
    module_count = Read-JukeboxTelemetryU32 76
}
$jukeboxChecks = [ordered]@{
    rom_reported_pass = $jukeboxTelemetry.pass -eq 1
    all_source_tracks_embedded = $jukeboxTelemetry.module_count -eq 41
    circular_previous_wrap = (
        $jukeboxTelemetry.previous_wraps -eq 1 -and
        $jukeboxTelemetry.track_changes -eq 3
    )
    circular_next_wrap = $jukeboxTelemetry.next_wraps -eq 1
    source_track_identity = $jukeboxTelemetry.last_jukebox_song -eq 1
    text_toggle_and_map_updates = (
        $jukeboxTelemetry.text_toggles -eq 2 -and
        $jukeboxTelemetry.text_map_commits -ge 5 -and
        $jukeboxTelemetry.hide_text -eq 0
    )
    live_palette_animation = $jukeboxTelemetry.palette_commits -ge 30
    projected_star_oam_budget = (
        $jukeboxTelemetry.max_hardware_oam -ge 80 -and
        $jukeboxTelemetry.max_hardware_oam -le 128
    )
    fade_exit_completed = (
        $jukeboxTelemetry.display_frames -ge 176 -and
        $jukeboxTelemetry.exits -eq 1 -and
        $jukeboxTelemetry.quitting -eq 1 -and
        $jukeboxTelemetry.fade_level -eq 0
    )
    return_to_title_music = (
        $jukeboxTelemetry.selected_song -eq 29 -and
        $jukeboxTelemetry.final_state -eq 0 -and
        $jukeboxTelemetry.final_mode4 -eq 1 -and
        $jukeboxTelemetry.music_active -eq 1
    )
}
$failedJukeboxChecks = @(
    $jukeboxChecks.GetEnumerator() |
        Where-Object { -not $_.Value } |
        ForEach-Object { $_.Key }
)
if ($failedJukeboxChecks.Count -ne 0) {
    throw (
        "Jukebox auto-test failed invariant(s): " +
        ($failedJukeboxChecks -join ", ")
    )
}

if (Test-Path -LiteralPath $matrixTestSave) {
    Remove-Item -LiteralPath $matrixTestSave -Force
}
$matrixTestElapsed = Start-TestProcess `
    -FilePath $headless `
    -Arguments @("-S", "3", "$matrixTestName.gba") `
    -WorkingDirectory $buildDir `
    -StandardOutput $matrixTestStdout `
    -StandardError $matrixTestStderr
$matrixRuntimeErrors = @(
    Select-String `
        -Path $matrixTestStdout, $matrixTestStderr `
        -Pattern "Bad memory|Invalid|Illegal|Hard crash|Fatal|Failed|Error"
)
if ($matrixRuntimeErrors.Count -ne 0) {
    throw (
        "mGBA ROMFS matrix reported " +
        "$($matrixRuntimeErrors.Count) runtime error(s)"
    )
}
if (-not (Test-Path -LiteralPath $matrixTestSave)) {
    throw "ROMFS matrix did not create SRAM telemetry"
}
$matrixSaveBytes = [IO.File]::ReadAllBytes($matrixTestSave)
if (
    $matrixSaveBytes.Length -lt 6024 -or
    [Text.Encoding]::ASCII.GetString($matrixSaveBytes, 0, 4) -ne "TGLM"
) {
    throw "ROMFS matrix SRAM telemetry is invalid"
}
function Read-MatrixTelemetryU32 {
    param([int]$Offset)
    return [BitConverter]::ToUInt32($matrixSaveBytes, $Offset)
}
$matrixTelemetry = [ordered]@{
    schema = $matrixSaveBytes[4]
    pass = $matrixSaveBytes[5]
    total_sections = Read-MatrixTelemetryU32 8
    passed_sections = Read-MatrixTelemetryU32 12
    failed_sections = Read-MatrixTelemetryU32 16
    total_events = Read-MatrixTelemetryU32 20
    total_enemy_pool_entries = Read-MatrixTelemetryU32 24
    unknown_events = Read-MatrixTelemetryU32 28
    route_checks = Read-MatrixTelemetryU32 32
    route_failures = Read-MatrixTelemetryU32 36
    romfs_failures = Read-MatrixTelemetryU32 40
    first_failure = Read-MatrixTelemetryU32 44
    background_approximations = Read-MatrixTelemetryU32 64
    shape_banks = Read-MatrixTelemetryU32 68
    sprites = Read-MatrixTelemetryU32 72
    enemies = Read-MatrixTelemetryU32 76
    weapons = Read-MatrixTelemetryU32 80
    detail_level = Read-MatrixTelemetryU32 96
    game_speed = Read-MatrixTelemetryU32 100
    sprite2_l2_frames = Read-MatrixTelemetryU32 6000
    sprite2_l2_pixels = Read-MatrixTelemetryU32 6004
    sprite2_l2_filter_frames = Read-MatrixTelemetryU32 6008
    sprite2_raw_catalog_valid = Read-MatrixTelemetryU32 6012
    sprite2_raw_bytes = Read-MatrixTelemetryU32 6016
    sprite2_raw_crc32 = Read-MatrixTelemetryU32 6020
}
$matrixChecks = [ordered]@{
    schema = $matrixTelemetry.schema -eq 2
    rom_reported_pass = $matrixTelemetry.pass -eq 1
    every_lvl_section = (
        $matrixTelemetry.total_sections -eq 62 -and
        $matrixTelemetry.passed_sections -eq 62 -and
        $matrixTelemetry.failed_sections -eq 0
    )
    complete_event_scan = (
        $matrixTelemetry.total_events -eq 53338 -and
        $matrixTelemetry.total_enemy_pool_entries -eq 459 -and
        $matrixTelemetry.unknown_events -eq 39
    )
    every_route = (
        $matrixTelemetry.route_checks -eq 24 -and
        $matrixTelemetry.route_failures -eq 0
    )
    no_data_failure = (
        $matrixTelemetry.romfs_failures -eq 0 -and
        $matrixTelemetry.first_failure -eq 0 -and
        $matrixTelemetry.background_approximations -eq 0
    )
    source_catalog = (
        $matrixTelemetry.shape_banks -eq 35 -and
        $matrixTelemetry.sprites -eq 6097 -and
        $matrixTelemetry.enemies -eq 818 -and
        $matrixTelemetry.weapons -eq 52
    )
    sprite2_runtime_pixel_parity = (
        $matrixTelemetry.sprite2_l2_frames -eq
            $matrixTelemetry.sprites + 1 -and
        $matrixTelemetry.sprite2_l2_pixels -eq 6146816 -and
        $matrixTelemetry.sprite2_l2_filter_frames -eq 1
    )
    sprite2_raw_catalog = (
        $matrixTelemetry.sprite2_raw_catalog_valid -eq 1 -and
        $matrixTelemetry.sprite2_raw_bytes -eq $sprite2RawBytes -and
        $matrixTelemetry.sprite2_raw_crc32 -eq $sprite2RawCrc32
    )
    requested_configuration = (
        $matrixTelemetry.detail_level -eq $expectedDetailLevel -and
        $matrixTelemetry.game_speed -eq $expectedGameSpeed
    )
}
$failedMatrixChecks = @(
    $matrixChecks.GetEnumerator() |
        Where-Object { -not $_.Value } |
        ForEach-Object { $_.Key }
)
if ($failedMatrixChecks.Count -ne 0) {
    throw (
        "ROMFS matrix failed invariant(s): " +
        ($failedMatrixChecks -join ", ")
    )
}

if (Test-Path -LiteralPath $campaignTestSave) {
    Remove-Item -LiteralPath $campaignTestSave -Force
}
$campaignTestElapsed = Start-TestProcess `
    -FilePath $headless `
    -Arguments @("-S", "3", "$campaignTestName.gba") `
    -WorkingDirectory $buildDir `
    -StandardOutput $campaignTestStdout `
    -StandardError $campaignTestStderr `
    -TimeoutMilliseconds 60000
$campaignRuntimeErrors = @(
    Select-String `
        -Path $campaignTestStdout, $campaignTestStderr `
        -Pattern "Bad memory|Invalid|Illegal|Hard crash|Fatal|Failed|Error"
)
if ($campaignRuntimeErrors.Count -ne 0) {
    throw (
        "mGBA campaign smoke reported " +
        "$($campaignRuntimeErrors.Count) runtime error(s)"
    )
}
if (-not (Test-Path -LiteralPath $campaignTestSave)) {
    throw "Campaign smoke did not create SRAM telemetry"
}
$campaignSaveBytes = [IO.File]::ReadAllBytes($campaignTestSave)
if (
    $campaignSaveBytes.Length -lt 6288 -or
    [Text.Encoding]::ASCII.GetString($campaignSaveBytes, 0, 4) -ne "TGCM"
) {
    throw "Campaign smoke SRAM telemetry is invalid"
}
function Read-CampaignTelemetryU32 {
    param([int]$Offset)
    return [BitConverter]::ToUInt32($campaignSaveBytes, $Offset)
}
$campaignTelemetry = [ordered]@{
    schema = $campaignSaveBytes[4]
    pass = $campaignSaveBytes[5]
    final_state = $campaignSaveBytes[6]
    expected_levels = Read-CampaignTelemetryU32 6096
    completed_levels = Read-CampaignTelemetryU32 6100
    failures = Read-CampaignTelemetryU32 6104
    route_checksum = Read-CampaignTelemetryU32 6108
}
$expectedCampaignRoutes = @(
    @(1, 3, 9, 17),
    @(1, 5, 1, 0),
    @(1, 29, 8, 32),
    @(1, 25, 10, 17)
)
$campaignRouteValid = $true
for ($recordIndex = 0; $recordIndex -lt 4; $recordIndex++) {
    $offset = 6144 + $recordIndex * 36
    $expectedRoute = $expectedCampaignRoutes[$recordIndex]
    if (
        (Read-CampaignTelemetryU32 $offset) -ne $expectedRoute[0] -or
        (Read-CampaignTelemetryU32 ($offset + 4)) -ne $expectedRoute[1] -or
        (Read-CampaignTelemetryU32 ($offset + 8)) -ne $expectedRoute[2] -or
        (Read-CampaignTelemetryU32 ($offset + 12)) -ne $expectedRoute[3] -or
        (Read-CampaignTelemetryU32 ($offset + 16)) -eq 0 -or
        (Read-CampaignTelemetryU32 ($offset + 20)) -eq 0 -or
        (Read-CampaignTelemetryU32 ($offset + 32)) -ne 0
    ) {
        $campaignRouteValid = $false
    }
}
$campaignChecks = [ordered]@{
    schema = $campaignTelemetry.schema -eq 3
    rom_reported_pass = $campaignTelemetry.pass -eq 1
    returned_to_game_menu = $campaignTelemetry.final_state -eq 7
    four_levels_completed = (
        $campaignTelemetry.expected_levels -eq 4 -and
        $campaignTelemetry.completed_levels -eq 4 -and
        $campaignTelemetry.failures -eq 0
    )
    source_route_sequence = $campaignRouteValid
    route_checksum = (
        $campaignTelemetry.route_checksum -eq
            [Convert]::ToUInt32("EAEB0109", 16)
    )
}
$failedCampaignChecks = @(
    $campaignChecks.GetEnumerator() |
        Where-Object { -not $_.Value } |
        ForEach-Object { $_.Key }
)
if ($failedCampaignChecks.Count -ne 0) {
    throw (
        "Campaign smoke failed invariant(s): " +
        ($failedCampaignChecks -join ", ")
    )
}

if (Test-Path -LiteralPath $episode2TestSave) {
    Remove-Item -LiteralPath $episode2TestSave -Force
}
$episode2TestElapsed = Start-TestProcess `
    -FilePath $headless `
    -Arguments @("-S", "3", "$episode2TestName.gba") `
    -WorkingDirectory $buildDir `
    -StandardOutput $episode2TestStdout `
    -StandardError $episode2TestStderr `
    -TimeoutMilliseconds 60000
$episode2RuntimeErrors = @(
    Select-String `
        -Path $episode2TestStdout, $episode2TestStderr `
        -Pattern "Bad memory|Invalid|Illegal|Hard crash|Fatal|Failed|Error"
)
if ($episode2RuntimeErrors.Count -ne 0) {
    throw (
        "mGBA Episode 2 smoke reported " +
        "$($episode2RuntimeErrors.Count) runtime error(s)"
    )
}
if (-not (Test-Path -LiteralPath $episode2TestSave)) {
    throw "Episode 2 smoke did not create SRAM telemetry"
}
$episode2SaveBytes = [IO.File]::ReadAllBytes($episode2TestSave)
if (
    $episode2SaveBytes.Length -lt 6312 -or
    [Text.Encoding]::ASCII.GetString(
        $episode2SaveBytes,
        0,
        4
    ) -ne "TGRS"
) {
    throw "Episode 2 smoke SRAM telemetry is invalid"
}
function Read-Episode2TelemetryU32 {
    param([int]$Offset)
    return [BitConverter]::ToUInt32($episode2SaveBytes, $Offset)
}
$episode2Telemetry = [ordered]@{
    schema = $episode2SaveBytes[4]
    pass = $episode2SaveBytes[5]
    final_state = $episode2SaveBytes[6]
    music_active = $episode2SaveBytes[7]
    logic_updates = Read-Episode2TelemetryU32 8
    display_frames = Read-Episode2TelemetryU32 12
    vblank_irqs = Read-Episode2TelemetryU32 16
    missed_vblanks = Read-Episode2TelemetryU32 20
    collisions = Read-Episode2TelemetryU32 32
    streamed_map_rows = Read-Episode2TelemetryU32 36
    max_active_enemies = Read-Episode2TelemetryU32 40
    stream_drops = Read-Episode2TelemetryU32 48
    event_index = Read-Episode2TelemetryU32 52
    final_level_position = Read-Episode2TelemetryU32 104
    source_unknown_visuals = Read-Episode2TelemetryU32 296
    sprite2_decode_failures = Read-Episode2TelemetryU32 356
    sprite2_cache_hits = Read-Episode2TelemetryU32 360
    sprite2_cache_misses = Read-Episode2TelemetryU32 364
    sprite2_cache_evictions = Read-Episode2TelemetryU32 368
    sprite2_cache_drops = Read-Episode2TelemetryU32 372
    sprite2_uploads = Read-Episode2TelemetryU32 376
    configured_detail_level = Read-Episode2TelemetryU32 600
    configured_game_speed = Read-Episode2TelemetryU32 604
    background_approximations = Read-Episode2TelemetryU32 648
    background_layer0_valid = Read-Episode2TelemetryU32 660
    background_layer1_valid = Read-Episode2TelemetryU32 664
    background_layer2_valid = Read-Episode2TelemetryU32 668
    background_prefetch_late_columns = Read-Episode2TelemetryU32 684
    route_episode = Read-Episode2TelemetryU32 720
    route_section = Read-Episode2TelemetryU32 724
    lvl_file_number = Read-Episode2TelemetryU32 728
    source_song = Read-Episode2TelemetryU32 732
    event_count = Read-Episode2TelemetryU32 736
    first_decode_failure = Read-Episode2TelemetryU32 6000
    combat_assists = Read-Episode2TelemetryU32 6028
    sprite2_l2_hits = Read-Episode2TelemetryU32 6244
    sprite2_l2_misses = Read-Episode2TelemetryU32 6248
    sprite2_l2_evictions = Read-Episode2TelemetryU32 6252
    sprite2_l2_drops = Read-Episode2TelemetryU32 6256
    sprite2_l2_raw_builds = Read-Episode2TelemetryU32 6264
    sprite2_l2_rle_fallbacks = Read-Episode2TelemetryU32 6268
}
$expectedEpisode2DisplayFrames = if ($GameSpeed -eq "low") {
    13079
} else {
    10475
}
$expectedEpisode2Sprite2Hits = if ($GameSpeed -eq "low") {
    $null
} else {
    53541
}
$episode2Checks = [ordered]@{
    schema = $episode2Telemetry.schema -eq 3
    rom_reported_pass = $episode2Telemetry.pass -eq 1
    returned_to_game_menu = (
        $episode2Telemetry.final_state -eq 7 -and
        $episode2Telemetry.music_active -eq 1
    )
    authored_route = (
        $episode2Telemetry.route_episode -eq 2 -and
        $episode2Telemetry.route_section -eq 1 -and
        $episode2Telemetry.lvl_file_number -eq 1 -and
        $episode2Telemetry.source_song -eq 27 -and
        $episode2Telemetry.event_count -eq 1752
    )
    authored_completion = (
        $episode2Telemetry.logic_updates -eq 6065 -and
        $episode2Telemetry.display_frames -eq
            $expectedEpisode2DisplayFrames -and
        $episode2Telemetry.event_index -eq 1484 -and
        $episode2Telemetry.final_level_position -eq 6632
    )
    source_workload = (
        $episode2Telemetry.collisions -eq 1795 -and
        $episode2Telemetry.streamed_map_rows -eq
            $(if ($DetailLevel -eq "low") { 2487 } else { 4145 }) -and
        $episode2Telemetry.max_active_enemies -eq 38
    )
    sprite2_l1_accounting = (
        (
            (
                $GameSpeed -eq "low" -and
                $episode2Telemetry.sprite2_cache_hits -gt 53541
            ) -or
            $episode2Telemetry.sprite2_cache_hits -eq
                $expectedEpisode2Sprite2Hits
        ) -and
        $episode2Telemetry.sprite2_cache_misses -eq 3006 -and
        $episode2Telemetry.sprite2_cache_evictions -eq 2982 -and
        $episode2Telemetry.sprite2_cache_drops -eq 0 -and
        $episode2Telemetry.sprite2_uploads -eq 3006
    )
    sprite2_l2_accounting = (
        $episode2Telemetry.sprite2_l2_hits -eq 2536 -and
        $episode2Telemetry.sprite2_l2_misses -eq 477 -and
        $episode2Telemetry.sprite2_l2_evictions -eq 413 -and
        $episode2Telemetry.sprite2_l2_drops -eq 0 -and
        $episode2Telemetry.sprite2_l2_raw_builds -eq 477 -and
        $episode2Telemetry.sprite2_l2_rle_fallbacks -eq 0
    )
    no_asset_or_stream_failure = (
        $episode2Telemetry.stream_drops -eq 0 -and
        $episode2Telemetry.source_unknown_visuals -eq 0 -and
        $episode2Telemetry.sprite2_decode_failures -eq 0 -and
        $episode2Telemetry.first_decode_failure -eq 0 -and
        $episode2Telemetry.combat_assists -eq 0
    )
    requested_configuration = (
        $episode2Telemetry.configured_detail_level -eq
            $expectedDetailLevel -and
        $episode2Telemetry.configured_game_speed -eq
            $expectedGameSpeed
    )
    background_working_set_budget = (
        $episode2Telemetry.background_approximations -eq 0
    )
    background_partition = (
        $episode2Telemetry.background_layer0_valid -eq 576 -and
        $episode2Telemetry.background_layer1_valid -eq
            $(if ($DetailLevel -eq "low") { 0 } else { 1 }) -and
        $episode2Telemetry.background_layer2_valid -eq 1
    )
    full_level_vblank_budget = (
        # The complete source sound workload and five-shot Pulse-Cannon may
        # consume at most 0.25 percent of the full Episode 2 display frames.
        $episode2Telemetry.missed_vblanks * 400 -le
            $episode2Telemetry.display_frames
    )
}
$failedEpisode2Checks = @(
    $episode2Checks.GetEnumerator() |
        Where-Object { -not $_.Value } |
        ForEach-Object { $_.Key }
)
if ($failedEpisode2Checks.Count -ne 0) {
    throw (
        "Episode 2 smoke failed invariant(s): " +
        ($failedEpisode2Checks -join ", ")
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
foreach (
    $info in @(
        $releaseInfo,
        $testInfo,
        $deathTestInfo,
        $jukeboxTestInfo,
        $matrixTestInfo,
        $campaignTestInfo,
        $episode2TestInfo
    )
) {
    foreach ($entry in $info.GetEnumerator()) {
        $verification.Add("$($info.name)_$($entry.Key)=$($entry.Value)")
    }
}
foreach ($memory in $memoryInfos) {
    foreach ($entry in $memory.GetEnumerator()) {
        $verification.Add(
            "memory_$($memory.name)_$($entry.Key)=$($entry.Value)"
        )
    }
}
$verification.Add("soundbank_bytes=$soundbankBytes")
$verification.Add("sprite2_raw_bytes=$sprite2RawBytes")
$verification.Add("sprite2_raw_sha256=$sprite2RawSha256")
foreach ($entry in $sprite2RawAudit.GetEnumerator()) {
    $verification.Add("sprite2_raw_audit_$($entry.Key)=$($entry.Value)")
}
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
$verification.Add("death_autotest_host_elapsed_ms=$deathTestElapsed")
$verification.Add(
    "death_autotest_runtime_error_count=$($deathRuntimeErrors.Count)"
)
foreach ($entry in $deathTelemetry.GetEnumerator()) {
    $verification.Add("death_telemetry_$($entry.Key)=$($entry.Value)")
}
$verification.Add("jukebox_autotest_host_elapsed_ms=$jukeboxTestElapsed")
$verification.Add(
    "jukebox_autotest_runtime_error_count=$($jukeboxRuntimeErrors.Count)"
)
foreach ($entry in $jukeboxTelemetry.GetEnumerator()) {
    $verification.Add("jukebox_telemetry_$($entry.Key)=$($entry.Value)")
}
$verification.Add("matrix_autotest_host_elapsed_ms=$matrixTestElapsed")
$verification.Add(
    "matrix_autotest_runtime_error_count=$($matrixRuntimeErrors.Count)"
)
foreach ($entry in $matrixTelemetry.GetEnumerator()) {
    $verification.Add("matrix_telemetry_$($entry.Key)=$($entry.Value)")
}
$verification.Add("campaign_autotest_host_elapsed_ms=$campaignTestElapsed")
$verification.Add(
    "campaign_autotest_runtime_error_count=$($campaignRuntimeErrors.Count)"
)
foreach ($entry in $campaignTelemetry.GetEnumerator()) {
    $verification.Add("campaign_telemetry_$($entry.Key)=$($entry.Value)")
}
$verification.Add("episode2_autotest_host_elapsed_ms=$episode2TestElapsed")
$verification.Add(
    "episode2_autotest_runtime_error_count=$($episode2RuntimeErrors.Count)"
)
foreach ($entry in $episode2Telemetry.GetEnumerator()) {
    $verification.Add("episode2_telemetry_$($entry.Key)=$($entry.Value)")
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
