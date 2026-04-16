[CmdletBinding()]
param(
    [switch]$Detailed
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"
Import-HLServerEnv

$matrices = @(Get-GlockComparisonMatrices)

if ($matrices.Count -eq 0) {
    throw "No Glock comparison matrices were found under $(Get-GlockComparisonMatricesRoot)."
}

if ($Detailed) {
    foreach ($matrix in $matrices) {
        Write-Host "$($matrix.Name)"
        Write-Host "  path        : $($matrix.Path)"
        Write-Host "  description : $($matrix.Description)"
        Write-Host "  steps       : $($matrix.StepCount)"

        foreach ($step in $matrix.Steps) {
            Write-Host "  step        : $($step.Name) (profile=$($step.GlockProfileName) target=$($step.LabTargetProfileName) map=$($step.Map))"
            if (-not [string]::IsNullOrWhiteSpace($step.OperatorNote)) {
                Write-Host "  note        : $($step.OperatorNote)"
            }
        }

        Write-Host ""
    }
}
else {
    $matrices |
        Select-Object Name, Path, Description, StepCount |
        Format-Table -AutoSize
}
