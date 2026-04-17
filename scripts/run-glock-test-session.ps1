[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [int]$Port = 27015,
    [string]$Map = "crossfire",
    [string]$GlockProfile,
    [string]$LabTargetProfile,
    [string[]]$SetCvar = @(),
    [string]$SessionTag,
    [string]$MatrixName,
    [string]$MatrixStep,
    [switch]$LabDummy,
    [switch]$NoClient,
    [switch]$NoTail,
    [switch]$AnalyzeLatestOnExit,
    [switch]$PassThru,
    [switch]$SkipChecklist,
    [switch]$DetachedServer = $true,
    [int]$TimeoutSeconds = 45,
    [string]$TemplateRoot,
    [string]$HldsExe,
    [string]$HlExe,
    [string]$SteamCmdExe,
    [switch]$AllowSteamCmdDownload,
    [switch]$PreferClientMatchedRuntime
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
        [string]$GlockProfile,
        [string]$LabTargetProfile,
        [string]$WeaponLogPath,
        [bool]$TailWindowRequested,
        [string]$ClientLaunchStatus,
        [bool]$LabDummy,
        [string]$GameDirName = "valve"
    )

    Write-Host ""
    Write-Host $(if ($LabDummy) { "Manual Glock lab session ready" } else { "Manual Glock session ready" })
    Write-Host "  configuration : $Configuration"
    Write-Host "  map           : $Map"
    Write-Host "  port          : $Port"
    Write-Host "  connect       : $ConnectAddress"
    Write-Host "  game          : $GameDirName"
    Write-Host "  runtime root  : $RuntimeRoot"
    Write-Host "  server log    : $ServerLogPath"
    Write-Host "  glock profile : $(if ([string]::IsNullOrWhiteSpace($GlockProfile)) { 'default' } else { $GlockProfile })"
    if ($LabDummy) {
        Write-Host "  target profile: $(if ([string]::IsNullOrWhiteSpace($LabTargetProfile)) { 'default' } else { $LabTargetProfile })"
    }

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
    Write-Host $(if ($LabDummy) { "Manual Glock lab checklist" } else { "Manual validation checklist" })

    if ($LabDummy) {
        Write-Host "1. Confirm the stock client connected to the disposable server at $ConnectAddress."
        Write-Host "2. Confirm one stationary Glock lab dummy appeared in front of the player."
        Write-Host "   Expected: a stock human model (default Barney) and one dummy_spawn line in the weapon log."
        Write-Host "3. Confirm which lab target profile is active."
        Write-Host "   Expected: target profile $(if ([string]::IsNullOrWhiteSpace($LabTargetProfile)) { 'default' } else { $LabTargetProfile }) is shown above and echoed in the session and dummy_spawn telemetry."
        Write-Host "4. Stand still, wait for recovery, then fire one careful single Glock primary shot at the dummy."
        Write-Host "   Expected: one accepted line and firstshot=1 once the recovery gate is satisfied."
        Write-Host "5. Land at least one dummy headshot and one dummy kill."
        Write-Host "   Expected: victim_kind=dummy hit or kill lines, plus headshot=1 and target-profile-aware armor fields for the headshot attempt."
        Write-Host "6. Hold primary without releasing."
        Write-Host "   Expected: no repeated accepted shots and tapfire_hold_blocked rejection lines when rejection logging is enabled."
        Write-Host "7. Move continuously and fire primary at the dummy."
        Write-Host "   Expected: accepted lines with move_penalty > 0."
        Write-Host "8. Confirm dummy kill and respawn behavior if autorespawn is enabled."
        Write-Host "   Expected: a dummy_respawn line shortly after the kill and another dummy target at the same test lane."
        Write-Host "9. Analyze the latest log afterward with .\\scripts\\analyze-weapon-log.ps1 -Latest."
        Write-Host "   Compare unarmored versus armored/head-protected sessions before drawing any balance conclusions."
        Write-Host ""
        Write-Host "The Glock lab dummy is a one-player server-side target. Its armor model is experimental and does not prove exact real-player armor or exact PvP equivalence."
    }
    else {
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
}

