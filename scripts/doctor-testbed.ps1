[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [string]$TemplateRoot,
    [string]$HldsExe,
    [string]$HlExe,
    [string]$SteamCmdExe,
    [switch]$AllowSteamCmdDownload,
    [switch]$PreferClientMatchedRuntime,
    [switch]$Repair,
    [switch]$BuildIfMissing,
    [string]$ExportJsonPath
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"
Import-HLServerEnv

$configuration = Get-ValidatedConfiguration -Configuration $Configuration
$effectiveAllowDownload = $AllowSteamCmdDownload -or $Repair

if ($Repair) {
    Write-Step "Repairing disposable runtime before diagnosis"
    & "$PSScriptRoot\install-testbed.ps1" -Configuration $configuration -TemplateRoot $TemplateRoot -HldsExe $HldsExe -HlExe $HlExe -SteamCmdExe $SteamCmdExe -AllowSteamCmdDownload:$effectiveAllowDownload -PreferClientMatchedRuntime:$PreferClientMatchedRuntime -BuildIfMissing:$BuildIfMissing -ForceTemplateRefresh
}

$report = Get-TestbedDoctorReport -Configuration $configuration -ExplicitTemplateRoot $TemplateRoot -ExplicitHldsExe $HldsExe -ExplicitHlExe $HlExe -ExplicitSteamCmdExe $SteamCmdExe -AllowSteamCmdDownload:$effectiveAllowDownload -PreferClientMatchedRuntime:$PreferClientMatchedRuntime
Write-TestbedDoctorSummary -Report $report

if (-not [string]::IsNullOrWhiteSpace($ExportJsonPath)) {
    $exportPath = Get-FullPath -Path $ExportJsonPath
    $report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $exportPath -Encoding ASCII
    Write-Host "Doctor JSON        : $exportPath"
}

if (-not $report.IsHealthy) {
    throw (Get-TestbedDoctorFailureMessage -Report $report)
}
