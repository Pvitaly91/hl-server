[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Matrix,
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [switch]$NoClient,
    [switch]$NoTail,
    [int]$PortBase = 27015,
    [switch]$AutoAdvance,
    [int]$MaxSteps,
    [switch]$OpenReport
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"
Import-HLServerEnv

function To-ConfigBool {
    param($Value)

    if ($null -eq $Value) {
        return $false
    }

    if ($Value -is [bool]) {
        return $Value
    }

    $text = ([string]$Value).Trim().ToLowerInvariant()
    if ([string]::IsNullOrWhiteSpace($text)) {
        return $false
    }

    switch ($text) {
        "1" { return $true }
        "true" { return $true }
        "yes" { return $true }
        default { return $false }
    }
}

function To-ConfigNumber {
    param($Value)

    if ($null -eq $Value -or [string]::IsNullOrWhiteSpace([string]$Value)) {
        return 0.0
    }

    return [double]$Value
}

function Get-StepSessionTag {
    param(
        [Parameter(Mandatory = $true)]
        [string]$MatrixName,
        [Parameter(Mandatory = $true)]
        [string]$RunStamp,
        [Parameter(Mandatory = $true)]
        [int]$StepNumber,
        [Parameter(Mandatory = $true)]
        [string]$StepName
    )

    return ("{0}-{1}-{2:D2}-{3}" -f $MatrixName, $RunStamp, $StepNumber, $StepName)
}

function Write-MatrixStepChecklist {
    param(
        [Parameter(Mandatory = $true)]
        [int]$StepNumber,
        [Parameter(Mandatory = $true)]
        [int]$TotalSteps,
        [Parameter(Mandatory = $true)]
        $Step,
        [Parameter(Mandatory = $true)]
        $SessionInfo
    )

    $targetCvars = $Step.LabTargetProfile.Cvars
    $armoredTarget = (To-ConfigNumber (Get-ObjectPropertyValue -InputObject $targetCvars -Names @("sv_exp_glock_lab_dummy_armor"))) -gt 0.0
    $protectedHeadTarget = To-ConfigBool (Get-ObjectPropertyValue -InputObject $targetCvars -Names @("sv_exp_glock_lab_dummy_head_protected"))

    Write-Host ""
    Write-Host ("Matrix step {0}/{1}" -f $StepNumber, $TotalSteps)
    Write-Host "  name          : $($Step.Name)"
    Write-Host "  matrix        : $($SessionInfo.MatrixName)"
    Write-Host "  session tag   : $($SessionInfo.SessionTag)"
    Write-Host "  glock profile : $($Step.GlockProfileName)"
    Write-Host "  target profile: $($Step.LabTargetProfileName)"
    Write-Host "  map           : $($Step.Map)"
    Write-Host "  connect       : $($SessionInfo.ConnectAddress)"
    Write-Host "  server log    : $($SessionInfo.ServerLogPath)"
    Write-Host "  weapon log    : $(if ([string]::IsNullOrWhiteSpace($SessionInfo.WeaponLogPath)) { 'pending' } else { $SessionInfo.WeaponLogPath })"
    if (-not [string]::IsNullOrWhiteSpace($Step.OperatorNote)) {
        Write-Host "  operator note : $($Step.OperatorNote)"
    }

    Write-Host ""
    Write-Host "Checklist"
    Write-Host "1. Confirm the stock client joined the disposable lab server at $($SessionInfo.ConnectAddress)."
    Write-Host "2. Confirm the lab dummy appeared in front of the player."
    Write-Host "3. Confirm the active Glock profile is $($Step.GlockProfileName) and the target profile is $($Step.LabTargetProfileName)."
    Write-Host "4. Fire one careful recovered shot after coming to rest."
    Write-Host "5. Hold primary to verify tap-fire rejection evidence."
    Write-Host "6. Fire while moving to exercise movement-penalty evidence."
    Write-Host "7. Attempt at least one headshot."

    if ($armoredTarget -or $protectedHeadTarget) {
        Write-Host "8. Attempt both torso and head shots so the armored/protected target path is represented."
        Write-Host "9. Press Enter when the step is finished so analysis can run."
    }
    else {
        Write-Host "8. Press Enter when the step is finished so analysis can run."
    }

    Write-Host ""
    Write-Host "This runner structures manual firing validation only. It does not create gameplay evidence by itself without human input."
}

if ($PortBase -lt 1) {
    throw "PortBase must be at least 1."
}

if ($MaxSteps -lt 0) {
    throw "MaxSteps cannot be negative."
}

$matrixDefinition = Get-GlockComparisonMatrix -Name $Matrix
$steps = @($matrixDefinition.Steps)
if ($MaxSteps -gt 0) {
    $steps = @($steps | Select-Object -First $MaxSteps)
}

if ($steps.Count -eq 0) {
    throw "Matrix '$Matrix' did not produce any runnable steps."
}

$configuration = Get-ValidatedConfiguration -Configuration $Configuration
$runStamp = Get-Date -Format "yyyyMMdd-HHmmss"
$matrixOutputRoot = Join-Path (Get-GlockComparisonReportsRoot) ("{0}-{1}" -f $matrixDefinition.Name, $runStamp)
$stepOutputRoot = Join-Path $matrixOutputRoot "steps"
Ensure-Directory -Path $stepOutputRoot

$preferredPort = Get-AvailableUdpPort -PreferredPort $PortBase
if ($preferredPort -ne $PortBase) {
    Write-Step "Requested port base $PortBase was busy, using free UDP port $preferredPort instead"
}

$currentPort = $preferredPort
$clientLaunched = $false
$stepReportPaths = New-Object System.Collections.Generic.List[string]
$stepResults = New-Object System.Collections.Generic.List[object]
$abortRemaining = $false

for ($index = 0; $index -lt $steps.Count; $index += 1) {
    $step = $steps[$index]
    $stepNumber = $index + 1
    $stepTag = $step.Name
    $stepFolder = Join-Path $stepOutputRoot ("{0:D2}-{1}" -f $stepNumber, $stepTag)
    Ensure-Directory -Path $stepFolder

    $sessionTag = Get-StepSessionTag -MatrixName $matrixDefinition.Name -RunStamp $runStamp -StepNumber $stepNumber -StepName $stepTag
    $sessionInfo = $null

    try {
        $sessionInfo = & "$PSScriptRoot\run-glock-test-session.ps1" `
            -Configuration $configuration `
            -Port $currentPort `
            -Map $step.Map `
            -GlockProfile $step.GlockProfileName `
            -LabTargetProfile $step.LabTargetProfileName `
            -SetCvar $step.Assignments `
            -SessionTag $sessionTag `
            -MatrixName $matrixDefinition.Name `
            -MatrixStep $stepTag `
            -LabDummy `
            -NoClient `
            -NoTail:$NoTail `
            -SkipChecklist `
            -PassThru

        $currentPort = $sessionInfo.Port

        if (-not $NoClient -and -not $AutoAdvance -and -not $clientLaunched) {
            $clientExe = Get-TestbedSessionClientExe -RuntimeRoot $sessionInfo.RuntimeRoot
            if ($clientExe) {
                & "$PSScriptRoot\run-client.ps1" -ConnectAddress $sessionInfo.ConnectAddress -HlExe $clientExe
                $clientLaunched = $true
                Write-Step "Launched the stock Half-Life client once for the matrix run; reuse that window for the remaining steps"
            }
            else {
                Write-Host "No stock Half-Life client executable was found for matrix step $stepTag. Continue with a manual client launch or rerun with -NoClient."
            }
        }
        elseif (-not $NoClient -and -not $AutoAdvance -and $clientLaunched) {
            Write-Host "Reuse the existing stock client window and reconnect to $($sessionInfo.ConnectAddress) if the previous server restart dropped the connection."
        }
        elseif ($AutoAdvance -and -not $NoClient) {
            Write-Host "AutoAdvance is enabled, so no stock client launch was requested for matrix step $stepTag."
        }

        Write-MatrixStepChecklist -StepNumber $stepNumber -TotalSteps $steps.Count -Step $step -SessionInfo $sessionInfo

        $stepStatus = if ($AutoAdvance) { "auto_advanced" } else { "completed" }
        if ($AutoAdvance) {
            Write-Host "AutoAdvance is enabled. The step will be analyzed immediately, which is suitable for tooling verification but not live gameplay evidence."
        }
        else {
            $operatorChoice = Read-Host "Press Enter to analyze this step, S to skip firing but still export artifacts, or Q to stop after this step"
            $normalizedChoice = ([string]$operatorChoice).Trim().ToUpperInvariant()
            switch ($normalizedChoice) {
                "Q" {
                    $stepStatus = "aborted"
                    $abortRemaining = $true
                    Write-Host "Stopping after this step. The current session will still be analyzed as-is."
                }
                "S" {
                    $stepStatus = "skipped"
                    Write-Host "Skipping manual firing for this step. The session will still be analyzed so the artifacts stay explicit."
                }
                default {
                    $stepStatus = "completed"
                }
            }
        }

        if (-not [string]::IsNullOrWhiteSpace($sessionInfo.WeaponLogPath) -and (Test-LeafPath -Path $sessionInfo.WeaponLogPath)) {
            $analysisResult = & "$PSScriptRoot\analyze-weapon-log.ps1" -Path $sessionInfo.WeaponLogPath -ExportJson -ExportCsv -OutputDir $stepFolder -PassThru
        }
        else {
            $analysisResult = & "$PSScriptRoot\analyze-weapon-log.ps1" -Latest -ExportJson -ExportCsv -OutputDir $stepFolder -PassThru
        }
        if ($analysisResult.Exports -and -not [string]::IsNullOrWhiteSpace($analysisResult.Exports.json)) {
            $stepReportPaths.Add($analysisResult.Exports.json)
        }

        $stepResults.Add([PSCustomObject]@{
            StepNumber = $stepNumber
            StepName = $stepTag
            Status = $stepStatus
            SessionTag = $sessionTag
            ReportPath = if ($analysisResult.Exports) { $analysisResult.Exports.json } else { $null }
            ReportCsvPath = if ($analysisResult.Exports) { $analysisResult.Exports.csv } else { $null }
        })
    }
    finally {
        if ($sessionInfo -and $sessionInfo.Process) {
            Stop-ProcessIfRunning -Process $sessionInfo.Process
        }
    }

    if ($abortRemaining) {
        break
    }
}

if ($stepReportPaths.Count -eq 0) {
    throw "The matrix runner did not produce any analyzer JSON reports."
}

$comparisonResult = & "$PSScriptRoot\compare-weapon-reports.ps1" `
    -ReportPaths $stepReportPaths.ToArray() `
    -ExportJson `
    -ExportCsv `
    -ExportMarkdown `
    -OutputDir $matrixOutputRoot `
    -PassThru

$completedCount = @($stepResults | Where-Object { $_.Status -eq "completed" }).Count
$skippedCount = @($stepResults | Where-Object { $_.Status -eq "skipped" }).Count
$autoAdvancedCount = @($stepResults | Where-Object { $_.Status -eq "auto_advanced" }).Count
$abortedCount = @($stepResults | Where-Object { $_.Status -eq "aborted" }).Count

Write-Host ""
Write-Host "Matrix run complete"
Write-Host "  matrix root      : $matrixOutputRoot"
Write-Host "  completed steps  : $completedCount"
Write-Host "  skipped steps    : $skippedCount"
Write-Host "  auto-advanced    : $autoAdvancedCount"
Write-Host "  aborted steps    : $abortedCount"

if ($comparisonResult.Exports) {
    if (-not [string]::IsNullOrWhiteSpace($comparisonResult.Exports.markdown)) {
        Write-Host "  markdown summary : $($comparisonResult.Exports.markdown)"
    }
    if (-not [string]::IsNullOrWhiteSpace($comparisonResult.Exports.json)) {
        Write-Host "  json summary     : $($comparisonResult.Exports.json)"
    }
    if (-not [string]::IsNullOrWhiteSpace($comparisonResult.Exports.csv)) {
        Write-Host "  csv summary      : $($comparisonResult.Exports.csv)"
    }
}

Write-Host ""
Write-Host "This tooling verified launch, tagging, and report aggregation. Fresh gameplay evidence still requires real firing in the stock client."

if ($OpenReport) {
    $reportTarget = if ($comparisonResult.Exports -and -not [string]::IsNullOrWhiteSpace($comparisonResult.Exports.markdown)) {
        $comparisonResult.Exports.markdown
    }
    else {
        $matrixOutputRoot
    }

    if (-not [string]::IsNullOrWhiteSpace($reportTarget)) {
        Start-Process -FilePath $reportTarget | Out-Null
    }
}
