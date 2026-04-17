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
$directLauncher = Join-Path $PSScriptRoot "play-hlserver-testbed-direct.ps1"

$effectivePreset = switch ($Weapon) {
    "glock" {
        if ([string]::IsNullOrWhiteSpace($Preset)) { "cs_tight" } else { $Preset }
    }
    "mp5" {
        if ([string]::IsNullOrWhiteSpace($Preset)) { "cs_burst" } else { $Preset }
    }
}

$effectiveTargetProfile = if ([string]::IsNullOrWhiteSpace($TargetProfile)) {
    if ($Weapon -eq "mp5") { "vest" } else { "vest_headprotected" }
}
else {
    $TargetProfile
}

$ignoredOptions = New-Object System.Collections.Generic.List[string]
if (-not [string]::IsNullOrWhiteSpace($TemplateRoot)) { $ignoredOptions.Add("-TemplateRoot") }
if (-not [string]::IsNullOrWhiteSpace($HldsExe)) { $ignoredOptions.Add("-HldsExe") }
if (-not [string]::IsNullOrWhiteSpace($SteamCmdExe)) { $ignoredOptions.Add("-SteamCmdExe") }
if ($AllowSteamCmdDownload) { $ignoredOptions.Add("-AllowSteamCmdDownload") }
if ($NoTail) { $ignoredOptions.Add("-NoTail") }
if ($AnalyzeLatestOnExit) { $ignoredOptions.Add("-AnalyzeLatestOnExit") }
if ($PassThru) { $ignoredOptions.Add("-PassThru") }
if ($ExtraSessionArgs -and $ExtraSessionArgs.Count -gt 0) {
    $ignoredOptions.Add("extra session arguments: $($ExtraSessionArgs -join ' ')")
}

Write-Host ""
Write-Host "Live launcher compatibility wrapper"
Write-Host "  requested weapon : $Weapon"
Write-Host "  effective preset : $effectivePreset"
Write-Host "  target profile   : $effectiveTargetProfile"
Write-Host "  launch path      : direct same-root launcher"
if ($ignoredOptions.Count -gt 0) {
    Write-Host "  ignored options  : $($ignoredOptions -join ', ')"
}

$directParameters = @{
    Mode = $Weapon
    Configuration = $configuration
    Map = $Map
    Port = $Port
    TargetProfile = $effectiveTargetProfile
}

if (-not [string]::IsNullOrWhiteSpace($HlExe)) {
    $directParameters.HlExe = $HlExe
}

if ($NoClient) {
    $directParameters.NoClient = $true
}

switch ($Weapon) {
    "glock" {
        $directParameters.GlockProfile = $effectivePreset
    }
    "mp5" {
        $directParameters.Mp5Profile = $effectivePreset
    }
}

& $directLauncher @directParameters
