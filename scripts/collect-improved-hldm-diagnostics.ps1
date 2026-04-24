[CmdletBinding()]
param(
    [string]$OutputRoot,

    [string]$HlExe,

    [switch]$RunEditorSelfTest
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"
Import-HLServerEnv

function Add-Line {
    param(
        [AllowEmptyCollection()]
        [System.Collections.Generic.List[string]]$Lines,

        [Parameter(Mandatory = $true)]
        [string]$Text
    )

    $Lines.Add($Text) | Out-Null
}

function Copy-IfExists {
    param(
        [string]$Path,
        [string]$DestinationDirectory,
        [string]$DestinationName
    )

    if ([string]::IsNullOrWhiteSpace($Path) -or -not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return $false
    }

    Ensure-Directory -Path $DestinationDirectory
    $target = if ([string]::IsNullOrWhiteSpace($DestinationName)) {
        Join-Path $DestinationDirectory (Split-Path -Leaf $Path)
    }
    else {
        Join-Path $DestinationDirectory $DestinationName
    }

    Copy-Item -LiteralPath $Path -Destination $target -Force
    return $true
}

function Get-LivePaths {
    param([string]$ExplicitHlExe)

    $clientInstall = Resolve-TestbedClientInstall -ExplicitHlExe $ExplicitHlExe
    $liveModRoot = if ($clientInstall) {
        Get-TestbedLiveModRoot -ClientRoot $clientInstall.Root -GameDirName (Get-TestbedLiveModName)
    }
    else {
        $null
    }

    return [PSCustomObject]@{
        ClientRoot = if ($clientInstall) { $clientInstall.Root } else { $null }
        HlExe = if ($clientInstall) { $clientInstall.HlExe } else { $null }
        HldsExe = if ($clientInstall -and $clientInstall.Probe) { $clientInstall.Probe.HldsExe } else { $null }
        LiveModRoot = $liveModRoot
        LiveDll = if ($liveModRoot) { Join-Path $liveModRoot "dlls\hl.dll" } else { $null }
        EditorExe = if ($liveModRoot) { Join-Path $liveModRoot "HlConfigEditorCpp.exe" } else { $null }
        MatchPacks = if ($liveModRoot) { Join-Path $liveModRoot "match_packs" } else { $null }
        LiveLogs = if ($liveModRoot) { Join-Path $liveModRoot "logs" } else { $null }
    }
}

function Get-LatestFile {
    param(
        [string[]]$Roots,
        [string]$Filter
    )

    $files = New-Object System.Collections.Generic.List[object]
    foreach ($root in $Roots) {
        if ([string]::IsNullOrWhiteSpace($root) -or -not (Test-Path -LiteralPath $root -PathType Container)) {
            continue
        }

        foreach ($file in (Get-ChildItem -LiteralPath $root -Filter $Filter -File -ErrorAction SilentlyContinue)) {
            $files.Add($file)
        }
    }

    return @($files | Sort-Object LastWriteTime -Descending | Select-Object -First 1)
}

function Get-LatestDirectory {
    param([string]$Root)

    if ([string]::IsNullOrWhiteSpace($Root) -or -not (Test-Path -LiteralPath $Root -PathType Container)) {
        return $null
    }

    return Get-ChildItem -LiteralPath $Root -Directory -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
}

function Invoke-CommandToFile {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Command,

        [Parameter(Mandatory = $true)]
        [string]$Destination
    )

    $exe = $Command[0]
    $arguments = @()
    if ($Command.Count -gt 1) {
        $arguments = $Command[1..($Command.Count - 1)]
    }

    $output = & $exe @arguments 2>&1
    $exitCode = $LASTEXITCODE
    $output | Set-Content -LiteralPath $Destination -Encoding ASCII
    return $exitCode
}

if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path (Join-Path (Get-TestbedLogsRoot) "reports") "stable-package-diagnostics"
}

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$destinationRoot = Join-Path $OutputRoot $timestamp
Ensure-Directory -Path $destinationRoot

$livePaths = Get-LivePaths -ExplicitHlExe $HlExe
$repoLogsRoot = Get-TestbedLogsRoot
$liveLogRoots = @($repoLogsRoot)
if ($livePaths.LiveLogs) {
    $liveLogRoots += $livePaths.LiveLogs
}
if ($livePaths.ClientRoot) {
    $liveLogRoots += (Join-Path $livePaths.ClientRoot "logs")
}

$summary = New-Object System.Collections.Generic.List[string]
Add-Line -Lines $summary -Text "Stable Improved HLDM diagnostics"
Add-Line -Lines $summary -Text "timestamp=$timestamp"
Add-Line -Lines $summary -Text "output=$destinationRoot"

