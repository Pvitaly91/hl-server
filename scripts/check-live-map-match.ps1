[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [string]$TemplateRoot,
    [string]$HldsExe,
    [string]$HlExe,
    [string]$SteamCmdExe,
    [switch]$AllowSteamCmdDownload,
    [switch]$PreferClientMatchedRuntime
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"
Import-HLServerEnv

$configuration = Get-ValidatedConfiguration -Configuration $Configuration
$report = Get-TestbedDoctorReport `
    -Configuration $configuration `
    -ExplicitTemplateRoot $TemplateRoot `
    -ExplicitHldsExe $HldsExe `
    -ExplicitHlExe $HlExe `
    -ExplicitSteamCmdExe $SteamCmdExe `
    -AllowSteamCmdDownload:$AllowSteamCmdDownload `
    -PreferClientMatchedRuntime:$PreferClientMatchedRuntime `
    -IgnoreLatestLaunchFailure

$live = $report.LiveContentStatus

Write-Step "Live content inspection"
Write-Host "Diagnosis           : $($report.Classification)"
Write-Host "Live mode           : $(if ($live.PreferClientMatchedRuntime) { 'same-root live mod requested' } else { 'default selection' })"
Write-Host "Runtime root        : $($live.RuntimeRoot)"
Write-Host "Runtime hlds.exe    : $(if ($live.RuntimeHldsExe) { $live.RuntimeHldsExe } else { 'missing' })"
Write-Host "Runtime hl.exe      : $(if ($live.RuntimeHlExe) { $live.RuntimeHlExe } else { 'not found' })"
Write-Host "Client hl.exe       : $(if ($live.ClientHlExe) { $live.ClientHlExe } else { 'not found' })"
Write-Host "Launch client exe   : $(if ($live.ClientLaunchExe) { $live.ClientLaunchExe } else { 'not found' })"
Write-Host "Game dir            : $($live.GameDirName)"
Write-Host "Client root         : $(if ($live.ClientRoot) { $live.ClientRoot } else { 'not found' })"
Write-Host "Runtime source root : $(if ($live.RuntimeSourceRoot) { $live.RuntimeSourceRoot } else { 'unavailable' })"
Write-Host "Content source root : $(if ($live.EffectiveContentRoot) { $live.EffectiveContentRoot } else { 'unavailable' })"
Write-Host "Manifest content    : $(if ($live.ManifestContentRoot) { $live.ManifestContentRoot } else { 'missing' })"
Write-Host "Live mod root       : $(if ($live.LiveModLinkPath) { $live.LiveModLinkPath } else { 'not used' })"
Write-Host "Live mod stage      : $(if ($live.LiveModStageRoot) { $live.LiveModStageRoot } else { 'not used' })"
Write-Host "Server work dir     : $(if ($live.ServerWorkingDirectory) { $live.ServerWorkingDirectory } else { 'unknown' })"
Write-Host "Client work dir     : $(if ($live.ClientWorkingDirectory) { $live.ClientWorkingDirectory } else { 'unknown' })"
Write-Host "Same-root launch    : $($live.SameRootLaunchLabel)"
Write-Host "content_match       : $($live.ContentMatchLabel)"
Write-Host "root_match          : $($live.RootMatchLabel)"
Write-Host "Diagnosis kind      : $($live.DiagnosisKind)"
Write-Host "Verdict             : $($live.Verdict)"
Write-Host "Runtime map         : $(if ($live.RuntimeMap.Path) { $live.RuntimeMap.Path } else { 'missing' })"
Write-Host "Client map          : $(if ($live.ClientMap.Path) { $live.ClientMap.Path } else { 'missing' })"
Write-Host "Source map          : $(if ($live.SourceMap.Path) { $live.SourceMap.Path } else { 'missing' })"
Write-Host "Runtime map hash    : $(if ($live.RuntimeMap.Hash) { $live.RuntimeMap.Hash } else { 'missing' })"
Write-Host "Client map hash     : $(if ($live.ClientMap.Hash) { $live.ClientMap.Hash } else { 'missing' })"
Write-Host "Source map hash     : $(if ($live.SourceMap.Hash) { $live.SourceMap.Hash } else { 'missing' })"

foreach ($warning in @($report.Warnings)) {
    Write-Host "Warning             : $warning"
}

foreach ($issue in @($report.Issues)) {
    Write-Host "Issue               : $issue"
}

foreach ($recommendation in @($report.Recommendations)) {
    Write-Host "Remediation         : $recommendation"
}

if (-not $report.IsHealthy) {
    throw (Get-TestbedDoctorFailureMessage -Report $report)
}
