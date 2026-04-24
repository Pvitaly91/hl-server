param(
    [int]$Count = 8,

    [ValidateSet("all", "glock", "mp5", "357", "shotgun")]
    [string]$Weapon = "all",

    [string]$ReportRoot = "",
    [string]$ExportCsv = ""
)

$ErrorActionPreference = "Stop"

if ($Count -lt 1) {
    throw "-Count must be at least 1."
}

$script:RepoRoot = Split-Path -Parent $PSScriptRoot
$commonScript = Join-Path $PSScriptRoot "common.ps1"
if (Test-Path $commonScript) {
    . $commonScript
}

if ([string]::IsNullOrWhiteSpace($ReportRoot)) {
    if (Get-Command Get-TestbedLogsRoot -ErrorAction SilentlyContinue) {
        $ReportRoot = Join-Path (Join-Path (Get-TestbedLogsRoot) "reports") "stock-client-playtests"
    } else {
        $ReportRoot = Join-Path $script:RepoRoot "testbed\logs\reports\stock-client-playtests"
    }
}

function Get-RatingValue {
    param(
        [object]$Entry,
        [string]$Name
    )

    if (-not $Entry.ratings) {
        return ""
    }

    $property = $Entry.ratings.PSObject.Properties[$Name]
    if (-not $property) {
        return ""
    }

    return $property.Value
}

if (-not (Test-Path -LiteralPath $ReportRoot -PathType Container)) {
    throw "Scorecard report root not found: $ReportRoot"
}

$scorecardFiles = Get-ChildItem -LiteralPath $ReportRoot -Directory |
    Sort-Object LastWriteTime -Descending |
    ForEach-Object {
        $scorecardPath = Join-Path $_.FullName "scorecard.json"
        if (Test-Path -LiteralPath $scorecardPath -PathType Leaf) {
            Get-Item -LiteralPath $scorecardPath
        }
    } |
    Select-Object -First $Count

$rows = @()
foreach ($scorecardFile in $scorecardFiles) {
    try {
        $card = Get-Content -LiteralPath $scorecardFile.FullName -Raw | ConvertFrom-Json
    } catch {
        Write-Warning "Skipping unreadable scorecard: $($scorecardFile.FullName) ($($_.Exception.Message))"
        continue
    }

    foreach ($entry in @($card.entries)) {
        if ($Weapon -ne "all" -and $entry.weapon -ne $Weapon) {
            continue
        }

        $rows += [pscustomobject]@{
            Timestamp = $card.timestamp
            Weapon = $entry.weapon
            MatchPack = $card.match_pack
            Target = "$($card.target_profile)/$($card.target_spot)"
            Telemetry = $entry.telemetry_fresh
            Events = $entry.telemetry_event_count
            Analyzer = $entry.analyzer_status
            Overall = Get-RatingValue -Entry $entry -Name "overall_feel"
            Single = Get-RatingValue -Entry $entry -Name "single_shot_accuracy"
            Spray = Get-RatingValue -Entry $entry -Name "spam_spray_penalty"
            Move = Get-RatingValue -Entry $entry -Name "movement_penalty_feel"
            Headshot = Get-RatingValue -Entry $entry -Name "headshot_feel"
            ClientSync = Get-RatingValue -Entry $entry -Name "client_visual_sync"
            Skipped = $entry.skipped
            Notes = $entry.notes
            Report = $card.report_dir
        }
    }
}

if ($rows.Count -eq 0) {
    Write-Host "No scorecard rows found under $ReportRoot."
    exit 0
}

if (-not [string]::IsNullOrWhiteSpace($ExportCsv)) {
    $rows | Export-Csv -LiteralPath $ExportCsv -NoTypeInformation -Encoding ASCII
    Write-Host "Scorecard CSV: $ExportCsv"
}

$rows |
    Sort-Object Timestamp, Weapon |
    Format-Table -AutoSize Timestamp, Weapon, MatchPack, Target, Telemetry, Events, Analyzer, Overall, Single, Spray, Move, Headshot, ClientSync, Skipped
