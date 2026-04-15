[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"
Import-HLServerEnv

$configuration = Get-ValidatedConfiguration -Configuration $Configuration
$buildDir = Get-BuildDirectory

if (-not (Test-Path -LiteralPath $buildDir -PathType Container)) {
    & "$PSScriptRoot\configure.ps1"
}

Write-Step "Building hl.dll ($configuration)"

$cmake = Get-CMakePath
& $cmake --build --preset (Get-BuildPresetName -Configuration $configuration)
if ($LASTEXITCODE -ne 0) {
    throw "Build failed for configuration $configuration."
}

$dllPath = Get-HlDllPath -Configuration $configuration
if (-not (Test-LeafPath -Path $dllPath)) {
    throw "Build completed but hl.dll was not found at $dllPath."
}

Write-Step "Build complete"
Write-Host "hl.dll: $dllPath"

$pdbPath = Get-HlPdbPath -Configuration $configuration
if (Test-LeafPath -Path $pdbPath) {
    Write-Host "hl.pdb: $pdbPath"
}
