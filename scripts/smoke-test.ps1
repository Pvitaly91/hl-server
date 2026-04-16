[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [string]$Map = "crossfire",
    [int]$Port = 27015,
    [int]$MaxPlayers = 4,
    [string[]]$SetCvar = @(),
    [switch]$EnableExperimentalGlock,
    [switch]$EnableExperimentalMp5,
    [switch]$EnableGlockLabDummy,
    [switch]$EnableMp5LabLoadout,
    [string]$TemplateRoot,
    [string]$HldsExe,
    [string]$HlExe,
    [string]$SteamCmdExe,
    [switch]$AllowSteamCmdDownload
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"
Import-HLServerEnv

$configuration = Get-ValidatedConfiguration -Configuration $Configuration
$resolvedPort = Get-AvailableUdpPort -PreferredPort $Port

$effectiveSetCvars = @()
if ($EnableExperimentalGlock) {
    $effectiveSetCvars += @(Get-ExperimentalGlockLaunchAssignments)
}
if ($EnableExperimentalMp5) {
    $effectiveSetCvars += @(Get-ExperimentalMp5LaunchAssignments)
}
if ($EnableGlockLabDummy) {
    $effectiveSetCvars += @(Get-GlockLabDummyLaunchAssignments)
}
if ($EnableMp5LabLoadout) {
    $effectiveSetCvars += @(Get-Mp5LabLoadoutLaunchAssignments)
}
$effectiveSetCvars += @($SetCvar)

$expectedCvars = [ordered]@{
    "sv_exp_pistol_tapfire" = "0"
    "sv_exp_move_spread_scale" = "0.0"
    "sv_exp_first_shot_accuracy" = "0"
    "sv_exp_spread_recovery" = "0.0"
    "sv_exp_weapon_under_test" = ""
    "sv_exp_debug_weaponlog" = "0"
    "sv_exp_debug_weaponlog_rejections" = "0"
    "sv_exp_session_tag" = ""
    "sv_exp_matrix_name" = ""
    "sv_exp_matrix_step" = ""
    "sv_exp_mp5_primary_enabled" = "0"
    "sv_exp_mp5_profile_name" = "default"
    "sv_exp_mp5_primary_base_spread" = "0.0523"
    "sv_exp_mp5_primary_ground_move_penalty" = "0.0200"
    "sv_exp_mp5_primary_air_move_penalty" = "0.0400"
    "sv_exp_mp5_primary_duck_penalty_scale" = "0.7000"
    "sv_exp_mp5_primary_burst_growth" = "0.0060"
    "sv_exp_mp5_primary_burst_max_additional_spread" = "0.0600"
    "sv_exp_mp5_primary_spread_recovery" = "0.3000"
    "sv_exp_mp5_primary_damage" = "12.0"
    "sv_exp_mp5_primary_headshot_scale" = "3.0"
    "sv_exp_mp5_primary_headshot_lethal" = "0"
    "sv_exp_mp5_primary_first_shot_accuracy" = "0"
    "sv_exp_mp5_primary_first_shot_speed_threshold" = "30.0"
    "sv_exp_mp5_primary_max_spread" = "0.1200"
    "sv_exp_mp5_lab_loadout" = "0"
    "sv_exp_mp5_lab_ammo" = "250"
    "sv_exp_mp5_lab_autoswitch" = "1"
    "sv_exp_glock_lab_dummy" = "0"
    "sv_exp_glock_lab_target_profile_name" = "default"
    "sv_exp_glock_lab_dummy_health" = "100.0"
    "sv_exp_glock_lab_dummy_armor" = "0.0"
    "sv_exp_glock_lab_dummy_head_protected" = "0"
    "sv_exp_glock_lab_dummy_armor_health_fraction" = "0.5"
    "sv_exp_glock_lab_dummy_armor_drain_scale" = "1.0"
    "sv_exp_glock_lab_dummy_autorespawn" = "1"
    "sv_exp_glock_lab_dummy_respawn_delay" = "1.0"
    "sv_exp_glock_lab_dummy_spawn_distance" = "256.0"
    "sv_exp_glock_lab_dummy_model" = "models/barney.mdl"
    "sv_exp_glock_lab_dummy_face_player" = "1"
    "sv_exp_glock_lab_dummy_offset_right" = "0.0"
    "sv_exp_glock_lab_dummy_offset_up" = "0.0"
}

foreach ($assignment in $effectiveSetCvars) {
    $separatorIndex = $assignment.IndexOf("=")
    if ($separatorIndex -lt 1 -or $separatorIndex -ge ($assignment.Length - 1)) {
        throw "Invalid -SetCvar value '$assignment'. Use Name=Value."
    }

    $expectedCvars[$assignment.Substring(0, $separatorIndex).Trim()] = $assignment.Substring($separatorIndex + 1).Trim()
}

Write-Step "Building $configuration for smoke test"
& "$PSScriptRoot\build.ps1" -Configuration $configuration

Write-Step "Installing disposable runtime for smoke test"
& "$PSScriptRoot\install-testbed.ps1" -Configuration $configuration -TemplateRoot $TemplateRoot -HldsExe $HldsExe -HlExe $HlExe -SteamCmdExe $SteamCmdExe -AllowSteamCmdDownload:$AllowSteamCmdDownload

$runtimeDll = Join-RepoPath "testbed\runtime\valve\dlls\hl.dll"
if (-not (Test-LeafPath -Path $runtimeDll)) {
    throw "Smoke test expected installed runtime DLL at $runtimeDll."
}

$doctorReport = Get-TestbedDoctorReport -Configuration $configuration -ExplicitTemplateRoot $TemplateRoot -ExplicitHldsExe $HldsExe -ExplicitHlExe $HlExe -ExplicitSteamCmdExe $SteamCmdExe -AllowSteamCmdDownload:$AllowSteamCmdDownload -IgnoreLatestLaunchFailure
Write-TestbedDoctorSummary -Report $doctorReport
if (-not $doctorReport.IsHealthy) {
    throw (Get-TestbedDoctorFailureMessage -Report $doctorReport)
}

$launchInfo = $null

try {
    Write-Step "Launching HLDS for smoke test"
    if ($resolvedPort -ne $Port) {
        Write-Step "Requested port $Port was busy, using free UDP port $resolvedPort instead"
    }

    $launchInfo = Start-HldsDetached -RuntimeRoot (Join-RepoPath "testbed\runtime") -Map $Map -Port $resolvedPort -MaxPlayers $MaxPlayers -SetCvar $effectiveSetCvars

    $startupObserved = Wait-ForHldsReady -LaunchInfo $launchInfo -Map $Map -TimeoutSeconds 30
    if (-not $startupObserved) {
        throw "Smoke test did not observe server startup in the launch logs."
    }

    foreach ($name in $expectedCvars.Keys) {
        $expectedPattern = 'Server cvar "{0}" = "{1}"' -f $name, $expectedCvars[$name]
        $observed = Wait-ForLogPattern -Paths (Get-HldsLogCandidates -LaunchInfo $launchInfo) -Pattern $expectedPattern -TimeoutSeconds 30 -Process $launchInfo.Process
        if (-not $observed) {
            throw "Smoke test did not observe '$expectedPattern' in the server logs."
        }
    }

    Write-Step "Smoke test passed"
    foreach ($name in $expectedCvars.Keys) {
        Write-Host ('Observed: Server cvar "{0}" = "{1}"' -f $name, $expectedCvars[$name])
    }
    Write-Host "stdout log: $($launchInfo.StdOutLog)"
}
finally {
    if ($launchInfo -and (-not (Wait-ForHldsReady -LaunchInfo $launchInfo -Map $Map -TimeoutSeconds 1))) {
        $postFailureDoctor = Get-TestbedDoctorReport -Configuration $configuration -ExplicitTemplateRoot $TemplateRoot -ExplicitHldsExe $HldsExe -ExplicitHlExe $HlExe -ExplicitSteamCmdExe $SteamCmdExe -AllowSteamCmdDownload:$AllowSteamCmdDownload
        Write-TestbedDoctorSummary -Report $postFailureDoctor
    }

    if ($launchInfo) {
        Stop-ProcessIfRunning -Process $launchInfo.Process
    }
}
