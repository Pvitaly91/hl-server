[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [string]$Map = "crossfire",
    [int]$Port = 27015,
    [int]$MaxPlayers = 4,
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
$expectedPattern = 'Server cvar "sv_exp_pistol_tapfire" = "0"'
$resolvedPort = Get-AvailableUdpPort -PreferredPort $Port

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

    $launchInfo = Start-HldsDetached -RuntimeRoot (Join-RepoPath "testbed\runtime") -Map $Map -Port $resolvedPort -MaxPlayers $MaxPlayers

    $observed = Wait-ForLogPattern -Paths (Get-HldsLogCandidates -LaunchInfo $launchInfo) -Pattern $expectedPattern -TimeoutSeconds 30 -Process $launchInfo.Process
    if (-not $observed) {
        throw "Smoke test did not observe '$expectedPattern' in the server logs."
    }

    Write-Step "Smoke test passed"
    Write-Host "Observed: $expectedPattern"
    Write-Host "stdout log: $($launchInfo.StdOutLog)"
}
finally {
    if ($launchInfo) {
        Stop-ProcessIfRunning -Process $launchInfo.Process
    }
}
