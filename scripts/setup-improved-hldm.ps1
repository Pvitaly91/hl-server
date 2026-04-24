[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [ValidateSet("Debug", "Release")]
    [string]$EditorConfiguration = "Release",

    [string]$HlExe,

    [switch]$SkipServerBuild,
    [switch]$SkipEditorBuild
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"
Import-HLServerEnv

function Resolve-MSBuild {
    $candidates = New-Object System.Collections.Generic.List[string]

    $command = Get-Command "MSBuild.exe" -ErrorAction SilentlyContinue
    if ($command -and -not [string]::IsNullOrWhiteSpace($command.Source)) {
        $candidates.Add($command.Source)
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswhere -PathType Leaf) {
        $vswhereResults = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" 2>$null
        foreach ($result in $vswhereResults) {
            if (-not [string]::IsNullOrWhiteSpace($result)) {
                $candidates.Add($result)
            }
        }
    }

    $programFiles = ${env:ProgramFiles}
    $programFilesX86 = ${env:ProgramFiles(x86)}
    foreach ($candidate in @(
        (Join-Path $programFiles "Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"),
        (Join-Path $programFiles "Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe"),
        (Join-Path $programFiles "Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"),
        (Join-Path $programFiles "Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"),
        (Join-Path $programFilesX86 "Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"),
        (Join-Path $programFilesX86 "Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe"),
        (Join-Path $programFilesX86 "Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"),
        (Join-Path $programFilesX86 "Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe")
    )) {
        if (-not [string]::IsNullOrWhiteSpace($candidate)) {
            $candidates.Add($candidate)
        }
    }

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    throw "MSBuild.exe was not found. Install Visual Studio 2022 Desktop development with C++, or run from a Developer PowerShell with MSBuild on PATH."
}

function Assert-ExistingFile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Label,

        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Label was not found at $Path"
    }

    Write-Host "[OK] $Label`: $Path"
}

function Assert-ExistingDirectory {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Label,

        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        throw "$Label was not found at $Path"
    }

    Write-Host "[OK] $Label`: $Path"
}

$configuration = Get-ValidatedConfiguration -Configuration $Configuration
$editorConfiguration = Get-ValidatedConfiguration -Configuration $EditorConfiguration
$repoRoot = Get-RepoRoot

Write-Step "Stable Improved HLDM setup"
Write-Host "Repo root          : $repoRoot"
Write-Host "Server config      : $configuration"
Write-Host "Editor config      : $editorConfiguration|Win32"

if (-not $SkipServerBuild) {
    Write-Step "Building server GameDLL"
    & "$PSScriptRoot\build.ps1" -Configuration $configuration
}
else {
    Write-Step "Skipping server GameDLL build"
}

Write-Step "Installing managed live mod"
if ([string]::IsNullOrWhiteSpace($HlExe)) {
    & "$PSScriptRoot\install-testbed.ps1" -Configuration $configuration -PreferClientMatchedRuntime -BuildIfMissing
}
else {
    & "$PSScriptRoot\install-testbed.ps1" -Configuration $configuration -PreferClientMatchedRuntime -BuildIfMissing -HlExe $HlExe
}

if (-not $SkipEditorBuild) {
    Write-Step "Building and deploying native C++ editor"
    $msbuild = Resolve-MSBuild
    $editorProject = Join-RepoPath "tools\HlConfigEditorCpp\HlConfigEditorCpp.vcxproj"
    & $msbuild $editorProject "/p:Configuration=$editorConfiguration" "/p:Platform=Win32"
    if ($LASTEXITCODE -ne 0) {
        throw "MSBuild failed with exit code $LASTEXITCODE."
    }
}
else {
    Write-Step "Skipping native C++ editor build"
}

$clientInstall = Resolve-TestbedClientInstall -ExplicitHlExe $HlExe
if (-not $clientInstall) {
    throw "No stock Half-Life client was resolved. Set HL_EXE in .env or pass -HlExe <path>."
}

$gameDirName = Get-TestbedLiveModName
$liveModRoot = Get-TestbedLiveModRoot -ClientRoot $clientInstall.Root -GameDirName $gameDirName
$liveDll = Join-Path $liveModRoot "dlls\hl.dll"
$editorExe = Join-Path $liveModRoot "HlConfigEditorCpp.exe"
$matchPackRoot = Join-Path $liveModRoot "match_packs"
$defaultPack = Join-Path $matchPackRoot "hldm_skill_default.json"
$defaultPackCfg = Join-Path $matchPackRoot "hldm_skill_default.cfg"
$liveLogsRoot = Join-Path $liveModRoot "logs"
$reportParent = Join-Path (Get-TestbedLogsRoot) "reports"
$reportRoot = Join-Path $reportParent "guided-tests"

Write-Step "Refreshing starter match packs and report folders"
Sync-TestbedStarterMatchPacks -ModRoot $liveModRoot
Ensure-Directory -Path $liveLogsRoot
Ensure-Directory -Path $reportRoot
Ensure-Directory -Path (Join-Path $reportRoot "comparisons")

Write-Step "Verifying stable package outputs"
Assert-ExistingFile -Label "Built server DLL" -Path (Get-HlDllPath -Configuration $configuration)
Assert-ExistingDirectory -Label "Live mod root" -Path $liveModRoot
Assert-ExistingFile -Label "Live mod DLL" -Path $liveDll
Assert-ExistingFile -Label "Deployed editor" -Path $editorExe
Assert-ExistingFile -Label "Recommended match pack metadata" -Path $defaultPack
Assert-ExistingFile -Label "Recommended match pack cfg" -Path $defaultPackCfg
Assert-ExistingDirectory -Label "Live weapon-log directory" -Path $liveLogsRoot
Assert-ExistingDirectory -Label "Guided test report directory" -Path $reportRoot

Write-Host ""
Write-Host "Stable Improved HLDM package is ready."
Write-Host ""
Write-Host "Next commands:"
Write-Host "  .\scripts\check-improved-hldm.ps1"
Write-Host "  .\scripts\play-improved-hldm.bat"
Write-Host "  .\scripts\open-hldm-editor.bat"
Write-Host ""
Write-Host "Useful live server commands:"
Write-Host "  exp_matchcfg_apply hldm_skill_default"
Write-Host "  exp_sandbox_start"
Write-Host "  exp_sandbox_reset"
Write-Host ""
Write-Host "Editor path:"
Write-Host "  $editorExe"
