[CmdletBinding()]
param(
    [switch]$Detailed
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"
Import-HLServerEnv

$profiles = @(Get-GlockProfiles)

if ($profiles.Count -eq 0) {
    throw "No Glock profiles were found under $(Get-GlockProfilesRoot)."
}

if ($Detailed) {
    foreach ($profile in $profiles) {
        Write-Host "$($profile.Name)"
        Write-Host "  path        : $($profile.Path)"
        Write-Host "  description : $($profile.Description)"

        foreach ($assignment in $profile.Assignments) {
            Write-Host "  cvar        : $assignment"
        }

        Write-Host ""
    }
}
else {
    $profiles |
        Select-Object Name, Path, Description |
        Format-Table -AutoSize
}
