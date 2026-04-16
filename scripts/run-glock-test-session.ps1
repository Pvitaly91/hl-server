[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [int]$Port = 27015,
    [string]$Map = "crossfire",
    [switch]$NoClient,
    [switch]$NoTail,
    [switch]$AnalyzeLatestOnExit,
    [switch]$DetachedServer = $true,
    [int]$TimeoutSeconds = 45
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"
Import-HLServerEnv

function Wait-ForSessionWeaponDebugLog {
    param(
        [System.IO.FileInfo]$PreviousLatestLog,
        [Parameter(Mandatory = $true)]
        [int]$TimeoutSeconds,
        [System.Diagnostics.Process]$Process
    )

    $previousPath = if ($PreviousLatestLog) { $PreviousLatestLog.FullName } else { $null }
    $previousWriteTimeUtc = if ($PreviousLatestLog) { $PreviousLatestLog.LastWriteTimeUtc } else { [DateTime]::MinValue }
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)

    while ((Get-Date) -lt $deadline) {
        $latestLog = Get-LatestWeaponDebugLog
        if ($latestLog) {
            $isNewLog = [string]::IsNullOrWhiteSpace($previousPath) -or
                (-not $latestLog.FullName.Equals($previousPath, [System.StringComparison]::OrdinalIgnoreCase)) -or
                ($latestLog.LastWriteTimeUtc -gt $previousWriteTimeUtc)

            if ($isNewLog -and (Select-String -Path $latestLog.FullName -Pattern "event=weapon_debug_session status=ready" -SimpleMatch -Quiet -ErrorAction SilentlyContinue)) {
                return $latestLog
            }
        }

        if ($Process) {
            $Process.Refresh()
            if ($Process.HasExited) {
                return $null
            }
        }

        Start-Sleep -Milliseconds 500
    }

    return $null
}

function Get-SessionClientExe {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RuntimeRoot
    )

    $runtimeClient = Join-Path $RuntimeRoot "hl.exe"
    if (Test-LeafPath -Path $runtimeClient) {
        return $runtimeClient
    }

    return (Resolve-HlExe)
}

function Start-WeaponLogTailWindow {
    param(
        [Parameter(Mandatory = $true)]
        [string]$WeaponLogPath
    )

    $tailScript = Join-Path $PSScriptRoot "tail-weapon-log.ps1"
    $arguments = @(
        "-NoExit",
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", $tailScript,
        "-Path", $WeaponLogPath
    )

    Start-Process -FilePath "powershell.exe" -WorkingDirectory (Get-RepoRoot) -ArgumentList $arguments | Out-Null
}

function Write-Checklist {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Configuration,
        [Parameter(Mandatory = $true)]
        [int]$Port,
        [Parameter(Mandatory = $true)]
        [string]$Map,
        [Parameter(Mandatory = $true)]
        [string]$ConnectAddress,
        [Parameter(Mandatory = $true)]
        [string]$RuntimeRoot,
        [Parameter(Mandatory = $true)]
        [string]$ServerLogPath,
        [string]$WeaponLogPath,
        [bool]$TailWindowRequested,
        [string]$ClientLaunchStatus
    )

    Write-Host ""
    Write-Host "Manual Glock session ready"
    Write-Host "  configuration : $Configuration"
    Write-Host "  map           : $Map"
    Write-Host "  port          : $Port"
    Write-Host "  connect       : $ConnectAddress"
    Write-Host "  game          : valve"
    Write-Host "  runtime root  : $RuntimeRoot"
    Write-Host "  server log    : $ServerLogPath"

    if ([string]::IsNullOrWhiteSpace($WeaponLogPath)) {
        Write-Host "  weapon log    : pending; run .\\scripts\\tail-weapon-log.ps1 once the first telemetry session line is created"
    }
    else {
        Write-Host "  weapon log    : $WeaponLogPath"
    }

    if ($TailWindowRequested) {
        Write-Host "  tailing       : requested in a separate PowerShell window"
    }
    else {
        Write-Host "  tailing       : disabled"
    }

    Write-Host "  client        : $ClientLaunchStatus"
    Write-Host ""
    Write-Host "Manual validation checklist"
    Write-Host "1. Confirm the stock client connected to the disposable server at $ConnectAddress."
    Write-Host "2. Stand still, wait a brief moment, then fire one single Glock primary shot."
    Write-Host "   Expected: one accepted telemetry line and firstshot=1 once the recovery gate is satisfied."
    Write-Host "3. Hold primary without releasing."
    Write-Host "   Expected: no repeated accepted shots and tapfire_hold_blocked rejection lines when rejection logging is enabled."
    Write-Host "4. Move continuously and fire primary."
    Write-Host "   Expected: accepted lines with move_penalty > 0."
    Write-Host "5. Stop, wait past recovery, and fire again."
    Write-Host "   Expected: the first-shot bonus can return."
    Write-Host "6. Crouch-move and compare against uncrouched movement at a similar speed."
    Write-Host "   Expected: reduced movement penalty relative to standing movement."
    Write-Host ""
    Write-Host "Telemetry proves the server-authoritative decision path, not client prediction or weapon feel."
}

