[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [string]$HlExe
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"
Import-HLServerEnv

$script:HasFailure = $false

function Write-CheckResult {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Label,

        [Parameter(Mandatory = $true)]
        [bool]$Passed,

        [string]$Details
    )

    if ($Passed) {
        Write-Host ("[PASS] {0}{1}" -f $Label, $(if ([string]::IsNullOrWhiteSpace($Details)) { "" } else { " - $Details" }))
    }
    else {
        $script:HasFailure = $true
        Write-Host ("[FAIL] {0}{1}" -f $Label, $(if ([string]::IsNullOrWhiteSpace($Details)) { "" } else { " - $Details" }))
    }
}

function Test-AndReportFile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Label,

        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    Write-CheckResult -Label $Label -Passed (Test-Path -LiteralPath $Path -PathType Leaf) -Details $Path
}

function Test-AndReportDirectory {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Label,

        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    Write-CheckResult -Label $Label -Passed (Test-Path -LiteralPath $Path -PathType Container) -Details $Path
}

$configuration = Get-ValidatedConfiguration -Configuration $Configuration
$repoRoot = Get-RepoRoot
$builtDll = Get-HlDllPath -Configuration $configuration
$clientInstall = Resolve-TestbedClientInstall -ExplicitHlExe $HlExe
$liveModRoot = $null
$liveDll = $null
$editorExe = $null
$matchPackRoot = $null
$defaultPack = $null
$liveLogsRoot = $null

Write-Step "Checking Stable Improved HLDM package"
Write-Host "Repo root     : $repoRoot"
Write-Host "Configuration : $configuration"

Test-AndReportFile -Label "Built server DLL" -Path $builtDll

if ($clientInstall) {
    Write-CheckResult -Label "Resolved Half-Life root" -Passed $true -Details $clientInstall.Root
    $liveModRoot = Get-TestbedLiveModRoot -ClientRoot $clientInstall.Root -GameDirName (Get-TestbedLiveModName)
    $liveDll = Join-Path $liveModRoot "dlls\hl.dll"
    $editorExe = Join-Path $liveModRoot "HlConfigEditorCpp.exe"
    $matchPackRoot = Join-Path $liveModRoot "match_packs"
    $defaultPack = Join-Path $matchPackRoot "hldm_skill_default.json"
    $liveLogsRoot = Join-Path $liveModRoot "logs"
}
else {
    Write-CheckResult -Label "Resolved Half-Life root" -Passed $false -Details "Set HL_EXE in .env or pass -HlExe <path>."
}

if ($liveModRoot) {
    Test-AndReportDirectory -Label "Live mod root" -Path $liveModRoot
    Test-AndReportFile -Label "Live mod DLL" -Path $liveDll
    Test-AndReportFile -Label "Deployed editor" -Path $editorExe
    Test-AndReportDirectory -Label "Match-pack directory" -Path $matchPackRoot
    Test-AndReportFile -Label "Recommended Improved HLDM pack" -Path $defaultPack

    try {
        Ensure-Directory -Path $liveLogsRoot
        Write-CheckResult -Label "Live logs directory" -Passed $true -Details $liveLogsRoot
    }
    catch {
        Write-CheckResult -Label "Live logs directory" -Passed $false -Details $_.Exception.Message
    }

    if (Test-Path -LiteralPath $matchPackRoot -PathType Container) {
        $packCount = @(Get-ChildItem -LiteralPath $matchPackRoot -Filter "*.json" -File -ErrorAction SilentlyContinue).Count
        Write-CheckResult -Label "Match-pack count" -Passed ($packCount -gt 0) -Details "$packCount JSON pack(s)"
    }
}

$repoLogsRoot = Get-TestbedLogsRoot
$reportParent = Join-Path $repoLogsRoot "reports"
$reportRoot = Join-Path $reportParent "guided-tests"
$comparisonRoot = Join-Path $reportRoot "comparisons"
try {
    Ensure-Directory -Path $repoLogsRoot
    Ensure-Directory -Path $reportRoot
    Ensure-Directory -Path $comparisonRoot
    Write-CheckResult -Label "Repo logs directory" -Passed $true -Details $repoLogsRoot
    Write-CheckResult -Label "Guided report directory" -Passed $true -Details $reportRoot
    Write-CheckResult -Label "Comparison export directory" -Passed $true -Details $comparisonRoot
}
catch {
    Write-CheckResult -Label "Report/log directories" -Passed $false -Details $_.Exception.Message
}

if ($script:HasFailure) {
    Write-Host ""
    Write-Host "Stable package check failed. Recommended repair:"
    Write-Host "  .\scripts\setup-improved-hldm.ps1"
    exit 1
}

Write-Host ""
Write-Host "Stable package check passed."
Write-Host "Recommended next commands:"
Write-Host "  .\scripts\play-improved-hldm.bat"
Write-Host "  .\scripts\open-hldm-editor.bat"
exit 0
