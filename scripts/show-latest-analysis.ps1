[CmdletBinding()]
param(
    [ValidateSet("all", "glock", "mp5")]
    [string]$Weapon = "all"
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"
Import-HLServerEnv

function Get-LatestAnalysisLog {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Weapon
    )

    $logsRoot = Get-WeaponDebugLogsRoot
    if (-not (Test-Path -LiteralPath $logsRoot -PathType Container)) {
        return $null
    }

    $candidateLogs = @(Get-ChildItem -LiteralPath $logsRoot -File -Filter "weapon-debug-*.log" | Sort-Object LastWriteTimeUtc, Name -Descending)
    if ($candidateLogs.Count -eq 0) {
        return $null
    }

    if ($Weapon -eq "all") {
        return $candidateLogs[0]
    }

    $sessionPattern = 'event=weapon_debug_session.*weapon_under_test="?{0}"?' -f [regex]::Escape($Weapon)

    foreach ($candidateLog in $candidateLogs) {
        if (Select-String -Path $candidateLog.FullName -Pattern $sessionPattern -Quiet -ErrorAction SilentlyContinue) {
            return $candidateLog
        }
    }

    foreach ($candidateLog in $candidateLogs) {
        if (Select-String -Path $candidateLog.FullName -Pattern ("weapon=" + $Weapon) -SimpleMatch -Quiet -ErrorAction SilentlyContinue) {
            return $candidateLog
        }
    }

    return $null
}

$latestLog = Get-LatestAnalysisLog -Weapon $Weapon
if (-not $latestLog) {
    $weaponLabel = if ($Weapon -eq "all") { "weapon" } else { $Weapon }
    Write-Host "No disposable $weaponLabel telemetry log was found under $(Get-WeaponDebugLogsRoot)."
    Write-Host "Run scripts\\play-glock-live.bat or scripts\\play-mp5-live.bat first, then rerun this launcher."
    exit 1
}

Write-Host "Latest telemetry log : $($latestLog.FullName)"
Write-Host "Reports root         : $(Get-WeaponDebugReportsRoot)"
Write-Host ""

& "$PSScriptRoot\analyze-weapon-log.ps1" -Path $latestLog.FullName -Weapon $Weapon
