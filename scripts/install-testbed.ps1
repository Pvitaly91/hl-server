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
$installResult = if ($PreferClientMatchedRuntime) {
    Install-TestbedLiveMod -Configuration $configuration -BuiltDllPath $dllPath -BuiltPdbPath $pdbPath -ExplicitHlExe $HlExe
}
else {
    Install-TestbedRuntime -Configuration $configuration -BuiltDllPath $dllPath -BuiltPdbPath $pdbPath -ExplicitTemplateRoot $TemplateRoot -ExplicitHldsExe $HldsExe -ExplicitHlExe $HlExe -ExplicitSteamCmdExe $SteamCmdExe -AllowSteamCmdDownload:$AllowSteamCmdDownload -PreferClientMatchedRuntime:$PreferClientMatchedRuntime -ForceTemplateRefresh:$ForceTemplateRefresh
}

if ($PreferClientMatchedRuntime) {
    Write-Step "Same-root live mod ready"
    Write-Host "Live mod stage     : $($installResult.StageRoot)"
    Write-Host "Live mod root      : $($installResult.LinkPath)"
    Write-Host "Live game dir      : $($installResult.GameDirName)"
    Write-Host "Client root        : $($installResult.ClientInstall.Root)"
    Write-Host "Installed DLL      : $($installResult.InstalledDllPath)"
    Write-Host "Live manifest      : $($installResult.ManifestPath)"
}
else {
    Write-Step "Disposable runtime ready"
    Write-Host "Runtime root       : $($installResult.RuntimeRoot)"
    Write-Host "Installed DLL      : $($installResult.InstalledDllPath)"
    Write-Host "Runtime manifest   : $($installResult.ManifestPath)"
    Write-Host "Created steam_appid: $($installResult.CreatedSteamAppIdFile)"
    if ($installResult.ClientInstall) {
        Write-Host "Client root        : $($installResult.ClientInstall.Root)"
    }
    Write-Host "Launch hlds        : $(if ($installResult.LiveContentStatus.RuntimeHldsExe) { $installResult.LiveContentStatus.RuntimeHldsExe } else { 'missing' })"
    Write-Host "Launch hl          : $(if ($installResult.LiveContentStatus.ClientLaunchExe) { $installResult.LiveContentStatus.ClientLaunchExe } else { 'not found' })"
    Write-Host "Same-root launch   : $($installResult.LiveContentStatus.SameRootLaunchLabel)"
    Write-Host "content_match      : $($installResult.LiveContentStatus.ContentMatchLabel)"
    Write-Host "Live content       : $($installResult.LiveContentStatus.Verdict)"
}
