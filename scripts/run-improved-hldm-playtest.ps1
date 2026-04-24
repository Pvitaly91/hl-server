[CmdletBinding()]
param(
    [ValidateSet("glock", "mp5", "357", "shotgun")]
    [string]$Weapon = "mp5",

    [string]$MatchPack = "hldm_skill_default",

    [ValidateSet("unarmored", "vest", "vest_headprotected")]
    [string]$TargetProfile = "vest_headprotected",

    [string]$TargetSpot = "default",

    [int]$Port = 27015,

    [string]$HlExe,

    [switch]$StartServer,
    [switch]$NoClient,
    [switch]$OpenEditor,
    [switch]$OpenLogs,
    [switch]$CollectDiagnostics
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"
Import-HLServerEnv

function Write-Header {
    param([string]$Text)
    Write-Host ""
    Write-Host $Text
    Write-Host ("".PadLeft($Text.Length, "-"))
}

function Write-WeaponChecklist {
    Write-Header "Manual weapon checklist"
    Write-Host "Glock:"
    Write-Host "  - apply hldm_skill_default or a Glock cadence/pattern cfg"
    Write-Host "  - stand still, fire careful single clicks, and wait for recovery"
    Write-Host "  - fire rapid spam and confirm telemetry shows cadence/pattern growth"
    Write-Host "  - confirm hard tapfire is not required for the intended skill expression"
    Write-Host ""
    Write-Host "MP5:"
    Write-Host "  - fire a 3-5 shot burst"
    Write-Host "  - pause long enough for reset/recovery"
    Write-Host "  - fire a longer spray and confirm burst-growth telemetry increases"
    Write-Host ""
    Write-Host "357:"
    Write-Host "  - fire one careful first shot"
    Write-Host "  - fire fast follow-ups"
    Write-Host "  - wait for cadence/pattern reset and fire again"
    Write-Host ""
    Write-Host "Shotgun:"
    Write-Host "  - stand close to the dummy and fire one shell"
    Write-Host "  - wait for pattern reset, then fire again"
    Write-Host "  - confirm hit/kill telemetry remains internally consistent"
}

Write-Step "Stable Improved HLDM playtest runner"
Write-Host "Weapon        : $Weapon"
Write-Host "Match pack    : $MatchPack"
Write-Host "Target profile: $TargetProfile"
Write-Host "Target spot   : $TargetSpot"
Write-Host "Port          : $Port"

Write-Step "Checking stable package state"
$checkArgs = @{}
if (-not [string]::IsNullOrWhiteSpace($HlExe)) {
    $checkArgs["HlExe"] = $HlExe
}
& "$PSScriptRoot\check-improved-hldm.ps1" @checkArgs

if ($StartServer) {
    Write-Step "Starting Improved HLDM launcher"
    $launchArgs = @{
        Port = $Port
    }
    if (-not [string]::IsNullOrWhiteSpace($HlExe)) {
        $launchArgs["HlExe"] = $HlExe
    }
    if ($NoClient) {
        $launchArgs["NoClient"] = $true
    }
    & "$PSScriptRoot\play-improved-hldm.ps1" @launchArgs
}
else {
    Write-Header "Launch command"
    Write-Host "Run this when you are ready to start the live session:"
    Write-Host "  .\scripts\play-improved-hldm.bat"
    Write-Host "Server-only variant:"
    Write-Host "  .\scripts\play-improved-hldm.ps1 -NoClient -Port $Port"
}

if ($OpenEditor) {
    Write-Step "Opening editor"
    $editorArgs = @{}
    if (-not [string]::IsNullOrWhiteSpace($HlExe)) {
        $editorArgs["HlExe"] = $HlExe
    }
    & "$PSScriptRoot\open-hldm-editor.ps1" @editorArgs
}
else {
    Write-Header "Editor command"
    Write-Host "Open the deployed editor with:"
    Write-Host "  .\scripts\open-hldm-editor.bat"
}

$sandboxCommands = @(
    "exp_matchcfg_apply $MatchPack",
    "exp_sandbox_start",
    "exp_sandbox_weapon $Weapon",
    "exp_sandbox_pack $MatchPack",
    "exp_sandbox_target $TargetProfile",
    "exp_sandbox_spot $TargetSpot",
    "exp_sandbox_reset",
    "exp_sandbox_status",
    "exp_sandbox_verify"
)

Write-Header "Sandbox setup commands"
foreach ($command in $sandboxCommands) {
    Write-Host "  $command"
}

Write-WeaponChecklist

Write-Header "After shooting"
Write-Host "Analyze latest telemetry:"
Write-Host "  .\scripts\analyze-improved-hldm-latest.ps1"
Write-Host "Collect a support bundle:"
Write-Host "  .\scripts\collect-improved-hldm-diagnostics.ps1"
Write-Host "Checklist doc:"
Write-Host "  docs\stable-playtest-checklist.md"

if ($OpenLogs) {
    $logRoot = Get-TestbedLogsRoot
    Ensure-Directory -Path $logRoot
    Invoke-Item -LiteralPath $logRoot
}

if ($CollectDiagnostics) {
    Write-Step "Collecting diagnostics"
    $diagnosticsArgs = @{}
    if (-not [string]::IsNullOrWhiteSpace($HlExe)) {
        $diagnosticsArgs["HlExe"] = $HlExe
    }
    & "$PSScriptRoot\collect-improved-hldm-diagnostics.ps1" @diagnosticsArgs
}