$gitInfoPath = Join-Path $destinationRoot "git-info.txt"
@(
    "branch=$(git rev-parse --abbrev-ref HEAD)",
    "commit=$(git rev-parse HEAD)",
    "status:",
    (git status --short)
) | Set-Content -LiteralPath $gitInfoPath -Encoding ASCII
Add-Line -Lines $summary -Text "git_info=$gitInfoPath"

$pathsPath = Join-Path $destinationRoot "paths.txt"
@(
    "repo_root=$(Get-RepoRoot)",
    "client_root=$($livePaths.ClientRoot)",
    "hl_exe=$($livePaths.HlExe)",
    "hlds_exe=$($livePaths.HldsExe)",
    "live_mod_root=$($livePaths.LiveModRoot)",
    "live_dll=$($livePaths.LiveDll)",
    "editor_exe=$($livePaths.EditorExe)",
    "live_match_packs=$($livePaths.MatchPacks)",
    "repo_logs=$repoLogsRoot",
    "live_logs=$($livePaths.LiveLogs)"
) | Set-Content -LiteralPath $pathsPath -Encoding ASCII
Add-Line -Lines $summary -Text "paths=$pathsPath"

$packageCheckPath = Join-Path $destinationRoot "package-check.txt"
$checkCommand = @("powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", (Join-Path $PSScriptRoot "check-improved-hldm.ps1"))
if (-not [string]::IsNullOrWhiteSpace($HlExe)) {
    $checkCommand += @("-HlExe", $HlExe)
}
$checkExit = Invoke-CommandToFile -Command $checkCommand -Destination $packageCheckPath
Add-Line -Lines $summary -Text "package_check=$packageCheckPath exit=$checkExit"

$latestWeaponLog = Get-LatestFile -Roots $liveLogRoots -Filter "weapon-debug-*.log"
$logsDirectory = Join-Path $destinationRoot "logs"
if ($latestWeaponLog) {
    Copy-IfExists -Path $latestWeaponLog.FullName -DestinationDirectory $logsDirectory -DestinationName "latest-weapon-debug.log" | Out-Null
    Add-Line -Lines $summary -Text "latest_weapon_log=$($latestWeaponLog.FullName)"
}
else {
    "No weapon-debug-*.log was found in: $($liveLogRoots -join '; ')" | Set-Content -LiteralPath (Join-Path $destinationRoot "NO_WEAPON_LOG_FOUND.txt") -Encoding ASCII
    Add-Line -Lines $summary -Text "latest_weapon_log=not_found"
}

$qconsoleCandidates = @()
if ($livePaths.ClientRoot) {
    $qconsoleCandidates += (Join-Path $livePaths.ClientRoot "qconsole.log")
    $qconsoleCandidates += (Join-Path $livePaths.ClientRoot "valve\qconsole.log")
}
if ($livePaths.LiveModRoot) {
    $qconsoleCandidates += (Join-Path $livePaths.LiveModRoot "qconsole.log")
}
$qconsoleCandidates += (Join-Path (Get-RepoRoot) "testbed\qconsole.log")
$latestQConsole = $qconsoleCandidates |
    Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) } |
    ForEach-Object { Get-Item -LiteralPath $_ } |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1
if ($latestQConsole) {
    Copy-IfExists -Path $latestQConsole.FullName -DestinationDirectory $logsDirectory -DestinationName "latest-qconsole.log" | Out-Null
    Add-Line -Lines $summary -Text "latest_qconsole=$($latestQConsole.FullName)"
}
else {
    Add-Line -Lines $summary -Text "latest_qconsole=not_found"
}

$latestLaunch = Get-LatestFile -Roots @($repoLogsRoot) -Filter "hlds-*-launch.txt"
if ($latestLaunch) {
    Copy-IfExists -Path $latestLaunch.FullName -DestinationDirectory $logsDirectory -DestinationName "latest-hlds-launch.txt" | Out-Null
    Add-Line -Lines $summary -Text "latest_hlds_launch=$($latestLaunch.FullName)"
}

foreach ($filter in @("hlds-*-stdout.log", "hlds-*-stderr.log")) {
    $latest = Get-LatestFile -Roots @($repoLogsRoot) -Filter $filter
    if ($latest) {
        Copy-IfExists -Path $latest.FullName -DestinationDirectory $logsDirectory -DestinationName ("latest-" + $latest.Name) | Out-Null
    }
}

$packListPath = Join-Path $destinationRoot "live-match-packs.txt"
if ($livePaths.MatchPacks -and (Test-Path -LiteralPath $livePaths.MatchPacks -PathType Container)) {
    Get-ChildItem -LiteralPath $livePaths.MatchPacks -File -ErrorAction SilentlyContinue |
        Sort-Object Name |
        Select-Object Name,Length,LastWriteTime |
        Format-Table -AutoSize |
        Out-String |
        Set-Content -LiteralPath $packListPath -Encoding ASCII
}
else {
    "Live match-pack directory was not found." | Set-Content -LiteralPath $packListPath -Encoding ASCII
}
Add-Line -Lines $summary -Text "live_match_packs=$packListPath"

