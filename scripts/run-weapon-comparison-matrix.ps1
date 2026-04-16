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

function Get-WeaponStepChecklistItems {
    param(
        [Parameter(Mandatory = $true)]
        $Step,
        [Parameter(Mandatory = $true)]
        [bool]$NoClientMode
    )

    $items = New-Object System.Collections.Generic.List[string]

    if ($NoClientMode) {
        $items.Add("No client was launched for this step. Treat it as a launch, tagging, and artifact-verification pass unless you attach manually.")
    }
    else {
        $items.Add("Confirm the stock client joined the disposable server at the printed connect address.")
    }

    if ($Step.WeaponUnderTest -eq "mp5") {
        $items.Add("Confirm the active MP5 profile is $($Step.WeaponProfileName).")
        $items.Add("Fire a short controlled burst.")
        $items.Add("Fire a longer burst to exercise burst growth.")
        $items.Add("Fire while moving to exercise moving spray evidence.")
        $items.Add("Attempt at least one dummy headshot.")
    }
    else {
        $items.Add("Confirm the active Glock profile is $($Step.WeaponProfileName).")
        $items.Add("Fire one recovered single shot.")
        $items.Add("Hold primary to exercise hold-to-fire rejection.")
        $items.Add("Fire while moving to exercise movement-penalty evidence.")
        $items.Add("Attempt at least one dummy headshot.")
    }

    if (-not [string]::IsNullOrWhiteSpace($Step.LabTargetProfileName)) {
        $items.Add("Confirm the lab target profile is $($Step.LabTargetProfileName).")

        $targetCvars = if ($Step.LabTargetProfile) { $Step.LabTargetProfile.Cvars } else { $null }
        $armoredTarget = (To-ConfigNumber (Get-ObjectPropertyValue -InputObject $targetCvars -Names @("sv_exp_glock_lab_dummy_armor"))) -gt 0.0
        $protectedHeadTarget = To-ConfigBool (Get-ObjectPropertyValue -InputObject $targetCvars -Names @("sv_exp_glock_lab_dummy_head_protected"))
        if ($armoredTarget -or $protectedHeadTarget) {
            $items.Add("Compare torso and head attempts for this armored or protected target.")
        }
    }

    $items.Add("Analyze the latest log after the step to capture the correct weapon-filtered evidence.")
    return $items.ToArray()
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
        $SessionInfo,
        [Parameter(Mandatory = $true)]
        [bool]$NoClientMode
    )

    Write-Host ""
    Write-Host ("Matrix step {0}/{1}" -f $StepNumber, $TotalSteps)
    Write-Host "  name          : $($Step.Name)"
    Write-Host "  matrix        : $($SessionInfo.MatrixName)"
    Write-Host "  session tag   : $($SessionInfo.SessionTag)"
    Write-Host "  weapon        : $($Step.WeaponUnderTest)"
    Write-Host "  profile       : $($Step.WeaponProfileName)"
    Write-Host "  target profile: $(if ([string]::IsNullOrWhiteSpace($Step.LabTargetProfileName)) { 'n/a' } else { $Step.LabTargetProfileName })"
    Write-Host "  map           : $($Step.Map)"
    Write-Host "  connect       : $($SessionInfo.ConnectAddress)"
    Write-Host "  server log    : $($SessionInfo.ServerLogPath)"
    Write-Host "  weapon log    : $(if ([string]::IsNullOrWhiteSpace($SessionInfo.WeaponLogPath)) { 'pending' } else { $SessionInfo.WeaponLogPath })"
    if (-not [string]::IsNullOrWhiteSpace($Step.OperatorNote)) {
        Write-Host "  operator note : $($Step.OperatorNote)"
    }

    Write-Host ""
    Write-Host "Checklist"
    $checklistItems = @(Get-WeaponStepChecklistItems -Step $Step -NoClientMode:$NoClientMode)
    for ($index = 0; $index -lt $checklistItems.Count; $index += 1) {
        Write-Host ("{0}. {1}" -f ($index + 1), $checklistItems[$index])
    }

    Write-Host ""
    Write-Host "Enter = analyze step, S = skip firing but still export artifacts, Q = abort after this step."
    Write-Host "This runner structures manual comparison passes. It does not prove gameplay balance or subjective weapon feel by itself."
}

function Start-ComparisonMatrixStepSession {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Configuration,
        [Parameter(Mandatory = $true)]
        [int]$Port,
        [Parameter(Mandatory = $true)]
        $Step,
        [Parameter(Mandatory = $true)]
        [string]$SessionTag,
        [Parameter(Mandatory = $true)]
        [string]$MatrixName,
        [Parameter(Mandatory = $true)]
        [bool]$NoTailMode
    )

    $sessionArgs = @{
        Configuration = $Configuration
        Port = $Port
        Map = $Step.Map
        SetCvar = @($Step.Assignments)
        SessionTag = $SessionTag
        MatrixName = $MatrixName
        MatrixStep = $Step.Name
        NoClient = $true
        NoTail = $NoTailMode
        SkipChecklist = $true
        PassThru = $true
    }

    if (-not [string]::IsNullOrWhiteSpace($Step.LabTargetProfileName)) {
        $sessionArgs["LabTargetProfile"] = $Step.LabTargetProfileName
        $sessionArgs["LabDummy"] = $true
    }

    if ($Step.WeaponUnderTest -eq "mp5") {
        $sessionArgs["Mp5Profile"] = $Step.Mp5ProfileName
        return (& "$PSScriptRoot\run-mp5-test-session.ps1" @sessionArgs)
    }

    $sessionArgs["GlockProfile"] = $Step.GlockProfileName
    return (& "$PSScriptRoot\run-glock-test-session.ps1" @sessionArgs)
}

