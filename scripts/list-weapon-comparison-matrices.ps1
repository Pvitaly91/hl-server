[CmdletBinding()]
param(
    [switch]$Detailed
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"
Import-HLServerEnv

$matrices = @(Get-WeaponComparisonMatrices)

if ($matrices.Count -eq 0) {
    throw "No weapon comparison matrices were found under $(Get-WeaponComparisonMatricesRoot)."
}

if ($Detailed) {
    foreach ($matrix in $matrices) {
        Write-Host "$($matrix.Name)"
        Write-Host "  path        : $($matrix.Path)"
        Write-Host "  description : $($matrix.Description)"
        Write-Host "  steps       : $($matrix.StepCount)"

        foreach ($step in $matrix.Steps) {
            Write-Host "  step        : $($step.Name) (weapon=$($step.WeaponUnderTest) profile=$($step.WeaponProfileName) target=$(if ([string]::IsNullOrWhiteSpace($step.LabTargetProfileName)) { 'n/a' } else { $step.LabTargetProfileName }) map=$($step.Map))"
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