$cfgListPath = Join-Path $destinationRoot "live-root-cfgs.txt"
if ($livePaths.LiveModRoot -and (Test-Path -LiteralPath $livePaths.LiveModRoot -PathType Container)) {
    Get-ChildItem -LiteralPath $livePaths.LiveModRoot -Filter "*.cfg" -File -ErrorAction SilentlyContinue |
        Sort-Object Name |
        Select-Object Name,Length,LastWriteTime |
        Format-Table -AutoSize |
        Out-String |
        Set-Content -LiteralPath $cfgListPath -Encoding ASCII
}
else {
    "Live mod root was not found." | Set-Content -LiteralPath $cfgListPath -Encoding ASCII
}
Add-Line -Lines $summary -Text "live_root_cfgs=$cfgListPath"

$selfTestSummary = Join-RepoPath "artifacts\HlConfigEditorCppSelfTest\selftest-summary.txt"
if ($RunEditorSelfTest -and $livePaths.EditorExe -and (Test-Path -LiteralPath $livePaths.EditorExe -PathType Leaf)) {
    $selfTestLog = Join-Path $destinationRoot "editor-self-test-run.txt"
    $process = Start-Process -FilePath $livePaths.EditorExe -ArgumentList "--self-test" -WorkingDirectory (Split-Path -Parent $livePaths.EditorExe) -Wait -PassThru
    "exit_code=$($process.ExitCode)" | Set-Content -LiteralPath $selfTestLog -Encoding ASCII
    Add-Line -Lines $summary -Text "editor_self_test_run=$selfTestLog exit=$($process.ExitCode)"
}
if (Test-Path -LiteralPath $selfTestSummary -PathType Leaf) {
    Copy-IfExists -Path $selfTestSummary -DestinationDirectory $destinationRoot -DestinationName "editor-selftest-summary.txt" | Out-Null
    Add-Line -Lines $summary -Text "editor_selftest_summary=$(Join-Path $destinationRoot 'editor-selftest-summary.txt')"
}

if ($latestWeaponLog) {
    $analysisRoot = Join-Path $destinationRoot "analysis"
    Ensure-Directory -Path $analysisRoot
    $analyzer = Join-Path $PSScriptRoot "analyze-weapon-log.ps1"
    foreach ($weapon in @("all", "glock", "mp5", "357", "shotgun")) {
        $analysisPath = Join-Path $analysisRoot ("analysis-{0}.txt" -f $weapon)
        $output = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $analyzer -Path $latestWeaponLog.FullName -Weapon $weapon 2>&1
        $exitCode = $LASTEXITCODE
        $output | Set-Content -LiteralPath $analysisPath -Encoding ASCII
        Add-Line -Lines $summary -Text ("analysis_{0}={1} exit={2}" -f $weapon, $analysisPath, $exitCode)
    }
}

$stockPlaytestRoot = Join-Path (Join-Path (Get-TestbedLogsRoot) "reports") "stock-client-playtests"
$latestStockPlaytest = Get-LatestDirectory -Root $stockPlaytestRoot
if ($latestStockPlaytest) {
    $stockDestination = Join-Path $destinationRoot "stock-client-playtest"
    Ensure-Directory -Path $stockDestination

    foreach ($fileName in @(
        "summary.txt",
        "package-check.txt",
        "per-weapon-summary.txt",
        "client-status.txt",
        "rcon-validation.txt",
        "commands.txt",
        "glock-analysis.txt",
        "mp5-analysis.txt",
        "357-analysis.txt",
        "shotgun-analysis.txt"
    )) {
        $source = Join-Path $latestStockPlaytest.FullName $fileName
        Copy-IfExists -Path $source -DestinationDirectory $stockDestination -DestinationName $fileName | Out-Null
    }

    Add-Line -Lines $summary -Text "latest_stock_client_playtest=$($latestStockPlaytest.FullName)"
    Add-Line -Lines $summary -Text "stock_client_playtest_copy=$stockDestination"
}
else {
    Add-Line -Lines $summary -Text "latest_stock_client_playtest=not_found"
}

$summaryPath = Join-Path $destinationRoot "diagnostics-summary.txt"
$summary | Set-Content -LiteralPath $summaryPath -Encoding ASCII

Write-Host "Stable Improved HLDM diagnostics collected"
Write-Host "  output  : $destinationRoot"
Write-Host "  summary : $summaryPath"
if ($latestWeaponLog) {
    Write-Host "  weapon log: $($latestWeaponLog.FullName)"
}
else {
    Write-Host "  weapon log: not found"
}
