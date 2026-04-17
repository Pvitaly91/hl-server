[CmdletBinding()]
param(
    [string]$ConnectAddress = "127.0.0.1:27015",
    [string]$HlExe,
    [string]$Game = "valve",
    [string]$WorkingDirectory
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"
Import-HLServerEnv

$resolvedHlExe = Resolve-HlExe -ExplicitPath $HlExe
if (-not $resolvedHlExe) {
    Write-Host "Half-Life client was not found. Set HL_EXE in .env or pass -HlExe to launch the stock client."
    return
}

Write-Step "Launching stock Half-Life client"

if ([string]::IsNullOrWhiteSpace($WorkingDirectory)) {
    $WorkingDirectory = Split-Path -Parent $resolvedHlExe
}

Start-Process -FilePath $resolvedHlExe -WorkingDirectory $WorkingDirectory -ArgumentList @("-game", $Game, "-console", "+connect", $ConnectAddress) | Out-Null
Write-Host "Client launch requested: $resolvedHlExe"