if ($PortBase -lt 1) {
    throw "PortBase must be at least 1."
}

if ($MaxSteps -lt 0) {
    throw "MaxSteps cannot be negative."
}

$matrixDefinition = Get-WeaponComparisonMatrix -Name $Matrix
$steps = @($matrixDefinition.Steps)
if ($MaxSteps -gt 0) {
    $steps = @($steps | Select-Object -First $MaxSteps)
}

if ($steps.Count -eq 0) {
    throw "Matrix '$Matrix' did not produce any runnable steps."
}

$configuration = Get-ValidatedConfiguration -Configuration $Configuration
$runStamp = Get-Date -Format "yyyyMMdd-HHmmss"
$matrixOutputRoot = Join-Path (Get-WeaponComparisonReportsRoot) ("{0}-{1}" -f $matrixDefinition.Name, $runStamp)
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
    $stepFolder = Join-Path $stepOutputRoot ("{0:D2}-{1}" -f $stepNumber, $step.Name)
    Ensure-Directory -Path $stepFolder

    $sessionTag = Get-StepSessionTag -MatrixName $matrixDefinition.Name -RunStamp $runStamp -StepNumber $stepNumber -StepName $step.Name
    $sessionInfo = $null

    try {
        $sessionInfo = Start-ComparisonMatrixStepSession -Configuration $configuration -Port $currentPort -Step $step -SessionTag $sessionTag -MatrixName $matrixDefinition.Name -NoTailMode:$NoTail
        $currentPort = $sessionInfo.Port

        if (-not $NoClient -and -not $AutoAdvance -and -not $clientLaunched) {
            $clientExe = Get-TestbedSessionClientExe -RuntimeRoot $sessionInfo.RuntimeRoot
            if ($clientExe) {
                & "$PSScriptRoot\run-client.ps1" -ConnectAddress $sessionInfo.ConnectAddress -HlExe $clientExe
                $clientLaunched = $true
                Write-Step "Launched the stock Half-Life client once for the matrix run; reuse that window for the remaining steps"
            }
            else {
                Write-Host "No stock Half-Life client executable was found for matrix step $($step.Name). Continue with a manual client launch or rerun with -NoClient."
            }
        }
        elseif (-not $NoClient -and -not $AutoAdvance -and $clientLaunched) {
            Write-Host "Reuse the existing stock client window and reconnect to $($sessionInfo.ConnectAddress) if the previous server restart dropped the connection."
        }
        elseif ($AutoAdvance -and -not $NoClient) {
            Write-Host "AutoAdvance is enabled, so no stock client launch was requested for matrix step $($step.Name)."
        }

        Write-MatrixStepChecklist -StepNumber $stepNumber -TotalSteps $steps.Count -Step $step -SessionInfo $sessionInfo -NoClientMode:$NoClient

        $stepStatus = if ($AutoAdvance) { "auto_advanced" } else { "completed" }
        if ($AutoAdvance) {
            Write-Host "AutoAdvance is enabled. This step will be analyzed immediately, which is suitable for tooling verification but not for live gameplay evidence."
        }
        else {
            $operatorChoice = Read-Host "Step action"
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

        $analysisArgs = @{
            Weapon = $step.WeaponUnderTest
            ExportJson = $true
            ExportCsv = $true
            OutputDir = $stepFolder
            PassThru = $true
        }

        if (-not [string]::IsNullOrWhiteSpace($sessionInfo.WeaponLogPath) -and (Test-LeafPath -Path $sessionInfo.WeaponLogPath)) {
            $analysisArgs["Path"] = $sessionInfo.WeaponLogPath
        }
        else {
            $analysisArgs["Latest"] = $true
        }

        $analysisResult = & "$PSScriptRoot\analyze-weapon-log.ps1" @analysisArgs
        if ($analysisResult.Exports -and -not [string]::IsNullOrWhiteSpace($analysisResult.Exports.json)) {
            $stepReportPaths.Add($analysisResult.Exports.json)
        }

        $stepResults.Add([PSCustomObject]@{
            StepNumber = $stepNumber
            StepName = $step.Name
            WeaponUnderTest = $step.WeaponUnderTest
            WeaponProfile = $step.WeaponProfileName
            LabTargetProfile = $step.LabTargetProfileName
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
    -LatestReportKind mixed `
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
Write-Host "This tooling verified launch, tagging, and report aggregation. Fresh gameplay evidence still requires manual firing in the stock client."

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