function Invoke-AnalyzeLatestWeaponLog {
    param(
        [string]$WeaponLogPath
    )

    $analysisScript = Join-Path $PSScriptRoot "analyze-weapon-log.ps1"
    $analysisArguments = @("-Weapon", "glock")

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
$sessionGameDir = if ($PreferClientMatchedRuntime) { Get-TestbedLiveModName } else { "valve" }
$previousWeaponLog = Get-LatestWeaponDebugLog
$useLabDummy = $LabDummy -or (-not [string]::IsNullOrWhiteSpace($LabTargetProfile))
$maxPlayers = if ($useLabDummy) { 1 } else { 4 }

Write-Step $(if ($useLabDummy) { "Installing disposable runtime for the Glock lab session" } else { "Installing disposable runtime for the Glock manual session" })
& "$PSScriptRoot\install-testbed.ps1" -Configuration $configuration -TemplateRoot $TemplateRoot -HldsExe $HldsExe -HlExe $HlExe -SteamCmdExe $SteamCmdExe -AllowSteamCmdDownload:$AllowSteamCmdDownload -PreferClientMatchedRuntime:$PreferClientMatchedRuntime

if ($PreferClientMatchedRuntime) {
    $clientInstall = Resolve-TestbedClientInstall -ExplicitHlExe $HlExe
    if ($clientInstall) {
        $runtimeRoot = Get-TestbedLiveModLinkPath -ClientRoot $clientInstall.Root -GameDirName $sessionGameDir
    }
}

Write-Step $(if ($useLabDummy) { "Launching disposable HLDS in experimental-debug lab mode" } else { "Launching disposable HLDS in experimental-debug mode" })
$sessionMetadataCvars = @(Get-SessionMetadataLaunchAssignments -SessionTag $SessionTag -MatrixName $MatrixName -MatrixStep $MatrixStep -WeaponUnderTest glock)
$effectiveSetCvars = @($sessionMetadataCvars) + @($SetCvar)
if ([string]::IsNullOrWhiteSpace($GlockProfile)) {
    $launchInfo = & "$PSScriptRoot\run-server.ps1" -Configuration $configuration -Map $Map -Port $resolvedPort -MaxPlayers $maxPlayers -Detached -EnableExperimentalGlock -EnableExperimentalGlockDebug -EnableGlockLabDummy:$useLabDummy -LabTargetProfile $LabTargetProfile -SetCvar $effectiveSetCvars -TemplateRoot $TemplateRoot -HldsExe $HldsExe -HlExe $HlExe -SteamCmdExe $SteamCmdExe -AllowSteamCmdDownload:$AllowSteamCmdDownload -PreferClientMatchedRuntime:$PreferClientMatchedRuntime -PassThru
}
else {
    $launchInfo = & "$PSScriptRoot\run-server.ps1" -Configuration $configuration -Map $Map -Port $resolvedPort -MaxPlayers $maxPlayers -Detached -EnableExperimentalGlock -EnableExperimentalGlockDebug -EnableGlockLabDummy:$useLabDummy -GlockProfile $GlockProfile -LabTargetProfile $LabTargetProfile -SetCvar $effectiveSetCvars -TemplateRoot $TemplateRoot -HldsExe $HldsExe -HlExe $HlExe -SteamCmdExe $SteamCmdExe -AllowSteamCmdDownload:$AllowSteamCmdDownload -PreferClientMatchedRuntime:$PreferClientMatchedRuntime -PassThru
}

# The session-ready weapon log appears only after a client joins, so the client
# must launch after HLDS reaches the map-ready state instead of before it.
$serverReady = Wait-ForHldsReady -LaunchInfo $launchInfo -Map $Map -TimeoutSeconds $TimeoutSeconds
if (-not $serverReady) {
    throw "Timed out waiting for the HLDS map-start marker for '$Map'. Check $($launchInfo.StdOutLog), $($launchInfo.StdErrLog), and $($launchInfo.QConsoleLog)."
}

$tailWindowRequested = $false

$connectAddress = "127.0.0.1:$resolvedPort"
$clientLaunchStatus = "disabled by -NoClient"

if (-not $NoClient) {
    $clientExe = Get-TestbedSessionClientExe -RuntimeRoot $runtimeRoot -ExplicitHlExe $HlExe -PreferClientMatchedRuntime:$PreferClientMatchedRuntime
    if ($clientExe) {
        $clientWorkingDirectory = if ($PreferClientMatchedRuntime) { Split-Path -Parent $clientExe } else { $null }
        & "$PSScriptRoot\run-client.ps1" -ConnectAddress $connectAddress -HlExe $clientExe -Game $sessionGameDir -WorkingDirectory $clientWorkingDirectory
        $clientLaunchStatus = "launch requested via $clientExe (-game $sessionGameDir)"
    }
    else {
        $clientLaunchStatus = "skipped because no stock hl.exe was found for the live session"
        Write-Host "No stock Half-Life client executable was found. Set HL_EXE in .env if your Half-Life root is not auto-detected."
    }
}

$weaponLog = $null
$shouldWaitForWeaponLog = (-not $NoClient) -and ((-not $NoTail) -or $AnalyzeLatestOnExit -or $PassThru)
if ($shouldWaitForWeaponLog) {
    $postClientWeaponLogTimeout = [Math]::Min($TimeoutSeconds, 15)
    $weaponLog = Wait-ForSessionWeaponDebugLog -PreviousLatestLog $previousWeaponLog -TimeoutSeconds $postClientWeaponLogTimeout -Process $launchInfo.Process
}

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
        Write-Host "Weapon debug log path is not ready yet. Run .\\scripts\\tail-weapon-log.ps1 once the session header appears."
    }
}

