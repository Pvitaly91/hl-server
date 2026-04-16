[CmdletBinding()]
param(
    [string]$ReportDir,
    [string[]]$ReportPaths = @(),
    [switch]$ExportJson,
    [switch]$ExportCsv,
    [switch]$ExportMarkdown,
    [string]$OutputDir,
    [ValidateSet("MatrixStep", "SessionTag", "WeaponUnderTest", "WeaponProfile", "LabTargetProfile", "AcceptedShotCount", "DummyHitCount", "DummyKillCount", "AppliedDamageAverage")]
    [string]$SortBy = "MatrixStep",
    [ValidateSet("auto", "glock", "mixed")]
    [string]$LatestReportKind = "auto",
    [switch]$PassThru
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"
Import-HLServerEnv

function To-ReportInt {
    param($Value)

    if ($null -eq $Value -or [string]::IsNullOrWhiteSpace([string]$Value)) {
        return 0
    }

    return [int]$Value
}

function To-ReportNum {
    param($Value)

    if ($null -eq $Value -or [string]::IsNullOrWhiteSpace([string]$Value)) {
        return $null
    }

    return [double]$Value
}

function To-ReportBool {
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
        "0" { return $false }
        "true" { return $true }
        "false" { return $false }
        "yes" { return $true }
        "no" { return $false }
        default { return $false }
    }
}

function Format-Number {
    param(
        [double]$Value,
        [int]$Digits = 2
    )

    if ($null -eq $Value) {
        return "n/a"
    }

    return $Value.ToString(("F{0}" -f $Digits), [Globalization.CultureInfo]::InvariantCulture)
}

function Escape-MarkdownTableValue {
    param(
        [string]$Value
    )

    if ([string]::IsNullOrWhiteSpace($Value)) {
        return "n/a"
    }

    return $Value.Replace("|", "/").Replace("`r", " ").Replace("`n", " ")
}

function Get-LatestComparisonInputDirectory {
    param(
        [Parameter(Mandatory = $true)]
        [ValidateSet("auto", "glock", "mixed")]
        [string]$Kind
    )

    $latestGlock = Get-LatestGlockComparisonReportDirectory
    $latestMixed = Get-LatestWeaponComparisonReportDirectory

    switch ($Kind) {
        "glock" { return $latestGlock }
        "mixed" { return $latestMixed }
        default {
            if ($latestGlock -and $latestMixed) {
                if ($latestMixed.LastWriteTimeUtc -ge $latestGlock.LastWriteTimeUtc) {
                    return $latestMixed
                }

                return $latestGlock
            }

            if ($latestMixed) {
                return $latestMixed
            }

            return $latestGlock
        }
    }
}

function Get-DefaultComparisonOutputDirectory {
    param(
        [Parameter(Mandatory = $true)]
        [ValidateSet("auto", "glock", "mixed")]
        [string]$Kind
    )

    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $root = if ($Kind -eq "glock") { Get-GlockComparisonReportsRoot } else { Get-WeaponComparisonReportsRoot }
    return (Join-Path $root ("compare-" + $stamp))
}

function Resolve-ComparisonInputPaths {
    $resolved = New-Object System.Collections.Generic.List[string]
    $seen = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::OrdinalIgnoreCase)
    $effectiveReportDir = $ReportDir

    if ([string]::IsNullOrWhiteSpace($effectiveReportDir) -and ($null -eq $ReportPaths -or $ReportPaths.Count -eq 0)) {
        $latestReportDir = Get-LatestComparisonInputDirectory -Kind $LatestReportKind
        if ($latestReportDir) {
            $effectiveReportDir = $latestReportDir.FullName
            $kindLabel = if ($latestReportDir.FullName.StartsWith((Get-FullPath -Path (Get-WeaponComparisonReportsRoot)), [System.StringComparison]::OrdinalIgnoreCase)) {
                "mixed weapon"
            }
            else {
                "Glock"
            }

            Write-Step "No report input was specified; using the latest $kindLabel comparison report directory at $effectiveReportDir"
        }
    }

    if (-not [string]::IsNullOrWhiteSpace($effectiveReportDir)) {
        $resolvedReportDir = Get-FullPath -Path $effectiveReportDir
        if (-not (Test-Path -LiteralPath $resolvedReportDir -PathType Container)) {
            throw "Comparison report directory was not found: $resolvedReportDir"
        }

        foreach ($reportFile in (Get-ChildItem -LiteralPath $resolvedReportDir -Recurse -File -Filter "*.json" | Sort-Object FullName)) {
            if ($seen.Add($reportFile.FullName)) {
                $resolved.Add($reportFile.FullName)
            }
        }
    }

    foreach ($reportPath in @($ReportPaths)) {
        if ([string]::IsNullOrWhiteSpace($reportPath)) {
            continue
        }

        $resolvedPath = Get-FullPath -Path $reportPath
        if (-not (Test-LeafPath -Path $resolvedPath)) {
            throw "Comparison report file was not found: $resolvedPath"
        }

        if ($seen.Add($resolvedPath)) {
            $resolved.Add($resolvedPath)
        }
    }

    if ($resolved.Count -eq 0) {
        throw "No comparison report inputs were found. Pass -ReportDir or -ReportPaths, or generate a comparison report folder first."
    }

    return $resolved.ToArray()
}

