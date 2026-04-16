[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [string]$Map = "crossfire",
    [int]$Port = 27015,
    [int]$MaxPlayers = 4,
    [string[]]$SetCvar = @(),
    [hashtable]$Cvars,
    [switch]$EnableExperimentalGlock,
    [switch]$EnableExperimentalGlockDebug,
    [switch]$Detached,
    [switch]$PassThru,
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
$runtimeRoot = Get-TestbedRuntimeRoot
$runtimeHldsExe = Join-Path $runtimeRoot "hlds.exe"
$runtimeDll = Join-Path $runtimeRoot "valve\dlls\hl.dll"

if ((-not (Test-LeafPath -Path $runtimeHldsExe)) -or (-not (Test-LeafPath -Path $runtimeDll))) {
    & "$PSScriptRoot\install-testbed.ps1" -Configuration $configuration -TemplateRoot $TemplateRoot -HldsExe $HldsExe -HlExe $HlExe -SteamCmdExe $SteamCmdExe -AllowSteamCmdDownload:$AllowSteamCmdDownload
}

Assert-UdpPortAvailable -Port $Port
$effectiveSetCvars = @()

if ($EnableExperimentalGlock -or $EnableExperimentalGlockDebug) {
    $effectiveSetCvars += @(Get-ExperimentalGlockLaunchAssignments)
}

if ($EnableExperimentalGlockDebug) {
    $effectiveSetCvars += @(Get-ExperimentalGlockDebugLaunchAssignments)
}

$effectiveSetCvars += @($SetCvar)

if ($Detached) {
    Write-Step "Launching HLDS in detached mode"
    $launchInfo = Start-HldsDetached -RuntimeRoot $runtimeRoot -Map $Map -Port $Port -MaxPlayers $MaxPlayers -SetCvar $effectiveSetCvars -Cvars $Cvars
    if ($PassThru) {
        return $launchInfo
    }

    Write-Host "PID: $($launchInfo.Process.Id)"
    Write-Host "stdout log: $($launchInfo.StdOutLog)"
    Write-Host "stderr log: $($launchInfo.StdErrLog)"
}
else {
    Write-Step "Launching HLDS in the foreground"
    $logPath = Invoke-HldsForeground -RuntimeRoot $runtimeRoot -Map $Map -Port $Port -MaxPlayers $MaxPlayers -SetCvar $effectiveSetCvars -Cvars $Cvars
    Write-Host "Log: $logPath"
}
