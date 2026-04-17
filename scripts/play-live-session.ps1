[CmdletBinding()]
param(
    [ValidateSet("glock", "mp5")]
    [string]$Weapon,

    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [string]$Map = "crossfire",
    [int]$Port = 27015,
    [string]$Preset,
    [string]$TargetProfile,
    [string]$TemplateRoot,
    [string]$HldsExe,
    [string]$HlExe,
    [string]$SteamCmdExe,
    [switch]$AllowSteamCmdDownload,
    [switch]$NoClient,
    [switch]$NoTail,
    [switch]$AnalyzeLatestOnExit,
    [switch]$PassThru,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$ExtraSessionArgs
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"
Import-HLServerEnv

$configuration = Get-ValidatedConfiguration -Configuration $Configuration
$runtimeRoot = Get-TestbedRuntimeRoot
$logsRoot = Get-TestbedLogsRoot
$reportsRoot = Get-WeaponDebugReportsRoot

switch ($Weapon) {
    "glock" {
        $sessionScript = Join-Path $PSScriptRoot "run-glock-test-session.ps1"
        $displayName = "Glock live lab"
        $profileParameterName = "-GlockProfile"
        $defaultPreset = "cs_tight"
        $defaultTargetProfile = "vest_headprotected"
    }
    "mp5" {
        $sessionScript = Join-Path $PSScriptRoot "run-mp5-test-session.ps1"
        $displayName = "MP5 live lab"
        $profileParameterName = "-Mp5Profile"
        $defaultPreset = "cs_burst"
        $defaultTargetProfile = "vest"
    }
}

$effectivePreset = if ([string]::IsNullOrWhiteSpace($Preset)) { $defaultPreset } else { $Preset }
$effectiveTargetProfile = if ([string]::IsNullOrWhiteSpace($TargetProfile)) { $defaultTargetProfile } else { $TargetProfile }

Write-Step "Repairing and validating the disposable runtime for $displayName"
& "$PSScriptRoot\doctor-testbed.ps1" `
    -Configuration $configuration `
    -TemplateRoot $TemplateRoot `
    -HldsExe $HldsExe `
    -HlExe $HlExe `
    -SteamCmdExe $SteamCmdExe `
    -AllowSteamCmdDownload:$AllowSteamCmdDownload `
    -PreferClientMatchedRuntime `
    -Repair `
    -BuildIfMissing

$doctorReport = Get-TestbedDoctorReport `
    -Configuration $configuration `
    -ExplicitTemplateRoot $TemplateRoot `
    -ExplicitHldsExe $HldsExe `
    -ExplicitHlExe $HlExe `
    -ExplicitSteamCmdExe $SteamCmdExe `
    -AllowSteamCmdDownload:$AllowSteamCmdDownload `
    -PreferClientMatchedRuntime `
    -IgnoreLatestLaunchFailure

$resolvedClientExe = $null
if (-not $NoClient) {
    $resolvedClientExe = Get-TestbedSessionClientExe -RuntimeRoot $runtimeRoot -ExplicitHlExe $HlExe -PreferClientMatchedRuntime
}

Write-Host ""
Write-Host "$displayName launcher"
Write-Host "  connect target : 127.0.0.1:$Port (the session may pick another free local port if this one is busy)"
Write-Host "  active preset  : $effectivePreset"
Write-Host "  target profile : $effectiveTargetProfile"
Write-Host "  chosen map     : $Map"
Write-Host "  logs           : $logsRoot"
Write-Host "  analyzer output: $reportsRoot"
Write-Host "  runtime root   : $($doctorReport.LiveContentStatus.RuntimeRoot)"
Write-Host "  launch hlds    : $(if ($doctorReport.LiveContentStatus.RuntimeHldsExe) { $doctorReport.LiveContentStatus.RuntimeHldsExe } else { 'missing' })"
Write-Host "  launch hl      : $(if ($doctorReport.LiveContentStatus.ClientLaunchExe) { $doctorReport.LiveContentStatus.ClientLaunchExe } else { 'not found' })"
Write-Host "  game dir       : $($doctorReport.LiveContentStatus.GameDirName)"
Write-Host "  client root    : $(if ($doctorReport.LiveContentStatus.ClientRoot) { $doctorReport.LiveContentStatus.ClientRoot } else { 'not found' })"
Write-Host "  runtime source : $(if ($doctorReport.LiveContentStatus.RuntimeSourceRoot) { $doctorReport.LiveContentStatus.RuntimeSourceRoot } else { 'unavailable' })"
Write-Host "  content source : $(if ($doctorReport.LiveContentStatus.EffectiveContentRoot) { $doctorReport.LiveContentStatus.EffectiveContentRoot } else { 'unavailable' })"
if ($doctorReport.LiveModState) {
    Write-Host "  live mod root  : $($doctorReport.LiveModState.LinkPath)"
    Write-Host "  live mod stage : $($doctorReport.LiveModState.StageRoot)"
}
Write-Host "  same-root      : $($doctorReport.LiveContentStatus.SameRootLaunchLabel)"
Write-Host "  content_match  : $($doctorReport.LiveContentStatus.ContentMatchLabel)"
Write-Host "  diagnosis kind : $($doctorReport.LiveContentStatus.DiagnosisKind)"
Write-Host "  live verdict   : $($doctorReport.LiveContentStatus.Verdict)"
Write-Host "  runtime map    : $(if ($doctorReport.LiveContentStatus.RuntimeMap.Path) { $doctorReport.LiveContentStatus.RuntimeMap.Path } else { 'missing' })"
Write-Host "  client map     : $(if ($doctorReport.LiveContentStatus.ClientMap.Path) { $doctorReport.LiveContentStatus.ClientMap.Path } else { 'missing' })"
Write-Host "  stock client   : $(if ($NoClient) { 'disabled by -NoClient' } elseif ($resolvedClientExe) { $resolvedClientExe } else { 'not found' })"

if ((-not $NoClient) -and (-not $resolvedClientExe)) {
    Write-Host ""
    Write-Host "Live visual testing requires a stock Half-Life client executable."
    Write-Host "Set HL_EXE in .env, pass -HlExe <path>, or use a disposable runtime template that already contains hl.exe."
    exit 1
}

if (-not $NoClient) {
    if ($doctorReport.LiveContentStatus.SameRootLaunchLabel -ne "yes") {
        Write-Host ""
        Write-Host "Live same-root preflight failed: the client and server are not launching from the same Half-Life root."
        Write-Host "Resolve HL_EXE or rerun .\scripts\doctor-testbed.ps1 -PreferClientMatchedRuntime -Repair before retrying."
        exit 1
    }

    if ($doctorReport.LiveContentStatus.ContentMatchLabel -ne "yes") {
        Write-Host ""
        Write-Host "Live same-root preflight failed: content_match=no for the managed live mod and the client."
        Write-Host "Check .\scripts\check-live-map-match.ps1 -PreferClientMatchedRuntime before retrying."
        exit 1
    }
}

$sessionParameters = @{
    Configuration = $configuration
    Map = $Map
    Port = $Port
    LabDummy = $true
    LabTargetProfile = $effectiveTargetProfile
}

switch ($Weapon) {
    "glock" {
        $sessionParameters.GlockProfile = $effectivePreset
    }
    "mp5" {
        $sessionParameters.Mp5Profile = $effectivePreset
    }
}

if ($NoClient) {
    $sessionParameters.NoClient = $true
}

if ($NoTail) {
    $sessionParameters.NoTail = $true
}

if ($AnalyzeLatestOnExit) {
    $sessionParameters.AnalyzeLatestOnExit = $true
}

if ($PassThru) {
    $sessionParameters.PassThru = $true
}

if (-not [string]::IsNullOrWhiteSpace($TemplateRoot)) {
    $sessionParameters.TemplateRoot = $TemplateRoot
}

if (-not [string]::IsNullOrWhiteSpace($HldsExe)) {
    $sessionParameters.HldsExe = $HldsExe
}

if (-not [string]::IsNullOrWhiteSpace($HlExe)) {
    $sessionParameters.HlExe = $HlExe
}

if (-not [string]::IsNullOrWhiteSpace($SteamCmdExe)) {
    $sessionParameters.SteamCmdExe = $SteamCmdExe
}

if ($AllowSteamCmdDownload) {
    $sessionParameters.AllowSteamCmdDownload = $true
}

$sessionParameters.PreferClientMatchedRuntime = $true

if ($ExtraSessionArgs) {
    & $sessionScript @sessionParameters @ExtraSessionArgs
}
else {
    & $sessionScript @sessionParameters
}
