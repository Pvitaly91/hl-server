[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [string]$TemplateRoot,
    [string]$HldsExe,
    [string]$HlExe,
    [string]$SteamCmdExe,
    [switch]$AllowSteamCmdDownload,
    [switch]$BuildIfMissing
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

$resolvedTemplateRoot = Resolve-HldsTemplateRoot -ExplicitTemplateRoot $TemplateRoot -ExplicitHldsExe $HldsExe -ExplicitHlExe $HlExe -ExplicitSteamCmdExe $SteamCmdExe -AllowSteamCmdDownload:$AllowSteamCmdDownload
$runtimeRoot = Join-RepoPath "testbed\runtime"
$testbedRoot = Join-RepoPath "testbed"

Write-Step "Preparing disposable runtime from $resolvedTemplateRoot"

Reset-DisposableDirectory -Path $runtimeRoot -AllowedRoot $testbedRoot
Copy-DirectoryContents -Source $resolvedTemplateRoot -Destination $runtimeRoot

$runtimeHldsExe = Join-Path $runtimeRoot "hlds.exe"
if (-not (Test-LeafPath -Path $runtimeHldsExe)) {
    throw "Resolved runtime template did not contain hlds.exe: $resolvedTemplateRoot"
}

$runtimeValveDlls = Join-Path $runtimeRoot "valve\dlls"
Ensure-Directory -Path $runtimeValveDlls

Copy-Item -LiteralPath $dllPath -Destination (Join-Path $runtimeValveDlls "hl.dll") -Force

$pdbPath = Get-HlPdbPath -Configuration $configuration
if (Test-LeafPath -Path $pdbPath) {
    Copy-Item -LiteralPath $pdbPath -Destination (Join-Path $runtimeValveDlls "hl.pdb") -Force
}

$markerPath = Join-Path $runtimeRoot ".hl-server-testbed.txt"
@"
Disposable runtime prepared by hl-server.
Template root: $resolvedTemplateRoot
Installed hl.dll: $dllPath
Timestamp: $(Get-Date -Format o)
"@ | Set-Content -LiteralPath $markerPath -Encoding ASCII

Write-Step "Disposable runtime ready"
Write-Host "Runtime root: $runtimeRoot"
Write-Host "Installed DLL: $(Join-Path $runtimeValveDlls 'hl.dll')"
