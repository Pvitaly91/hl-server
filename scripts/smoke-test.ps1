[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [string]$Map = "crossfire",
    [int]$Port = 27015,
    [int]$MaxPlayers = 4,
    [string[]]$SetCvar = @(),
    [switch]$EnableExperimentalGlock,
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
$effectiveSetCvars += @($SetCvar)

$expectedCvars = [ordered]@{
    "sv_exp_pistol_tapfire" = "0"
    "sv_exp_move_spread_scale" = "0.0"
    "sv_exp_first_shot_accuracy" = "0"
    "sv_exp_spread_recovery" = "0.0"
    "sv_exp_debug_weaponlog" = "0"
    "sv_exp_debug_weaponlog_rejections" = "0"
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
    if ($launchInfo) {
        Stop-ProcessIfRunning -Process $launchInfo.Process
    }
}
