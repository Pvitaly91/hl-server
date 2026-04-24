[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [string]$Map = "crossfire",
    [int]$Port = 27015,
    [int]$MaxPlayers = 16,
    [string]$HlExe,
    [switch]$NoClient,
    [switch]$NoAutoConnect
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"
Import-HLServerEnv

$configuration = Get-ValidatedConfiguration -Configuration $Configuration
$defaultCfg = Join-RepoPath "configs\match-packs\hldm_skill_default.cfg"
if (-not (Test-Path -LiteralPath $defaultCfg -PathType Leaf)) {
    throw "Recommended Improved HLDM cfg was not found at $defaultCfg"
}

$clientInstall = Resolve-TestbedClientInstall -ExplicitHlExe $HlExe
if ($clientInstall) {
    $editorPath = Join-Path (Get-TestbedLiveModRoot -ClientRoot $clientInstall.Root -GameDirName (Get-TestbedLiveModName)) "HlConfigEditorCpp.exe"
}
else {
    $editorPath = "<HalfLifeRoot>\hlserver_testbed\HlConfigEditorCpp.exe"
}

Write-Step "Launching Improved HLDM stable package"
Write-Host "Cfg applied at launch : $defaultCfg"
Write-Host "Map                   : $Map"
Write-Host "Port                  : $Port"
Write-Host "Editor                : $editorPath"
Write-Host ""
Write-Host "Useful live commands after HLDS is ready:"
Write-Host "  exp_matchcfg_apply hldm_skill_default"
Write-Host "  exp_matchcfg_status"
Write-Host "  exp_sandbox_start"
Write-Host "  exp_sandbox_reset"
Write-Host ""

$launchArgs = @{
    Configuration = $configuration
    Map = $Map
    Port = $Port
    MaxPlayers = $MaxPlayers
    CfgPath = $defaultCfg
}

if (-not [string]::IsNullOrWhiteSpace($HlExe)) {
    $launchArgs["HlExe"] = $HlExe
}

if ($NoClient) {
    $launchArgs["NoClient"] = $true
}

if ($NoAutoConnect) {
    $launchArgs["NoAutoConnect"] = $true
}

& "$PSScriptRoot\play-hlserver-testbed-direct.ps1" @launchArgs