if (-not $SkipChecklist) {
    Write-Checklist -Configuration $configuration -Port $resolvedPort -Map $Map -ConnectAddress $connectAddress -RuntimeRoot $runtimeRoot -ServerLogPath $launchInfo.StdOutLog -GlockProfile $GlockProfile -LabTargetProfile $LabTargetProfile -WeaponLogPath $(if ($weaponLog) { $weaponLog.FullName } else { $null }) -TailWindowRequested $tailWindowRequested -ClientLaunchStatus $clientLaunchStatus -LabDummy:$useLabDummy -GameDirName $sessionGameDir
}

if ($AnalyzeLatestOnExit) {
    Invoke-AnalyzeLatestWeaponLog -WeaponLogPath $(if ($weaponLog) { $weaponLog.FullName } else { $null })
}

if ($PassThru) {
    return [PSCustomObject]@{
        Configuration = $configuration
        Map = $Map
        Port = $resolvedPort
        ConnectAddress = $connectAddress
        RuntimeRoot = $runtimeRoot
        ServerLogPath = $launchInfo.StdOutLog
        ServerErrorLogPath = $launchInfo.StdErrLog
        WeaponLogPath = if ($weaponLog) { $weaponLog.FullName } else { $null }
        TailWindowRequested = $tailWindowRequested
        ClientLaunchStatus = $clientLaunchStatus
        WeaponUnderTest = "glock"
        WeaponProfile = if ([string]::IsNullOrWhiteSpace($GlockProfile)) { "default" } else { $GlockProfile }
        GlockProfile = if ([string]::IsNullOrWhiteSpace($GlockProfile)) { "default" } else { $GlockProfile }
        LabTargetProfile = if ([string]::IsNullOrWhiteSpace($LabTargetProfile)) { "default" } else { $LabTargetProfile }
        LabDummy = $useLabDummy
        SessionTag = $SessionTag
        MatrixName = $MatrixName
        MatrixStep = $MatrixStep
        LaunchInfo = $launchInfo
        Process = $launchInfo.Process
    }
}
