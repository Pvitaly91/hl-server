[CmdletBinding()]
param(
    [switch]$Detailed
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"
Import-HLServerEnv

$targets = @(Get-GlockLabTargets)

if ($targets.Count -eq 0) {
    throw "No Glock lab target profiles were found under $(Get-GlockLabTargetsRoot)."
}

if ($Detailed) {
    foreach ($target in $targets) {
        Write-Host "$($target.Name)"
        Write-Host "  path        : $($target.Path)"
        Write-Host "  description : $($target.Description)"

        foreach ($assignment in $target.Assignments) {
            Write-Host "  cvar        : $assignment"
        }

        Write-Host ""
    }
}
else {
    $targets |
        Select-Object Name, Path, Description |
        Format-Table -AutoSize
}
