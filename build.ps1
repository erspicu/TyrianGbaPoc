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
$releaseName = "tyrian_gba_level1_source_parity_romfs_v14"
$testName = "tyrian_gba_level1_source_parity_autotest_romfs_v14"
$releaseRom = Join-Path $buildDir "$releaseName.gba"
$testRom = Join-Path $buildDir "$testName.gba"
$testSave = Join-Path $buildDir "$testName.sav"
$testStdout = Join-Path $buildDir "autotest_mgba_stdout.txt"
$testStderr = Join-Path $buildDir "autotest_mgba_stderr.txt"
$perfStdout = Join-Path $buildDir "release_boot_perf.csv"
$perfStderr = Join-Path $buildDir "release_boot_perf.stderr.txt"
$verificationPath = Join-Path $buildDir "verification.txt"
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
if ($saveBytes.Length -lt 200) {
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
    final_event_offset = Read-TelemetryU32 52
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
}

$telemetryChecks = @(
    $telemetry.version -eq 9,
    $telemetry.pass -eq 1,
    $telemetry.final_state -eq 0,
    $telemetry.title_music_active -eq 1,
    $telemetry.logic_updates -ge 5400,
    $telemetry.final_level_position -ge 5400,
    $telemetry.spawn_events -eq 414,
    $telemetry.control_events -eq 380,
    $telemetry.missed_vblanks -eq 0,
    $telemetry.stream_drops -eq 0,
    $telemetry.effect_drops -eq 0,
    $telemetry.reward_spawns -gt 0,
    $telemetry.reward_pickups -gt 0,
    $telemetry.reward_drops -eq 0,
    $telemetry.enemy_shots_spawned -gt 0,
    $telemetry.enemy_shot_drops -eq 0,
    $telemetry.enemy_pool_replacements -eq 0,
    $telemetry.direct_kill_cash -gt 0,
    $telemetry.final_cash -ge $telemetry.direct_kill_cash,
    $telemetry.reward_control_events -eq 33,
    $telemetry.reward_assignments -gt 0,
    $telemetry.pause_toggles -eq 2,
    $telemetry.paused_display_frames -ge 60,
    $telemetry.source_parity_events -eq 878,
    $telemetry.source_parity_events_applied -eq 869,
    $telemetry.source_parity_events_deferred -eq 5,
    $telemetry.source_parity_events_skipped -eq 4,
    (
        $telemetry.source_parity_events_applied +
        $telemetry.source_parity_events_deferred +
        $telemetry.source_parity_events_skipped
    ) -eq $telemetry.source_parity_events,
    $telemetry.source_parity_spawn_attempts -eq 473,
    (
        $telemetry.source_parity_spawn_successes +
        $telemetry.source_parity_spawn_pool_full +
        $telemetry.source_parity_spawn_missing
    ) -eq $telemetry.source_parity_spawn_attempts,
    $telemetry.source_parity_spawn_successes -eq 100,
    $telemetry.source_parity_spawn_pool_full -eq 373,
    $telemetry.source_parity_spawn_missing -eq 0,
    $telemetry.source_parity_max_enemies -eq 100,
    $telemetry.source_parity_control_writes -eq 3586,
    $telemetry.source_parity_rng_calls -eq 30,
    $telemetry.romfs_entries -eq $romfsAudit.entry_count,
    $telemetry.romfs_image_bytes -eq $romfsAudit.image_bytes,
    $telemetry.romfs_payload_bytes -eq $romfsAudit.payload_bytes,
    $telemetry.romfs_self_test_checks -eq $romfsExpectedSelfTestChecks,
    $telemetry.romfs_self_test_failures -eq 0,
    $telemetry.romfs_manifest_crc32 -eq $romfsManifestCrc32,
    $telemetry.max_active_enemy_shots -le 60,
    $telemetry.max_hardware_oam -le 128,
    $telemetry.state_transitions -eq 5
)
if ($telemetryChecks -contains $false) {
    throw "Auto-test SRAM contains a failed invariant"
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
$verification | Set-Content -LiteralPath $verificationPath -Encoding utf8
$verification
