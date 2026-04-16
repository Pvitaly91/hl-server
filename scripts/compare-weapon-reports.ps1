[CmdletBinding()]
param(
    [string]$ReportDir,
    [string[]]$ReportPaths,
    [switch]$ExportJson,
    [switch]$ExportCsv,
    [switch]$ExportMarkdown,
    [string]$OutputDir,
    [ValidateSet("MatrixStep", "SessionTag", "GlockProfile", "LabTargetProfile", "AcceptedShotCount", "DummyHitCount", "DummyKillCount", "AppliedDamageAverage")]
    [string]$SortBy = "MatrixStep",
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

function Format-Number {
    param(
        $Value,
        [int]$Digits = 2
    )

    if ($null -eq $Value) {
        return "n/a"
    }

    return ([double]$Value).ToString(("F{0}" -f $Digits), [System.Globalization.CultureInfo]::InvariantCulture)
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

function Get-DefaultComparisonOutputDirectory {
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    return (Join-Path (Get-GlockComparisonReportsRoot) ("compare-" + $stamp))
}

function Resolve-ComparisonInputPaths {
    $resolved = New-Object System.Collections.Generic.List[string]
    $seen = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::OrdinalIgnoreCase)

    $effectiveReportDir = $ReportDir
    if ([string]::IsNullOrWhiteSpace($effectiveReportDir) -and ($null -eq $ReportPaths -or $ReportPaths.Count -eq 0)) {
        $latestReportDir = Get-LatestGlockComparisonReportDirectory
        if ($latestReportDir) {
            $effectiveReportDir = $latestReportDir.FullName
            Write-Step "No report input was specified; using the latest Glock comparison report directory at $effectiveReportDir"
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
        throw "No comparison report inputs were found. Pass -ReportDir or -ReportPaths, or generate a matrix report folder first."
    }

    return $resolved.ToArray()
}

function Test-AnalyzerReportShape {
    param($Report)

    if ($null -eq $Report) {
        return $false
    }

    return ($null -ne (Get-ObjectPropertyValue -InputObject $Report -Names @("comparisonSummary"))) -or
        ($null -ne (Get-ObjectPropertyValue -InputObject $Report -Names @("counters"))) -or
        ($null -ne (Get-ObjectPropertyValue -InputObject $Report -Names @("session")))
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

    $row = [PSCustomObject]@{
        SourceReportPath = $SourcePath
        SessionTag = Get-OptionalObjectString -InputObject $summary -Names @("sessionTag")
        MatrixName = Get-OptionalObjectString -InputObject $summary -Names @("matrixName")
        MatrixStep = Get-OptionalObjectString -InputObject $summary -Names @("matrixStep")
        GlockProfile = Get-OptionalObjectString -InputObject $summary -Names @("glockProfile")
        LabTargetProfile = Get-OptionalObjectString -InputObject $summary -Names @("labTargetProfile")
        AcceptedShotCount = To-ReportInt (Get-ObjectPropertyValue -InputObject $summary -Names @("acceptedShotCount"))
        RejectedShotCount = To-ReportInt (Get-ObjectPropertyValue -InputObject $summary -Names @("rejectedShotCount"))
        HitCount = To-ReportInt (Get-ObjectPropertyValue -InputObject $summary -Names @("hitCount"))
        KillCount = To-ReportInt (Get-ObjectPropertyValue -InputObject $summary -Names @("killCount"))
        DummyHitCount = To-ReportInt (Get-ObjectPropertyValue -InputObject $summary -Names @("dummyHitCount"))
        DummyKillCount = To-ReportInt (Get-ObjectPropertyValue -InputObject $summary -Names @("dummyKillCount"))
        DummyHeadshotHitCount = To-ReportInt (Get-ObjectPropertyValue -InputObject $summary -Names @("dummyHeadshotHitCount"))
        DummyHeadshotKillCount = To-ReportInt (Get-ObjectPropertyValue -InputObject $summary -Names @("dummyHeadshotKillCount"))
        DummyLethalHeadshotEvidenceCount = To-ReportInt (Get-ObjectPropertyValue -InputObject $summary -Names @("dummyLethalHeadshotEvidenceCount"))
        ArmoredDummyHitCount = To-ReportInt (Get-ObjectPropertyValue -InputObject $summary -Names @("armoredDummyHitCount"))
        ProtectedDummyHeadshotHitCount = To-ReportInt (Get-ObjectPropertyValue -InputObject $summary -Names @("protectedDummyHeadshotHitCount"))
        ProtectedDummyHeadshotKillCount = To-ReportInt (Get-ObjectPropertyValue -InputObject $summary -Names @("protectedDummyHeadshotKillCount"))
        AppliedDamageMin = To-ReportNum (Get-ObjectPropertyValue -InputObject $appliedDamageSummary -Names @("min"))
        AppliedDamageAverage = To-ReportNum (Get-ObjectPropertyValue -InputObject $appliedDamageSummary -Names @("average"))
        AppliedDamageMax = To-ReportNum (Get-ObjectPropertyValue -InputObject $appliedDamageSummary -Names @("max"))
        TapFireRejectionEvidence = To-ReportBool (Get-ObjectPropertyValue -InputObject $evidenceSummary -Names @("tapFireRejection"))
        MovementPenaltyEvidence = To-ReportBool (Get-ObjectPropertyValue -InputObject $evidenceSummary -Names @("movementPenalty"))
        DummyHeadshotEvidence = To-ReportBool (Get-ObjectPropertyValue -InputObject $evidenceSummary -Names @("dummyHeadshotPath"))
        ArmoredDummyEvidence = To-ReportBool (Get-ObjectPropertyValue -InputObject $evidenceSummary -Names @("armoredDummyPath"))
        ProtectedHeadDummyEvidence = To-ReportBool (Get-ObjectPropertyValue -InputObject $evidenceSummary -Names @("protectedHeadDummyPath"))
        LethalHeadshotEvidence = To-ReportBool (Get-ObjectPropertyValue -InputObject $evidenceSummary -Names @("lethalHeadshotPath"))
        MissingSignalNotes = @((Get-ObjectPropertyValue -InputObject $summary -Names @("missingSignalNotes")))
    }

    if ([string]::IsNullOrWhiteSpace($row.SessionTag)) {
        $row.SessionTag = Get-OptionalObjectString -InputObject $metadata -Names @("sessionTag")
    }
    if ([string]::IsNullOrWhiteSpace($row.MatrixName)) {
        $row.MatrixName = Get-OptionalObjectString -InputObject $metadata -Names @("matrixName")
    }
    if ([string]::IsNullOrWhiteSpace($row.MatrixStep)) {
        $row.MatrixStep = Get-OptionalObjectString -InputObject $metadata -Names @("matrixStep")
    }
    if ([string]::IsNullOrWhiteSpace($row.GlockProfile)) {
        $row.GlockProfile = Get-OptionalObjectString -InputObject $metadata -Names @("glockProfile")
    }
    if ([string]::IsNullOrWhiteSpace($row.LabTargetProfile)) {
        $row.LabTargetProfile = Get-OptionalObjectString -InputObject $metadata -Names @("labTargetProfile")
    }

    if ([string]::IsNullOrWhiteSpace($row.SessionTag)) {
        $row.SessionTag = Get-OptionalObjectString -InputObject $session -Names @("sessionTag")
    }
    if ([string]::IsNullOrWhiteSpace($row.MatrixName)) {
        $row.MatrixName = Get-OptionalObjectString -InputObject $session -Names @("matrixName")
    }
    if ([string]::IsNullOrWhiteSpace($row.MatrixStep)) {
        $row.MatrixStep = Get-OptionalObjectString -InputObject $session -Names @("matrixStep")
    }
    if ([string]::IsNullOrWhiteSpace($row.GlockProfile)) {
        $row.GlockProfile = Get-OptionalObjectString -InputObject $session -Names @("profileName")
    }
    if ([string]::IsNullOrWhiteSpace($row.LabTargetProfile)) {
        $row.LabTargetProfile = Get-OptionalObjectString -InputObject $session -Names @("targetProfileName")
    }

    if ($row.AcceptedShotCount -eq 0) { $row.AcceptedShotCount = To-ReportInt (Get-ObjectPropertyValue -InputObject $counters -Names @("acceptedShots")) }
    if ($row.RejectedShotCount -eq 0) { $row.RejectedShotCount = To-ReportInt (Get-ObjectPropertyValue -InputObject $counters -Names @("rejectedShots")) }
    if ($row.HitCount -eq 0) { $row.HitCount = To-ReportInt (Get-ObjectPropertyValue -InputObject $counters -Names @("hitEvents")) }
    if ($row.KillCount -eq 0) { $row.KillCount = To-ReportInt (Get-ObjectPropertyValue -InputObject $counters -Names @("killEvents")) }
    if ($row.DummyHitCount -eq 0) { $row.DummyHitCount = To-ReportInt (Get-ObjectPropertyValue -InputObject $counters -Names @("dummyHits")) }
    if ($row.DummyKillCount -eq 0) { $row.DummyKillCount = To-ReportInt (Get-ObjectPropertyValue -InputObject $counters -Names @("dummyKills")) }
    if ($row.DummyHeadshotHitCount -eq 0) { $row.DummyHeadshotHitCount = To-ReportInt (Get-ObjectPropertyValue -InputObject $counters -Names @("dummyHeadshotHits")) }
    if ($row.DummyHeadshotKillCount -eq 0) { $row.DummyHeadshotKillCount = To-ReportInt (Get-ObjectPropertyValue -InputObject $counters -Names @("dummyHeadshotKills")) }
    if ($row.DummyLethalHeadshotEvidenceCount -eq 0) { $row.DummyLethalHeadshotEvidenceCount = To-ReportInt (Get-ObjectPropertyValue -InputObject $counters -Names @("dummyLethalHeadshotEvidence")) }
    if ($row.ArmoredDummyHitCount -eq 0) { $row.ArmoredDummyHitCount = To-ReportInt (Get-ObjectPropertyValue -InputObject $counters -Names @("armoredDummyHits")) }
    if ($row.ProtectedDummyHeadshotHitCount -eq 0) { $row.ProtectedDummyHeadshotHitCount = To-ReportInt (Get-ObjectPropertyValue -InputObject $counters -Names @("protectedDummyHeadshotHits")) }
    if ($row.ProtectedDummyHeadshotKillCount -eq 0) { $row.ProtectedDummyHeadshotKillCount = To-ReportInt (Get-ObjectPropertyValue -InputObject $counters -Names @("protectedDummyHeadshotKills")) }

    if ($null -eq $row.AppliedDamageMin) { $row.AppliedDamageMin = To-ReportNum (Get-ObjectPropertyValue -InputObject $appliedDamageStats -Names @("Min")) }
    if ($null -eq $row.AppliedDamageAverage) { $row.AppliedDamageAverage = To-ReportNum (Get-ObjectPropertyValue -InputObject $appliedDamageStats -Names @("Average")) }
    if ($null -eq $row.AppliedDamageMax) { $row.AppliedDamageMax = To-ReportNum (Get-ObjectPropertyValue -InputObject $appliedDamageStats -Names @("Max")) }

    if (-not $row.TapFireRejectionEvidence) { $row.TapFireRejectionEvidence = To-ReportBool (Get-ObjectPropertyValue -InputObject $signals -Names @("tapFireHoldRejections")) }
    if (-not $row.MovementPenaltyEvidence) { $row.MovementPenaltyEvidence = To-ReportBool (Get-ObjectPropertyValue -InputObject $signals -Names @("movementPenaltyPositive")) }
    if (-not $row.DummyHeadshotEvidence) { $row.DummyHeadshotEvidence = To-ReportBool (Get-ObjectPropertyValue -InputObject $signals -Names @("dummyHeadshotHitsPresent")) }
    if (-not $row.ArmoredDummyEvidence) { $row.ArmoredDummyEvidence = To-ReportBool (Get-ObjectPropertyValue -InputObject $signals -Names @("armoredDummyHitsPresent")) }
    if (-not $row.ProtectedHeadDummyEvidence) { $row.ProtectedHeadDummyEvidence = To-ReportBool (Get-ObjectPropertyValue -InputObject $signals -Names @("protectedDummyHeadshotHitsPresent")) }
    if (-not $row.LethalHeadshotEvidence) {
        $row.LethalHeadshotEvidence = To-ReportBool (Get-ObjectPropertyValue -InputObject $signals -Names @("dummyLethalHeadshotEvidencePresent"))
        if (-not $row.LethalHeadshotEvidence) {
            $row.LethalHeadshotEvidence = To-ReportBool (Get-ObjectPropertyValue -InputObject $signals -Names @("lethalHeadshotEvidencePresent"))
        }
    }

    $missingNotes = @($row.MissingSignalNotes | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) })
    if ($missingNotes.Count -eq 0) {
        if (-not $row.TapFireRejectionEvidence) { $missingNotes += "No tap-fire rejection evidence was logged." }
        if (-not $row.MovementPenaltyEvidence) { $missingNotes += "No movement-penalty evidence was logged." }
        if (-not $row.DummyHeadshotEvidence) { $missingNotes += "No dummy headshot evidence was logged." }
        if (-not $row.ArmoredDummyEvidence) { $missingNotes += "No armored dummy evidence was logged." }
        if (-not $row.ProtectedHeadDummyEvidence) { $missingNotes += "No protected-head dummy evidence was logged." }
        if (-not $row.LethalHeadshotEvidence) { $missingNotes += "No lethal-headshot evidence was logged." }
    }

    $row | Add-Member -NotePropertyName MissingSignalsText -NotePropertyValue ($missingNotes -join " ")
    $row | Add-Member -NotePropertyName EvidenceSummary -NotePropertyValue (
        "tap:{0} move:{1} hs:{2} armor:{3} prot:{4} lethal:{5}" -f
            $(if ($row.TapFireRejectionEvidence) { "Y" } else { "N" }),
            $(if ($row.MovementPenaltyEvidence) { "Y" } else { "N" }),
            $(if ($row.DummyHeadshotEvidence) { "Y" } else { "N" }),
            $(if ($row.ArmoredDummyEvidence) { "Y" } else { "N" }),
            $(if ($row.ProtectedHeadDummyEvidence) { "Y" } else { "N" }),
            $(if ($row.LethalHeadshotEvidence) { "Y" } else { "N" })
    )

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

Write-Host "Weapon comparison aggregation"
Write-Host "  source report count      : $($sortedRows.Count)"
Write-Host "  sort field               : $SortBy"

$tableRows = $sortedRows | Select-Object `
    @{Name = "MatrixStep"; Expression = { if ([string]::IsNullOrWhiteSpace($_.MatrixStep)) { "n/a" } else { $_.MatrixStep } } }, `
    @{Name = "Profile"; Expression = { if ([string]::IsNullOrWhiteSpace($_.GlockProfile)) { "n/a" } else { $_.GlockProfile } } }, `
    @{Name = "Target"; Expression = { if ([string]::IsNullOrWhiteSpace($_.LabTargetProfile)) { "n/a" } else { $_.LabTargetProfile } } }, `
    @{Name = "Acc"; Expression = { $_.AcceptedShotCount } }, `
    @{Name = "Rej"; Expression = { $_.RejectedShotCount } }, `
    @{Name = "DHits"; Expression = { $_.DummyHitCount } }, `
    @{Name = "DKills"; Expression = { $_.DummyKillCount } }, `
    @{Name = "ProtHS"; Expression = { $_.ProtectedDummyHeadshotHitCount } }, `
    @{Name = "AvgDmg"; Expression = { Format-Number -Value $_.AppliedDamageAverage -Digits 2 } }, `
    @{Name = "Signals"; Expression = { $_.EvidenceSummary } }

$formattedTable = ($tableRows | Format-Table -AutoSize | Out-String).TrimEnd()
if (-not [string]::IsNullOrWhiteSpace($formattedTable)) {
    Write-Host $formattedTable
}

$rowsWithMissingSignals = @($sortedRows | Where-Object { -not [string]::IsNullOrWhiteSpace($_.MissingSignalsText) })
if ($rowsWithMissingSignals.Count -gt 0) {
    Write-Host ""
    Write-Host "Missing signal notes"
    foreach ($row in $rowsWithMissingSignals) {
        $rowLabel = if (-not [string]::IsNullOrWhiteSpace($row.MatrixStep)) { $row.MatrixStep } elseif (-not [string]::IsNullOrWhiteSpace($row.SessionTag)) { $row.SessionTag } else { Split-Path -Leaf $row.SourceReportPath }
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
    $resolvedOutputDir = if ([string]::IsNullOrWhiteSpace($OutputDir)) { Get-DefaultComparisonOutputDirectory } else { Get-FullPath -Path $OutputDir }
    Ensure-Directory -Path $resolvedOutputDir
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"

    if ($ExportJson) {
        $jsonPath = Join-Path $resolvedOutputDir ("glock-comparison-summary-" + $stamp + ".json")
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
        $csvPath = Join-Path $resolvedOutputDir ("glock-comparison-summary-" + $stamp + ".csv")
        $sortedRows | Export-Csv -LiteralPath $csvPath -NoTypeInformation -Encoding UTF8
        $exports.csv = $csvPath
    }

    if ($ExportMarkdown) {
        $markdownPath = Join-Path $resolvedOutputDir ("glock-comparison-summary-" + $stamp + ".md")
        $markdownLines = New-Object System.Collections.Generic.List[string]
        $markdownLines.Add("# Glock comparison summary")
        $markdownLines.Add("")
        $markdownLines.Add(("Generated: {0}" -f (Get-Date).ToString("s")))
        $markdownLines.Add(("Source report count: {0}" -f $sortedRows.Count))
        $markdownLines.Add("")
        $markdownLines.Add("| Matrix step | Session tag | Glock profile | Target profile | Accepted | Rejected | Dummy hits | Dummy kills | Dummy headshot hits | Protected headshot hits | Avg damage | Signals | Missing notes |")
        $markdownLines.Add("| --- | --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- |")

        foreach ($row in $sortedRows) {
            $markdownLines.Add((
                "| {0} | {1} | {2} | {3} | {4} | {5} | {6} | {7} | {8} | {9} | {10} | {11} | {12} |" -f
                (Escape-MarkdownTableValue $row.MatrixStep),
                (Escape-MarkdownTableValue $row.SessionTag),
                (Escape-MarkdownTableValue $row.GlockProfile),
                (Escape-MarkdownTableValue $row.LabTargetProfile),
                $row.AcceptedShotCount,
                $row.RejectedShotCount,
                $row.DummyHitCount,
                $row.DummyKillCount,
                $row.DummyHeadshotHitCount,
                $row.ProtectedDummyHeadshotHitCount,
                (Escape-MarkdownTableValue (Format-Number -Value $row.AppliedDamageAverage -Digits 2)),
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
