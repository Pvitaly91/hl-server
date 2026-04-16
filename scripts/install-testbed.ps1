[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [string]$TemplateRoot,
    [string]$HldsExe,
    [string]$HlExe,
    [string]$SteamCmdExe,
    [switch]$AllowSteamCmdDownload,
    [switch]$BuildIfMissing,
    [switch]$ForceTemplateRefresh
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"
Import-HLServerEnv

$configuration = Get-ValidatedConfiguration -Configuration $Configuration
$dllPath = Get-HlDllPath -Configuration $configuration

if (-not (Test-LeafPath -Path $dllPath)) {
    if ($BuildIfMissing) {
        & "$PSScriptRoot\build.ps1" -Configuration $configuration
    }
    else {
        throw "Build artifact not found at $dllPath. Run scripts/build.ps1 first or pass -BuildIfMissing."
    }
}

$pdbPath = Get-HlPdbPath -Configuration $configuration
$installResult = Install-TestbedRuntime -Configuration $configuration -BuiltDllPath $dllPath -BuiltPdbPath $pdbPath -ExplicitTemplateRoot $TemplateRoot -ExplicitHldsExe $HldsExe -ExplicitHlExe $HlExe -ExplicitSteamCmdExe $SteamCmdExe -AllowSteamCmdDownload:$AllowSteamCmdDownload -ForceTemplateRefresh:$ForceTemplateRefresh

Write-Step "Disposable runtime ready"
Write-Host "Runtime root       : $($installResult.RuntimeRoot)"
Write-Host "Installed DLL      : $($installResult.InstalledDllPath)"
Write-Host "Runtime manifest   : $($installResult.ManifestPath)"
Write-Host "Created steam_appid: $($installResult.CreatedSteamAppIdFile)"