function Test-AnalyzerReportShape {
    param(
        [Parameter(Mandatory = $true)]
        $Report
    )

    return ($null -ne (Get-ObjectPropertyValue -InputObject $Report -Names @("comparisonSummary"))) -or
        ($null -ne (Get-ObjectPropertyValue -InputObject $Report -Names @("metadata"))) -or
        ($null -ne (Get-ObjectPropertyValue -InputObject $Report -Names @("counters"))) -or
        ($null -ne (Get-ObjectPropertyValue -InputObject $Report -Names @("session")))
}

function Get-ComparisonString {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Names,
        [object[]]$InputObjects = @()
    )

    foreach ($inputObject in @($InputObjects)) {
        $value = Get-OptionalObjectString -InputObject $inputObject -Names $Names
        if (-not [string]::IsNullOrWhiteSpace($value)) {
            return $value
        }
    }

    return $null
}

function Get-ComparisonBool {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Names,
        [object[]]$InputObjects = @()
    )

    foreach ($inputObject in @($InputObjects)) {
        $value = Get-ObjectPropertyValue -InputObject $inputObject -Names $Names
        if ($null -ne $value) {
            return (To-ReportBool $value)
        }
    }

    return $false
}

function Get-ComparisonInt {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Names,
        [object[]]$InputObjects = @()
    )

    foreach ($inputObject in @($InputObjects)) {
        $value = Get-ObjectPropertyValue -InputObject $inputObject -Names $Names
        if ($null -ne $value -and -not [string]::IsNullOrWhiteSpace([string]$value)) {
            return (To-ReportInt $value)
        }
    }

    return 0
}

function Get-ComparisonNumber {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Names,
        [object[]]$InputObjects = @()
    )

    foreach ($inputObject in @($InputObjects)) {
        $value = Get-ObjectPropertyValue -InputObject $inputObject -Names $Names
        if ($null -ne $value -and -not [string]::IsNullOrWhiteSpace([string]$value)) {
            return (To-ReportNum $value)
        }
    }

    return $null
}

