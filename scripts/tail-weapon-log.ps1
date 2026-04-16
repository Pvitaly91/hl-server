[CmdletBinding()]
param(
    [string]$Path,
    [switch]$NoFollow
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"
Import-HLServerEnv

$targetLog = $null

if (-not [string]::IsNullOrWhiteSpace($Path)) {
    $resolvedPath = Get-FullPath -Path $Path
    if (-not (Test-LeafPath -Path $resolvedPath)) {
        throw "Weapon debug log was not found: $resolvedPath"
    }

    $targetLog = Get-Item -LiteralPath $resolvedPath
}
else {
    $targetLog = Get-LatestWeaponDebugLog
    if (-not $targetLog) {
        Write-Host "No weapon debug log was found under $(Get-WeaponDebugLogsRoot). Launch with scripts\\run-testbed.bat experimental-debug first."
        exit 0
    }
}

Write-Host "Weapon debug log: $($targetLog.FullName)"

if ($NoFollow) {
    Get-Content -LiteralPath $targetLog.FullName
}
else {
    Get-Content -LiteralPath $targetLog.FullName -Wait
}
