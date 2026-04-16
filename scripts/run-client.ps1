[CmdletBinding()]
param(
    [string]$ConnectAddress = "127.0.0.1:27015",
    [string]$HlExe
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

Start-Process -FilePath $resolvedHlExe -WorkingDirectory (Split-Path -Parent $resolvedHlExe) -ArgumentList @("-game", "valve", "-console", "+connect", $ConnectAddress) | Out-Null
Write-Host "Client launch requested: $resolvedHlExe"