function ConvertTo-ComparisonRow {
    param(
        [Parameter(Mandatory = $true)]
        $Report,
        [Parameter(Mandatory = $true)]
        [string]$SourcePath
    )

    $summary = Get-ObjectPropertyValue -InputObject $Report -Names @("comparisonSummary")
    $metadata = Get-ObjectPropertyValue -InputObject $Report -Names @("metadata")
    $session = Get-ObjectPropertyValue -InputObject $Report -Names @("session")
    $counters = Get-ObjectPropertyValue -InputObject $Report -Names @("counters")
    $signals = Get-ObjectPropertyValue -InputObject $Report -Names @("signals")
    $stats = Get-ObjectPropertyValue -InputObject $Report -Names @("stats")
    $appliedDamageSummary = Get-ObjectPropertyValue -InputObject $summary -Names @("appliedDamage")
    $appliedDamageStats = Get-ObjectPropertyValue -InputObject $stats -Names @("appliedDamage")
    $evidenceSummary = Get-ObjectPropertyValue -InputObject $summary -Names @("evidence")

    $weaponUnderTest = Get-ComparisonString -Names @("weaponUnderTest") -InputObjects @($summary, $metadata, $session)
    $glockProfile = Get-ComparisonString -Names @("glockProfile", "glockProfileName") -InputObjects @($summary, $metadata, $session)
    $mp5Profile = Get-ComparisonString -Names @("mp5Profile", "mp5ProfileName") -InputObjects @($summary, $metadata, $session)
    $weaponProfile = Get-ComparisonString -Names @("weaponProfile", "weaponProfileName") -InputObjects @($summary, $metadata, $session)

    if ([string]::IsNullOrWhiteSpace($weaponUnderTest)) {
        if (-not [string]::IsNullOrWhiteSpace($mp5Profile)) {
            $weaponUnderTest = "mp5"
        }
        elseif (-not [string]::IsNullOrWhiteSpace($glockProfile)) {
            $weaponUnderTest = "glock"
        }
    }

    if ([string]::IsNullOrWhiteSpace($weaponProfile)) {
        if ($weaponUnderTest -eq "mp5") {
            $weaponProfile = $mp5Profile
        }
        elseif ($weaponUnderTest -eq "glock") {
            $weaponProfile = $glockProfile
        }
        elseif (-not [string]::IsNullOrWhiteSpace($mp5Profile)) {
            $weaponProfile = $mp5Profile
        }
        elseif (-not [string]::IsNullOrWhiteSpace($glockProfile)) {
            $weaponProfile = $glockProfile
        }
        else {
            $weaponProfile = Get-ComparisonString -Names @("profileName") -InputObjects @($session)
        }
    }

    $missingSignalNotes = @()
    $rawMissingSignalNotes = Get-ObjectPropertyValue -InputObject $summary -Names @("missingSignalNotes")
    foreach ($entry in @($rawMissingSignalNotes)) {
        if (-not [string]::IsNullOrWhiteSpace([string]$entry)) {
            $missingSignalNotes += ([string]$entry)
        }
    }

    $row = [PSCustomObject]@{
        SourceReportPath = $SourcePath
        WeaponUnderTest = $weaponUnderTest
        WeaponProfile = $weaponProfile
        SessionTag = Get-ComparisonString -Names @("sessionTag") -InputObjects @($summary, $metadata, $session)
        MatrixName = Get-ComparisonString -Names @("matrixName") -InputObjects @($summary, $metadata, $session)
        MatrixStep = Get-ComparisonString -Names @("matrixStep") -InputObjects @($summary, $metadata, $session)
        GlockProfile = $glockProfile
        Mp5Profile = $mp5Profile
        LabTargetProfile = Get-ComparisonString -Names @("labTargetProfile", "targetProfileName") -InputObjects @($summary, $metadata, $session)
        AcceptedShotCount = Get-ComparisonInt -Names @("acceptedShotCount", "acceptedShots") -InputObjects @($summary, $counters)
        RejectedShotCount = Get-ComparisonInt -Names @("rejectedShotCount", "rejectedShots") -InputObjects @($summary, $counters)
        HitCount = Get-ComparisonInt -Names @("hitCount", "hitEvents") -InputObjects @($summary, $counters)
        KillCount = Get-ComparisonInt -Names @("killCount", "killEvents") -InputObjects @($summary, $counters)
        DummyHitCount = Get-ComparisonInt -Names @("dummyHitCount", "dummyHits") -InputObjects @($summary, $counters)
        DummyKillCount = Get-ComparisonInt -Names @("dummyKillCount", "dummyKills") -InputObjects @($summary, $counters)
        DummyHeadshotHitCount = Get-ComparisonInt -Names @("dummyHeadshotHitCount", "dummyHeadshotHits") -InputObjects @($summary, $counters)
        DummyHeadshotKillCount = Get-ComparisonInt -Names @("dummyHeadshotKillCount", "dummyHeadshotKills") -InputObjects @($summary, $counters)
        LethalHeadshotEvidenceCount = Get-ComparisonInt -Names @("lethalHeadshotEvidenceCount", "lethalHeadshotEvidence") -InputObjects @($summary, $counters)
        DummyLethalHeadshotEvidenceCount = Get-ComparisonInt -Names @("dummyLethalHeadshotEvidenceCount", "dummyLethalHeadshotEvidence") -InputObjects @($summary, $counters)
        ArmoredDummyHitCount = Get-ComparisonInt -Names @("armoredDummyHitCount", "armoredDummyHits") -InputObjects @($summary, $counters)
        ProtectedHeadDummyHeadshotHitCount = Get-ComparisonInt -Names @("protectedHeadDummyHeadshotHitCount", "protectedDummyHeadshotHitCount", "protectedDummyHeadshotHits") -InputObjects @($summary, $counters)
        ProtectedHeadDummyHeadshotKillCount = Get-ComparisonInt -Names @("protectedHeadDummyHeadshotKillCount", "protectedDummyHeadshotKillCount", "protectedDummyHeadshotKills") -InputObjects @($summary, $counters)
        BurstGrowthEvidenceCount = Get-ComparisonInt -Names @("burstGrowthEvidenceCount", "burstGrowthEvidence") -InputObjects @($summary, $counters)
        MovementPenaltyEvidenceCount = Get-ComparisonInt -Names @("movementPenaltyEvidenceCount", "acceptedMovePenaltyPositive") -InputObjects @($summary, $counters)
        AppliedDamageMin = Get-ComparisonNumber -Names @("min", "Min") -InputObjects @($appliedDamageSummary, $appliedDamageStats)
        AppliedDamageAverage = Get-ComparisonNumber -Names @("average", "Average") -InputObjects @($appliedDamageSummary, $appliedDamageStats)
        AppliedDamageMax = Get-ComparisonNumber -Names @("max", "Max") -InputObjects @($appliedDamageSummary, $appliedDamageStats)
        TapFireRejectionEvidence = Get-ComparisonBool -Names @("tapFireRejectionEvidence", "tapFireRejection") -InputObjects @($evidenceSummary)
        BurstGrowthEvidence = Get-ComparisonBool -Names @("burstGrowthEvidence", "burstGrowth") -InputObjects @($evidenceSummary)
        MovementPenaltyEvidence = Get-ComparisonBool -Names @("movementPenaltyEvidence", "movementPenalty") -InputObjects @($evidenceSummary)
        DummyHeadshotPathEvidence = Get-ComparisonBool -Names @("dummyHeadshotPathEvidence", "dummyHeadshotPath") -InputObjects @($evidenceSummary)
        ArmoredDummyEvidence = Get-ComparisonBool -Names @("armoredDummyEvidence", "armoredDummyPath") -InputObjects @($evidenceSummary)
        ProtectedHeadDummyEvidence = Get-ComparisonBool -Names @("protectedHeadDummyEvidence", "protectedHeadDummyPath") -InputObjects @($evidenceSummary)
        LethalHeadshotEvidence = Get-ComparisonBool -Names @("lethalHeadshotEvidence", "lethalHeadshotPath") -InputObjects @($evidenceSummary)
        MissingSignalNotes = @($missingSignalNotes)
    }

    if (-not $row.TapFireRejectionEvidence) {
        $row.TapFireRejectionEvidence = Get-ComparisonBool -Names @("tapFireHoldRejections") -InputObjects @($signals)
    }
    if (-not $row.BurstGrowthEvidence) {
        $row.BurstGrowthEvidence = Get-ComparisonBool -Names @("burstGrowthEvidencePresent") -InputObjects @($signals)
        if (-not $row.BurstGrowthEvidence) {
            $row.BurstGrowthEvidence = $row.BurstGrowthEvidenceCount -gt 0
        }
    }
    if (-not $row.MovementPenaltyEvidence) {
        $row.MovementPenaltyEvidence = Get-ComparisonBool -Names @("movementPenaltyPositive", "movementPenaltyEvidencePresent") -InputObjects @($signals)
        if (-not $row.MovementPenaltyEvidence) {
            $row.MovementPenaltyEvidence = $row.MovementPenaltyEvidenceCount -gt 0
        }
    }
    if (-not $row.DummyHeadshotPathEvidence) {
        $row.DummyHeadshotPathEvidence = Get-ComparisonBool -Names @("dummyHeadshotHitsPresent") -InputObjects @($signals)
    }
    if (-not $row.ArmoredDummyEvidence) {
        $row.ArmoredDummyEvidence = Get-ComparisonBool -Names @("armoredDummyHitsPresent") -InputObjects @($signals)
        if (-not $row.ArmoredDummyEvidence) {
            $row.ArmoredDummyEvidence = $row.ArmoredDummyHitCount -gt 0
        }
    }
    if (-not $row.ProtectedHeadDummyEvidence) {
        $row.ProtectedHeadDummyEvidence = Get-ComparisonBool -Names @("protectedDummyHeadshotHitsPresent") -InputObjects @($signals)
        if (-not $row.ProtectedHeadDummyEvidence) {
            $row.ProtectedHeadDummyEvidence = $row.ProtectedHeadDummyHeadshotHitCount -gt 0
        }
    }
    if (-not $row.LethalHeadshotEvidence) {
        $row.LethalHeadshotEvidence = Get-ComparisonBool -Names @("dummyLethalHeadshotEvidencePresent", "lethalHeadshotEvidencePresent") -InputObjects @($signals)
        if (-not $row.LethalHeadshotEvidence) {
            $row.LethalHeadshotEvidence = ($row.LethalHeadshotEvidenceCount -gt 0) -or ($row.DummyLethalHeadshotEvidenceCount -gt 0)
        }
    }

    if ($row.MissingSignalNotes.Count -eq 0) {
        if ($row.WeaponUnderTest -eq "glock" -and -not $row.TapFireRejectionEvidence) {
            $row.MissingSignalNotes += "No tap-fire rejection evidence was logged."
        }
        if ($row.WeaponUnderTest -eq "mp5" -and -not $row.BurstGrowthEvidence) {
            $row.MissingSignalNotes += "No burst-growth evidence was logged."
        }
        if (-not $row.MovementPenaltyEvidence) {
            $row.MissingSignalNotes += "No movement-penalty evidence was logged."
        }
        if (-not $row.DummyHeadshotPathEvidence) {
            $row.MissingSignalNotes += "No dummy headshot path evidence was logged."
        }
        if (-not $row.ArmoredDummyEvidence) {
            $row.MissingSignalNotes += "No armored dummy evidence was logged."
        }
        if (-not $row.ProtectedHeadDummyEvidence) {
            $row.MissingSignalNotes += "No protected-head dummy evidence was logged."
        }
        if (-not $row.LethalHeadshotEvidence) {
            $row.MissingSignalNotes += "No lethal-headshot evidence was logged."
        }
    }

    $row | Add-Member -NotePropertyName ProtectedDummyHeadshotHitCount -NotePropertyValue $row.ProtectedHeadDummyHeadshotHitCount
    $row | Add-Member -NotePropertyName ProtectedDummyHeadshotKillCount -NotePropertyValue $row.ProtectedHeadDummyHeadshotKillCount
    $row | Add-Member -NotePropertyName DummyHeadshotEvidence -NotePropertyValue $row.DummyHeadshotPathEvidence
    $row | Add-Member -NotePropertyName MissingSignalsText -NotePropertyValue (($row.MissingSignalNotes -join " ").Trim())

    $evidenceSummary = if ($row.WeaponUnderTest -eq "mp5") {
        "burst:{0} move:{1} dummy:{2} armor:{3} prot:{4} lethal:{5}" -f
            $(if ($row.BurstGrowthEvidence) { "Y" } else { "N" }),
            $(if ($row.MovementPenaltyEvidence) { "Y" } else { "N" }),
            $(if ($row.DummyHeadshotPathEvidence) { "Y" } else { "N" }),
            $(if ($row.ArmoredDummyEvidence) { "Y" } else { "N" }),
            $(if ($row.ProtectedHeadDummyEvidence) { "Y" } else { "N" }),
            $(if ($row.LethalHeadshotEvidence) { "Y" } else { "N" })
    }
    elseif ($row.WeaponUnderTest -eq "glock") {
        "tap:{0} move:{1} dummy:{2} armor:{3} prot:{4} lethal:{5}" -f
            $(if ($row.TapFireRejectionEvidence) { "Y" } else { "N" }),
            $(if ($row.MovementPenaltyEvidence) { "Y" } else { "N" }),
            $(if ($row.DummyHeadshotPathEvidence) { "Y" } else { "N" }),
            $(if ($row.ArmoredDummyEvidence) { "Y" } else { "N" }),
            $(if ($row.ProtectedHeadDummyEvidence) { "Y" } else { "N" }),
            $(if ($row.LethalHeadshotEvidence) { "Y" } else { "N" })
    }
    else {
        "move:{0} dummy:{1} armor:{2} prot:{3} lethal:{4}" -f
            $(if ($row.MovementPenaltyEvidence) { "Y" } else { "N" }),
            $(if ($row.DummyHeadshotPathEvidence) { "Y" } else { "N" }),
            $(if ($row.ArmoredDummyEvidence) { "Y" } else { "N" }),
            $(if ($row.ProtectedHeadDummyEvidence) { "Y" } else { "N" }),
            $(if ($row.LethalHeadshotEvidence) { "Y" } else { "N" })
    }

    $row | Add-Member -NotePropertyName EvidenceSummary -NotePropertyValue $evidenceSummary
    return $row
}

