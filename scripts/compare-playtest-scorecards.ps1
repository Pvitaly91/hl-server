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

function Get-PropertyValue {
    param(
        [object]$Object,
        [string]$Name,
        [object]$Fallback = ""
    )

    if (-not $Object) {
        return $Fallback
    }

    $property = $Object.PSObject.Properties[$Name]
    if (-not $property) {
        return $Fallback
    }

    return $property.Value
}

function Get-RatingValue {
    param(
        [object]$Entry,
        [string]$Name
    )

    $ratings = Get-PropertyValue -Object $Entry -Name "ratings" -Fallback $null
    if (-not $ratings) {
        return ""
    }

    $property = $ratings.PSObject.Properties[$Name]
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

    $entries = Get-PropertyValue -Object $card -Name "entries" -Fallback @()
    foreach ($entry in @($entries)) {
        $entryWeapon = Get-PropertyValue -Object $entry -Name "weapon"
        if ($Weapon -ne "all" -and $entryWeapon -ne $Weapon) {
            continue
        }

        $targetProfile = Get-PropertyValue -Object $card -Name "target_profile"
        $targetSpot = Get-PropertyValue -Object $card -Name "target_spot"
        $rows += [pscustomobject]@{
            Timestamp = Get-PropertyValue -Object $card -Name "timestamp"
            Weapon = $entryWeapon
            MatchPack = Get-PropertyValue -Object $card -Name "match_pack"
            Target = "$targetProfile/$targetSpot"
            Telemetry = Get-PropertyValue -Object $entry -Name "telemetry_fresh"
            Events = Get-PropertyValue -Object $entry -Name "telemetry_event_count"
            Analyzer = Get-PropertyValue -Object $entry -Name "analyzer_status"
            Overall = Get-RatingValue -Entry $entry -Name "overall_feel"
            Single = Get-RatingValue -Entry $entry -Name "single_shot_accuracy"
            Spray = Get-RatingValue -Entry $entry -Name "spam_spray_penalty"
            Move = Get-RatingValue -Entry $entry -Name "movement_penalty_feel"
            Headshot = Get-RatingValue -Entry $entry -Name "headshot_feel"
            ClientSync = Get-RatingValue -Entry $entry -Name "client_visual_sync"
            Skipped = Get-PropertyValue -Object $entry -Name "skipped"
            Notes = Get-PropertyValue -Object $entry -Name "notes"
            Report = Get-PropertyValue -Object $card -Name "report_dir"
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
