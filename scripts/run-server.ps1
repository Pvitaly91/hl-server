[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [string]$Map = "crossfire",
    [int]$Port = 27015,
    [int]$MaxPlayers = 4,
    [string[]]$SetCvar = @(),
    [string]$GlockProfile,
    [string]$Mp5Profile,
    [string]$LabTargetProfile,
    [hashtable]$Cvars,
    [switch]$EnableExperimentalGlock,
    [switch]$EnableExperimentalMp5,
    [switch]$EnableExperimentalGlockDebug,
    [switch]$EnableExperimentalWeaponDebug,
    [switch]$EnableGlockLabDummy,
    [switch]$EnableMp5LabLoadout,
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
$preflightReport = Get-TestbedDoctorReport -Configuration $configuration -ExplicitTemplateRoot $TemplateRoot -ExplicitHldsExe $HldsExe -ExplicitHlExe $HlExe -ExplicitSteamCmdExe $SteamCmdExe -AllowSteamCmdDownload:$AllowSteamCmdDownload -IgnoreLatestLaunchFailure

if (-not $preflightReport.IsHealthy) {
    Write-Step "Refreshing disposable runtime before launch"
    & "$PSScriptRoot\install-testbed.ps1" -Configuration $configuration -TemplateRoot $TemplateRoot -HldsExe $HldsExe -HlExe $HlExe -SteamCmdExe $SteamCmdExe -AllowSteamCmdDownload:$AllowSteamCmdDownload
    $preflightReport = Get-TestbedDoctorReport -Configuration $configuration -ExplicitTemplateRoot $TemplateRoot -ExplicitHldsExe $HldsExe -ExplicitHlExe $HlExe -ExplicitSteamCmdExe $SteamCmdExe -AllowSteamCmdDownload:$AllowSteamCmdDownload -IgnoreLatestLaunchFailure
}

if (-not $preflightReport.IsHealthy) {
    Write-TestbedDoctorSummary -Report $preflightReport
    throw (Get-TestbedDoctorFailureMessage -Report $preflightReport)
}

if ($preflightReport.SourceSelection -and $preflightReport.SourceSelection.SelectedCandidate) {
    $selectedSource = $preflightReport.SourceSelection.SelectedCandidate
    Write-Host "Runtime source: $($selectedSource.Root)"
    Write-Host "Source reason : $($selectedSource.Reason)"
}

Assert-UdpPortAvailable -Port $Port
$effectiveSetCvars = @()
$useGlockProfile = -not [string]::IsNullOrWhiteSpace($GlockProfile)
$useMp5Profile = -not [string]::IsNullOrWhiteSpace($Mp5Profile)
$useLabTargetProfile = -not [string]::IsNullOrWhiteSpace($LabTargetProfile)

if ($EnableExperimentalGlock -or $EnableExperimentalGlockDebug -or $useGlockProfile) {
    $effectiveSetCvars += @(Get-ExperimentalGlockLaunchAssignments)
}

if ($EnableExperimentalMp5 -or $useMp5Profile) {
    $effectiveSetCvars += @(Get-ExperimentalMp5LaunchAssignments)
}

if ($EnableExperimentalGlockDebug -or $EnableExperimentalWeaponDebug) {
    $effectiveSetCvars += @(Get-ExperimentalWeaponDebugLaunchAssignments)
}

if ($EnableGlockLabDummy) {
    $effectiveSetCvars += @(Get-GlockLabDummyLaunchAssignments)
}

if ($EnableMp5LabLoadout) {
    $effectiveSetCvars += @(Get-Mp5LabLoadoutLaunchAssignments)
}

if ($useGlockProfile) {
    $effectiveSetCvars += @(Get-GlockProfileLaunchAssignments -Name $GlockProfile)
}

if ($useMp5Profile) {
    $effectiveSetCvars += @(Get-Mp5ProfileLaunchAssignments -Name $Mp5Profile)
}

if ($useLabTargetProfile) {
    $effectiveSetCvars += @(Get-GlockLabTargetLaunchAssignments -Name $LabTargetProfile)
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
    Write-Host "launch metadata: $($launchInfo.LaunchMetadataPath)"
    Write-Host "working directory: $runtimeRoot"
}
else {
    Write-Step "Launching HLDS in the foreground"
    $logPath = Invoke-HldsForeground -RuntimeRoot $runtimeRoot -Map $Map -Port $Port -MaxPlayers $MaxPlayers -SetCvar $effectiveSetCvars -Cvars $Cvars
    Write-Host "Log: $logPath"
}
