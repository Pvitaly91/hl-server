[CmdletBinding()]
param(
    [string]$HlExe
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"
Import-HLServerEnv

$clientInstall = Resolve-TestbedClientInstall -ExplicitHlExe $HlExe
if (-not $clientInstall) {
    throw "No stock Half-Life client was resolved. Set HL_EXE in .env or pass -HlExe <path>, then run scripts\setup-improved-hldm.ps1."
}

$editorPath = Join-Path (Get-TestbedLiveModRoot -ClientRoot $clientInstall.Root -GameDirName (Get-TestbedLiveModName)) "HlConfigEditorCpp.exe"
if (-not (Test-Path -LiteralPath $editorPath -PathType Leaf)) {
    throw "Deployed editor was not found at $editorPath. Run scripts\setup-improved-hldm.ps1 first."
}

Write-Step "Opening Improved HLDM editor"
Write-Host "Editor: $editorPath"
Start-Process -FilePath $editorPath | Out-Null
