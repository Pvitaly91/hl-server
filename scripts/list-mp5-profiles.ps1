[CmdletBinding()]
param(
    [switch]$Detailed
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"
Import-HLServerEnv

$profiles = @(Get-Mp5Profiles)

if ($profiles.Count -eq 0) {
    throw "No MP5 profiles were found under $(Get-Mp5ProfilesRoot)."
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