$inputPaths = @(Resolve-ComparisonInputPaths)
$rows = New-Object System.Collections.Generic.List[object]
$skippedInputs = New-Object System.Collections.Generic.List[string]

foreach ($inputPath in $inputPaths) {
    try {
        $report = Get-Content -LiteralPath $inputPath -Raw | ConvertFrom-Json
    }
    catch {
        throw "Failed to parse comparison report input '$inputPath': $($_.Exception.Message)"
    }

    if (-not (Test-AnalyzerReportShape -Report $report)) {
        $skippedInputs.Add($inputPath)
        continue
    }

    $rows.Add((ConvertTo-ComparisonRow -Report $report -SourcePath $inputPath))
}

if ($rows.Count -eq 0) {
    $skippedText = if ($skippedInputs.Count -gt 0) { ($skippedInputs -join ", ") } else { "none" }
    throw "No analyzer-style comparison inputs were found. Skipped files: $skippedText"
}

$sortedRows = @($rows | Sort-Object $SortBy, MatrixName, MatrixStep, SessionTag, SourceReportPath)

Write-Host "Weapon comparison summary"
Write-Host "  source reports : $($sortedRows.Count)"
Write-Host "  sort           : $SortBy"

$tableRows = $sortedRows | Select-Object `
    @{Name = "Step"; Expression = { if ([string]::IsNullOrWhiteSpace($_.MatrixStep)) { "n/a" } else { $_.MatrixStep } } }, `
    @{Name = "Wpn"; Expression = { if ([string]::IsNullOrWhiteSpace($_.WeaponUnderTest)) { "n/a" } else { $_.WeaponUnderTest } } }, `
    @{Name = "Prof"; Expression = { if ([string]::IsNullOrWhiteSpace($_.WeaponProfile)) { "n/a" } else { $_.WeaponProfile } } }, `
    @{Name = "Tgt"; Expression = { if ([string]::IsNullOrWhiteSpace($_.LabTargetProfile)) { "n/a" } else { $_.LabTargetProfile } } }, `
    @{Name = "Acc"; Expression = { $_.AcceptedShotCount } }, `
    @{Name = "Rej"; Expression = { $_.RejectedShotCount } }, `
    @{Name = "Hit"; Expression = { $_.HitCount } }, `
    @{Name = "Kill"; Expression = { $_.KillCount } }, `
    @{Name = "DHit"; Expression = { $_.DummyHitCount } }, `
    @{Name = "DKill"; Expression = { $_.DummyKillCount } }, `
    @{Name = "Leth"; Expression = { $_.LethalHeadshotEvidenceCount } }, `
    @{Name = "Sig"; Expression = { $_.EvidenceSummary } }