function Invoke-AnalyzeLatestWeaponLog {
    param(
        [string]$WeaponLogPath
    )

    $analysisScript = Join-Path $PSScriptRoot "analyze-weapon-log.ps1"
    $analysisArguments = @()

    if (-not [string]::IsNullOrWhiteSpace($WeaponLogPath) -and (Test-LeafPath -Path $WeaponLogPath)) {
        $analysisArguments += @("-Path", $WeaponLogPath)
    }
    else {
        $analysisArguments += "-Latest"
    }

    Write-Host ""
    Write-Host "Press Enter after the manual firing pass to analyze the latest Glock telemetry log."
    [void](Read-Host)

    & $analysisScript @analysisArguments
}

$configuration = Get-ValidatedConfiguration -Configuration $Configuration

if (-not $DetachedServer) {
    throw "run-glock-test-session.ps1 always uses a detached HLDS process so it can wait for readiness, open log tailing, and print the manual checklist."
}

if ($TimeoutSeconds -lt 1) {
    throw "TimeoutSeconds must be at least 1."
}

$resolvedPort = Get-AvailableUdpPort -PreferredPort $Port
if ($resolvedPort -ne $Port) {
    Write-Step "Requested port $Port was busy, using free UDP port $resolvedPort instead"
}

$runtimeRoot = Get-TestbedRuntimeRoot
$previousWeaponLog = Get-LatestWeaponDebugLog

Write-Step "Installing disposable runtime for the Glock manual session"
& "$PSScriptRoot\install-testbed.ps1" -Configuration $configuration

Write-Step "Launching disposable HLDS in experimental-debug mode"
$launchInfo = & "$PSScriptRoot\run-server.ps1" -Configuration $configuration -Map $Map -Port $resolvedPort -Detached -EnableExperimentalGlock -EnableExperimentalGlockDebug -PassThru

$serverReady = Wait-ForHldsReady -LaunchInfo $launchInfo -Map $Map -TimeoutSeconds $TimeoutSeconds
if (-not $serverReady) {
    throw "Timed out waiting for HLDS to start map '$Map'. Check $($launchInfo.StdOutLog) and $($launchInfo.StdErrLog)."
}

$weaponLog = Wait-ForSessionWeaponDebugLog -PreviousLatestLog $previousWeaponLog -TimeoutSeconds ([Math]::Min($TimeoutSeconds, 15)) -Process $launchInfo.Process
$tailWindowRequested = $false

if (-not $NoTail) {
    if ($weaponLog) {
        try {
            Start-WeaponLogTailWindow -WeaponLogPath $weaponLog.FullName
            $tailWindowRequested = $true
        }
        catch {
            Write-Host "Unable to open a separate tail window automatically. Run .\\scripts\\tail-weapon-log.ps1 -Path `"$($weaponLog.FullName)`" in another PowerShell window."
        }
    }
    else {
        Write-Host "Weapon debug log path was not ready before the timeout. Run .\\scripts\\tail-weapon-log.ps1 once the session header appears."
    }
}

$connectAddress = "127.0.0.1:$resolvedPort"
$clientLaunchStatus = "disabled by -NoClient"

if (-not $NoClient) {
    $clientExe = Get-SessionClientExe -RuntimeRoot $runtimeRoot
    if ($clientExe) {
        & "$PSScriptRoot\run-client.ps1" -ConnectAddress $connectAddress -HlExe $clientExe
        $clientLaunchStatus = "launch requested via $clientExe"
    }
    else {
        $clientLaunchStatus = "skipped because no stock hl.exe was found in testbed/runtime or via HL_EXE"
        Write-Host "No stock Half-Life client executable was found. Set HL_EXE in .env if your runtime template does not contain hl.exe."
    }
}

Write-Checklist -Configuration $configuration -Port $resolvedPort -Map $Map -ConnectAddress $connectAddress -RuntimeRoot $runtimeRoot -ServerLogPath $launchInfo.StdOutLog -WeaponLogPath $(if ($weaponLog) { $weaponLog.FullName } else { $null }) -TailWindowRequested $tailWindowRequested -ClientLaunchStatus $clientLaunchStatus

if ($AnalyzeLatestOnExit) {
    Invoke-AnalyzeLatestWeaponLog -WeaponLogPath $(if ($weaponLog) { $weaponLog.FullName } else { $null })
}
