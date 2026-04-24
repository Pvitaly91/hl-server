[CmdletBinding()]
param(
    [string]$Path,

    [string]$OutputRoot,

    [string]$HlExe
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"
Import-HLServerEnv

function Get-LiveLogRoots {
    param([string]$ExplicitHlExe)

    $roots = New-Object System.Collections.Generic.List[string]
    $roots.Add((Get-TestbedLogsRoot))

    $clientInstall = Resolve-TestbedClientInstall -ExplicitHlExe $ExplicitHlExe
    if ($clientInstall) {
        $roots.Add((Join-Path $clientInstall.Root "logs"))
        $roots.Add((Join-Path (Get-TestbedLiveModRoot -ClientRoot $clientInstall.Root -GameDirName (Get-TestbedLiveModName)) "logs"))
    }

    return @($roots | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Select-Object -Unique)
}

function Find-LatestWeaponLog {
    param([string]$ExplicitPath, [string]$ExplicitHlExe)

    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
        if (-not (Test-Path -LiteralPath $ExplicitPath -PathType Leaf)) {
            throw "Weapon log was not found at $ExplicitPath"
        }

        return Get-Item -LiteralPath $ExplicitPath
    }

    $candidates = New-Object System.Collections.Generic.List[object]
    foreach ($root in (Get-LiveLogRoots -ExplicitHlExe $ExplicitHlExe)) {
        if (-not (Test-Path -LiteralPath $root -PathType Container)) {
            continue
        }

        foreach ($log in (Get-ChildItem -LiteralPath $root -Filter "weapon-debug-*.log" -File -ErrorAction SilentlyContinue)) {
            $candidates.Add($log)
        }
    }

    return @($candidates | Sort-Object LastWriteTime -Descending | Select-Object -First 1)
}

function Invoke-Analyzer {
    param(
        [Parameter(Mandatory = $true)]
        [string]$LogPath,

        [Parameter(Mandatory = $true)]
        [string]$Weapon,

        [Parameter(Mandatory = $true)]
        [string]$Destination
    )

    $analyzer = Join-Path $PSScriptRoot "analyze-weapon-log.ps1"
    if (-not (Test-Path -LiteralPath $analyzer -PathType Leaf)) {
        throw "Analyzer script was not found at $analyzer"
    }

    $output = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $analyzer -Path $LogPath -Weapon $Weapon 2>&1
    $exitCode = $LASTEXITCODE
    $output | Set-Content -LiteralPath $Destination -Encoding ASCII

    return [PSCustomObject]@{
        Weapon = $Weapon
        Path = $Destination
        ExitCode = $exitCode
    }
}

$latestLog = Find-LatestWeaponLog -ExplicitPath $Path -ExplicitHlExe $HlExe
if (-not $latestLog) {
    Write-Host "No weapon-debug-*.log file was found."
    Write-Host "Searched:"
    foreach ($root in (Get-LiveLogRoots -ExplicitHlExe $HlExe)) {
        Write-Host "  $root"
    }
    exit 2
}

if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path (Join-Path (Get-TestbedLogsRoot) "reports") "improved-hldm-analysis"
}

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$outputDir = Join-Path $OutputRoot $timestamp
Ensure-Directory -Path $outputDir

$summaryPath = Join-Path $outputDir "analysis-summary.txt"
$weapons = @("all", "glock", "mp5", "357", "shotgun")
$results = New-Object System.Collections.Generic.List[object]

foreach ($weapon in $weapons) {
    $destination = Join-Path $outputDir ("analysis-{0}.txt" -f $weapon)
    $results.Add((Invoke-Analyzer -LogPath $latestLog.FullName -Weapon $weapon -Destination $destination))
}

$summary = New-Object System.Collections.Generic.List[string]
$summary.Add("Improved HLDM analysis run")
$summary.Add("timestamp=$timestamp")
$summary.Add("log=$($latestLog.FullName)")
$summary.Add("output=$outputDir")
foreach ($result in $results) {
    $summary.Add(("weapon={0} exit={1} path={2}" -f $result.Weapon, $result.ExitCode, $result.Path))
}
$summary | Set-Content -LiteralPath $summaryPath -Encoding ASCII

Write-Host "Improved HLDM analyzer output"
Write-Host "  log     : $($latestLog.FullName)"
Write-Host "  output  : $outputDir"
Write-Host "  summary : $summaryPath"
foreach ($result in $results) {
    Write-Host ("  {0,-7}: {1} (exit {2})" -f $result.Weapon, $result.Path, $result.ExitCode)
}