$formattedTable = ($tableRows | Format-Table -AutoSize | Out-String).TrimEnd()
if (-not [string]::IsNullOrWhiteSpace($formattedTable)) {
    Write-Host $formattedTable
}

$rowsWithMissingSignals = @($sortedRows | Where-Object { -not [string]::IsNullOrWhiteSpace($_.MissingSignalsText) })
if ($rowsWithMissingSignals.Count -gt 0) {
    Write-Host ""
    Write-Host "Missing signal notes"
    foreach ($row in $rowsWithMissingSignals) {
        $rowLabel = if (-not [string]::IsNullOrWhiteSpace($row.MatrixStep)) {
            $row.MatrixStep
        }
        elseif (-not [string]::IsNullOrWhiteSpace($row.SessionTag)) {
            $row.SessionTag
        }
        else {
            Split-Path -Leaf $row.SourceReportPath
        }

        Write-Host "  - ${rowLabel}: $($row.MissingSignalsText)"
    }
}

if ($skippedInputs.Count -gt 0) {
    Write-Host ""
    Write-Host "Skipped non-analyzer JSON inputs"
    foreach ($path in $skippedInputs) {
        Write-Host "  - $path"
    }
}

$exports = [ordered]@{}
if ($ExportJson -or $ExportCsv -or $ExportMarkdown) {
    $resolvedOutputDir = if ([string]::IsNullOrWhiteSpace($OutputDir)) {
        Get-DefaultComparisonOutputDirectory -Kind $LatestReportKind
    }
    else {
        Get-FullPath -Path $OutputDir
    }

    Ensure-Directory -Path $resolvedOutputDir
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"

    if ($ExportJson) {
        $jsonPath = Join-Path $resolvedOutputDir ("weapon-comparison-summary-" + $stamp + ".json")
        ([ordered]@{
            generatedAt = (Get-Date).ToString("s")
            sourceReportCount = $sortedRows.Count
            sourceReports = @($sortedRows | Select-Object -ExpandProperty SourceReportPath)
            rows = @($sortedRows)
            skippedInputs = @($skippedInputs)
        } | ConvertTo-Json -Depth 10) | Set-Content -LiteralPath $jsonPath -Encoding UTF8
        $exports.json = $jsonPath
    }

    if ($ExportCsv) {
        $csvPath = Join-Path $resolvedOutputDir ("weapon-comparison-summary-" + $stamp + ".csv")
        $sortedRows | Export-Csv -LiteralPath $csvPath -NoTypeInformation -Encoding UTF8
        $exports.csv = $csvPath
    }

    if ($ExportMarkdown) {
        $markdownPath = Join-Path $resolvedOutputDir ("weapon-comparison-summary-" + $stamp + ".md")
        $markdownLines = New-Object System.Collections.Generic.List[string]
        $markdownLines.Add("# Weapon comparison summary")
        $markdownLines.Add("")
        $markdownLines.Add(("Generated: {0}" -f (Get-Date).ToString("s")))
        $markdownLines.Add(("Source report count: {0}" -f $sortedRows.Count))
        $markdownLines.Add("")
        $markdownLines.Add("| Matrix step | Weapon | Weapon profile | Target profile | Accepted | Rejected | Hits | Kills | Dummy hits | Dummy kills | Lethal evidence | Signals | Missing notes |")
        $markdownLines.Add("| --- | --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- |")

        foreach ($row in $sortedRows) {
            $markdownLines.Add((
                "| {0} | {1} | {2} | {3} | {4} | {5} | {6} | {7} | {8} | {9} | {10} | {11} | {12} |" -f
                (Escape-MarkdownTableValue $row.MatrixStep),
                (Escape-MarkdownTableValue $row.WeaponUnderTest),
                (Escape-MarkdownTableValue $row.WeaponProfile),
                (Escape-MarkdownTableValue $row.LabTargetProfile),
                $row.AcceptedShotCount,
                $row.RejectedShotCount,
                $row.HitCount,
                $row.KillCount,
                $row.DummyHitCount,
                $row.DummyKillCount,
                $row.LethalHeadshotEvidenceCount,
                (Escape-MarkdownTableValue $row.EvidenceSummary),
                (Escape-MarkdownTableValue $row.MissingSignalsText)
            ))
        }

        if ($skippedInputs.Count -gt 0) {
            $markdownLines.Add("")
            $markdownLines.Add("Skipped non-analyzer JSON inputs:")
            foreach ($path in $skippedInputs) {
                $markdownLines.Add(("- " + $path))
            }
        }

        Set-Content -LiteralPath $markdownPath -Value $markdownLines -Encoding UTF8
        $exports.markdown = $markdownPath
    }
}

if ($exports.Count -gt 0) {
    Write-Host ""
    Write-Host "Comparison exports"
    foreach ($key in $exports.Keys) {
        Write-Host "  $key : $($exports[$key])"
    }
}

if ($PassThru) {
    return [PSCustomObject]@{
        Rows = @($sortedRows)
        SkippedInputs = @($skippedInputs)
        Exports = [PSCustomObject]$exports
    }
}
