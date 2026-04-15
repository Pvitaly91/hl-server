[CmdletBinding()]
param(
    [switch]$NoFollow
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"
Import-HLServerEnv

$latestLog = Get-LatestWeaponDebugLog
if (-not $latestLog) {
    Write-Host "No weapon debug log was found under $(Get-WeaponDebugLogsRoot). Launch with scripts\\run-testbed.bat experimental-debug first."
    exit 0
}

Write-Host "Weapon debug log: $($latestLog.FullName)"

if ($NoFollow) {
    Get-Content -LiteralPath $latestLog.FullName
}
else {
    Get-Content -LiteralPath $latestLog.FullName -Wait
}
